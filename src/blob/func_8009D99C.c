/* flags: -g0 -O3 -mips2 -G 0 -non_shared  (-O2: 170 words differ, saves s0 in an 8-byte frame) */
/*
 * Sibling of func_8009D45C (same body, already matched): build an N64
 * fixed-point Mtx from a float orientation given as three rows of four
 * floats (only columns 0..2 are read; the Mtx gets m[row][col] transposed
 * pairs with a zero fourth column), a translation `pos` (taken absolute, or
 * relative to the view origin D_80150B70[idx].pos) scaled by 2^20 and a
 * homogeneous `w` scaled by 2^16.  Returns 0 without writing when any
 * relative coordinate is outside (-2048, 2048), else 1.
 *
 * Source is func_8009D45C's with the 4-wide row type and a separate position
 * pointer; no quirks beyond that file's (single `sc` local for 65536.0f,
 * a/b/c/d temporaries reused).  func_8009D45C is recorded at -O2; this one
 * only matches at -O3.
 */
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef u32 Mtx[16];
typedef struct {
    u8 pad0[36];
    f32 pos[3];
    u8 pad1[152 - 48];
} Ent;
extern Ent gCamera[];
float sqrtf(float);
#pragma intrinsic (sqrtf)

/* pack two floats into the N64 fixed-point Mtx word pair */
#define HI(a, b) (((u32)(a) & 0xFFFF0000) | ((u32)(b) >> 16))
#define LO(a, b) (((u32)(a) << 16) | ((u32)(b) & 0xFFFF))

#ifdef TARGET_PC
/* PC: video.draw_distance (docs/port/DETAIL.md 2.3). The N64 leaves out a node 2,048 ft or more from the eye on
 * any axis: its place no longer fits the matrix's s15.16 translation, in sixteenths of a foot. Far and max reach
 * further by dividing the whole matrix, its rotation, its place and its w, by a power of two h
 * (pc_draw_range_scale): every vertex comes out h times smaller in x, y, z and w, the same point on the screen with
 * the same z/w, so the same depth and fog. Inside the N64's range h is 1 and every word is the N64's. */
s32 func_8009D99C(s32 idx, f32 (*m)[4], f32 *pos, Mtx *out, f32 w, s32 absolute) {
    f32 x, y, z, sc;
    s32 a, b, c, d, h;
    u32 *o = (u32 *)out;

    if (absolute != 0) {
        x = pos[0];
        y = pos[1];
        z = pos[2];
    } else {
        x = pos[0] - gCamera[idx].pos[0];
        y = pos[1] - gCamera[idx].pos[1];
        z = pos[2] - gCamera[idx].pos[2];
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
    a = (s32)(m[0][0] * sc);
    b = (s32)(m[1][0] * sc);
    o[0] = HI(a, b);
    o[8] = LO(a, b);
    a = (s32)(m[2][0] * sc);
    o[1] = HI(a, 0);
    o[9] = LO(a, 0);
    a = (s32)(m[0][1] * sc);
    b = (s32)(m[1][1] * sc);
    o[2] = HI(a, b);
    o[10] = LO(a, b);
    a = (s32)(m[2][1] * sc);
    o[3] = HI(a, 0);
    o[11] = LO(a, 0);
    a = (s32)(m[0][2] * sc);
    b = (s32)(m[1][2] * sc);
    o[4] = HI(a, b);
    o[12] = LO(a, b);
    a = (s32)(m[2][2] * sc);
    o[5] = HI(a, 0);
    o[13] = LO(a, 0);
    return 1;
}
#else
s32 func_8009D99C(s32 idx, f32 (*m)[4], f32 *pos, Mtx *out, f32 w, s32 absolute) {
    f32 x, y, z, sc;
    s32 a, b, c, d;
    u32 *o = (u32 *)out;

    if (absolute != 0) {
        x = pos[0];
        y = pos[1];
        z = pos[2];
    } else {
        x = pos[0] - gCamera[idx].pos[0];
        y = pos[1] - gCamera[idx].pos[1];
        z = pos[2] - gCamera[idx].pos[2];
    }
    if (x <= -2048.0f || x >= 2048.0f) {
        return 0;
    }
    if (y <= -2048.0f || y >= 2048.0f) {
        return 0;
    }
    if (z <= -2048.0f || z >= 2048.0f) {
        return 0;
    }
    a = (s32)(x * 1048576.0f);
    b = (s32)(y * 1048576.0f);
    c = (s32)(z * 1048576.0f);
    d = (s32)(w * 65536.0f);
    o[6] = HI(a, b);
    o[14] = LO(a, b);
    o[7] = HI(c, d);
    o[15] = LO(c, d);
    sc = 65536.0f;
    a = (s32)(m[0][0] * sc);
    b = (s32)(m[1][0] * sc);
    o[0] = HI(a, b);
    o[8] = LO(a, b);
    a = (s32)(m[2][0] * sc);
    o[1] = HI(a, 0);
    o[9] = LO(a, 0);
    a = (s32)(m[0][1] * sc);
    b = (s32)(m[1][1] * sc);
    o[2] = HI(a, b);
    o[10] = LO(a, b);
    a = (s32)(m[2][1] * sc);
    o[3] = HI(a, 0);
    o[11] = LO(a, 0);
    a = (s32)(m[0][2] * sc);
    b = (s32)(m[1][2] * sc);
    o[4] = HI(a, b);
    o[12] = LO(a, b);
    a = (s32)(m[2][2] * sc);
    o[5] = HI(a, 0);
    o[13] = LO(a, 0);
    return 1;
}
#endif
