/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitGraphics (0x800EE8B4, 456 B; historical label) = InitGraphics@gfx.c (MAT3 GC ngcGraph.c / DC gfx.c
 * InitGraphics 8C02E3EE): _InitGfx(); <display-list end = cursor>; ZOID_Init (ZOID_Reset); InitBlits
 * (func_800A5488); InitCameras (ZeroCameras). No references anywhere: the kept out-of-line copy of a
 * global whose only caller, InitSystem (InitSystem, heap_release/initsystem.c), inlines it.
 * _InitGfx is the N64 platform body (InitGfx, initgfx.c) and lives in ANOTHER source file: umerge's
 * cross-file inline leaves as1 block boundaries where the inlined file's code starts and ends. That is what retail
 * shows (prologue alone in the entry block, so the swc1 does not fill the bnez slot; the frame_dl store and its
 * reload for frame_dl_start in different blocks). proved both residuals were as1 block boundaries (asm.sh with
 * two hand-inserted labels) but no single-file source form made them; two files do, with no shaping.
 * _InitGfx's deleted-procedure stub is the caller-less locked stub InitGfx right before this function
 * (post-order: callee first).
 */
typedef unsigned char u8;
typedef short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct {
    u8 pad00[0x58];
    u8 *dl_base;           /* 0x58 */
    s16 unk5C;             /* 0x5C */
    u8 pad5E[0x7C - 0x5E];
    u32 buffer;            /* 0x7C */
} SegmentTableEntry;
extern s32 video_texture_filter, video_color_dither, video_dither_filter, video_divot;
extern f32 HFOV;
extern u32 osTvType;     /* osTvType */
extern s32 screen_width, screen_height;
extern u32 zbuffer, framebuffer_a, framebuffer_b;
extern SegmentTableEntry frame_records[];
extern u8 frame_command_buffer_a[], frame_command_buffer_b[];
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

void InitGfx(void);   /* _InitGfx (initgfx.c) */

void InitGraphics(void)
{
    InitGfx();
    frame_dl_start = frame_dl;
    ZOID_Reset();            /* ZOID_Init */
    InitBlits();            /* InitBlits */
    ZeroCameras();    /* InitCameras */
}
