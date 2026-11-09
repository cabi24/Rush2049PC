#include "game/game.h"

void PlayLRSound(s32 arg0) {
    s32 var_a0;

    if (arg0 & 0x1000) {
        var_a0 = 0x29;
    } else {
        var_a0 = 0x2C;
    }
    AudPlayEffect(var_a0, 0, 1, 0U);
}
