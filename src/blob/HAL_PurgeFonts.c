#include "game/game.h"

void HAL_PurgeFonts(void) {
    s32 temp_v0;
    s32 temp_v0_2;
    s32 var_s0;

    var_s0 = 0;
    do {
        temp_v0 = FindWaveInstanceInternal(var_s0 + 0x26, -1);
        if (temp_v0 >= 0) {
            UnlockWaveInstance(temp_v0);
        }
        temp_v0_2 = FindWaveInstanceInternal(var_s0 + 0x16, -1);
        if (temp_v0_2 >= 0) {
            UnlockWaveInstance(temp_v0_2);
        }
        var_s0 += 1;
    } while (var_s0 != 0x10);
}
