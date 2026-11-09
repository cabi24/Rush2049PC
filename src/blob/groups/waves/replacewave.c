#include "game/game.h"

VIEW(anon_loaded_ready_14[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view

void ReplaceWaveInstance(int idx, int newWave)  /* _ReplaceWaveInstance (deleted stub 0x800A5014) */
{
    anon_loaded_ready_14 *winst;
    winst = &gWaveInstance[idx];
    ValidateWaveInstance(idx, 1, 1);
    winst->loaded = 0; winst->ready = 0; winst->wave = newWave; winst->mirrored = 0;
    SetWaveInstanceModel(idx);
}

void ReplaceWave(int curWave, int newWave)
{
    int idx;
    if (FindWaveInstanceInternal(newWave, -1) < 0) {
        idx = FindWaveInstanceInternal(curWave, -1);
        if (idx >= 0) {
            while (*(volatile s16 *)&gRenderInProgress) { pc_os_spin_yield("ReplaceWave"); }
            ReplaceWaveInstance(idx, newWave); ReadWave(idx);
        }
    }
}
