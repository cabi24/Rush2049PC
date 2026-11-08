/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
/* Direct ancestor: rushtherock/game/drivetra.c:drivetrain.
 * Native coordinates reverse the vertical load sign. Tire92 is the compact
 * record independently established by the accepted frictioncircle source. */
typedef float f32;
typedef signed char s8;
typedef unsigned char u8;
typedef f32 Vec3[3];
typedef struct Tire92 {
    f32 tradius,springK,rubdamp,unk12,Cfmax,invmi,unk24,Afmax,k1,k2,k3;
    u8 pad44[24];
    f32 patchy,angvel,sliptorque,sideforce,traction;
    s8 slipflag;
    u8 align[3];
} Tire92;
typedef struct Model2056 {
    u8 prefix[10]; s8 autotrans; u8 to_force[89];
    Vec3 tireforce[4];
    u8 to_torque[800]; f32 torque[4];
    u8 to_dwtorque[76]; f32 dwtorque;
    u8 to_efdwinvmi[4]; f32 efdwinvmi,dwangvel;
    u8 to_tires[16]; Tire92 tires[4];
    u8 gap1440[8]; s8 magicdif; u8 tail[607];
} Model2056;
extern void autoshift(Model2056 *);
extern void engine(Model2056 *);
extern void transmission(Model2056 *);
extern void whatslips(Model2056 *);
void drivetrain(Model2056 *m)
{
    f32 rearload;
    if (m->autotrans) autoshift(m);
    engine(m);
    transmission(m);
    m->dwangvel=(m->tires[2].angvel+m->tires[3].angvel)*(f32).5;
    whatslips(m);
    rearload=m->tireforce[2][1]+m->tireforce[3][1];
    if (!m->magicdif || rearload<500) {
        m->torque[2]+=m->dwtorque*(f32).5;
        m->torque[3]+=m->dwtorque*(f32).5;
    } else if (m->tireforce[2][1]<=0) {
        m->torque[2]=0;
        m->torque[3]+=m->dwtorque;
    } else if (m->tireforce[3][1]<=0) {
        m->torque[3]=0;
        m->torque[2]+=m->dwtorque;
    } else {
        m->torque[2]+=(m->dwtorque*m->tireforce[2][1])/rearload;
        m->torque[3]+=(m->dwtorque*m->tireforce[3][1])/rearload;
    }
    m->tires[2].invmi=m->efdwinvmi*2;
    m->tires[3].invmi=m->efdwinvmi*2;
}
