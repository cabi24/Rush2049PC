#include "game/game.h"

void TB_DrawText(s16 x, s16 y, void *text)
{
    if (gJust.h == 1) x = TB_CenterText(text, x);
    else if (gJust.h == 2) x = TB_RightText(text, x);
    if (gJust.v == 1) y -= TB_GetFontHeight() / 2;
    else if (gJust.v == 4) y -= TB_GetFontHeight();
    HAL_SetTextPos(x, y); HAL_DrawText(text, -1);
}
