#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "frame_replay.h"
#include "ss22_board.h"

void ss22_eeprom_save(void);
void ss22_eeprom_persist(const char *path);

static bool file_is(const char *path,const char *expected){
    FILE *file=fopen(path,"rb");if(!file)return false;
    char value[128]={0};size_t n=fread(value,1,sizeof value-1,file);fclose(file);
    return n==strlen(expected)&&!memcmp(value,expected,n);
}

int main(int argc,char **argv){
    assert(argc==2);char *game_argv[6]={"untouched",NULL,NULL,NULL,NULL,NULL};int game_argc=-9;
    int result=tcvr_frame_select_launch(game_argv,&game_argc);
    if(!strcmp(argv[1],"normal")){
        assert(result==0&&game_argc==5);
        assert(!strcmp(game_argv[0],"TimeCrisisVR")&&!strcmp(game_argv[3],"--record")&&
               !strcmp(game_argv[4],"last-session.inputs"));
        assert(file_is("previous-session.inputs","latest-session")&&access("last-session.inputs",F_OK)!=0);
        assert(access(TCVR_FRAME_REPLAY_MARKER,F_OK)!=0);
        puts("PASS: normal Frame startup rotates exactly the latest session and selects --record");
    }else if(!strcmp(argv[1],"replay")){
        assert(result==1&&game_argc==5);
        assert(!strcmp(game_argv[0],"TimeCrisisVR")&&!strcmp(game_argv[3],"--replay")&&
               !strcmp(game_argv[4],TCVR_FRAME_REPLAY_INPUT));
        for(int i=0;i<game_argc;i++)assert(strcmp(game_argv[i],"--record"));
        assert(access(TCVR_FRAME_REPLAY_MARKER,F_OK)!=0);
        assert(file_is("last-session.inputs","latest-session")&&file_is("previous-session.inputs","previous-session"));
        assert(!tcvr_frame_replay_eeprom_persist_allowed(true));
        assert(tcvr_frame_replay_eeprom_persist_allowed(false));
        if(tcvr_frame_replay_eeprom_persist_allowed(true))ss22_eeprom_persist("replay-eeprom-should-not-exist.nv");
        g_ss22.eeprom[0]=0x5a;ss22_eeprom_save();
        assert(access("replay-eeprom-should-not-exist.nv",F_OK)!=0);
        puts("PASS: validated replay selects --replay without rotation/recording, and the production EEPROM policy leaves periodic save inert");
    }else{
        assert(!strcmp(argv[1],"invalid")||!strcmp(argv[1],"truncated"));
        assert(result<0&&game_argc==-9&&!strcmp(game_argv[0],"untouched"));
        const char *bad_source=!strcmp(argv[1],"invalid")?"not a TC replay\n":"SS22REC 1 TC 00\n";
        assert(access(TCVR_FRAME_REPLAY_MARKER,F_OK)==0&&file_is(TCVR_FRAME_REPLAY_INPUT,bad_source));
        assert(file_is("last-session.inputs","latest-session")&&file_is("previous-session.inputs","previous-session"));
        puts("PASS: malformed replay is rejected before argument selection and preserves marker, source and session recordings");
    }
    return 0;
}
