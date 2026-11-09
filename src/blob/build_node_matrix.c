/* build_node_matrix.c: a scene node's model matrix in the RSP's fixed-point format.
 *
 * The scene draw (Traverse, zoid_draw) builds one per drawn node a frame into the frame's matrix pool and loads it
 * with gSPMatrix; build_node_matrix_rows4 is its twin for the camera-facing nodes. */
#include "game/game.h"

/* The RSP's Mtx: a 4x4 s15.16 matrix as 16 words, the integer halves of the 16 elements (two to a word, row by row)
 * in words 0-7, their fractions in words 8-15. */
typedef u32 Mtx[16];
/* A node's place: its orientation (3x3, row by row: the object's axes) and its position (MAT3 tUVMatrix). */
typedef struct XForm {
    f32 m[9];
    f32 pos[3];
} XForm;
float sqrtf(float);
#pragma intrinsic (sqrtf)

/* The two words of a pair of elements a, b (each times 65536 already): the integer halves, and the fractions. */
#define HI(a, b) (((u32)(a) & 0xFFFF0000) | ((u32)(b) >> 16))
#define LO(a, b) (((u32)(a) << 16) | ((u32)(b) & 0xFFFF))

/* Write the matrix of the node placed at src into out for the view `view`: rows 0-2 the node's axes, row 3 its
 * position and w. `depth` is how many node matrices Traverse has pushed above this node: with none (0) its
 * position is a world one, from which the view's camera position is taken so the matrix is relative to the eye;
 * else it is relative to the parent's matrix already. The position goes in in sixteenths of a foot
 * (x 16 x 65536), w as it is. Returns 0 (and writes nothing) when the node is too far away to place.
 *
 * PC: video.draw_distance (docs/port/DETAIL.md 2.3). The N64 leaves out a node 2,048 ft or more from the eye on
 * any axis: its place no longer fits the matrix's s15.16 translation, in sixteenths of a foot. Far and max reach
 * further by dividing the whole matrix, its rotation, its place and its w, by a power of two h
 * (pc_draw_range_scale): every vertex comes out h times smaller in x, y, z and w, the same point on the screen with
 * the same z/w, so the same depth and fog. Inside the N64's range h is 1 and every word is the N64's. */
s32 build_node_matrix(s32 view, XForm *src, Mtx *out, f32 w, s32 depth) {
    f32 x, y, z, sc;
    s32 a, b, c, d, h;
    u32 *words = (u32 *)out;

    if (depth != 0) {
        x = src->pos[0];
        y = src->pos[1];
        z = src->pos[2];
    } else {
        x = src->pos[0] - gCamera[view].position[0];
        y = src->pos[1] - gCamera[view].position[1];
        z = src->pos[2] - gCamera[view].position[2];
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
    words[6] = HI(a, b);
    words[14] = LO(a, b);
    words[7] = HI(c, d);
    words[15] = LO(c, d);
    a = (s32)(src->m[0] * sc);
    b = (s32)(src->m[1] * sc);
    words[0] = HI(a, b);
    words[8] = LO(a, b);
    a = (s32)(src->m[2] * sc);
    words[1] = HI(a, 0);
    words[9] = LO(a, 0);
    a = (s32)(src->m[3] * sc);
    b = (s32)(src->m[4] * sc);
    words[2] = HI(a, b);
    words[10] = LO(a, b);
    a = (s32)(src->m[5] * sc);
    words[3] = HI(a, 0);
    words[11] = LO(a, 0);
    a = (s32)(src->m[6] * sc);
    b = (s32)(src->m[7] * sc);
    words[4] = HI(a, b);
    words[12] = LO(a, b);
    a = (s32)(src->m[8] * sc);
    words[5] = HI(a, 0);
    words[13] = LO(a, 0);
    return 1;
}
