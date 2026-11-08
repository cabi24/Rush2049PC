/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitGfx (0x800EE8AC, 8 B deleted-procedure stub) = N64 _InitGfx (GC ngcGraph.c has its own; DC gfx.c
 * func_8C02E18C): counters video_texture_filter/video_color_dither/video_dither_filter/video_divot = 0, FOV HFOV = 90, wait for osTvType
 * (0x80000300), video_set_mode, three 0x22620-byte frame heaps (audio_dma_sync / AllocateMemoryHeapHigh, cleared by
 * audio_loop_control, aligned (+32) & ~63), the two 128-byte frame records frame_records[0..1] (display-list
 * buffers frame_command_buffer_a / frame_command_buffer_b, count 2, heap), display-list pointers frame_dynamic / frame_matrix_cursor (+1728) and the
 * cursor frame_dl (+0x9CC0). Its only caller InitGraphics (= InitGraphics, gfx.c) inlines it across
 * files, so only the stub is emitted. Statement shape from the draft (record stores in field-pair order).
 */
typedef unsigned char u8;
typedef short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
/* PC: the native OSScTask's shape, pointer fields where it has them (N64 0x58 bytes, host 0xA0) */
typedef struct { void *next; u32 state, flags; void *fb; u32 type, tflags; void *boot; u32 boot_size;
                 void *ucode; u32 ucode_size; void *udata; u32 udata_size; void *stack; u32 stack_size;
                 void *out; void *out_size; void *data; u32 data_size; void *yield; u32 yield_size;
                 void *msgQ; void *msg; } PcScTaskShape;
typedef struct {
    PcScTaskShape task;
    u8 *dl_base;           /* 0x58 */
    s16 unk5C;             /* 0x5C */
    u8 pad5E[0x7C - 0x5E];
    uintptr_t buffer;      /* 0x7C */
} SegmentTableEntry;
extern s32 video_texture_filter, video_color_dither, video_dither_filter, video_divot;
extern f32 HFOV;
extern u32 osTvType;     /* osTvType */
extern s32 screen_width, screen_height;
extern uintptr_t zbuffer;
extern u32 framebuffer_a, framebuffer_b;
extern SegmentTableEntry frame_records[];
extern u8 frame_command_buffer_a[], frame_command_buffer_b[];
/* PC: the two frame command buffers hold 16-byte commands, so they are larger than the N64's frame_command_buffer_a /
 * frame_command_buffer_b (docs/port/POINTERS.md), and have room for 12 cars in two views: PC_FRAME_MATRICES matrices from +1728
 * and the display list's PC_GLIST_LEN commands from PC_GLIST_OFFSET, where the N64 has 600 and 4000 from +0x9CC0
 * (pc_game_types.h, docs/port/MORECARS.md 8). */
u8 pc_frame_dl[2][PC_FRAME_BYTES] __attribute__((aligned(16)));
#define DL_BUFFER_0 pc_frame_dl[0]
#define DL_BUFFER_1 pc_frame_dl[1]
extern u8 *frame_dynamic, *frame_matrix_cursor;
extern u8 *frame_dl;           /* 0x801497C8, display-list cursor */
extern u8 *frame_dl_start;
s32 video_mode_base_index(void);
void video_set_mode(s32 tv, s32 mode, s32 width, s32 height);
u32 AllocateMemoryHeap(s32 arg0, s32 size);
u32 AllocateMemoryHeapHigh(s32 arg0, s32 size);
void PurgeableMemory(u32 buf, s32 arg1);
void ZOID_Reset(void);
void InitBlits(void);
void ZeroCameras(void);

void InitGfx(void)
{
    video_texture_filter = 0;
    video_color_dither = 0;
    video_dither_filter = 0;
    video_divot = 0;
    HFOV = 90.0f;
    while (osTvType == 0) {
    }
    video_set_mode(osTvType, video_mode_base_index(), screen_width, screen_height);
    zbuffer = AllocateMemoryHeap(0, 0x22620);
    PurgeableMemory(zbuffer, 0);
    framebuffer_a = AllocateMemoryHeapHigh(0, 0x22620);
    PurgeableMemory(framebuffer_a, 0);
    framebuffer_b = AllocateMemoryHeapHigh(0, 0x22620);
    PurgeableMemory(framebuffer_b, 0);
    zbuffer = (zbuffer + 32) & ~63;
    framebuffer_a = (framebuffer_a + 32) & ~63;
    framebuffer_b = (framebuffer_b + 32) & ~63;
    frame_records[0].buffer = framebuffer_a;
    frame_records[1].buffer = framebuffer_b;
    frame_records[0].unk5C = 2;
    frame_records[1].unk5C = 2;
    frame_records[0].dl_base = DL_BUFFER_0;
    frame_records[1].dl_base = DL_BUFFER_1;
    frame_dynamic = frame_records[0].dl_base;
    frame_matrix_cursor = frame_records[0].dl_base + 1728;
    frame_dl = frame_records[0].dl_base + PC_GLIST_OFFSET;
}
