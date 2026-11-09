/* ControllerUpdate (MAT3 controlsys.c; the N64's reads the pads over the serial bus): reads the four controllers
 * into gControllerData once a retrace (the model thread calls it) and hands them to ControlIRQ. controlsys/group.c
 * turns them into the game's held, pressed and stick state. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

typedef struct OSContPad {u16 button;s8 stick_x,stick_y;u8 errno;} OSContPad;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);

/* libultra's controller error bit (cont_status[].errno, OSContPad.errno): no controller answered. */
#define CONT_NO_RESPONSE_ERROR 0x8
/* The type of the serial bus's event message: init_controllers.c has si_event_queue get si_message when a serial
 * command finishes (osSetEventMesg(OS_EVENT_SI, ...), OS_EVENT_SI being 5 too). */
#define SI_EVENT_MSG 5
/* Retraces between status queries (which find a Controller Pak being plugged in or out). */
#define CONT_QUERY_INTERVAL 30

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

VIEW(u32[19], n64_button_map);
#define n64_button_map n64_button_map_view

/* Makes the serial bus's lock the first time (MAT3 controlsys.c's _InitSemaphore, the N64 form). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue,&si_lock_msg,1);
        osSendMesg(&si_lock_queue,0,0);
    }
}

/* Takes the serial bus's lock (shared with the Controller Pak and Rumble Pak code); si_unlock gives it back. */
static void pak_lock(void)
{
    OSMesg message;

    pak_queue_init();
    osRecvMesg(&si_lock_queue,&message,1);
}

/* Every CONT_QUERY_INTERVAL calls, queries the controllers' status (cont_status) and notes paks plugged in or out
 * (pak_poll_inserted); then reads the four pads. A controller that doesn't answer is detached and reads as nothing.
 * Each pad's buttons are turned into the game's button bits (bit j for n64_button_map[j]) and its stick copied; the
 * second stick and the analog triggers read 0. Then ControlIRQ updates the game's input state. Returns 0 when the
 * read could not be started (the bus is busy), else 1. */
s32 ControllerUpdate(void)
{
    static u8 queryTimer = CONT_QUERY_INTERVAL - 1;
    OSMesg msg = 0;
    s32 i;
    s32 j;
    OSContPad pads[4];

    pak_lock();
    osRecvMesg(&si_event_queue, &msg, 0);
    if (++queryTimer == CONT_QUERY_INTERVAL) {
        queryTimer = 0;
        osContStartQuery(&si_event_queue);
        do {
            osRecvMesg(&si_event_queue, &msg, 1);
        } while (*(s16 *)msg != SI_EVENT_MSG);
        osContGetQuery(cont_status);
        pak_poll_inserted();
    }
    if (osContStartReadData(&si_event_queue) != 0) {
        si_unlock();
        return 0;
    }
    osRecvMesg(&si_event_queue, &msg, 1);
    osContGetReadData(pads);
    si_unlock();
    for (i = 0; i < 4; i++) {
        if ((cont_status[i].errno & CONT_NO_RESPONSE_ERROR) || (pads[i].errno & CONT_NO_RESPONSE_ERROR)) {
            if (gControllerData[i].attached) {
                gControllerData[i].attached = 0;
            }
        } else {
            if (!gControllerData[i].attached) {
                gControllerData[i].attached = 1;
            }
        }
        if (!gControllerData[i].attached) {
            pads[i].stick_x = pads[i].stick_y = 0;
            pads[i].button = 0;
        }
        gControllerData[i].buttons = 0;
        for (j = 0; j < 19; j++) {
            if (pads[i].button & n64_button_map[j]) {
                gControllerData[i].buttons |= 1 << j;
            }
        }
        gControllerData[i].stick[0] = pads[i].stick_x;
        gControllerData[i].stick[1] = pads[i].stick_y;
        gControllerData[i].stick2[0] = 0;
        gControllerData[i].stick2[1] = 0;
        gControllerData[i].analog[0] = 0;
        gControllerData[i].analog[1] = 0;
    }
    ControlIRQ();
    return 1;
}
