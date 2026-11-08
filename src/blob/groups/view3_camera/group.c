/* flags: -g0 -O2 -mips2 -G 0 -non_shared ; best hand seed, 732/762 words, NOT a match */

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;
typedef union 
{
  struct 
  {
    u32 w0;
    uintptr_t w1;
  } words;
  u64 force_structure_alignment;
} Gfx;
typedef u32 Mtx[4][4];
typedef f32 F32;
typedef s32 S32;
typedef s16 S16;
typedef s8 S08;
typedef u32 U32;
typedef u16 U16;
#define NULL ((void*)0)
/*
 * This header contains macros emitted by m2c in "valid syntax" mode,
 * which can be enabled by passing `--valid-syntax` on the command line.
 *
 * In this mode, unhandled types and expressions are emitted as macros so
 * that the output is compilable without human intervention.
 */

#ifndef M2C_MACROS_H
#define M2C_MACROS_H

/* Unknown types */
typedef s32 M2C_UNK;
typedef s8  M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;

/* Unknown field access, like `*(type_ptr) &expr->unk_offset` */
#define M2C_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8 *)(expr) + (offset)))

/* Bitwise (reinterpret) cast */
#define M2C_BITWISE(type, expr) ((type)(expr))

/* Unaligned reads */
#define M2C_LWL(expr) (expr)
#define M2C_FIRST3BYTES(expr) (expr)
#define M2C_UNALIGNED32(expr) (expr)

/* Unhandled instructions */
#define M2C_ERROR(desc) (0)
#define M2C_TRAP_IF(cond) (0)
#define M2C_BREAK() (0)
#define M2C_SYNC() (0)

#define GLUE_F64(a, b) (0.0)
#define MULT_HI(a, b) (0)
#define MULTU_HI(a, b) (0)
#define DMULT_HI(a, b) (0)
#define DMULTU_HI(a, b) (0)
#define CLZ(x) (0)
#define REVERSE_BITS(x) (0)
#define ROTATE_RIGHT(x, shift) (0)
#define ARM_RRX(x, carry) (0)
#define BSWAP32(x) (0)
#define BSWAP16(x) (0)
#define BSWAP16X2(x) (0)

/* Carry/overflow bits from partially-implemented instructions */
#define M2C_CARRY 0
#define M2C_OVERFLOW(a) (0)

/* Memcpy patterns */
#define M2C_MEMCPY_ALIGNED memcpy
#define M2C_MEMCPY_UNALIGNED memcpy
#define M2C_STRUCT_COPY memcpy

#endif
float sqrtf(float);
#pragma intrinsic (sqrtf)
extern f32 rear_z[];
extern f32 rear_y[];
extern s8 view3_switch[];
extern s8 gTireCnt[];
extern f32 gSusAvg[];
extern f32 zf[];
extern f32 yf[];
extern f32 last_zvec[][3];
extern f32 last_yvec[][3];
extern u8 gCamera[][0x98];
extern f32 uvector[3];
extern f32 rvector[3];
extern volatile f32 gFrameSeconds;
extern f32 view3_speed_stretch, view3_elastic_range, view3_elastic_max, view3_elastic_min, view3_yf_rate_range, view3_yf_rate_base, view3_zvec_min_len, view3_zvec_elastic_min_len, view3_yvec_min_len, view3_yvec_elastic_min_len, view3_xvec_min_len;
extern s32 gRace;


extern void CopyUV(void *, void *);
extern void ZOID_UpdateObject(s16, void *, void *);
extern f32 invmagnitude(f32 *);
extern f32 FSCLS_from_one[];
extern s8 gFrameScale;
f32 vector_magnitude(f32 *v);
void vector_direction(f32 *a, f32 *b);
void Interpolate(f32 *a, f32 *b, s16 n, f32 lo, f32 hi, f32 t, f32 *out) {
    f32 k;
    s16 i;
    k = hi - lo;
    if (hi != lo) {
        k = (t - lo) / k;
    }
    for (i = 0; i < n; i++) {
        out[i] = (b[i] - a[i]) * k + a[i];
    }
}
void update_car_object(void *car, void *vel, void *mat) {
    f32 m[9];
    CopyUV(mat, m);
    if (gFrameScale != 0) {
        m[1] *= FSCLS_from_one[gFrameScale];
        m[4] *= FSCLS_from_one[gFrameScale];
        m[7] *= FSCLS_from_one[gFrameScale];
    }
    if (*(s32 *)((u8 *)car + 244) >= 0) {
        ZOID_UpdateObject(*(s32 *)((u8 *)car + 244), vel, m);
    }
}
#define car ((u8 *)arg0)
#define CF(off) (*(f32 *)(car + (off)))
#define CB(off) (*(s8 *)(car + (off)))
/* arg0 is a car (update_camera): its fields past the visuals are at CAR_OFS(n) on the PC (docs/port/CARS.md 2.3). */
#define OF(off) (*(f32 *)(gCamera[idx] + (off)))
#define A1(off) (*(f32 *)((u8 *)arg1 + (off)))
void View3Cam(void *arg0, void *arg1) {
    s32 idx;
    f32 len;
    f32 sp94;
    f32 sp90;
    s32 i;
    s32 unused;
    f32 sp84;
    f32 f2;
    f32 sp74[3];
    f32 sp68[3];
    f32 sp5C[3];
    f32 sp50[3];
    f32 sp44[3];
    f32 f12;

    idx = CB(CAR_OFS(0x35C));
    sp94 = rear_z[idx];
    sp90 = rear_y[idx];
    update_car_object(arg0, arg1, car + 0x50);
    if (CB(CAR_OFS(0x35D)) == 3) {
        sp94 -= 6.0f;
        sp90 -= 2.0f;
    }
    len = vector_magnitude((f32 *)(car + 0x14));
    if (len > 100.0f) {
        f2 = 0.25f;
    } else {
        f2 = len * view3_speed_stretch;
    }
    sp84 = sqrtf(sp94 * sp94 + sp90 * sp90) * (1.0f + f2);
    sp94 *= 1.0f + f2;
    if (view3_switch[idx] != 0 || gRace == 6) {
        f12 = 0.0;
    } else if (len < 140.0f) {
        f12 = PC_FILTER(view3_elastic_max - (len / 140.0f) * view3_elastic_range);
    } else {
        f12 = PC_FILTER(view3_elastic_min);
    }
    view3_switch[idx] = 0;
    if (gTireCnt[idx] >= 3 && gSusAvg[idx] > 0.5f) {
        zf[idx] = zf[idx] + gFrameSeconds;
    } else if (len < 30.0f) {
        zf[idx] += gFrameSeconds * 10.0f;
    } else {
        zf[idx] = zf[idx] - gFrameSeconds;
    }
    if (zf[idx] < 0.0f) {
        zf[idx] = 0.0f;
    } else if (zf[idx] > 1.0f) {
        zf[idx] = 1.0f;
    }
    if (gTireCnt[idx] < 3) {
        yf[idx] -= gFrameSeconds;
    } else {
        if (gSusAvg[idx] > 1.5f) {
            yf[idx] += gFrameSeconds * 10.0f;
        } else if (gSusAvg[idx] > 0.5f) {
            yf[idx] += gFrameSeconds / (view3_yf_rate_base + (1.5f - gSusAvg[idx]) * view3_yf_rate_range);
        } else {
            yf[idx] += gFrameSeconds;
        }
    }
    if (yf[idx] < 0.0f) {
        yf[idx] = 0.0f;
    } else if (yf[idx] > 1.0f) {
        yf[idx] = 1.0f;
    }
    for (i = 0; i < 3; i++) {
        sp74[i] = -CF(0x44 + i * 4);
    }
    if (zf[idx] < 1.0f) {
        if (len < 1.0f) {
            sp68[0] = sp74[0];
            sp68[1] = sp74[1];
            sp68[2] = sp74[2];
        } else if (CF(0xAC) < 0 && gTireCnt[idx] >= 3) {
            sp68[0] = CF(0x14) * (1 / len);
            sp68[1] = CF(0x18) * (1 / len);
            sp68[2] = CF(0x1C) * (1 / len);
        } else {
            sp68[0] = CF(0x14) * (-1 / len);
            sp68[1] = CF(0x18) * (-1 / len);
            sp68[2] = CF(0x1C) * (-1 / len);
        }
    }
    if (zf[idx] == 0.0f) {
        sp5C[0] = sp68[0]; sp5C[1] = sp68[1]; sp5C[2] = sp68[2];
    } else if (zf[idx] == 1.0f) {
        sp5C[0] = sp74[0]; sp5C[1] = sp74[1]; sp5C[2] = sp74[2];
    } else {
        Interpolate(sp68, sp74, 3, 0.0f, 1.0f, zf[idx], sp5C);
        len = vector_magnitude(sp5C);
        if (len < view3_zvec_min_len) {
            sp5C[0] = sp74[0]; sp5C[1] = sp74[1]; sp5C[2] = sp74[2];
        } else {
            sp5C[0] *= (1.0f / len); sp5C[1] *= (1.0f / len); sp5C[2] *= (1.0f / len);
        }
    }
    if (f12 > 0) {
        sp5C[0] *= (1.0f - f12); sp5C[1] *= (1.0f - f12); sp5C[2] *= (1.0f - f12);
        sp5C[0] += last_zvec[idx][0] * f12;
        sp5C[1] += last_zvec[idx][1] * f12;
        sp5C[2] += last_zvec[idx][2] * f12;
        len = vector_magnitude(sp5C);
        if (len < view3_zvec_elastic_min_len) {
            sp5C[0] = sp74[0]; sp5C[1] = sp74[1]; sp5C[2] = sp74[2];
        } else {
            sp5C[0] *= (1.0f / len); sp5C[1] *= (1.0f / len); sp5C[2] *= (1.0f / len);
        }
    }
    last_zvec[idx][0] = sp5C[0];
    last_zvec[idx][1] = sp5C[1];
    last_zvec[idx][2] = sp5C[2];
    for (i = 0; i < 3; i++) {
        sp74[i] = CF(0x38 + i * 4);
    }
    if (yf[idx] < 1.0f) {
        sp68[0] = uvector[0];
        sp68[1] = uvector[1];
        sp68[2] = uvector[2];
    }
    if (yf[idx] == 0) {
        sp50[0] = sp68[0]; sp50[1] = sp68[1]; sp50[2] = sp68[2];
    } else if (yf[idx] == 1.0f) {
        sp50[0] = sp74[0]; sp50[1] = sp74[1]; sp50[2] = sp74[2];
    } else {
        Interpolate(sp68, sp74, 3, 0, 1.0f, yf[idx], sp50);
        len = vector_magnitude(sp50);
        if (len < view3_yvec_min_len) {
            sp50[0] = sp74[0]; sp50[1] = sp74[1]; sp50[2] = sp74[2];
        } else {
            sp50[0] *= (1.0f / len); sp50[1] *= (1.0f / len); sp50[2] *= (1.0f / len);
        }
    }
    if (f12 > 0) {
        sp50[0] *= (1.0f - f12); sp50[1] *= (1.0f - f12); sp50[2] *= (1.0f - f12);
        sp50[0] += last_yvec[idx][0] * f12;
        sp50[1] += last_yvec[idx][1] * f12;
        sp50[2] += last_yvec[idx][2] * f12;
        len = vector_magnitude(sp50);
        if (len < view3_yvec_elastic_min_len) {
            sp50[0] = sp74[0]; sp50[1] = sp74[1]; sp50[2] = sp74[2];
        } else {
            sp50[0] *= (1.0f / len); sp50[1] *= (1.0f / len); sp50[2] *= (1.0f / len);
        }
    }
    last_yvec[idx][0] = sp50[0];
    last_yvec[idx][1] = sp50[1];
    last_yvec[idx][2] = sp50[2];
    sp44[0] = sp5C[0] * sp94; sp44[1] = sp5C[1] * sp94; sp44[2] = sp5C[2] * sp94;
    sp44[0] += sp50[0] * sp90;
    sp44[1] += sp50[1] * sp90;
    sp44[2] += sp50[2] * sp90;
    vector_direction(sp44, gCamera[idx] + 0x78);
    sp44[0] = OF(0x78) * sp84;
    sp44[1] = OF(0x7C) * sp84;
    sp44[2] = OF(0x80) * sp84;
    if (CB(CAR_OFS(0x35D)) == 2) {
        sp44[0] += sp50[0] * 4.0f;
        sp44[1] += sp50[1] * 4.0f;
        sp44[2] += sp50[2] * 4.0f;
    } else if (CB(CAR_OFS(0x35D)) == 3) {
        sp44[0] += sp50[0] * 4.0f;
        sp44[1] += sp50[1] * 4.0f;
        sp44[2] += sp50[2] * 4.0f;
    }
    OF(0x84) = A1(0) + sp44[0];
    OF(0x88) = A1(4) + sp44[1];
    OF(0x8C) = A1(8) + sp44[2];
    OF(0x78) *= -1.0f;
    OF(0x7C) *= -1.0f;
    OF(0x80) *= -1.0f;
    OF(0x60) = sp50[1] * OF(0x80) - sp50[2] * OF(0x7C);
    OF(0x64) = sp50[2] * OF(0x78) - sp50[0] * OF(0x80);
    OF(0x68) = sp50[0] * OF(0x7C) - sp50[1] * OF(0x78);
    len = vector_magnitude((f32 *)(gCamera[idx] + 0x60));
    if (len < view3_xvec_min_len) {
        OF(0x60) = rvector[0];
        OF(0x64) = rvector[1];
        OF(0x68) = rvector[2];
    } else {
        OF(0x60) *= (1.0f / len);
        OF(0x64) *= (1.0f / len);
        OF(0x68) *= (1.0f / len);
    }
    OF(0x6C) = OF(0x7C) * OF(0x68) - OF(0x80) * OF(0x64);
    OF(0x70) = OF(0x80) * OF(0x60) - OF(0x78) * OF(0x68);
    OF(0x74) = OF(0x78) * OF(0x64) - OF(0x7C) * OF(0x60);
}

