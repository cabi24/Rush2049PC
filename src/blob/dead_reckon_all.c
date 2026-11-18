/* dead_reckon_all.c: take each car's dead-reckoning snapshot (MAT3 rekon.c, dead_reckon_all()).
 *
 * The model thread calls it after a step: the game thread then draws the cars from these snapshots, extrapolated
 * to the frame's time (game_reckon_all), rather than from the simulation's own state. */
#include "game/game.h"

/* For each car in the race that isn't dead (dead below CAR_DEAD), copy its position, velocity, acceleration,
 * orientation, suspension and speed into its reckon snapshot (dead_reckon_posuv). */
void dead_reckon_all(void) {
    tModelData *m;
    s32 i;
    for (i = 0; i != PC_RACE_CARS; i++) {
        m = &pc_models[i];
        if ((m->in_game != 0) && ((s8) pc_cars[i].dead < CAR_DEAD)) {
            dead_reckon_posuv(m);
        }
    }
}
