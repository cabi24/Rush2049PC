/* uninitGameState: what a game state loaded, unloaded as it ends (MAT3 state.c). */
#include "game/game.h"

/* Unloads the asset waves of the state ending (gGameState): 0x4000 wave 57, 0x100 wave 56 (the waves then set up
 * again), 0x80 wave 60, 0x20000000 wave 55 (and 54, 58, 59 loaded and locked), 0x10000000 the fonts. */
void uninitGameState(void)
{
    unsigned int state=gGameState;
    if(state&0x4000) UnloadWaveAll(57);
    else if(state&0x100) {
        UnloadWaveAll(56);
        InitWaves();
    } else if(state&0x80) UnloadWaveAll(60);
    else if(state&0x20000000) {
        UnloadWaveAll(55);
        LockAndLoad(54);
        LockAndLoad(58);
        LockAndLoad(59);
    } else if(state&0x10000000) HAL_PurgeFonts();
}

