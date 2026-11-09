#include "game/game.h"

void HAL_SetTextPos(s16 arg0, s16 arg1) {
    if (arg0 != -0x8000) {
        gFontXpos = arg0;
    }
    if (arg1 != -0x8000) {
        gFontYpos = arg1;
    }
}
