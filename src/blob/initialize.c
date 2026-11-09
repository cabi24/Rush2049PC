/* initialize.c: a car's simulation state reset and its constants set (MAT3 initiali.c, initialize()).
 *
 * syminit calls it when a car starts or respawns. */
#include "game/game.h"

/* The model record (tModelData), the fields initialize sets itself. */
typedef struct tModelData {
    u8 pad0[0x10]; f32 vectors[6][3];   /* +0x010 f, m, a, aa, v, w: the forces, accelerations and velocities */
    u8 pad58[0x2EC - 0x58]; f32 uvs[9]; /* +0x2EC the orientation */
    u8 pad310[0x3B4 - 0x310]; f32 torque[4];   /* +0x3B4 each wheel's drive torque */
    u8 pad3C4[0x3D8 - 0x3C4]; f32 brakeroll;   /* +0x3D8 */
    u8 pad3DC[0x3EC - 0x3DC]; f32 pitchangle;  /* +0x3EC */
    u8 pad3F0[0x408 - 0x3F0]; f32 engangvel;   /* +0x408 the engine's speed */
    u8 pad40C[0x634 - 0x40C]; f32 dt;          /* +0x634 the model step, seconds */
    u8 pad638[0x640 - 0x638]; u8 crashflag, topscrape, thumpflag;   /* +0x640 */
    u8 pad643[0x64C - 0x643]; f32 thumpthreshold, bodyForces;       /* +0x64C the body force a thump takes; +0x650 */
    u8 pad654[0x658 - 0x654]; f32 shifttime; s16 bogState;          /* +0x658, +0x65C */
    u8 pad65E[0x7C6 - 0x65E]; s16 slot;        /* +0x7C6 */
} tModelData;

/* Reset model m: no pitch, no drive torque on the front wheels and 50 on the rear, the model step (model_step_dt,
 * MAT3 0.0025 s: 400 steps a second), brakeroll 3, no shift pending, the thump threshold (initial_thump_threshold,
 * MAT3 7500), every force, acceleration and velocity 0, the engine stopped, the orientation the identity (with the
 * super speed cheat its diagonal super_speed_unit_diagonal, MAT3 1.00025, as in make_super_speed). Then the car's
 * constants (carsetup), the road under it (init_road), its collision box (init_collision), not crashed; its car
 * record's battle state back to full health (800), no shot, no fade, no shield, no aim, and its crash state
 * CRASH_NONE; the body free, bogState 3. */
void initialize(tModelData *m) {
    m->pitchangle=0.0f;
    m->torque[1]=0.0f; m->torque[0]=0.0f;
    m->torque[2]=50.0f; m->torque[3]=50.0f;
    m->dt=model_step_dt;
    m->brakeroll=3.0f; m->shifttime=0.0f;
    m->thumpthreshold=initial_thump_threshold;
    m->thumpflag=0;
    m->vectors[0][2]=m->vectors[0][1]=m->vectors[0][0]=0.0f;
    m->vectors[1][2]=m->vectors[1][1]=m->vectors[1][0]=0.0f;
    m->vectors[2][2]=m->vectors[2][1]=m->vectors[2][0]=0.0f;
    m->vectors[3][2]=m->vectors[3][1]=m->vectors[3][0]=0.0f;
    m->vectors[5][2]=m->vectors[5][1]=m->vectors[5][0]=0.0f;
    m->vectors[4][2]=m->vectors[4][1]=m->vectors[4][0]=0.0f;
    m->engangvel=0.0f;
    CopyUV(unitmatrix,m->uvs);
    if(gSuperSpeed) {
        f32 diagonal=super_speed_unit_diagonal;
        m->uvs[0]=diagonal; m->uvs[4]=diagonal; m->uvs[8]=diagonal;
    }
    carsetup(m);
    init_road(m);
    init_collision(m);
    m->crashflag=0;
    /* the car record's battle block (MAT3 tCarData.battle) and crash_state, by the port's accessors */
    PC_CAR_HEALTH(m->slot)=800;
    PC_CAR_B3A0(m->slot)=0;             /* shoot */
    PC_CAR_B3A2(m->slot)=0;             /* fadeState */
    PC_CAR_BATTLE_FLAGS(m->slot)=0;     /* defense.active */
    PC_CAR_F3B0(m->slot)=0.0f;          /* pitch */
    PC_CAR_F3B4(m->slot)=0.0f;          /* yaw */
    PC_CAR_B358(m->slot)=CRASH_NONE;    /* crash_state */
    m->bodyForces=0.0f;
    m->bogState=3;
}
