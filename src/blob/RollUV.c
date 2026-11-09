/* RollUV.c: roll an orientation matrix about its own z axis (MAT3 fmath.c, RollUV()).
 *
 * The object-axis rotation family of PitchUV.c (see there). */
#include "game/game.h"

extern f32 sinf(f32);
extern f32 cosf(f32);

/* Turn the orientation uv by `angle` radians about its own z (forward) axis: the right and up rows turn into each
 * other. Angles within 0.0001 of zero are ignored. */
void RollUV(f32 angle, f32 uv[][3]) {
    f32 ut, sint, cost;
    int i;

    if ((angle < -0.0001f) || (angle > 0.0001f)) {
        sint = sinf(angle);
        cost = cosf(angle);
        for (i = 0; i < 3; i++) {
            ut       = uv[0][i]*cost - uv[1][i]*sint;
            uv[1][i] = uv[1][i]*cost + uv[0][i]*sint;
            uv[0][i] = ut;
        }
    }
}
