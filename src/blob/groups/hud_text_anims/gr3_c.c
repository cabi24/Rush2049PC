/* The title panes of the audio and options screens in the pause screen (MAT3 audioscr.c's and optionscr.c's
 * _TitlePane). */
#include "game/game.h"

void ChangePaneSize(intptr_t, s32, s32, s32, s32);
intptr_t InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);

/* Makes or resizes the audio screen's title pane for its title, or for "player N: controller removed" when the
 * pausing player's controller is out. */
void audio_TitlePane(void)
{
    char buf[76];
    s32 w;

    HAL_SetFont(13);
    if (audio_gControllerRemoved) {
        TB_Sprintf(buf, audio_title_player_format, playerInPause + 1, gText.single[233]);
        w = HAL_GetTextWidth(buf, -1);
    } else {
        w = HAL_GetTextWidth(gText.single[52], -1);
    }
    if (audio_gTitlePane) {
        ChangePaneSize(audio_gTitlePane, 152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14);
    } else {
        audio_gTitlePane = InitPane(152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
    }
}


/* The same for the options screen. */
void options_TitlePane(void)
{
    char buf[76];
    s32 w;

    HAL_SetFont(13);
    if (options_gControllerRemoved) {
        TB_Sprintf(buf, options_title_format, playerInPause + 1, gText.single[233]);
        w = HAL_GetTextWidth(buf, -1);
    } else {
        w = HAL_GetTextWidth(gText.single[50], -1);
    }
    if (options_gTitlePane) {
        ChangePaneSize(options_gTitlePane, 152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14);
    } else {
        options_gTitlePane = InitPane(152 - w / 2, 6, w / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
    }
}

