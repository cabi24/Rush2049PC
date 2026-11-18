/* Loading a wave (MAT3 waves.c LoadWave): a free wave instance, its heap block, and the read, now or on the
 * background loader thread (waves/group.c has the waves). */
#include "game/game.h"

/* Takes the first free wave instance for wave waveNum and allocates its block from heap (0: the main heap), as a
 * handle when handle is set (the heap may then move it). Returns the instance's index (MAT3 _NewWaveInstance).
 * Original game: with all 64 instances taken it returns nothing (MAT3 the same). */
int NewWaveInstance(int waveNum, int handle, void *heap)
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
            winst->size = GetPtrSize(winst->ptr);
            SetWaveInstanceModel(idx);
            return idx;
        }
    }
}

/* Hands wave instance idx to the background loader thread: waits until it is idle, then posts the request. */
void _BGLoadWave(int idx)
{
    OSMesg msg;
    msg = queue_recv(&bg_load_idle_queue);
    bg_load_request = idx;
    queue_send(&bg_load_request_queue, NULL);
}

/* Loads wave waveNum and returns its wave instance. Unless loadAgain is set, an instance that already holds the
 * wave (with the same handle kind) is returned, mirrored to the current track direction if it is a model.
 * Otherwise a new instance is read now, or by the background loader when loadBackground is set. */
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
