/* InitSystem (MAT3 Init.c InitSystem): the game's start-up on the main thread, before the game thread starts:
 * the heap, the saves, the graphics, the sound, the controls, the options, the timer and the text. */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;

#define PC_ZEXT(x) ((uintptr_t)PC_N64P((u32)(x)))   /* an RDRAM-window address held as s32: its pointer */

extern s32 osTvType;

VIEW(intptr_t, zbuffer);
#define zbuffer zbuffer_view

/* The frame command buffers (ctx_initsystem.c has their storage and their sizes). */
extern u8 pc_frame_dl[2][PC_FRAME_BYTES];

extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);

/* The video and frame set-up (MAT3 has its own in its graphics layer): no texture filter or dithering options,
 * a 90-degree field of view; waits for libultra to know the TV type and sets the video mode; allocates the
 * z-buffer at the heap's bottom and the two frame buffers at its top, each locked and aligned to 64 bytes; and gives
 * each frame record its frame buffer, its command buffer and its done message (OS_SC_DONE_MSG). The first frame's
 * dynamic data, matrices and display list start in the first command buffer. */
static void _InitGfx_Init(void)
{
    video_texture_filter = 0; video_color_dither = 0; video_dither_filter = 0; video_divot = 0; HFOV = 90.0f;
    while (osTvType == 0) {}
    video_set_mode(osTvType, video_mode_base_index(), screen_width, screen_height);
    zbuffer = (intptr_t)AllocateMemoryHeap(0, FRAME_BUFFER_BYTES); PurgeableMemory(PC_N64A(zbuffer), 0);
    framebuffer_a = (s32)PC_N64A(AllocateMemoryHeapHigh(0, FRAME_BUFFER_BYTES)); PurgeableMemory(framebuffer_a, 0);
    framebuffer_b = (s32)PC_N64A(AllocateMemoryHeapHigh(0, FRAME_BUFFER_BYTES)); PurgeableMemory(framebuffer_b, 0);
    zbuffer = (zbuffer + 32) & ~63; framebuffer_a = (framebuffer_a + 32) & ~63; framebuffer_b = (framebuffer_b + 32) & ~63;
    frame_records[0].colorBuffer = PC_ZEXT(framebuffer_a); frame_records[1].colorBuffer = PC_ZEXT(framebuffer_b);
    frame_records[0].commands = DL_BUFFER_0; frame_records[1].commands = DL_BUFFER_1;
    frame_records[0].doneMsgType = OS_SC_DONE_MSG; frame_records[1].doneMsgType = OS_SC_DONE_MSG;
    frame_dynamic = frame_records[0].commands; frame_matrix_cursor = (u8 *)frame_records[0].commands + 1728;
    frame_dl_start = frame_dl = (u8 *)frame_records[0].commands + PC_GLIST_OFFSET;
}

/* The graphics' start-up (MAT3 InitGraphics): the video and frames, the scene objects, the blits, the cameras. */
static void InitGraphics_Init(void)
{
    _InitGfx_Init();
    ZOID_Init();
    InitBlits();
    InitCameras();
}

/* Without an Expansion Pak, frees the 3D sound tables AudioInit made, as there is no room for them (MAT3
 * FreeBuffers). */
static void FreeBuffers_Init(void)
{
    if (!expansion_pak_present) {
        if (gListeners) {
            DeallocateMemory_Init(PC_N64A(gListeners));
            gListeners = 0;
        }
        if (gEmitters) {
            DeallocateMemory_Init(PC_N64A(gEmitters));
            gEmitters = 0;
        }
    }
}

/* The sound system: 32 voices, 16 effects, 16 music voices, 5 listeners, 64 emitters, a foot a unit, the front 200
 * units away, no Doppler. */
static void AudioInit_Init(void)
{
    AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
}

/* The game's start-up (main_thread_entry calls it before it starts the game thread): the heap (50 master slots, 10
 * more each time they run out), the saves, the graphics, the sound, the controls (no controller active yet), the
 * options, the frame timer and the text. */
void InitSystem(void)
{
    InitMemory(50, 10);
    SaveInit();
    InitGraphics_Init();
    AudioInit_Init();
    FreeBuffers_Init();
    InitRushControls();
    SetControllerActive(-1, 0);
    InitOptions();
    InitializeTimer();
    HAL_InitText_Init();
}
