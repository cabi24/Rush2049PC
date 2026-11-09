/* build_far_node_matrix (the N64 scene draw's): the matrix of a far scenery node, pulled in toward the camera so
 * that it stays inside the far plane, and the PC's check for a node with no place yet. */
#include "game/game.h"

typedef struct View {
    f32 rot[3][3];
    f32 pos[3];
    char pad30[0x68];
} View; /* 0x98 */

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

/* PC: video.draw_distance (docs/port/DETAIL.md 2.3). Far scenery (a node with flag 0x400000) further than 1,600 ft
 * is pulled in to 1,600 ft and shrunk by as much: the same picture, inside the N64's 2,000-ft far plane. Far pulls it
 * in to PC_DRAW_FAR_FACTOR times that, max not at all (its true place), with the depth range's far plane moved out
 * as far (port/gfx/gfx_detail.c); past 2,048 ft the whole matrix is divided by a power of two h, as in
 * build_node_matrix. At original the N64's words. */
/* PC: far scenery whose place from the eye is not a finite number is left out (Traverse's
 * PC_SCENERY_CHECK): the camera of a race's first frames, before PrePlay places it, is NaN (seen at draw_distance =
 * max, which draws the scenery then), and converting NaN to the matrix's integers is undefined in C. On the N64 the
 * FPU gives a word of its own; the game's other nodes are left out by their cull (zoid_node_visible) first. */
s32 pc_scenery_unplaced(s32 idx, void *node_obj, s32 absolute) {
    View *obj = node_obj;
    f32 x, y, z;

    if (absolute) {
        x = obj->pos[0];
        y = obj->pos[1];
        z = obj->pos[2];
    } else {
        x = obj->pos[0] - gCamera[idx].mat.pos[0];
        y = obj->pos[1] - gCamera[idx].mat.pos[1];
        z = obj->pos[2] - gCamera[idx].mat.pos[2];
    }
    return !(x - x == 0.0f && y - y == 0.0f && z - z == 0.0f);   /* NaN or infinite */
}

/* Writes into m (an N64 fixed-point Mtx: integer halves in m[0..7], fractions in m[8..15]) the matrix of node obj
 * for window idx's camera: its place relative to the camera (or absolute), pulled in to the draw reach and shrunk
 * by as much when it is farther (and scaled down by a power of two past the depth range, pc_draw_range_scale), its
 * rotation scaled by the same shrink, and scale in the w column. */
void build_far_node_matrix(s32 idx, View *obj, u32 *m, f32 scale, s32 absolute) {
    f32 x, y, z;
    f32 s, reach;
    s32 a, b, h;
    s32 xi, yi, zi, wi;

    if (absolute) {
        x = obj->pos[0];
        y = obj->pos[1];
        z = obj->pos[2];
    } else {
        x = obj->pos[0] - gCamera[idx].mat.pos[0];
        y = obj->pos[1] - gCamera[idx].mat.pos[1];
        z = obj->pos[2] - gCamera[idx].mat.pos[2];
    }
    reach = PC_DRAW_REACH(1600.0f);
    s = sqrtf(x * x + y * y + z * z);
    if (s > reach) {
        s = reach / s;
        x *= s;
        y *= s;
        z *= s;
    } else {
        s = 1.0f;
    }
    h = pc_draw_range_scale(x, y, z);
    if (h > 1) {
        f32 k = 1.0f / (f32)h;
        x *= k;
        y *= k;
        z *= k;
        scale *= k;
        s *= k;
    }
    xi = x * 1048576.0f;
    yi = y * 1048576.0f;
    zi = z * 1048576.0f;
    m[6] = (xi & 0xFFFF0000) | ((u32)yi >> 16);
    m[14] = (xi << 16) | (yi & 0xFFFF);
    m[7] = (zi & 0xFFFF0000) | ((u32)(s32)(scale * 65536.0f) >> 16);
    m[15] = (zi << 16) | ((s32)(scale * 65536.0f) & 0xFFFF);
    s *= 65536;
    a = obj->rot[0][0] * s;
    b = obj->rot[0][1] * s;
    m[0] = (a & 0xFFFF0000) | ((u32)b >> 16);
    m[8] = (a << 16) | (b & 0xFFFF);
    a = obj->rot[0][2] * s;
    m[1] = a & 0xFFFF0000;
    m[9] = a << 16;
    a = obj->rot[1][0] * s;
    b = obj->rot[1][1] * s;
    m[2] = (a & 0xFFFF0000) | ((u32)b >> 16);
    m[10] = (a << 16) | (b & 0xFFFF);
    a = obj->rot[1][2] * s;
    m[3] = a & 0xFFFF0000;
    m[11] = a << 16;
    a = obj->rot[2][0] * s;
    b = obj->rot[2][1] * s;
    m[4] = (a & 0xFFFF0000) | ((u32)b >> 16);
    m[12] = (a << 16) | (b & 0xFFFF);
    a = obj->rot[2][2] * s;
    m[5] = a & 0xFFFF0000;
    m[13] = a << 16;
}
