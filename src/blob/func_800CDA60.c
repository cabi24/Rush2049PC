/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef signed char s8;
typedef int s32;
typedef struct { u8 pad[1860]; s8 tab[1]; } Inner;
#ifdef TARGET_PC
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Inner **inner; } Mid;
#else
typedef struct { u8 pad[44]; Inner **inner; } Mid;
#endif
typedef struct { Mid *mid; } Outer;
s8 func_800CDA60(Outer *a0, u8 a1, u8 a2) {
    return (*a0->mid->inner)->tab[a1 * 16 + a2];
}
