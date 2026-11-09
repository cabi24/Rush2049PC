/* GetControllerStickEdges (MAT3 controlsys.c): whether a controller's stick drives the d-pad (the menus). */
#include "game/game.h"

/* Whether controller which turns its stick into d-pad presses; for -1, whether all four do. */
s8 GetControllerStickEdges(s32 which) {
    if (which == -1) {
        if (gStickEdges[0] == 0) {
            return 0;
        }
        if (gStickEdges[1] == 0) {
            return 0;
        }
        if (gStickEdges[2] == 0) {
            return 0;
        }
        if (gStickEdges[3] == 0) {
            return 0;
        }
        return 1;
    }
    return gStickEdges[which];
}
