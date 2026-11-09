/* fpitch.c: pitch an orientation matrix by a known sine and cosine (MAT3 fmath.c, fpitch()).
 *
 * PitchUV without the threshold and the trigonometry, for callers that have the sine and cosine already. */
#include "game/game.h"

/* Turn the orientation m about its own x axis by the angle whose sine and cosine are given (the up and forward rows
 * turn into each other). */
void fpitch(f32 sint, f32 cost, f32 m[3][3]) {
    f32 ut;
    ut = m[1][0] * cost - m[2][0] * sint;
    m[2][0] = m[2][0] * cost + m[1][0] * sint;
    m[1][0] = ut;
    ut = m[1][1] * cost - m[2][1] * sint;
    m[2][1] = m[2][1] * cost + m[1][1] * sint;
    m[1][1] = ut;
    ut = m[1][2] * cost - m[2][2] * sint;
    m[2][2] = m[2][2] * cost + m[1][2] * sint;
    m[1][2] = ut;
}
