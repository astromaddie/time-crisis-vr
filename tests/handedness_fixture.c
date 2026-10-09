/* Exercise the real host input path against a deterministic OpenXR runtime.
 * Render/startup entry points are excluded; no headset is required. */
#include <assert.h>
#define TCVR_INPUT_TEST
#include "../quest/quest_host.c"

static char paths[200][100],action_names[32][64],profile_names[3][100];
static unsigned path_count,action_count,space_count,binding_count;
static XrActionSuggestedBinding suggested[64],profile_bindings[3][64];
static unsigned profile_counts[3];
static XrActionType types[32];
static bool dual[32],buttons[32][3],connected[2]={true,true},tracked[2]={true,true};
static XrPath current_profile;
static float stick_y;
static bool stick_active=true;
static Q4 controller_rotation={0,0,0,1};
static float triggers[2],grips[2];
static XrTime trigger_times[2];
static bool trigger_active[2]={true,true};
static unsigned space_kind[5],space_hand[5];
static XrTime pose_locate_time[2][2];
static XrResult pose_locate_result[2][2];
static XrPosef pose_values[2][2];
static XrSpaceLocationFlags pose_flags[2][2];
static bool pose_active[2][2]={{true,true},{true,true}},custom_pose_values;
static XrPath last_haptic,last_stop;
static XrResult sync_result=XR_SUCCESS;
static uint16_t arcade_bits;
static float reported_rate=72;
static unsigned rate_requests;
static unsigned haptic_calls;
static XrResult XRAPI_CALL mock_get_rate(XrSession s,float *rate){(void)s;*rate=reported_rate;return XR_SUCCESS;}
static XrResult XRAPI_CALL mock_request_rate(XrSession s,float rate){(void)s;assert(rate==120);rate_requests++;return XR_SUCCESS;}
uint16_t g_ss22_gun_x,g_ss22_gun_y;
bool g_ss22_gun_off;
static unsigned id(XrAction a){return (unsigned)(uintptr_t)a;}
static int hand_index(XrPath p){
    if(p==XR_NULL_PATH)return 2;
    if(p==hand_paths[LEFT_HAND])return LEFT_HAND;
    assert(p==hand_paths[RIGHT_HAND]);return RIGHT_HAND;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(XrInstance i,const char *s,XrPath *p){
    (void)i;for(unsigned n=1;n<=path_count;n++)if(!strcmp(paths[n],s)){*p=n;return XR_SUCCESS;}
    assert(path_count+1<200);*p=++path_count;snprintf(paths[*p],100,"%s",s);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(XrInstance i,XrResult r,char buffer[XR_MAX_RESULT_STRING_SIZE]){
    (void)i;snprintf(buffer,XR_MAX_RESULT_STRING_SIZE,"Mock result %d",r);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(XrInstance i,const XrActionSetCreateInfo *c,XrActionSet *out){
    (void)i;(void)c;*out=(XrActionSet)(uintptr_t)1;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(XrActionSet set,const XrActionCreateInfo *c,XrAction *out){
    (void)set;unsigned n=++action_count;assert(n<32);types[n]=c->actionType;snprintf(action_names[n],sizeof action_names[n],"%s",c->actionName);
    dual[n]=c->countSubactionPaths==2;
    if(dual[n]){assert(c->subactionPaths[0]==hand_paths[0]);assert(c->subactionPaths[1]==hand_paths[1]);}
    else assert(c->countSubactionPaths==0);
    *out=(XrAction)(uintptr_t)n;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(XrInstance i,const XrInteractionProfileSuggestedBinding *s){
    (void)i;const char *profile=paths[s->interactionProfile];unsigned slot;
    if(!strcmp(profile,"/interaction_profiles/valve/frame_controller_valve"))slot=0;
    else if(!strcmp(profile,"/interaction_profiles/oculus/touch_controller"))slot=1;
    else if(!strcmp(profile,"/interaction_profiles/valve/index_controller"))slot=2;
    else {fprintf(stderr,"Unexpected interaction profile: %s\n",profile);abort();}
    snprintf(profile_names[slot],sizeof profile_names[slot],"%s",profile);
    assert(s->countSuggestedBindings<=64);profile_counts[slot]=s->countSuggestedBindings;
    memcpy(profile_bindings[slot],s->suggestedBindings,s->countSuggestedBindings*sizeof profile_bindings[slot][0]);
    binding_count=s->countSuggestedBindings;memcpy(suggested,s->suggestedBindings,binding_count*sizeof suggested[0]);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(XrSession s,const XrSessionActionSetsAttachInfo *a){
    (void)s;assert(a->countActionSets==1&&a->actionSets[0]==action_set);return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(XrSession s,const XrActionSpaceCreateInfo *c,XrSpace *out){
    (void)s;assert(dual[id(c->action)]);assert(c->poseInActionSpace.orientation.w==1);
    int hand=hand_index(c->subactionPath);assert(hand<2);unsigned kind;
    if(c->action==aim_action)kind=0;
#ifdef TCVR_STEAM_FRAME
    else if(c->action==grip_pose_action)kind=1;
#endif
    else {assert(!"Unexpected pose action space");return XR_ERROR_RUNTIME_FAILURE;}
    unsigned handle=kind*2+(unsigned)hand+1;assert(handle<5);space_kind[handle]=kind;space_hand[handle]=(unsigned)hand;
    *out=(XrSpace)(uintptr_t)handle;space_count++;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession s,const XrActionsSyncInfo *c){
    (void)s;assert(c->countActiveActionSets==1);return sync_result;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession s,const XrActionStateGetInfo *get,XrActionStateBoolean *out){
    (void)s;unsigned a=id(get->action);int h=hand_index(get->subactionPath);
    assert(types[a]==XR_ACTION_TYPE_BOOLEAN_INPUT);assert(dual[a]?h<2:h==2);
    out->currentState=buttons[a][h];out->isActive=h==2||connected[h];return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession s,const XrActionStateGetInfo *get,XrActionStateFloat *out){
    (void)s;assert((get->action==trigger_action||get->action==grip_action)&&dual[id(get->action)]);
    int h=hand_index(get->subactionPath);assert(h<2);bool fire=get->action==trigger_action;
    out->isActive=connected[h]&&(!fire||trigger_active[h]);out->currentState=fire?triggers[h]:grips[h];
    out->lastChangeTime=fire?trigger_times[h]:0;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(XrSession s,const XrActionStateGetInfo *get,XrActionStatePose *out){
    (void)s;int kind;
    if(get->action==aim_action)kind=0;
#ifdef TCVR_STEAM_FRAME
    else if(get->action==grip_pose_action)kind=1;
#endif
    else {assert(!"Unexpected pose action query");return XR_ERROR_RUNTIME_FAILURE;}
    assert(dual[id(get->action)]);int h=hand_index(get->subactionPath);assert(h<2);
    out->isActive=connected[h]&&pose_active[kind][h];return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession s,const XrActionStateGetInfo *get,XrActionStateVector2f *out){
    (void)s;assert(get->action==pitch_action&&get->subactionPath==XR_NULL_PATH);
    assert(types[id(get->action)]==XR_ACTION_TYPE_VECTOR2F_INPUT);
    out->isActive=stick_active&&connected[RIGHT_HAND];out->currentState=(XrVector2f){0,stick_y};return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace space,XrSpace base,XrTime t,XrSpaceLocation *out){
    (void)base;unsigned handle=(unsigned)(uintptr_t)space;assert(handle>0&&handle<5);
    unsigned kind=space_kind[handle];int h=(int)space_hand[handle];pose_locate_time[kind][h]=t;
    if(custom_pose_values){out->locationFlags=pose_flags[kind][h];out->pose=pose_values[kind][h];}
    else{
        out->locationFlags=kind==0&& !tracked[h]?0:XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        out->pose=(XrPosef){{controller_rotation.x,controller_rotation.y,controller_rotation.z,controller_rotation.w},{h==LEFT_HAND?-.25f:.25f,1.4f,-.5f}};
    }
    return pose_locate_result[kind][h];
}
XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(XrSession s,const XrHapticActionInfo *info,const XrHapticBaseHeader *v){
    (void)s;(void)v;assert(info->action==haptic_action&&dual[id(info->action)]);haptic_calls++;
    assert(hand_index(info->subactionPath)<2);last_haptic=info->subactionPath;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(XrSession s,XrPath hand,XrInteractionProfileState *out){
    (void)s;assert(hand==hand_paths[RIGHT_HAND]);out->interactionProfile=current_profile;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(XrSession s,const XrHapticActionInfo *info){
    (void)s;assert(info->action==haptic_action);last_stop=info->subactionPath;return XR_SUCCESS;
}
void eng_audio_set_volume(int v){(void)v;}
void ss22_snd_inputs(uint16_t bits,unsigned w,unsigned p1,unsigned p2){(void)w;(void)p1;(void)p2;arcade_bits=bits;}
V3 qgun_muzzle(V3 p,Q4 q){return add(p,rotate(q,v3(0,0,-.1f)));}
bool qvr_aim(V3 p,V3 d,float *x,float *y,V3 *hit){*x=p.x+.5f;*y=.5f;*hit=add(p,mul(d,2.f));return true;}
static void tick_at(XrTime time){sync_input(time,true,1.7f);ss22_input_update();}
static void tick(void){tick_at(100);}
static void click(XrAction a,int h){buttons[id(a)][h]=true;tick();buttons[id(a)][h]=false;tick();}
static void bound(XrAction a,const char *name){
    for(unsigned i=0;i<binding_count;i++)if(suggested[i].action==a&&!strcmp(paths[suggested[i].binding],name))return;
    fprintf(stderr,"Missing binding: %s\n",name);abort();
}
#ifdef TCVR_STEAM_FRAME
Uint64 SDL_GetTicks64(void){return 1;}
static void frame_bound(unsigned profile,XrAction a,const char *name){
    assert(profile<3);
    for(unsigned i=0;i<profile_counts[profile];i++)if(profile_bindings[profile][i].action==a&&!strcmp(paths[profile_bindings[profile][i].binding],name))return;
    fprintf(stderr,"Missing Frame profile binding: %s\n",name);abort();
}
static char *read_report(FILE *out){
    assert(fflush(out)==0&&fseek(out,0,SEEK_END)==0);long size=ftell(out);assert(size>=0&&fseek(out,0,SEEK_SET)==0);
    char *text=calloc((size_t)size+1,1);assert(text);assert(fread(text,1,(size_t)size,out)==(size_t)size);return text;
}
static void frame_pose_report_regressions(const char *expected_profile){
    const float radians=3.14159265358979323846f/180.f;
    pose_values[0][LEFT_HAND]=(XrPosef){{0,sinf(30.f*radians/2),0,cosf(30.f*radians/2)},{.4f,1.9f,-.7f}};
    pose_values[1][LEFT_HAND]=(XrPosef){{0,sinf(-20.f*radians/2),0,cosf(-20.f*radians/2)},{-.2f,1.3f,-.4f}};
    pose_values[0][RIGHT_HAND]=(XrPosef){{0,0,sinf(15.f*radians/2),cosf(15.f*radians/2)},{.7f,1.7f,-.9f}};
    pose_values[1][RIGHT_HAND]=(XrPosef){{0,0,sinf(-35.f*radians/2),cosf(-35.f*radians/2)},{-.3f,1.2f,-.5f}};
    for(int kind=0;kind<2;kind++)for(int hand=0;hand<2;hand++)pose_flags[kind][hand]=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    pose_active[0][LEFT_HAND]=pose_active[1][LEFT_HAND]=pose_active[0][RIGHT_HAND]=pose_active[1][RIGHT_HAND]=true;
    custom_pose_values=true;active_profile_path=!strcmp(expected_profile,"steam_frame")?frame_profile_path:touch_profile_path;
    active_weapon_hand=LEFT_HAND;options.gun_pitch=7;frame_diag.synthetic_hands=false;
    frame_diag.pose_report=true;frame_pose_report=(FramePoseReport){0};
    const XrTime sample_time=1234567890LL;
    sync_input(sample_time,true,1.7f);
    assert(frame_pose_report.samples_ready);
    assert(pose_locate_time[0][LEFT_HAND]==sample_time&&pose_locate_time[1][LEFT_HAND]==sample_time&&
           pose_locate_time[0][RIGHT_HAND]==sample_time&&pose_locate_time[1][RIGHT_HAND]==sample_time);
    assert(fabsf(gun_position.x-.4f)<1e-6f&&fabsf(gun_position.y-.25f)<1e-6f&&fabsf(gun_position.z+.7f)<1e-6f);
    assert(gun_tracked&&aim_valid); /* Production gun remains driven by aim pose, never grip pose. */
    FILE *out=tmpfile();assert(out);frame_diag_report_poses(out,sample_time);char *text=read_report(out);fclose(out);
    assert(strstr(text,"[vr-pose]")&&strstr(text,expected_profile));
    assert(strstr(text,"hand=left weapon_hand=left pitch=7deg render_source=live anchor=aim anchor_source=live"));
    assert(strstr(text,"aim{flags=0x3,pos=(0.400,1.900,-0.700)"));
    assert(strstr(text,"grip{flags=0x3,pos=(-0.200,1.300,-0.400)"));
    assert(strstr(text,"grip_to_aim_m=(0.600,0.600,-0.300)"));
    assert(strstr(text,"grip_to_aim=50.00deg"));
    assert(strstr(text,"gun=pos=(0.400,0.250,-0.700)"));
    assert(strstr(text,"muzzle=")&&strstr(text,"direction=")&&strstr(text,"target_hit="));
    assert(strstr(text,"hand=right weapon_hand=left")&&strstr(text,"grip{flags=0x3,pos=(-0.300,1.200,-0.500)"));
    assert(strstr(text,"grip_to_aim_m=(1.000,0.500,-0.400)"));
    assert(strstr(text,"grip_to_aim=50.00deg"));
    free(text);
    out=tmpfile();assert(out);frame_diag_report_poses(out,sample_time+500000000LL);text=read_report(out);fclose(out);
    assert(!*text);free(text); /* Per-hand rate limit is one report per second. */
    pose_flags[1][RIGHT_HAND]=XR_SPACE_LOCATION_POSITION_VALID_BIT;
    frame_diag.synthetic_hands=true;XrTime next_sample=sample_time+1000000000LL;
    sync_input(next_sample,true,1.7f);
    assert(pose_locate_time[0][RIGHT_HAND]==next_sample&&pose_locate_time[1][RIGHT_HAND]==next_sample);
    out=tmpfile();assert(out);frame_diag_report_poses(out,next_sample);text=read_report(out);fclose(out);
    assert(strstr(text,"grip_to_aim=unavailable(grip:orientation-invalid)")&&
           strstr(text,"render_source=synthetic_hands anchor=synthetic_hands anchor_source=synthetic_hands")&&
           strstr(text,"aim{flags=0x3,pos=(0.400,1.900,-0.700)"));free(text);
    frame_diag.synthetic_hands=false;pose_flags[1][RIGHT_HAND]=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    pose_locate_result[1][RIGHT_HAND]=XR_ERROR_RUNTIME_FAILURE;XrTime failed_time=sample_time+2000000000LL;
    sync_input(failed_time,true,1.7f);
    out=tmpfile();assert(out);frame_diag_report_poses(out,failed_time);text=read_report(out);fclose(out);
    assert(strstr(text,"grip{unavailable(locate-error=-2)}")&&
           strstr(text,"grip_to_aim_m=unavailable(grip:locate-error)")&&
           strstr(text,"grip_to_aim=unavailable(grip:locate-error)"));free(text);
    pose_locate_result[1][RIGHT_HAND]=XR_SUCCESS;focused=false;
    out=tmpfile();assert(out);frame_diag_report_poses(out,sample_time+3000000000LL);text=read_report(out);fclose(out);
    assert(!*text);free(text);focused=true;
    frame_diag.pose_report=false;
    out=tmpfile();assert(out);frame_diag_report_poses(out,sample_time+2000000000LL);text=read_report(out);fclose(out);
    assert(!*text);free(text);
    pose_active[0][LEFT_HAND]=pose_active[1][LEFT_HAND]=pose_active[0][RIGHT_HAND]=pose_active[1][RIGHT_HAND]=true;
    pose_flags[1][RIGHT_HAND]=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    pose_locate_result[0][LEFT_HAND]=pose_locate_result[1][LEFT_HAND]=pose_locate_result[0][RIGHT_HAND]=pose_locate_result[1][RIGHT_HAND]=XR_SUCCESS;
    custom_pose_values=false;frame_diag.pose_report=false;frame_pose_report.samples_ready=false;
    frame_pose_report.next_log[LEFT_HAND]=frame_pose_report.next_log[RIGHT_HAND]=0;
}
static bool near3(V3 a,V3 b){return fabsf(a.x-b.x)<1e-5f&&fabsf(a.y-b.y)<1e-5f&&fabsf(a.z-b.z)<1e-5f;}
static void frame_grip_anchor_regressions(void){
    int old_hand=active_weapon_hand,old_pitch=options.gun_pitch;
    origin=v3(0,1.65f,0);origin_rotation=(Q4){0,0,0,1};origin_set=true;
    custom_pose_values=true;frame_diag.synthetic_hands=false;frame_diag.pose_report=false;frame_diag.grip_anchor=false;
    frame_diag.hand_yaw_degrees=0;options.gun_pitch=7;
    for(int hand=0;hand<HAND_COUNT;hand++){
        frame_diag.synthetic_hands=false;frame_diag.pose_report=false;frame_diag.grip_anchor=false;
        frame_pose_report=(FramePoseReport){0};
        for(int kind=0;kind<2;kind++)for(int h=0;h<HAND_COUNT;h++)pose_locate_time[kind][h]=-1;
        pose_flags[0][hand]=pose_flags[1][hand]=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        pose_active[0][hand]=pose_active[1][hand]=true;
        pose_locate_result[0][hand]=pose_locate_result[1][hand]=XR_SUCCESS;
        active_weapon_hand=hand;
        XrTime base=50000000000LL+(XrTime)hand*10000000000LL;
        sync_input(base,true,1.7f);
        V3 aim_position=relative_position(pose_values[0][hand].position);
        assert(near3(gun_position,aim_position)&&frame_gun_anchor==FRAME_GUN_ANCHOR_AIM);
        assert(pose_locate_time[1][hand]==-1); /* Default-off path does not poll grip for rendering. */
        V3 aim_muzzle=gun_origin,aim_target=gun_hit,aim_direction=rotate(gun_rotation,v3(0,0,-1));Q4 aim_rotation=gun_rotation;

        frame_diag.grip_anchor=true;
        if(hand==RIGHT_HAND)pose_flags[1][hand]=XR_SPACE_LOCATION_POSITION_VALID_BIT; /* Anchor only needs position validity. */
        XrTime grip_time=base+100000000LL;sync_input(grip_time,true,1.7f);
        V3 grip_position=relative_position(pose_values[1][hand].position),delta=sub(grip_position,aim_position);
        assert(pose_locate_time[1][hand]==grip_time&&frame_pose_report.grip[hand].action_active&&frame_pose_report.grip[hand].locate_attempted);
        assert(frame_gun_anchor==FRAME_GUN_ANCHOR_GRIP&&near3(gun_position,grip_position));
        assert(fabsf(gun_rotation.x-aim_rotation.x)<1e-6f&&fabsf(gun_rotation.y-aim_rotation.y)<1e-6f&&
               fabsf(gun_rotation.z-aim_rotation.z)<1e-6f&&fabsf(gun_rotation.w-aim_rotation.w)<1e-6f);
        assert(near3(rotate(gun_rotation,v3(0,0,-1)),aim_direction));
        assert(near3(sub(gun_origin,aim_muzzle),delta)&&near3(sub(gun_hit,aim_target),delta));

        frame_diag.pose_report=true;XrTime report_time=grip_time+100000000LL;sync_input(report_time,true,1.7f);
        FILE *out=tmpfile();assert(out);frame_diag_report_poses(out,report_time);char *text=read_report(out);fclose(out);
        assert(frame_gun_anchor==FRAME_GUN_ANCHOR_GRIP&&strstr(text,"anchor=grip anchor_source=live"));free(text);

        pose_locate_result[1][hand]=XR_ERROR_RUNTIME_FAILURE;
        XrTime fallback_time=report_time+1000000000LL;sync_input(fallback_time,true,1.7f);
        assert(frame_gun_anchor==FRAME_GUN_ANCHOR_AIM_FALLBACK&&near3(gun_position,aim_position));
        assert(near3(rotate(gun_rotation,v3(0,0,-1)),aim_direction));
        out=tmpfile();assert(out);frame_diag_report_poses(out,fallback_time);text=read_report(out);fclose(out);
        assert(strstr(text,"anchor=aim_fallback anchor_source=live")&&strstr(text,"grip{unavailable(locate-error=-2)}"));free(text);
        pose_locate_result[1][hand]=XR_SUCCESS;

        frame_diag.synthetic_hands=true;XrTime synthetic_time=fallback_time+1000000000LL;
        sync_input(synthetic_time,true,1.7f);frame_diag_hands();
        assert(frame_gun_anchor==FRAME_GUN_ANCHOR_SYNTHETIC_HANDS);
        assert(fabsf(gun_position.x-(hand==LEFT_HAND?-.28f:.28f))<1e-6f);
        out=tmpfile();assert(out);frame_diag_report_poses(out,synthetic_time);text=read_report(out);fclose(out);
        assert(strstr(text,"render_source=synthetic_hands anchor=synthetic_hands anchor_source=synthetic_hands"));free(text);
        frame_diag.synthetic_hands=false;
    }
    custom_pose_values=false;frame_diag.synthetic_hands=false;frame_diag.pose_report=false;frame_diag.grip_anchor=false;
    frame_gun_anchor=FRAME_GUN_ANCHOR_AIM;frame_pose_report=(FramePoseReport){0};
    active_weapon_hand=old_hand;options.gun_pitch=old_pitch;
    pose_flags[0][LEFT_HAND]=pose_flags[1][LEFT_HAND]=pose_flags[0][RIGHT_HAND]=pose_flags[1][RIGHT_HAND]=
        XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    pose_active[0][LEFT_HAND]=pose_active[1][LEFT_HAND]=pose_active[0][RIGHT_HAND]=pose_active[1][RIGHT_HAND]=true;
    pose_locate_result[0][LEFT_HAND]=pose_locate_result[1][LEFT_HAND]=pose_locate_result[0][RIGHT_HAND]=pose_locate_result[1][RIGHT_HAND]=XR_SUCCESS;
    puts("PASS: Frame grip anchor A/B uses valid grip position for both hands, preserves aim rotation and moves muzzle/ray together, falls back on locate error and skips synthetic hands");
}
static void frame_diagnostic_regressions(void){
    coin_frames=0;frame_diag.auto_fire_ticks=frame_diag.auto_fire_delay_ticks=0;
    frame_diag.autostart_started=false;frame_diag.input_ticks=0;
    assert(!frame_diag.grip_anchor); /* Omitted one-shot keys retain the production-safe default. */
    FILE *config=fopen("vrdiag.once","w");assert(config);
    fputs("force_render=1\nautostart=1\nautostart_tick=42\n",config);fclose(config);
    frame_diag_load();assert(frame_diag.autostart_tick==42&&!coin_frames&&!frame_diag.auto_fire_delay_ticks);
    config=fopen("vrdiag.once","w");assert(config);
    fputs("force_render=1\nautostart=1\ndump_tick=600\npose_report=1\ngrip_anchor=1\n",config);fclose(config);
    frame_diag_load();assert(frame_diag.autostart_tick==600&&frame_diag.pose_report&&frame_diag.grip_anchor&&!coin_frames&&!frame_diag.auto_fire_delay_ticks);
    remove("vrdiag.last");
    frame_diag.grip_anchor=false;
    uint32_t width,height;
    frame_eye_dimensions(1728,1440,2500,2500,2160,&width,&height);assert(width==2160&&height==1800);
    frame_eye_dimensions(1728,1440,2000,1700,2160,&width,&height);assert(width==2000&&height==1666);
    frame_eye_dimensions(3000,2000,4096,4096,3600,&width,&height);assert(width==2160&&height==1440);
    frame_eye_dimensions(500,500,400,400,128,&width,&height);assert(width==400&&height==400);

    const float angles[]={0.f,90.f,180.f};
    const float expected_x[]={-.064f,0.f,.064f},expected_z[]={0.f,.064f,0.f};
    XrViewState state={XR_TYPE_VIEW_STATE};
    for(unsigned i=0;i<3;i++){
        frame_diag.yaw_degrees=angles[i];frame_diag_views(&state);frame_diag_origin();
        Q4 relative=relative_rotation(views[0].pose.orientation);
        float half=angles[i]*(3.14159265358979323846f/360.f);
        assert(fabsf(relative.y-sinf(half))<1e-5f&&fabsf(relative.w-cosf(half))<1e-5f);
        V3 left=relative_position(views[0].pose.position),right=relative_position(views[1].pose.position);
        V3 baseline=sub(left,right),center=mul(add(left,right),.5f);
        assert(fabsf(baseline.x-expected_x[i])<1e-5f&&fabsf(baseline.z-expected_z[i])<1e-5f);
        assert(fabsf(center.x)<1e-6f&&fabsf(center.y)<1e-6f&&fabsf(center.z)<1e-6f);
    }

    focused=false;paused=false;frame_diag.force_render=true;frame_diag.autostart=true;frame_diag.autostart_started=false;
    frame_diag.synthetic_hands=false;frame_diag.autostart_tick=600;frame_diag.auto_fire_ticks=0;frame_diag.auto_fire_delay_ticks=0;coin_frames=0;
    frame_diag.dump_tick=600;frame_diag.input_ticks=0;assert(!frame_diag_dump_ready());
    frame_diag_input();assert(pedal==1.f);
    for(unsigned tick=1;tick<600;tick++){
        ss22_input_update();
        assert((arcade_bits&0x11)==0); /* Safe boot: no credit or fire before the configured tick. */
        assert(arcade_bits&0x20); /* The synthetic pedal remains held while boot settles. */
        assert(!frame_diag.autostart_started&&!frame_diag_dump_ready());
    }
    assert(frame_diag.input_ticks==599&&!coin_frames&&!frame_diag.auto_fire_delay_ticks);
    for(unsigned tick=600;tick<=647;tick++){
        ss22_input_update();
        bool coin=(tick>=601&&tick<=606)||(tick>=613&&tick<=618)||(tick>=625&&tick<=630);
        bool fire=tick>=635&&tick<=646;
        assert(((arcade_bits&1)!=0)==coin);
        assert(((arcade_bits&0x10)!=0)==fire);
        assert(arcade_bits&0x20);
        if(tick==600)assert(frame_diag.autostart_started&&frame_diag_dump_ready());
    }
    assert(frame_diag.input_ticks==647&&frame_diag.auto_fire_ticks==0&&!frame_diag.auto_fire_delay_ticks&&!coin_frames);
    frame_diag.force_render=false;trigger=pedal=1.f;frame_diag.auto_fire_ticks=0;coin_frames=36;
    ss22_input_update();assert(arcade_bits==0); /* Standby stays neutral without the one-shot flag. */
    focused=true;trigger=pedal=0;coin_frames=0;frame_diag.autostart=false;frame_diag.autostart_started=false;
    frame_diag.auto_fire_delay_ticks=0;frame_diag.dump_tick=0;frame_diag.input_ticks=0;
    puts("PASS: Frame synthetic geometry and delayed autostart with no early input, three credits before the 12-tick fire pulse");
}
#endif
#ifndef TCVR_STEAM_FRAME
int main(void){
    /* A successful request is not proof of application. Retry while focused,
     * stop on measured success, and cap retries if the runtime refuses to switch. */
    get_rate=mock_get_rate;request_rate=mock_request_rate;preferred_rate=120;focused=true;
    display_rate_check(1);assert(!rate_requests);display_rate_check(2000000001LL);assert(rate_requests==1);
    reported_rate=120;display_rate_check(4000000001LL);assert(rate_attempts==4);
    display_rate_check(6000000001LL);assert(rate_requests==1);
    reported_rate=72;rate_attempts=0;rate_check_time=0;rate_requests=0;
    for(int i=0;i<8;i++)display_rate_check(1+(int64_t)i*2000000000LL);
    assert(rate_requests==3&&rate_attempts==4);
    focused=false;rate_attempts=0;rate_check_time=0;display_rate_check(30000000000LL);assert(!rate_check_time);
    get_rate=NULL;request_rate=NULL;
    options_path="handedness-test.cfg";qoptions_load(options_path,&options);
    assert(!options.left_handed);instance=(XrInstance)(uintptr_t)1;session=(XrSession)(uintptr_t)1;
    running=focused=origin_set=true;assert(actions_init());assert(space_count==2&&binding_count==16);
    for(int h=0;h<2;h++){
        char name[100];const char *prefix=h?"/user/hand/right":"/user/hand/left";
        snprintf(name,sizeof name,"%s/input/aim/pose",prefix);bound(aim_action,name);
        snprintf(name,sizeof name,"%s/input/trigger/value",prefix);bound(trigger_action,name);
        snprintf(name,sizeof name,"%s/input/squeeze/value",prefix);bound(grip_action,name);
        snprintf(name,sizeof name,"%s/input/%s/click",prefix,h?"a":"x");bound(lower_action,name);
        snprintf(name,sizeof name,"%s/input/%s/click",prefix,h?"b":"y");bound(upper_action,name);
        snprintf(name,sizeof name,"%s/output/haptic",prefix);bound(haptic_action,name);
    }
    bound(pause_action,"/user/hand/left/input/menu/click");bound(hand_action,"/user/hand/right/input/thumbstick/click");
    tick();assert(gun_position.x==.25f&&gun_origin.x==.25f&&aim_valid);
    /* Either grip exposes; only releasing both hides. Trigger no longer opens cover. */
    grips[LEFT_HAND]=.9f;tick();assert((arcade_bits&0x30)==0x20);
    grips[RIGHT_HAND]=.8f;grips[LEFT_HAND]=0;tick();assert((arcade_bits&0x30)==0x20);
    grips[LEFT_HAND]=.9f;grips[RIGHT_HAND]=0;tick();assert((arcade_bits&0x30)==0x20);
    grips[RIGHT_HAND]=.8f;grips[LEFT_HAND]=0;tick();assert(pedal==.8f);
    grips[RIGHT_HAND]=0;triggers[RIGHT_HAND]=.9f;tick();assert((arcade_bits&0x30)==0x10);
    ss22_input_rumble(0,48000,45);assert(last_haptic==hand_paths[RIGHT_HAND]);
    /* Opposite trigger pressed while old one remains held: new arcade fire edge. */
    triggers[LEFT_HAND]=.9f;tick();assert(weapon_hand()==LEFT_HAND&&!(arcade_bits&0x10));
    assert(last_stop==hand_paths[RIGHT_HAND]&&gun_position.x==-.25f&&aim_valid);
    tick();assert(arcade_bits&0x10);ss22_input_rumble(0,48000,45);assert(last_haptic==hand_paths[LEFT_HAND]);
    for(int n=0;n<8;n++){tick();assert(weapon_hand()==LEFT_HAND);}
    triggers[LEFT_HAND]=0;tick();assert(!(arcade_bits&0x10)&&weapon_hand()==LEFT_HAND);
    assert(!options.left_handed); /* Handoffs do not change the saved button layout. */
    click(lower_action,RIGHT_HAND);assert(coin_frames>0);coin_frames=0;
    click(upper_action,RIGHT_HAND);assert(!options.laser_enabled&&!paused);
    QOptions saved;qoptions_load(options_path,&saved);assert(!saved.left_handed&&!saved.laser_enabled);
    triggers[RIGHT_HAND]=0;tick();triggers[RIGHT_HAND]=.9f;tick();assert(weapon_hand()==RIGHT_HAND&&(arcade_bits&0x10));
    /* A tap between simulation ticks must still produce one press. */
    triggers[0]=triggers[1]=0;tick();triggers[LEFT_HAND]=.9f;sync_input(100,true,1.7f);
    triggers[LEFT_HAND]=0;sync_input(101,true,1.7f);ss22_input_update();assert(arcade_bits&0x10);
    tick();assert(!(arcade_bits&0x10));
    /* Simultaneous samples use event time; exact ties keep the active hand. */
    triggers[0]=triggers[1]=.9f;trigger_times[0]=10;trigger_times[1]=20;tick();assert(weapon_hand()==RIGHT_HAND);
    triggers[0]=triggers[1]=0;tick();triggers[0]=triggers[1]=.9f;trigger_times[0]=trigger_times[1]=30;
    tick();assert(weapon_hand()==RIGHT_HAND);
    triggers[0]=triggers[1]=0;tick();triggers[0]=triggers[1]=.9f;trigger_times[0]=50;trigger_times[1]=40;
    tick();assert(weapon_hand()==LEFT_HAND);
    /* Keep cover operational without an aim pose; recovery requires trigger release. */
    grips[RIGHT_HAND]=.8f;tracked[LEFT_HAND]=false;tick();assert(!gun_tracked&&!aim_valid&&trigger==0&&pedal==.8f);
    tracked[LEFT_HAND]=true;tick();assert(trigger==0);triggers[LEFT_HAND]=0;tick();triggers[LEFT_HAND]=.9f;tick();assert(trigger==1);
    connected[RIGHT_HAND]=false;tick();assert(pedal==0&&gun_tracked);connected[RIGHT_HAND]=true;
    /* Action loss also cancels an unconsumed shot, even if pose remains valid. */
    triggers[LEFT_HAND]=0;tick();triggers[LEFT_HAND]=.9f;sync_input(100,true,1.7f);assert(shot_pending);
    trigger_active[LEFT_HAND]=false;tick();assert(!(arcade_bits&0x10));
    trigger_active[LEFT_HAND]=true;tick();assert(!(arcade_bits&0x10));
    /* Focus loss/pause/sync errors cannot replay a held trigger or button. */
    focused=false;tick();assert(arcade_bits==0);buttons[id(upper_action)][RIGHT_HAND]=true;
    focused=true;tick();assert(!options.laser_enabled&&trigger==0);
    buttons[id(upper_action)][RIGHT_HAND]=false;tick();click(upper_action,RIGHT_HAND);assert(options.laser_enabled);
    sync_result=XR_ERROR_RUNTIME_FAILURE;tick();assert(trigger==0&&pedal==0);
    buttons[id(upper_action)][RIGHT_HAND]=true;sync_result=XR_SUCCESS;tick();assert(options.laser_enabled&&trigger==0);
    buttons[id(upper_action)][RIGHT_HAND]=false;tick();
    click(hand_action,2);assert(!options.left_handed&&!paused); /* Menu-only preference. */
    click(pause_action,2);assert(paused&&arcade_bits==0);recoil_started=500;click(hand_action,2);
    assert(options.left_handed&&paused&&options_saved&&!recoil_started&&weapon_hand()==LEFT_HAND);
    qoptions_load(options_path,&saved);assert(saved.left_handed&&saved.laser_enabled);
    click(pause_action,2);assert(!paused&&trigger==0);
    triggers[0]=triggers[1]=0;tick();triggers[RIGHT_HAND]=.9f;tick();assert(weapon_hand()==RIGHT_HAND&&(arcade_bits&0x10));
    click(lower_action,LEFT_HAND);assert(coin_frames>0);coin_frames=0;recenter_requested=false;
    click(lower_action,RIGHT_HAND);assert(recenter_requested&&coin_frames==0);
    click(upper_action,LEFT_HAND);assert(!options.laser_enabled&&!paused);
    click(upper_action,RIGHT_HAND);assert(!options.physical_crouch&&!paused);
    click(pause_action,2);click(upper_action,RIGHT_HAND);assert(options.physical_crouch);
    click(pause_action,2);grips[0]=grips[1]=0;tick();assert(pedal==1);
    grips[0]=grips[1]=1;sync_input(100,true,1.45f);assert(pedal==0);tick();assert(pedal==1);
    sync_input(100,false,1.7f);assert(pedal==0);tick();
    /* Held face buttons must not acquire new roles on manual layout change. */
    click(pause_action,2);buttons[id(lower_action)][LEFT_HAND]=true;buttons[id(upper_action)][LEFT_HAND]=true;tick();
    bool laser=options.laser_enabled;bool physical=options.physical_crouch;recenter_requested=false;
    buttons[id(hand_action)][2]=true;tick();assert(!options.left_handed&&paused);
    for(int n=0;n<6;n++)tick();
    assert(!recenter_requested&&coin_frames==0&&options.laser_enabled==laser&&options.physical_crouch==physical);
    memset(buttons,0,sizeof buttons);triggers[0]=triggers[1]=0;tick();
    click(lower_action,RIGHT_HAND);assert(coin_frames==36);coin_frames=0;
    click(lower_action,LEFT_HAND);assert(recenter_requested);
    click(upper_action,RIGHT_HAND);assert(options.laser_enabled!=laser);
    click(upper_action,LEFT_HAND);assert(options.physical_crouch!=physical);
    /* Angle only changes in options, after a neutral stick, at a bounded rate. */
    bound(pitch_action,"/user/hand/right/input/thumbstick");bound(pitch_reset_action,"/user/hand/left/input/thumbstick/click");
    assert(paused&&options.gun_pitch==0);stick_y=-1;sync_input(1000,true,1.7f);assert(options.gun_pitch==-1);
    sync_input(399999999,true,1.7f);assert(options.gun_pitch==-1);
    sync_input(400001000,true,1.7f);assert(options.gun_pitch==-2);
    sync_input(500001000,true,1.7f);assert(options.gun_pitch==-3);
    stick_y=0;tick();stick_y=1;tick();assert(options.gun_pitch==-2);
    stick_active=false;tick();stick_active=true;tick();assert(options.gun_pitch==-2);
    stick_y=0;tick();stick_y=1;tick();assert(options.gun_pitch==-1);
    focused=false;tick();focused=true;tick();assert(options.gun_pitch==-1);
    stick_y=NAN;tick();assert(options.gun_pitch==-1);
    click(pitch_reset_action,2);assert(options.gun_pitch==0);
    stick_y=0;tick();set_gun_pitch(100);assert(options.gun_pitch==60);set_gun_pitch(-100);assert(options.gun_pitch==-60);
    set_gun_pitch(-45);qoptions_load(options_path,&saved);assert(saved.gun_pitch==-45);
    click(pause_action,2);assert(!paused);stick_y=1;tick();click(pitch_reset_action,2);assert(options.gun_pitch==-45);
    click(pause_action,2);assert(paused);tick();assert(options.gun_pitch==-45); /* Held across menu open. */
    /* A controller pitched 45 degrees up becomes level at -45 in BOTH hands.
     * Muzzle and target use precisely the same corrected pose as the model. */
    controller_rotation=(Q4){sinf(3.14159265f/8),0,0,cosf(3.14159265f/8)};
    for(int h=0;h<2;h++){
        select_weapon_hand(h);tick();V3 direction=rotate(gun_rotation,v3(0,0,-1));
        assert(fabsf(direction.y)<1e-6f&&fabsf(direction.z+1)<1e-6f);
        assert(fabsf(gun_origin.y-gun_position.y)<1e-6f&&fabsf(gun_origin.z-gun_position.z+.1f)<1e-6f);
        assert(fabsf(gun_hit.y-gun_origin.y)<1e-6f&&fabsf(gun_hit.z-gun_origin.z+2)<1e-6f);
    }
    /* Apply pitch in controller space after yaw, not around the world's X axis. */
    controller_rotation=product((Q4){0,sinf(.6f),0,cosf(.6f)},controller_rotation);tick();
    V3 forward=rotate(gun_rotation,v3(0,0,-1));assert(fabsf(forward.y)<1e-6f&&fabsf(forward.x+sinf(1.2f))<1e-6f);
    controller_rotation=(Q4){0,0,0,1};click(pitch_reset_action,2);tick();assert(options.gun_pitch==0&&gun_rotation.w==1);
    puts("PASS: real host controls, gun pitch persistence/bounds/repeat/reset, model/muzzle/aim alignment in both hands, local-space correction, focus/action loss and existing input regressions");
    return 0;
}
#else
int main(void){
    frame_diagnostic_regressions();
#ifdef TCVR_FRAME_PROFILE_UNSUPPORTED
    options_path="touch-fallback-test.cfg";
#else
    options_path="frame-controller-test.cfg";qoptions_load(options_path,&options);
#endif
#ifdef TCVR_FRAME_PROFILE_UNSUPPORTED
    qoptions_load(options_path,&options);
#endif
    instance=(XrInstance)(uintptr_t)1;session=(XrSession)(uintptr_t)1;
#ifndef TCVR_FRAME_PROFILE_UNSUPPORTED
    frame_profile_supported=true;
#endif
    assert(actions_init());assert(space_count==4&&action_count==13);
#ifdef TCVR_FRAME_PROFILE_UNSUPPORTED
    assert(!frame_profile_supported&&profile_counts[0]==0&&profile_counts[1]==18&&profile_counts[2]==0);
    assert(!strcmp(profile_names[1],"/interaction_profiles/oculus/touch_controller"));
    frame_bound(1,lower_action,"/user/hand/right/input/a/click");
    frame_bound(1,grip_pose_action,"/user/hand/left/input/grip/pose");
    frame_bound(1,grip_pose_action,"/user/hand/right/input/grip/pose");
    frame_bound(1,lower_action,"/user/hand/left/input/x/click");
    frame_bound(1,upper_action,"/user/hand/right/input/b/click");
    frame_bound(1,upper_action,"/user/hand/left/input/y/click");
    frame_bound(1,pause_action,"/user/hand/left/input/menu/click");
    for(unsigned i=0;i<profile_counts[1];i++)
        assert(profile_bindings[1][i].action!=view_action&&profile_bindings[1][i].action!=cover_mode_action);
    focused=running=origin_set=true;current_profile=path("/interaction_profiles/oculus/touch_controller");update_frame_profile();
    assert(!frame_profile_active);frame_pose_report_regressions("touch_fallback");frame_grip_anchor_regressions();
    options.left_handed=true;options.physical_crouch=false;paused=false;tick();
    click(lower_action,RIGHT_HAND);assert(coin_frames>0);coin_frames=0;
    click(lower_action,LEFT_HAND);assert(recenter_requested&&!recenter_calibration_requested);
    assert(options.left_handed); /* Frame fallback keeps right A/B fixed. */
    click(upper_action,RIGHT_HAND);assert(!options.laser_enabled);
    click(pause_action,2);assert(paused);
    click(upper_action,LEFT_HAND);assert(options.physical_crouch);
    puts("PASS: Touch fallback suggested and routed when Frame interaction profile is unavailable");
    return 0;
#else
    assert(profile_counts[0]==18&&profile_counts[1]==18&&profile_counts[2]==0);
    assert(!strcmp(profile_names[0],"/interaction_profiles/valve/frame_controller_valve"));
    assert(!strcmp(profile_names[1],"/interaction_profiles/oculus/touch_controller"));
    assert(!strcmp(action_names[id(aim_action)],"aim")&&!strcmp(action_names[id(trigger_action)],"trigger"));
    assert(!strcmp(action_names[id(grip_action)],"grip_cover")&&!strcmp(action_names[id(view_action)],"view_back_recenter"));
    assert(!strcmp(action_names[id(grip_pose_action)],"grip_pose")&&types[id(grip_pose_action)]==XR_ACTION_TYPE_POSE_INPUT&&dual[id(grip_pose_action)]);
    assert(!strcmp(action_names[id(cover_mode_action)],"cover_mode"));
    for(unsigned i=0;i<profile_counts[0];i++){
        XrPath p=profile_bindings[0][i].binding;assert(p!=XR_NULL_PATH&&profile_bindings[0][i].action!=XR_NULL_HANDLE);
        assert(!strstr(paths[p],"/input/squeeze/click")&&!strstr(paths[p],"/input/system/"));
    }
    for(int hand=0;hand<2;hand++){
        char p[100];const char *side=hand?"right":"left";
        snprintf(p,sizeof p,"/user/hand/%s/input/aim/pose",side);frame_bound(0,aim_action,p);
        snprintf(p,sizeof p,"/user/hand/%s/input/grip/pose",side);frame_bound(0,grip_pose_action,p);
        snprintf(p,sizeof p,"/user/hand/%s/input/trigger/value",side);frame_bound(0,trigger_action,p);
        snprintf(p,sizeof p,"/user/hand/%s/input/squeeze/value",side);frame_bound(0,grip_action,p);
        snprintf(p,sizeof p,"/user/hand/%s/output/haptic",side);frame_bound(0,haptic_action,p);
    }
    frame_bound(0,lower_action,"/user/hand/right/input/a/click");
    frame_bound(0,upper_action,"/user/hand/right/input/b/click");
    frame_bound(0,cover_mode_action,"/user/hand/right/input/x/click");
    frame_bound(0,pause_action,"/user/hand/right/input/menu/click");
    frame_bound(0,view_action,"/user/hand/left/input/view/click");
    frame_bound(0,hand_action,"/user/hand/right/input/thumbstick/click");
    frame_bound(0,pitch_action,"/user/hand/right/input/thumbstick");
    frame_bound(0,pitch_reset_action,"/user/hand/left/input/thumbstick/click");
    for(unsigned i=0;i<profile_counts[0];i++)assert(!strstr(paths[profile_bindings[0][i].binding],"system"));

    focused=running=origin_set=true;current_profile=frame_profile_path;update_frame_profile();assert(frame_profile_active);
    frame_pose_report_regressions("steam_frame");frame_grip_anchor_regressions();
    options.left_handed=true;options.laser_enabled=true;options.physical_crouch=false;options.gun_pitch=0;options_saved=qoptions_save(options_path,&options);
    active_weapon_hand=LEFT_HAND;cover_calibration_pending=true;tick();assert(!cover_calibration_pending);
    /* Frame's right-side A/B/X/Menu mapping ignores the saved default hand. */
    click(lower_action,RIGHT_HAND);assert(coin_frames>0&&coin_frames<=36);coin_frames=0;
    click(lower_action,LEFT_HAND);assert(coin_frames==0&&!recenter_requested);
    click(upper_action,RIGHT_HAND);assert(!options.laser_enabled);
    click(upper_action,LEFT_HAND);assert(!options.laser_enabled);
    bool physical=options.physical_crouch;click(cover_mode_action,2);assert(!options.physical_crouch&&!paused);
    click(pause_action,2);assert(paused);
    tick();click(cover_mode_action,2);assert(options.physical_crouch!=physical);
    click(hand_action,2);assert(!options.left_handed&&paused);
    /* Left View under 1 s returns from options; it does not recenter. */
    options.physical_crouch=false;paused=true;cover_calibration_pending=false;recenter_requested=false;recenter_calibration_requested=false;
    view_hold=(ViewHold){0};buttons[id(view_action)][LEFT_HAND]=false;tick_at(1);
    buttons[id(view_action)][LEFT_HAND]=true;tick_at(100);
    tick_at(500000000LL);assert(!recenter_requested&&!recenter_calibration_requested);
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(600000000LL);assert(!paused&&!recenter_requested);
    /* At exactly 1 s the hold recenters, but leaves cover height untouched. */
    paused=true;cover_calibration_pending=false;recenter_requested=false;recenter_calibration_requested=false;
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(2000000000LL);buttons[id(view_action)][LEFT_HAND]=true;tick_at(2000000100LL);
    unsigned before=haptic_calls;tick_at(3000000099LL);assert(!recenter_requested&&!recenter_calibration_requested&&!cover_calibration_pending);
    tick_at(3000000100LL);assert(recenter_requested&&!recenter_calibration_requested&&!cover_calibration_pending&&haptic_calls>=before+2);
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(3200000100LL);assert(paused);
    /* Three seconds adds upright height calibration and still suppresses short-back. */
    recenter_requested=recenter_calibration_requested=cover_calibration_pending=false;cover.upright_y=1.4f;cover.calibrated=true;
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(4000000000LL);buttons[id(view_action)][LEFT_HAND]=true;tick_at(4000000100LL);
    tick_at(7000000100LL);assert(recenter_requested&&recenter_calibration_requested&&!cover_calibration_pending);
    assert(cover.calibrated&&fabsf(cover.upright_y-1.7f)<1e-6f);
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(7300000100LL);assert(paused);
    /* Focus loss while View is held cannot replay a short-back event. */
    recenter_requested=false;view_hold=(ViewHold){0};buttons[id(view_action)][LEFT_HAND]=false;tick_at(8000000000LL);
    focused=false;buttons[id(view_action)][LEFT_HAND]=true;tick_at(8000000100LL);focused=true;tick_at(8000000200LL);
    buttons[id(view_action)][LEFT_HAND]=false;tick_at(8000000300LL);assert(paused&&!recenter_requested);
    /* Either squeeze holds the virtual pedal; release of both hides/reloads. */
    paused=false;options.physical_crouch=false;cover_calibration_pending=false;grips[LEFT_HAND]=.7f;tick();assert(fabsf(pedal-.7f)<1e-6f);
    grips[LEFT_HAND]=0;grips[RIGHT_HAND]=0;tick();assert(pedal==0);
    /* Either trigger selects that weapon hand and fires, independent of handedness. */
    active_weapon_hand=LEFT_HAND;triggers[LEFT_HAND]=0;triggers[RIGHT_HAND]=0;tick();triggers[RIGHT_HAND]=.9f;tick();
    assert(weapon_hand()==RIGHT_HAND&&trigger==1&&!options.left_handed);
    /* Right stick changes angle only in options; left stick click resets it. */
    click(pause_action,2);stick_y=0;tick();stick_y=1;tick();assert(options.gun_pitch==1);
    set_gun_pitch(42);click(pitch_reset_action,2);assert(options.gun_pitch==0);
    click(pause_action,2);stick_y=1;tick();assert(options.gun_pitch==0);
    puts("PASS: production Frame profile bindings, fixed-side routing, trigger/squeeze behavior, View 1s/3s thresholds, focus rearm, options-only sticks and no system/squeeze-click paths");
    return 0;
#endif
}
#endif
