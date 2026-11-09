#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue { s32 pad[6]; } OSMesgQueue;

VIEW(anon_loaded_ready_14[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view
VIEW(unsigned char, gNumModels);
#define gNumModels gNumModels_view

VIEW(OSMesgQueue, bg_load_idle_queue);
#define bg_load_idle_queue bg_load_idle_queue_view

s32 osRecvMesg(OSMesgQueue *, OSMesg *, s32);
s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

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
