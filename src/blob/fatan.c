/* fatan.c: the arctangent (MAT3 atan.c, fatan()).
 *
 * The game's arctangents are single-precision versions of the Unix V7 libm ones (Hart and Cheney's 5076):
 * fatan and fatan2 reduce to fsatan, which reduces to the rational approximation fxatan. Their constants are game
 * data (atan_p0 .. atan_pio4, atan2_pio2 ..). */
#include "game/game.h"

/* The arctangent of x, radians in (-pi/2, pi/2): fsatan of |x|, with x's sign. */
f32 fatan(f32 x) {
    if (x > 0.0f) {
        return fsatan(x);
    }
    return -fsatan(-x);
}
