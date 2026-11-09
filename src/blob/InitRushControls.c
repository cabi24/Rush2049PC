/* InitRushControls (MAT3 controls.c): the game's controls at start-up. */
#include "game/game.h"

M2C_UNK memcpy(s8 *, s32 *, M2C_UNK);               /* extern */

/* Starts the controller system, makes the default control layout (gDefaultControls) and gives it to all four
 * controllers (gNewControls[4][10], which ends where gFrontDistance begins). */
void InitRushControls(void) {
    s8 *controls;

    InitControlSys();
    SetDefaultControls(&gDefaultControls);
    controls = (s8 *) gNewControls;
    do {
        memcpy(controls, &gDefaultControls, 10);
        controls += 10;
    } while (controls != (void *) &gFrontDistance);
}
