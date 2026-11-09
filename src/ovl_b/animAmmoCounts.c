/* animAmmoCounts: the weapons' ammunition counts, a text blit's draw function (MAT3 hudextra.c; runtime image B). */
#include "game/game.h"

#define WEAPON_NONE 8   /* a car's weapon kind: none */

typedef struct TextPosition { s16 x, y; } TextPosition;

VIEW(TextPosition[4][4], ammoPos);
#define ammoPos ammoPos_view

/* Draws each window's ammunition left (when its car has a weapon and a count) at ammoPos, centred, in white with a
 * black shadow, at no depth. Returns 1. */
int animAmmoCounts(void *callback_context)
{
    s16 i;
    s16 x, y;
    char text[12];
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetJust(kTB_JustCenter, kTB_JustCenter);
    for (i = 0; i < gNumWindows; i++) {
        if (i < gNumWindows && PC_CAR_WEAPON_TYPE(i) != WEAPON_NONE) {
            x = ammoPos[gNumWindows - 1][i].x;
            y = ammoPos[gNumWindows - 1][i].y;
            if (PC_CAR_AMMO_COUNT(i) >= 0) {
                TB_Sprintf(text, ammo_count_format, PC_CAR_AMMO_COUNT(i));
                TB_SetTextColor(kTB_BlackText);
                TB_DrawText(x + 1, y + 1, text);
                TB_SetTextColor(kTB_WhiteText);
                TB_DrawText(x, y, text);
            }
        }
    }
    TB_SetJust(kTB_JustLeft, kTB_JustTop);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
