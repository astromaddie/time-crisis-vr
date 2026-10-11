/* Frame's launch UI runs inside OpenXR, before game/EEPROM initialization.
 * The existing transport owns compatibility and the fresh-run start barrier. */
#include "frame_lobby.h"
#include "quest_coop.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { HOME,HOST,JOIN,WAIT,KEYPAD };
static int page;
static bool done,searching,search_ready;
static pthread_t search_thread;
static pthread_mutex_t search_lock=PTHREAD_MUTEX_INITIALIZER;
static char **found;
static size_t found_count;
static QLobbyView view;
static char address[16],local_ip[40],message[120];
enum { JOB_NONE,JOB_HOST,JOB_JOIN,JOB_SOLO,JOB_BACK };
static pthread_t action_thread;
static bool action_busy,action_ready;
static int action_job;
static int host_port=QCOOP_DEFAULT_TCP_PORT;
static void field(const char *json,const char *name,char *out,size_t size){
    char key[64];snprintf(key,sizeof key,"\"%s\":\"",name);const char *p=strstr(json,key);
    if(!p){out[0]=0;return;}p+=strlen(key);size_t n=strcspn(p,"\"");if(n>=size)n=size-1;memcpy(out,p,n);out[n]=0;
}
static void status(const char *s){
    memset(view.status,0,sizeof view.status);
    for(int row=0;row<3&&*s;row++){
        size_t n=strlen(s),take=n<36?n:36;
        if(take<n){size_t split=take;while(split&&s[split]!=' ')split--;if(split>12)take=split;}
        memcpy(view.status[row],s,take);s+=take;while(*s==' ')s++;
    }
}
static void button(const char *label,bool enabled,int x,int y,int w){
    if(view.count==QLOBBY_MAX_BUTTONS)return;
    QLobbyButton *b=&view.buttons[view.count++];*b=(QLobbyButton){.x=x,.y=y,.w=w,.h=44,.enabled=enabled};snprintf(b->label,sizeof b->label,"%s",label);
}
static void row(const char *s,bool enabled){button(s,enabled,24,160+view.count*50,592);}
static void clear_found(void){for(size_t i=0;i<found_count;i++)free(found[i]);free(found);found=NULL;found_count=0;}
static void *discover(void *unused){
    (void)unused;size_t n=0;char **list=qcoop_discover("roms",QCOOP_DEFAULT_DISCOVERY_PORT,1000,&n);
    pthread_mutex_lock(&search_lock);found=list;found_count=n;search_ready=true;pthread_mutex_unlock(&search_lock);return NULL;
}
static void search(void){
    if(searching)return;clear_found();search_ready=false;searching=true;
    if(pthread_create(&search_thread,NULL,discover,NULL)){searching=false;snprintf(message,sizeof message,"Search unavailable. Use direct IPv4.");}
}
static void *perform_action(void *unused){
    (void)unused;
    if(action_job==JOB_HOST)qcoop_start_host("roms",QCOOP_DEFAULT_TCP_PORT,QCOOP_DEFAULT_DISCOVERY_PORT);
    else if(action_job==JOB_JOIN)qcoop_start_join("roms",address,host_port,QCOOP_DEFAULT_DISCOVERY_PORT);
    else qcoop_cancel(action_job==JOB_SOLO);
    pthread_mutex_lock(&search_lock);action_ready=true;pthread_mutex_unlock(&search_lock);return NULL;
}
static void action(int job){
    if(action_busy)return;action_job=job;action_busy=true;action_ready=false;
    if(pthread_create(&action_thread,NULL,perform_action,NULL)){action_busy=false;snprintf(message,sizeof message,"Could not start LAN action. Retry.");}
}
void qlobby_begin(void){
    done=false;page=HOME;view=(QLobbyView){0};address[0]=message[0]=0;
    size_t n=0;char **ips=qcoop_local_ipv4_addresses(&n);snprintf(local_ip,sizeof local_ip,"%s",n?ips[0]:"NO LAN ADDRESS");
    for(size_t i=0;i<n;i++)free(ips[i]);free(ips);qlobby_poll();
}
void qlobby_poll(void){
    if(searching){pthread_mutex_lock(&search_lock);bool ready=search_ready;pthread_mutex_unlock(&search_lock);if(ready){pthread_join(search_thread,NULL);searching=false;}}
    if(action_busy){
        pthread_mutex_lock(&search_lock);bool ready=action_ready;pthread_mutex_unlock(&search_lock);
        if(ready){pthread_join(action_thread,NULL);action_busy=false;
            if(action_job==JOB_SOLO)done=true;
            else if(action_job==JOB_HOST)page=HOST;
            else if(action_job==JOB_JOIN)page=WAIT;
            else page=HOME;
            view.selected=0;
        }
    }
    int selected=view.selected;view.count=0;
    snprintf(view.title,sizeof view.title,"%s",page==HOME?"TIME CRISIS VR":page==HOST?"HOST / PLAYER 1":page==KEYPAD?"ENTER HOST IPV4":"JOIN / PLAYER 2");
    if(action_busy){status("Preparing session... Please wait.");row("PLEASE WAIT",false);}
    else if(page==HOME){status("Choose a solo game or a shared LAN run.");row("SOLO",true);row("HOST / PLAYER 1",true);row("JOIN / PLAYER 2",true);}
    else if(page==HOST||page==WAIT){
        char *json=qcoop_status_json();char state[24],info[120];field(json?json:"","state",state,sizeof state);field(json?json:"","message",info,sizeof info);
        if(!strcmp(state,"starting"))done=true;
        char text[160];snprintf(text,sizeof text,"%s%s%s",page==HOST?local_ip:"",page==HOST?" - ":"",info[0]?info:"Waiting for player...");status(text);
        if(page==HOST)row("START SHARED RUN",json&&strstr(json,"\"canStart\":true"));
        else row("WAITING FOR HOST TO START",false);
        row("BACK",true);free(json);
    }else if(page==JOIN){
        status(searching?"Searching this LAN...":message[0]?message:found_count?"Choose a compatible host.":"No host found. Rescan or enter IPv4.");
        if(!searching)for(size_t i=0;i<found_count&&i<3;i++){char ip[40];field(found[i],"ipv4",ip,sizeof ip);row(ip,strstr(found[i],"\"compatible\":true")!=NULL);}
        row("RESCAN",!searching);row("DIRECT IPV4",true);row("BACK",true);
    }else{
        char info[100];snprintf(info,sizeof info,"%s",address[0]?address:"Host address: use digits and dots.");status(info);
        const char *keys[]={"1","2","3","4","5","6","7","8","9",".","0","DEL"};
        for(int i=0;i<12;i++)button(keys[i],true,24+(i%3)*150,160+(i/3)*50,140);
        button("JOIN",address[0]!=0,480,160,136);button("BACK",true,480,210,136);
    }
    view.selected=selected<view.count?selected:0;
}
const QLobbyView *qlobby_view(void){return &view;}
bool qlobby_done(void){return done;}
bool qlobby_hover(int x,int y){for(int i=0;i<view.count;i++){QLobbyButton *b=&view.buttons[i];if(x>=b->x&&x<b->x+b->w&&y>=b->y&&y<b->y+b->h){view.selected=i;return true;}}return false;}
void qlobby_move(int x,int y){
    if(!view.count)return;
    /* Spatial navigation keeps the keypad and the stacked session rows usable. */
    QLobbyButton *a=&view.buttons[view.selected];int best=-1,score=1000000;
    for(int i=0;i<view.count;i++){QLobbyButton *b=&view.buttons[i];int dx=(b->x+b->w/2)-(a->x+a->w/2),dy=b->y-a->y;
        int along=x?dx*x:dy*y,across=abs(x?dy:dx);if(along<=0)continue;int cost=along+across*4;if(cost<score){score=cost;best=i;}}
    if(best>=0)view.selected=best;
}
void qlobby_back(void){
    if(page==HOME||action_busy)return;
    if(page==HOST||page==WAIT){action(JOB_BACK);qlobby_poll();return;}
    page=page==KEYPAD?JOIN:HOME;view.selected=0;message[0]=0;qlobby_poll();
}
void qlobby_select(void){
    if(view.selected<0||view.selected>=view.count||!view.buttons[view.selected].enabled)return;
    char label[40];snprintf(label,sizeof label,"%s",view.buttons[view.selected].label);
    if(!strcmp(label,"BACK")){qlobby_back();return;}
    if(page==HOME){
        if(view.selected==0){action(JOB_SOLO);}
        else if(view.selected==1){action(JOB_HOST);}
        else{page=JOIN;search();}
    }else if(page==HOST){qcoop_host_start();}
    else if(page==JOIN){
        if(!strcmp(label,"RESCAN"))search();
        else if(!strcmp(label,"DIRECT IPV4")){page=KEYPAD;host_port=QCOOP_DEFAULT_TCP_PORT;}
        else{snprintf(address,sizeof address,"%s",label);const char *p=strstr(found[view.selected],"\"tcpPort\":");host_port=p?atoi(p+10):QCOOP_DEFAULT_TCP_PORT;action(JOB_JOIN);}
    }else if(page==KEYPAD){
        size_t n=strlen(address);
        if(!strcmp(label,"JOIN")){action(JOB_JOIN);}
        else if(!strcmp(label,"DEL")){if(n)address[n-1]=0;}
        else if(n<sizeof address-1){address[n]=label[0];address[n+1]=0;}
        qlobby_poll();return;
    }
    view.selected=0;qlobby_poll();
}
void qlobby_close(void){if(action_busy){pthread_join(action_thread,NULL);action_busy=false;}if(searching){pthread_join(search_thread,NULL);searching=false;}clear_found();}
