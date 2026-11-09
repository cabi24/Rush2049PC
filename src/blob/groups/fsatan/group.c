/* fsatan/group.c: the arctangent of a positive number (MAT3 atan.c, fsatan()); see fatan.c for the family. */
#include "game/game.h"

/* The arctangent of x >= 0: fxatan directly below sqrt(2) - 1 (atan_sq2m1); above sqrt(2) + 1 (atan_sq2p1)
 * pi/2 - atan(1/x); between them pi/4 + atan((x - 1) / (x + 1)). Each keeps fxatan's argument small. */
f32 fsatan(f32 x) {
    if (x < atan_sq2m1) {
        return fxatan(x);
    }
    if (atan_sq2p1 < x) {
        return atan_pio2 - fxatan(1.0f / x);
    }
    return fxatan((x - 1.0f) / (x + 1.0f)) + atan_pio4;
}
