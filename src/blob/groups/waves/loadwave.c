/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* LoadWave = LoadWave (MAT3 GameSource/src/waves.c LoadWave, GC 0x8005B648) with
 * _NewWaveInstance (GC 0x8005B9BC) inlined. N64: background load hand-off to the loader
 * thread (two message queues) instead of the GC/PS2 _BGLoadWaveCheck path.
 * Author: (unchanged code). packaging: the inlined
 * helpers are non-static so their deleted-procedure stubs carry symbols in the group object:
 * _NewWaveInstance = NewWaveInstance (its retail stub), _BGLoadWave (no retail stub found; invented name). */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
typedef void *OSMesg;
typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;

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
extern unsigned int gWaveSizes[];           /* gWaveSizes */
extern OSMesgQueue bg_load_idle_queue;              /* loader: ready queue */
extern OSMesgQueue bg_load_request_queue;              /* loader: request queue */
extern int bg_load_request;                      /* loader: wave instance to read */
#define gWaveInstance D_80156D38

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);
int FindWaveInstanceInternal(int waveNum, signed char handle);          /* _FindWaveInstance */
void MirrorWaveInstance(int idx);
unsigned char *AllocateHandleHeap(void *heap, unsigned int size);
unsigned char *AllocateMemoryHeap(void *heap, unsigned int size);
/* GetPtrSize's definition takes the block's 32-bit N64 address: N64A(p) is a window pointer's (pc_prelude.h,
 * docs/port/NOFIXED.md 2.3). */
unsigned int GetPtrSize(unsigned int ptr);
#define N64A(x) PC_N64A(x)
void SetWaveInstanceModel(int idx);                                 /* _SetWaveInstanceModel */
void ReadWave(int idx);                                 /* _ReadWave */

int NewWaveInstance(int waveNum, int handle, void *heap)  /* _NewWaveInstance (deleted stub 0x800972BC) */
{
    anon_loaded_ready_14 *winst;
    int idx;
    for (idx = 0; idx < 64; idx++) {
        if (!gWaveInstance[idx].ptr) {
            winst = &gWaveInstance[idx];
            winst->loaded = 0; winst->ready = 0; winst->locked = 0; winst->mirrored = 0;
            winst->wave = waveNum; winst->handle = handle;
            if (handle) winst->ptr = AllocateHandleHeap(heap, (gWaveSizes[waveNum] + 31) & ~31);
            else winst->ptr = AllocateMemoryHeap(heap, (gWaveSizes[waveNum] + 31) & ~31);
            winst->size = GetPtrSize(N64A(winst->ptr));
            SetWaveInstanceModel(idx);
            return idx;
        }
    }
}

void _BGLoadWave(int idx)  /* N64 background-load hand-off (no retail stub symbol) */
{
    OSMesg msg;
    osRecvMesg(&bg_load_idle_queue, &msg, 1);
    bg_load_request = idx;
    osSendMesg(&bg_load_request_queue, 0, 1);
}

int LoadWave(int waveNum, int loadAgain, int loadBackground, int handle, void *heap)
{
    int idx;
    anon_loaded_ready_14 *winst;
    if (!loadAgain) {
        idx = FindWaveInstanceInternal(waveNum, handle);
        if (idx >= 0) {
            winst = &gWaveInstance[idx];
            if (winst->model) MirrorWaveInstance(idx);
            return idx;
        }
    }
    idx = NewWaveInstance(waveNum, handle, heap);
    if (loadBackground) _BGLoadWave(idx);
    else ReadWave(idx);
    return idx;
}
