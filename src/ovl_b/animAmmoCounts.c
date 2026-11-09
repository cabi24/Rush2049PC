#include "game/game.h"

typedef struct TextPosition { s16 x, y; } TextPosition;

VIEW(TextPosition[4][4], ammoPos);
#define ammoPos ammoPos_view

int animAmmoCounts(void *callback_context)
{
    s16 i;
    s16 x, y;
    char text[12];
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(10);
    TB_SetJust(1, 1);
    for (i = 0; i < gNumWindows; i++) {
        if (i < gNumWindows && PC_CAR_KIND(i) != 8) {
            x = ammoPos[gNumWindows - 1][i].x;
            y = ammoPos[gNumWindows - 1][i].y;
            if (PC_CAR_LIVES(i) >= 0) {
                TB_Sprintf(text, ammo_count_format, PC_CAR_LIVES(i));
                TB_SetTextColor(0);
                TB_DrawText(x + 1, y + 1, text);
                TB_SetTextColor(1);
                TB_DrawText(x, y, text);
            }
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
