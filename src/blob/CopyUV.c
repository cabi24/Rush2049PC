/* CopyUV.c: copy an orientation matrix (MAT3 fmath.c, CopyUV()).
 *
 * The game keeps an orientation as a 3x3 "unit vector" matrix (uvs): row k is the object's own axis k (0 right,
 * 1 up, 2 forward) in world coordinates; WorldVector and BodyVector convert between the two frames with it. */
#include "game/game.h"

/* Element (row, col) of a 3x3 matrix by its byte offset, as the decompiled code reaches it (plain indexing, or no
 * (f32) on the right, gives other unoptimised code). */
#define UV_AT(p, row, col) M2C_FIELD(p, f32 *, ((row) * 3 + (col)) * 4)

/* Copy the nine floats of the matrix src into dst, row by row. */
void CopyUV(f32 (*src)[3], f32 (*dst)[3]) {
    UV_AT(dst, 0, 0) = (f32) UV_AT(src, 0, 0);
    UV_AT(dst, 0, 1) = (f32) UV_AT(src, 0, 1);
    UV_AT(dst, 0, 2) = (f32) UV_AT(src, 0, 2);
    UV_AT(dst, 1, 0) = (f32) UV_AT(src, 1, 0);
    UV_AT(dst, 1, 1) = (f32) UV_AT(src, 1, 1);
    UV_AT(dst, 1, 2) = (f32) UV_AT(src, 1, 2);
    UV_AT(dst, 2, 0) = (f32) UV_AT(src, 2, 0);
    UV_AT(dst, 2, 1) = (f32) UV_AT(src, 2, 1);
    UV_AT(dst, 2, 2) = (f32) UV_AT(src, 2, 2);
}
