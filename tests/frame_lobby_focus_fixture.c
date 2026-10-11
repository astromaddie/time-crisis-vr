/* Exercise production XR event handling against the real SS22 sound input
 * implementation: a no-op sound mock hides pre-game null dereferences. */
#define TCVR_REAL_SOUND_INPUTS
#define main unused_controller_test_main
#include "handedness_fixture.c"
#undef main
const ss22_game *g_ss22_game;
extern void (*ss22_input_rec_hook)(uint16_t,unsigned,unsigned,unsigned);
static XrSessionState pending_state;
static unsigned cabinet_writes;
static void record_input(uint16_t p,unsigned wheel,unsigned p1,unsigned p2){
    assert(p==0&&wheel==0x200&&p1==0&&p2==0);cabinet_writes++;
}
int SDL_PollEvent(SDL_Event *event){(void)event;return 0;}
XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(XrInstance i,XrEventDataBuffer *event){
    (void)i;if(!pending_state)return XR_EVENT_UNAVAILABLE;
    XrEventDataSessionStateChanged change={XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};change.state=pending_state;
    memcpy(event,&change,sizeof change);pending_state=XR_SESSION_STATE_UNKNOWN;return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(XrSession s,const XrSessionBeginInfo *info){(void)s;(void)info;return XR_SUCCESS;}
XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(XrSession s){(void)s;return XR_SUCCESS;}
static void focus_event(XrSessionState next){pending_state=next;assert(events());}
int main(void){
    in_lobby=true;host=NULL;g_ss22_game=NULL;
    /* Same event progression as an ordinary launch and putting on the Frame. */
    focus_event(XR_SESSION_STATE_READY);focus_event(XR_SESSION_STATE_SYNCHRONIZED);
    focus_event(XR_SESSION_STATE_VISIBLE);focus_event(XR_SESSION_STATE_FOCUSED);
    assert(focused&&running&&g_ss22_game==NULL);
    trigger=pedal=1;shot_pending=shot_needs_release=fire_output_down=true;
    focus_event(XR_SESSION_STATE_VISIBLE);
    assert(!focused&&!trigger&&!pedal&&!shot_pending&&!shot_needs_release&&!fire_output_down);
    focus_event(XR_SESSION_STATE_FOCUSED);assert(focused);
    /* Once the game is attached, focus changes must still neutralize its real
     * cabinet backend, including the existing recording hook. */
    static const ss22_game game={.snd={.adc_pedal={1,2}}};
    static const ss22_host_game game_host={0};
    g_ss22_game=&game;host=&game_host;in_lobby=false;
    ss22_input_rec_hook=record_input;
    focus_event(XR_SESSION_STATE_VISIBLE);assert(cabinet_writes==1);
    focus_event(XR_SESSION_STATE_FOCUSED);assert(cabinet_writes==2);
    ss22_input_rec_hook=NULL;
    puts("PASS: real XR focus events keep pre-game lobby input local and neutralize the real cabinet backend after game attach");
}
