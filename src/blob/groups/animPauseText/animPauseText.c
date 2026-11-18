/* The pause screen's menu text (MAT3 pausescr.c animPauseText): an animation function of a blit, run each frame
 * the blit is drawn. */
#include "game/game.h"

#define TEXT_PAUSE 56             /* gText.tab[]: the pause screen's strings: "paused", the menu, quit, restart */

/* Draws the pause menu's eight lines (the language's pause words) under the title, centred, the chosen one
 * (pauseMenuSelect) highlighted. Returns 1: the blit stays. */
s32 animPauseText(struct Blit *blt) {
    s32 i;
    s32 x;
    s32 y;
    u8 *str;

    if (Hidden(blt, 0)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    for (i = 0; i < 8; i++) {
        TB_SetTextColor(i == pauseMenuSelect ? 22 : 1);
        str = gText.taba[gText.tab[TEXT_PAUSE] + i + 1];
        TB_DrawText(TB_CenterText(str, x), y, str);
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
