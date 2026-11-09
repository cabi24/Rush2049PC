/* tan_cody_waite.c: the tangent, Cody and Waite's algorithm (MAT3 tan.c, ftan()).
 *
 * ZOID_SetWindow uses it for the view's field of view. Its constants are game data (tan_max_arg, tan_two_over_pi,
 * tan_half_pi_hi/lo, tan_epsilon, tan_p1..p3, tan_q1..q4). */
#include "game/game.h"

f32 fabsf(f32);
#pragma intrinsic(fabsf)
f32 modff(f32, f32 *);

/* tan(x), x in radians; 0 past tan_max_arg. x is reduced by the nearest multiple xn of pi/2 (pi/2 split in a high
 * and a low part so the subtraction stays exact: f = x - xn pi/2, built from x's integer and fraction parts g and
 * f), then tan(f) is a rational approximation xnum / xden; an odd xn gives -1 / tan(f) instead. */
f32 tan_cody_waite(f32 x) {
    f32 xnum;
    f32 g;
    f32 xn;
    f32 y;
    f32 f;
    f32 xden;

    if (fabsf(x) > tan_max_arg) {
        return 0.0f;
    }
    y = modff(x * tan_two_over_pi, &xn);
    if (fabsf(y) >= 0.5f) {
        xn += (x < 0.0f) ? -1.0f : 1.0f;
    }
    f = modff(x, &g);
    xnum = ((g - xn * tan_half_pi_hi) + f) - xn * tan_half_pi_lo;
    f = xnum;
    if (fabsf(xnum) < tan_epsilon) {
        xden = 1.0f;
    } else {
        g = f * f;
        xnum = ((tan_p3 * g + tan_p2) * g + tan_p1) * g * f + f;
        xden = (((tan_q4 * g + tan_q3) * g + tan_q2) * g + tan_q1) * g + 1.0f;
    }
    if ((s32) xn & 1) {
        return xden / -xnum;
    }
    return xnum / xden;
}
