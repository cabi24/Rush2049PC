/* InitGfx: the graphics' video and frame set-up, as a global function. Nothing calls it on the PC: InitSystem
 * uses its own copy (heap_release/initsystem.c's _InitGfx_Init, which also sets frame_dl_start). */
#include "game/game.h"

extern u32 osTvType;

VIEW(uintptr_t, zbuffer);
#define zbuffer zbuffer_view
VIEW(u32, framebuffer_a);
#define framebuffer_a framebuffer_a_view
VIEW(u32, framebuffer_b);
#define framebuffer_b framebuffer_b_view

/* The frame command buffers (ctx_initsystem.c has their storage and their sizes). */
extern u8 pc_frame_dl[2][PC_FRAME_BYTES];
#define DL_BUFFER_0 pc_frame_dl[0]
#define DL_BUFFER_1 pc_frame_dl[1]
VIEW(u8 *, frame_dynamic);
#define frame_dynamic frame_dynamic_view
VIEW(u8 *, frame_dl);
#define frame_dl frame_dl_view

/* The frame buffers' and z-buffer's size: 320 x 220 16-bit pixels, and 32 bytes to align them to 64. */
#define FRAME_BUFFER_BYTES 0x22620

/* As initsystem.c's _InitGfx_Init: the video mode, the z-buffer and frame buffers, the frame records. */
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
    zbuffer = AllocateMemoryHeap(0, FRAME_BUFFER_BYTES);
    PurgeableMemory(zbuffer, 0);
    framebuffer_a = AllocateMemoryHeapHigh(0, FRAME_BUFFER_BYTES);
    PurgeableMemory(framebuffer_a, 0);
    framebuffer_b = AllocateMemoryHeapHigh(0, FRAME_BUFFER_BYTES);
    PurgeableMemory(framebuffer_b, 0);
    zbuffer = (zbuffer + 32) & ~63;
    framebuffer_a = (framebuffer_a + 32) & ~63;
    framebuffer_b = (framebuffer_b + 32) & ~63;
    frame_records[0].colorBuffer = framebuffer_a;
    frame_records[1].colorBuffer = framebuffer_b;
    frame_records[0].doneMsgType = OS_SC_DONE_MSG;
    frame_records[1].doneMsgType = OS_SC_DONE_MSG;
    frame_records[0].commands = DL_BUFFER_0;
    frame_records[1].commands = DL_BUFFER_1;
    frame_dynamic = frame_records[0].commands;
    frame_matrix_cursor = frame_records[0].commands + 1728;
    frame_dl = frame_records[0].commands + PC_GLIST_OFFSET;
}
