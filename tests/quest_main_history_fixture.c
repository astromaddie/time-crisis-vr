#include "../quest/quest_coop.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool cooperative;
static const char *storage;
static int saved_argc;
static unsigned cancel_calls;
static char saved_argv[8][128];
const char *SDL_AndroidGetInternalStoragePath(void){return storage;}
int SDL_ShowSimpleMessageBox(unsigned flags,const char *title,const char *message,void *window){(void)flags;(void)title;(void)message;(void)window;return 0;}
int __android_log_print(int priority,const char *tag,const char *format,...){(void)priority;(void)tag;(void)format;return 0;}
bool qcoop_requested(void){return cooperative;}
bool qcoop_is_failed(void){return false;}
void qcoop_copy_display_status(char *out,size_t size){if(size)out[0]=0;}
void qcoop_cancel(bool force_solo){assert(!force_solo);cancel_calls++;}
int tc_game_main(int argc,char **argv){saved_argc=argc;for(int i=0;i<argc&&i<8;i++)snprintf(saved_argv[i],sizeof saved_argv[i],"%s",argv[i]);return 0;}
int SDL_main(int argc,char **argv);
static void expect_file(const char *name,const char *expected){FILE *f=fopen(name,"rb");assert(f);char data[128]={0};size_t n=fread(data,1,sizeof data-1,f);assert(fclose(f)==0);assert(n==strlen(expected)&&!memcmp(data,expected,n));}
int main(int argc,char **argv){
    assert(argc==2);storage=argv[1];
    cooperative=true;assert(SDL_main(0,NULL)==0);assert(cancel_calls==1);assert(saved_argc==3);assert(!strcmp(saved_argv[0],"TimeCrisisVR")&&!strcmp(saved_argv[1],"roms")&&!strcmp(saved_argv[2],"--window"));
    expect_file("last-session.inputs","solo-current");expect_file("previous-session.inputs","solo-prior");
    cooperative=false;assert(SDL_main(0,NULL)==0);assert(cancel_calls==1);assert(saved_argc==5);assert(!strcmp(saved_argv[3],"--record")&&!strcmp(saved_argv[4],"last-session.inputs"));
    expect_file("previous-session.inputs","solo-current");assert(access("last-session.inputs",F_OK)!=0);
    puts("PASS: co-op startup preserves both Solo input recordings; the next Solo launch keeps its original rotation and record path.");return 0;
}
