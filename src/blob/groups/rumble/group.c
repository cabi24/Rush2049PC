/* si_lock and the rumble thread: the serial bus lock and the Rumble Paks' motors (MAT3 controlsys.c and paksysdc.c;
 * the duty cycle is the N64's own). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { s32 w[6]; } OSMesgQueue;

typedef struct { void *next; OSMesgQueue *msgQ; } OSScClient;

/* A pak's record (tPak) as the rumble thread reads it, its level and phase volatile. */
typedef struct {
    char pad0[6];
    s8 rumble;                  /* a Rumble Pak is in */
    char pad7[5];
    PcPfs pfs[1];               /* the OSPfs the motor calls take */
    s32 error, FreeSpace;
    s8 motorOn;
    s8 request;
    volatile s8 level;
    s8 unk7F;
    volatile s16 phase;
    char pad82[0x282];
} Controller;

VIEW(OSMesgQueue, rumble_queue);
#define rumble_queue rumble_queue_view

VIEW(OSMesgQueue, si_lock_queue);
#define si_lock_queue si_lock_queue_view

VIEW(s32, gScheduler);
#define gScheduler gScheduler_view
VIEW(OSMesgQueue, si_event_queue);
#define si_event_queue si_event_queue_view

VIEW(Controller[4], gPaks);
#define gPaks gPaks_view

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count);
s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flags);
s32 osSendMesg(OSMesgQueue *mq, OSMesg msg, s32 flags);

#define RUMBLE_MAX 40        /* the most rumble a pak is asked for */
#define RUMBLE_FULL 10       /* from this level the motor runs all the time */
#define RUMBLE_PERIOD 10     /* retraces of the duty cycle (rumble_duty_masks) */

/* Makes the serial bus lock: a one-message queue holding its message. */
static void si_lock_init(void) {
    si_lock_inited = 1;
    osCreateMesgQueue(&si_lock_queue, &si_lock_msg, 1);
    osSendMesg(&si_lock_queue, 0, 0);
}

/* Takes the serial bus lock (the controllers and their paks), making it the first time; si_unlock gives it back. */
void si_lock(void) {
    OSMesg msg;

    if (!si_lock_inited) {
        si_lock_init();
    }
    osRecvMesg(&si_lock_queue, &msg, 1);
}


/* The rumble thread: woken each retrace by the scheduler, it runs each Rumble Pak's motor under the serial bus lock.
 * A pak's level follows its request (at most RUMBLE_MAX), falling by one a retrace; from RUMBLE_FULL up the motor
 * runs, below it the motor runs on the retraces of the RUMBLE_PERIOD-retrace cycle that rumble_duty_masks[level]
 * marks. With rumbling stopped (gRumble 0, the pause screen) every motor is stopped. A motor command that fails is
 * retried once after osMotorInit. */
void rumble_thread_entry(void *arg) {
    OSScClient client;
    OSMesg msg;
    s32 i;
    s32 retried;
    s32 level;
    Controller *p;

    osCreateMesgQueue(&rumble_queue, &rumble_msg, 1);
    osScAddClient(&gScheduler, &client, &rumble_queue);
    for (;;) {
        osRecvMesg(&rumble_queue, &msg, 1);
        si_lock();
        for (i = 0; i < 4; i++) {
            switch (gPaks[i].rumble) {
            case 0:
skip:
                continue;
            }
            retried = 0;
            if (++gPaks[i].phase >= RUMBLE_PERIOD) {
                gPaks[i].phase = 0;
            }
            level = gPaks[i].request;
            if (level > RUMBLE_MAX) {
                level = RUMBLE_MAX;
            }
            if (level < gPaks[i].level) {
                level = gPaks[i].level - 1;
            }
            gPaks[i].level = level;
            if (gRumble == 0) goto off;
            if (level > 0) goto check;
off:
            if (gPaks[i].motorOn) {
                if (!gRumble) {
                    osMotorInit(&si_event_queue, gPaks[i].pfs, i);
                }
                while (1) {
                    if (__osMotorAccess(gPaks[i].pfs, 0) == 0) {
                        gPaks[i].motorOn = 0;
                        break;
                    }
                    if (retried) break;
                    if (osMotorInit(&si_event_queue, gPaks[i].pfs, i) != 0) break;
                    retried = 1;
                }
            }
            goto skip;
check:
            if (level < RUMBLE_FULL) goto check2;
on:
            if (!gPaks[i].motorOn) {
                while (1) {
                    if (__osMotorAccess(gPaks[i].pfs, 1) == 0) {
                        gPaks[i].motorOn = 1;
                        break;
                    }
                    if (retried) break;
                    if (osMotorInit(&si_event_queue, gPaks[i].pfs, i) != 0) break;
                    retried = 1;
                }
            }
            goto skip;
check2:
            if (rumble_duty_masks[level] & (1 << gPaks[i].phase)) goto on;
            goto off;
        }
        si_unlock();
    }
}

