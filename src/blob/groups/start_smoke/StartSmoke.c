/* StartSmoke.c: puff smoke, flames or vapour off a car (MAT3 smoke.c, StartSmoke()).
 *
 * UpdateVisuals calls it each frame for a skidding tire and for a wreck, StartNewBlast for a blast's flames, and
 * AnimateWing for the wing trails; each call adds one to nine puffs (scene objects in gSmokeObjs) that UpdateSmoke
 * then moves, grows and fades. */
#include "game/game.h"

/* The car record (tCarData, N64 offsets; docs/port/CARS.md 2): the fields the puffs start from. dr_uvs's rows are
 * the car's axes: [0] x (sideways), [1] y (up), [2] z (forwards). */
typedef struct tCarData { u8 pad0[8]; f32 dr_pos[3]; f32 dr_vel[3]; u8 pad20[12]; f32 dr_uvs[3][3]; u8 pad50[36]; f32 dr_tirepos[4][3]; u8 padA4[84]; s16 rwmph; u8 padFA[0x110 - 0xFA]; struct pc_visual visuals[21]; u8 pad308[0x35C - 0x308]; s8 camera; s8 view; u8 pad35E[0x380 - 0x35E]; void *player; u8 pad384[0x3B8 - 0x384]; } tCarData;
/* The model record (tModelData): body_type, the tires' surfaces (sviscode[4], SURF_VIS_*) and the suspension's
 * compression (reckon.suscomp). sviscode is declared 8 long here because StartSmoke reads it for every type. */
typedef struct tModelData {_tCar *car; Tune44 *tune;u8 body_type;u8 pad9[919]; streetyp *lasttp[4]; u8 pad3B0[92]; const s16 *torquecurve; u8 pad410[524];u16 sviscode[8];u8 pad62C[28]; surftyp *movingSurface; u8 pad64C[272];f32 reckon_suscomp[4];u8 pad76C[156];} tModelData;
/* A smoke puff (MAT3 smoke.c's anon_ll_mat_58, types.h's PcPuff). PC: the handle is the s32 MBOX_NewObject
 * returned, and index its low half: the second s16 on the N64, the first on a little-endian host
 * (docs/port/ENDIAN.md). */
typedef struct SmokePuff { void *next; f32 uvs[9]; f32 pos[3]; union { s32 handle; struct {s16 index,high;} part; } mbIdx; s32 tlifeTime; f32 lifeTime; f32 swTime; f32 vel[3]; u32 data; s16 slot; s16 tIdx; } SmokePuff;
/* A texture definition (MAT3 tTexture, 36 bytes): only its size matters here. */
typedef struct tTexture { u8 bytes[36]; } tTexture;
/* On the PC the effects' per-car arrays are PC data, PC_MAX_LINKS long, with no N64 address (pc_game_types.h,
 * docs/port/DECOUPLE.md 6). */
#define gCarSmokeDir pc_tire_fx
#define gCarBlastSmokeClr pc_smoke_shade
#define gCarWingSmokePos pc_trail_pts


f32 fabsf(f32 x);
#pragma intrinsic (fabsf)

static s16 get_mph(tCarData *c);
static tCarData *get_car(s16 slot);

/* Start smoke of `type` (SMOKE_* in cars.h) off car `slot`; with `water` set (types 0-3 only) a water spray
 * instead (StartWater). `smkoffset` (the wing trails only) is where the wing is, along the car's axes.
 *
 * Nothing starts for a tire in the air (sviscode SURF_VIS_AIRBORNE), for tire smoke below 10 mph or while the frame
 * rate is poor (gGoodFrameRate), nor for tire smoke and wing trails with three or four players. The wreck smokes on
 * every third call only (SMOKE_WRECK_WAIT*). How many puffs: fire 5; tire smoke 1, 2 from 85 mph, 3 from 100, 4
 * from 115; a wing trail that much plus 5 (plus 3 more from 150 mph), or 1 for its first call.
 * Where they go: tire smoke trails behind its tire, 2.25 ft apart, from 2 ft back, sideways at random; the wreck's
 * at random over the car's middle; fire around it; a wing trail fills the line from the trail's last point to the
 * wing's place now, so the trail is unbroken however fast the car goes. Each puff is shown in every view but the
 * car's own in its close camera modes, and coloured: the wing's colour for its trail, a grey that darkens with each
 * puff (175 down to 53) for the wreck, white fire, dirt-coloured or road-coloured tire smoke. */
void StartSmoke(s16 slot, u32 type, s32 water, f32 *smkoffset)
{
    SmokePuff *smk;
    tCarData *car;
    f32 pos[3];
    f32 pos2[3];
    f32 gap;
    f32 offset;
    f32 offset2;
    tModelData *m = PC_MODEL(slot);
    f32 prevPos[3];
    s16 surface = m->sviscode[type];
    s16 mph2;
    s16 first;
    s32 howmany;
    s32 howmanydone;
    s32 wht = PC_RGBA(smoke_white);
    s32 wg1 = PC_RGBA(smoke_wing_color1);
    s32 wg2 = PC_RGBA(smoke_wing_color2);
    s32 wg3 = PC_RGBA(smoke_wing_color3);
    s32 rdClr = PC_RGBA(smoke_road_color);
    s32 dtClr = PC_RGBA(smoke_dirt_color);

    if (!gGoodFrameRate && type < 4 && !water) return;
    /* PC: game.frame_rate 60: smoke only on the frames that complete a 30 Hz frame (pc_game_fps.h), but fire */
    if (pc_fps60 && !pc_frame_steps && !water && type != SMOKE_FIRE) return;

    howmanydone = 0;
    howmany = 1;
    car = get_car(slot);
    mph2 = get_mph(car);
    mph2 = fabsf((f32)mph2);

    /* Original game: `surface` is sviscode[type] for types 4-7 too, past the four tires (roadboost[type - 4]);
     * the wreck's smoke (type 4) is then skipped if roadboost[0] happens to be 8. MAT3 does the same.
     * (The cast keeps surface in memory, as the compiled code has it.) */
    if (*(s16 *)&surface == SURF_VIS_AIRBORNE && type < 5) return;
    if (mph2 < 10 && type < 4) return;
    first = 1;
    gap = type >= SMOKE_WING0 ? 0.75f : 2.25f;
    if (water) {
        StartWater(slot, type);
        return;
    }
    if (type == SMOKE_FIRE) howmany = 5;
    else if (type < 4 || type >= SMOKE_WING0) {
        if (gNumPlayers >= 3) return;
        if (mph2 >= 115) howmany = 4;
        else if (mph2 >= 100) howmany = 3;
        else if (mph2 >= 85) howmany = 2;
        if (type >= SMOKE_WING0) {
            if (type == SMOKE_WING0 && !(gCarSmokeDir[slot] & SMOKE_TRAIL_WING0)) howmany = 1;
            else if (type == SMOKE_WING1 && !(gCarSmokeDir[slot] & SMOKE_TRAIL_WING1)) howmany = 1;
            else howmany += 5;
            if (mph2 >= 150 && howmany != 1) howmany += 3;
        }
    } else {
        /* the wreck: one call puffs, the next two (three with three or four players) don't */
        if (gCarSmokeDir[slot] & SMOKE_WRECK_WAIT1) {
            gCarSmokeDir[slot] &= ~SMOKE_WRECK_WAIT1;
            gCarSmokeDir[slot] |= SMOKE_WRECK_WAIT2;
            return;
        }
        if (gCarSmokeDir[slot] & SMOKE_WRECK_WAIT2) {
            gCarSmokeDir[slot] &= ~SMOKE_WRECK_WAIT2;
            if (gNumPlayers >= 3) gCarSmokeDir[slot] |= SMOKE_WRECK_WAIT3;
            return;
        }
        if (gCarSmokeDir[slot] & SMOKE_WRECK_WAIT3) {
            gCarSmokeDir[slot] &= ~SMOKE_WRECK_WAIT3;
            return;
        }
    }
    do {
        smk = GetStatLLE(&gSmokeList);
        if (!smk) return;
        smk->swTime = 0.0333333015f;    /* seconds a frame of its animation shows */
        smk->data = type << PUFF_TYPE_SHIFT;
        smk->slot = slot;
        /* the scene object: the fire's, or one of the eight smoke frames at random (with 4 MB on the first six
         * tracks, the one-frame smoke) */
        if (type == SMOKE_FIRE) {
            smk->tIdx = -1;
            smk->mbIdx.handle = ZOID_AddObject(gObjList[204], smk->uvs, -1, 0x52000);
            ObjInstanceList[smk->mbIdx.handle].w = 1.00999999f;
        } else {
            if (expansion_pak_present == 0 && gTrackNum >= 0 && gTrackNum < 6) {
                smk->tIdx = 0;
                smk->mbIdx.handle = ZOID_AddObject(gObjList[195], smk->uvs, -1, 0x52000);
            } else {
                smk->tIdx = (s32)Random(8.0f);
                smk->mbIdx.handle = ZOID_AddObject(gObjList[196 + smk->tIdx], smk->uvs, -1, 0x52000);
            }
        }
        /* how long it lives, seconds */
        if (type >= SMOKE_WING0) smk->lifeTime = Random(0.0700000003f) + 0.0799999982f;
        else if (howmany == 1) smk->lifeTime = Random(1.5f) + .5f;
        else if (type == SMOKE_FIRE) smk->lifeTime = Random(2.25f) + 0.5f;
        else smk->lifeTime = Random(0.25f) + .5f;
        /* its drift: an eighth of the car's velocity (the wreck's a sixteenth), half for a wing trail */
        if (type < 5) {
            smk->vel[0] = car->dr_vel[0];
            smk->vel[1] = car->dr_vel[1];
            smk->vel[2] = car->dr_vel[2];
            smk->vel[0] *= 0.125f;
            smk->vel[1] *= 0.125f;
            smk->vel[2] *= 0.125f;
            if (type == SMOKE_WRECK) {
                smk->vel[0] *= 0.5f;
                smk->vel[1] *= 0.5f;
                smk->vel[2] *= 0.5f;
            }
        } else if (type >= SMOKE_WING0) {
            smk->vel[0] = car->dr_vel[0];
            smk->vel[1] = car->dr_vel[1];
            smk->vel[2] = car->dr_vel[2];
            smk->vel[0] *= 0.5f;
            smk->vel[1] *= 0.5f;
            smk->vel[2] *= 0.5f;
        }
        switch (type) {
        case 0:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_T0) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_T0; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_T0; smk->data |= PUFF_FORWARD;
            break;
        case 1:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_T1) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_T1; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_T1; smk->data |= PUFF_FORWARD;
            break;
        case 2:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_T2) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_T2; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_T2; smk->data |= PUFF_FORWARD;
            break;
        case 3:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_T3) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_T3; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_T3; smk->data |= PUFF_FORWARD;
            break;
        case SMOKE_WRECK:
            gCarSmokeDir[slot] |= SMOKE_WRECK_WAIT1;
            if (gNumPlayers == 1) smk->lifeTime += 0.5f;
            if (!(gCarSmokeDir[slot] & SMOKE_WRECK_ON)) {
                gCarSmokeDir[slot] |= SMOKE_WRECK_ON;
                gCarBlastSmokeClr[slot] = 175;
            }
            if (gCarSmokeDir[slot] & SMOKE_SPIN_WRECK) gCarSmokeDir[slot] &= ~SMOKE_SPIN_WRECK;
            else { gCarSmokeDir[slot] |= SMOKE_SPIN_WRECK; smk->data |= PUFF_FORWARD; }
            /* anywhere over the car: 5 ft across, 7 ft along, at its middle's height and 0.75 ft up */
            pos[0] = car->dr_pos[0];
            pos[1] = car->dr_pos[1];
            pos[2] = car->dr_pos[2];
            pos[0] = pos[0] + car->dr_uvs[0][0] * (Random(5.0f) - 2.5f);
            pos[1] = pos[1] + car->dr_uvs[0][1] * (Random(5.0f) - 2.5f);
            pos[2] = pos[2] + car->dr_uvs[0][2] * (Random(5.0f) - 2.5f);
            pos[0] = pos[0] + car->dr_uvs[2][0] * (Random(7.0f) - 3.5f);
            pos[1] = pos[1] + car->dr_uvs[2][1] * (Random(7.0f) - 3.5f);
            pos[2] = pos[2] + car->dr_uvs[2][2] * (Random(7.0f) - 3.5f);
            pos[0] = pos[0] + car->dr_uvs[1][0] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + car->dr_uvs[1][1] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[2] = pos[2] + car->dr_uvs[1][2] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + 0.75f;
            ObjInstanceList[smk->mbIdx.handle].scale = Random(0.5f) + 0.75f;
            break;
        case SMOKE_FIRE:
            /* the five flames get the five fire textures; vel holds their place on the car (4 ft across, 8 ft
             * along), which UpdateSmoke keeps them at */
            ObjInstanceList[smk->mbIdx.handle].scale = 0.200000003f;
            ZOID_SetTexture(smk->mbIdx.part.index, &gTexDefPtrs[gFireTexs[howmanydone] >> 10].texDefs[gFireTexs[howmanydone] & 0x3ff], -1);
            pos[0] = car->dr_pos[0];
            pos[1] = car->dr_pos[1];
            pos[2] = car->dr_pos[2];
            smk->vel[0] = Random(4.0f) - 2.0f;
            smk->vel[2] = Random(8.0f) - 4.0f;
            pos[0] = pos[0] + car->dr_uvs[0][0] * smk->vel[0];
            pos[1] = pos[1] + car->dr_uvs[0][1] * smk->vel[0];
            pos[2] = pos[2] + car->dr_uvs[0][2] * smk->vel[0];
            pos[0] = pos[0] + car->dr_uvs[2][0] * smk->vel[2];
            pos[1] = pos[1] + car->dr_uvs[2][1] * smk->vel[2];
            pos[2] = pos[2] + car->dr_uvs[2][2] * smk->vel[2];
            pos[0] = pos[0] + car->dr_uvs[1][0] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[1] = pos[1] + car->dr_uvs[1][1] * gCarMiddles[PC_MODEL_BODY(slot)];
            pos[2] = pos[2] + car->dr_uvs[1][2] * gCarMiddles[PC_MODEL_BODY(slot)];
            break;
        case SMOKE_WING0:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_WING0) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_WING0; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_WING0; smk->data |= PUFF_FORWARD;
            break;
        case SMOKE_WING1:
            if (gCarSmokeDir[slot] & SMOKE_SPIN_WING1) { gCarSmokeDir[slot] &= ~SMOKE_SPIN_WING1; break; }
            gCarSmokeDir[slot] |= SMOKE_SPIN_WING1; smk->data |= PUFF_FORWARD;
            break;
        }
        if (type < 4) {
            /* behind the tire, gap ft further back for each puff, up to a foot to either side */
            offset2 = -2.0f - (f32)howmanydone * gap;
            pos[0] = car->dr_uvs[2][0] * offset2;
            pos[1] = car->dr_uvs[2][1] * offset2;
            pos[2] = car->dr_uvs[2][2] * offset2;
            pos[0] = pos[0] + car->dr_tirepos[type][0];
            pos[1] = pos[1] + car->dr_tirepos[type][1];
            pos[2] = pos[2] + car->dr_tirepos[type][2];
            pos[0] = pos[0] + car->dr_uvs[0][0] * (Random(2.0f) - 1.0f);
            pos[1] = pos[1] + car->dr_uvs[0][1] * (Random(2.0f) - 1.0f);
            pos[2] = pos[2] + car->dr_uvs[0][2] * (Random(2.0f) - 1.0f);
            pos[1] += PC_MODEL_SUSPENSION(slot)[type];
            ObjInstanceList[smk->mbIdx.handle].scale = Random(0.75f) + 0.25f;
        }
        if (type >= SMOKE_WING0) {
            /* at the wing: smkoffset along the car's axes, then 1.15 ft back, 2.6 ft out to its side, 0.5 ft up */
            pos[0] = car->dr_pos[0];
            pos[1] = car->dr_pos[1];
            pos[2] = car->dr_pos[2];
            pos[0] = pos[0] + car->dr_uvs[1][0] * smkoffset[1];
            pos[1] = pos[1] + car->dr_uvs[1][1] * smkoffset[1];
            pos[2] = pos[2] + car->dr_uvs[1][2] * smkoffset[1];
            pos[0] = pos[0] + car->dr_uvs[2][0] * smkoffset[2];
            pos[1] = pos[1] + car->dr_uvs[2][1] * smkoffset[2];
            pos[2] = pos[2] + car->dr_uvs[2][2] * smkoffset[2];
            pos[0] = pos[0] + -1.14999998f * car->dr_uvs[2][0];
            pos[1] = pos[1] + -1.14999998f * car->dr_uvs[2][1];
            pos[2] = pos[2] + -1.14999998f * car->dr_uvs[2][2];
            if (type == SMOKE_WING0) {
                pos[0] = pos[0] + car->dr_uvs[0][0] * smkoffset[0];
                pos[1] = pos[1] + car->dr_uvs[0][1] * smkoffset[0];
                pos[2] = pos[2] + car->dr_uvs[0][2] * smkoffset[0];
                pos[0] = pos[0] + -2.5999999f * car->dr_uvs[0][0];
                pos[1] = pos[1] + -2.5999999f * car->dr_uvs[0][1];
                pos[2] = pos[2] + -2.5999999f * car->dr_uvs[0][2];
            } else {
                pos[0] = pos[0] + car->dr_uvs[0][0] * smkoffset[0];
                pos[1] = pos[1] + car->dr_uvs[0][1] * smkoffset[0];
                pos[2] = pos[2] + car->dr_uvs[0][2] * smkoffset[0];
                pos[0] = pos[0] + 2.5999999f * car->dr_uvs[0][0];
                pos[1] = pos[1] + 2.5999999f * car->dr_uvs[0][1];
                pos[2] = pos[2] + 2.5999999f * car->dr_uvs[0][2];
            }
            pos[0] = pos[0] + 0.5f * car->dr_uvs[1][0];
            pos[1] = pos[1] + 0.5f * car->dr_uvs[1][1];
            pos[2] = pos[2] + 0.5f * car->dr_uvs[1][2];
            if (first) {
                /* the first puff: keep the trail's last point (in prevPos) and make this one the last */
                if (type == SMOKE_WING0) {
                    if (!(gCarSmokeDir[slot] & SMOKE_TRAIL_WING0)) gCarSmokeDir[slot] |= SMOKE_TRAIL_WING0;
                    else { prevPos[0] = gCarWingSmokePos[slot][0]; prevPos[1] = gCarWingSmokePos[slot][1]; prevPos[2] = gCarWingSmokePos[slot][2]; }
                    gCarWingSmokePos[slot][0] = pos[0]; gCarWingSmokePos[slot][1] = pos[1]; gCarWingSmokePos[slot][2] = pos[2];
                } else if (type == SMOKE_WING1) {
                    if (!(gCarSmokeDir[slot] & SMOKE_TRAIL_WING1)) gCarSmokeDir[slot] |= SMOKE_TRAIL_WING1;
                    else { prevPos[0] = gCarWingSmokePos[slot][3]; prevPos[1] = gCarWingSmokePos[slot][4]; prevPos[2] = gCarWingSmokePos[slot][5]; }
                    gCarWingSmokePos[slot][3] = pos[0]; gCarWingSmokePos[slot][4] = pos[1]; gCarWingSmokePos[slot][5] = pos[2];
                }
            } else {
                /* the others: spread along the line back to the last point */
                pos2[0] = prevPos[0] - pos[0];
                pos2[1] = prevPos[1] - pos[1];
                pos2[2] = prevPos[2] - pos[2];
                offset = NormalVector(pos2);
                if (offset != 0.0f) {
                    offset2 = (f32)howmanydone * (offset / (f32)howmany);
                    pos[0] = pos[0] + offset2 * pos2[0];
                    pos[1] = pos[1] + offset2 * pos2[1];
                    pos[2] = pos[2] + offset2 * pos2[2];
                }
            }
            ObjInstanceList[smk->mbIdx.handle].scale = Random(0.100000001f) + 0.075000003f;
        }
        MBOX_ShowObject(smk->mbIdx.handle, first = 0, 15);
        if (car->camera >= 0 && (car->view == 0 || car->view == 1))
            MBOX_HideObject(smk->mbIdx.handle, 0, 1 << car->camera);
        if (type >= SMOKE_WING0) {
            switch (gCarWings[slot + 1][m->body_type]) {
            case 0: { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = wg1; } break;
            case 1: { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = wg2; } break;
            case 2: { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = wg3; } break;
            }
        } else if (type == SMOKE_WRECK) {
            OBJ_COLOR(ObjInstanceList[smk->mbIdx.handle].color)[0] = gCarBlastSmokeClr[slot];
            OBJ_COLOR(ObjInstanceList[smk->mbIdx.handle].color)[1] = gCarBlastSmokeClr[slot];
            OBJ_COLOR(ObjInstanceList[smk->mbIdx.handle].color)[2] = gCarBlastSmokeClr[slot];
            OBJ_COLOR(ObjInstanceList[smk->mbIdx.handle].color)[3] = 255;
            gCarBlastSmokeClr[slot] -= 3;
            if (gCarBlastSmokeClr[slot] < 53) gCarBlastSmokeClr[slot] = 53;
        } else if (type == SMOKE_FIRE) { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = wht; }
        else if (surface == SURF_VIS_DIRT) { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = dtClr; }
        else { s16 h = smk->mbIdx.handle; ObjInstanceList[h].color = rdClr; }
        smk->pos[0] = pos[0];
        smk->pos[1] = pos[1];
        smk->pos[2] = pos[2];
        howmanydone++;
    } while (howmany != howmanydone);
}

/* The car record of `slot`. */
static tCarData *get_car(s16 slot) { return PC_CAR(slot); }
/* The car's speed, mph (rwmph holds mph x 4). */
static s16 get_mph(tCarData *c) { return c->rwmph >> 2; }
