/* FVecInterp: a point between two (MAT3 fmath.c). */
#include "game/game.h"

/* Sets `out` to the point fraction `t` of the way from `a` to `b` (a * (1 - t) + b * t). */
void FVecInterp(f32 *a, f32 *b, f32 t, f32 *out)
{
    s16 i;
    f32 c;
    i = 0;
    do {
        i += 1;
        c = 1.0f - t;
        *out = a[0] * c + b[0] * t;
        out++;
        a++;
        b++;
    } while (i < 3);
}
