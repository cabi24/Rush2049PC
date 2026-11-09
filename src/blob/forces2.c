/* forces2.c: the total force on a car (MAT3 drivsym.c, forces2()).
 *
 * A stage of the physics step (regular, sym), after forces1: the sum of the tire forces, gravity, the drag, the
 * body's contacts and the centre force, in the car's frame. */
#include "game/game.h"

typedef f32 Vec3[3];
/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its differential blend. */
typedef struct tTune {f32 diffbl; /* +0x00 */} tTune;
/* The model record (tModelData), the fields forces2 reads and writes. */
typedef struct tModelData {
    _tCar *car;                 /* +0x000 the physics descriptor */
    tTune *tune;                /* +0x004 the tune record */
    u8 body_type;               /* +0x008 the car, 0..12 */
    u8 pad9[0x10 - 0x9];
    Vec3 f,m,a,aa,v,w,d;        /* +0x010 the total force, moment, accelerations, velocities, the drag (forces1) */
    Vec3 tireForce[4];          /* +0x064 each tire's force */
    u8 pad94[0xC4 - 0x94];
    Vec3 bodyForce[4];          /* +0x0C4 the force at each body corner (the body touching the road, CollideCarBody) */
    u8 padF4[0x124 - 0xF4];
    Vec3 centerForce;           /* +0x124 the force at the centre (MovingSurfaceForce, collisions) */
    u8 pad130[0x208 - 0x130];
    Vec3 g;                     /* +0x208 gravity in the car's frame */
    u8 pad214[0x22C - 0x214];
    Vec3 rwr;                   /* +0x22C the position, world frame */
    Vec3 rwr2;                  /* +0x238 the position a step ago */
    u8 pad244[0x2D4 - 0x244];
    Vec3 grw;                   /* +0x2D4 gravity in the world's frame */
    u8 pad2E0[0x2EC - 0x2E0];
    f32 uvs[9];                 /* +0x2EC the orientation */
    u8 pad310[0x3A0 - 0x310];
    streetyp *lasttp[4];
    u8 pad3B0[0x3F8 - 0x3B0];
    s16 wing;                   /* +0x3F8 the wing deployed */
    u8 pad3FA[0x40C - 0x3FA];
    const s16 *torquecurve;
    u8 pad410[0x5A9 - 0x410];
    s8 nothrusttorque;          /* +0x5A9 average the rear tires' forward force */
    u8 pad5AA[0x5C0 - 0x5AA];
    f32 mass;                   /* +0x5C0 */
    u8 pad5C4[0x60C - 0x5C4];
    s32 roadcode[4];            /* +0x60C each tire's road code */
    u8 pad61C[0x648 - 0x61C];
    surftyp *movingSurface;
    u8 pad64C[0x650 - 0x64C];
    f32 bodyForces;             /* +0x650 the size of the body's contact force (velocities damps the spin by it) */
    u8 pad654[0x7C6 - 0x654];
    s16 slot;                   /* +0x7C6 */
    u8 pad7C8[0x7CC - 0x7C8];
    s8 drone_type;              /* +0x7CC DRONE_TYPE_* */
    u8 tail[0x808 - 0x7CD];
} tModelData;
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
