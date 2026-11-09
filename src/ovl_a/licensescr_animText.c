#include "game/game.h"

VIEW(u32, gContLevels);
#define gContLevels gContLevels_view

s32 licensescr_animText(s32 arg0)
{
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(0);
    HAL_SetFontFixedWidth(0);
    HAL_SetFontSpacing(2);
    TB_SetJust(1, 1);
    TB_SetTextColor(1);
    TB_DrawTextScrollBox(160, 165, 288, 110, -1, 0, gText.single[234]);
    if (gContLevels == 0x3C000) {
        TB_SetTextColor(14);
        TB_Printf(210, 30, licensescr_build_info_format, build_date_text, build_time_text);
    }
    if (expansion_pak_present != 0) {
        TB_SetTextColor(14);
        TB_DrawText(160, 10, gText.single[230]);
    }
    TB_SetJust(0, 3);
    HAL_SetFontFixedWidth(1);
    HAL_SetFontSpacing(-1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
