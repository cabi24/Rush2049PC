/* InitWaves (MAT3 waves.c): frees every wave but the locked ones, between game states (waves/group.c has the
 * waves). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;

VIEW(unsigned char, gNumModels);
#define gNumModels gNumModels_view

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

/* Whether the background loader is idle: without wait, 0 when it is busy; with wait, blocks until it is idle. Gives
 * the idle token back either way, so it returns 1 (MAT3 _BGLoadWaveCheck). */
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

/* Frees the main heap's purgeable blocks (in use, unlocked, not behind a handle, not the heap's own) and compacts
 * what is left (MAT3 ClearMemory). */
static void ClearMemory(void)
{
    ClearMemoryHeap(0);
}

/* Waits for the background loader and for the frame being drawn, resets the scene objects and the blits (which
 * point into the waves), frees every wave instance that isn't locked, counts the instances that may hold models
 * (gNumModels), compacts the heap and reloads the text font's wave.
 * MAT3 frees only the waves 77-232 that aren't locked; the N64 frees every unlocked one. */
void InitWaves(void)
{
    int i;
    _BGLoadWaveCheck(1);
    while (gRenderInProgress) { pc_os_spin_yield("InitWaves"); }
    ZOID_Reset(); InitBlits();
    for (i = 0; i < 64; i++) {
        if (gWaveInstance[i].ptr && !gWaveInstance[i].locked) UnloadWaveInstance(i);
    }
    gNumModels = 0;
    for (i = 0; i < 64; i++) if (gWaveInstance[i].ptr) gNumModels = i + 1;
    ClearMemory();
    InitTextWave();
}
