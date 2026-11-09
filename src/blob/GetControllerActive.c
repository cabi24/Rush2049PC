/* GetControllerActive (MAT3 controlsys.c): whether a controller counts in the active controllers' union. */
#include "game/game.h"

/* Whether controller which is active; for -1, whether all four are. */
s8 GetControllerActive(s32 which) {
    if (which == -1) {
        if (gActiveControllers[0] == 0) {
            return 0;
        }
        if (gActiveControllers[1] == 0) {
            return 0;
        }
        if (gActiveControllers[2] == 0) {
            return 0;
        }
        if (gActiveControllers[3] == 0) {
            return 0;
        }
        return 1;
    }
    return gActiveControllers[which];
}
