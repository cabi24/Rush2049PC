#include "game/game.h"
#define tHudIcon tHudIconView   /* this file's own tHudIcon: include/game's tHudIcon is another type */
#define tStuntCoin tStuntCoinView   /* this file's own tStuntCoin: include/game's tStuntCoin is another type */

typedef struct tUVMatrix { f32 uvs[4][3]; } tUVMatrix;
typedef struct tHudIcon { s16 type, idx; tUVMatrix mat; } tHudIcon;          /* anon_type_idx_34 */
typedef struct tBattleCoin { s16 idx; s16 pad; tUVMatrix mat; } tBattleCoin; /* anon_idx_mat_34 */
typedef struct tStuntCoin {
    s16 type, idx;
    tUVMatrix mat;
    f32 alpha;
    s16 screenPos[2], startPos[2], destPos[2];
    f32 scrollTime;
} tStuntCoin;
typedef struct tBattlePoint {      /* anon_active_cnt_18 */
    s8 active, cnt, plto, plfrom;
    f32 startx, starty, endx, endy;
    s8 p;
    u8 pad[3];
} tBattlePoint;
typedef struct tBlitSet tBlitSet;
typedef struct tBlitDef tBlitDef;
/* gcc rejects arrays of an incomplete type, which IDO accepts. MAT3 tBlitDef (size 0x24), the same layout as
   MultiBlit in src/blob/NewMultiBlit.c. */
struct tBlitDef { const char *texname; s16 dulx, duly, width, height, top, bot, left, right; u32 zdepth, alpha;
                  s32 (*animfunc)(void *); u32 animid; };

VIEW(tHudIcon[4], weaponIcon);
#define weaponIcon weaponIcon_view
VIEW(tHudIcon[4], powerupIcon);
#define powerupIcon powerupIcon_view
VIEW(tBattleCoin[4], battleCoin);
#define battleCoin battleCoin_view
VIEW(tStuntCoin[4][10], stuntCoin);
#define stuntCoin stuntCoin_view

VIEW(tBattlePoint[4], battlePoint);
#define battlePoint battlePoint_view
VIEW(s32[4][4][2], stuntPos);
#define stuntPos stuntPos_view
VIEW(u8[4][2][4], hudextra_playerPaneColors);
#define hudextra_playerPaneColors hudextra_playerPaneColors_view
VIEW(tBlitDef[], BattleMultiBlit);
#define BattleMultiBlit BattleMultiBlit_view
VIEW(tBlitDef[], StuntMultiBlit);
#define StuntMultiBlit StuntMultiBlit_view

#define blit hudextra_blit
#define playerPaneColors hudextra_playerPaneColors
#define kBattle 6
#define kStunt 4

extern void *NewMultiBlit(s16, s16, tBlitDef *, s16);

extern tBlitSet *InitPane(s32, s32, s32, s32, s32, void *, u32, s32);

void ShowHUDExtra(void) /* ShowHUDExtra */
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

void ClearBattlePoints(void)
{
    int i;
    battlePointCnt = 0;
    for (i = 0; i < 4; i++) battlePoint[i].active = 0;
}
