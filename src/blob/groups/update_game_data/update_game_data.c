/* update_game_data.c: the model thread's results to the car records, once a frame (MAT3 mdrive.c,
 * update_game_data() and _UpdateTireAppearance()).
 *
 * The cars are simulated on the model thread in their model records (tModelData); everything else (drawing, the
 * sounds, the HUD, the cameras) reads the car records (tCarData). Each frame the game thread stops the model thread,
 * dead-reckons every car to the frame's time (game_reckon_all) and copies each car's position, orientation,
 * velocity and tire positions across, works out the body's lean from the tire forces, and the effects the car
 * shows (the appearance bits: brake lights, sparks, skid marks, smoke), then lets the model thread go on. */
#include "game/game.h"

/* The car's physics descriptor (MAT3 _tCar, game data in the N64's layout), the field used here. */
typedef struct _tCar {
    /* 0x00 */ u8 pad0[0x70];
    /* 0x70 */ f32 tireR[4][3];
} _tCar;

/* The car's tune record (MAT3 anon_diffbl_weight_scale_2c, gTune), the field used here. */
typedef struct tTune {
    /* 0x00 */ u8 pad0[0x20];
    /* 0x20 */ f32 skidk;           /* the skids' force scale */
} tTune;

/* A tire's state in the model record (MAT3 anon_tradius_springK_5c), the fields used here. */
typedef struct tTireState { u8 pad0[0x50]; f32 sideforce; f32 traction; u8 pad58[4]; } tTireState; /* 0x5C */

enum n64p_tTune { n64p_tTune_ };
enum n64p__tCar { n64p__tCar_ };
/* The model record (tModelData, docs/port/CARS.md 4), the fields used here. */
typedef struct tModelData {
    /* 0x000 */ enum n64p__tCar car;        /* its physics descriptor: a 32-bit window address */
    /* 0x004 */ enum n64p_tTune tune;       /* its tune record: likewise */
    /* 0x008 */ u8 body_type;              /* the car, 0-12 */
    /* 0x009 */ u8 pad009[0x40 - 0x9];
    /* 0x040 */ f32 v[3];                  /* the velocity, car frame, feet a second */
    /* 0x04C */ u8 pad04C[0x64 - 0x4C];
    /* 0x064 */ f32 tireForce[4][3];       /* the force at each tire, car frame */
    /* 0x094 */ u8 pad094[0xC4 - 0x94];
    /* 0x0C4 */ f32 bodyForce[4][3];       /* the force at each body corner (scraping) */
    /* 0x0F4 */ u8 pad0F4[0x2E0 - 0xF4];
    /* 0x2E0 */ f32 stuntW[3];             /* the spin since the last frame, for the stunt code */
    /* 0x2EC */ u8 pad2EC[0x3D4 - 0x2EC];
    /* 0x3D4 */ f32 brake;                 /* 0..1 */
    /* 0x3D8 */ u8 pad3D8[0x3F0 - 0x3D8];
    /* 0x3F0 */ f32 magvel;                /* the speed, feet a second */
    /* 0x3F4 */ u8 pad3F4[0x430 - 0x3F4];
    /* 0x430 */ tTireState tires[4];
    /* 0x5A0 */ u8 pad5A0[0x61C - 0x5A0];
    /* 0x61C */ u16 sviscode[4];           /* each tire's surface, SURF_VIS_* */
    /* 0x624 */ u8 pad624[0x62C - 0x624];
    /* 0x62C */ u16 sound_flags[4];        /* each tire's road sound flags (0x100: in a tunnel) */
    /* 0x634 */ u8 pad634[0x642 - 0x634];
    /* 0x642 */ s8 thumpflag;              /* frames of an underbody hit */
    /* 0x643 */ u8 pad643[0x710 - 0x643];
    /* 0x710 */ s32 modelIteration;        /* fastin: the model step's count and time */
    /* 0x714 */ f32 modelTime;
    /* 0x718 */ u8 pad718[0x758 - 0x718];
    /* 0x758 */ s16 reckon_rwmph;          /* reckon (the dead-reckoned state): the speed shown, mph x 4 */
    /* 0x75A */ u8 pad75A[0x77C - 0x75A];
    /* 0x77C */ f32 reckon_rwa[3];         /* reckon: acceleration, velocity, position and orientation */
    /* 0x788 */ f32 reckon_rwv[3];
    /* 0x794 */ f32 reckon_rwr[3];
    /* 0x7A0 */ f32 reckon_uvs[3][3];
    /* 0x7C4 */ u8 pad7C4[4];
    /* 0x7C8 */ s16 in_game;
    /* 0x7CA */ u8 pad7CA[0x7D4 - 0x7CA];
    /* 0x7D4 */ u32 appearance;            /* APPEAR_* bits */
    /* 0x7D8 */ u8 pad7D8[0x7DD - 0x7D8];
    /* 0x7DD */ s8 data_valid;
    /* 0x7DE */ u8 pad7DE[0x7EA - 0x7DE];
    /* 0x7EA */ s8 collidable;
    /* 0x7EB */ s8 solid;
    /* 0x7EC */ u8 pad7EC[0x7F8 - 0x7EC];
    /* 0x7F8 */ f32 skidIntensity[4];      /* each skid direction's intensity, 0..1 (also the skid sounds') */
} tModelData; /* 0x808 */
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_tTune: (tTune *)PC_N64P(x), enum n64p__tCar: (_tCar *)PC_N64P(x), default: (x))

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields set here. */
typedef struct tCarData {
    /* 0x000 */ s32 modelIteration;
    /* 0x004 */ f32 modelTime;
    /* 0x008 */ f32 dr_pos[3];
    /* 0x014 */ f32 dr_vel[3];
    /* 0x020 */ f32 dr_acc[3];
    /* 0x02C */ f32 dr_uvs[3][3];
    /* 0x050 */ f32 dr_car_uvs[3][3];
    /* 0x074 */ f32 dr_tirepos[4][3];
    /* 0x0A4 */ f32 v[3];
    /* 0x0B0 */ f32 tireR[4][3];
    /* 0x0E0 */ f32 pitch;
    /* 0x0E4 */ f32 roll;
    /* 0x0E8 */ u32 appearance;
    /* 0x0EC */ s8 data_valid;
    /* 0x0ED */ u8 pad0ED[0xF8 - 0xED];
    /* 0x0F8 */ s16 rwmph;
    /* 0x0FA */ u8 pad0FA[0x110 - 0xFA]; struct pc_visual visuals[21];
    /* 0x308 */ s8 collidable;
    /* 0x309 */ s8 solid;
    /* 0x30C */ f32 collide_time;
    /* 0x310 */ s8 collide_count;
    /* 0x311 */ s8 collide_state;
    /* 0x312 */ u8 pad312[0x344 - 0x312];
    /* 0x344 */ u16 sound_flags[4];
    /* 0x34C */ u8 pad34C[0x359 - 0x34C];
    /* 0x359 */ s8 dead;
    /* 0x35A */ s8 in_tunnel;
    /* 0x35B */ u8 pad35B[0x368 - 0x35B];
    /* 0x368 */ f32 stuntW[3];
    /* 0x374 */ u8 pad374[0x380 - 0x374]; void *input; u8 pad374_384[0x3B8 - 0x384];
} tCarData; /* 0x3B8 */

/* A suspension's lean (MAT3 _tSusp, carSuspensions): from each tire force total the lean starts at (min), the
 * force over which it grows (range) and the most it leans (angle), forwards, backwards and sideways. */
typedef struct _tSusp {
    f32 fwdforcemin;
    f32 fwdforcerange;
    f32 fwdangle;
    f32 bckforcemin;
    f32 bckforcerange;
    f32 bckangle;
    f32 sideforcemin;
    f32 sideforcerange;
    f32 sideangle;
} _tSusp;

VIEW(u8[], model_thread);
#define model_thread model_thread_view

void osStopThread(void *t);
void osStartThread(void *t);

/* The appearance bits of each skid direction and tire: sSmoke and sSkid by skid direction (0 the -x pair sliding
 * sideways, 1 the +x pair, 2 the front wheels spinning or locked, 3 the rear), sSmoke2 by tire (the dust behind a
 * tire on dirt); sSkidData each direction's force table and its thresholds for skid marks and smoke. */
VIEW(u32[4], sSmoke);
#define sSmoke sSmoke_view
VIEW(u32[4], sSmoke2);
#define sSmoke2 sSmoke2_view
VIEW(u32[4], sSkid);
#define sSkid sSkid_view
VIEW(tSkidData[4], sSkidData);
#define sSkidData sSkidData_view

f32 fabsf(f32);

/* Car `droneIndex`'s skid marks and tire smoke (MAT3 _UpdateTireAppearance, a static there).
 *
 * A tire on dirt raises its dust (sSmoke2), any other surface clears it and the skid of the direction with its
 * number (sSkid[tire]: the original code mixes the two indexes). In the air (either middle tire, 1 or 2), in water
 * or after the finish there are no skids or smoke. Else each skid direction's force (the larger side force of its
 * pair, or the larger traction of the front pair, or the sum of the rear's) against its force table, scaled by the
 * tune's skidk (3 in a battle): from the table's first value on, the intensity is the force's place in the table,
 * 0..1 in quarters interpolated between its five values (capped at the last), and above the direction's thresholds
 * it shows skid marks and smoke; below the first value an intensity left over fades by 0.125 a frame (PC_FSCALE:
 * a frame of the game's 30 a second), showing them while above the thresholds. */
void UpdateTireAppearance(s16 droneIndex)
{
    s32 index;
    s32 side;
    s32 forceLow;
    s32 force;
    tModelData *m = PC_MODEL(droneIndex);
    tSkidData *skidData;
    s32 airborne = m->sviscode[1] == SURF_VIS_AIRBORNE || m->sviscode[2] == SURF_VIS_AIRBORNE;
    s32 wet = m->sviscode[1] == SURF_VIS_WATER || m->sviscode[2] == SURF_VIS_WATER;

    for (side = 0; side < 4; side++) {
        if (m->sviscode[side] == SURF_VIS_DIRT) {
            m->appearance |= sSmoke2[side];
        } else {
            m->appearance &= ~(sSkid[side] | sSmoke2[side]);
        }
    }
    if (airborne || wet || gCoastFlag) {
        for (side = 0; side < 4; side++) {
            m->skidIntensity[side] = 0;
            m->appearance &= ~(sSkid[side] | sSmoke[side]);
        }
    } else {
        for (side = 0; side < 4; side++) {
            skidData = &sSkidData[side];
            switch (side) {
            case 0:
                force = fabsf(m->tires[1].sideforce) > fabsf(m->tires[3].sideforce) ?
                    fabsf(m->tires[1].sideforce) : fabsf(m->tires[3].sideforce);
                break;
            case 1:
                force = fabsf(m->tires[0].sideforce) > fabsf(m->tires[2].sideforce) ?
                    fabsf(m->tires[0].sideforce) : fabsf(m->tires[2].sideforce);
                break;
            case 2:
                force = fabsf(m->tires[0].traction) > fabsf(m->tires[1].traction) ?
                    fabsf(m->tires[0].traction) : fabsf(m->tires[1].traction);
                break;
            case 3:
                force = fabsf(m->tires[2].traction) + fabsf(m->tires[3].traction);
                break;
            }
            {
                f32 scale = gRace == 6 ? 3.0f : N64P(m->tune)->skidk;   /* kBattle */
                f32 intensity;
                s32 maxForce;

                if (force >= scale * skidData->forceTable[0]) {
                    index = 0;
                    maxForce = skidData->forceTable[4];
                    if (force > maxForce) {
                        force = maxForce;
                    }
                    for (index = 0; index < 4; index++) {
                        if (force <= skidData->forceTable[index + 1]) {
                            break;
                        }
                    }
                    forceLow = skidData->forceTable[index];
                    if (maxForce) {}
                    intensity = 0.25f * (index +
                        (f32)(force - forceLow) / (skidData->forceTable[index + 1] - forceLow));
                    m->skidIntensity[side] = intensity;
                    if (intensity > skidData->skidThreshold) {
                        m->appearance |= sSkid[side];
                    }
                    if (m->skidIntensity[side] > skidData->smokeThreshold) {
                        m->appearance |= sSmoke[side];
                    }
                } else if (m->skidIntensity[side]) {
                    if (m->skidIntensity[side] > skidData->skidThreshold) {
                        m->appearance |= sSkid[side];
                    }
                    if (m->skidIntensity[side] > skidData->smokeThreshold) {
                        m->appearance |= sSmoke[side];
                    }
                    m->skidIntensity[side] -= PC_FSCALE(0.125f);
                    if (m->skidIntensity[side] <= 0) {
                        m->skidIntensity[side] = 0;
                        m->appearance &= ~(sSkid[side] | sSmoke[side]);
                    }
                }
            }
        }
    }
}

/* Copy every car in the race that isn't dead from its model record to its car record (see the file comment), with
 * the model thread stopped meanwhile. The body leans with the summed tire force: pitch from the forward force
 * past the suspension's forward or backward minimum, roll from the side force, each filtered (0.6 and 0.7 of the
 * last value for a frame of the game's 30 a second, PC_FILTER) and applied to the drawn orientation (dr_car_uvs).
 * For a car this machine simulates or a human's, it also sets the effects: the tunnel flag from the sound flags
 * most tires share, brake lights while braking, sparks at a scraping body corner above 20 feet a second, sparks
 * under the car after more than two frames of an underbody hit, the blink when solid changes, the skids and smoke
 * (UpdateTireAppearance), and copies the velocity and the speed shown. The body-hit bit lasts one frame. A car
 * that is not solid gets its collide time and blink started; a solid one's collide time is cleared. */
void update_game_data(void)
{
    s16 i;
    s16 j;
    s16 k;
    s16 l;
    s16 high_index;
    s16 same_count[4];
    tModelData *m;
    tCarData *gc;
    f32 force[3];
    f32 angle;
    const _tSusp *susp;
    u16 tsf;

    osStopThread(model_thread);
    game_reckon_all(gModelTime);
    for (i = 0; i < PC_RACE_CARS; i++) {
        m = PC_MODEL(i);
        gc = PC_CAR(i);
        if (m->in_game && gc->dead < CAR_DEAD) {
            gc->modelIteration = m->modelIteration;
            gc->modelTime = m->modelTime;
            gc->dr_pos[0] = m->reckon_rwr[0];
            gc->dr_pos[1] = m->reckon_rwr[1];
            gc->dr_pos[2] = m->reckon_rwr[2];
            gc->stuntW[0] = m->stuntW[0];
            gc->stuntW[1] = m->stuntW[1];
            gc->stuntW[2] = m->stuntW[2];
            m->stuntW[0] = 0.0f;
            m->stuntW[1] = 0.0f;
            m->stuntW[2] = 0.0f;
            CopyUV(m->reckon_uvs, gc->dr_uvs);
            CopyUV(m->reckon_uvs, gc->dr_car_uvs);
            susp = carSuspensions[gCarSuspensions[m->body_type]];
            force[0] = m->tireForce[0][0] + m->tireForce[1][0];
            force[1] = m->tireForce[0][1] + m->tireForce[1][1];
            force[2] = m->tireForce[0][2] + m->tireForce[1][2];
            force[0] = m->tireForce[2][0] + force[0];
            force[1] = m->tireForce[2][1] + force[1];
            force[2] = m->tireForce[2][2] + force[2];
            force[0] = m->tireForce[3][0] + force[0];
            force[1] = m->tireForce[3][1] + force[1];
            force[2] = m->tireForce[3][2] + force[2];
            if ((angle = force[2] - susp->fwdforcemin) > 0) {
                angle *= susp->fwdangle / susp->fwdforcerange;
                if (angle > susp->fwdangle) angle = susp->fwdangle;
            } else {
                if ((angle = force[2] - susp->bckforcemin) < 0) {
                    angle *= -susp->bckangle / susp->bckforcerange;
                    if (angle < susp->bckangle) angle = susp->bckangle;
                } else angle = 0;
            }
            gc->pitch = PC_FILTER(0.6f) * gc->pitch + PC_FILTER_REST(0.6f, 0.4f) * angle;
            PitchUV(gc->pitch, gc->dr_car_uvs);
            if ((angle = force[0] - susp->sideforcemin) > 0) {
                angle *= -susp->sideangle / susp->sideforcerange;
                if (angle > susp->sideangle) angle = susp->sideangle;
            } else {
                if ((angle = force[0] + susp->sideforcemin) < 0) {
                    angle *= -susp->sideangle / susp->sideforcerange;
                    if (angle < -susp->sideangle) angle = -susp->sideangle;
                } else angle = 0;
            }
            gc->roll = PC_FILTER(0.7f) * gc->roll + PC_FILTER_REST(0.7f, 0.3f) * angle;
            RollUV(gc->roll, gc->dr_car_uvs);
            for (j = 0; j < 4; j++) {
                gc->tireR[j][0] = N64P(m->car)->tireR[j][0];
                gc->tireR[j][1] = N64P(m->car)->tireR[j][1];
                gc->tireR[j][2] = N64P(m->car)->tireR[j][2];
                WorldVector(N64P(m->car)->tireR[j], gc->dr_tirepos[j], gc->dr_uvs);
                gc->dr_tirepos[j][0] = gc->dr_tirepos[j][0] + gc->dr_pos[0];
                gc->dr_tirepos[j][1] = gc->dr_tirepos[j][1] + gc->dr_pos[1];
                gc->dr_tirepos[j][2] = gc->dr_tirepos[j][2] + gc->dr_pos[2];
            }
            gc->dr_vel[0] = m->reckon_rwv[0];
            gc->dr_vel[1] = m->reckon_rwv[1];
            gc->dr_vel[2] = m->reckon_rwv[2];
            gc->dr_acc[0] = m->reckon_rwa[0];
            gc->dr_acc[1] = m->reckon_rwa[1];
            gc->dr_acc[2] = m->reckon_rwa[2];
            if (PC_LINK_OWNER(i) == 0 || PC_LINK_OWNER(i) == 6) {   /* LINK_OWNER_DRONE, LINK_OWNER_HUMAN */
                high_index = 0;
                for (k = 0; k < 4; k++) {
                    gc->sound_flags[k] = m->sound_flags[k];
                    same_count[k] = 0;
                    if (m->sviscode[k] != SURF_VIS_AIRBORNE || m->sound_flags[k]) {
                        for (l = k + 1; l < 4; l++) {
                            if (m->sound_flags[k] == m->sound_flags[l]) same_count[k]++;
                        }
                    }
                    if (same_count[k] >= same_count[high_index]) {
                        high_index = k;
                        tsf = m->sound_flags[k];
                    }
                }
                gc->in_tunnel = (tsf & 0x100) >> 8;
                if (m->brake) m->appearance |= APPEAR_BRAKE;
                else m->appearance &= ~APPEAR_BRAKE;
                if (m->magvel > 20.0f && m->bodyForce[0][0]) m->appearance |= APPEAR_SPARK_C0;
                else m->appearance &= ~APPEAR_SPARK_C0;
                if (m->magvel > 20.0f && m->bodyForce[2][0]) m->appearance |= APPEAR_SPARK_C2;
                else m->appearance &= ~APPEAR_SPARK_C2;
                if (m->magvel > 20.0f && m->bodyForce[1][0]) m->appearance |= APPEAR_SPARK_C1;
                else m->appearance &= ~APPEAR_SPARK_C1;
                if (m->magvel > 20.0f && m->bodyForce[3][0]) m->appearance |= APPEAR_SPARK_C3;
                else m->appearance &= ~APPEAR_SPARK_C3;
                if (m->thumpflag > 2) m->appearance |= APPEAR_THUMP;
                else if (!m->thumpflag) m->appearance &= ~APPEAR_THUMP;
                /* the frame solid changes: hidden for the frame it becomes solid, shown when it stops (as MAT3);
                 * BlinkResurrectingCars blinks it in between */
                if (gc->solid && !m->solid) m->appearance &= ~APPEAR_INVISIBLE;
                else if (!gc->solid && m->solid) m->appearance |= APPEAR_INVISIBLE;
                UpdateTireAppearance(i);
                gc->v[0] = m->v[0];
                gc->v[1] = m->v[1];
                gc->v[2] = m->v[2];
                gc->rwmph = m->reckon_rwmph;
            }
            gc->appearance = m->appearance;
            m->appearance &= ~APPEAR_BODY_HIT;
            gc->collidable = m->collidable;
            gc->solid = m->solid;
            if (!gc->solid && gc->collide_time == 0) {
                gc->collide_time = m->modelTime;
                gc->collide_state = -1;
                gc->collide_count = -1;
            }
            if (gc->solid == 1) gc->collide_time = 0;
            gc->data_valid = m->data_valid;
        }
    }
    osStartThread(model_thread);
}
