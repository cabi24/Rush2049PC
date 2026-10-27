/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_800EE8AC (0x800EE8AC, 8 B deleted-procedure stub) = N64 _InitGfx (GC ngcGraph.c has its own; DC gfx.c
 * func_8C02E18C): counters D_8015F738/D_80161380/D_80161398/D_801613A4 = 0, FOV D_80154188 = 90, wait for osTvType
 * (0x80000300), viewport_setup, three 0x22620-byte frame heaps (audio_dma_sync / sound_play_menu, cleared by
 * audio_loop_control, aligned (+32) & ~63), the two 128-byte frame records D_80156BE0[0..1] (display-list
 * buffers D_800586D0 / D_8006A090, count 2, heap), display-list pointers D_8015B250 / D_8015B260 (+1728) and the
 * cursor D_801497C8 (+0x9CC0). Its only caller world_effect_update (= InitGraphics, gfx.c) inlines it across
 * files, so only the stub is emitted. Statement shape from the draft (record stores in field-pair order).
 */
typedef unsigned char u8;
typedef short s16;
typedef signed int s32;
typedef unsigned int u32;
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
#ifdef TARGET_PC
/* PC: the native OSScTask's shape, pointer fields where it has them (N64 0x58 bytes, host 0xA0) */
typedef struct { void *next; u32 state, flags; void *fb; u32 type, tflags; void *boot; u32 boot_size;
                 void *ucode; u32 ucode_size; void *udata; u32 udata_size; void *stack; u32 stack_size;
                 void *out; void *out_size; void *data; u32 data_size; void *yield; u32 yield_size;
                 void *msgQ; void *msg; } PcScTaskShape;
#endif
typedef struct {
#ifdef TARGET_PC
    PcScTaskShape task;
#else
    u8 pad00[0x58];
#endif
    u8 *dl_base;           /* 0x58 */
    s16 unk5C;             /* 0x5C */
    u8 pad5E[0x7C - 0x5E];
    uintptr_t buffer;      /* 0x7C */
} SegmentTableEntry;
extern s32 D_8015F738, D_80161380, D_80161398, D_801613A4;
extern f32 D_80154188;
extern u32 D_80000300;     /* osTvType */
extern s32 D_8002AFC0, D_8002AFC4;
#ifdef TARGET_PC
extern uintptr_t D_8002EBB0;
extern u32 D_80156CEC, D_80157240;
#else
extern u32 D_8002EBB0, D_80156CEC, D_80157240;
#endif
extern SegmentTableEntry D_80156BE0[];
extern u8 D_800586D0[], D_8006A090[];
#ifdef TARGET_PC
/* PC: the two frame command buffers, 0x9CC0 bytes of matrices and vertices then 4000 Gfx, hold 16-byte
 * commands, so they are larger than the N64's D_800586D0 / D_8006A090 (docs/port/POINTERS.md). */
u8 pc_frame_dl[2][0x9CC0 + 4000 * 16] __attribute__((aligned(16)));
#define DL_BUFFER_0 pc_frame_dl[0]
#define DL_BUFFER_1 pc_frame_dl[1]
#else
#define DL_BUFFER_0 D_800586D0
#define DL_BUFFER_1 D_8006A090
#endif
extern u8 *D_8015B250, *D_8015B260;
extern u8 *D_801497C8;           /* 0x801497C8, display-list cursor */
extern u8 *D_801497F4;
s32 get_tv_offset(void);
void viewport_setup(s32 tv, s32 mode, s32 width, s32 height);
u32 AllocateMemoryHeap(s32 arg0, s32 size);
u32 sound_play_menu(s32 arg0, s32 size);
void PurgeableMemory(u32 buf, s32 arg1);
void func_800A5B3C(void);
void InitBlits(void);
void particle_velocity_set(void);

void func_800EE8AC(void)
{
    D_8015F738 = 0;
    D_80161380 = 0;
    D_80161398 = 0;
    D_801613A4 = 0;
    D_80154188 = 90.0f;
    while (D_80000300 == 0) {
    }
    viewport_setup(D_80000300, get_tv_offset(), D_8002AFC0, D_8002AFC4);
    D_8002EBB0 = AllocateMemoryHeap(0, 0x22620);
    PurgeableMemory(D_8002EBB0, 0);
    D_80156CEC = sound_play_menu(0, 0x22620);
    PurgeableMemory(D_80156CEC, 0);
    D_80157240 = sound_play_menu(0, 0x22620);
    PurgeableMemory(D_80157240, 0);
    D_8002EBB0 = (D_8002EBB0 + 32) & ~63;
    D_80156CEC = (D_80156CEC + 32) & ~63;
    D_80157240 = (D_80157240 + 32) & ~63;
    D_80156BE0[0].buffer = D_80156CEC;
    D_80156BE0[1].buffer = D_80157240;
    D_80156BE0[0].unk5C = 2;
    D_80156BE0[1].unk5C = 2;
    D_80156BE0[0].dl_base = DL_BUFFER_0;
    D_80156BE0[1].dl_base = DL_BUFFER_1;
    D_8015B250 = D_80156BE0[0].dl_base;
    D_8015B260 = D_80156BE0[0].dl_base + 1728;
    D_801497C8 = D_80156BE0[0].dl_base + 0x9CC0;
}
