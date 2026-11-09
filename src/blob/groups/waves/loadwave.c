#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;

VIEW(anon_loaded_ready_14[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view

VIEW(OSMesgQueue, bg_load_idle_queue);
#define bg_load_idle_queue bg_load_idle_queue_view
VIEW(OSMesgQueue, bg_load_request_queue);
#define bg_load_request_queue bg_load_request_queue_view

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

#define N64A(x) PC_N64A(x)

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
