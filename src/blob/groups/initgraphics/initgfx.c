#include "game/game.h"

typedef struct {
    PcScTaskShape task;
    u8 *dl_base;           /* 0x58 */
    s16 unk5C;             /* 0x5C */
    u8 pad5E[0x7C - 0x5E];
    uintptr_t buffer;      /* 0x7C */
} SegmentTableEntry;

extern u32 osTvType;

VIEW(uintptr_t, zbuffer);
#define zbuffer zbuffer_view
VIEW(u32, framebuffer_a);
#define framebuffer_a framebuffer_a_view
VIEW(u32, framebuffer_b);
#define framebuffer_b framebuffer_b_view
VIEW(SegmentTableEntry[], frame_records);
#define frame_records frame_records_view

/* PC: the two frame command buffers hold 16-byte commands, so they are larger than the N64's frame_command_buffer_a /
 * frame_command_buffer_b (docs/port/POINTERS.md), and have room for 12 cars in two views: PC_FRAME_MATRICES matrices from +1728
 * and the display list's PC_GLIST_LEN commands from PC_GLIST_OFFSET, where the N64 has 600 and 4000 from +0x9CC0
 * (pc_game_types.h, docs/port/MORECARS.md 8). */
u8 pc_frame_dl[2][PC_FRAME_BYTES] __attribute__((aligned(16)));
#define DL_BUFFER_0 pc_frame_dl[0]
#define DL_BUFFER_1 pc_frame_dl[1]
VIEW(u8 *, frame_dynamic);
#define frame_dynamic frame_dynamic_view
VIEW(u8 *, frame_dl);
#define frame_dl frame_dl_view

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
