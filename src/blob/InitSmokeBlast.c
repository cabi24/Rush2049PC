/* InitSmokeBlast.c: reset a car's wreck smoke (MAT3 smoke.c, InitSmokeBlast()).
 *
 * PlayGame calls it when a car restarts after a crash, so its next wreck smoke starts light grey again. */
#include "game/game.h"

/* a table, (&gCarSmokeDir)[i]: on the PC the tire-effect latches pc_tire_fx, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6), declared whole (OPTIMIZED.md 2.7), its first word here */
#define gCarSmokeDir (*(s32 *)pc_tire_fx)

/* Clear car `slot`'s SMOKE_WRECK_ON latch: its next wreck smoke starts again at grey 175 (StartSmoke). */
void InitSmokeBlast(s16 slot) {
    s32 *latches;

    latches = &(&gCarSmokeDir)[slot];
    *latches &= ~SMOKE_WRECK_ON;
}
