/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;
typedef int s32;
typedef struct { u8 pad[76]; u8 b76; u8 pad1[19]; u8 b96; u8 pad2[15]; u8 b112; u8 pad3[19]; u8 b132; } Inner;
#ifdef TARGET_PC
typedef struct { void *next; void *prev; void *fileh; u8 pad_C[0x2C - 0xC]; Inner **inner; } Mid;
#else
typedef struct { u8 pad[44]; Inner **inner; } Mid;
#endif
typedef struct { Mid *mid; } Outer;
u8 func_800CDC3C(Outer *a0, s32 a1) {
    switch (a1) {
    case 6: return (*a0->mid->inner)->b96;
    case 4: return (*a0->mid->inner)->b112;
    case 5: return (*a0->mid->inner)->b132;
    default: return (*a0->mid->inner)->b76;
    }
}
