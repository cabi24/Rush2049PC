/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitSystem (0x800EEA7C, 820 B; historical label) = MAT3 InitSystem@Init.c (GC 0x80024948, 172 B), the N64
 * version: InitMemory?(50, 10) (InitMemory), SaveInit, InitGraphics (N64 _InitGfx: options
 * video_texture_filter/video_color_dither/video_dither_filter/video_divot = 0, FOV HFOV = 90, wait for osTvType (0x80000300),
 * video_set_mode, three 0x22620-byte frame heaps (audio_dma_sync / AllocateMemoryHeapHigh, cleared by audio_loop_control,
 * aligned (+32) & ~63), the two 128-byte frame records frame_records[0..1] (display-list buffers frame_command_buffer_a /
 * frame_command_buffer_b, count 2, heap), the display-list pointers frame_dynamic / frame_matrix_cursor = +1728 / frame_dl = frame_dl_start
 * = +0x9CC0; then ZOID_Init (ZOID_Reset), InitBlits (func_800A5488), InitCameras (ZeroCameras)),
 * AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0) (AudioInit), the two gListeners / gEmitters buffers freed with
 * DeallocateMemory when expansion_pak_present is clear, InitRushControls, SetControllerActive(-1, 0)
 * (SetControllerActive), InitOptions, InitializeTimer and HAL_InitText (_InitSemaphore: font queue
 * text_lock_queue created once, function-local static sInited at 0x801147C0; gFontIndex gFontIndex = -1).
 * MAT3 names in the helper names; the N64 body differs from GC (GC has no display setup or frees).
 * Base: draft (structure, buffers). levers:
 *  - DeallocateMemory (MAT3 memsys.c: _GetSemaphore; _DeallocateMemory(ptr, 0); _ReleaseSemaphore) inlined from a
 *    non-static copy calling the group's heap_lock / DeallocateMemoryInternal / heap_unlock (retail: pointer
 *    homes 72/48, DeallocateMemoryInternal's IPA a1/a2 convention). Calling the group's own DeallocateMemory_stub moves an
 *    unnamed deleted-procedure stub directly after InitSystem in the group object (1 extra word).
 *  - Wrapper inlines InitGraphics/_InitGfx/ZOID_Init/InitCameras, the free block and AudioInit size the 128-byte
 *    frame (each inlined call level reserves 8 bytes; without the AudioInit and free wrappers the frame is 112).
 *  - The second display buffer is frame_command_buffer_b (the draft had 0x80069090); frame_dynamic/frame_matrix_cursor/frame_dl read the
 *    record's buffer back (retail adds +1728 / +0x9CC0 to the kept 0x800586D0, no folded constants).
 *  - sInited is MAT3's function-local static (folded lui/lb and lui/sb as retail; extern keeps the address).
 * Shaping (disclosed): the bodies of _InitGfx and _InitSemaphore are each written on ONE source line. as1 breaks
 * ready-list ties by source line; retail's schedule (e.g. the FOV store in the osTvType branch delay slot, the
 * record stores, sInited's store after the queue arguments) is what one line per inlined body gives, i.e. the
 * original cross-file inlined code shared one line. With multi-line bodies: 31 differing rows.
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef void *OSMesg;
/* PC: the native OSScTask's shape, pointer fields where it has them (N64 0x58 bytes, host 0xA0) */
typedef struct { void *next; u32 state, flags; void *fb; u32 type, tflags; void *boot; u32 boot_size;
                 void *ucode; u32 ucode_size; void *udata; u32 udata_size; void *stack; u32 stack_size;
                 void *out; void *out_size; void *data; u32 data_size; void *yield; u32 yield_size;
                 void *msgQ; void *msg; } PcScTaskShape;
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
#define PC_ZEXT(x) ((uintptr_t)PC_N64P((u32)(x)))   /* an RDRAM-window address held as s32: its pointer */
/* The heap's API and the color buffers take 32-bit N64 addresses: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
#define N64A0(x) PC_N64A(x)
#define N64AS(x) (s32)PC_N64A(x)
extern OSMesgQueue text_lock_queue;
extern OSMesg text_lock_msg[];
extern OSMesgQueue heap_lock_queue;
extern s32 video_texture_filter, video_color_dither, video_dither_filter, video_divot;
extern f32 HFOV;
extern s32 osTvType;
extern s32 screen_width, screen_height;
extern intptr_t zbuffer;
extern s32 framebuffer_a, framebuffer_b;
extern Segs frame_records;
extern u8 frame_command_buffer_a[], frame_command_buffer_b[];
/* PC: the two frame command buffers hold 16-byte commands, so they are larger than the N64's frame_command_buffer_a /
 * frame_command_buffer_b (docs/port/POINTERS.md), and have room for 12 cars in two views: PC_FRAME_MATRICES matrices from +1728
 * and the display list's PC_GLIST_LEN commands from PC_GLIST_OFFSET, where the N64 has 600 and 4000 from +0x9CC0
 * (pc_game_types.h, docs/port/MORECARS.md 8). */
u8 pc_frame_dl[2][PC_FRAME_BYTES] __attribute__((aligned(16)));
#define DL_BUFFER_0 pc_frame_dl[0]
#define DL_BUFFER_1 pc_frame_dl[1]
extern u8 *frame_dynamic, *frame_matrix_cursor;
extern u8 *frame_dl, *frame_dl_start;
extern s8 expansion_pak_present;
extern void *gListeners, *gEmitters;
extern s8 text_lock_inited;
extern s8 gFontIndex;
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern void InitMemory(u16 arg0, u16 arg1);
extern void SaveInit(void);
extern s32 video_mode_base_index(void);
extern void video_set_mode(s32, s32, s32, s32);
extern void *AllocateMemoryHeap(s32 arg0, s32 arg1);
extern void PurgeableMemory(u32 arg0, s32 arg1);
extern void *AllocateMemoryHeapHigh(s32 arg0, s32 arg1);
extern void ZOID_Reset(void);
extern void InitBlits(void);
extern void ZeroCameras(void);
extern void AudioInit(s32, s32, s32, s32, s32, f32, f32, s32);
extern void DeallocateMemoryInternal(u32 arg0, s32 arg1);
extern void InitRushControls(void);
extern void SetControllerActive(s32, s32);
extern void InitOptions();
extern void InitializeTimer(void);
void heap_lock(void);   /* _GetSemaphore@memsys.c (audio_sfx_shutdown.c) */
void heap_unlock(void);   /* _ReleaseSemaphore@memsys.c (audio_sfx_shutdown.c) */
void DeallocateMemory_Init(u32 address);


static void _InitSemaphore_Init(void)     /* MAT3 _InitSemaphore@HAL_text.c, N64 form */
{
    static s8 sInited = 0; if (!sInited) { sInited = 1; osCreateMesgQueue(&text_lock_queue, text_lock_msg, 1); osSendMesg(&text_lock_queue, (OSMesg)0, 0); }
}

void HAL_InitText_Init(void)     /* MAT3 HAL_InitText@HAL_text.c */;

static void _InitGfx_Init(void)     /* N64 _InitGfx (GC ngcGraph.c has its own) */
{
    video_texture_filter = 0; video_color_dither = 0; video_dither_filter = 0; video_divot = 0; HFOV = 90.0f; while (osTvType == 0) {} video_set_mode(osTvType, video_mode_base_index(), screen_width, screen_height); zbuffer = (intptr_t)AllocateMemoryHeap(0, 0x22620); PurgeableMemory(N64A0(zbuffer), 0); framebuffer_a = N64AS(AllocateMemoryHeapHigh(0, 0x22620)); PurgeableMemory(framebuffer_a, 0); framebuffer_b = N64AS(AllocateMemoryHeapHigh(0, 0x22620)); PurgeableMemory(framebuffer_b, 0); zbuffer = (zbuffer + 32) & ~63; framebuffer_a = (framebuffer_a + 32) & ~63; framebuffer_b = (framebuffer_b + 32) & ~63; frame_records.f124 = PC_ZEXT(framebuffer_a); frame_records.f252 = PC_ZEXT(framebuffer_b); frame_records.buf88 = DL_BUFFER_0; frame_records.buf216 = DL_BUFFER_1; frame_records.n92 = 2; frame_records.n220 = 2; frame_dynamic = frame_records.buf88; frame_matrix_cursor = (u8 *)frame_records.buf88 + 1728; frame_dl_start = frame_dl = (u8 *)frame_records.buf88 + PC_GLIST_OFFSET;
}

void ZOID_Init(void);

void InitCameras(void);

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
            DeallocateMemory_Init(N64A(gListeners));
            gListeners = 0;
        }
        if (gEmitters) {
            DeallocateMemory_Init(N64A(gEmitters));
            gEmitters = 0;
        }
    }
}

static void AudioInit_Init(void)
{
    AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
}

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


void DeallocateMemory_Init(u32 address)     /* MAT3 DeallocateMemory@memsys.c */;
