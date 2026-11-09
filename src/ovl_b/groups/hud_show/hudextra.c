/* ShowHUDExtra and ClearBattlePoints: the HUD extras of battle and stunt races, made at the race's start (MAT3
 * hudextra.c; runtime image B). */
#include "game/game.h"

#define blit hudextra_blit
#define playerPaneColors hudextra_playerPaneColors

/* Makes the battle or stunt HUD once a race (nothing when hudextra_blit exists): battle's multiblit (BattleMultiBlit:
 * health, ammo, weapons, points), or for stunt races a pane in the player's colours behind each player's points (as
 * wide as "8888888" in the HUD font of this many windows) and StuntMultiBlit; then no battle point is flying and no
 * weapon, powerup, battle coin or stunt coin icon is shown (type 8, 0 and 11 their "none" icons). */
void ShowHUDExtra(void)
{
    int i;
    tBlitSet *pane;
    int w, h;
    int playerIdx;
    int s;
    if (!blit) {
        if (gRace == kBattle)
            blit = NewMultiBlit(0, 0, BattleMultiBlit, 10);
        else if (gRace == kStunt) {
            if (gNumWindows == 1) HAL_SetFont(10);
            else if (gNumWindows == 2) HAL_SetFont(11);
            else HAL_SetFont(12);
            w = HAL_GetTextWidth(stunt_pane_width_sample, -1) + 8;
            h = TB_GetFontLineHeight() + 4;
            for (playerIdx = 0; playerIdx < gNumPlayers; playerIdx++) {
                pane = InitPane(stuntPos[gNumWindows-1][playerIdx][0]-w/2,
                    stuntPos[gNumWindows-1][playerIdx][1]-h/2,
                    stuntPos[gNumWindows-1][playerIdx][0]+w/2,
                    stuntPos[gNumWindows-1][playerIdx][1]+h/2,
                    128, 0, playerIdx << 4, 1);
                ChangePaneColor(pane, playerPaneColors[playerIdx][0], playerPaneColors[playerIdx][1]);
            }
            blit = NewMultiBlit(0, 0, StuntMultiBlit, 17);
        }
        ClearBattlePoints();
        for (i = 0; i < 4; i++) {
            weaponIcon[i].type = 8;
            weaponIcon[i].idx = -1;
            powerupIcon[i].type = 0;
            powerupIcon[i].idx = -1;
            battleCoin[i].idx = -1;
            for (s = 0; s < 10; s++) {
                stuntCoin[i][s].type = 11;
                stuntCoin[i][s].idx = -1;
            }
        }
    }
}

/* Ends every flying battle point (animBattlePoints) and forgets them. */
void ClearBattlePoints(void)
{
    int i;
    battlePointCnt = 0;
    for (i = 0; i < 4; i++) battlePoint[i].active = 0;
}
