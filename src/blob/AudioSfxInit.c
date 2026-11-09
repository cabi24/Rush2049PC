/* AudioSfxInit: the sound server's start-up (MAT3 audiosfx.c). */
#include "game/game.h"
#include "game/audiomgr.h"

/* The sound server's lists, in this file's view of the header's (the same layout, without the volatile). */

void *memset(void *, s32, u32);

/* The number of command records per effect instance (MAT3 audiosfx.c's numCmds = maxSfx * 3). */
#define SFX_CMDS_PER_INSTANCE 3

/* Sets up the sound server for maxSfx effects at once (AudioInitSys passes the effect quota): maxSfx effect
 * instances (gSfxInstances) and three times as many command records on the free queue (gFreeQ), from the heap;
 * every channel free (gNumChannels), none stopped this frame. */
void AudioSfxInit(s32 maxSfx) {
    s32 i;
    s32 numCmds;

    sound_list_init(&gCmdQ, 0, 1);
    if (maxSfx == 0) {
    }
    gNumInstances = maxSfx;
    sound_list_init(&gFreeQ, 0, 1);
    gSfxInstances = AllocateMemoryHeap(0, gNumInstances * sizeof(tSfxVoice));
    PurgeableMemory(PC_N64A(gSfxInstances), 0);
    memset(gSfxInstances, 0, gNumInstances * sizeof(tSfxVoice));
    numCmds = maxSfx * SFX_CMDS_PER_INSTANCE;
    audiosfx_gCmds = AllocateMemoryHeap(0, numCmds * sizeof(tSfxCmd));
    PurgeableMemory(PC_N64A(audiosfx_gCmds), 0);
    memset(audiosfx_gCmds, 0, numCmds * sizeof(tSfxCmd));
    for (i = 0; i < numCmds; i++) {
        InsertLLE(&gFreeQ, &audiosfx_gCmds[i].dlle, gFreeQ.head);
    }
    gNumChannels = maxSfx;
    /* The N64 keeps five counts of stopped channels where MAT3 keeps one (AudioSfxServer). */
    for (i = 0; i < 5; i++) {
        gStoppedChannels[i] = 0;
    }
    gStopAllEffects = 0;
    audiosfx_gInited = 1;
}
