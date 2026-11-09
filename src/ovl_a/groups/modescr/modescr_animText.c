/* modescr_animText: the mode menu's text (MAT3 modescr.c animText). */
#include "game/game.h"

typedef TextColor Color4;   /* the flashing colours (include/game's TextColor) */
typedef struct Slot { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; u8 unknown3C[4]; } Slot;
typedef struct Camera { f32 uv[3][3], position[3]; u8 unknown30[104]; } Camera;

VIEW(f32[3], modescr_title_button_position);
#define modescr_title_button_position modescr_title_button_position_view
VIEW(Camera[], gCamera);
#define gCamera gCamera_view

VIEW(Slot[], modescr_buttonInfo);
#define modescr_buttonInfo modescr_buttonInfo_view

VIEW(Color4, flashColors);
#define flashColors flashColors_view
VIEW(Color4, flashColors_at_1);
#define flashColors_at_1 flashColors_at_1_view

#define TEXT_MODE_TITLE 40          /* gText.single[]: the screen's title */
#define TEXT_ONE_PLAYER 29          /* gText.single[]: "player" */
#define TEXT_PLAYERS 20             /* gText.tab[]: "players" */
#define TEXT_MODE_NAMES 22          /* gText.tab[]: the modes' names */
#define MODE_ROWS 7

/* The menu's text blit callback: the title (on its 3D button once the buttons are up), then each offered mode's
 * name, beside its button when they are up: skipped while the button faces away, half see-through while it is
 * turning, flashing on the selected one. Always 1. */
s32 modescr_animText(void *callback_context)
{
    s16 x, y;
    s32 i;
    char *title;
    Slot *slot;
    char text[36];
    s16 point[2];
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    x = 160;
    y = 10;
    if (modescr_buttonsInited == 1) {
        TB_SetJust(kTB_JustLeft, kTB_JustCenter);
        ZOID_ProjectWindow(0, modescr_title_button_position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
    }
    title = gText.single[TEXT_MODE_TITLE];
    TB_DrawText(TB_CenterText(title, x), y, title);
    y += TB_GetFontLineHeight() * 2;
    /* Original game: the player count's line is formatted but never drawn (MAT3 the same). */
    if (gNumPlayers == 1) {
        TB_Sprintf(text, modescr_one_player_format, gNumPlayers, gText.single[TEXT_ONE_PLAYER]);
    } else {
        TB_Sprintf(text, modescr_players_format, gNumPlayers, gText.taba[gText.tab[TEXT_PLAYERS]]);
    }
    TB_SetTextColor(10);
    y += TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    for (i = 0; i < MODE_ROWS; i++) {
        if (modescr_optionValid(i)) {
            TB_SetTextColor(i == gModeMenuSelect ? kTB_FlashText : kTB_WhiteText);
            if (modescr_buttonsInited == 1) {
                slot = &modescr_buttonInfo[i];
                if (slot->angle > 0.6981317f && slot->angle < 2.443461f) continue;
                if (slot->angle > 0.34906584f && slot->angle < 2.7925267219543457f) {
                    HAL_SetTextTranslucency(128.0f);
                } else {
                    HAL_SetTextTranslucency(-1.0f);
                }
                if (slot->angle < 1.5707964f) {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(kTB_WhiteText);
                } else {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(kTB_FlashText);
                }
                ZOID_ProjectWindow(0, slot->position, &gCamera[0], 0, point);
                x = point[0];
                y = point[1];
                x -= 45;
            }
            title = gText.taba[gText.tab[TEXT_MODE_NAMES] + i];
            TB_DrawText(x, y, title);
            y += TB_GetFontLineHeight();
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors, flashColors_at_1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
