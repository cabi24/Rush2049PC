/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * game_mode_handler: N64 mode-switch handshake with the scheduler/graphics thread. Sets frame_end_requested, jams a
 * 2750 message (a 32-byte message record on the stack, type s16 at +0) at the front of queue main_thread_queue,
 * clears game_frame_building, then drains queue game_thread_queue (one non-blocking receive, one blocking receive, then
 * non-blocking receives until empty), sets game_frame_building / clears frame_start_pending, copies frame_dl to frame_dl_start
 * and calls UpdateFrameTimer, UpdateRushControls and HAL_CallOncePerFrameFuncs. N64-only; no arcade ancestor.
 *
 * Shaping: the three flag bytes are volatile (lui; addiu; sb 0). The first receive's result is compared with
 * -1 in an empty `if` (compiled-out check): that starts the -1 constant web right after the first call, which
 * is where retail materialises `li s2,-1`. The message record is declared before `msg` (frame slots 48 / 44).
 * Also matches at -O2.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef void *OSMesg;
typedef struct OSMesgQueue OSMesgQueue;

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);
void UpdateFrameTimer(void);
void UpdateRushControls(void);
s32 HAL_CallOncePerFrameFuncs(void);

extern volatile u8 frame_start_pending;
extern volatile u8 game_frame_building;
extern volatile u8 frame_end_requested;
extern OSMesgQueue game_thread_queue;
extern OSMesgQueue main_thread_queue;
extern intptr_t frame_dl;
extern intptr_t frame_dl_start;

typedef struct { s16 type; s16 pad; u8 data[28]; } SchedMsg;

void UpdateFrame(void)
{
    SchedMsg sm;
    OSMesg msg;

    frame_end_requested = 1;
    sm.type = 2750;
    osSendMesg(&main_thread_queue, &sm, 1);
    game_frame_building = 0;
    if (osRecvMesg(&game_thread_queue, &msg, 0) == -1) {}
    osRecvMesg(&game_thread_queue, &msg, 1);
    while (osRecvMesg(&game_thread_queue, &msg, 0) != -1) {
    }
    game_frame_building = 1;
    frame_start_pending = 0;
    frame_dl_start = frame_dl;
    UpdateFrameTimer();
    UpdateRushControls();
    HAL_CallOncePerFrameFuncs();
}
