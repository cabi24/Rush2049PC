#include "game/game.h"

void StopSound(s32 *arg0, s32 arg1) {
    if (arg1 != 0) {
        AudStopEmitter(*arg0);
    } else {
        AudStopEffect(*arg0);
    }
    InitSoundState(arg0);
}
