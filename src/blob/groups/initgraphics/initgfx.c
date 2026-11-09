/* InitGfx: the graphics' video and frame set-up, as a global function. Nothing calls it on the PC: InitSystem
 * uses its own copy (heap_release/initsystem.c's _InitGfx_Init, which also sets frame_dl_start). */
#include "game/game.h"

extern u32 osTvType;

/* A buffer's start in its block: the next 64-byte boundary (as initsystem.c's ALIGN_64). */
#define ALIGN_64(p) ((void *)(((uintptr_t)(p) + 32) & ~(uintptr_t)63))

/* The frame command buffers (initsystem_start.c has their storage and their sizes). */
extern u8 pc_frame_dl[2][PC_FRAME_BYTES];
VIEW(u8 *, frame_dynamic);
#define frame_dynamic frame_dynamic_view
VIEW(u8 *, frame_dl);
#define frame_dl frame_dl_view

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
    zbuffer = ALIGN_64(zbuffer);
    framebuffer_a = ALIGN_64(framebuffer_a);
    framebuffer_b = ALIGN_64(framebuffer_b);
    frame_records[0].colorBuffer = (uintptr_t)framebuffer_a;
    frame_records[1].colorBuffer = (uintptr_t)framebuffer_b;
    frame_records[0].doneMsgType = OS_SC_DONE_MSG;
    frame_records[1].doneMsgType = OS_SC_DONE_MSG;
    frame_records[0].commands = DL_BUFFER_0;
    frame_records[1].commands = DL_BUFFER_1;
    frame_dynamic = frame_records[0].commands;
    frame_matrix_cursor = frame_records[0].commands + 1728;
    frame_dl = frame_records[0].commands + PC_GLIST_OFFSET;
}
