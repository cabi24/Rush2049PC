#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 w[6]; } OSMesgQueue;

typedef struct { void *next; OSMesgQueue *msgQ; } OSScClient;

typedef struct {
    char pad[0x2B0];   /* gScheduler's curRSPTask (scheduler_current_rsp_task) natively: docs/port/ULTRA.md */
    void *volatile cur;
} Sched;
VIEW(Sched, gScheduler);
#define gScheduler gScheduler_view

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

VIEW(OSMesgQueue, audio_retrace_queue);
#define audio_retrace_queue audio_retrace_queue_view
VIEW(OSMesg[8], audio_retrace_msgs);
#define audio_retrace_msgs audio_retrace_msgs_view

void audio_retrace_thread_entry(void *arg) {
    OSScClient client;
    s16 *msg = 0;

    osCreateMesgQueue(&audio_retrace_queue, audio_retrace_msgs, 8);
    osScAddClient(&gScheduler, &client, &audio_retrace_queue);
    gAudioSeconds = 0.0f;
    while (1) {
        osRecvMesg(&audio_retrace_queue, (OSMesg *) &msg, 1);
        switch (*msg) {
        case 1:
            if (musyx_retrace_callback != 0) {
                while (scheduler_audio_task != 0 && scheduler_audio_task == gScheduler.cur) { pc_os_spin_yield("audio_retrace_thread_entry");
                }
                musyx_retrace_callback(musyx_retrace_callback);
            }
            gAudioSeconds += gRetraceTime;
            if (gAudioSeconds > 14400.0f) {
                gAudioSeconds -= 14400.0f;
            }
            break;
        case 4:
            audio_prenmi();
            break;
        }
    }
}

