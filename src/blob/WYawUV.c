/* WYawUV.c: yaw an orientation matrix about the world's y axis (MAT3 fmath.c, WYawUV()).
 *
 * The world-axis rotation family of WPitchUV.c (see there). */
#include "game/game.h"

extern f32 sinf(f32);
extern f32 cosf(f32);

/* Turn the orientation uv by `angle` radians about the world's y (vertical) axis (each row's x and z components
 * turn into each other). Angles within 0.001 of zero are ignored. */
void WYawUV(f32 angle, f32 uv[][3]) {
    f32 ut, sint, cost;
    int i;

    if ((angle < -0.001f) || (angle > 0.001f)) {
        sint = sinf(angle);
        cost = cosf(angle);
        for (i = 0; i < 3; i++) {
            ut       = uv[i][0]*cost + uv[i][2]*sint;
            uv[i][2] = uv[i][2]*cost - uv[i][0]*sint;
            uv[i][0] = ut;
        }
    }
}
