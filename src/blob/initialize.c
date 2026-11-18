/* initialize.c: a car's simulation state reset and its constants set (MAT3 initiali.c, initialize()).
 *
 * syminit calls it when a car starts or respawns. */
#include "game/game.h"

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
    m->f[2]=m->f[1]=m->f[0]=0.0f;
    m->m[2]=m->m[1]=m->m[0]=0.0f;
    m->a[2]=m->a[1]=m->a[0]=0.0f;
    m->aa[2]=m->aa[1]=m->aa[0]=0.0f;
    m->w[2]=m->w[1]=m->w[0]=0.0f;
    m->v[2]=m->v[1]=m->v[0]=0.0f;
    m->engangvel=0.0f;
    CopyUV(unitmatrix,m->uvs);
    if(gSuperSpeed) {
        f32 diagonal=super_speed_unit_diagonal;
        m->uvs[0][0]=diagonal; m->uvs[1][1]=diagonal; m->uvs[2][2]=diagonal;
    }
    carsetup(m);
    init_road(m);
    init_collision(m);
    m->crashflag=0;
    /* the car record's battle block (MAT3 tCarData.battle) and crash_state, by the port's accessors */
    PC_CAR_HEALTH(m->slot)=800;
    PC_CAR_SHOOT(m->slot)=0;             /* shoot */
    PC_CAR_FADE_STATE(m->slot)=0;             /* fadeState */
    PC_CAR_DEFENSE_ACTIVE(m->slot)=0;     /* defense.active */
    PC_CAR_BATTLE_PITCH(m->slot)=0.0f;          /* pitch */
    PC_CAR_BATTLE_YAW(m->slot)=0.0f;          /* yaw */
    PC_CAR_CRASH_STATE(m->slot)=CRASH_NONE;    /* crash_state */
    m->bodyForces=0.0f;
    m->bogState=3;
}
