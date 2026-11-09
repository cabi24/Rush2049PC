/* animCreditText: the credits shown in the demo, a blit animation function (MAT3 hud.c). */
#include "game/game.h"

#define CREDIT_TITLES 0              /* gText.tab: where the credits' titles start in taba */

/* In the demo with credits on (gShowCredits): draws credit page gCredit, its title in yellow and its gNumCreditNames
 * names below in white, in corner gCorner of the screen (0 top left, 1 top right, 2 bottom left, 3 bottom right).
 * The blit is hidden otherwise. Returns 1. */
s32 animCreditText(tBlit *blt)
{
    s32 x;
    s32 hide;
    s32 y;
    s16 sx;
    s32 i;

    hide = !(gGameState & GS_DEMO) || !gShowCredits;
    if (blt->hide != hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    if (blt->hide) {
        return 1;
    }
    switch (gCorner) {
    case 0:
        x = 30;
        y = 30;
        break;
    case 1:
        x = 290 - gCreditWidth;
        y = 30;
        break;
    case 2:
        x = 30;
        y = 155 - gNumCreditNames * 10;
        break;
    default:
        x = 290 - gCreditWidth;
        y = 155 - gNumCreditNames * 10;
        break;
    }
    HAL_SetFont(11);
    TB_SetTextColor(kTB_YellowText);
    sx = x;
    TB_DrawText(sx, y, gText.taba[gText.tab[CREDIT_TITLES] + gCredit]);
    TB_SetTextColor(kTB_WhiteText);
    i = 0;
    y += 15;
    for (; i < gNumCreditNames; i++) {
        TB_DrawText(sx, y, cStrCreditNames[gCredit].names[i]);
        y += 10;
    }
    return 1;
}
