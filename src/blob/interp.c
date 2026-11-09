/* interp.c: integer linear interpolation (MAT3 drivetra.c, interp()).
 *
 * enginetorque uses it to read between the points of the torque curve. */
#include "game/game.h"

/* The value rem / total of the way from a to b: a + (b - a) * rem / total, in integers (the division truncates
 * toward zero), as a 16-bit result. */
s16 interp(s32 a, s32 b, s32 rem, s32 total) {
    return (s16) (((s32) ((b - a) * rem) / total) + a);
}
