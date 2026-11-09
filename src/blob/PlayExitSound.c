#include "game/game.h"

void PlayExitSound(s32 arg0) {
    s32 var_a0;

    if (arg0 & 1) {
        var_a0 = 0x2E;
    } else {
        var_a0 = 0x26;
    }
    AudPlayEffect(var_a0, 0, 1, 0U);
}
