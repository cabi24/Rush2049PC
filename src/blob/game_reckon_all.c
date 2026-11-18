/* game_reckon_all.c: dead-reckon every car to the frame's time (MAT3 rekon.c, game_reckon_all()).
 *
 * The model thread steps the cars at its own pace; the game thread draws a frame at another time. Each frame this
 * moves every car's drawn position and orientation (tModelData.reckon) on from its last model step by its velocity
 * and spin over the time between them. */
#include "game/game.h"

/* For each car in the race that isn't dead (dead below CAR_DEAD): over deltime, the time from its last model step
 * to `time` (the frame's model time, seconds) scaled by its time_fudge, its drawn position becomes its position
 * plus velocity x deltime, and its drawn orientation its orientation turned by angular velocity x deltime
 * (RotateUV). */
void game_reckon_all(f32 time) {
    tModelData *m;
    s32 i;
    f32 deltime, temp[3];

    for (i = 0; i < PC_RACE_CARS; i++) {
        m = &pc_models[i];
        if (m->in_game && pc_cars[i].dead < CAR_DEAD) {
            deltime = (time - m->fastin.modelTime) * m->time_fudge;
            temp[0] = m->rwv[0] * deltime;
            temp[1] = m->rwv[1] * deltime;
            temp[2] = m->rwv[2] * deltime;
            m->reckon.rwr[0] = m->rwr[0] + temp[0];
            m->reckon.rwr[1] = m->rwr[1] + temp[1];
            m->reckon.rwr[2] = m->rwr[2] + temp[2];
            CopyUV(m->uvs, m->reckon.uvs);
            temp[0] = m->w[0] * deltime;
            temp[1] = m->w[1] * deltime;
            temp[2] = m->w[2] * deltime;
            RotateUV(temp, m->reckon.uvs);
        }
    }
}
