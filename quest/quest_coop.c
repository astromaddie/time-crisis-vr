#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "quest_coop.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
# if __has_include("coop_identity.h")
#  include "coop_identity.h"
# endif
#endif
#ifndef TCVR_COOP_SIM_HASH
#define TCVR_COOP_SIM_HASH "0000000000000000000000000000000000000000000000000000000000000000"
#endif
#ifndef TCVR_COOP_ROM_HASH
#define TCVR_COOP_ROM_HASH "0000000000000000000000000000000000000000000000000000000000000000"
#endif

#define WIRE_MAGIC "TCVR"
#define WIRE_HEAD 7
#define WIRE_MAX 9000
#define DISCOVERY_SIZE 177
#define DISCOVERY_MAGIC "TCVD"
#define NAME_TEXT "Time Crisis VR"
#define BOOT_MAX 8192
#define TICK_QUEUE 2
#define INPUT_WIRE_SIZE 17
#define CANON_WIRE_SIZE 16
#define HELLO_SIZE 134
#define WELCOME_SIZE 133
#define SESSION_TIMEOUT_SEC 45

enum { MSG_HELLO=1,MSG_WELCOME,MSG_START,MSG_BOOT,MSG_ENGINE_READY,MSG_ENGINE_GO,MSG_INPUT,MSG_CANON,MSG_ACK,MSG_PAUSE,MSG_BYE };
typedef enum LobbyState { L_IDLE,L_HOST_WAITING,L_JOIN_CONNECTING,L_READY,L_STARTING,L_RUNNING,L_PAUSED,L_ERROR } LobbyState;
typedef enum Role { ROLE_NONE,ROLE_HOST,ROLE_JOIN } Role;
typedef struct TickSlot { uint64_t number; QCoopTick tick; bool used; } TickSlot;

typedef struct CoopSession {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t worker;
    bool worker_started, stop, solo_override, active;
    LobbyState state;
    Role role;
    char message[192], peer_ip[INET_ADDRSTRLEN], target_ip[INET_ADDRSTRLEN];
    int tcp_port, discovery_port, listen_fd, udp_fd, tcp_fd;
    bool connected, compatible, start_requested, start_sent, engine_attached;
    bool boot_sent, boot_received, engine_ready_sent, peer_engine_ready, engine_go;
    bool local_ready, remote_ready, session_paused, pause_dirty, remote_pause_dirty;
    bool local_sample_ready, local_tick_outstanding, remote_sample_ready;
    bool capture_trigger_down, pending_local_shot_ready;
    QCoopInput pending_local_shot;
    uint64_t next_tick, generated_tick, host_ack, client_ack, last_progress_ms;
    uint64_t local_sample_tick, remote_sample_tick;
    QCoopInput latest_local_sample, local_sample, remote_sample;
    bool ack_dirty;
    uint64_t ack_value;
    TickSlot ticks[TICK_QUEUE];
    QCoopTick current_tick;
    bool current_tick_ready;
    QCoopArbiter arbiter;
    uint8_t eeprom[BOOT_MAX];
    size_t eeprom_size;
} CoopSession;

static CoopSession g = {
    .mutex=PTHREAD_MUTEX_INITIALIZER,
    .condition=PTHREAD_COND_INITIALIZER,
    .state=L_IDLE,
    .role=ROLE_NONE,
    .listen_fd=-1,.udp_fd=-1,.tcp_fd=-1,
    .local_ready=true,.remote_ready=true,
    .next_tick=1,.generated_tick=1
};

typedef struct RxBuffer { uint8_t bytes[16384]; size_t used; } RxBuffer;

static uint64_t monotonic_ms(void){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;}
static uint64_t get_u64(const uint8_t *p){uint64_t v=0;for(int i=7;i>=0;i--)v=(v<<8)|p[i];return v;}
static uint16_t get_u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static void put_u16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void put_u64(uint8_t *p,uint64_t v){for(unsigned i=0;i<8;i++){p[i]=(uint8_t)v;v>>=8;}}
static void set_state_locked(LobbyState state,const char *message){g.state=state;if(state==L_RUNNING)g.last_progress_ms=monotonic_ms();if(message)snprintf(g.message,sizeof g.message,"%s",message);pthread_cond_broadcast(&g.condition);}
static void fail_locked(const char *message){set_state_locked(L_ERROR,message);g.active=true;g.connected=false;g.compatible=false;g.session_paused=true;}
static int make_nonblocking(int fd){int flags=fcntl(fd,F_GETFL,0);return flags<0?-1:fcntl(fd,F_SETFL,flags|O_NONBLOCK);}
static void reset_arbiter_locked(void){memset(&g.arbiter,0,sizeof g.arbiter);g.arbiter.neutral_rearm[0]=g.arbiter.neutral_rearm[1]=true;g.arbiter.tie_preference=0;}
static void session_resume_if_ready_locked(void){
    bool was=g.session_paused;
    g.session_paused=!(g.local_ready&&g.remote_ready);
    if(g.session_paused){if(g.state==L_RUNNING||g.state==L_PAUSED)set_state_locked(L_PAUSED,"Both players must be focused to continue.");}
    else if(was){reset_arbiter_locked();if(g.state==L_PAUSED)set_state_locked(L_RUNNING,"Both players connected.");}
}

void qcoop_arbitrate(QCoopArbiter *a,const QCoopInput p[2],QCoopTick *out){
    if(!a||!p||!out)return;
    memset(out,0,sizeof *out);
    /* Keep a stable non-firing aim from P1; an accepted shot below replaces
     * it with the originating player's exact aim and offscreen state. */
    out->gun_x=p[0].gun_x;out->gun_y=p[0].gun_y;out->offscreen=p[0].offscreen||!p[0].aim_valid;
    out->exposed=p[0].exposed||p[1].exposed;out->coin=p[0].coin||p[1].coin;
    bool someone_exposed=out->exposed;
    for(unsigned i=0;i<2;i++){
        bool down=p[i].trigger;
        if(a->neutral_rearm[i]){
            if(!down)a->neutral_rearm[i]=false;
            a->trigger_was_down[i]=down;
            continue;
        }
        bool rising=down&&!a->trigger_was_down[i];
        a->trigger_was_down[i]=down;
        /* The original cabinet trigger also starts and confirms menus while
         * the player is behind cover. Preserve that safe pulse only when the
         * shared player is covered; a covered partner cannot fire while the
         * other player is exposed. */
        if(!rising||a->pending_count[i]>=QCOOP_SHOT_QUEUE||(!p[i].exposed&&someone_exposed))continue;
        unsigned tail=(a->pending_head[i]+a->pending_count[i])%QCOOP_SHOT_QUEUE;
        a->pending[i][tail]=(QCoopShot){true,p[i].exposed,p[i].gun_x,p[i].gun_y,p[i].offscreen||!p[i].aim_valid};
        a->pending_count[i]++;
    }
    /* A physical single gun cannot accept two edges at once. It also needs one
     * complete released simulation tick between shots. Keep the unserved edge
     * queued and alternate the first player on exact ties. */
    if(a->previous_fire){a->previous_fire=false;return;}
    int chosen=-1;
    if(a->pending_count[0]&&a->pending_count[1])chosen=(int)(a->tie_preference&1u);
    else if(a->pending_count[0])chosen=0;
    else if(a->pending_count[1])chosen=1;
    if(chosen<0)return;
    QCoopShot shot=a->pending[chosen][a->pending_head[chosen]];
    memset(&a->pending[chosen][a->pending_head[chosen]],0,sizeof(QCoopShot));
    a->pending_head[chosen]=(a->pending_head[chosen]+1)%QCOOP_SHOT_QUEUE;
    a->pending_count[chosen]--;
    if(!shot.exposed&&out->exposed){
        /* A safe menu pulse queued while hidden must never become a shot if
         * either player has since left cover. Consume it without firing. */
        a->tie_preference=(unsigned)(chosen^1);
        return;
    }
    out->fire=true;
    if(shot.exposed){out->exposed=true;out->gun_x=shot.gun_x;out->gun_y=shot.gun_y;out->offscreen=shot.offscreen;}
    a->previous_fire=true;
    a->tie_preference=(unsigned)(chosen^1);
}

static bool compatible_bytes(const uint8_t *sim,const uint8_t *rom,uint16_t protocol){
    return protocol==QCOOP_PROTOCOL_VERSION&&memcmp(sim,TCVR_COOP_SIM_HASH,64)==0&&memcmp(rom,TCVR_COOP_ROM_HASH,64)==0;
}
static void wire_input(uint8_t out[INPUT_WIRE_SIZE],uint64_t tick,QCoopInput in){
    memset(out,0,INPUT_WIRE_SIZE);put_u64(out,tick);out[8]=in.trigger;out[9]=in.exposed;out[10]=in.aim_valid;
    put_u16(out+11,in.gun_x);put_u16(out+13,in.gun_y);out[15]=in.offscreen;out[16]=in.coin;
}
static QCoopInput parse_input(const uint8_t *p){QCoopInput in={0};in.trigger=p[8]!=0;in.exposed=p[9]!=0;in.aim_valid=p[10]!=0;in.gun_x=get_u16(p+11);in.gun_y=get_u16(p+13);in.offscreen=p[15]!=0;in.coin=p[16]!=0;return in;}
static void wire_tick(uint8_t out[CANON_WIRE_SIZE],uint64_t tick,QCoopTick in){
    memset(out,0,CANON_WIRE_SIZE);put_u64(out,tick);out[8]=in.fire;out[9]=in.exposed;put_u16(out+10,in.gun_x);put_u16(out+12,in.gun_y);out[14]=in.offscreen;out[15]=in.coin;
}
static QCoopTick parse_tick(const uint8_t *p){QCoopTick t={0};t.fire=p[8]!=0;t.exposed=p[9]!=0;t.gun_x=get_u16(p+10);t.gun_y=get_u16(p+12);t.offscreen=p[14]!=0;t.coin=p[15]!=0;return t;}
uint16_t qcoop_to_arcade_bits(QCoopTick tick){return (tick.fire?0x10u:0u)|(tick.exposed?0x20u:0u)|(tick.coin?1u:0u);}
static bool send_record(int fd,uint8_t type,const void *payload,uint16_t size){
    uint8_t data[WIRE_HEAD+WIRE_MAX];if(size>WIRE_MAX)return false;
    memcpy(data,WIRE_MAGIC,4);data[4]=type;put_u16(data+5,size);if(size&&payload)memcpy(data+WIRE_HEAD,payload,size);
    size_t offset=0,total=WIRE_HEAD+size;int attempts=0;
    while(offset<total&&attempts++<30){ssize_t n=send(fd,data+offset,total-offset,MSG_NOSIGNAL);if(n>0){offset+=(size_t)n;continue;}if(n<0&&(errno==EINTR))continue;if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){struct pollfd p={fd,POLLOUT,0};if(poll(&p,1,100)<=0)continue;}return false;}
    return offset==total;
}
static void build_hello(uint8_t payload[HELLO_SIZE]){
    memset(payload,0,HELLO_SIZE);put_u16(payload,QCOOP_PROTOCOL_VERSION);memcpy(payload+2,TCVR_COOP_SIM_HASH,64);memcpy(payload+66,TCVR_COOP_ROM_HASH,64);snprintf((char *)payload+130,4,"VR");
}
static void build_welcome(uint8_t payload[WELCOME_SIZE],bool compatible){
    memset(payload,0,WELCOME_SIZE);payload[0]=compatible?0:1;put_u16(payload+1,QCOOP_PROTOCOL_VERSION);memcpy(payload+3,TCVR_COOP_SIM_HASH,64);memcpy(payload+67,TCVR_COOP_ROM_HASH,64);memcpy(payload+131,"VR",2);
}
static int create_bound_tcp(int port){
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((uint16_t)port);
    if(bind(fd,(struct sockaddr *)&a,sizeof a)||listen(fd,1)||make_nonblocking(fd)){close(fd);return -1;}return fd;
}
static int create_bound_udp(int port){
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return -1;int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons((uint16_t)port);
    if(bind(fd,(struct sockaddr *)&a,sizeof a)||make_nonblocking(fd)){close(fd);return -1;}return fd;
}
static bool valid_port(int p){return p>=1024&&p<=65535;}
static bool start_common(Role role,int tcp,int discovery){
    qcoop_cancel(false);
    if(!valid_port(tcp)||!valid_port(discovery)){
        pthread_mutex_lock(&g.mutex);g.active=true;g.role=role;fail_locked("Choose a valid TCP and discovery port (1024-65535).");pthread_mutex_unlock(&g.mutex);return false;
    }
    pthread_mutex_lock(&g.mutex);
    g.stop=false;g.active=true;g.solo_override=false;g.role=role;g.state=role==ROLE_HOST?L_HOST_WAITING:L_JOIN_CONNECTING;
    g.tcp_port=tcp;g.discovery_port=discovery;g.compatible=false;g.connected=false;g.start_requested=false;g.start_sent=false;
    g.engine_attached=false;g.boot_sent=false;g.boot_received=false;g.engine_ready_sent=false;g.peer_engine_ready=false;g.engine_go=false;
    g.local_ready=true;g.remote_ready=true;g.session_paused=false;g.pause_dirty=true;g.remote_pause_dirty=false;
    g.next_tick=g.generated_tick=1;g.local_sample_ready=g.local_tick_outstanding=g.remote_sample_ready=false;g.capture_trigger_down=g.pending_local_shot_ready=false;memset(&g.pending_local_shot,0,sizeof g.pending_local_shot);g.last_progress_ms=0;memset(g.ticks,0,sizeof g.ticks);g.current_tick_ready=false;reset_arbiter_locked();
    g.message[0]=0;g.peer_ip[0]=0;g.listen_fd=g.udp_fd=g.tcp_fd=-1;pthread_mutex_unlock(&g.mutex);return true;
}
static void *worker_entry(void *unused);

bool qcoop_start_host(const char *rom_dir,int tcp_port,int discovery_port){
    (void)rom_dir;
    if(!start_common(ROLE_HOST,tcp_port,discovery_port))return false;
    int listener=create_bound_tcp(tcp_port),udp=create_bound_udp(discovery_port);
    if(listener<0||udp<0){if(listener>=0)close(listener);if(udp>=0)close(udp);pthread_mutex_lock(&g.mutex);fail_locked("Could not open the LAN host ports. Check that this port is available.");pthread_mutex_unlock(&g.mutex);return false;}
    pthread_mutex_lock(&g.mutex);g.listen_fd=listener;g.udp_fd=udp;pthread_mutex_unlock(&g.mutex);
    if(pthread_create(&g.worker,NULL,worker_entry,NULL)){close(listener);close(udp);pthread_mutex_lock(&g.mutex);g.listen_fd=g.udp_fd=-1;fail_locked("Could not start the LAN host service.");pthread_mutex_unlock(&g.mutex);return false;}
    pthread_mutex_lock(&g.mutex);g.worker_started=true;pthread_mutex_unlock(&g.mutex);return true;
}

bool qcoop_start_join(const char *rom_dir,const char *ipv4,int tcp_port,int discovery_port){
    (void)rom_dir;
    qcoop_cancel(false);
    struct in_addr parsed;if(!ipv4||inet_pton(AF_INET,ipv4,&parsed)!=1){pthread_mutex_lock(&g.mutex);g.active=true;g.role=ROLE_JOIN;fail_locked("Enter a valid IPv4 host address.");pthread_mutex_unlock(&g.mutex);return false;}
    if(!start_common(ROLE_JOIN,tcp_port,discovery_port))return false;
    pthread_mutex_lock(&g.mutex);snprintf(g.target_ip,sizeof g.target_ip,"%s",ipv4);pthread_mutex_unlock(&g.mutex);
    if(pthread_create(&g.worker,NULL,worker_entry,NULL)){pthread_mutex_lock(&g.mutex);fail_locked("Could not start the LAN connection service.");pthread_mutex_unlock(&g.mutex);return false;}
    pthread_mutex_lock(&g.mutex);g.worker_started=true;pthread_mutex_unlock(&g.mutex);return true;
}

bool qcoop_host_start(void){
    pthread_mutex_lock(&g.mutex);
    bool allowed=g.role==ROLE_HOST&&g.state==L_READY&&g.connected&&g.compatible&&!g.stop;
    if(allowed){g.start_requested=true;set_state_locked(L_STARTING,"Starting a fresh shared run.");pthread_cond_broadcast(&g.condition);}
    pthread_mutex_unlock(&g.mutex);return allowed;
}

static bool wait_fd(int fd,short events,int timeout_ms){struct pollfd p={fd,events,0};int r;do{r=poll(&p,1,timeout_ms);}while(r<0&&errno==EINTR);return r>0&&(p.revents&events)!=0;}
static int connect_host(const char *ip,int port){
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;make_nonblocking(fd);
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);inet_pton(AF_INET,ip,&a.sin_addr);
    int r=connect(fd,(struct sockaddr *)&a,sizeof a);if(r<0&&errno!=EINPROGRESS){close(fd);return -1;}
    if(r<0&&!wait_fd(fd,POLLOUT,6000)){close(fd);return -1;}
    int e=0;socklen_t n=sizeof e;if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&e,&n)||e){close(fd);return -1;}return fd;
}
static bool set_connection(int fd,const char *ip){int one=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);pthread_mutex_lock(&g.mutex);if(g.stop||g.tcp_fd>=0){pthread_mutex_unlock(&g.mutex);close(fd);return false;}g.tcp_fd=fd;g.connected=true;if(ip)snprintf(g.peer_ip,sizeof g.peer_ip,"%s",ip);pthread_mutex_unlock(&g.mutex);return true;}
static void set_error(const char *message){pthread_mutex_lock(&g.mutex);if(g.state!=L_ERROR)fail_locked(message);pthread_mutex_unlock(&g.mutex);}
void qcoop_report_error(const char *message){set_error(message?message:"The shared run could not start.");}
bool qcoop_launch_arguments_supported(int argc,char **argv){
    if(argc<0||(!argv&&argc))return false;
    for(int i=1;i<argc;i++)if(argv[i]&&argv[i][0]=='-'&&argv[i][1]=='-'&&strcmp(argv[i],"--window")&&strcmp(argv[i],"--fullscreen"))return false;
    return true;
}
bool qcoop_frame_diagnostics_pending(void){
    const char *replay=getenv("TCVR_FRAME_REPLAY");
    return (replay&&!strcmp(replay,"1"))||access("vrdiag.once",F_OK)==0||access("vrdiag-replay.once",F_OK)==0;
}

static bool discovery_request(uint8_t data[DISCOVERY_SIZE],const char nonce[8]){
    memset(data,0,DISCOVERY_SIZE);memcpy(data,DISCOVERY_MAGIC,4);put_u16(data+4,QCOOP_PROTOCOL_VERSION);memcpy(data+6,TCVR_COOP_SIM_HASH,64);memcpy(data+70,TCVR_COOP_ROM_HASH,64);memcpy(data+134,nonce,8);memcpy(data+142,NAME_TEXT,strlen(NAME_TEXT));return true;
}
static bool discovery_reply(const uint8_t *data,size_t n,uint8_t out[DISCOVERY_SIZE]){
    if(n!=DISCOVERY_SIZE||memcmp(data,DISCOVERY_MAGIC,4))return false;
    uint16_t version=get_u16(data+4);bool ok=compatible_bytes(data+6,data+70,version);
    memcpy(out,data,DISCOVERY_SIZE);out[174]=ok?1:0;put_u16(out+175,(uint16_t)g.tcp_port);
    return true;
}
static void send_discovery_response(int fd,const struct sockaddr_in *from,const uint8_t *data,size_t n){
    uint8_t out[DISCOVERY_SIZE];if(!discovery_reply(data,n,out))return;sendto(fd,out,sizeof out,0,(const struct sockaddr *)from,sizeof *from);
}

static void apply_pause_locked(bool remote_ready){
    bool old=g.session_paused;g.remote_ready=remote_ready;session_resume_if_ready_locked();
    if(!old&&g.session_paused){
        bool committed=false;for(unsigned i=0;i<TICK_QUEUE;i++)if(g.ticks[i].used&&g.ticks[i].number==g.next_tick)committed=true;
        /* Preserve an already-issued canonical tick so the slower copy can
         * reach the same boundary after focus returns; discard unissued input. */
        if(!committed){g.local_sample_ready=false;g.local_tick_outstanding=false;g.remote_sample_ready=false;}
        g.latest_local_sample.trigger=false;g.capture_trigger_down=false;g.pending_local_shot_ready=false;reset_arbiter_locked();
    }
}
static void enqueue_tick_locked(uint64_t number,QCoopTick tick){
    for(unsigned i=0;i<TICK_QUEUE;i++)if(!g.ticks[i].used){g.ticks[i]=(TickSlot){number,tick,true};pthread_cond_broadcast(&g.condition);return;}
    fail_locked("The shared game tick queue overflowed. Close both games and start a new session.");
}
static bool dequeue_tick_locked(uint64_t number,QCoopTick *tick){
    for(unsigned i=0;i<TICK_QUEUE;i++)if(g.ticks[i].used&&g.ticks[i].number==number){*tick=g.ticks[i].tick;g.ticks[i].used=false;g.current_tick=*tick;g.current_tick_ready=true;return true;}return false;
}
static void process_record(uint8_t type,const uint8_t *p,size_t n){
    if(type==MSG_HELLO&&n==HELLO_SIZE&&g.role==ROLE_HOST){
        uint16_t version=get_u16(p);bool ok=compatible_bytes(p+2,p+66,version);uint8_t welcome[WELCOME_SIZE];build_welcome(welcome,ok);int fd;
        pthread_mutex_lock(&g.mutex);fd=g.tcp_fd;pthread_mutex_unlock(&g.mutex);
        if(fd>=0&&!send_record(fd,MSG_WELCOME,welcome,sizeof welcome))set_error("The LAN connection closed during compatibility check.");
        pthread_mutex_lock(&g.mutex);
        if(!ok){fail_locked("The other headset has a different simulation build, protocol, or ROM set.");g.tcp_fd=-1;pthread_mutex_unlock(&g.mutex);close(fd);return;}
        g.compatible=true;g.remote_ready=true;g.local_ready=true;g.session_paused=false;set_state_locked(L_READY,"Compatible player connected.");pthread_mutex_unlock(&g.mutex);return;
    }
    if(type==MSG_WELCOME&&n==WELCOME_SIZE&&g.role==ROLE_JOIN){
        bool ok=p[0]==0&&compatible_bytes(p+3,p+67,get_u16(p+1));pthread_mutex_lock(&g.mutex);
        if(ok){g.compatible=g.connected=true;g.remote_ready=true;g.local_ready=true;g.session_paused=false;set_state_locked(L_READY,"Connected to a compatible host.");}
        else{fail_locked("The host has a different simulation build, protocol, or ROM set.");}
        pthread_mutex_unlock(&g.mutex);return;
    }
    pthread_mutex_lock(&g.mutex);int fd=g.tcp_fd;Role role=g.role;pthread_mutex_unlock(&g.mutex);
    if(type==MSG_START&&n==0&&role==ROLE_JOIN){pthread_mutex_lock(&g.mutex);if(g.state==L_READY)set_state_locked(L_STARTING,"Starting a fresh shared run.");pthread_mutex_unlock(&g.mutex);return;}
    if(type==MSG_BOOT&&role==ROLE_JOIN&&n>=2){
        uint16_t size=get_u16(p);pthread_mutex_lock(&g.mutex);
        if(size>BOOT_MAX||n!=(size_t)size+2){fail_locked("The host sent an invalid fresh-run EEPROM image.");}
        else{memcpy(g.eeprom,p+2,size);g.eeprom_size=size;g.boot_received=true;pthread_cond_broadcast(&g.condition);}
        pthread_mutex_unlock(&g.mutex);return;
    }
    if(type==MSG_ENGINE_READY&&n==0&&role==ROLE_HOST){pthread_mutex_lock(&g.mutex);g.peer_engine_ready=true;pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);return;}
    if(type==MSG_ENGINE_GO&&n==0&&role==ROLE_JOIN){pthread_mutex_lock(&g.mutex);g.engine_go=true;set_state_locked(L_RUNNING,"Shared run started.");pthread_mutex_unlock(&g.mutex);return;}
    if(type==MSG_INPUT&&n==INPUT_WIRE_SIZE&&role==ROLE_HOST){
        QCoopInput in=parse_input(p);uint64_t tick=get_u64(p);pthread_mutex_lock(&g.mutex);
        if(!g.session_paused&&tick==g.generated_tick){g.remote_sample=in;g.remote_sample_tick=tick;g.remote_sample_ready=true;pthread_cond_broadcast(&g.condition);}
        pthread_mutex_unlock(&g.mutex);return;
    }
    if(type==MSG_CANON&&n==CANON_WIRE_SIZE&&role==ROLE_JOIN){
        uint64_t tick=get_u64(p);QCoopTick out=parse_tick(p);pthread_mutex_lock(&g.mutex);
        if(tick>=g.next_tick&&tick<g.next_tick+TICK_QUEUE)enqueue_tick_locked(tick,out);
        pthread_mutex_unlock(&g.mutex);return;
    }
    if(type==MSG_ACK&&n==8&&role==ROLE_HOST){uint64_t ack=get_u64(p);pthread_mutex_lock(&g.mutex);if(ack>=g.client_ack&&ack<g.generated_tick){g.client_ack=ack;g.last_progress_ms=monotonic_ms();}pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);return;}
    if(type==MSG_PAUSE&&n==1){pthread_mutex_lock(&g.mutex);apply_pause_locked(p[0]!=0);pthread_mutex_unlock(&g.mutex);return;}
    if(type==MSG_BYE){set_error("The other headset left. Close both games and start a new session.");return;}
    if(type!=MSG_HELLO&&type!=MSG_WELCOME)set_error("The LAN session sent an invalid message. Close both games and start a new session.");
    (void)fd;
}

static bool parse_rx(RxBuffer *rx){
    while(rx->used>=WIRE_HEAD){
        if(memcmp(rx->bytes,WIRE_MAGIC,4)){set_error("The LAN connection sent invalid data.");return false;}
        size_t n=get_u16(rx->bytes+5);if(n>WIRE_MAX){set_error("The LAN connection sent an oversized message.");return false;}
        if(rx->used<WIRE_HEAD+n)return true;
        uint8_t type=rx->bytes[4];process_record(type,rx->bytes+WIRE_HEAD,n);
        size_t total=WIRE_HEAD+n;memmove(rx->bytes,rx->bytes+total,rx->used-total);rx->used-=total;
        pthread_mutex_lock(&g.mutex);bool failed=g.state==L_ERROR||g.stop;pthread_mutex_unlock(&g.mutex);if(failed)return false;
    }
    return true;
}
static bool pump_tcp(int fd,RxBuffer *rx){
    for(;;){if(rx->used==sizeof rx->bytes){set_error("The LAN connection buffer is full.");return false;}
        ssize_t n=recv(fd,rx->bytes+rx->used,sizeof rx->bytes-rx->used,0);
        if(n>0){rx->used+=(size_t)n;if(!parse_rx(rx))return false;continue;}
        if(n==0){set_error("The other headset disconnected. Start a new session.");return false;}
        if(errno==EINTR)continue;if(errno==EAGAIN||errno==EWOULDBLOCK)return true;set_error("LAN connection failed. Close both games and start a new session.");return false;
    }
}
static void send_client_input_if_ready(int fd){
    QCoopInput in;uint64_t tick;bool sendit=false;
    pthread_mutex_lock(&g.mutex);
    if(g.role==ROLE_JOIN&&g.state==L_RUNNING&&!g.session_paused&&g.local_sample_ready){in=g.local_sample;tick=g.local_sample_tick;g.local_sample_ready=false;sendit=true;}
    pthread_mutex_unlock(&g.mutex);
    if(sendit){uint8_t data[INPUT_WIRE_SIZE];wire_input(data,tick,in);if(!send_record(fd,MSG_INPUT,data,sizeof data))set_error("The LAN connection failed while sending player input.");}
}
static void host_make_tick(int fd){
    QCoopTick output;uint64_t tick;bool made=false;
    pthread_mutex_lock(&g.mutex);
    if(g.state==L_RUNNING&&!g.session_paused&&g.local_sample_ready&&g.remote_sample_ready&&g.local_sample_tick==g.generated_tick&&g.remote_sample_tick==g.generated_tick&&g.host_ack==g.client_ack&&g.host_ack==g.generated_tick-1){
        QCoopInput players[2];players[0]=g.local_sample;players[1]=g.remote_sample;qcoop_arbitrate(&g.arbiter,players,&output);
        tick=g.generated_tick++;g.local_sample_ready=false;g.remote_sample_ready=false;enqueue_tick_locked(tick,output);made=true;
    }
    pthread_mutex_unlock(&g.mutex);
    if(made){uint8_t data[CANON_WIRE_SIZE];wire_tick(data,tick,output);if(!send_record(fd,MSG_CANON,data,sizeof data))set_error("The LAN connection failed while sharing the game tick.");}
}
static void service_control(int fd){
    bool send_pause=false,ready=false,send_start=false,send_boot=false,send_engine_ready=false,send_engine_go=false,send_ack=false;
    uint64_t ack_value=0;
    uint8_t eeprom[BOOT_MAX];size_t eeprom_size=0;
    pthread_mutex_lock(&g.mutex);
    if(g.pause_dirty){send_pause=true;ready=g.local_ready;g.pause_dirty=false;}
    if(g.ack_dirty){send_ack=true;ack_value=g.ack_value;g.ack_dirty=false;}
    if(g.role==ROLE_HOST&&g.start_requested&&!g.start_sent&&g.connected){g.start_sent=true;send_start=true;set_state_locked(L_STARTING,"Starting a fresh shared run.");}
    if(g.role==ROLE_HOST&&g.state==L_STARTING&&g.engine_attached&&!g.boot_sent){g.boot_sent=true;send_boot=true;eeprom_size=g.eeprom_size;memcpy(eeprom,g.eeprom,eeprom_size);}
    if(g.role==ROLE_JOIN&&g.state==L_STARTING&&g.boot_received&&g.engine_attached&&!g.engine_ready_sent){g.engine_ready_sent=true;send_engine_ready=true;}
    if(g.role==ROLE_HOST&&g.state==L_STARTING&&g.engine_attached&&g.peer_engine_ready&&!g.engine_go)send_engine_go=true;
    pthread_mutex_unlock(&g.mutex);
    if(send_pause){uint8_t p=ready;if(!send_record(fd,MSG_PAUSE,&p,1))set_error("The LAN connection failed while synchronizing pause state.");}
    if(send_ack){uint8_t ack[8];put_u64(ack,ack_value);if(!send_record(fd,MSG_ACK,ack,sizeof ack))set_error("The LAN connection failed while acknowledging a shared tick.");}
    if(send_start&&!send_record(fd,MSG_START,NULL,0))set_error("The LAN connection failed while starting the shared run.");
    if(send_boot){uint8_t data[BOOT_MAX+2];put_u16(data,(uint16_t)eeprom_size);memcpy(data+2,eeprom,eeprom_size);if(!send_record(fd,MSG_BOOT,data,(uint16_t)(eeprom_size+2)))set_error("The LAN connection failed while synchronizing the fresh run.");}
    if(send_engine_ready&&!send_record(fd,MSG_ENGINE_READY,NULL,0))set_error("The LAN connection failed at the shared start barrier.");
    if(send_engine_go){
        if(!send_record(fd,MSG_ENGINE_GO,NULL,0))set_error("The LAN connection failed at the shared start barrier.");
        else{pthread_mutex_lock(&g.mutex);g.engine_go=true;set_state_locked(L_RUNNING,"Shared run started.");pthread_mutex_unlock(&g.mutex);}
    }
}

static bool host_accept_one(int listener){
    struct sockaddr_in peer;socklen_t n=sizeof peer;int fd=accept(listener,(struct sockaddr *)&peer,&n);if(fd<0)return false;
    make_nonblocking(fd);char ip[INET_ADDRSTRLEN]={0};inet_ntop(AF_INET,&peer.sin_addr,ip,sizeof ip);
    pthread_mutex_lock(&g.mutex);bool allowed=g.tcp_fd<0&&g.state!=L_ERROR&&!g.stop;pthread_mutex_unlock(&g.mutex);
    if(!allowed){close(fd);return false;}return set_connection(fd,ip);
}
static bool check_running_stall(void){
    bool stalled=false;pthread_mutex_lock(&g.mutex);
    if(g.state==L_RUNNING&&!g.session_paused&&g.last_progress_ms&&monotonic_ms()-g.last_progress_ms>=QCOOP_STALL_TIMEOUT_MS){
        fail_locked("Shared game stopped. Close both games and start a new session.");stalled=true;
    }
    pthread_mutex_unlock(&g.mutex);return stalled;
}

static void *worker_entry(void *unused){
    (void)unused;Role role;int listener,udp;char target[INET_ADDRSTRLEN];int port;
    pthread_mutex_lock(&g.mutex);role=g.role;listener=g.listen_fd;udp=g.udp_fd;snprintf(target,sizeof target,"%s",g.target_ip);port=g.tcp_port;pthread_mutex_unlock(&g.mutex);
    RxBuffer rx={0};bool hello_sent=false;
    if(role==ROLE_JOIN){int fd=connect_host(target,port);if(fd<0){set_error("Could not reach that IPv4 host on the local network.");goto done;}
        if(!set_connection(fd,target))goto done;uint8_t hello[HELLO_SIZE];build_hello(hello);if(!send_record(fd,MSG_HELLO,hello,sizeof hello)){set_error("The LAN host closed during compatibility check.");goto done;}hello_sent=true;}
    while(1){
        pthread_mutex_lock(&g.mutex);bool stop=g.stop,failed=g.state==L_ERROR;int fd=g.tcp_fd;int lfd=g.listen_fd,ufd=g.udp_fd;bool host= g.role==ROLE_HOST;pthread_mutex_unlock(&g.mutex);
        if(stop||failed)break;
        if(fd>=0){service_control(fd);if(host)host_make_tick(fd);else send_client_input_if_ready(fd);}
        struct pollfd fds[3];nfds_t nfds=0;int fd_index=-1,listen_index=-1,udp_index=-1;
        if(fd>=0){fd_index=(int)nfds;fds[nfds++]=(struct pollfd){fd,POLLIN,0};}
        if(host&&lfd>=0&&fd<0){listen_index=(int)nfds;fds[nfds++]=(struct pollfd){lfd,POLLIN,0};}
        if(host&&ufd>=0){udp_index=(int)nfds;fds[nfds++]=(struct pollfd){ufd,POLLIN,0};}
        int pr=poll(fds,nfds,4);if(pr<0&&errno!=EINTR){set_error("The LAN service stopped unexpectedly.");break;}
        if(fd_index>=0&&(fds[fd_index].revents&(POLLERR|POLLHUP|POLLNVAL))){set_error("The other headset disconnected. Start a new session.");break;}
        if(fd_index>=0&&(fds[fd_index].revents&POLLIN)&&!pump_tcp(fd,&rx))break;
        if(listen_index>=0&&(fds[listen_index].revents&POLLIN))host_accept_one(listener);
        if(udp_index>=0&&(fds[udp_index].revents&POLLIN)){
            uint8_t data[DISCOVERY_SIZE+64];struct sockaddr_in from;socklen_t n=sizeof from;ssize_t got=recvfrom(udp,data,sizeof data,0,(struct sockaddr *)&from,&n);
            if(got>0&&memcmp(data,DISCOVERY_MAGIC,4)==0)send_discovery_response(udp,&from,data,(size_t)got);
        }
        if(role==ROLE_JOIN&&!hello_sent){set_error("Could not start the LAN compatibility check.");break;}
        if(check_running_stall())break;
    }
done:
    pthread_mutex_lock(&g.mutex);int close_tcp=g.tcp_fd,close_listener=g.listen_fd,close_udp=g.udp_fd;g.tcp_fd=g.listen_fd=g.udp_fd=-1;g.connected=false;pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);
    if(close_tcp>=0)close(close_tcp);if(close_listener>=0)close(close_listener);if(close_udp>=0)close(close_udp);return NULL;
}

bool qcoop_requested(void){pthread_mutex_lock(&g.mutex);bool v=g.active&&g.role!=ROLE_NONE;pthread_mutex_unlock(&g.mutex);return v;}
bool qcoop_is_active(void){pthread_mutex_lock(&g.mutex);bool v=g.active&&g.role!=ROLE_NONE&&g.state!=L_IDLE;pthread_mutex_unlock(&g.mutex);return v;}
bool qcoop_is_failed(void){pthread_mutex_lock(&g.mutex);bool v=g.active&&g.state==L_ERROR;pthread_mutex_unlock(&g.mutex);return v;}
bool qcoop_is_paused(void){pthread_mutex_lock(&g.mutex);bool v=g.active&&(g.state==L_PAUSED||g.state==L_ERROR||g.session_paused);pthread_mutex_unlock(&g.mutex);return v;}
bool qcoop_is_client(void){pthread_mutex_lock(&g.mutex);bool v=g.active&&g.role==ROLE_JOIN;pthread_mutex_unlock(&g.mutex);return v;}
void qcoop_copy_display_status(char *out,size_t size){if(!out||!size)return;pthread_mutex_lock(&g.mutex);if(g.state==L_ERROR||g.state==L_PAUSED)snprintf(out,size,"%s",g.message);else out[0]=0;pthread_mutex_unlock(&g.mutex);}
const char *qcoop_failure_message(void){static _Thread_local char text[192];pthread_mutex_lock(&g.mutex);snprintf(text,sizeof text,"%s",g.state==L_ERROR?g.message:"");pthread_mutex_unlock(&g.mutex);return text;}
void qcoop_set_local_ready(bool ready){pthread_mutex_lock(&g.mutex);if(g.active&&g.role!=ROLE_NONE&&g.local_ready!=ready){bool old=g.session_paused;g.local_ready=ready;g.pause_dirty=true;session_resume_if_ready_locked();if(!old&&g.session_paused){bool committed=false;for(unsigned i=0;i<TICK_QUEUE;i++)if(g.ticks[i].used&&g.ticks[i].number==g.next_tick)committed=true;if(!committed){g.local_sample_ready=false;g.local_tick_outstanding=false;g.remote_sample_ready=false;}g.latest_local_sample.trigger=false;g.capture_trigger_down=false;g.pending_local_shot_ready=false;reset_arbiter_locked();}pthread_cond_broadcast(&g.condition);}pthread_mutex_unlock(&g.mutex);}
void qcoop_capture_local(QCoopInput input){pthread_mutex_lock(&g.mutex);if(g.active&&g.role!=ROLE_NONE&&g.state==L_RUNNING&&!g.session_paused){if(input.trigger&&!g.capture_trigger_down&&!g.pending_local_shot_ready){g.pending_local_shot=input;g.pending_local_shot_ready=true;}g.capture_trigger_down=input.trigger;g.latest_local_sample=input;}pthread_mutex_unlock(&g.mutex);}
bool qcoop_try_tick(void){
    pthread_mutex_lock(&g.mutex);if(!g.active||g.role==ROLE_NONE||g.state!=L_RUNNING||g.session_paused){pthread_mutex_unlock(&g.mutex);return false;}
    QCoopTick tick;if(dequeue_tick_locked(g.next_tick,&tick)){g.next_tick++;g.last_progress_ms=monotonic_ms();g.local_tick_outstanding=false;g.current_tick=tick;g.current_tick_ready=true;if(g.role==ROLE_HOST)g.host_ack=g.next_tick-1;else{g.local_sample_ready=false;g.client_ack=g.next_tick-1;g.ack_dirty=true;g.ack_value=g.next_tick-1;}pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);
        return true;}
    if(!g.local_tick_outstanding){g.local_sample_tick=g.next_tick;g.local_sample=g.latest_local_sample;if(g.pending_local_shot_ready){bool coin=g.local_sample.coin;g.local_sample=g.pending_local_shot;g.local_sample.coin=coin;g.local_sample.trigger=true;g.pending_local_shot_ready=false;}g.local_sample_ready=true;g.local_tick_outstanding=true;}
    pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);return false;
}
bool qcoop_current_input(QCoopTick *tick){pthread_mutex_lock(&g.mutex);bool ok=g.current_tick_ready;if(ok&&tick)*tick=g.current_tick;g.current_tick_ready=false;pthread_mutex_unlock(&g.mutex);return ok;}

static struct timespec deadline_after(int seconds){struct timespec t;clock_gettime(CLOCK_REALTIME,&t);t.tv_sec+=seconds;return t;}
int qcoop_engine_boot(uint8_t *eeprom,size_t size,bool replay,bool diagnostics){
    pthread_mutex_lock(&g.mutex);if(!g.active||g.role==ROLE_NONE){pthread_mutex_unlock(&g.mutex);return 0;}
    if(replay||diagnostics){fail_locked("Co-op cannot start with a diagnostic replay or one-shot input override. Remove it and choose Host or Join again.");pthread_mutex_unlock(&g.mutex);return -1;}
    if(size>BOOT_MAX||!eeprom){fail_locked("This game build cannot synchronize its EEPROM image.");pthread_mutex_unlock(&g.mutex);return -1;}
    if(g.state!=L_STARTING){fail_locked("The lobby did not authorize a fresh game start.");pthread_mutex_unlock(&g.mutex);return -1;}
    g.engine_attached=true;g.eeprom_size=size;if(g.role==ROLE_HOST)memcpy(g.eeprom,eeprom,size);pthread_cond_broadcast(&g.condition);
    struct timespec until=deadline_after(SESSION_TIMEOUT_SEC);
    if(g.role==ROLE_JOIN){while(!g.boot_received&&g.state!=L_ERROR&&!g.stop){if(pthread_cond_timedwait(&g.condition,&g.mutex,&until)==ETIMEDOUT){fail_locked("Timed out waiting for the host's fresh-run data.");break;}}
        if(g.state==L_ERROR||g.eeprom_size!=size){if(g.state!=L_ERROR)fail_locked("The host game has an incompatible EEPROM format.");pthread_mutex_unlock(&g.mutex);return -1;}memcpy(eeprom,g.eeprom,size);g.engine_ready_sent=false;
    }
    while(!g.engine_go&&g.state!=L_ERROR&&!g.stop){if(pthread_cond_timedwait(&g.condition,&g.mutex,&until)==ETIMEDOUT){fail_locked("Timed out at the shared fresh-run start barrier.");break;}}
    bool ok=g.engine_go&&g.state!=L_ERROR;pthread_mutex_unlock(&g.mutex);return ok?1:-1;
}

void qcoop_cancel(bool force_solo){
    pthread_t worker;bool join=false;int fds[3];
    pthread_mutex_lock(&g.mutex);worker=g.worker;join=g.worker_started;g.stop=true;fds[0]=g.tcp_fd;fds[1]=g.listen_fd;fds[2]=g.udp_fd;for(unsigned i=0;i<3;i++)if(fds[i]>=0)shutdown(fds[i],SHUT_RDWR);pthread_mutex_unlock(&g.mutex);
    if(join&&!pthread_equal(worker,pthread_self()))pthread_join(worker,NULL);
    pthread_mutex_lock(&g.mutex);if(!join)for(unsigned i=0;i<3;i++)if(fds[i]>=0)close(fds[i]);g.tcp_fd=g.listen_fd=g.udp_fd=-1;g.worker_started=false;g.stop=false;g.active=false;g.connected=g.compatible=false;g.role=ROLE_NONE;g.start_requested=g.start_sent=false;g.engine_attached=g.boot_received=g.engine_go=false;g.session_paused=false;g.local_sample_ready=g.local_tick_outstanding=g.remote_sample_ready=false;g.state=L_IDLE;g.message[0]=0;g.peer_ip[0]=0;g.solo_override=force_solo;pthread_cond_broadcast(&g.condition);pthread_mutex_unlock(&g.mutex);
}

static char *dup_printf(const char *fmt,...){va_list ap;va_start(ap,fmt);va_list cp;va_copy(cp,ap);int n=vsnprintf(NULL,0,fmt,cp);va_end(cp);if(n<0){va_end(ap);return NULL;}char *s=malloc((size_t)n+1);if(s)vsnprintf(s,(size_t)n+1,fmt,ap);va_end(ap);return s;}
char *qcoop_status_json(void){
    pthread_mutex_lock(&g.mutex);const char *state="idle";switch(g.state){case L_HOST_WAITING:state="host_waiting";break;case L_JOIN_CONNECTING:state="connecting";break;case L_READY:state="ready";break;case L_STARTING:state="starting";break;case L_RUNNING:state="running";break;case L_PAUSED:state="paused";break;case L_ERROR:state="error";break;default:break;}
    const char *role=g.role==ROLE_HOST?"host":g.role==ROLE_JOIN?"join":"none";char *out=dup_printf("{\"state\":\"%s\",\"role\":\"%s\",\"peerIpv4\":\"%s\",\"compatible\":%s,\"canStart\":%s,\"message\":\"%s\",\"tcpPort\":%d,\"protocolVersion\":%d}",state,role,g.peer_ip,g.compatible?"true":"false",(g.role==ROLE_HOST&&g.state==L_READY&&g.connected&&g.compatible)?"true":"false",g.message,g.tcp_port,QCOOP_PROTOCOL_VERSION);pthread_mutex_unlock(&g.mutex);return out;
}

char **qcoop_local_ipv4_addresses(size_t *count){
    if(count)*count=0;struct ifaddrs *all=NULL;if(getifaddrs(&all))return NULL;char **list=NULL;size_t n=0;
    for(struct ifaddrs *it=all;it;it=it->ifa_next){if(!it->ifa_addr||it->ifa_addr->sa_family!=AF_INET)continue;
        char ip[INET_ADDRSTRLEN];struct sockaddr_in *a=(struct sockaddr_in *)it->ifa_addr;if(!inet_ntop(AF_INET,&a->sin_addr,ip,sizeof ip)||!strncmp(ip,"127.",4))continue;bool duplicate=false;for(size_t i=0;i<n;i++)if(!strcmp(list[i],ip))duplicate=true;if(duplicate)continue;
        char **next=realloc(list,(n+1)*sizeof *list);if(!next)break;list=next;list[n]=strdup(ip);if(!list[n])break;n++;
    }
    freeifaddrs(all);if(count)*count=n;return list;
}

char **qcoop_discover(const char *rom_dir,int discovery_port,int timeout_ms,size_t *count){
    (void)rom_dir;if(count)*count=0;if(!valid_port(discovery_port))return NULL;
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return NULL;int yes=1;setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof yes);make_nonblocking(fd);
    char nonce[8];uint64_t seed=(uint64_t)time(NULL)^(uint64_t)getpid();put_u64((uint8_t *)nonce,seed);uint8_t query[DISCOVERY_SIZE];discovery_request(query,nonce);
    struct sockaddr_in to={0};to.sin_family=AF_INET;to.sin_addr.s_addr=htonl(INADDR_BROADCAST);to.sin_port=htons((uint16_t)discovery_port);sendto(fd,query,sizeof query,0,(struct sockaddr *)&to,sizeof to);
    struct ifaddrs *interfaces=NULL;
    if(getifaddrs(&interfaces)==0){
        for(struct ifaddrs *it=interfaces;it;it=it->ifa_next){
            if(!it->ifa_addr||it->ifa_addr->sa_family!=AF_INET||!it->ifa_broadaddr||it->ifa_broadaddr->sa_family!=AF_INET)continue;
            const struct sockaddr_in *local=(const struct sockaddr_in *)it->ifa_addr;
            if((ntohl(local->sin_addr.s_addr)>>24)==127)continue;
            struct sockaddr_in broadcast=*(const struct sockaddr_in *)it->ifa_broadaddr;broadcast.sin_port=htons((uint16_t)discovery_port);
            if(broadcast.sin_addr.s_addr!=htonl(INADDR_BROADCAST))sendto(fd,query,sizeof query,0,(struct sockaddr *)&broadcast,sizeof broadcast);
        }
        freeifaddrs(interfaces);
    }
    char **entries=NULL;size_t n=0;int timeout=timeout_ms<0?0:timeout_ms>5000?5000:timeout_ms;struct timespec until;clock_gettime(CLOCK_MONOTONIC,&until);until.tv_sec+=timeout/1000;until.tv_nsec+=(long)(timeout%1000)*1000000L;if(until.tv_nsec>=1000000000L){until.tv_sec++;until.tv_nsec-=1000000000L;}
    while(timeout>0){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);long remain=(until.tv_sec-now.tv_sec)*1000+(until.tv_nsec-now.tv_nsec)/1000000;if(remain<=0)break;struct pollfd p={fd,POLLIN,0};int wait=(int)(remain>100?100:remain);int r=poll(&p,1,wait);if(r<=0)continue;
        uint8_t data[DISCOVERY_SIZE+32];struct sockaddr_in from;socklen_t sl=sizeof from;ssize_t got=recvfrom(fd,data,sizeof data,0,(struct sockaddr *)&from,&sl);if(got!=DISCOVERY_SIZE||memcmp(data,DISCOVERY_MAGIC,4))continue;
        char ip[INET_ADDRSTRLEN];if(!inet_ntop(AF_INET,&from.sin_addr,ip,sizeof ip))continue;if(memcmp(data+134,nonce,8))continue;uint16_t protocol=get_u16(data+4),port=get_u16(data+175);bool compat=data[174]==1&&compatible_bytes(data+6,data+70,protocol);
        bool duplicate=false;for(size_t i=0;i<n;i++)if(strstr(entries[i],ip)&&strstr(entries[i],"tcpPort")){char needle[32];snprintf(needle,sizeof needle,"\"tcpPort\":%u",port);if(strstr(entries[i],needle))duplicate=true;}if(duplicate)continue;
        char *entry=dup_printf("{\"ipv4\":\"%s\",\"address\":\"%s\",\"name\":\"%s\",\"label\":\"%s at %s\",\"tcpPort\":%u,\"protocolVersion\":%u,\"compatible\":%s}",ip,ip,NAME_TEXT,NAME_TEXT,ip,port,protocol,compat?"true":"false");if(!entry)continue;char **next=realloc(entries,(n+1)*sizeof *entries);if(!next){free(entry);break;}entries=next;entries[n++]=entry;
    }
    close(fd);if(count)*count=n;return entries;
}
