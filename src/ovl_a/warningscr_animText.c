#include "game/game.h"

s32 sprintf(char *, const char *, ...);

s32 warningscr_animText(s32 arg0)
{
    char buffer[256];
    s32 y;
    s32 height;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    HAL_SetFont(11);
    TB_SetTextColor(1);
    sprintf(buffer, warningscr_message_format,
            num_paks_found > 0 ? gText.single[153] : gText.single[152], gText.single[154],
            gText.single[88], gText.single[90],
            PS_GetBlocks(0x810), gText.single[86]);
    TB_DrawTextScrollBox(160, 90, 300, 220, -1, 0, buffer);
    height = TB_GetFontLineHeight();
    y = 200 - height * 3;
    HAL_SetFont(gPakScr ? 11 : 10);
    TB_SetTextColor(gPakScr ? 1 : 22);
    TB_DrawText(160, y - (gPakScr ? 0 : 1), gText.single[236]);
    if (num_paks_found > 0) {
        HAL_SetFont(gPakScr ? 10 : 11);
        TB_SetTextColor(gPakScr ? 22 : 1);
        TB_DrawText(160, y - (gPakScr ? 1 : 0) + height, gText.single[156]);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
