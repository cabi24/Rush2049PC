/* The pause screen's "are you sure" text (MAT3 pausescr.c animSureText): an animation function of a blit. */
#include "game/game.h"

/* The blit the animation function runs for (MAT3 _tBlit_2's leading fields). */
typedef struct MenuItem {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 dirty;
} MenuItem;

#define TEXT_SURE 216             /* gText.single[]: "are you sure?" */

/* Sets item's hidden flag to off, redrawing its blit when that changes; returns the flag. */
s32 item_disable_B9C8(MenuItem *item, s32 off)
{
    if (off != item->dirty) {
        item->dirty = off;
        UpdateBlit(item);
    }
    return item->dirty;
}

/* For restart and quit (gPauseState 7 and 8: MAT3 kPauseRestart, kPauseQuit): the question (message 216) under
 * the title, and yes and no beside each other a quarter of its width from the centre, the chosen one (pause_gYes)
 * highlighted. Hidden in the other pause states. Returns 1: the blit stays. */
s32 animSureText(MenuItem *item) {
    s32 w;
    s32 x;
    s32 y;
    u8 *str;

    if (item_disable_B9C8(item, gPauseState < 7)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    TB_SetTextColor(1);
    str = gText.single[TEXT_SURE];
    TB_DrawText(TB_CenterText(str, x), y, str);
    y += TB_GetFontLineHeight();
    w = HAL_GetTextWidth(gText.single[TEXT_SURE], -1);
    x = pause_titlePos[0] - w / 4;
    str = gText.taba[gText.tab[TEXT_YES_NO]];
    TB_SetTextColor(pause_gYes ? 22 : 1);
    TB_DrawText(TB_CenterText(str, x), y, str);
    x = pause_titlePos[0] + w / 4;
    str = gText.taba[gText.tab[TEXT_YES_NO] + 1];
    TB_SetTextColor(pause_gYes ? 1 : 22);
    TB_DrawText(TB_CenterText(str, x), y, str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
