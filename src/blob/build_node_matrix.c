#include "game/game.h"

typedef u32 Mtx[16];
typedef struct {
    u8 pad0[36];
    f32 pos[3];
    u8 pad1[152 - 48];
} Ent;
typedef struct XForm {
    f32 m[9];
    f32 pos[3];
} XForm;
VIEW(Ent[], gCamera);
#define gCamera gCamera_view
float sqrtf(float);
#pragma intrinsic (sqrtf)

/* pack two floats into the N64 fixed-point Mtx word pair */
#define HI(a, b) (((u32)(a) & 0xFFFF0000) | ((u32)(b) >> 16))
#define LO(a, b) (((u32)(a) << 16) | ((u32)(b) & 0xFFFF))

/* PC: video.draw_distance (docs/port/DETAIL.md 2.3). The N64 leaves out a node 2,048 ft or more from the eye on
 * any axis: its place no longer fits the matrix's s15.16 translation, in sixteenths of a foot. Far and max reach
 * further by dividing the whole matrix, its rotation, its place and its w, by a power of two h
 * (pc_draw_range_scale): every vertex comes out h times smaller in x, y, z and w, the same point on the screen with
 * the same z/w, so the same depth and fog. Inside the N64's range h is 1 and every word is the N64's. */
s32 build_node_matrix(s32 idx, XForm *src, Mtx *out, f32 w, s32 absolute) {
    f32 x, y, z, sc;
    s32 a, b, c, d, h;
    u32 *o = (u32 *)out;

    if (absolute != 0) {
        x = src->pos[0];
        y = src->pos[1];
        z = src->pos[2];
    } else {
        x = src->pos[0] - gCamera[idx].pos[0];
        y = src->pos[1] - gCamera[idx].pos[1];
        z = src->pos[2] - gCamera[idx].pos[2];
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
    o[6] = HI(a, b);
    o[14] = LO(a, b);
    o[7] = HI(c, d);
    o[15] = LO(c, d);
    a = (s32)(src->m[0] * sc);
    b = (s32)(src->m[1] * sc);
    o[0] = HI(a, b);
    o[8] = LO(a, b);
    a = (s32)(src->m[2] * sc);
    o[1] = HI(a, 0);
    o[9] = LO(a, 0);
    a = (s32)(src->m[3] * sc);
    b = (s32)(src->m[4] * sc);
    o[2] = HI(a, b);
    o[10] = LO(a, b);
    a = (s32)(src->m[5] * sc);
    o[3] = HI(a, 0);
    o[11] = LO(a, 0);
    a = (s32)(src->m[6] * sc);
    b = (s32)(src->m[7] * sc);
    o[4] = HI(a, b);
    o[12] = LO(a, b);
    a = (s32)(src->m[8] * sc);
    o[5] = HI(a, 0);
    o[13] = LO(a, 0);
    return 1;
}
