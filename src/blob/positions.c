/* positions.c: a car's position and orientation from its velocities (MAT3 drivsym.c, positions()).
 *
 * The last stage of the physics step (regular, sym): integrates the position and the orientation over the step and
 * moves the tires' and body corners' world positions with the car. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), its tire positions. */
typedef struct {
    char pad0[0x60];
    TireDesc *tires[4];     /* 0x60 each tire's constants */
    f32 tireR[4][3];        /* 0x70 each tire's position from the centre of mass, car frame */
} tCar;

/* An orientation matrix: rows the car's x (side), y (up) and z (forward) axes in world coordinates. */
typedef struct {
    f32 fpuvs[3][3];
} UVECT;

/* One step of model m's motion:
 * - the velocity and acceleration to the world frame, and the position moved by rwv dt (the old one kept in rwr2);
 * - the orientation turned by w dt (RotateUV), the same angles summed into stuntW in a stunt race; every 1024 steps
 *   its rows renormalized (or, with the super speed cheat, stretched: make_super_speed) against rounding drift;
 *   the velocity back to the car frame in the new orientation;
 * - a suspension compressed past 0.5 ft caps at 0.5 and lifts the car by the largest excess (along its up axis);
 * - the tires' world positions (their arm, lowered by the compression) and the body corners'. */
void positions(tModelData *m) {
    f32 temp[3], temp1[3], temp2[3], *bp, *rwp;
    f32 movef, mover;
    f32 yvect[3], ftroll[3], rtroll[3];   /* temp2, mover, these and rollang are unused (they shape the frame) */
    f32 rollang;
    s32 i;

    m->rwr2[0] = m->rwr[0], m->rwr2[1] = m->rwr[1], m->rwr2[2] = m->rwr[2];

    WorldVector(m->a, m->rwa, m->uvs[0]);
    WorldVector(m->v, m->rwv, m->uvs[0]);

    temp[0] = m->rwv[0] * m->dt;
    temp[1] = m->rwv[1] * m->dt;
    temp[2] = m->rwv[2] * m->dt;

    m->rwr[0] = temp[0] + m->rwr[0];
    m->rwr[1] = temp[1] + m->rwr[1];
    m->rwr[2] = temp[2] + m->rwr[2];

    temp[0] = m->w[0] * m->dt;
    temp[1] = m->w[1] * m->dt;
    temp[2] = m->w[2] * m->dt;

    if (gRace == kStunt) {
        m->stuntW[0] = temp[0] + m->stuntW[0];
        m->stuntW[1] = temp[1] + m->stuntW[1];
        m->stuntW[2] = temp[2] + m->stuntW[2];
    }

    RotateUV(temp, &m->uvs);

    if ((m->fastin.modelIteration & 0x3FF) == 0x3FF) {
        if (gSuperSpeed != 0) {
            make_super_speed(m);
        } else {
            movef = invmagnitude(m->uvs[0]);
            m->uvs[0][0] *= movef;
            m->uvs[0][1] *= movef;
            m->uvs[0][2] *= movef;
            movef = invmagnitude(m->uvs[1]);
            m->uvs[1][0] *= movef;
            m->uvs[1][1] *= movef;
            m->uvs[1][2] *= movef;
            movef = invmagnitude(m->uvs[2]);
            m->uvs[2][0] *= movef;
            m->uvs[2][1] *= movef;
            m->uvs[2][2] *= movef;
        }
    }

    BodyVector(m->rwv, m->v, m->uvs[0]);

    temp[1] = 0.0f;
    for (i = 0; i < 4; i++) {
        movef = m->tpcomp[i] - 0.5f;
        if (temp[1] < movef) {
            temp[1] = movef;
            m->tpcomp[i] = 0.5f;
        }
    }
    if (temp[1] > 0.0f) {
        temp[0] = 0.0f;
        temp[2] = 0.0f;
        WorldVector(temp, temp1, m->uvs[0]);
        m->rwr[0] = temp1[0] + m->rwr[0];
        m->rwr[1] = temp1[1] + m->rwr[1];
        m->rwr[2] = temp1[2] + m->rwr[2];
    }

    for (i = 0; i < 4; i++) {
        rwp = m->tireRwr[i];
        bp = m->car->tireR[i];
        temp[0] = bp[0];
        temp[1] = bp[1] + m->tpcomp[i];
        temp[2] = bp[2];
        WorldVector(temp, rwp, m->uvs[0]);
        rwp[0] = rwp[0] + m->rwr[0];
        rwp[1] = rwp[1] + m->rwr[1];
        rwp[2] = rwp[2] + m->rwr[2];
    }

    for (i = 0; i < 4; i++) {
        bp = m->bodyR[i];
        rwp = m->bodyrwr[i];
        WorldVector(bp, rwp, m->uvs[0]);
        rwp[0] = rwp[0] + m->rwr[0];
        rwp[1] = rwp[1] + m->rwr[1];
        rwp[2] = rwp[2] + m->rwr[2];
    }
}
