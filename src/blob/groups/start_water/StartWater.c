#include "game/game.h"

float fabsf(float);
#pragma intrinsic (fabsf)

typedef struct Car {
    u8 pad0[20];
    f32 dir[3];          /* +0x14 */
    u8 pad32[12];
    f32 vel[3];          /* +0x2C */
    u8 pad56[60];
    f32 wheel[4][3];     /* +0x74 */
    u8 pad164[84];
    s16 h248;            /* +0xF8 */
    u8 pad250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad250_308[0x380 - 0x308]; void *input; u8 pad250_384[0x3B8 - 0x384];
} Car;

typedef struct Obj {
    void *next;          /* the pool link: a puff (UpdateSmoke's Puff), pointer-sized here */
    f32 m[3][3];         /* +0x04 */
    f32 pos[3];          /* +0x28 */
    s32 w52;             /* +0x34 */
    u8 pad56[8];
    f32 f64;             /* +0x40 */
    f32 vel[3];          /* +0x44 */
    s32 w80;             /* +0x50 */
    s16 h84;             /* +0x54 */
    s16 h86;             /* +0x56 */
} Obj;

/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
VIEW(u8[], gSmokeList);
#define gSmokeList gSmokeList_view

#define DEBUG_PRINT(args)

s32 ZOID_AddObject(s32 a, void *b, s16 parent, s32 d);

static void attach(Obj *o) {
    s32 h;
    h = ZOID_AddObject(gObjList[205 + o->h86], o->m, -1, 0x40000);
    o->w52 = h;
}
void StartWater(s16 idx, u32 type) {
    Car *car;
    s32 flags;
    s32 i, j;
    f32 dir[3];
    f32 r;
    s32 pB1;
    f32 scale;
    s32 pC1, pC2, pC3, pC4, pC5, pC6, pC7;
    s16 speed;
    s16 aspeed;
    s32 pD1, pD2;
    Obj *o;
    s32 snap;
    s32 pE1, pE2, pE3;
    f32 basis[3][3];
    s32 pF0;
    s32 pF1;
    s32 pF2;
    s32 pF3;
    s32 pF4;

    car = PC_CAR(idx);
    snap = start_water_sentinel;
    speed = car->h248 >> 2;
    aspeed = fabsf((f32)speed);

    if (type > 3) DEBUG_PRINT(("t"));
    switch (type) {
    case 0: if (gCarSmokeDir[idx] & 0x100) return; gCarSmokeDir[idx] |= 0x100; break;
    case 1: if (gCarSmokeDir[idx] & 0x200) return; gCarSmokeDir[idx] |= 0x200; break;
    case 2: if (gCarSmokeDir[idx] & 0x400) return; gCarSmokeDir[idx] |= 0x400; break;
    case 3: if (gCarSmokeDir[idx] & 0x800) return; gCarSmokeDir[idx] |= 0x800; break;
    }

    o = GetStatLLE(gSmokeList);
    if (o == 0) return;
    o->f64 = 0.0333333f;
    o->h84 = idx;
    o->w80 = flags = (type << 17) | 0x10;
    dir[0] = car->dir[0];
    dir[1] = car->dir[1];
    dir[2] = car->dir[2];
    NormalVector(dir);
    LookInDir(dir, basis);
    CopyUV(basis, o->m);
    if (aspeed < 90) {
        if (speed < 0) {
            scale = (f32)aspeed / 70.0f * 0.75f + 0.25f;
        } else {
            scale = (f32)aspeed / 90.0f * 0.85f + 0.15f;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                o->m[i][j] *= scale;
            }
        }
    }
    o->pos[0] = car->wheel[type][0];
    o->pos[1] = car->wheel[type][1];
    o->pos[2] = car->wheel[type][2];
    o->vel[0] = car->vel[0];
    o->vel[1] = car->vel[1];
    o->vel[2] = car->vel[2];
    r = Random(1.0f) - 0.5f;
    o->h86 = 0;
    o->vel[0] *= r;
    o->vel[1] *= r;
    o->vel[2] *= r;
    o->pos[0] = o->pos[0] + o->vel[0];
    o->pos[1] = o->pos[1] + o->vel[1];
    o->pos[2] = o->pos[2] + o->vel[2];
    o->pos[1] += 1.25f;
    o->w52 = ZOID_AddObject(gObjList[205], o->m, -1, 0x40000);

    if (snap != start_water_sentinel) DEBUG_PRINT(("x"));
    o = GetStatLLE(gSmokeList);
    if (o == 0) return;
    o->f64 = 0.0333333f;
    o->w80 = flags;
    o->h84 = idx;
    CopyUV(basis, o->m);
    if (aspeed < 90) {
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                o->m[i][j] *= scale;
            }
        }
    }
    o->pos[0] = car->wheel[type][0];
    o->pos[1] = car->wheel[type][1];
    o->pos[2] = car->wheel[type][2];
    o->vel[0] = car->vel[0];
    o->vel[1] = car->vel[1];
    o->vel[2] = car->vel[2];
    r = Random(1.0f) - 0.5f;
    o->h86 = 4;
    o->vel[0] *= r;
    o->vel[1] *= r;
    o->vel[2] *= r;
    o->pos[0] = o->pos[0] + o->vel[0];
    o->pos[1] = o->pos[1] + o->vel[1];
    o->pos[2] = o->pos[2] + o->vel[2];
    o->pos[1] += 1.25f;
    attach(o);
}
