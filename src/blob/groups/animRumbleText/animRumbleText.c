/* The pause screen's rumble page (MAT3 pausescr.c's rumble screen text): an animation function of a blit. */
#include "game/game.h"

#define TEXT_RUMBLE_OFF 170       /* gText.single[]: the rumble page's "off" */
#define TEXT_RUMBLE_ON 171        /* "on" */
#define TEXT_RUMBLE_TITLE 226     /* its title */

/* On the pause screen's rumble page (gPauseState 6, MAT3 kPauseRumbleScreen): a title (message 226) when some
 * player has no Rumble Pak, then each player's name in their colour at their place on the page with their rumble
 * state under it. Hidden on the other pages. Returns 1: the blit stays. */
s32 animRumbleText(tBlit *blt)
{
    u16 i;
    s32 ok;
    s16 x;
    s16 y;
    char *str;
    s32 ctl;

    ok = 1;
    if (Hidden(blt, gPauseState != 6)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    for (i = 0; i < gNumPlayers; i++) {
        ctl = gPlayers[i].controller;
        if (!PS_PakRumble(ctl)) {
            ok = 0;
        }
    }
    if (!ok) {
        HAL_SetFont(10);
        x = pause_gPlayerRects[0][1].cx;
        y = pause_gPlayerRects[0][1].cy - TB_GetFontLineHeight() / 2;
        TB_SetTextColor(22);
        str = gText.single[TEXT_RUMBLE_TITLE];
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetFont(11);
    TB_SetTextColor(1);
    for (i = 0; i < gNumPlayers; i++) {
        x = pause_gPlayerRects[gNumPlayers - 1][i].cx;
        y = pause_gPlayerRects[gNumPlayers - 1][i].cy - TB_GetFontLineHeight();
        TB_SetTextColor(playerTextColor[i]);
        str = (char *)(*gPlayers[i].save)->name;
        TB_DrawText(TB_CenterText(str, x), y, str);
        y += TB_GetFontLineHeight();
        ctl = gPlayers[i].controller;
        if (PS_PakRumble(ctl)) {
            str = gText.single[TEXT_RUMBLE_ON];
        } else {
            str = gText.single[TEXT_RUMBLE_OFF];
        }
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
