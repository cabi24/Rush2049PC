/* UpdateSmoke.c: move, grow, fade and animate every smoke puff once a frame (MAT3 smoke.c, UpdateSmoke()).
 *
 * The puffs are scene objects StartSmoke and StartWater put in the world: tire smoke, the wreck's smoke, a blast's
 * flames, the wing trails and the water sprays (types: SMOKE_* in cars.h). Each lives in gSmokeObjs, listed by
 * gSmokeList; this frees it when it is done. */
#include "game/game.h"

typedef f32 Vec3[3];
typedef f32 Mat3[3][3];

/* On the PC the tire-effect latches are pc_tire_fx, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx


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
            gc = &pc_cars[slot];
            mph = gc->rwmph >> 2;
            mph2 = fabsf((f32)mph);
            if (gc->view < 2) {
                MBOX_HideObject(puff->mbIdx, 1, 1 << gc->camera);
            } else {
                MBOX_ShowObject(puff->mbIdx, 0, 1 << gc->camera);
            }
            if (pc_models[slot].sviscode[type] != SURF_VIS_WATER || mph2 < 10) {
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
            CopyUV(mat, puff->mat.uvs);
            if (mph2 < 90) {
                if (mph < 0) {
                    t = mph2 / 70.0f * 0.75f + 0.25f;
                } else {
                    t = mph2 / 90.0f * 0.85f + 0.15f;
                }
                for (i = 0; i < 3; i++) {
                    for (j = 0; j < 3; j++) {
                        puff->mat.uvs[i][j] *= t;
                    }
                }
            }
            puff->mat.pos[0] = gc->dr_tirepos[type][0];
            puff->mat.pos[1] = gc->dr_tirepos[type][1];
            puff->mat.pos[2] = gc->dr_tirepos[type][2];
            vadd(puff->mat.pos, puff->vel, puff->mat.pos);
            puff->mat.pos[1] += 1.25f;
            puff->swTime -= gFrameSeconds;
            if (puff->swTime <= 0.0f) {
                resource_set_frame(puff->mbNode, gObjList[196 + 9 + puff->tIdx]);
                puff->tIdx++;
                if (puff->tIdx >= 8) {
                    puff->tIdx = 0;
                }
                puff->swTime = 0.0333333f;
            }
        } else if (type == SMOKE_FIRE) {
            do {
                if (!pc_models[slot].crashflag) {
                    goto release;
                }
                if (puff->tIdx == -1) {
                    if (ObjInstanceList[puff->mbIdx].scale < puff->lifeTime) {
                        ObjInstanceList[puff->mbIdx].scale += PC_FSCALE(0.04f);
                    } else {
                        puff->tIdx = 0;
                    }
                } else {
                    if (puff->tIdx < 8) {
                        ObjInstanceList[puff->mbIdx].scale -= PC_FSCALE(0.03f);
                    } else {
                        ObjInstanceList[puff->mbIdx].scale += PC_FSCALE(0.03f);
                    }
                    if (PC_ON30(puff->tIdx >= 14)) {
                        puff->tIdx = 0;
                    } else {
                        PC_EACH30(puff->tIdx++;)
                    }
                }
                /* fire keeps its place on the car: vel holds its sideways and lengthways offsets */
                gc = &pc_cars[slot];
                pos[0] = gc->dr_pos[0];
                pos[1] = gc->dr_pos[1];
                pos[2] = gc->dr_pos[2];
                pos[0] = pos[0] + gc->dr_uvs[0][0] * puff->vel[0];
                pos[1] = pos[1] + gc->dr_uvs[0][1] * puff->vel[0];
                pos[2] = pos[2] + gc->dr_uvs[0][2] * puff->vel[0];
                pos[0] = pos[0] + gc->dr_uvs[2][0] * puff->vel[2];
                pos[1] = pos[1] + gc->dr_uvs[2][1] * puff->vel[2];
                pos[2] = pos[2] + gc->dr_uvs[2][2] * puff->vel[2];
                pos[0] = pos[0] + gc->dr_uvs[1][0] * gCarMiddles[pc_models[slot].body_type];
                pos[1] = pos[1] + gc->dr_uvs[1][1] * gCarMiddles[pc_models[slot].body_type];
                pos[2] = pos[2] + gc->dr_uvs[1][2] * gCarMiddles[pc_models[slot].body_type];
                puff->mat.pos[0] = pos[0];
                puff->mat.pos[1] = pos[1];
                puff->mat.pos[2] = pos[2];
            } while (0);
        } else {
            puff->lifeTime -= gFrameSeconds;
            if (puff->lifeTime <= 0.0f) {
release:
                RemoveObjectTree(puff->mbNode, 0, 0);
                FreeStatLLE(&gSmokeList, puff);
                continue;
            }
            gFrameSeconds;      /* a read of the volatile and nothing more (MAT3: t = gFrameSeconds, unused) */
            if (puff->lifeTime < 1.0f) {
                if (OBJ_COLOR(ObjInstanceList[puff->mbIdx].color)[3] >= 9) {
                    PC_EACH30(OBJ_COLOR(ObjInstanceList[puff->mbIdx].color)[3] -= 9;)
                }
            }
            if (type == SMOKE_WRECK) {
                PC_EACH30(puff->mat.pos[0] += Random(0.3f) - 0.15f;)
                PC_EACH30(puff->mat.pos[1] += Random(0.25f);)
                PC_EACH30(puff->mat.pos[2] += Random(0.15f) - 0.075f;)
                ObjInstanceList[puff->mbIdx].scale += PC_FSCALE(0.05f);
            }
            puff->swTime -= gFrameSeconds;
            if (puff->swTime <= 0.0f) {
                if (expansion_pak_present || gTrackNum >= 6) {
                    resource_set_frame(puff->mbNode, gObjList[196 + puff->tIdx]);
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
                ObjInstanceList[puff->mbIdx].scale += PC_FSCALE(0.03f);
            } else {
                ObjInstanceList[puff->mbIdx].scale += PC_FSCALE(0.05f);
            }
            puff->mat.pos[0] += puff->vel[0] * gFrameSeconds;
            puff->mat.pos[1] += puff->vel[1] * gFrameSeconds;
            puff->mat.pos[2] += puff->vel[2] * gFrameSeconds;
        }
    }
}
