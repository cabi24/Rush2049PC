/* forces2.c: the total force on a car (MAT3 drivsym.c, forces2()).
 *
 * A stage of the physics step (regular, sym), after forces1: the sum of the tire forces, gravity, the drag, the
 * body's contacts and the centre force, in the car's frame. */
#include "game/game.h"

typedef f32 Vec3[3];
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its differential blend. */
typedef struct tTune {f32 diffbl; /* +0x00 */} tTune;
#define vecadd(a,b,r) {r[0]=a[0]+b[0];r[1]=a[1]+b[1];r[2]=a[2]+b[2];}

/* Model m's total force f, in the car's frame: the four tires, gravity (mass times gGravity, down; with the wing out
 * and the car level or falling, times the wing's lift factor, gWingLift), the drag, the body corners' forces (whose
 * size goes to bodyForces) and the centre force. With nothrusttorque, a pushing rear tire's forward force is first
 * blended with the pair's mean by the tune's diffbl (none at all for a tire whose partner is in the air). */
void forces2(tModelData *m)
{
    f32 mean,body_force[3],blend;
    s32 row;
    if(m->nothrusttorque) {
        mean=(m->tireForce[2][2]+m->tireForce[3][2])*(f32).5;
        if(mean>0) {
            if(m->roadcode[3]==ROADCODE_AIRBORNE) m->tireForce[2][2]=0;
            else {
                blend=m->tune->diffbl;
                m->tireForce[2][2]=m->tireForce[2][2]*blend+mean*((f32)1.0-blend);
            }
            if(m->roadcode[2]==ROADCODE_AIRBORNE) m->tireForce[3][2]=0;
            else {
                blend=m->tune->diffbl;
                m->tireForce[3][2]=m->tireForce[3][2]*blend+mean*((f32)1.0-blend);
            }
        }
    }
    vecadd(m->tireForce[0],m->tireForce[1],m->f);
    vecadd(m->tireForce[2],m->f,m->f);
    vecadd(m->tireForce[3],m->f,m->f);
    m->grw[1]=m->mass*gGravity*(f32)-1.0;
    if(m->wing && m->rwr[1]<=m->rwr2[1]) {
        if(m->drone_type==DRONE_TYPE_HUMAN) row=m->slot+1;   /* a human: its own wings */
        else row=0;
        m->grw[1]*=gWingLift[gCarWings[row][m->body_type]];
    }
    BodyVector(m->grw,m->g,m->uvs);
    vecadd(m->g,m->f,m->f);
    vecadd(m->d,m->f,m->f);
    vecadd(m->bodyForce[0],m->bodyForce[1],body_force);
    vecadd(m->bodyForce[2],body_force,body_force);
    vecadd(m->bodyForce[3],body_force,body_force);
    vecadd(body_force,m->f,m->f);
    m->bodyForces=vector_magnitude(body_force);
    vecadd(m->centerForce,m->f,m->f);
}
