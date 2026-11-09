#include "game/game.h"

void ChangePaneSize(void *, s32, s32, s32, s32);

/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);

s32 font_set_slp(s32 font) { s32 old; text_lock(); old = HAL_SetFontInternal(font); text_unlock(); return old; }

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
