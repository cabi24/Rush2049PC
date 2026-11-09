/* calcControlMenuMaxSize: how many rows the controller-setup screen's list shows (MAT3 controlscr.c
 * calcControlMenuMaxSize). The screen is the front end's GS_CONTROLS and the pause menu's controls page. */
#include "game/game.h"

/* Set gControlMenuSize, the rows of the controls list that fit: the 16-pixel rows that fit in a player's text
 * rectangle less 24 pixels, at most the list's length (two rows plus the 12 offered controls). In the front end
 * the whole list always shows. */
void calcControlMenuMaxSize(void) {
    s32 rectHeight = controls_gPlayerRects[gNumWindows - 1][0].h;
    s32 i;
    s32 gControlMenuCount;
    HAL_SetFont(11);
    rectHeight -= 24;
    gControlMenuSize = rectHeight / 16;
    gControlMenuCount = 2;
    for (i = 0; i < 12; i++) {
        if (controls_validOption(0, i)) gControlMenuCount++;
    }
    if (gControlMenuSize > gControlMenuCount || (gGameState & GS_FRONTEND_MASK))
        gControlMenuSize = gControlMenuCount;
}
