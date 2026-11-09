#include "game/game.h"

void PlayUDSound(s32 arg0) {
    s32 var_a0;

    if (arg0 & 0x400) {
        var_a0 = 0x28;
    } else {
        var_a0 = 0x27;
    }
    AudPlayEffect(var_a0, 0, 1, 0U);
}
