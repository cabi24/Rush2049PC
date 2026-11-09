/* WPitchUV.c: pitch an orientation matrix about the world's x axis (MAT3 fmath.c, WPitchUV()).
 *
 * The world-axis rotation family (WYawUV, WPitchUV, WRollUV): they combine the matrix's columns, so every axis of
 * the object turns about the world's axis; PitchUV.c has the object-axis family. */
#include "game/game.h"

extern f32 sinf(f32);
extern f32 cosf(f32);

/* Turn the orientation uv by `angle` radians about the world's x axis (each row's y and z components turn into
 * each other). Angles within 0.001 of zero are ignored. */
void WPitchUV(f32 angle, f32 uv[][3]) {
    f32 ut, sint, cost;
    int i;

    if ((angle < -0.001f) || (angle > 0.001f)) {
        sint = sinf(angle);
        cost = cosf(angle);
        for (i = 0; i < 3; i++) {
            ut       = uv[i][1]*cost - uv[i][2]*sint;
            uv[i][2] = uv[i][2]*cost + uv[i][1]*sint;
            uv[i][1] = ut;
        }
    }
}
