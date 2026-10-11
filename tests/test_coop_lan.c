#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "../quest/quest_coop.h"
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define EEPROM_BYTES 0x2000
#define TEST_TIMEOUT_MS 6000

enum { CMD_TICK=1, CMD_PAUSE, CMD_RESUME, CMD_DISCONNECT, CMD_CAPTURE, CMD_STATE };
typedef struct Command {
    int kind;
    uint8_t trigger, exposed, aim_valid, offscreen, coin;
    uint16_t gun_x, gun_y;
} Command;
typedef struct Reply { int kind; QCoopTick tick; } Reply;

static void write_all(int fd,const void *source,size_t size){
    const uint8_t *p=source;while(size){ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;assert(n>0);p+=n;size-=(size_t)n;}
}
static void read_all(int fd,void *target,size_t size){
    uint8_t *p=target;while(size){ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;assert(n>0);p+=n;size-=(size_t)n;}
}
static int reserve_port(void){
    int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_in address={0};address.sin_family=AF_INET;
    assert(bind(fd,(struct sockaddr *)&address,sizeof address)==0);socklen_t size=sizeof address;assert(getsockname(fd,(struct sockaddr *)&address,&size)==0);
    int port=ntohs(address.sin_port);close(fd);return port;
}
static long elapsed_ms(struct timespec start,struct timespec now){return (now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000;}
static bool wait_state(const char *wanted,int timeout_ms){
    struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
    do{char *json=qcoop_status_json();bool found=json&&strstr(json,wanted);free(json);if(found)return true;usleep(1000);clock_gettime(CLOCK_MONOTONIC,&now);}while(elapsed_ms(start,now)<timeout_ms);
    return false;
}
static QCoopInput to_input(Command command){
    QCoopInput input={0};input.trigger=command.trigger!=0;input.exposed=command.exposed!=0;input.aim_valid=command.aim_valid!=0;
    input.offscreen=command.offscreen!=0;input.coin=command.coin!=0;input.gun_x=command.gun_x;input.gun_y=command.gun_y;return input;
}
static void child_process(int commands,int replies,int tcp,int discovery){
    char start;read_all(commands,&start,1);assert(start=='S');
    if(!qcoop_start_join(NULL,"127.0.0.1",tcp,discovery))_exit(11);
    if(!wait_state("\"state\":\"ready\"",TEST_TIMEOUT_MS))_exit(12);
    char ready='R';write_all(replies,&ready,1);
    if(!wait_state("\"state\":\"starting\"",TEST_TIMEOUT_MS))_exit(13);
    uint8_t eeprom[EEPROM_BYTES];memset(eeprom,0,sizeof eeprom);
    if(qcoop_engine_boot(eeprom,sizeof eeprom,false,false)!=1)_exit(14);
    for(size_t i=0;i<sizeof eeprom;i++)if(eeprom[i]!=(uint8_t)(i*37u+11u))_exit(15);
    char boot='B';write_all(replies,&boot,1);
    for(;;){Command command;read_all(commands,&command,sizeof command);
        if(command.kind==CMD_TICK){qcoop_capture_local(to_input(command));struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
            while(!qcoop_try_tick()){if(qcoop_is_failed())_exit(16);usleep(1000);clock_gettime(CLOCK_MONOTONIC,&now);if(elapsed_ms(start,now)>TEST_TIMEOUT_MS)_exit(17);}
            Reply reply={.kind=CMD_TICK};if(!qcoop_current_input(&reply.tick))_exit(18);write_all(replies,&reply,sizeof reply);
        }else if(command.kind==CMD_PAUSE){qcoop_set_local_ready(false);char response='P';write_all(replies,&response,1);
        }else if(command.kind==CMD_RESUME){qcoop_set_local_ready(true);char response='U';write_all(replies,&response,1);
        }else if(command.kind==CMD_DISCONNECT){qcoop_cancel(false);char response='D';write_all(replies,&response,1);_exit(0);
        }else if(command.kind==CMD_CAPTURE){qcoop_capture_local(to_input(command));char response='C';write_all(replies,&response,1);
        }else if(command.kind==CMD_STATE){char response=qcoop_is_failed()?'E':qcoop_is_paused()?'P':'R';write_all(replies,&response,1);
        }else _exit(19);
    }
}
static bool tick_equal(QCoopTick a,QCoopTick b){return a.fire==b.fire&&a.exposed==b.exposed&&a.coin==b.coin&&a.gun_x==b.gun_x&&a.gun_y==b.gun_y&&a.offscreen==b.offscreen;}
static QCoopTick exchange_tick(int commands,int replies,QCoopInput host_input,QCoopInput join_input){
    Command command={.kind=CMD_TICK,.trigger=join_input.trigger,.exposed=join_input.exposed,.aim_valid=join_input.aim_valid,.offscreen=join_input.offscreen,.coin=join_input.coin,.gun_x=join_input.gun_x,.gun_y=join_input.gun_y};
    write_all(commands,&command,sizeof command);qcoop_capture_local(host_input);
    struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
    while(!qcoop_try_tick()){assert(!qcoop_is_failed());usleep(1000);clock_gettime(CLOCK_MONOTONIC,&now);assert(elapsed_ms(start,now)<TEST_TIMEOUT_MS);}
    Reply host={.kind=CMD_TICK};assert(qcoop_current_input(&host.tick));Reply join;read_all(replies,&join,sizeof join);assert(join.kind==CMD_TICK);assert(tick_equal(host.tick,join.tick));return host.tick;
}
static QCoopTick exchange_origin_tick(int commands,int replies,QCoopInput host_edge,QCoopInput host_release,QCoopInput join_release){
    qcoop_capture_local(host_edge);
    qcoop_capture_local(host_release);
    qcoop_capture_local((QCoopInput){.trigger=true,.exposed=true,.aim_valid=true,.gun_x=888,.gun_y=111});
    qcoop_capture_local(host_release); /* The first unconsumed edge must retain its own aim. */
    Command capture={.kind=CMD_CAPTURE,.trigger=1,.exposed=1,.aim_valid=1,.gun_x=777,.gun_y=333};
    write_all(commands,&capture,sizeof capture);char ack;read_all(replies,&ack,1);assert(ack=='C');
    qcoop_capture_local(host_release);capture=(Command){.kind=CMD_CAPTURE,.trigger=0,.exposed=0,.aim_valid=1,.gun_x=901,.gun_y=222};
    write_all(commands,&capture,sizeof capture);read_all(replies,&ack,1);assert(ack=='C');
    Command tick={.kind=CMD_TICK,.trigger=join_release.trigger,.exposed=join_release.exposed,.aim_valid=join_release.aim_valid,.gun_x=join_release.gun_x,.gun_y=join_release.gun_y};
    write_all(commands,&tick,sizeof tick);
    struct timespec start,now;clock_gettime(CLOCK_MONOTONIC,&start);
    while(!qcoop_try_tick()){assert(!qcoop_is_failed());usleep(1000);clock_gettime(CLOCK_MONOTONIC,&now);assert(elapsed_ms(start,now)<TEST_TIMEOUT_MS);}
    Reply host={.kind=CMD_TICK};assert(qcoop_current_input(&host.tick));Reply join;read_all(replies,&join,sizeof join);assert(join.kind==CMD_TICK);assert(tick_equal(host.tick,join.tick));return host.tick;
}
static void test_launch_guards(void){
    char *normal[]={"game","roms","--window","1200","--fullscreen"};assert(qcoop_launch_arguments_supported(5,normal));
    char *record[]={"game","roms","--record","last-session.inputs"};assert(!qcoop_launch_arguments_supported(4,record));
    char *replay[]={"game","roms","--replay","last-session.inputs"};assert(!qcoop_launch_arguments_supported(4,replay));
    char *override[]={"game","roms","--autoplay"};assert(!qcoop_launch_arguments_supported(3,override));
    assert(setenv("TCVR_FRAME_REPLAY","0",1)==0);assert(!qcoop_frame_diagnostics_pending());
    assert(setenv("TCVR_FRAME_REPLAY","1",1)==0);assert(qcoop_frame_diagnostics_pending());assert(unsetenv("TCVR_FRAME_REPLAY")==0);
    char old[1024];assert(getcwd(old,sizeof old));char directory[128];snprintf(directory,sizeof directory,"/tmp/tcvr-diagnostics-%ld",(long)getpid());assert(mkdir(directory,0700)==0);assert(chdir(directory)==0);
    assert(!qcoop_frame_diagnostics_pending());int marker=open("vrdiag.once",O_CREAT|O_WRONLY,0600);assert(marker>=0);close(marker);assert(qcoop_frame_diagnostics_pending());assert(unlink("vrdiag.once")==0);
    marker=open("vrdiag-replay.once",O_CREAT|O_WRONLY,0600);assert(marker>=0);close(marker);assert(qcoop_frame_diagnostics_pending());assert(unlink("vrdiag-replay.once")==0);
    assert(chdir(old)==0);assert(rmdir(directory)==0);
}
static void test_arbitration(void){
    QCoopArbiter arbiter={0};arbiter.neutral_rearm[0]=arbiter.neutral_rearm[1]=true;
    QCoopInput players[2]={{.trigger=true,.exposed=true,.aim_valid=true,.gun_x=100,.gun_y=200,.coin=true},
                           {.trigger=true,.exposed=true,.aim_valid=true,.offscreen=true,.gun_x=700,.gun_y=400}};
    QCoopTick tick;qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire&&tick.exposed&&tick.coin);
    players[0].trigger=players[1].trigger=false;qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire);
    players[0].trigger=players[1].trigger=true;qcoop_arbitrate(&arbiter,players,&tick);assert(tick.fire&&tick.gun_x==100&&!tick.offscreen);
    qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire&&tick.exposed); /* mandatory full release tick */
    qcoop_arbitrate(&arbiter,players,&tick);assert(tick.fire&&tick.gun_x==700&&tick.offscreen); /* queued simultaneous shot */
    memset(&arbiter,0,sizeof arbiter);arbiter.neutral_rearm[0]=arbiter.neutral_rearm[1]=true;
    players[0]=(QCoopInput){.trigger=true,.exposed=false,.aim_valid=true,.gun_x=123};players[1]=(QCoopInput){.trigger=true,.exposed=true,.aim_valid=true,.gun_x=456};
    qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire);players[0].trigger=players[1].trigger=false;qcoop_arbitrate(&arbiter,players,&tick);
    players[0].trigger=players[1].trigger=true;qcoop_arbitrate(&arbiter,players,&tick);assert(tick.fire&&tick.gun_x==456&&tick.exposed);
    players[0].exposed=true;players[1].exposed=false;qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire); /* held shot is not re-issued */
    memset(&arbiter,0,sizeof arbiter);arbiter.neutral_rearm[0]=arbiter.neutral_rearm[1]=true;
    players[0]=(QCoopInput){.trigger=true,.aim_valid=true,.gun_x=125};players[1]=(QCoopInput){.aim_valid=true,.gun_x=625};
    qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire); /* both controllers rearm at a neutral sample */
    players[0].trigger=false;qcoop_arbitrate(&arbiter,players,&tick);
    players[0].trigger=true;qcoop_arbitrate(&arbiter,players,&tick);
    assert(tick.fire&&!tick.exposed&&qcoop_to_arcade_bits(tick)==0x10); /* trigger starts/confirms while hidden, without firing */
    players[0].trigger=false;qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire);
    players[0].exposed=true;players[1].trigger=true;
    qcoop_arbitrate(&arbiter,players,&tick);assert(!tick.fire&&tick.exposed); /* covered P2 cannot shoot while P1 is exposed */
    players[1].trigger=false;qcoop_arbitrate(&arbiter,players,&tick);
    players[1].exposed=true;players[1].trigger=true;
    qcoop_arbitrate(&arbiter,players,&tick);assert(tick.fire&&tick.exposed&&qcoop_to_arcade_bits(tick)==0x30);
    assert(qcoop_to_arcade_bits((QCoopTick){.coin=true})==1);
}
int main(void){
    test_launch_guards();test_arbitration();int tcp=reserve_port(),discovery=reserve_port();while(discovery==tcp)discovery=reserve_port();
    int commands[2],replies[2];assert(pipe(commands)==0&&pipe(replies)==0);pid_t child=fork();assert(child>=0);
    if(child==0){close(commands[1]);close(replies[0]);child_process(commands[0],replies[1],tcp,discovery);_exit(0);}
    close(commands[0]);close(replies[1]);assert(qcoop_start_host(NULL,tcp,discovery));char start='S';write_all(commands[1],&start,1);
    char ready;read_all(replies[0],&ready,1);assert(ready=='R');assert(wait_state("\"state\":\"ready\"",TEST_TIMEOUT_MS));
    char *status_json=qcoop_status_json();assert(status_json&&strstr(status_json,"\"role\":\"host\"")&&strstr(status_json,"\"peerIpv4\":\"127.0.0.1\"")&&
        strstr(status_json,"\"compatible\":true")&&strstr(status_json,"\"canStart\":true")&&strstr(status_json,"\"tcpPort\":")&&strstr(status_json,"\"protocolVersion\":1"));free(status_json);
    size_t found=0;char **entries=qcoop_discover(NULL,discovery,1200,&found);assert(found>0);bool compatible=false;
    for(size_t i=0;i<found;i++){assert(strstr(entries[i],"\"ipv4\":\"")&&strstr(entries[i],"\"address\":\"")&&strstr(entries[i],"\"name\":\"")&&
        strstr(entries[i],"\"label\":\"")&&strstr(entries[i],"\"tcpPort\":")&&strstr(entries[i],"\"protocolVersion\":1"));if(strstr(entries[i],"\"compatible\":true"))compatible=true;free(entries[i]);}free(entries);assert(compatible);
    assert(qcoop_host_start());uint8_t host_eeprom[EEPROM_BYTES];for(size_t i=0;i<sizeof host_eeprom;i++)host_eeprom[i]=(uint8_t)(i*37u+11u);
    int host_boot=qcoop_engine_boot(host_eeprom,sizeof host_eeprom,false,false);if(host_boot!=1){char *state=qcoop_status_json();fprintf(stderr,"host boot=%d status=%s\n",host_boot,state?state:"(null)");free(state);}assert(host_boot==1);char boot;read_all(replies[0],&boot,1);assert(boot=='B');
    QCoopInput host={.trigger=true,.exposed=true,.aim_valid=true,.gun_x=100,.gun_y=200,.coin=true};
    QCoopInput join={.trigger=true,.exposed=true,.aim_valid=true,.offscreen=true,.gun_x=700,.gun_y=400};
    QCoopTick tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire&&tick.exposed&&tick.coin);
    host.trigger=join.trigger=false;tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire);
    host.trigger=true;host.exposed=false;host.gun_x=111;join.trigger=true;join.exposed=true;tick=exchange_tick(commands[1],replies[0],host,join);assert(tick.fire&&tick.exposed&&tick.gun_x==700&&tick.offscreen);
    host.trigger=join.trigger=false;host.exposed=join.exposed=false;tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire&&!tick.exposed);
    host.trigger=join.trigger=true;host.exposed=join.exposed=true;host.gun_x=101;join.gun_x=701;tick=exchange_tick(commands[1],replies[0],host,join);assert(tick.fire&&tick.gun_x==101);
    tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire);
    tick=exchange_tick(commands[1],replies[0],host,join);assert(tick.fire&&tick.gun_x==701);
    Command pause={.kind=CMD_PAUSE};write_all(commands[1],&pause,sizeof pause);char paused;read_all(replies[0],&paused,1);assert(paused=='P');assert(wait_state("\"state\":\"paused\"",TEST_TIMEOUT_MS));assert(qcoop_is_paused()&&!qcoop_try_tick());
    usleep((QCOOP_STALL_TIMEOUT_MS+250)*1000);assert(!qcoop_is_failed());Command state={.kind=CMD_STATE};write_all(commands[1],&state,sizeof state);char child_state;read_all(replies[0],&child_state,1);assert(child_state=='P');
    Command resume={.kind=CMD_RESUME};write_all(commands[1],&resume,sizeof resume);char resumed;read_all(replies[0],&resumed,1);assert(resumed=='U');assert(wait_state("\"state\":\"running\"",TEST_TIMEOUT_MS));
    host=(QCoopInput){0};join=(QCoopInput){0};tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire); /* neutral rearm before the next trigger edge */
    host=(QCoopInput){.trigger=true,.aim_valid=true,.gun_x=125};
    tick=exchange_tick(commands[1],replies[0],host,join);assert(tick.fire&&!tick.exposed&&qcoop_to_arcade_bits(tick)==0x10); /* Production LAN tick preserves the arcade's covered start/confirm trigger. */
    host=(QCoopInput){.aim_valid=true};tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire);
    host.exposed=true;join=(QCoopInput){.trigger=true,.aim_valid=true,.gun_x=625};
    tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire&&tick.exposed); /* A covered P2 cannot shoot during P1's exposed tick. */
    join.trigger=false;tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire&&tick.exposed);
    host=(QCoopInput){.aim_valid=true};join.exposed=true;join.trigger=true;
    tick=exchange_tick(commands[1],replies[0],host,join);assert(tick.fire&&tick.exposed&&tick.gun_x==625);
    host=(QCoopInput){0};join=(QCoopInput){0};tick=exchange_tick(commands[1],replies[0],host,join);assert(!tick.fire); /* full release tick between accepted shots */
    host=(QCoopInput){.trigger=true,.exposed=true,.aim_valid=true,.gun_x=123,.gun_y=456};
    QCoopInput released={.trigger=false,.exposed=false,.aim_valid=true,.gun_x=901,.gun_y=222};
    QCoopInput remote_released={.trigger=false,.exposed=false,.aim_valid=true,.gun_x=500,.gun_y=300};
    tick=exchange_origin_tick(commands[1],replies[0],host,released,remote_released);
    assert(tick.fire&&tick.exposed&&tick.gun_x==123&&tick.gun_y==456); /* short trigger edge retains its exposed origin aim through later release samples */
    Command disconnect={.kind=CMD_DISCONNECT};write_all(commands[1],&disconnect,sizeof disconnect);char disconnected;read_all(replies[0],&disconnected,1);assert(disconnected=='D');
    assert(wait_state("\"state\":\"error\"",TEST_TIMEOUT_MS));assert(qcoop_is_failed()&&qcoop_is_paused());char message[192];qcoop_copy_display_status(message,sizeof message);assert(strstr(message,"disconnected")||strstr(message,"connection"));
    int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);qcoop_cancel(false);assert(!qcoop_requested());close(commands[1]);close(replies[0]);
    puts("PASS: production LAN service negotiated a fresh EEPROM boot, discovered the host, synchronized canonical ticks and pause/resume, then stopped visibly on disconnect.");return 0;
}
