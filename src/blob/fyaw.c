/* fyaw.c: yaw an orientation matrix by a known sine and cosine (MAT3 fmath.c, fyaw()).
 *
 * YawUV without the threshold and the trigonometry, for callers that have the sine and cosine already. */
#include "game/game.h"

/* Turn the orientation m about its own y (up) axis by the angle whose sine and cosine are given (the right and
 * forward rows turn into each other). */
void fyaw(f32 sint, f32 cost, f32 m[3][3]) {
    f32 ut;
    ut = m[0][0] * cost + m[2][0] * sint;
    m[2][0] = m[2][0] * cost - m[0][0] * sint;
    m[0][0] = ut;
    ut = m[0][1] * cost + m[2][1] * sint;
    m[2][1] = m[2][1] * cost - m[0][1] * sint;
    m[0][1] = ut;
    ut = m[0][2] * cost + m[2][2] * sint;
    m[2][2] = m[2][2] * cost - m[0][2] * sint;
    m[0][2] = ut;
}
