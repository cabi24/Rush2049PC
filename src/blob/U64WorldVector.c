/* U64WorldVector.c: a vector from a camera's frame into world coordinates (MAT3 fmath.c, U64WorldVector()).
 *
 * WorldVector with signs flipped: the x body component counts negatively in world y and z, the y and z body
 * components negatively in world x. The game uses it with a camera's matrix (gCamera[].mat) to lay out quads that
 * face the camera (the player arrows, animate_arrow's AnimateArrow; the countdown's sounds). "U64" is Ultra 64,
 * the N64: MAT3 kept it as is. */
#include "game/game.h"

/* uvec = bodvec, given in the frame of the matrix uvs (nine floats, row by row), in world coordinates with the
 * sign flips above. */
void U64WorldVector(f32 *bodvec, f32 *uvec, f32 *uvs) {
 uvec[0] = bodvec[0]*uvs[0] - bodvec[1]*uvs[3] - bodvec[2]*uvs[6];
 uvec[1] = (-bodvec[0]*uvs[1] + bodvec[1]*uvs[4]) + bodvec[2]*uvs[7];
 uvec[2] = (-bodvec[0]*uvs[2] + bodvec[1]*uvs[5]) + bodvec[2]*uvs[8];
}
