/* audio_retrace_thread_entry.c: the audio retrace thread, the MusyX sound library's clock (N64 only; no MAT3
 * counterpart).
 *
 * A scheduler client: each retrace it runs the sound library's retrace callback (which builds the next audio task)
 * and advances the audio clock gAudioSeconds; on the reset button's pre-NMI it silences the audio. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 w[6]; } OSMesgQueue;

/* A scheduler client (libultra's OSScClient): the scheduler sends its messages to msgQ. */
typedef struct { void *next; OSMesgQueue *msgQ; } OSScClient;

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);

VIEW(OSMesgQueue, audio_retrace_queue);
#define audio_retrace_queue audio_retrace_queue_view
VIEW(OSMesg[8], audio_retrace_msgs);
#define audio_retrace_msgs audio_retrace_msgs_view

/* The scheduler's message kinds (the first s16 of its message) are system.h's: OS_SC_RETRACE_MSG, a retrace, and
 * OS_SC_PRE_NMI_MSG, the reset button's pre-NMI. */

/* The thread's body (arg unused): join the scheduler, then forever wait for its messages. On a retrace, wait while
 * the last audio task (scheduler_audio_task) is still the RSP's current task, then call musyx_retrace_callback;
 * and count the retrace's time (gRetraceTime) into gAudioSeconds, kept under four hours (14,400 s). On the
 * pre-NMI, audio_prenmi. */
void audio_retrace_thread_entry(void *arg) {
    OSScClient client;
    s16 *msg = 0;

    osCreateMesgQueue(&audio_retrace_queue, audio_retrace_msgs, 8);
    osScAddClient(&gScheduler, &client, &audio_retrace_queue);
    gAudioSeconds = 0.0f;
    while (1) {
        osRecvMesg(&audio_retrace_queue, (OSMesg *) &msg, 1);
        switch (*msg) {
        case OS_SC_RETRACE_MSG:
            if (musyx_retrace_callback != 0) {
                while (scheduler_audio_task != 0 && scheduler_audio_task == gScheduler.curRSPTask) { pc_os_spin_yield("audio_retrace_thread_entry");
                }
                musyx_retrace_callback(musyx_retrace_callback);
            }
            gAudioSeconds += gRetraceTime;
            if (gAudioSeconds > 14400.0f) {
                gAudioSeconds -= 14400.0f;
            }
            break;
        case OS_SC_PRE_NMI_MSG:
            audio_prenmi();
            break;
        }
    }
}
