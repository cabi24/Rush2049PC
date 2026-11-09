#include "game/game.h"

/* a table, (&gCarSmokeDir)[i]: on the PC the tire-effect latches pc_tire_fx, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6), declared whole (OPTIMIZED.md 2.7), its first word here */
#define gCarSmokeDir (*(s32 *)pc_tire_fx)

void InitWingSmoke(s16 arg0, s16 arg1) {
    s32 *temp_v0;
    s32 *temp_v0_2;

    if (arg1 != 0) {
        temp_v0 = &(&gCarSmokeDir)[arg0];
        *temp_v0 &= 0xFFFF7FFF;
        return;
    }
    temp_v0_2 = &(&gCarSmokeDir)[arg0];
    *temp_v0_2 &= ~0x4000;
}
