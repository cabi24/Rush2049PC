/* RotateUV.c: turn an orientation matrix by pitch, yaw and roll angles (MAT3 fmath.c, RotateUV()). */
#include "game/game.h"

/* Turn the orientation uvs about its own axes by the angles pyr (radians: pitch, yaw, roll): yaw first, then pitch,
 * then roll (YawUV, PitchUV, RollUV). */
void RotateUV(f32 *pyr, f32 (*uvs)[3]) {
    YawUV(pyr[1], uvs);
    PitchUV(pyr[0], uvs);
    RollUV(pyr[2], uvs);
}
