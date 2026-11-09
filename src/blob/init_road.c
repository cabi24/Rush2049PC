/* init_road.c: a car placed at its start point (MAT3 road.c, init_road()).
 *
 * initialize calls it when a car starts or respawns: the position and orientation from the start (or respawn)
 * record, the tires and the body corners placed with it, the road state cleared. */
#include "game/game.h"

#define vecadd(a, b, r) { r[0] = a[0] + b[0]; r[1] = a[1] + b[1]; r[2] = a[2] + b[2]; }
#define veccopy(a, r) { r[0] = a[0]; r[1] = a[1]; r[2] = a[2]; }

/* The car's physics descriptor (MAT3 _tCar), its tire positions. */
typedef struct {
    /* 0x000 */ u8 pad0[0x60];
    /* 0x060 */ TireDesc *tires[4];   /* each tire's constants */
    /* 0x070 */ f32 tireR[4][3];   /* each tire's position from the centre of mass, car frame */
} tCar;

/* The model record (tModelData), the fields init_road reads and writes. */
typedef struct tModelData {
    /* 0x000 */ tCar *car;           /* the physics descriptor */
    /* 0x004 */ Tune44 *tune;
    /* 0x008 */ u8 pad8[0xEC];
    /* 0x0F4 */ f32 bodyR[4][3];        /* the body's corners, car frame */
    /* 0x124 */ u8 pad124[0x108];
    /* 0x22C */ f32 rwr[3];             /* the position */
    /* 0x238 */ u8 pad238[0xC];
    /* 0x244 */ f32 tireRwr[4][3];      /* each tire's world position */
    /* 0x274 */ f32 bodyrwr[4][3];      /* each body corner's world position */
    /* 0x2A4 */ f32 bodyrwr2[4][3];     /* the same a step ago */
    /* 0x2D4 */ u8 pad2D4[0x18];
    /* 0x2EC */ f32 uvs[3][3];          /* the orientation */
    /* 0x310 */ u8 pad310[0x90];
    /* 0x3A0 */ streetyp *lasttp[4];
    /* 0x3B0 */ u8 pad3B0[0x5C];
    /* 0x40C */ const s16 *torquecurve;
    /* 0x410 */ u8 pad410[0x1BC];
    /* 0x5CC */ f32 suscomp[4];
    /* 0x5DC */ f32 tpcomp[4];
    /* 0x5EC */ u8 pad5EC[0x20];
    /* 0x60C */ u32 roadcode[4];
    /* 0x61C */ u16 sviscode[4];
    /* 0x624 */ u16 roadboost[4];
    /* 0x62C */ u16 sound_flags[4];
    /* 0x634 */ u8 pad634[0x14];
    /* 0x648 */ surftyp *movingSurface;
    /* 0x64C */ u8 pad64C[0x88];
    /* 0x6D4 */ f32 initpos[3];         /* initin: the start point */
    /* 0x6E0 */ f32 inituvs[3][3];      /* the orientation there */
    /* 0x704 */ f32 initoffset[3];      /* the car's offset from it (its grid place), car frame */
    /* 0x710 */ u8 pad710[0x3C];
    /* 0x74C */ f32 reckon_base_rwr[3]; /* reckon: the dead reckoning's base position */
    /* 0x758 */ u8 pad758[0x3C];
    /* 0x794 */ f32 reckon_rwr[3];      /* its position */
    /* 0x7A0 */ f32 reckon_uvs[3][3];   /* its orientation */
    /* 0x7C4 */ u16 obResPt;
    /* 0x7C6 */ s16 slot;
    /* 0x7C8 */ u8 pad7C8[0x40];
} tModelData; /* 0x808 */

/* Place model m at its start: the orientation from initin (stretched with the super speed cheat) for the model, the
 * dead reckoning and the car record (dr_uvs); the position the start point plus the car's offset turned into the
 * world, for the model, the dead reckoning (its position and base) and the car record (dr_pos). Each tire and body
 * corner goes to its world position; the suspensions are uncompressed and every tire is in the air (road and
 * visual code 8), with no boost and no sound flags. */
void init_road(tModelData *m) {
    f32 temp[3];
    s32 i;

    CopyUV(m->inituvs, m->uvs);
    if (gSuperSpeed) {
        make_super_speed(m);
    }
    CopyUV(m->inituvs, m->reckon_uvs);
    CopyUV(m->inituvs, PC_CAR_UVS(m->slot));
    WorldVector(m->initoffset, temp, m->uvs);
    vecadd(m->initpos, temp, m->rwr);
    veccopy(m->rwr, m->reckon_rwr);
    veccopy(m->rwr, m->reckon_base_rwr);
    veccopy(m->rwr, PC_CAR_POS(m->slot));
    for (i = 0; i < 4; i++) {
        WorldVector(m->car->tireR[i], m->tireRwr[i], m->uvs);
        vecadd(m->tireRwr[i], m->rwr, m->tireRwr[i]);
        m->suscomp[i] = 0;
        m->tpcomp[i] = 0;
        m->sviscode[i] = SURF_VIS_AIRBORNE;
        m->roadcode[i] = m->sviscode[i];
        m->roadboost[i] = 0;
        m->sound_flags[i] = 0;
        WorldVector(m->bodyR[i], m->bodyrwr[i], m->uvs);
        vecadd(m->bodyrwr[i], m->rwr, m->bodyrwr[i]);
        veccopy(m->bodyrwr[i], m->bodyrwr2[i]);
    }
}
