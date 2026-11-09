/* licensescr_animText: the licence screen's text (MAT3 licensescr.c animText). */
#include "game/game.h"

#define TEXT_LICENSE 234            /* gText.single[]: the licence text */
#define TEXT_EXPANSION_PAK 230      /* gText.single[]: the Expansion Pak notice */
#define BUILD_INFO_BUTTONS 0x3C000  /* the buttons that show the build's date and time while held (game bits 14-17) */

/* The licence screen's text blit callback: the licence text in the fixed-width small font; the build's date and
 * time while exactly the build-info buttons are held on the first controller (gContLevels), and a notice when an
 * Expansion Pak is in. Always 1. */
s32 licensescr_animText(s32 blit)
{
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(0);
    HAL_SetFontFixedWidth(0);
    HAL_SetFontSpacing(2);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawTextScrollBox(160, 165, 288, 110, -1, 0, gText.single[TEXT_LICENSE]);
    if (gContLevels[0] == BUILD_INFO_BUTTONS) {
        TB_SetTextColor(14);
        TB_Printf(210, 30, licensescr_build_info_format, build_date_text, build_time_text);
    }
    if (expansion_pak_present != 0) {
        TB_SetTextColor(14);
        TB_DrawText(160, 10, gText.single[TEXT_EXPANSION_PAK]);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetFontFixedWidth(1);
    HAL_SetFontSpacing(-1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
