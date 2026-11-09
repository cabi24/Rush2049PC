/* animStuntPoints: the stunt points and the stunts' messages, a text blit's draw function (MAT3 hudextra.c;
 * runtime image B). */
#include "game/game.h"
#define tStuntCoin tStuntCoinView   /* this file's own tStuntCoin: include/game's tStuntCoin is another type */

typedef struct tUVMatrix { f32 uvs[4][3]; } tUVMatrix;

typedef struct tStuntCoin {
    s16 type, idx;
    tUVMatrix mat;
    f32 alpha;
    s16 screenPos[2], startPos[2], destPos[2];
    f32 scrollTime;
} tStuntCoin;
typedef struct tText {
    u16 numSingle;
    u8 **single;
    u16 numTab, numTabA;
    u16 *tab;
    u8 **taba;
} tText;

VIEW(tText, gText);
#define gText gText_view

/* Draws each stunt player's points, the stunt just landed and its bonus, and scrolls the coins won to the player's
 * coin row (stuntCoin), for the stunt race HUD. Returns 1. */
int animStuntPoints(void *blt)
{
    u8 str[32];
    int i, j;
    int x, y;
    tStuntPlayer *p;
    tStuntCoin *coin;
    int showBonus;
    int unused0, unused1;
    if (gGameState & GS_FRONTEND_MASK) return 1;
    HAL_SetTextDepth(0.0f);
    TB_SetJust(0, 1);
    if (gNumWindows == 1) HAL_SetFont(10); else HAL_SetFont(12);
    for (i = 0; i < gNumPlayers; i++) {
        p = &gStuntPlayer[i];
        TB_SetTextColor(1);
        if (p->show && (gModelTime - p->time < 2.69f || p->numstunts)) {
            for (j = 0; j < 10; j++) {
                coin = &stuntCoin[i][j];
                if (coin->type != 11 && p->stuntsHud[coin->type] && coin->alpha) {
                    HAL_SetTextTranslucency((u8)coin->alpha);
                    x = coin->screenPos[0] + (gNumWindows == 1 ? 12 : 6);
                    y = coin->screenPos[1];
                    TB_Sprintf(str, stunt_coin_count_format, p->stuntsHud[coin->type]);
                    TB_DrawText(x, y, str);
                }
            }
            x = stuntCoinPos[gNumWindows-1][i][0] + (gNumWindows == 1 ? 12 : 6);
            y = stuntCoinPos[gNumWindows-1][i][1];
            TB_Sprintf(str, stunt_points_format, p->pointsHud);
            TB_DrawText(TB_CenterText(str, x), y, str);
        } else if (!p->numstunts) {
            p->show = 0;
            p->crash = 0;
            p->bonusHud = 1;
            p->wingBonusHud = 0;
            p->doneBonusHud = 0;
            p->pointsHud = 0;
            for (j = 0; j < 10; j++) {
                p->stuntsHud[j] = 0;
            }
        }
        HAL_SetTextTranslucency(-1.0f);
        showBonus = gStuntPlayer[i].showBonus;
        if (showBonus > 0) {
            if (bonuscnt[i] == -1) {
                p->showBonus = showBonus - 1;
                bonuscnt[i] = -2;
            }
            else if (bonuscnt[i] == -2) bonuscnt[i] = 10;
            if (bonuscnt[i] >= 0) {
                int x = wrongWayPos[gNumWindows-1][i].x;
                int y = wrongWayPos[gNumWindows-1][i].y;
                TB_Sprintf(str, gText.single[181]);
                TB_Sprintf(str, stunt_bonus_format, str, p->bonusHud);
                TB_DrawText(TB_CenterText(str, x), y, str);
                PC_EACH30(bonuscnt[i]--;)
            }
        } else bonuscnt[i] = -2;
        if (!p->done) p->pointsHud = p->points;
        else if (!p->crash) {
            x = wrongWayPos[gNumWindows-1][i].x;
            y = wrongWayPos[gNumWindows-1][i].y;
            if (p->bonusHud > 1) {
                TB_Sprintf(str, gText.single[181]);
                TB_Sprintf(str, stunt_landed_bonus_format, str, p->bonusHud);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
            if (p->doneBonusHud) {
                TB_DrawText(TB_CenterText(gText.taba[gText.tab[38] + (p->doneBonusHud - 3)], x), y, gText.taba[gText.tab[38] + (p->doneBonusHud - 3)]);
                y += TB_GetFontLineHeight();
            }
            if (p->wingBonusHud == 1) {
                TB_DrawText(TB_CenterText(gText.single[179], x), y, gText.single[179]);
            } else if (p->wingBonusHud == 2) {
                TB_DrawText(TB_CenterText(gText.single[180], x), y, gText.single[180]);
            }
        }
        TB_SetTextColor(1);
        x = stuntPos[gNumWindows-1][i][0];
        y = stuntPos[gNumWindows-1][i][1];
        TB_Sprintf(str, stunt_total_points_format, p->totalPoints);
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    TB_SetJust(0, 3);
    HAL_SetTextDepth(-1.0f);
    return 1;
}
