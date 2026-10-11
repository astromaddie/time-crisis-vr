#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "../quest/quest_coop.h"
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void write_all(int fd,const void *source,size_t size){const char *p=source;while(size){ssize_t n=write(fd,p,size);if(n<0&&errno==EINTR)continue;assert(n>0);p+=n;size-=(size_t)n;}}
static void read_all(int fd,void *target,size_t size){char *p=target;while(size){ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;assert(n>0);p+=n;size-=(size_t)n;}}
static int reserve_port(void){int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_in a={0};a.sin_family=AF_INET;assert(bind(fd,(struct sockaddr *)&a,sizeof a)==0);socklen_t n=sizeof a;assert(getsockname(fd,(struct sockaddr *)&a,&n)==0);int port=ntohs(a.sin_port);close(fd);return port;}
static long elapsed_ms(struct timespec a,struct timespec b){return (b.tv_sec-a.tv_sec)*1000+(b.tv_nsec-a.tv_nsec)/1000000;}
static bool wait_state(const char *needle,int timeout_ms){struct timespec begin,now;clock_gettime(CLOCK_MONOTONIC,&begin);do{char *json=qcoop_status_json();bool found=json&&strstr(json,needle);free(json);if(found)return true;usleep(1000);clock_gettime(CLOCK_MONOTONIC,&now);}while(elapsed_ms(begin,now)<timeout_ms);return false;}
static void child(int commands,int replies,int tcp,int discovery){
    char command;read_all(commands,&command,1);assert(command=='S');assert(qcoop_start_join(NULL,"127.0.0.1",tcp,discovery));
    assert(wait_state("\"state\":\"ready\"",3000));command='R';write_all(replies,&command,1);
    assert(wait_state("\"state\":\"starting\"",3000));uint8_t eeprom[16]={0};assert(qcoop_engine_boot(eeprom,sizeof eeprom,false,false)==1);command='B';write_all(replies,&command,1);
    for(;;){read_all(commands,&command,1);if(command=='Q'){char state=qcoop_is_failed()?'E':'R';write_all(replies,&state,1);char message[192];qcoop_copy_display_status(message,sizeof message);write_all(replies,message,sizeof message);}else if(command=='X'){qcoop_cancel(false);_exit(0);}else _exit(21);}
}
int main(void){
    int tcp=reserve_port(),discovery=reserve_port();while(tcp==discovery)discovery=reserve_port();int commands[2],replies[2];assert(pipe(commands)==0&&pipe(replies)==0);pid_t pid=fork();assert(pid>=0);
    if(pid==0){close(commands[1]);close(replies[0]);child(commands[0],replies[1],tcp,discovery);_exit(0);}
    close(commands[0]);close(replies[1]);assert(qcoop_start_host(NULL,tcp,discovery));char signal='S';write_all(commands[1],&signal,1);read_all(replies[0],&signal,1);assert(signal=='R');assert(wait_state("\"state\":\"ready\"",3000));assert(qcoop_host_start());uint8_t eeprom[16]={1,2,3};assert(qcoop_engine_boot(eeprom,sizeof eeprom,false,false)==1);read_all(replies[0],&signal,1);assert(signal=='B');
    assert(wait_state("\"state\":\"error\"",QCOOP_STALL_TIMEOUT_MS+3000));char host_message[192];qcoop_copy_display_status(host_message,sizeof host_message);
    int status;assert(waitpid(pid,&status,WNOHANG)==0);signal='Q';write_all(commands[1],&signal,1);char peer_state;read_all(replies[0],&peer_state,1);assert(peer_state=='E');char peer_message[192];read_all(replies[0],peer_message,sizeof peer_message);
    bool host_stalled=strstr(host_message,"Shared game stopped")!=NULL,peer_stalled=strstr(peer_message,"Shared game stopped")!=NULL;
    bool host_disconnected=strstr(host_message,"disconnected")!=NULL,peer_disconnected=strstr(peer_message,"disconnected")!=NULL;
    assert(host_message[0]&&peer_message[0]);assert(host_stalled||peer_stalled);assert(host_stalled||host_disconnected);assert(peer_stalled||peer_disconnected);
    signal='X';write_all(commands[1],&signal,1);assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);qcoop_cancel(false);close(commands[1]);close(replies[0]);
    puts("PASS: a connected peer that stops producing ticks causes a visible bounded stall error on both production LAN services.");return 0;
}
