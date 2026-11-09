/* enginetorque.c: the engine's torque off its torque curve (MAT3 drivetra.c, enginetorque()).
 *
 * engine() calls it each model step. */
#include "game/game.h"

/* The tune record (MAT3 anon_diffbl_weight_scale_2c), its torque scale. */
typedef struct {
    u8 pad0[8];
    f32 torque_scale;       /* +0x08 */
} tTune;

/* The model record (tModelData), the fields enginetorque reads. */
typedef struct tModelData {
    _tCar *car;             /* +0x000 the physics descriptor */
    tTune *tune;            /* +0x004 the tune record */
    u8 pad8[3];
    s8 handling;            /* +0x00B the handling (tune) choice */
    s8 engine_type;         /* +0x00C the engine choice */
} tModelData;

#define RPM_SCALE 1150      /* the curve's rpm step: its 12 columns span 0 to 12,650 rpm */

/* The torque of model m's engine at `rpm` with `throttle` (0..128), from torquecurve (stdtorquecurve: 10 rows of
 * throttle, 14 apart, each 12 columns of rpm, RPM_SCALE apart): interpolated between the four neighbouring entries
 * (interp), rpm first, clamped to the curve's ends; then scaled by the tune's torque_scale and carEngineTorque for
 * the engine and handling choices. */
s16 enginetorque(tModelData *m, s16 rpm, s16 throttle, const s16 *torquecurve)
{
    s16 rindex, tindex, rrem, trem, left, right;
    const s16 *low_ptr, *hi_ptr;

    rindex = rpm / (s16)RPM_SCALE;
    rrem = rpm % (s16)RPM_SCALE;
    if (rindex < 0) {
        rindex = 0;
        rrem = 0;
    }
    if (rindex >= 11) {
        rindex = 10;
        rrem = RPM_SCALE - 1;
    }
    tindex = throttle / (s16)14;
    trem = throttle % (s16)14;
    if (tindex >= 9) {
        tindex = 8;
        trem = 13;
    }
    low_ptr = torquecurve + (tindex * 12) + rindex;
    hi_ptr = torquecurve + (tindex * 12) + rindex + 1;
    left = interp((*low_ptr), (*hi_ptr), rrem, RPM_SCALE - 1);
    low_ptr = torquecurve + ((tindex + 1) * 12) + rindex;
    hi_ptr = torquecurve + ((tindex + 1) * 12) + rindex + 1;
    right = interp((*low_ptr), (*hi_ptr), rrem, RPM_SCALE - 1);
    right = interp(left, right, trem, 14);
    right *= m->tune->torque_scale * carEngineTorque[m->engine_type][m->handling];
    return right;
}
