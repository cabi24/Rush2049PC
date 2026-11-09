/* ClearControls (MAT3 controlsys.c): forgets every controller's input for this frame (controlsys/group.c has the
 * controller system). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;

extern int osRecvMesg(OSMesgQueue *, void **, int);
extern int osSendMesg(OSMesgQueue *, void *, int);

VIEW(f32[4][2], gContStick);
#define gContStick gContStick_view

/* Clears the frame's held, pressed and auto-repeated buttons and sticks, each controller's and the active ones'
 * union, and the presses ControlIRQ has gathered since the last frame; under the controls' lock. */
void ClearControls(void)
{
    s32 i;

    osRecvMesg(&controlsys_lock_queue, 0, 1);
    gActiveEdges = gActiveAutoEdges = gActiveLevels = 0;
    for (i = 0; i < 4; i++) {
        gContStick[i][0] = gContStick[i][1] = 0.0f;
        gNewEdges[i] = 0;
        gAutoEdges[i] = 0;
        gContLevels[i] = 0;
        gContEdges[i] = 0;
    }
    osSendMesg(&controlsys_lock_queue, 0, 0);
}
