#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

VIEW(s8[4][2], gContStickRange);
#define gContStickRange gContStickRange_view
VIEW(OSMesgQueue, controlsys_lock_queue);
#define controlsys_lock_queue controlsys_lock_queue_view

extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void InitControlSys(void)
{
    static s8 queueCreated = 0;    /* 0x80111968 */
    s32 i;

    if (gInitControls == 0) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    }
    gInitControls = 1;
    for (i = 0; i < 4; i++) {
        gContStickRange[i][1] = 70; gContStickRange[i][0] = 70;
    }
    if (queueCreated == 0) {
        queueCreated = 1;
        osCreateMesgQueue(&controlsys_lock_queue, &controlsys_lock_msg, 1);
        osSendMesg(&controlsys_lock_queue, 0, 0);
    }
    while (*(volatile u8 *)&cont_bit_pattern == 0x80) { pc_os_spin_yield("InitControlSys");
    }
    UpdateControls();
}
