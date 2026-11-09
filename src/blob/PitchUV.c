/* PitchUV.c: pitch an orientation matrix about its own x axis (MAT3 fmath.c, PitchUV()).
 *
 * One of six rotations of a 3x3 orientation matrix (uvs: row k the object's axis k in world coordinates): YawUV,
 * PitchUV and RollUV turn the object about its own axes (they combine its rows), WYawUV, WPitchUV and WRollUV
 * about the world's (they combine its columns). The arcade's code; on the N64 the threshold is a float and the
 * sine and cosine are sinf and cosf. The operand order of each product is the original's. */
#include "game/game.h"

extern f32 sinf(f32);
extern f32 cosf(f32);

/* Turn the orientation uv by `angle` radians about its own x axis (the up and forward rows turn into each other).
 * Angles within 0.0001 of zero are ignored. */
void PitchUV(f32 angle, f32 uv[][3]) {
    f32 ut, sint, cost;
    int i;

    if ((angle < -0.0001f) || (angle > 0.0001f)) {
        sint = sinf(angle);
        cost = cosf(angle);
        for (i = 0; i < 3; i++) {
            ut       = uv[1][i]*cost - uv[2][i]*sint;
            uv[2][i] = uv[2][i]*cost + uv[1][i]*sint;
            uv[1][i] = ut;
        }
    }
}
