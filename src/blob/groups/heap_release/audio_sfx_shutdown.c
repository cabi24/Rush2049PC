#include "game/game.h"

typedef struct List16 { u8 indirect, doubly, pad2[2]; u32 count; void *head, *tail; } List16;

VIEW(volatile List16, gCmdQ);
#define gCmdQ gCmdQ_view
VIEW(uintptr_t, gSfxInstances);
#define gSfxInstances gSfxInstances_view
VIEW(uintptr_t, audiosfx_gCmds);
#define audiosfx_gCmds audiosfx_gCmds_view

void audio_sfx_shutdown(void)
{
    AudioStopAllEffects();
    while (gStopAllEffects != 0) { pc_os_spin_yield("wheel_params_set 1"); }
    while (gCmdQ.head != 0) { pc_os_spin_yield("wheel_params_set 2"); }
    audiosfx_gInited = 0;
    DeallocateMemory(gSfxInstances);
    DeallocateMemory(audiosfx_gCmds);
}
