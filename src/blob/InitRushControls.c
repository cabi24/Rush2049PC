/* InitRushControls (MAT3 controls.c): the game's controls at start-up. */
#include "game/game.h"

M2C_UNK memcpy(s8 *, s8 *, M2C_UNK);               /* extern */

/* Starts the controller system, makes the default control layout (gDefaultControls) and gives it to each of the four
 * controllers (gNewControls). */
void InitRushControls(void) {
    s32 pad;

    InitControlSys();
    SetDefaultControls(gDefaultControls);
    for (pad = 0; pad < 4; pad++) {
        memcpy(gNewControls[pad], gDefaultControls, sizeof gNewControls[pad]);
    }
}
