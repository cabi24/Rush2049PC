#include "game/game.h"


#define gFontSem text_lock_queue_view

typedef struct tText { u16 numSingle; u8 **single; } tText;
typedef struct tFontHeader { u8 pad0[2]; u8 ascent; u8 descent; } tFontHeader;

VIEW(s32[4][4][2], speedPos);
#define speedPos speedPos_view
VIEW(tText, gText);
#define gText gText_view
VIEW(tFontHeader *, gFontHeader);
#define gFontHeader gFontHeader_view
VIEW(OSMesgQueue, text_lock_queue);

s32 animSpeedText(void *blt)
{
    f32 mph;
    u8 digits[3];
    u8 str[2];
    s32 digitW, x, y, digitIdx, slot;

    if (!gShowSpeed)
        return 1;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 3);
    if (gNumWindows == 1)
        HAL_SetFont(4);
    else
        HAL_SetFont(1);
    digitW = HAL_GetCharWidth('8') + 1;
    for (slot = 0; slot < gNumWindows; slot++) {
        if (y) {}
        if (PC_CAR_PLACE_LOCKED(PC_MODEL_SLOT(slot)) == 1)
            continue;
        mph = PC_CAR_MPH(slot) >> 2;
        mph *= 1.2f;
        if (gShowMetric)
            mph *= 1.61f;
        if (mph < 0.0f)
            mph = -mph;
        x = speedPos[gNumWindows - 1][slot][0];
        x -= digitW;
        y = speedPos[gNumWindows - 1][slot][1];
        digits[0] = ((s32)(mph / 100.0f) % 10) + '0';
        digits[1] = ((s32)(mph / 10.0f) % 10) + '0';
        digits[2] = ((s32)mph % 10) + '0';
        if (gNumWindows == 1)
            HAL_SetFont(4);
        else
            HAL_SetFont(1);
        for (digitIdx = 0; digitIdx < 3; digitIdx++) {
            str[1] = 0;
            if (digits[digitIdx] != ':' && digits[digitIdx] != '.') {
                str[0] = '8';
                TB_SetTextColor(0);
                TB_DrawText(x, y, str);
            }
            str[0] = digits[digitIdx];
            TB_SetTextColor(14);
            TB_DrawText(x, y, str);
            x += digitW;
        }
        x = speedPos[gNumWindows - 1][slot][0];
        y += TB_GetFontHeight() - 4;
        HAL_SetFont(12);
        if (gShowMetric) {
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, gText.single[203]);
            TB_SetTextColor(1);
            TB_DrawText(x, y, gText.single[203]);
        } else {
            TB_SetTextColor(0);
            TB_DrawText(x + 1, y + 1, gText.single[202]);
            TB_SetTextColor(1);
            TB_DrawText(x, y, gText.single[202]);
        }
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}


u8 HAL_GetFontAscent_Speed(void)
{
    SetupFontData(0);
    return gFontHeader->ascent;
}

u8 HAL_GetFontDescent_Speed(void)
{
    SetupFontData(0);
    return gFontHeader->descent;
}

s16 TB_GetFontHeight_Speed(void)
{
    return HAL_GetFontAscent_Speed() + HAL_GetFontDescent_Speed();
}
