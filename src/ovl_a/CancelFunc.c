#include "game/game.h"

s32 CancelFunc(s32 arg0)
{
    if (PS_PakChanged(arg0) || (gPakErr == 2 && PS_PakDamaged(arg0))) {
        return 1;
    }
    return 0;
}
