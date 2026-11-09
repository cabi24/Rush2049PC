#include "game/game.h"

typedef struct Color4 { u32 word; } Color4;
typedef struct Language { s32 unknown0; char **text; s32 unknown8; u16 *indices; char **indexed; } Language;
typedef struct Slot { f32 x,y,angle; u8 unknown0C[36]; f32 position[3]; u8 unknown3C[4]; } Slot;
typedef struct Camera { f32 uv[3][3], position[3]; u8 unknown30[104]; } Camera;
VIEW(Language, gText);
#define gText gText_view

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

extern void TB_FlashTextColors(Color4,Color4);

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
    TB_SetTextColor(1);
    x = 160;
    y = 10;
    if (modescr_buttonsInited == 1) {
        TB_SetJust(0, 1);
        ZOID_ProjectWindow(0, modescr_title_button_position, &gCamera[0], 0, point);
        x = point[0];
        y = point[1];
    }
    title = gText.text[40];
    TB_DrawText(TB_CenterText(title, x), y, title);
    y += TB_GetFontLineHeight() * 2;
    if (gNumPlayers == 1) {
        TB_Sprintf(text, modescr_one_player_format, gNumPlayers, gText.text[29]);
    } else {
        TB_Sprintf(text, modescr_players_format, gNumPlayers, gText.indexed[gText.indices[20]]);
    }
    TB_SetTextColor(10);
    y += TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    for (i = 0; i < 7; i++) {
        if (modescr_optionValid(i)) {
            TB_SetTextColor(i == gModeMenuSelect ? 22 : 1);
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
                    TB_SetTextColor(1);
                } else {
                    TB_FlashTextColors(flashColors, flashColors_at_1);
                    TB_SetTextColor(22);
                }
                ZOID_ProjectWindow(0, slot->position, &gCamera[0], 0, point);
                x = point[0];
                y = point[1];
                x -= 45;
            }
            title = gText.indexed[gText.indices[22] + i];
            TB_DrawText(x, y, title);
            y += TB_GetFontLineHeight();
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextTranslucency(-1.0f);
    TB_FlashTextColors(flashColors, flashColors_at_1);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
