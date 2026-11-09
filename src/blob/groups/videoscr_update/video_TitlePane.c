/* The video screen's title pane (MAT3 videoscr.c _TitlePane). */
#include "game/game.h"

/* HAL_SetFont: the text font, under the text lock; returns the one before. */
s32 font_set_slp(s32 font) { s32 old; text_lock(); old = HAL_SetFontInternal(font); text_unlock(); return old; }

/* Makes or resizes the pane behind the video screen's title (message 53, or "CONTROLLER n removed" while a
 * controller is out), centred at the top of the 320-pixel screen. */
void video_TitlePane(void)
{
    s32 width;
    s32 height;
    s32 x;
    char str1[64];

    font_set_slp(13);
    if (video_gControllerRemoved) {
        TB_Sprintf(str1, "CONTROLLER %d %s", playerInPause + 1, gText.single[233]);
        width = HAL_GetTextWidth(str1, -1);
    } else {
        width = HAL_GetTextWidth(gText.single[53], -1);
    }
    if (video_gTitlePane) {
        height = TB_GetFontLineHeight();
        x = width / 2;
        ChangePaneSize(video_gTitlePane, 160 - x - 8, 6, x + 168, height + 14);
    } else {
        height = TB_GetFontLineHeight();
        x = width / 2;
        video_gTitlePane = InitPane(160 - x - 8, 6, x + 168, height + 14, 176, 0, 0, 0);
    }
}
