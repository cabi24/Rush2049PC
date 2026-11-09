/* StartWater.c: start the water spray behind a tire in water (MAT3 smoke.c, StartWater()).
 *
 * StartSmoke hands a tire over here when it is in water. A spray is two puffs (scene objects in gSmokeObjs, the
 * second showing the spray's animation four frames on); UpdateSmoke keeps them at the tire and ends them when the
 * tire leaves the water. */
#include "game/game.h"

float fabsf(float);
#pragma intrinsic (fabsf)

/* The car record (tCarData, N64 offsets; docs/port/CARS.md 2): the fields the spray starts from. */
typedef struct tCarData {
    u8 pad0[20];
    f32 dr_vel[3];          /* +0x14 */
    u8 pad20[12];
    f32 dr_uvs0[3];         /* +0x2C the car's x axis (sideways) */
    u8 pad38[60];
    f32 dr_tirepos[4][3];   /* +0x74 */
    u8 padA4[84];
    s16 rwmph;              /* +0xF8 mph x 4 */
    u8 padFA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad308[0x380 - 0x308]; void *player; u8 pad384[0x3B8 - 0x384];
} tCarData;

/* A smoke puff (MAT3 smoke.c's anon_ll_mat_58, types.h's PcPuff); the offsets are the N64's. */
typedef struct SmokePuff {
    void *next;             /* the pool link: a puff (UpdateSmoke's SmokePuff), pointer-sized here */
    f32 uvs[3][3];          /* +0x04 */
    f32 pos[3];             /* +0x28 */
    s32 mbIdx;              /* +0x34 its scene object */
    u8 pad38[8];
    f32 swTime;             /* +0x40 seconds to its next animation frame */
    f32 vel[3];             /* +0x44 its offset from the tire */
    s32 data;               /* +0x50 PUFF_WATER, the tire << PUFF_TYPE_SHIFT */
    s16 slot;               /* +0x54 */
    s16 tIdx;               /* +0x56 its animation frame */
} SmokePuff;

/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
VIEW(u8[], gSmokeList);
#define gSmokeList gSmokeList_view

#define DEBUG_PRINT(args)

s32 ZOID_AddObject(s32 a, void *b, s16 parent, s32 d);

/* Put puff `smk` in the scene as frame tIdx of the spray's animation (gObjList[205..212]). */
static void attach(SmokePuff *smk) {
    s32 handle;
    handle = ZOID_AddObject(gObjList[205 + smk->tIdx], smk->uvs, -1, 0x40000);
    smk->mbIdx = handle;
}

/* Start car `slot`'s water spray behind `tire` (0-3), unless that tire already has one (its SMOKE_WATER latch,
 * which UpdateSmoke frees). Both puffs point along the car's velocity, 1.25 ft above the tire and up to half a foot
 * to either side along the car's x axis; below 90 mph they are smaller (15%-100% forwards, 25%-100% in reverse).
 * The two puffs share the turn and size and differ in their sideways offset and animation frame (0 and 4). */
void StartWater(s16 slot, u32 tire) {
    tCarData *car;
    s32 data;
    s32 i, j;
    f32 pos2[3];
    f32 offset;
    f32 offset2;
    s16 mph;
    s16 mph2;
    SmokePuff *smk;
    s32 snap;               /* the sentinel, for a debug check the retail build compiled out */
    f32 mat[3][3];

    car = PC_CAR(slot);
    snap = start_water_sentinel;
    mph = car->rwmph >> 2;
    mph2 = fabsf((f32)mph);

    if (tire > 3) DEBUG_PRINT(("t"));
    switch (tire) {
    case 0: if (gCarSmokeDir[slot] & SMOKE_WATER_T0) return; gCarSmokeDir[slot] |= SMOKE_WATER_T0; break;
    case 1: if (gCarSmokeDir[slot] & SMOKE_WATER_T1) return; gCarSmokeDir[slot] |= SMOKE_WATER_T1; break;
    case 2: if (gCarSmokeDir[slot] & SMOKE_WATER_T2) return; gCarSmokeDir[slot] |= SMOKE_WATER_T2; break;
    case 3: if (gCarSmokeDir[slot] & SMOKE_WATER_T3) return; gCarSmokeDir[slot] |= SMOKE_WATER_T3; break;
    }

    smk = GetStatLLE(gSmokeList);
    if (smk == 0) return;
    smk->swTime = 0.0333333f;
    smk->slot = slot;
    smk->data = data = (tire << PUFF_TYPE_SHIFT) | PUFF_WATER;
    pos2[0] = car->dr_vel[0];
    pos2[1] = car->dr_vel[1];
    pos2[2] = car->dr_vel[2];
    NormalVector(pos2);
    LookInDir(pos2, mat);
    CopyUV(mat, smk->uvs);
    if (mph2 < 90) {
        if (mph < 0) {
            offset2 = (f32)mph2 / 70.0f * 0.75f + 0.25f;
        } else {
            offset2 = (f32)mph2 / 90.0f * 0.85f + 0.15f;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                smk->uvs[i][j] *= offset2;
            }
        }
    }
    smk->pos[0] = car->dr_tirepos[tire][0];
    smk->pos[1] = car->dr_tirepos[tire][1];
    smk->pos[2] = car->dr_tirepos[tire][2];
    smk->vel[0] = car->dr_uvs0[0];
    smk->vel[1] = car->dr_uvs0[1];
    smk->vel[2] = car->dr_uvs0[2];
    offset = Random(1.0f) - 0.5f;
    smk->tIdx = 0;
    smk->vel[0] *= offset;
    smk->vel[1] *= offset;
    smk->vel[2] *= offset;
    smk->pos[0] = smk->pos[0] + smk->vel[0];
    smk->pos[1] = smk->pos[1] + smk->vel[1];
    smk->pos[2] = smk->pos[2] + smk->vel[2];
    smk->pos[1] += 1.25f;
    smk->mbIdx = ZOID_AddObject(gObjList[205], smk->uvs, -1, 0x40000);

    if (snap != start_water_sentinel) DEBUG_PRINT(("x"));
    smk = GetStatLLE(gSmokeList);
    if (smk == 0) return;
    smk->swTime = 0.0333333f;
    smk->data = data;
    smk->slot = slot;
    CopyUV(mat, smk->uvs);
    if (mph2 < 90) {
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                smk->uvs[i][j] *= offset2;
            }
        }
    }
    smk->pos[0] = car->dr_tirepos[tire][0];
    smk->pos[1] = car->dr_tirepos[tire][1];
    smk->pos[2] = car->dr_tirepos[tire][2];
    smk->vel[0] = car->dr_uvs0[0];
    smk->vel[1] = car->dr_uvs0[1];
    smk->vel[2] = car->dr_uvs0[2];
    offset = Random(1.0f) - 0.5f;
    smk->tIdx = 4;
    smk->vel[0] *= offset;
    smk->vel[1] *= offset;
    smk->vel[2] *= offset;
    smk->pos[0] = smk->pos[0] + smk->vel[0];
    smk->pos[1] = smk->pos[1] + smk->vel[1];
    smk->pos[2] = smk->pos[2] + smk->vel[2];
    smk->pos[1] += 1.25f;
    attach(smk);
}
