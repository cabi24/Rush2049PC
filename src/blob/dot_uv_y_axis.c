/* dot_uv_y_axis.c: a vector's component along an orientation's up axis. */
#include "game/game.h"

/* The dot product of the vector a with row 1 of the orientation matrix b (nine floats, row by row): how far a points
 * along the object's own up (y) axis; the y component of BodyVector(a, ., b). */
f32 dot_uv_y_axis(f32 *a,f32 *b) {
    return (a[0]*b[3]+a[1]*b[4])+a[2]*b[5];
}
