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

/* Place model m at its start: the orientation from initin (stretched with the super speed cheat) for the model, the
 * dead reckoning and the car record (dr_uvs); the position the start point plus the car's offset turned into the
 * world, for the model, the dead reckoning (its position and base) and the car record (dr_pos). Each tire and body
 * corner goes to its world position; the suspensions are uncompressed and every tire is in the air (road and
 * visual code 8), with no boost and no sound flags. */
void init_road(tModelData *m) {
    f32 temp[3];
    s32 i;

    CopyUV(m->initin.uvs, m->uvs);
    if (gSuperSpeed) {
        make_super_speed(m);
    }
    CopyUV(m->initin.uvs, m->reckon.uvs);
    CopyUV(m->initin.uvs, PC_CAR_UVS(m->slot));
    WorldVector(m->initin.offset, temp, m->uvs);
    vecadd(m->initin.pos, temp, m->rwr);
    veccopy(m->rwr, m->reckon.rwr);
    veccopy(m->rwr, m->reckon.base_rwr);
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
