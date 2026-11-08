/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* tire_compound_set = InitWaves. MATCH (standalone -O3 and whole-program unit).
 * MAT3 GameSource/src/waves.c InitWaves (GC 0x8005B4B4): wait for the background loader, wait for the
 * renderer, ZOID_Reset + InitBlits, unload every unlocked wave instance, recount gNumModels, then
 * InitTextWave. N64 differences: _BGLoadWaveCheck(1) is real (take/return the loader-idle token on
 * bg_load_idle_queue; its deleted inlined stub is bg_load_wave_check_stub), UnloadWaveInstance takes one argument (no
 * texture/model wave-range test), and the N64 clears the memory heap (ClearMemoryHeap = memsys.c
 * ClearMemoryHeap, erased on GC) before InitTextWave.
 * Inlined helpers (each inline call reserves 8 frame bytes, retail frame 64): _BGLoadWaveCheck (msg at
 * 52(sp) as retail), ZOID_Reset (zoid.c; byte-equivalent copy of the locked out-of-line ZOID_Reset,
 * inlined cross-file in the whole-program build) and ClearMemory() = ClearMemoryHeap(0): a public memsys
 * wrapper around the static ClearMemoryHeap is a HYPOTHESIS (identity unconfirmed); it supplies the third
 * 8-byte inline area after _BGLoadWaveCheck that puts msg at 52 (a plain extra local does not).
 * Callees: func_800A5A40 = ZOID_ResetWindows, func_800A5488 = InitBlits, UnloadWaveInstance =
 * UnloadWaveInstance, InitTextWave = InitTextWave (map).
 */
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef float f32;
typedef void *OSMesg;
typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;
typedef struct Heap Heap;

typedef struct anon_loaded_ready_14 {
    signed char loaded;
    signed char ready;
    signed char locked;
    signed char model;
    unsigned char mirrored;
    signed char handle;
    unsigned char wave;
    unsigned int size;
    unsigned char *ptr;
    unsigned char *lastPtr;
} anon_loaded_ready_14;

extern anon_loaded_ready_14 D_80156D38[64];  /* gWaveInstance */
extern unsigned char gNumModels;            /* gNumModels */
extern s16 gRenderInProgress;                      /* gRenderInProgress */
extern OSMesgQueue bg_load_idle_queue;              /* background loader idle token */
extern s32 gNumObjects, cull_vertex_count, gNumBlitInsts, gNumMatrices, gNumPolyInsts;
extern s16 ObjHead;                      /* ObjHead */
extern f32 gGlobalCameraFacingScale;                      /* gGlobalCameraFacingScale */
extern u16 gNumLights;
extern intptr_t gLights;
extern s32 default_lights;
#define gWaveInstance D_80156D38

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
void ZOID_ResetWindows(void);                   /* ZOID_ResetWindows */
void InitBlits(void);                   /* InitBlits */
void UnloadWaveInstance(s32 idx);                /* UnloadWaveInstance */
void ClearMemoryHeap(Heap *heap);
void InitTextWave(void);                   /* InitTextWave */

static int _BGLoadWaveCheck(int wait)
{
    OSMesg msg;
    if (osRecvMesg(&bg_load_idle_queue, &msg, 0) == -1) {
        if (!wait) return 0;
        osRecvMesg(&bg_load_idle_queue, &msg, 1);
    }
    osSendMesg(&bg_load_idle_queue, 0, 1);
    return 1;
}

void ZOID_Reset(void);

static void ClearMemory(void)
{
    ClearMemoryHeap(0);
}

void InitWaves(void)
{
    int i;
    _BGLoadWaveCheck(1);
    while (*(volatile s16 *)&gRenderInProgress) { pc_os_spin_yield("InitWaves"); }
    ZOID_Reset(); InitBlits();
    for (i = 0; i < 64; i++) {
        if (gWaveInstance[i].ptr && !gWaveInstance[i].locked) UnloadWaveInstance(i);
    }
    gNumModels = 0;
    for (i = 0; i < 64; i++) if (gWaveInstance[i].ptr) gNumModels = i + 1;
    ClearMemory();
    InitTextWave();
}
