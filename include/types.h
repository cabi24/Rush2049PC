/**
 * @file types.h
 * @brief Common type definitions for Rush 2049 N64
 *
 * Mirrors arcade source conventions where possible.
 * See reference/repos/rushtherock/game/typedefs.h
 */

#ifndef TYPES_H
#define TYPES_H

/* Basic integer types */
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

/* Pointer-sized integer types: uintptr_t and intptr_t come from port/include/pc_prelude.h */

/* Volatile types (for hardware registers) */
typedef volatile u8  vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8  vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

/* N64 Graphics types (from libultra) */
typedef union {
    struct {
        u32 w0;
        u32 w1;
    } words;
    u64 force_structure_alignment;
} Gfx;
typedef u32 Mtx[4][4];

/* Arcade-style type definitions */
typedef f32 F32;          /* 32-bit float */
typedef s32 S32;          /* Signed 32-bit */
typedef s16 S16;          /* Signed 16-bit */
typedef s8  S08;          /* Signed 8-bit */
typedef u32 U32;          /* Unsigned 32-bit */
typedef u16 U16;          /* Unsigned 16-bit */
typedef u8  U08;          /* Unsigned 8-bit */

/* Boolean */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef s32 BOOL;

/* Vector types */
typedef f32 Vec3f[3];
typedef s16 Vec3s[3];
typedef s32 Vec3i[3];

/* Matrix types */
typedef f32 Mat3f[3][3];
typedef f32 Mat4f[4][4];

/* Fixed-point math (arcade uses 0x4000 = 1.0 scale) */
#define FRAC_BITS 14
#define FRAC_ONE (1 << FRAC_BITS)  /* 0x4000 = 16384 */

/* Common null pointer */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* N64 OS constants */
#define OS_READ  0
#define OS_WRITE 1
#define OS_MESG_NOBLOCK 0
#define OS_MESG_BLOCK   1

/* Controller Pak (PFS) constants - see include/PR/os_pfs.h for full definitions */
/* Basic constants kept here for compatibility */
#ifndef _OS_PFS_H_
#define PFS_READ  0
#define PFS_WRITE 1
#define PFS_ERR_INVALID      5   /* Invalid parameter */
#define PFS_INITIALIZED      0x01
#define PFS_BLOCKSIZE        32
#define PFS_ONE_PAGE         8
#endif
#define PFS_LABEL_SIZE      28

/* Float matrix type alias */
typedef f32 MtxF[4][4];

/* N64 Controller Pak (PFS) - see include/PR/os_pfs.h for full definition */
/* Forward declaration only - typedef is in os_pfs.h */
#ifndef _OS_PFS_H_
struct OSPfs;
#endif

/* N64 OS timer - defined in include/PR/os_time.h */
/* Include os_time.h for OSTimer definition */

/* N64 RSP task type constants */
#define M_GFXTASK   1
#define M_AUDTASK   2
#define M_VIDTASK   3
#define M_HVQTASK   4

#define SP_UCODE_DATA_SIZE 0x800

/* N64 OS Message Priority */
#define OS_MESG_PRI_NORMAL 0
#define OS_MESG_PRI_HIGH   1

/* N64 I/O Message - defined in include/PR/os_pi.h */
/* Include os_pi.h for OSIoMesg definition */

/* External RSP boot/ucode symbols */
extern u8 rspbootTextStart[], rspbootTextEnd[];
extern u8 gspF3DEX2_fifoTextStart[], gspF3DEX2_fifoTextEnd[];
extern u8 gspF3DEX2_fifoDataStart[], gspF3DEX2_fifoDataEnd[];

/* N64 OSTask structure for RSP tasks */
typedef struct {
    u32 type;                /* Task type (M_GFXTASK, M_AUDTASK, etc.) */
    u32 flags;               /* Task flags */
    u64 *ucode_boot;         /* RSP boot microcode */
    u32 ucode_boot_size;     /* Boot microcode size */
    u64 *ucode;              /* RSP microcode */
    u32 ucode_size;          /* Microcode size */
    u64 *ucode_data;         /* Microcode data */
    u32 ucode_data_size;     /* Data size */
    u64 *dram_stack;         /* DRAM stack */
    u32 dram_stack_size;     /* Stack size */
    u64 *output_buff;        /* Output buffer */
    u64 *output_buff_size;   /* Output buffer size */
    u64 *data_ptr;           /* Data pointer */
    u32 data_size;           /* Data size */
    u64 *yield_data_ptr;     /* Yield data pointer */
    u32 yield_data_size;     /* Yield data size */
} OSTask_t;

typedef struct {
    OSTask_t t;
} OSTask;

#endif /* TYPES_H */
