/* InitWingSmoke.c: break a car's wing vapour trail (MAT3 smoke.c, InitWingSmoke()).
 *
 * Called when a wing is set up (StartWing) and when it stops trailing (AnimateWing), so the next trail starts
 * afresh instead of joining up with where the last one ended. */
#include "game/game.h"

/* a table, (&gCarSmokeDir)[i]: on the PC the tire-effect latches pc_tire_fx, PC_MAX_LINKS long, with no N64 address
 * (pc_game_types.h, docs/port/DECOUPLE.md 6), declared whole (OPTIMIZED.md 2.7), its first word here */
#define gCarSmokeDir (*(s32 *)pc_tire_fx)

/* Forget the last point of car `slot`'s wing trail on `side` (0: SMOKE_WING0, else SMOKE_WING1): clear its
 * SMOKE_TRAIL_WING latch, so StartSmoke's next trail puff starts a new trail. */
void InitWingSmoke(s16 slot, s16 side) {
    s32 *latches;
    s32 *latches_2;

    if (side != 0) {
        latches = &(&gCarSmokeDir)[slot];
        *latches &= ~SMOKE_TRAIL_WING1;
        return;
    }
    latches_2 = &(&gCarSmokeDir)[slot];
    *latches_2 &= ~SMOKE_TRAIL_WING0;
}
