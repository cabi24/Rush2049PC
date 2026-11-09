/* Replacing one wave by another in place (MAT3 waves.c ReplaceWave): the new file is read into the old one's block
 * (waves/group.c has the waves). */
#include "game/game.h"

/* Makes wave instance idx hold wave newWave, not yet read (MAT3 _ReplaceWaveInstance). */
void ReplaceWaveInstance(int idx, int newWave)
{
    anon_loaded_ready_14 *winst;
    winst = &gWaveInstance[idx];
    ValidateWaveInstance(idx, 1, 1);
    winst->loaded = 0; winst->ready = 0; winst->wave = newWave; winst->mirrored = 0;
    SetWaveInstanceModel(idx);
}

/* Reads wave newWave over the instance that holds curWave, unless newWave is already loaded or curWave isn't; waits
 * for the frame being drawn first, since it may still read the old file. */
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
