#include "game/game.h"

VIEW(PlayerRect[4][4], controls_gPlayerRects);
#define controls_gPlayerRects controls_gPlayerRects_view

void calcControlMenuMaxSize(void) {
    s32 rectHeight = controls_gPlayerRects[gNumWindows - 1][0].h;
    s32 i;
    s32 gControlMenuCount;
    HAL_SetFont(11);
    rectHeight -= 24;
    gControlMenuSize = rectHeight / 16;
    gControlMenuCount = 2;
    for (i = 0; i < 12; i++) {
        if (gNumWindows) {}
        if (controls_validOption(0, i)) gControlMenuCount++;
    }
    do {} while (0);
    if (gControlMenuSize > gControlMenuCount || (gGameState & 0x7C03FFFE))
        gControlMenuSize = gControlMenuCount;
}

