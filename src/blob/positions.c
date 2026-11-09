#include "game/game.h"

typedef f32 F32;

#define XCOMP 0
#define YCOMP 1
#define ZCOMP 2

typedef struct {
    char pad0[0x70];
    F32 TIRER[4][3];        /* 0x70 */
} CARTYPE;

typedef struct {
    F32 fpuvs[3][3];
} UVECT;

enum n64p_CARTYPE { n64p_CARTYPE_ };
typedef struct MODELDAT {
    enum n64p_CARTYPE car;           /* 0x000 */
    char pad4[0x24];
    F32 A[3];               /* 0x028 */
    char pad34[0xC];
    F32 V[3];               /* 0x040 */
    F32 W[3];               /* 0x04C */
    char pad58[0x9C];
    F32 BODYR[4][3];        /* 0x0F4 */
    char pad124[0xF0];
    F32 RWA[3];             /* 0x214 */
    F32 RWV[3];             /* 0x220 */
    F32 RWR[3];             /* 0x22C */
    F32 lastRWR[3];         /* 0x238 */
    F32 TIRERWR[4][3];      /* 0x244 */
    F32 BODYRWR[4][3];      /* 0x274 */
    char pad2A4[0x3C];
    F32 angle[3];           /* 0x2E0 */
    UVECT UV;               /* 0x2EC */
    char pad310[0x2CC];
    F32 tpcomp[4];          /* 0x5DC */
    char pad5EC[0x48];
    F32 dt;                 /* 0x634 */
    char pad638[0xD8];
    s32 frame;              /* 0x710 */
} MODELDAT;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_CARTYPE: (CARTYPE *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

void positions(MODELDAT *m) {
    F32 temp[3], temp1[3], temp2[3], *bp, *rwp;
    F32 movef, mover;
    F32 yvect[3], ftroll[3], rtroll[3];
    F32 rollang;
    s32 i;

    m->lastRWR[0] = m->RWR[0], m->lastRWR[1] = m->RWR[1], m->lastRWR[2] = m->RWR[2];

    WorldVector(m->A, m->RWA, m->UV.fpuvs[0]);
    WorldVector(m->V, m->RWV, m->UV.fpuvs[0]);

    temp[0] = m->RWV[0] * m->dt;
    temp[1] = m->RWV[1] * m->dt;
    temp[2] = m->RWV[2] * m->dt;

    m->RWR[0] = temp[0] + m->RWR[0];
    m->RWR[1] = temp[1] + m->RWR[1];
    m->RWR[2] = temp[2] + m->RWR[2];

    temp[XCOMP] = m->W[XCOMP] * m->dt;
    temp[YCOMP] = m->W[YCOMP] * m->dt;
    temp[ZCOMP] = m->W[ZCOMP] * m->dt;

    if (gRace == 4) {
        m->angle[0] = temp[0] + m->angle[0];
        m->angle[1] = temp[1] + m->angle[1];
        m->angle[2] = temp[2] + m->angle[2];
    }

    RotateUV(temp, &m->UV);

    if ((m->frame & 0x3FF) == 0x3FF) {
        if (gSuperSpeed != 0) {
            make_super_speed(m);
        } else {
            movef = invmagnitude(m->UV.fpuvs[0]);
            m->UV.fpuvs[0][0] *= movef;
            m->UV.fpuvs[0][1] *= movef;
            m->UV.fpuvs[0][2] *= movef;
            movef = invmagnitude(m->UV.fpuvs[1]);
            m->UV.fpuvs[1][0] *= movef;
            m->UV.fpuvs[1][1] *= movef;
            m->UV.fpuvs[1][2] *= movef;
            movef = invmagnitude(m->UV.fpuvs[2]);
            m->UV.fpuvs[2][0] *= movef;
            m->UV.fpuvs[2][1] *= movef;
            m->UV.fpuvs[2][2] *= movef;
        }
    }

    BodyVector(m->RWV, m->V, m->UV.fpuvs[0]);

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
        WorldVector(temp, temp1, m->UV.fpuvs[0]);
        m->RWR[0] = temp1[0] + m->RWR[0];
        m->RWR[1] = temp1[1] + m->RWR[1];
        m->RWR[2] = temp1[2] + m->RWR[2];
    }

    for (i = 0; i < 4; i++) {
        rwp = m->TIRERWR[i];
        bp = N64P(m->car)->TIRER[i];
        temp[0] = bp[0];
        temp[1] = bp[1] + m->tpcomp[i];
        temp[2] = bp[2];
        WorldVector(temp, rwp, m->UV.fpuvs[0]);
        rwp[0] = rwp[0] + m->RWR[0];
        rwp[1] = rwp[1] + m->RWR[1];
        rwp[2] = rwp[2] + m->RWR[2];
    }

    for (i = 0; i < 4; i++) {
        bp = m->BODYR[i];
        rwp = m->BODYRWR[i];
        WorldVector(bp, rwp, m->UV.fpuvs[0]);
        rwp[0] = rwp[0] + m->RWR[0];
        rwp[1] = rwp[1] + m->RWR[1];
        rwp[2] = rwp[2] + m->RWR[2];
    }
}
