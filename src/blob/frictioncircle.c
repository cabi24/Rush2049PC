/* frictioncircle.c: a tire's road forces and its spin (MAT3 tires.c, frictioncircle()).
 *
 * The heart of the tire model, which dotireforce calls for each tire each step: the drive or brake torque against
 * the grip the load allows (the friction circle: forward and side force share cfmax times the load), the wheel's
 * spin, and the side force from the slip angle by the Fiala cubic (tire_constants' coefficients) with a contact
 * patch that deflects like a spring (patchy) at low speed. slipflag records which case applied: 3 in the air,
 * 10/20 wheelspin, 30/40 locking, 4/5 torque past the grip, then 6..9 the side's, +100 the patch spring, +200 the
 * cubic. */
#include "game/game.h"

/* The model record (tModelData), the fields frictioncircle reads. */
typedef struct tModelData {
    _tCar *car; Tune44 *tune;                  /* +0x000 the physics descriptor and its tune */
    char pad8[0x398]; streetyp *lasttp[4];     /* +0x3A0 each tire's last road node */
    f32 steerangle;                            /* +0x3B0 the front wheels' angle, radians */
    char pad3B4[0x3D4 - 0x3B4]; f32 brake;     /* +0x3D4 0..1 */
    char pad3D8[0x3F0 - 0x3D8]; f32 magvel;    /* +0x3F0 the speed, feet a second */
    char pad3F4[0x40C - 0x3F4]; const s16 *torquecurve;   /* +0x40C the torque curve */
    char pad410[0x5C0 - 0x410]; f32 mass;      /* +0x5C0 */
    char pad5C4[0x634 - 0x5C4]; f32 dt;        /* +0x634 the step, seconds */
} tModelData;
/* A tire in the model record (MAT3 anon_tradius_springK_5c, tModelData.tires[i]). */
typedef struct Tire {
    f32 tradius,springK,rubdamp,cstiff,cfmax,invmi,zforce,afmax,k1,k2,k3;
    char pad44[24];
    f32 patchy,angvel,sliptorque,sideforce,traction;
    signed char slipflag;
} Tire;

extern f32 sqrtf(f32);
extern f32 fabsf(f32);

/* Tire `tire`'s forces on model m this step. tirev is the contact patch's velocity in the tire's frame (x sideways,
 * z forward, feet a second), normalforce the tire's load, torque the drive (or brake) torque on the wheel. Out: *trp
 * the forward force (traction) and *sfp the side force; the tire's spin angvel, sliptorque, patchy and slipflag are
 * updated.
 * - No load: no force, the wheel spins up under the torque and the brake slows it (1000 per unit of brake).
 * - The wheel turning faster than the road (wheelspin) or slower (locking up; braking hard above 5 rad/s it follows
 *   the road speed less the steering's share): the full grip, along the patch's sliding direction.
 * - Otherwise the torque gives the forward force (all the grip if it asks for more), and what grip is left for the
 *   side (the circle: cfmax' = sqrt(cfmax^2 - (forward / load)^2)) sets the side force's cubic; past its full-slide
 *   angle the patch slides and the side force is the patch spring's, at most the grip. */
void frictioncircle(tModelData *m, f32 tirev[3], f32 normalforce, f32 torque,
                    Tire *tire, f32 *sfp, f32 *trp)
{
    f32 maxtraction;
    f32 maxtorque,maxf,Cfmax,Afmax,temp,roadangvel;
    f32 ydot,p,k2,k3,l2,l3,patchvel,patchspeed,realtorque;
        f32 alpha;


    realtorque = torque;
    temp = m->mass*tire->tradius;

    tire->slipflag=0;

    torque = temp*torque/(temp + (f32)1.0/tire->invmi);

    if(normalforce <= 0) {
        *trp = *sfp = 0;
        tire->slipflag += 3;
        tire->angvel += realtorque * tire->invmi * m->dt;
        if(m->brake > 0) {
            tire->angvel -= m->brake * 1000.0f * tire->invmi * m->dt;
            if(tire->angvel < 0) tire->angvel = 0;
        }
        return;
    }

    maxtraction = tire->cfmax*normalforce;
    maxtorque = maxtraction*tire->tradius;
    roadangvel = tirev[2]/tire->tradius;
    if(tire->angvel > roadangvel){
        tire->sliptorque = maxtorque;
        tire->angvel += (realtorque - tire->sliptorque)*tire->invmi*m->dt;
        if(tire->angvel <= roadangvel){
            tire->slipflag = 10;
            tire->angvel = roadangvel;
        }
        else{
            patchvel = tirev[2] - tire->angvel*tire->tradius;
            patchspeed = sqrtf(patchvel*patchvel +
                 tirev[0]*tirev[0]);
            if(patchspeed == 0){
                *trp = maxtraction;
                *sfp = 0;
            }
            else{
                *trp = -maxtraction * patchvel / patchspeed;
                *sfp = -maxtraction * tirev[0] / patchspeed;
            }
            tire->slipflag=20;
            return;
        }
    }
    else if(tire->angvel < roadangvel){
        tire->sliptorque = -(f32)maxtorque;
        if(m->brake > frictioncircle_brake_threshold && roadangvel > 5.0f)
            tire->angvel = roadangvel - fabsf(m->steerangle) * (roadangvel - tire->angvel);
        else
            tire->angvel += (realtorque-tire->sliptorque)*tire->invmi*m->dt;

        if(tire->angvel >= roadangvel){
            tire->angvel = roadangvel;
            tire->slipflag = 30;
        }
        else{
            patchvel = tirev[2] - tire->angvel*tire->tradius;
            patchspeed = sqrtf(patchvel*patchvel +
                 tirev[0]*tirev[0]);

            if(patchspeed == 0){
                *trp = -maxtraction;
                *sfp = 0;
            }
            else{
                *trp = -maxtraction * patchvel / patchspeed;
                if(m->magvel > 20.0f) {
                    patchspeed = sqrtf(tirev[2]*tirev[2] + tirev[0]*tirev[0]);
                    *sfp = -maxtraction * tirev[0] / (2.0f + patchspeed);
                }
                else
                    *sfp = -maxtraction * tirev[0] / patchspeed;
            }

            tire->slipflag=40;
            return;
        }
    }

    tire->sliptorque=0;

    if(torque != 0){

        if(torque >= maxtorque){
            *trp = maxtraction;
            *sfp = 0;
            tire->slipflag += 4;
            tire->sliptorque = maxtorque;
            tire->angvel += (realtorque - tire->sliptorque)*
                tire->invmi*m->dt;

            return;
        }

        if(torque <= -maxtorque){
            *trp = -(f32)maxtraction;
            *sfp = 0;
            tire->slipflag += 5;
            tire->sliptorque = -(f32)maxtorque;
            tire->angvel += (realtorque - tire->sliptorque)*
                tire->invmi*m->dt;
            return;
        }

        *trp = torque/tire->tradius;
        temp = *trp/normalforce;

        Cfmax = sqrtf(tire->cfmax * tire->cfmax - temp * temp);

        k2 = tire->k1*tire->k1/(3*Cfmax);
        k3 = tire->k1*tire->k1*tire->k1/(27*Cfmax*Cfmax);
        Afmax = 3*Cfmax/tire->k1;
    }
    else{
        *trp = 0;
        k2 = tire->k2;
        k3 = tire->k3;
        Cfmax = tire->cfmax;
        Afmax = tire->afmax;
    }

    l2 = k2;
    l3 = k3;

    maxf = Cfmax * normalforce;

    alpha = calcalpha(tirev);

    tire->sliptorque=0;

    if(alpha >= 0){
        if(alpha >= Afmax){
            ydot = -(f32)tirev[0];
            tire->patchy += ydot*m->dt;

            *sfp = tire->springK*tire->patchy +
                tire->rubdamp * ydot;

            if(*sfp < -maxf){
                *sfp = -(f32)maxf;
                tire->patchy = *sfp/tire->springK;
            }
            tire->slipflag += 6;
            tire->sliptorque = *trp * tire->tradius;
            tire->angvel += (realtorque - tire->sliptorque)*
                tire->invmi*m->dt;
            return;
        }
        tire->slipflag += 7;
        p = tirev[2]*tire->springK/(normalforce *
            (tire->k1 - l2*alpha + l3*alpha*alpha));
        if(p <  0)p = -(f32)p;

        if(p * m->dt < (f32).5){
            ydot = -p * tire->patchy - tirev[0];
            tire->patchy += ydot*m->dt;

            *sfp = tire->springK*tire->patchy +
                tire->rubdamp * ydot;
            tire->slipflag += 100;
        }
        else{
            *sfp = -(tire->k1*alpha - k2*alpha*alpha +
                k3*alpha*alpha*alpha)*normalforce;

            tire->patchy = *sfp/tire->springK;
            tire->slipflag += 200;
        }

    }
    else{
        if(alpha <= -Afmax){
            ydot = -(f32)tirev[0];
            tire->patchy += ydot*m->dt;

            *sfp = tire->springK*tire->patchy +
                tire->rubdamp * ydot;
            if(*sfp > maxf){
                *sfp = maxf;
                tire->patchy = *sfp/tire->springK;
            }
            tire->slipflag += 8;
            tire->sliptorque = *trp * tire->tradius;
            tire->angvel += (realtorque - tire->sliptorque)*
                tire->invmi*m->dt;
            return;
        }

        tire->slipflag += 9;
        p = tirev[2]*tire->springK/(normalforce *
            (tire->k1 + l2*alpha + l3*alpha*alpha));

        if(p <  0)p = -(f32)p;

        if(p * m->dt < (f32).5){
            ydot = -p * tire->patchy - tirev[0];
            tire->patchy += ydot*m->dt;

            *sfp = tire->springK*tire->patchy +
                tire->rubdamp * ydot;
            tire->slipflag += 100;
        }
        else{
            *sfp = -(tire->k1*alpha + k2*alpha*alpha +
                k3*alpha*alpha*alpha)*normalforce;

            tire->patchy = *sfp / tire->springK;
            tire->slipflag += 200;
        }

    }
}
