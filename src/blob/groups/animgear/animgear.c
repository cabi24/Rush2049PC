/* animGear: the gear shown by the speedometer, a text blit's draw function (MAT3 hud.c). */
#include "game/game.h"

#define gFontSem text_lock_queue_view

typedef struct tText { u16 numSingle; u8 **single; } tText;

VIEW(s32[4][4][2], gearPos);
#define gearPos gearPos_view
VIEW(tText, gText);
#define gText gText_view
VIEW(OSMesgQueue, text_lock_queue);

/* Draws, for each window's player who shifts by hand and races on, the gear label right of gearPos's x and the gear
 * (R, N or its number) left of it, each with a black shadow a pixel down and right, in the HUD font for the number of
 * windows, at no depth. Returns 1. */
s32 animGear(void *blt)
{
    u8 str[2];
    s32 slot;
    s16 x, y;
    s8 gear;

    HAL_SetTextDepth(0.0f);
    if (gNumWindows == 1)
        HAL_SetFont(10);
    else if (gNumWindows == 2)
        HAL_SetFont(11);
    else
        HAL_SetFont(12);
    for (slot = 0; slot < gNumWindows; slot++) {
        gear = PC_MODEL_GEAR(slot);
        if (slot >= gNumWindows || !gShowShifter || PC_MODEL_AUTOTRANS(slot) ||
            PC_CAR_FINISHED(PC_MODEL_SLOT(slot)) == 1)
            continue;
        x = gearPos[gNumWindows - 1][slot][0] - 4;
        y = gearPos[gNumWindows - 1][slot][1];
        TB_SetTextColor(kTB_BlackText);
        TB_DrawText(TB_RightText(gText.single[204], x + 1), y + 1, gText.single[204]);
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(TB_RightText(gText.single[204], x), y, gText.single[204]);
        x = gearPos[gNumWindows - 1][slot][0] + 4;
        y = gearPos[gNumWindows - 1][slot][1];
        if (gear == -1)
            str[0] = 'R';
        else if (gear == 0)
            str[0] = 'N';
        else
            str[0] = gear + '0';
        str[1] = 0;
        TB_SetTextColor(kTB_BlackText);
        TB_DrawText(x + 1, y + 1, str);
        TB_SetTextColor(kTB_YellowText);
        TB_DrawText(x, y, str);
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}

 
