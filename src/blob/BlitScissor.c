#include "game/game.h"

void BlitScissor(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    s32 temp_v0;
    s32 temp_v0_2;

    if (arg0 < 0) {
        blitXmin = 0;
    } else {
        blitXmin = arg0;
    }
    if (arg1 < 0) {
        blitYmin = 0;
    } else {
        blitYmin = arg1;
    }
    temp_v0 = screen_width - 1;
    if (temp_v0 < arg2) {
        blitXmax = temp_v0;
    } else {
        blitXmax = arg2;
    }
    temp_v0_2 = screen_height - 1;
    if (temp_v0_2 < arg3) {
        blitYmax = temp_v0_2;
        return;
    }
    blitYmax = arg3;
}

