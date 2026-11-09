/* game_reckon_all.c: dead-reckon every car to the frame's time (MAT3 rekon.c, game_reckon_all()).
 *
 * The model thread steps the cars at its own pace; the game thread draws a frame at another time. Each frame this
 * moves every car's drawn position and orientation (tModelData.reckon) on from its last model step by its velocity
 * and spin over the time between them. */
#include "game/game.h"

/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    u8 pad0[0x4C];
    f32 w[3];                   /* 0x04C the angular velocity, car frame, radians a second */
    u8 pad58[0x220 - 0x58];
    f32 rwv[3];                 /* 0x220 the velocity in the world, feet a second */
    f32 rwr[3];                 /* 0x22C the position in the world, feet */
    u8 pad238[0x2EC - 0x238];
    f32 uvs[3][3];              /* 0x2EC the orientation */
    u8 pad310[0x714 - 0x310];
    f32 modelTime;              /* 0x714 fastin.modelTime: the time of the last model step, seconds */
    u8 pad718[0x794 - 0x718];
    f32 reckon_rwr[3];          /* 0x794 reckon.rwr: the position to draw */
    f32 reckon_uvs[3][3];       /* 0x7A0 reckon.uvs: the orientation to draw */
    u8 pad7C4[0x7C8 - 0x7C4];
    s16 in_game;                /* 0x7C8 in the race */
    u8 pad7CA[0x7F4 - 0x7CA];
    f32 time_fudge;             /* 0x7F4 a scale on the time step (1 normally) */
    u8 pad7F8[0x808 - 0x7F8];
} tModelData;

/* For each car in the race that isn't dead (dead below CAR_DEAD): over deltime, the time from its last model step
 * to `time` (the frame's model time, seconds) scaled by its time_fudge, its drawn position becomes its position
 * plus velocity x deltime, and its drawn orientation its orientation turned by angular velocity x deltime
 * (RotateUV). */
void game_reckon_all(f32 time) {
    tModelData *m;
    s32 i;
    f32 deltime, temp[3];

    for (i = 0; i < PC_RACE_CARS; i++) {
        m = PC_MODEL(i);
        if (m->in_game && PC_CAR_DEAD(i) < CAR_DEAD) {
            deltime = (time - m->modelTime) * m->time_fudge;
            temp[0] = m->rwv[0] * deltime;
            temp[1] = m->rwv[1] * deltime;
            temp[2] = m->rwv[2] * deltime;
            m->reckon_rwr[0] = m->rwr[0] + temp[0];
            m->reckon_rwr[1] = m->rwr[1] + temp[1];
            m->reckon_rwr[2] = m->rwr[2] + temp[2];
            CopyUV(m->uvs, m->reckon_uvs);
            temp[0] = m->w[0] * deltime;
            temp[1] = m->w[1] * deltime;
            temp[2] = m->w[2] * deltime;
            RotateUV(temp, m->reckon_uvs);
        }
    }
}
