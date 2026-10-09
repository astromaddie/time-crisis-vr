#include <SDL2/SDL.h>
#include <android/log.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef TCVR_STEAM_FRAME
#include "frame_replay.h"
#endif
int tc_game_main(int,char**);
int SDL_main(int argc,char **argv) {
    (void)argc;(void)argv;
#ifdef TCVR_STEAM_FRAME
    const char *dir="/sdcard/Documents/TimeCrisisVR";
    if(access(dir,R_OK|W_OK)||chdir(dir)){
        __android_log_print(ANDROID_LOG_ERROR,"TCVR","Cannot use persistent Frame storage at %s: %s",dir,strerror(errno));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Time Crisis VR","Cannot read/write /sdcard/Documents/TimeCrisisVR. Check storage access and retry.",NULL);
        return 2;
    }
#else
    const char *dir=SDL_AndroidGetInternalStoragePath();
    if(!dir||chdir(dir))return 2;
#endif
#ifdef TCVR_STEAM_FRAME
    char *args[6];int game_argc=0;
    int replay=tcvr_frame_select_launch(args,&game_argc);
    if(replay<0){
        __android_log_print(ANDROID_LOG_ERROR,"TCVR","Refusing diagnostic replay: %s (%s)",TCVR_FRAME_REPLAY_INPUT,strerror(errno));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Time Crisis VR","Diagnostic replay input is missing or invalid. Persistent session recordings were left untouched.",NULL);
        return 2;
    }
    setenv("TCVR_FRAME_REPLAY",replay?"1":"0",1);
#endif
    remove("timecris-vr-previous.log");rename("timecris-vr.log","timecris-vr-previous.log");
#ifndef TCVR_STEAM_FRAME
    remove("previous-session.inputs");rename("last-session.inputs","previous-session.inputs");
#endif
    freopen("timecris-vr.log","w",stderr);setvbuf(stderr,NULL,_IOLBF,0);
    setenv("SDL_ACCELEROMETER_AS_JOYSTICK","0",1);
    setenv("ENG_HUD_CENTER","1",1);
    setenv("ENG_OUTPUT_GAIN","2.4",1);
    /* Fine-grained engine timers call the clock several times per polygon.
     * Keep them opt-in; the lightweight XR frame/stage counters stay enabled. */
    if(!access("profile.request",F_OK))setenv("ENG_FTIME","1",1);else unsetenv("ENG_FTIME");
    /* Bound cold texture work per original game frame. Upstream refines coarse
     * entries over subsequent frames; both stereo eyes share the same budget. */
    setenv("ENG_TEX_BUDGET","100000:100000",1);
    /* The engine records controller-derived arcade inputs, not video or audio.
     * This permits exact replay of a reported crash, even after restarting once. */
#ifdef TCVR_STEAM_FRAME
    if(replay)fprintf(stderr,"[vr] replay source=%s; session recording rotation disabled\n",TCVR_FRAME_REPLAY_INPUT);
    else fprintf(stderr,"[vr] normal session recording enabled: last-session.inputs\n");
    int result=tc_game_main(game_argc,args);
#else
    char *args[]={"TimeCrisisVR","roms","--window","--record","last-session.inputs",NULL};
    int result=tc_game_main(5,args);
#endif
    __android_log_print(ANDROID_LOG_ERROR,"TCVR","Game ended: %d; see files/timecris-vr.log",result);
    if(result) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Time Crisis VR","Startup failed. Log: files/timecris-vr.log (adb run-as).",NULL);
    return result;
}
