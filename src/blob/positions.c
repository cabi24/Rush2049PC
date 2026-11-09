/* positions.c: a car's position and orientation from its velocities (MAT3 drivsym.c, positions()).
 *
 * The last stage of the physics step (regular, sym): integrates the position and the orientation over the step and
 * moves the tires' and body corners' world positions with the car. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar), its tire positions. */
typedef struct {
    char pad0[0x70];
    f32 tireR[4][3];        /* 0x70 each tire's position from the centre of mass, car frame */
} tCar;

/* An orientation matrix: rows the car's x (side), y (up) and z (forward) axes in world coordinates. */
typedef struct {
    f32 fpuvs[3][3];
} UVECT;

enum n64p_tCar { n64p_tCar_ };
/* The model record (tModelData), the fields positions reads and writes. */
typedef struct tModelData {
    enum n64p_tCar car;     /* 0x000 the physics descriptor (a window address) */
    char pad4[0x24];
    f32 a[3];               /* 0x028 the acceleration, car frame */
    char pad34[0xC];
    f32 v[3];               /* 0x040 the velocity, car frame */
    f32 w[3];               /* 0x04C the angular velocity, car frame */
    char pad58[0x9C];
    f32 bodyR[4][3];        /* 0x0F4 the body's corners, car frame */
    char pad124[0xF0];
    f32 rwa[3];             /* 0x214 the acceleration, world frame */
    f32 rwv[3];             /* 0x220 the velocity, world frame */
    f32 rwr[3];             /* 0x22C the position, world frame */
    f32 rwr2[3];            /* 0x238 the position a step ago */
    f32 tireRwr[4][3];      /* 0x244 each tire's world position */
    f32 bodyrwr[4][3];      /* 0x274 each body corner's world position */
    char pad2A4[0x3C];
    f32 stuntW[3];          /* 0x2E0 the rotation summed for the stunt score */
    UVECT uvs;              /* 0x2EC the orientation */
    char pad310[0x2CC];
    f32 tpcomp[4];          /* 0x5DC each tire's suspension compression (0 or more) */
    char pad5EC[0x48];
    f32 dt;                 /* 0x634 the step, seconds */
    char pad638[0xD8];
    s32 modelIteration;     /* 0x710 fastin.modelIteration: the steps run */
} tModelData;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tCar: (tCar *)PC_N64P(x), default: (x))

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

    WorldVector(m->a, m->rwa, m->uvs.fpuvs[0]);
    WorldVector(m->v, m->rwv, m->uvs.fpuvs[0]);

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

    if ((m->modelIteration & 0x3FF) == 0x3FF) {
        if (gSuperSpeed != 0) {
            make_super_speed(m);
        } else {
            movef = invmagnitude(m->uvs.fpuvs[0]);
            m->uvs.fpuvs[0][0] *= movef;
            m->uvs.fpuvs[0][1] *= movef;
            m->uvs.fpuvs[0][2] *= movef;
            movef = invmagnitude(m->uvs.fpuvs[1]);
            m->uvs.fpuvs[1][0] *= movef;
            m->uvs.fpuvs[1][1] *= movef;
            m->uvs.fpuvs[1][2] *= movef;
            movef = invmagnitude(m->uvs.fpuvs[2]);
            m->uvs.fpuvs[2][0] *= movef;
            m->uvs.fpuvs[2][1] *= movef;
            m->uvs.fpuvs[2][2] *= movef;
        }
    }

    BodyVector(m->rwv, m->v, m->uvs.fpuvs[0]);

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
        WorldVector(temp, temp1, m->uvs.fpuvs[0]);
        m->rwr[0] = temp1[0] + m->rwr[0];
        m->rwr[1] = temp1[1] + m->rwr[1];
        m->rwr[2] = temp1[2] + m->rwr[2];
    }

    for (i = 0; i < 4; i++) {
        rwp = m->tireRwr[i];
        bp = N64P(m->car)->tireR[i];
        temp[0] = bp[0];
        temp[1] = bp[1] + m->tpcomp[i];
        temp[2] = bp[2];
        WorldVector(temp, rwp, m->uvs.fpuvs[0]);
        rwp[0] = rwp[0] + m->rwr[0];
        rwp[1] = rwp[1] + m->rwr[1];
        rwp[2] = rwp[2] + m->rwr[2];
    }

    for (i = 0; i < 4; i++) {
        bp = m->bodyR[i];
        rwp = m->bodyrwr[i];
        WorldVector(bp, rwp, m->uvs.fpuvs[0]);
        rwp[0] = rwp[0] + m->rwr[0];
        rwp[1] = rwp[1] + m->rwr[1];
        rwp[2] = rwp[2] + m->rwr[2];
    }
}
