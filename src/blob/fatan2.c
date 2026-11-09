/* fatan2.c: the angle of a 2D vector (MAT3 atan.c, fatan2()); see fatan.c for the family. */
#include "game/game.h"

/* The angle of (x, y) from the x axis, radians in [-pi, pi]: the C library's atan2(y, x). When x is negligible
 * beside y (y + x == y) the answer is +-pi/2 directly, 0 for (0, 0). */
f32 fatan2(f32 y, f32 x) {
    if (y == y + x) {
        if (y == 0.0f) {
            return 0;
        }
        if (y > 0.0f) {
            return atan2_pio2;
        }
        return atan2_neg_pio2;
    }
    if (x < 0.0f) {
        if (y >= 0.0f) {
            return atan2_pi - fsatan(-y / x);
        }
        return fsatan(y / x) + atan2_neg_pi;
    }
    if (y > 0) {
        return fatan(y / x);
    }
    return -fatan(-y / x);
}
