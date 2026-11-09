/* build_node_matrix_rows4.c: a camera-facing scene node's matrix in the RSP's fixed-point format.
 *
 * build_node_matrix's twin for the nodes that turn to face the camera (Traverse, zoid_draw): the rotation is a
 * shared 4x4 float matrix (zoid_view_matrix, zoid_upright_matrix or a scaled copy), the position the node's. */
#include "game/game.h"

/* The RSP's Mtx (build_node_matrix.c). */
typedef u32 Mtx[16];
float sqrtf(float);
#pragma intrinsic (sqrtf)

/* Write into out the matrix of a node at `pos` turned by the 4x4 float matrix m, for the view `view`: rows 0-2 are
 * m's columns 0-2 (m is a lookat-style view matrix, so its transpose turns the object toward the eye), row 3 the
 * position and w, as in build_node_matrix (depth 0, no node matrix above: pos is a world position, taken
 * relative to the camera). Returns 0 (and writes nothing) when the node is too far away to place.
 *
 * PC: video.draw_distance (docs/port/DETAIL.md 2.3). The N64 leaves out a node 2,048 ft or more from the eye on
 * any axis: its place no longer fits the matrix's s15.16 translation, in sixteenths of a foot. Far and max reach
 * further by dividing the whole matrix, its rotation, its place and its w, by a power of two h
 * (pc_draw_range_scale): every vertex comes out h times smaller in x, y, z and w, the same point on the screen with
 * the same z/w, so the same depth and fog. Inside the N64's range h is 1 and every word is the N64's. */
s32 build_node_matrix_rows4(s32 view, f32 (*m)[4], f32 *pos, Mtx *out, f32 w, s32 depth) {
    f32 x, y, z, sc;
    s32 a, b, c, d, h;
    u32 *words = (u32 *)out;

    if (depth != 0) {
        x = pos[0];
        y = pos[1];
        z = pos[2];
    } else {
        x = pos[0] - gCamera[view].pos[0];
        y = pos[1] - gCamera[view].pos[1];
        z = pos[2] - gCamera[view].pos[2];
    }
    h = pc_draw_range_scale(x, y, z);
    if (h == 0) {
        return 0;
    }
    sc = 65536.0f;
    if (h > 1) {
        f32 k = 1.0f / (f32)h;
        x *= k;
        y *= k;
        z *= k;
        w *= k;
        sc *= k;
    }
    a = (s32)(x * 1048576.0f);
    b = (s32)(y * 1048576.0f);
    c = (s32)(z * 1048576.0f);
    d = (s32)(w * 65536.0f);
    words[6] = MTX_HI(a, b);
    words[14] = MTX_LO(a, b);
    words[7] = MTX_HI(c, d);
    words[15] = MTX_LO(c, d);
    a = (s32)(m[0][0] * sc);
    b = (s32)(m[1][0] * sc);
    words[0] = MTX_HI(a, b);
    words[8] = MTX_LO(a, b);
    a = (s32)(m[2][0] * sc);
    words[1] = MTX_HI(a, 0);
    words[9] = MTX_LO(a, 0);
    a = (s32)(m[0][1] * sc);
    b = (s32)(m[1][1] * sc);
    words[2] = MTX_HI(a, b);
    words[10] = MTX_LO(a, b);
    a = (s32)(m[2][1] * sc);
    words[3] = MTX_HI(a, 0);
    words[11] = MTX_LO(a, 0);
    a = (s32)(m[0][2] * sc);
    b = (s32)(m[1][2] * sc);
    words[4] = MTX_HI(a, b);
    words[12] = MTX_LO(a, b);
    a = (s32)(m[2][2] * sc);
    words[5] = MTX_HI(a, 0);
    words[13] = MTX_LO(a, 0);
    return 1;
}
