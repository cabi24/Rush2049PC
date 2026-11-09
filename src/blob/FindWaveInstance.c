#include "game/game.h"

/* The wave instance (slot) that holds wave waveNum, whatever its handle; -1 when none does. */
s32 FindWaveInstance(s32 waveNum) {
    return FindWaveInstanceInternal(waveNum, -1);
}
