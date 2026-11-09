/* PS_RumbleStop: stop every Rumble Pak (MAT3 paksysdc.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;
extern void osCreateMesgQueue(OSMesgQueue *,OSMesg *,s32);
extern s32 osSendMesg(OSMesgQueue *,OSMesg,s32),osRecvMesg(OSMesgQueue *,OSMesg *,s32);

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

/* Makes the serial bus lock (a one-message queue) the first time it is needed (a copy of pfs_queue_init). */
static void pak_queue_init(void)
{
    if (!si_lock_inited) {
        si_lock_inited = 1; osCreateMesgQueue(&si_lock_queue,&si_lock_msg,1);
        osSendMesg(&si_lock_queue,0,0);
    }
}

/* Takes the serial bus lock (a copy of si_lock). */
static void pak_lock(void)
{
    OSMesg message;

    pak_queue_init();
    osRecvMesg(&si_lock_queue,&message,1);
}

/* Stops pak i's motor if it is a Rumble Pak: no rumble asked, the motor told to stop, and noted off if it did.
 * Returns -1 for a pak that isn't one, and nothing in particular otherwise. */
static s32 motor_init(tPak *p, s32 i)
{
    if (!p->rumble) return -1;
    p->request = 0;
    osMotorInit(&si_event_queue, &p->pfs, i);
    if (__osMotorAccess(&p->pfs, 0) == 0) {
        p->motorOn = 0;
    }
}

/* Stops all rumbling and keeps the rumble thread from starting it again until PS_RumbleGo. */
void PS_RumbleStop(void)
{
    s32 i;
    tPak *p;

    gRumble = 0;
    pak_lock();
    for (i = 0; i < NUM_PAKS; i++) {
        p = &gPaks[i];
        motor_init(p, i);
    }
    osSendMesg(&si_lock_queue,0,0);
}
