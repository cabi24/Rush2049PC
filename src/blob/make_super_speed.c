/* make_super_speed.c: the super speed cheat's trick (MAT3 road.c, make_super_speed()).
 *
 * With gSuperSpeed set, init_road and the physics step call it on the car's orientation. */
#include "game/game.h"

/* The model record (tModelData), its orientation. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x2E4];
    f32 uvs[3][3];              /* +0x2EC the orientation: rows the car's x, y and z axes */
} tModelData;

/* Rescale each row of model m's orientation to a length of super_speed_scale_x, _y and _z (MAT3 1.00025) instead of
 * 1. The orientation turns the car's velocity into the world's every step, so a matrix a little longer than a
 * rotation makes the car gain speed step after step: that is the cheat. */
void make_super_speed(tModelData *m) {
    f32 scale;
    f32 scale_2;
    f32 scale_3;

    scale = invmagnitude(m->uvs[0]) * super_speed_scale_x;
    m->uvs[0][0] = m->uvs[0][0] * scale;
    m->uvs[0][1] = m->uvs[0][1] * scale;
    m->uvs[0][2] = m->uvs[0][2] * scale;
    scale_2 = invmagnitude(m->uvs[1]) * super_speed_scale_y;
    m->uvs[1][0] = m->uvs[1][0] * scale_2;
    m->uvs[1][1] = m->uvs[1][1] * scale_2;
    m->uvs[1][2] = m->uvs[1][2] * scale_2;
    scale_3 = invmagnitude(m->uvs[2]) * super_speed_scale_z;
    m->uvs[2][0] = m->uvs[2][0] * scale_3;
    m->uvs[2][1] = m->uvs[2][1] * scale_3;
    m->uvs[2][2] = m->uvs[2][2] * scale_3;
}
