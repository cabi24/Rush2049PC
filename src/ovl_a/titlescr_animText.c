#include "game/game.h"

s32 titlescr_animText(s32 arg0)
{
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    if (attract_alt_text_flag == 1) {
        HAL_SetFont(13);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 190, 320, 110, -1, 0, titlescr_prompt_text);
    } else {
        HAL_SetFont(11);
        TB_SetTextColor(22);
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText.single[32]);
        TB_SetTextColor(0);
        HAL_SetFont(12);
        TB_DrawText(160, 205, gText.single[235]);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
