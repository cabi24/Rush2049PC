/* MovingSurfaceForce.c: what a moving road surface does to a car (MAT3 drivsym.c, MovingSurfaceForce()).
 *
 * A stage of the physics step (sym), run when road contact (TireSurf) found the car on a surface whose sound flags
 * mark it as moving: a boost strip pushes the car up to a speed, a platform or conveyor carries it. */
#include "game/game.h"

/* The model record (tModelData), the fields MovingSurfaceForce reads and writes. */
typedef struct tModelData {
    _tCar *car;
    Tune44 *tune;
    u8 pad8[0x5C];
    f32 tireForce[4][3];    /* +0x064 the force on each tire, in the car's frame */
    u8 pad94[0x124 - 0x94];
    f32 centerForce[3];     /* +0x124 the force at the centre of mass, in the car's frame */
    u8 pad130[0x220 - 0x130];
    f32 rwv[3];             /* +0x220 the velocity, world frame, feet a second */
    f32 rwr[3];             /* +0x22C the position, world frame, feet */
    u8 pad238[0x2EC - 0x238];
    f32 uvs[9];             /* +0x2EC the orientation (rows: the car's axes) */
    u8 pad310[0x3A0 - 0x310];
    streetyp *lasttp[4];
    u8 pad3B0[0x40C - 0x3B0];
    const s16 *torquecurve;
    u8 pad410[0x5A0 - 0x410];
    u16 tiresli[4];         /* +0x5A0 each tire's road surface (an index into stree_surf) */
    u8 pad5A8[0x5C0 - 0x5A8];
    f32 mass;               /* +0x5C0 */
    u8 pad5C4[0x5EC - 0x5C4];
    f32 airdist[4];         /* +0x5EC each tire's height above the road (0 or less: touching) */
    u8 pad5FC[0x634 - 0x5FC];
    f32 dt;                 /* +0x634 the step, seconds */
    u8 pad638[0x648 - 0x638];
    surftyp *movingSurface; /* +0x648 the moving surface under the car */
} tModelData;

/* Apply model m's moving surface (m->movingSurface) for this step:
 * - a push strip (snd_flags 0x10) faster than 50 mph: a force at the centre along the strip's direction, mass times
 *   the speed the car lacks along it, so the car is driven up to the strip's speed;
 * - else, with three or four tires on moving surfaces: the car rides it, its position moved by the surface's motion
 *   for the step (a target's velocity, snd_flags 0x20, or the surface's direction at its speed);
 * - else a force of 30 times the mass on each tire that is on a moving surface, along its motion. */
void MovingSurfaceForce(tModelData *m) {
    surftyp *sp;
    s32 i;
    f32 temp[3];
    f32 temp1[3];
    s32 on;
    s32 obj;
    u32 mask;
    f32 speed;

    sp = m->movingSurface;
    on = 0;
    mask = 0;
    i = 0;
    obj = (sp->snd_flags & 0xF800) >> 11;
    if (sp->snd_flags & 0x10) {
        obj *= 8;
        if (obj > 50) {
            temp[0] = sp->uvs[0][0] * (1.0f / 16384.0f);
            temp[1] = sp->uvs[0][1] * (1.0f / 16384.0f);
            temp[2] = sp->uvs[0][2] * (1.0f / 16384.0f);
            speed = temp[0] * m->rwv[0] + temp[1] * m->rwv[1] + temp[2] * m->rwv[2];
            speed = obj * MPH_TO_FTPS - speed;
            speed *= m->mass;
            temp1[0] = temp[0] * speed;
            temp1[1] = temp[1] * speed;
            temp1[2] = temp[2] * speed;
            BodyVector(temp1, temp, m->uvs);
            m->centerForce[0] += temp[0];
            m->centerForce[1] += temp[1];
            m->centerForce[2] += temp[2];
            return;
        }
    }
    for (i = 0; i < 4; i++) {
        if (m->airdist[i] <= 0.0f) {
            if (stree_surf[m->tiresli[i]].snd_flags & 0x30) {
                on++;
                mask |= 1 << i;
            }
        }
    }
    if (on >= 3) {
        if (sp->snd_flags & 0x20) {
            GetTargetVelocity(obj, temp);
            m->rwr[0] += temp[0] * m->dt;
            m->rwr[2] += temp[2] * m->dt;
        } else {
            speed = obj * MPH_TO_FTPS * m->dt;
            m->rwr[0] += speed * sp->uvs[0][0] * (1.0f / 16384.0f);
            m->rwr[2] += speed * sp->uvs[0][2] * (1.0f / 16384.0f);
        }
        return;
    }
    for (i = 0; i < 4; i++) {
        if ((1 << i) & mask) {
            if (sp->snd_flags & 0x20) {
                GetTargetVelocity(obj, temp);
                NormalVector(temp);
            } else {
                temp[0] = sp->uvs[0][0] * (1.0f / 16384.0f);
                temp[1] = sp->uvs[0][1] * (1.0f / 16384.0f);
                temp[2] = sp->uvs[0][2] * (1.0f / 16384.0f);
            }
            temp[0] *= 30.0f * m->mass;
            temp[1] *= 30.0f * m->mass;
            temp[2] *= 30.0f * m->mass;
            BodyVector(temp, temp1, m->uvs);
            if (i) {
                /* no effect */
            }
            m->tireForce[i][0] += temp1[0];
            m->tireForce[i][1] += temp1[1];
            m->tireForce[i][2] += temp1[2];
        }
    }
}
