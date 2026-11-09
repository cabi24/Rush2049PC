#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flag);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag);

VIEW(volatile u8, frame_end_requested);
#define frame_end_requested frame_end_requested_view
VIEW(OSMesgQueue, game_thread_queue);
#define game_thread_queue game_thread_queue_view
VIEW(OSMesgQueue, main_thread_queue);
#define main_thread_queue main_thread_queue_view
VIEW(intptr_t, frame_dl);
#define frame_dl frame_dl_view
VIEW(intptr_t, frame_dl_start);
#define frame_dl_start frame_dl_start_view

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
