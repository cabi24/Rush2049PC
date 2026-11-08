/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * StartWater(idx, type): spawn the two "wheel effect" objects for car idx, wheel `type` (0..3).
 *   - once-per-wheel latch: bit 0x100<<type in gCarSmokeDir[idx]; returns if already set.
 *   - two objects from the pool gSmokeList (GetStatLLE): f64 = 0.0333333f, h84 = idx,
 *     w80 = (type << 17) | 0x10; orientation = basis(normalize(car->dir)) (NormalVector,
 *     LookInDir, CopyUV copy), scaled when |speed| < 90
 *     (speed = car->h248 >> 2; reverse: |s|/70*0.75+0.25, forward: |s|/90*0.85+0.15);
 *     pos = car->wheel[type], vel = car->vel * (Random(1.0f) - 0.5f), pos += vel, pos.y += 1.25;
 *     w52 = ZOID_AddObject(gObjList_water[h86], &o->m, -1, 0x40000) with h86 = 0 / 4.
 * No arcade ancestor identified.
 * STATE: EQUAL in the whole-program unit (was 22/386).
 * Frame shaping (all disclosed):
 *   - The last ZOID_AddObject call is an inlined static helper attach(o) whose named
 * local h holds the result. Mechanism: umerge places each inlined callee's
 *     area at an 8-aligned start in call order. The two inlined Random areas leave
 *     the merged frame at 200; attach's 4-byte area after them gives retail's 204, so
 *     every uopt spill-temp home lands +4 as in retail (no extra coloured web needed).
 *     The helper's real identity is unknown; it leaves no stub (static, one call site).
 *   - Unused locals pB1, pC1..pC7, pD1, pD2, pE1..pE3, pF0..pF4 (18 words) are the exact
 *     frame residual: they reproduce every retail local offset (dir 228, scale 216,
 * speed 186, aspeed 184, o 172, snap 168, basis 120). needed 22; with the
 *     helper area the bottom four (pF5..pF8) are no longer needed (3 or 4 removed both match).
 *   - Random() = Random(1.0f) (locked, inlined; keeps the *1.0f multiply);
 *   - `snap = start_water_sentinel` + compiled-out check before the second alloc = retail's dead sw 168(sp);
 *   - `if (type > 3) DEBUG_PRINT` before the switch fixes car t4 / seed t3 colouring;
 *   - u32 type stops the switch's `case 3` sharing the loop-bound register (li at,3);
 *   - pos = pos + vel in order 0,1,2; first lookup is gObjList_water[0], second gObjList_water[o->h86].
 */
typedef signed char s8; typedef unsigned char u8; typedef signed short s16; typedef unsigned short u16;
typedef signed int s32; typedef unsigned int u32; typedef float f32;
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

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
extern s32 start_water_sentinel;
/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
extern u8 gSmokeList[];
extern u16 gObjList_water[];
extern int rand_seed;
#define DEBUG_PRINT(args)

Obj *GetStatLLE(void *pool);
f32 NormalVector(f32 *v);
void LookInDir(f32 *dir, f32 m[3][3]);
void CopyUV(void *src, void *dst);
s32 ZOID_AddObject(s32 a, void *b, s16 parent, s32 d);

f32 Random(f32 max);

static void attach(Obj *o) {
    s32 h;
    h = ZOID_AddObject(gObjList_water[o->h86], o->m, -1, 0x40000);
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
    o->w52 = ZOID_AddObject(gObjList_water[0], o->m, -1, 0x40000);

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
