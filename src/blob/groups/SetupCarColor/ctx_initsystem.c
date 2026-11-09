#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */
#define Segs SegsView   /* this file's own Segs: include/game's Segs is another type */

typedef struct { u32 opaque[6]; } OSMesgQueue;

typedef struct {
    PcScTaskShape task0;
    void *buf88;        /* 88 */
    s16 n92;            /* 92 */
    u8 other94[30];
    uintptr_t f124;     /* 124: the color buffer, pointer-sized */
    PcScTaskShape task1;
    void *buf216;       /* 216 */
    s16 n220;           /* 220 */
    u8 other222[30];
    uintptr_t f252;     /* 252 */
} Segs;
#define PC_ZEXT(x) ((u32)(x))   /* an RDRAM-window address held as s32 */
VIEW(OSMesgQueue, text_lock_queue);
#define text_lock_queue text_lock_queue_view

VIEW(OSMesgQueue, heap_lock_queue);
#define heap_lock_queue heap_lock_queue_view

extern s32 osTvType;

VIEW(intptr_t, zbuffer);
#define zbuffer zbuffer_view

VIEW(Segs, frame_records);
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

VIEW(void *, gEmitters);
#define gEmitters gEmitters_view

extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);

/* Their manifest owners are empty deleted-procedure stubs (src/blob/heap_lock.c, heap_unlock.c): the
 * bodies get written in place (docs/port/PROTOTYPES.md). */
#define heap_lock() osRecvMesg(&heap_lock_queue, 0, 1)
#define heap_unlock() osSendMesg(&heap_lock_queue, 0, 0)

static void _InitSemaphore_Init(void)     /* MAT3 _InitSemaphore@HAL_text.c, N64 form */
{
    static s8 sInited = 0; if (!sInited) { sInited = 1; osCreateMesgQueue(&text_lock_queue, text_lock_msg, 1); osSendMesg(&text_lock_queue, (OSMesg)0, 0); }
}

void HAL_InitText_Init(void)     /* MAT3 HAL_InitText@HAL_text.c */
{
    _InitSemaphore_Init();
    gFontIndex = -1;
}

static void _InitGfx_Init(void)     /* N64 _InitGfx (GC ngcGraph.c has its own) */
{
    video_texture_filter = 0; video_color_dither = 0; video_dither_filter = 0; video_divot = 0; HFOV = 90.0f; while (osTvType == 0) {} video_set_mode(osTvType, video_mode_base_index(), screen_width, screen_height); zbuffer = (intptr_t)AllocateMemoryHeap(0, 0x22620); PurgeableMemory(zbuffer, 0); framebuffer_a = (s32)AllocateMemoryHeapHigh(0, 0x22620); PurgeableMemory(framebuffer_a, 0); framebuffer_b = (s32)AllocateMemoryHeapHigh(0, 0x22620); PurgeableMemory(framebuffer_b, 0); zbuffer = (zbuffer + 32) & ~63; framebuffer_a = (framebuffer_a + 32) & ~63; framebuffer_b = (framebuffer_b + 32) & ~63; frame_records.f124 = PC_ZEXT(framebuffer_a); frame_records.f252 = PC_ZEXT(framebuffer_b); frame_records.buf88 = DL_BUFFER_0; frame_records.buf216 = DL_BUFFER_1; frame_records.n92 = 2; frame_records.n220 = 2; frame_dynamic = frame_records.buf88; frame_matrix_cursor = (u8 *)frame_records.buf88 + 1728; frame_dl_start = frame_dl = (u8 *)frame_records.buf88 + PC_GLIST_OFFSET;
}

static void InitGraphics_Init(void)
{
    _InitGfx_Init();
    ZOID_Init();
    InitBlits();
    InitCameras();
}

static void FreeBuffers_Init(void)
{
    if (!expansion_pak_present) {
        if (gListeners) {
            DeallocateMemory_Init((u32)gListeners);
            gListeners = 0;
        }
        if (gEmitters) {
            DeallocateMemory_Init((u32)gEmitters);
            gEmitters = 0;
        }
    }
}

static void AudioInit_Init(void)
{
    AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
}


void DeallocateMemory_Init(u32 address)     /* MAT3 DeallocateMemory@memsys.c */
{
    heap_lock();
    DeallocateMemoryInternal(address, 0);
    heap_unlock();
}
