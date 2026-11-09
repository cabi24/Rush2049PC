/* UpdateFrame: the end of a frame on the game thread (MAT3 game.c's frame end, the N64's scheduler hand-off). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);

/* A message to the main thread (the scheduler): its type. */
typedef struct { s16 type; s16 pad; u8 data[28]; } SchedMsg;

/* Asks the main thread to end the frame (message 2750) and waits on the game thread's queue for the next frame's
 * start, emptying it of any older messages; then the new frame's display list, the frame timer, the controls and the
 * once-a-frame functions. */
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
