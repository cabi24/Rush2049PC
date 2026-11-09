/* UpdateSmoke.c: move, grow, fade and animate every smoke puff once a frame (MAT3 smoke.c, UpdateSmoke()).
 *
 * The puffs are scene objects StartSmoke and StartWater put in the world: tire smoke, the wreck's smoke, a blast's
 * flames, the wing trails and the water sprays (types: SMOKE_* in cars.h). Each lives in gSmokeObjs, listed by
 * gSmokeList; this frees it when it is done. */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];
/* PC: the handle is the s32 MBOX_NewObject returned, and index its low half: the second s16 on the N64, the first
 * on a little-endian host (docs/port/ENDIAN.md). */
typedef union Id { s32 handle; struct { s16 index, high; } part; } Id;
/* A smoke puff (MAT3 smoke.c's anon_ll_mat_58, types.h's PcPuff); the offsets are the N64's, the PC's link is
 * pointer-sized. */
typedef struct SmokePuff {
    struct SmokePuff *next;
    Mat3 uvs;           /* 4: its orientation and size (the scene object's matrix) */
    Vec3 pos;           /* 40: its position */
    Id mbIdx;           /* 52: its scene object */
    f32 tlifeTime;
    f32 lifeTime;       /* 60: seconds left (fire: the size it grows to) */
    f32 swTime;         /* 64: seconds to the next animation frame */
    Vec3 vel;           /* 68: its drift, feet a second (fire, a water spray: its offset from the car) */
    u32 data;           /* 80: PUFF_FORWARD, PUFF_WATER, the type << PUFF_TYPE_SHIFT */
    s16 slot;           /* 84: the car it came from */
    s16 tIdx;           /* 86: its animation frame, 0-7 (fire: -1 while growing, then 0-14) */
} SmokePuff;
/* The car record (tCarData, N64 offsets; docs/port/CARS.md 2), the fields the puffs follow. */
typedef struct tCarData {
    u8 pad0[8];
    Vec3 dr_pos;        /* 8 */
    Vec3 dr_vel;        /* 20 */
    u8 pad32[12];
    Mat3 dr_uvs;        /* 44: its orientation: right, up, forward */
    u8 pad80[36];
    Vec3 dr_tirepos[4]; /* 116 */
    u8 pad164[84];
    s16 rwmph;          /* 248: mph x 4 */
    u8 pad250[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308];
    s8 camera;          /* 860: the view that follows the car, a bit of the views' mask */
    s8 view;            /* 861: that view's camera mode (0 and 1 hide the car's own sprays and smoke) */
    u8 pad35E[0x380 - 0x35E]; void *player; u8 pad384[0x3B8 - 0x384];
} tCarData;

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */
/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx


VIEW(volatile f32, gFrameSeconds);
#define gFrameSeconds gFrameSeconds_view

extern f32 fabsf(f32);
#pragma intrinsic(fabsf)
/* PC: game.frame_rate 60 (pc_game_fps.h). The glow's pulse and the puffs' fade, jitter and growth are per frame of
 * two retraces on the N64: scaled to the frame, or run once a 30 Hz frame; PC_ON30(c) is c on a frame that
 * completes a 30 Hz frame, false on the others. */
#define PC_ON30(c) ((c) && (!pc_fps60 || pc_frame_steps))

/* Show scene node `index` as object definition `def` (ZOID_SetObjectDef's work, inlined). */
static void resource_set_frame(s16 index, u16 def)
{
    ObjInstanceList[index].index = def;
}

/* out = a + b, three floats. */
static void vadd(f32 *a, f32 *b, f32 *out)
{
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
}

/* One frame of every puff:
 * - a water spray (PUFF_WATER) stays at its tire, its offset vel added and 1.25 ft up, turned along the car's
 *   velocity and shrunk below 90 mph (to 15%-100% forwards, 25%-100% in reverse); it stops, and frees its tire's
 *   SMOKE_WATER latch, when the tire leaves water (sviscode 2) or the car slows below 10 mph. Hidden from the car's
 *   own view in camera modes 0 and 1.
 * - fire (SMOKE_FIRE) rides the car while it is crashed: it grows to lifeTime by 0.04 a frame, then pulses (14
 *   frames: shrinking by 0.03 for 8, growing for the rest); it ends with the crash.
 * - every other puff drifts by vel, grows (wing trails 0.03 a frame, the rest 0.05), fades in its last second (alpha
 *   down 9 a frame) and ends when its lifeTime runs out; the wreck's smoke also jitters upwards.
 * Every puff steps its animation (the smoke objects of gObjList[196..203], the spray's of [205..212]) every swTime
 * seconds, forwards or backwards by PUFF_FORWARD; with 4 MB and on the first six tracks the smoke has one frame. */
void UpdateSmoke(void)
{
    SmokePuff *puff;
    s16 slot;
    s16 type;
    s16 mph;
    s16 mph2;
    f32 t;
    s32 i, j;
    tCarData *gc;
    Vec3 pos;
    Vec3 pos2;
    s32 pad1[2];        /* unused: the frame layout m2c matched */
    SmokePuff *next;
    s32 pad2[5];        /* unused */
    Mat3 mat;

    for (puff = gSmokeList.head; puff != 0; puff = next) {
        next = puff->next;
        slot = puff->slot;
        type = (puff->data >> PUFF_TYPE_SHIFT) & 0xF;
        if (puff->data & PUFF_WATER) {
            gc = (tCarData *)(uintptr_t)PC_CAR(slot);
            mph = gc->rwmph >> 2;
            mph2 = fabsf((f32)mph);
            if (gc->view < 2) {
                MBOX_HideObject(puff->mbIdx.handle, 1, 1 << gc->camera);
            } else {
                MBOX_ShowObject(puff->mbIdx.handle, 0, 1 << gc->camera);
            }
            if (PC_MODEL_VISCODE(slot)[type] != SURF_VIS_WATER || mph2 < 10) {
                switch (type) {
                case 0:
                    gCarSmokeDir[slot] &= ~SMOKE_WATER_T0;
                    break;
                case 1:
                    gCarSmokeDir[slot] &= ~SMOKE_WATER_T1;
                    break;
                case 2:
                    gCarSmokeDir[slot] &= ~SMOKE_WATER_T2;
                    break;
                case 3:
                    gCarSmokeDir[slot] &= ~SMOKE_WATER_T3;
                    break;
                }
                goto release;
            }
            pos2[0] = gc->dr_vel[0];
            pos2[1] = gc->dr_vel[1];
            pos2[2] = gc->dr_vel[2];
            NormalVector(pos2);
            LookInDir(pos2, mat);
            CopyUV(mat, puff->uvs);
            if (mph2 < 90) {
                if (mph < 0) {
                    t = mph2 / 70.0f * 0.75f + 0.25f;
                } else {
                    t = mph2 / 90.0f * 0.85f + 0.15f;
                }
                for (i = 0; i < 3; i++) {
                    for (j = 0; j < 3; j++) {
                        puff->uvs[i][j] *= t;
                    }
                }
            }
            puff->pos[0] = gc->dr_tirepos[type][0];
            puff->pos[1] = gc->dr_tirepos[type][1];
            puff->pos[2] = gc->dr_tirepos[type][2];
            vadd(puff->pos, puff->vel, puff->pos);
            puff->pos[1] += 1.25f;
            puff->swTime -= gFrameSeconds;
            if (puff->swTime <= 0.0f) {
                resource_set_frame(puff->mbIdx.part.index, gObjList[196 + 9 + puff->tIdx]);
                puff->tIdx++;
                if (puff->tIdx >= 8) {
                    puff->tIdx = 0;
                }
                puff->swTime = 0.0333333f;
            }
        } else if (type == SMOKE_FIRE) {
            do {
                if (!PC_MODEL_CRASHFLAG(slot)) {
                    goto release;
                }
                if (puff->tIdx == -1) {
                    if (ObjInstanceList[puff->mbIdx.handle].scale < puff->lifeTime) {
                        ObjInstanceList[puff->mbIdx.handle].scale += PC_FSCALE(0.04f);
                    } else {
                        puff->tIdx = 0;
                    }
                } else {
                    if (puff->tIdx < 8) {
                        ObjInstanceList[puff->mbIdx.handle].scale -= PC_FSCALE(0.03f);
                    } else {
                        ObjInstanceList[puff->mbIdx.handle].scale += PC_FSCALE(0.03f);
                    }
                    if (PC_ON30(puff->tIdx >= 14)) {
                        puff->tIdx = 0;
                    } else {
                        PC_EACH30(puff->tIdx++;)
                    }
                }
                /* fire keeps its place on the car: vel holds its sideways and lengthways offsets */
                gc = (tCarData *)(uintptr_t)PC_CAR(slot);
                pos[0] = gc->dr_pos[0];
                pos[1] = gc->dr_pos[1];
                pos[2] = gc->dr_pos[2];
                pos[0] = pos[0] + gc->dr_uvs[0][0] * puff->vel[0];
                pos[1] = pos[1] + gc->dr_uvs[0][1] * puff->vel[0];
                pos[2] = pos[2] + gc->dr_uvs[0][2] * puff->vel[0];
                pos[0] = pos[0] + gc->dr_uvs[2][0] * puff->vel[2];
                pos[1] = pos[1] + gc->dr_uvs[2][1] * puff->vel[2];
                pos[2] = pos[2] + gc->dr_uvs[2][2] * puff->vel[2];
                pos[0] = pos[0] + gc->dr_uvs[1][0] * gCarMiddles[PC_MODEL_BODY(slot)];
                pos[1] = pos[1] + gc->dr_uvs[1][1] * gCarMiddles[PC_MODEL_BODY(slot)];
                pos[2] = pos[2] + gc->dr_uvs[1][2] * gCarMiddles[PC_MODEL_BODY(slot)];
                puff->pos[0] = pos[0];
                puff->pos[1] = pos[1];
                puff->pos[2] = pos[2];
            } while (0);
        } else {
            puff->lifeTime -= gFrameSeconds;
            if (puff->lifeTime <= 0.0f) {
release:
                RemoveObjectTree(puff->mbIdx.part.index, 0, 0);
                FreeStatLLE(&gSmokeList, puff);
                continue;
            }
            gFrameSeconds;      /* a read of the volatile and nothing more (MAT3: t = gFrameSeconds, unused) */
            if (puff->lifeTime < 1.0f) {
                if (OBJ_COLOR(ObjInstanceList[puff->mbIdx.handle].color)[3] >= 9) {
                    PC_EACH30(OBJ_COLOR(ObjInstanceList[puff->mbIdx.handle].color)[3] -= 9;)
                }
            }
            if (type == SMOKE_WRECK) {
                PC_EACH30(puff->pos[0] += Random(0.3f) - 0.15f;)
                PC_EACH30(puff->pos[1] += Random(0.25f);)
                PC_EACH30(puff->pos[2] += Random(0.15f) - 0.075f;)
                ObjInstanceList[puff->mbIdx.handle].scale += PC_FSCALE(0.05f);
            }
            puff->swTime -= gFrameSeconds;
            if (puff->swTime <= 0.0f) {
                if (expansion_pak_present || gTrackNum >= 6) {
                    resource_set_frame(puff->mbIdx.part.index, gObjList[196 + puff->tIdx]);
                    if (puff->data & PUFF_FORWARD) {
                        puff->tIdx++;
                        if (puff->tIdx >= 8) {
                            puff->tIdx = 0;
                        }
                    } else {
                        puff->tIdx--;
                        if (puff->tIdx < 0) {
                            puff->tIdx = 7;
                        }
                    }
                }
                puff->swTime = 0.0333333f;
            }
            if (type >= SMOKE_WING0) {
                ObjInstanceList[puff->mbIdx.handle].scale += PC_FSCALE(0.03f);
            } else {
                ObjInstanceList[puff->mbIdx.handle].scale += PC_FSCALE(0.05f);
            }
            puff->pos[0] += puff->vel[0] * gFrameSeconds;
            puff->pos[1] += puff->vel[1] * gFrameSeconds;
            puff->pos[2] += puff->vel[2] * gFrameSeconds;
        }
    }
}
