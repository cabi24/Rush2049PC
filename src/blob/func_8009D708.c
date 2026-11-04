/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (-O3 required: at -O2 `scale` is spilled to its home slot) */
/*
 * Builds an N64 fixed-point Mtx (16 words: integer halves 0..7, fraction
 * halves 8..15) from a 3x3 rotation and a position.  Unless `absolute` is set
 * the position is taken relative to view record D_80150B70[idx] (0x98 bytes,
 * same rot/pos layout).  If the offset is longer than 1600 units it is pulled
 * in to 1600 and the rotation is scaled by the same factor, so far objects
 * are drawn nearer and smaller.  Positions are stored x16 (2^20 instead of
 * 2^16); element [3][3] is `scale`.  The [n][3] elements of the rotation
 * rows are 0.  N64-only (no arcade ancestor identified).
 *
 * Shaping quirks:
 *   - the distance reuses `s` (a separate `dist` local takes $f20 and a frame);
 *   - `s *= 65536` with an int literal: `65536.0f` joins the web of the
 *     `scale * 65536.0f` literal and is kept in a register;
 *   - `(s32)(scale * 65536.0f)` is written out twice instead of going through
 *     a local: as a local it takes a2 and moves `m` to a3.
 */
typedef float f32;
typedef int s32;
typedef unsigned int u32;

typedef struct {
    f32 rot[3][3];
    f32 pos[3];
    char pad30[0x68];
} View; /* 0x98 */

extern View gCamera[];

extern f32 sqrtf(f32);
#pragma intrinsic(sqrtf)

#ifdef TARGET_PC
/* PC: video.draw_distance (docs/port/DETAIL.md 2.3). Far scenery (a node with flag 0x400000) further than 1,600 ft
 * is pulled in to 1,600 ft and shrunk by as much: the same picture, inside the N64's 2,000-ft far plane. Far pulls it
 * in to PC_DRAW_FAR_FACTOR times that, max not at all (its true place), with the depth range's far plane moved out
 * as far (port/gfx/gfx_detail.c); past 2,048 ft the whole matrix is divided by a power of two h, as in
 * func_8009D45C. At original the N64's words. */
/* PC: far scenery whose place from the eye is not a finite number is left out (track_collision_wall's
 * PC_SCENERY_CHECK): the camera of a race's first frames, before PrePlay places it, is NaN (seen at draw_distance =
 * max, which draws the scenery then), and converting NaN to the matrix's integers is undefined in C. On the N64 the
 * FPU gives a word of its own; the game's other nodes are left out by their cull (track_collision_edge) first. */
s32 pc_scenery_unplaced(s32 idx, void *node_obj, s32 absolute) {
    View *obj = node_obj;
    f32 x, y, z;

    if (absolute) {
        x = obj->pos[0];
        y = obj->pos[1];
        z = obj->pos[2];
    } else {
        x = obj->pos[0] - gCamera[idx].pos[0];
        y = obj->pos[1] - gCamera[idx].pos[1];
        z = obj->pos[2] - gCamera[idx].pos[2];
    }
    return !(x - x == 0.0f && y - y == 0.0f && z - z == 0.0f);   /* NaN or infinite */
}

void func_8009D708(s32 idx, View *obj, u32 *m, f32 scale, s32 absolute) {
    f32 x, y, z;
    f32 s, reach;
    s32 a, b, h;
    s32 xi, yi, zi, wi;

    if (absolute) {
        x = obj->pos[0];
        y = obj->pos[1];
        z = obj->pos[2];
    } else {
        x = obj->pos[0] - gCamera[idx].pos[0];
        y = obj->pos[1] - gCamera[idx].pos[1];
        z = obj->pos[2] - gCamera[idx].pos[2];
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
#else
void func_8009D708(s32 idx, View *obj, u32 *m, f32 scale, s32 absolute) {
    f32 x, y, z;
    f32 s;
    s32 a, b;
    s32 xi, yi, zi, wi;

    if (absolute) {
        x = obj->pos[0];
        y = obj->pos[1];
        z = obj->pos[2];
    } else {
        x = obj->pos[0] - gCamera[idx].pos[0];
        y = obj->pos[1] - gCamera[idx].pos[1];
        z = obj->pos[2] - gCamera[idx].pos[2];
    }
    s = sqrtf(x * x + y * y + z * z);
    if (s > 1600.0f) {
        s = 1600.0f / s;
        x *= s;
        y *= s;
        z *= s;
    } else {
        s = 1.0f;
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
#endif
