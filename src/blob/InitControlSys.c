/* InitControlSys (MAT3 controlsys.c): starts the controller system (controlsys/group.c has it). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define OSThread OSThreadView   /* this file's own OSThread: include/game's OSThread is another type */

typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

VIEW(OSMesgQueue, controlsys_lock_queue);
#define controlsys_lock_queue controlsys_lock_queue_view

extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

/* The first time, makes every controller active and stick-driven; each time, resets the sticks' calibration
 * (70, where MAT3's GameCube pads have 110), makes the controls' lock once, waits until the model thread's
 * init_controllers has the controllers' first status (cont_bit_pattern is 0x80 until then), and reads the input
 * once. */
void InitControlSys(void)
{
    static s8 queueCreated = 0;
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
