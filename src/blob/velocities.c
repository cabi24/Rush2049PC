/* velocities.c: a car's velocities from its accelerations (MAT3 drivsym.c, velocities()).
 *
 * A stage of the physics step (regular, sym), after accelerations: one Euler step of the velocity and the angular
 * velocity, with the limits and dampings that keep the car steady. */
#include "game/game.h"

/* The model record (tModelData), the fields velocities reads and writes. */
typedef struct tModelData {
    _tCar *car; Tune44 *tune;                 /* +0x000 the physics descriptor and its tune */
    u8 pad8[0x20];f32 a[3],aa[3],v[3],w[3];   /* +0x028 the accelerations and velocities, car frame, feet and radians */
    u8 pad58[0x2FC - 0x58];f32 up;            /* +0x2FC uvs[1][1]: the car's up axis, vertically (1 upright, -1 upside down) */
    u8 pad300[0x3A0 - 0x300]; streetyp *lasttp[4];   /* +0x3A0 each tire's last road node */
    u8 pad3B0[0x3D0 - 0x3B0];f32 throttle,brake;   /* +0x3D0 */
    u8 pad3D8[0x3F0 - 0x3D8];f32 magvel;      /* +0x3F0 the speed, feet a second */
    u8 pad3F4[0x40C - 0x3F4]; const s16 *torquecurve;   /* +0x40C the torque curve */
    u8 pad410[0x5EC - 0x410];f32 airdist[4];  /* +0x5EC each tire's height above the road */
    u8 pad5FC[0x634 - 0x5FC];f32 dt;          /* +0x634 the step, seconds */
    u8 pad638[0x640 - 0x638];s8 crashflag;    /* +0x640 */
    u8 pad641[0x648 - 0x641]; surftyp *movingSurface;   /* +0x648 the moving surface under the car */
    u8 pad64C[0x650 - 0x64C];f32 bodyForces;  /* +0x650 the size of the body's contact force */
    u8 pad654[0x7C6 - 0x654];s16 slot;        /* +0x7C6 */
} tModelData;

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
    if(m->magvel<3.0f && (m->brake>0.5f || m->up<0.0f)) {
        m->v[0]=0.0f;
        m->v[2]=0.0f;
    }
    if(m->magvel<3.0f && m->up< -0.75f) {
        if(vector_magnitude(m->w)<0.5f) {
            m->w[0]=0.0f;
            m->w[1]=0.0f;
            m->w[2]=0.0f;
        }
    }
}
