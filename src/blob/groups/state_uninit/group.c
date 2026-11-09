#include "game/game.h"

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

