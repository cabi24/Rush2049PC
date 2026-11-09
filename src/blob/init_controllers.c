/* init_controllers.c: start the controllers (N64 only: libultra's serial interface; no MAT3 counterpart).
 *
 * Makes the serial interface's event queue, initialises the controllers through libultra (osContInit, under the
 * serial-bus lock the Controller Pak and Rumble Pak code share) and clears each controller's Rumble Pak state. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

/* A controller's record (16 bytes); its first byte is its Rumble Pak state. */
typedef struct Rumble16 {u8 flag;u8 pad[15];} Rumble16;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);
extern void osSetEventMesg(s32,OSMesgQueue *,OSMesg);  /* SDK osSetEventMesg */

/* libultra's event of a serial interface transfer done (OS_EVENT_SI). */
#define OS_EVENT_SI 5

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

VIEW(Rumble16[4], gControllerData);
#define gControllerData gControllerData_view

/* Make the serial-bus lock the first time: a queue of one message, the token, posted at once (free). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue,&si_lock_msg,1);
        osSendMesg(&si_lock_queue,0,0);
    }
}

/* Take the serial-bus lock (wait for its token); si_unlock gives it back. */
static void pak_lock(void)
{
    OSMesg message;

    pak_queue_init();
    osRecvMesg(&si_lock_queue,&message,1);
}

/* Start the controllers: the serial interface's event queue (8 messages, the message for a finished transfer
 * si_message), osContInit under the lock (the connected controllers into cont_bit_pattern, their status into
 * cont_status), and the four controllers' Rumble Pak state cleared. */
void init_controllers(void)
{
    static s16 si_message;
    s32 i;

    osCreateMesgQueue(&si_event_queue, si_event_msgs, 8);
    si_message = 5;
    osSetEventMesg(OS_EVENT_SI, &si_event_queue, &si_message);
    pak_lock();
    osContInit(&si_event_queue, &cont_bit_pattern, cont_status);
    si_unlock();
    for (i = 0; i < 4; i++) {
        gControllerData[i].flag = 0;
    }
}
