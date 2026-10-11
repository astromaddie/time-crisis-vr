#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include "../quest/frame_lobby.h"
#include "../quest/quest_coop.h"
#include "../quest/quest_ui.c"
static void pause_ms(void){struct timespec t={0,10000000};nanosleep(&t,NULL);}
static void choose(const char *label){const QLobbyView *v=qlobby_view();for(int i=0;i<v->count;i++)if(!strcmp(v->buttons[i].label,label)){qlobby_hover(v->buttons[i].x+2,v->buttons[i].y+2);qlobby_select();for(int n=0;n<100&&!strcmp(qlobby_view()->buttons[0].label,"PLEASE WAIT");n++){pause_ms();qlobby_poll();}return;}assert(!"missing lobby action");}
static void capture(const char *dir,const char *name){
    const QLobbyView *v=qlobby_view();
    for(int i=0;i<v->count;i++){assert(v->buttons[i].x>=0&&v->buttons[i].y>=0);assert(v->buttons[i].x+v->buttons[i].w<=WIDTH&&v->buttons[i].y+v->buttons[i].h<438);assert(strlen(v->buttons[i].label)*16+24<=(size_t)v->buttons[i].w);}
    lobby_panel=v;panel(false,false,false,true,0,true,"",false);lobby_panel=NULL;
    char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);assert(qui_capture_ppm(path));
}
int main(int argc,char **argv){assert(argc==2);
    /* Fork before any transport/discovery threads. The peer uses the real LAN
     * handshake and fresh boot barrier, independently of the native UI. */
    int ready[2];assert(pipe(ready)==0);pid_t peer=fork();assert(peer>=0);
    if(!peer){close(ready[1]);char go;assert(read(ready[0],&go,1)==1);assert(qcoop_start_join("roms","127.0.0.1",QCOOP_DEFAULT_TCP_PORT,QCOOP_DEFAULT_DISCOVERY_PORT));
        bool starting=false;for(int i=0;i<500;i++){char *s=qcoop_status_json();starting=strstr(s,"\"state\":\"starting\"")!=NULL;free(s);if(starting)break;pause_ms();}assert(starting);
        uint8_t eeprom[32]={0};assert(qcoop_engine_boot(eeprom,sizeof eeprom,false,false)==1);assert(eeprom[0]==42);qcoop_cancel(false);_exit(0);
    }
    close(ready[0]);qlobby_begin();capture(argv[1],"home");
    choose("JOIN / PLAYER 2");choose("DIRECT IPV4");
    const char *keys[]={"1","2","7",".","0",".","0",".","1"};for(int i=0;i<9;i++)choose(keys[i]);capture(argv[1],"keypad");
    assert(!strcmp(qlobby_view()->status[0],"127.0.0.1"));choose("DEL");choose("1");choose("BACK");
    for(int i=0;i<150;i++){pause_ms();qlobby_poll();}capture(argv[1],"discovery");choose("BACK");
    choose("HOST / PLAYER 1");capture(argv[1],"host-waiting");assert(!qlobby_view()->buttons[0].enabled);choose("START SHARED RUN");assert(!qlobby_done());
    assert(write(ready[1],"x",1)==1);close(ready[1]);
    for(int i=0;i<500&&!qlobby_view()->buttons[0].enabled;i++){pause_ms();qlobby_poll();}
    assert(qlobby_view()->buttons[0].enabled);capture(argv[1],"host-ready");choose("START SHARED RUN");assert(qlobby_done());
    uint8_t eeprom[32]={42};assert(qcoop_engine_boot(eeprom,sizeof eeprom,false,false)==1);qlobby_close();
    int status;assert(waitpid(peer,&status,0)==peer&&WIFEXITED(status)&&WEXITSTATUS(status)==0);qcoop_cancel(false);
    qlobby_begin();choose("SOLO");assert(qlobby_done()&&!qcoop_requested());qlobby_close();
    puts("PASS: production native lobby renders all pages, edits IPv4, gates Host start on compatible peer, crosses fresh-run barrier and selects Solo");
}
