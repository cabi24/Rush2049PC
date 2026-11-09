/* titlescr_animText: the title screen's text (MAT3 titlescr.c animText): the flashing "press start" prompt. */
#include "game/game.h"

#define TEXT_PRESS_START 32         /* gText.single[]: the prompt */
#define TEXT_TITLE_LINE 235         /* gText.single[]: the line under it */

/* The title screen's text blit callback: the attract mode's prompt (titlescr_prompt_text) when
 * attract_alt_text_flag is set, else the usual prompt and the line under it. Always 1. */
s32 titlescr_animText(s32 blit)
{
    HAL_SetTextDepth(0.0f);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    if (attract_alt_text_flag == 1) {
        HAL_SetFont(13);
        TB_SetTextColor(kTB_FlashText);
        TB_DrawTextScrollBox(160, 190, 320, 110, -1, 0, titlescr_prompt_text);
    } else {
        HAL_SetFont(11);
        TB_SetTextColor(kTB_FlashText);
        TB_DrawTextScrollBox(160, 185, 320, 110, -1, 0, gText.single[TEXT_PRESS_START]);
        TB_SetTextColor(kTB_BlackText);
        HAL_SetFont(12);
        TB_DrawText(160, 205, gText.single[TEXT_TITLE_LINE]);
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
