#include "game/game.h"

void ZOID_SetBGColor(u16 color) {
    gBgColor = (color << 16) | color;
}
