/* drivetrain.c: the engine, gearbox and differential for one step (MAT3 drivetra.c, drivetrain()).
 *
 * A stage of the physics step (forces1 calls it): the engine's torque through the gears and the differential to
 * the rear (driven) wheels. */
#include "game/game.h"

typedef f32 Vec3[3];
/* A tire in the model record (MAT3 anon_tradius_springK_5c, tModelData.tires[i]). */
typedef struct Tire {
    f32 tradius,springK,rubdamp,cstiff,cfmax,invmi,zforce,afmax,k1,k2,k3;
    u8 pad2C[0x44 - 0x2C];      /* l2, l3, m1 .. m4 */
    f32 patchy,angvel,sliptorque,sideforce,traction;   /* angvel: the wheel's spin, radians a second */
    s8 slipflag;
    u8 align[3];
} Tire;
/* The model record (tModelData), the fields drivetrain reads and writes. */
typedef struct tModelData {
    u8 pad0[0xA]; s8 autotrans; u8 padB[0x64 - 0xB];
    Vec3 tireForce[4];          /* +0x064 each tire's force; [i][1] its load (the road's push up) */
    u8 pad94[0x3B4 - 0x94]; f32 torque[4];          /* +0x3B4 each wheel's drive torque */
    u8 pad3C4[0x410 - 0x3C4]; f32 dwtorque;         /* +0x410 the torque at the driven wheels (whatslips) */
    u8 pad414[0x418 - 0x414]; f32 efdwinvmi,dwangvel;   /* +0x418 the driveline's inverse inertia; +0x41C the
                                                         * driven wheels' mean speed */
    u8 pad420[0x430 - 0x420]; Tire tires[4];        /* +0x430 */
    u8 pad5A0[0x5A8 - 0x5A0]; s8 magicdiff;         /* +0x5A8 the "magic" (limited-slip) differential */
    u8 tail[0x808 - 0x5A9];
} tModelData;

/* One step of model m's drivetrain: the automatic gearbox (autoshift), the engine's torque (engine) and the gear
 * ratios (transmission); the driven wheels' mean speed; whatslips for the clutch and the torque that reaches the
 * wheels; then the differential. An open one (or with less than 500 of load on the rear) splits the torque evenly;
 * the magic differential gives it all to a wheel whose partner is in the air, else splits it by the wheels' loads.
 * Each rear wheel's inertia becomes twice the driveline's effective inverse inertia. */
void drivetrain(tModelData *m)
{
    f32 rearload;
    if (m->autotrans) autoshift(m);
    engine(m);
    transmission(m);
    m->dwangvel=(m->tires[2].angvel+m->tires[3].angvel)*.5f;
    whatslips(m);
    rearload=m->tireForce[2][1]+m->tireForce[3][1];
    if (!m->magicdiff || rearload<500) {
        m->torque[2]+=m->dwtorque*.5f;
        m->torque[3]+=m->dwtorque*.5f;
    } else if (m->tireForce[2][1]<=0) {
        m->torque[2]=0;
        m->torque[3]+=m->dwtorque;
    } else if (m->tireForce[3][1]<=0) {
        m->torque[3]=0;
        m->torque[2]+=m->dwtorque;
    } else {
        m->torque[2]+=(m->dwtorque*m->tireForce[2][1])/rearload;
        m->torque[3]+=(m->dwtorque*m->tireForce[3][1])/rearload;
    }
    m->tires[2].invmi=m->efdwinvmi*2;
    m->tires[3].invmi=m->efdwinvmi*2;
}
