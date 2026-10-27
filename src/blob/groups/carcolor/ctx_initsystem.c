/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * game_late_init (0x800EEA7C, 820 B; historical label) = MAT3 InitSystem@Init.c (GC 0x80024948, 172 B), the N64
 * version: InitMemory?(50, 10) (func_800E7D0C), SaveInit (menu_save_options), InitGraphics (N64 _InitGfx: options
 * D_8015F738/D_80161380/D_80161398/D_801613A4 = 0, FOV D_80154188 = 90, wait for osTvType (0x80000300),
 * viewport_setup, three 0x22620-byte frame heaps (audio_dma_sync / sound_play_menu, cleared by audio_loop_control,
 * aligned (+32) & ~63), the two 128-byte frame records D_80156BE0[0..1] (display-list buffers D_800586D0 /
 * D_8006A090, count 2, heap), the display-list pointers D_8015B250 / D_8015B260 = +1728 / D_801497C8 = D_801497F4
 * = +0x9CC0; then ZOID_Init (func_800A5B3C), InitBlits (func_800A5488), InitCameras (particle_velocity_set)),
 * AudioInit(32, 16, 16, 5, 64, 1.0f, 200.0f, 0) (hud_setup), the two D_8011025C / D_80110260 buffers freed with
 * DeallocateMemory when D_80156994 is clear, InitRushControls (effect_spawn), SetControllerActive(-1, 0)
 * (player_state_set), func_800DE45C, InitializeTimer (viTickStart) and HAL_InitText (_InitSemaphore: font queue
 * D_801461D0 created once, function-local static sInited at 0x801147C0; gFontIndex D_80149DA0 = -1).
 * MAT3 names in the helper names; the N64 body differs from GC (GC has no display setup or frees).
 * Base: draft (structure, buffers). levers:
 *  - DeallocateMemory (MAT3 memsys.c: _GetSemaphore; _DeallocateMemory(ptr, 0); _ReleaseSemaphore) inlined from a
 *    non-static copy calling the group's func_80095CF4 / audio_reverb_update / func_80095CFC (retail: pointer
 *    homes 72/48, audio_reverb_update's IPA a1/a2 convention). Calling the group's own func_800960CC moves an
 *    unnamed deleted-procedure stub directly after game_late_init in the group object (1 extra word).
 *  - Wrapper inlines InitGraphics/_InitGfx/ZOID_Init/InitCameras, the free block and AudioInit size the 128-byte
 *    frame (each inlined call level reserves 8 bytes; without the AudioInit and free wrappers the frame is 112).
 *  - The second display buffer is D_8006A090 (the draft had 0x80069090); D_8015B250/D_8015B260/D_801497C8 read the
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
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;
typedef void *OSMesg;
#ifdef TARGET_PC
/* PC: the native OSScTask's shape, pointer fields where it has them (N64 0x58 bytes, host 0xA0) */
typedef struct { void *next; u32 state, flags; void *fb; u32 type, tflags; void *boot; u32 boot_size;
                 void *ucode; u32 ucode_size; void *udata; u32 udata_size; void *stack; u32 stack_size;
                 void *out; void *out_size; void *data; u32 data_size; void *yield; u32 yield_size;
                 void *msgQ; void *msg; } PcScTaskShape;
#endif
typedef struct {
#ifdef TARGET_PC
    PcScTaskShape task0;
#else
    u8 other0[88];
#endif
    void *buf88;        /* 88 */
    s16 n92;            /* 92 */
    u8 other94[30];
#ifdef TARGET_PC
    uintptr_t f124;     /* 124: the color buffer, pointer-sized */
#else
    s32 f124;           /* 124 */
#endif
#ifdef TARGET_PC
    PcScTaskShape task1;
#else
    u8 other128[88];
#endif
    void *buf216;       /* 216 */
    s16 n220;           /* 220 */
    u8 other222[30];
#ifdef TARGET_PC
    uintptr_t f252;     /* 252 */
#else
    s32 f252;           /* 252 */
#endif
} Segs;
#ifdef TARGET_PC
#define PC_ZEXT(x) ((u32)(x))   /* an RDRAM-window address held as s32 */
#else
#define PC_ZEXT(x) x
#endif
extern OSMesgQueue D_801461D0;
extern OSMesg D_801461FC[];
extern OSMesgQueue D_80152770;
extern s32 D_8015F738, D_80161380, D_80161398, D_801613A4;
extern f32 D_80154188;
extern s32 D_80000300;
extern s32 D_8002AFC0, D_8002AFC4;
#ifdef TARGET_PC
extern intptr_t D_8002EBB0;
extern s32 D_80156CEC, D_80157240;
#else
extern s32 D_8002EBB0, D_80156CEC, D_80157240;
#endif
extern Segs D_80156BE0;
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
extern u8 *D_801497C8, *D_801497F4;
extern s8 D_80156994;
extern void *D_8011025C, *D_80110260;
extern s8 D_801147C0;
extern s8 D_80149DA0;
extern s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern void func_800E7D0C(u16 arg0, u16 arg1);
extern void menu_save_options(void);
extern s32 get_tv_offset(void);
extern void viewport_setup(s32, s32, s32, s32);
extern void *AllocateMemoryHeap(s32 arg0, s32 arg1);
extern void PurgeableMemory(u32 arg0, s32 arg1);
extern void *sound_play_menu(s32 arg0, s32 arg1);
extern void func_800A5B3C(void);
extern void InitBlits(void);
extern void particle_velocity_set(void);
extern void hud_setup(s32, s32, s32, s32, s32, f32, f32, s32);
extern void audio_reverb_update(u32 arg0, s32 arg1);
extern void effect_spawn(void);
extern void SetControllerActive(s32, s32);
extern void func_800DE45C();
extern void viTickStart(void);
void func_80095CF4(void);   /* _GetSemaphore@memsys.c (wheel_params_set.c) */
void func_80095CFC(void);   /* _ReleaseSemaphore@memsys.c (wheel_params_set.c) */
void DeallocateMemory_Init(u32 address);


static void _InitSemaphore_Init(void)     /* MAT3 _InitSemaphore@HAL_text.c, N64 form */
{
    static s8 sInited = 0; if (!sInited) { sInited = 1; osCreateMesgQueue(&D_801461D0, D_801461FC, 1); osSendMesg(&D_801461D0, (OSMesg)0, 0); }
}

void HAL_InitText_Init(void)     /* MAT3 HAL_InitText@HAL_text.c */
{
    _InitSemaphore_Init();
    D_80149DA0 = -1;
}

static void _InitGfx_Init(void)     /* N64 _InitGfx (GC ngcGraph.c has its own) */
{
    D_8015F738 = 0; D_80161380 = 0; D_80161398 = 0; D_801613A4 = 0; D_80154188 = 90.0f; while (D_80000300 == 0) {} viewport_setup(D_80000300, get_tv_offset(), D_8002AFC0, D_8002AFC4); D_8002EBB0 = (intptr_t)AllocateMemoryHeap(0, 0x22620); PurgeableMemory(D_8002EBB0, 0); D_80156CEC = (s32)sound_play_menu(0, 0x22620); PurgeableMemory(D_80156CEC, 0); D_80157240 = (s32)sound_play_menu(0, 0x22620); PurgeableMemory(D_80157240, 0); D_8002EBB0 = (D_8002EBB0 + 32) & ~63; D_80156CEC = (D_80156CEC + 32) & ~63; D_80157240 = (D_80157240 + 32) & ~63; D_80156BE0.f124 = PC_ZEXT(D_80156CEC); D_80156BE0.f252 = PC_ZEXT(D_80157240); D_80156BE0.buf88 = DL_BUFFER_0; D_80156BE0.buf216 = DL_BUFFER_1; D_80156BE0.n92 = 2; D_80156BE0.n220 = 2; D_8015B250 = D_80156BE0.buf88; D_8015B260 = (u8 *)D_80156BE0.buf88 + 1728; D_801497F4 = D_801497C8 = (u8 *)D_80156BE0.buf88 + 0x9CC0;
}

static void ZOID_Init_Init(void)
{
    func_800A5B3C();
}

static void InitCameras_Init(void)
{
    particle_velocity_set();
}

static void InitGraphics_Init(void)
{
    _InitGfx_Init();
    ZOID_Init_Init();
    InitBlits();
    InitCameras_Init();
}

static void FreeBuffers_Init(void)
{
    if (!D_80156994) {
        if (D_8011025C) {
            DeallocateMemory_Init((u32)D_8011025C);
            D_8011025C = 0;
        }
        if (D_80110260) {
            DeallocateMemory_Init((u32)D_80110260);
            D_80110260 = 0;
        }
    }
}

static void AudioInit_Init(void)
{
    hud_setup(32, 16, 16, 5, 64, 1.0f, 200.0f, 0);
}

void game_late_init(void)
{
    func_800E7D0C(50, 10);
    menu_save_options();
    InitGraphics_Init();
    AudioInit_Init();
    FreeBuffers_Init();
    effect_spawn();
    SetControllerActive(-1, 0);
    func_800DE45C();
    viTickStart();
    HAL_InitText_Init();
}


void DeallocateMemory_Init(u32 address)     /* MAT3 DeallocateMemory@memsys.c */
{
    func_80095CF4();
    audio_reverb_update(address, 0);
    func_80095CFC();
}
