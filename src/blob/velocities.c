/* velocities.c: a car's velocities from its accelerations (MAT3 drivsym.c, velocities()).
 *
 * A stage of the physics step (regular, sym), after accelerations: one Euler step of the velocity and the angular
 * velocity, with the limits and dampings that keep the car steady. */
#include "game/game.h"

extern f32 fabsf ( f32 );

/* Integrate model m's velocities over the step dt:
 * - v += a dt; with less than 0.1 of throttle a sideways or forward speed under 0.025 ft/s stops;
 * - the speed (unless the car is CAR_DEAD), at most 300 ft/s: past that the velocity is scaled back to it and the
 *   spin isn't updated this step;
 * - w += aa dt, less 0.025 of w while the body touches something; in the air (all four tires) or crashed, the spin
 *   loses its square times gWingAngVDrag (gWingAngVDrag_1 crashed) of itself: an air drag on the rotation;
 * - below 3 ft/s, braking or upside down, the car stops sliding; nearly upside down, a slow spin stops too. */
void velocities(tModelData *m)
{
    f32 temp[3],velfact,speed;f32 coef;
    temp[0]=m->a[0]*m->dt;
    temp[1]=m->a[1]*m->dt;
    temp[2]=m->a[2]*m->dt;
    m->v[0]=m->v[0]+temp[0];
    m->v[1]=m->v[1]+temp[1];
    m->v[2]=m->v[2]+temp[2];
    if(m->throttle<0.1f) {
        if(fabsf(m->v[0])<0.025f)m->v[0]=0.0f;
        if(fabsf(m->v[2])<0.025f)m->v[2]=0.0f;
    }
    if(PC_CAR_DEAD(m->slot)!=CAR_DEAD)m->magvel=vector_magnitude(m->v);
    if(m->magvel>300.0f) {
        velfact=300.0f/m->magvel;
        m->v[0]*=velfact;
        m->v[1]*=velfact;
        m->v[2]*=velfact;
        return;
    }
    temp[0]=m->aa[0]*m->dt;
    temp[1]=m->aa[1]*m->dt;
    temp[2]=m->aa[2]*m->dt;
    if(m->bodyForces!=0.0f) {
        temp[0]-=m->w[0]*.025f;
        temp[1]-=m->w[1]*.025f;
        temp[2]-=m->w[2]*.025f;
    }
    m->w[0]=m->w[0]+temp[0];
    m->w[1]=m->w[1]+temp[1];
    m->w[2]=m->w[2]+temp[2];
    if((m->airdist[0]>0 && m->airdist[1]>0 && m->airdist[2]>0 && m->airdist[3]>0) || m->crashflag) {
        speed=vector_magnitude(m->w);
        speed=speed*speed;
        if(m->crashflag)coef=gWingAngVDrag_1;else coef=gWingAngVDrag;
        speed=speed*coef;
        m->w[0]-=m->w[0]*speed;
        m->w[1]-=m->w[1]*speed;
        m->w[2]-=m->w[2]*speed;
    }
    if(m->magvel<3.0f && (m->brake>0.5f || m->uvs[1][1]<0.0f)) {
        m->v[0]=0.0f;
        m->v[2]=0.0f;
    }
    if(m->magvel<3.0f && m->uvs[1][1]< -0.75f) {
        if(vector_magnitude(m->w)<0.5f) {
            m->w[0]=0.0f;
            m->w[1]=0.0f;
            m->w[2]=0.0f;
        }
    }
}
