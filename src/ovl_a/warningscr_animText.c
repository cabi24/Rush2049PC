/* warningscr_animText: the controller-pak warning screen's text (MAT3 warningscr.c animText; the N64's screen at
 * boot that says how many pages a profile needs, and offers to go on or to open the pak manager). */
#include "game/game.h"

s32 sprintf(char *, const char *, ...);

#define PROFILE_BYTES 0x810         /* a profile's save data (PS_GetBlocks: the pages it takes) */

/* The warning screen's text blit callback: the message (worded by whether any pak was found, with the pages a
 * profile needs), then the two choices, the highlighted one (gPakScr: 0 go on, 1 the pak manager, offered only when
 * a pak was found) flashing in the smaller font. Always 1. */
s32 warningscr_animText(s32 blit)
{
    char buffer[256];
    s32 y;
    s32 height;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    sprintf(buffer, warningscr_message_format,
            num_paks_found > 0 ? gText.single[153] : gText.single[152], gText.single[154],
            gText.single[88], gText.single[90],
            PS_GetBlocks(PROFILE_BYTES), gText.single[86]);
    TB_DrawTextScrollBox(160, 90, 300, 220, -1, 0, buffer);
    height = TB_GetFontLineHeight();
    y = 200 - height * 3;
    HAL_SetFont(gPakScr ? 11 : 10);
    TB_SetTextColor(gPakScr ? kTB_WhiteText : kTB_FlashText);
    TB_DrawText(160, y - (gPakScr ? 0 : 1), gText.single[236]);
    if (num_paks_found > 0) {
        HAL_SetFont(gPakScr ? 10 : 11);
        TB_SetTextColor(gPakScr ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(160, y - (gPakScr ? 1 : 0) + height, gText.single[156]);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
