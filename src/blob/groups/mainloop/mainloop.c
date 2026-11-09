#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef struct { u8 pad[636]; s32 frameCount; } Sched;

VIEW(OSMesgQueue, main_thread_queue);
#define main_thread_queue main_thread_queue_view
VIEW(OSMesgQueue, game_thread_queue);
#define game_thread_queue game_thread_queue_view
VIEW(u8[], frame_start_msg);
#define frame_start_msg frame_start_msg_view
VIEW(volatile s8, frame_start_pending);
#define frame_start_pending frame_start_pending_view
VIEW(volatile s8, game_frame_building);
#define game_frame_building game_frame_building_view
VIEW(volatile Sched, gScheduler);
#define gScheduler gScheduler_view
VIEW(volatile s32, gLastFrameCount);
#define gLastFrameCount gLastFrameCount_view

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
void *osViGetCurrentFramebuffer(void);
void *osViGetNextFramebuffer(void);
void osViBlack(u8);

void MainPreNMI(void)
{
    sound_freeze_output();
    video_restore_base_mode();
    osViBlack(1);
    modelrun = 0;
    PS_RumbleStop();
    gPakSysInited = 0;
    for (;;) {
    }
}

void MainEndFrame(void)
{
    frame_end_requested = 0;
    EndGraphics();
    frame_record_index ^= 1;
    gfx_tasks_pending++;
}


void process_scheduler_message(void)
{
    static s32 task_done_parity = 0;
    s16 type;
    OSMesg msg = 0;
    intptr_t cur;

    osRecvMesg(&main_thread_queue, &msg, 1);
    type = *(s16 *)msg;
    switch (type) {
    case 4:
        MainPreNMI();
        break;
    case 1:
        cur = (intptr_t)osViGetCurrentFramebuffer();
        if ((intptr_t)osViGetNextFramebuffer() == cur && !game_frame_building && !frame_start_pending && !frame_end_requested &&
            gLastFrameCount != gScheduler.frameCount && gfx_tasks_pending < 2) {
            retraces_since_frame = 0;
            StartGraphics();
            frame_start_pending = 1;
            osSendMesg(&game_thread_queue, frame_start_msg, 1);
        }
        cur = retraces_since_frame;
        retraces_since_frame = cur + 1;
        break;
    case 2750:
        MainEndFrame();
        break;
    case 2:
        task_done_parity ^= 1;
        gfx_tasks_pending--;
        break;
    }
}
