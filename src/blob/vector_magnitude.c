/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
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
f32 vector_magnitude(f32 *v) {
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
#define car ((u8 *)arg0)
#define CF(off) (*(f32 *)(car + (off)))
#define CB(off) (*(s8 *)(car + (off)))
#define OF(off) (*(f32 *)(gCamera[idx] + (off)))
#define A1(off) (*(f32 *)((u8 *)arg1 + (off)))

