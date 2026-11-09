#include "game/game.h"

VIEW(s8, gNewControls);
#define gNewControls gNewControls_view

M2C_UNK memcpy(s8 *, s32 *, M2C_UNK);               /* extern */

void InitRushControls(void) {
    s8 *var_s0;

    InitControlSys();
    SetDefaultControls(&gDefaultControls);
    do { var_s0 = (s8 *) &gNewControls; do { memcpy(var_s0, &gDefaultControls, 0xA); var_s0 += 0xA; } while (var_s0 != (void *) &gFrontDistance); } while (0);
}

