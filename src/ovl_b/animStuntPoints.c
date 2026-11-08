/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/* Image B, 0x80392894..0x80392FE4 (1872 bytes): animStuntPoints, the stunt-mode
 * HUD text callback (blit animFunc; draws the per-coin "X n" counts, the running
 * and total stunt points, the bonus multiplier and the landing/wing messages).
 * Donor: MAT3 GameCube decomp, GameSource/src/hudextra.c animStuntPoints
 * (GC 0x80022a94, size 0x534); typed port with MAT3 names, N64 offsets.
 * Reconstructed with the GameCube/Dreamcast ports' source as reference; byte-matched against the retail ROM, 2026-10.
 * (2/468), which supplied the N64 record layouts and the named bonus copy.
 * N64 differences from the GC body: coin screen positions are used unhalved,
 * the coin record is the older 0x48-byte form (no *_Coin fields), the text
 * table indices are 181/179/180 and tab[38], and the callback takes the blit.
 * Shaping (disclosed):
 * - `showBonus` is read into a local through gStuntPlayer[i] (global-array
 *   root): uopt then emits `.noalias` for the bonuscnt pointer against p, which
 *   lets as1 schedule `li v0,-2` before the showBonus store as retail does
 *   (proven first by adding that one directive to the ugen listing).
 * - The decrement/sentinel pair and the stuntsHud clear loop body sit on their
 *   own lines: as1 breaks scheduling ties by source line.
 * - unused0/unused1 are the exact frame residual (rule): retail
 *   frame 184; 176 without them; two 4-byte slots. Not recovered names.
 * Format strings are the image's own data (stunt_coin_count_format.."%i"/"X %i"/"%s X %i").
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct tUVMatrix { f32 uvs[4][3]; } tUVMatrix;
typedef struct tStuntPlayer {
    s16 show, crash;
    f32 time;
    s32 maxStuntPoints, points, pointsHud, totalPoints, numstunts, lastStuntType;
    s16 stunts[10], stuntsHud[10], totalStunts[10], unused;
    f32 wingTime;
    s8 wingBonus, wingBonusHud, doneBonus, doneBonusHud;
    s32 bonus, bonusHud, showBonus, done;
} tStuntPlayer;
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

extern u32 gGameState;
extern s16 gNumWindows, gNumPlayers;
extern f32 gModelTime;
extern tStuntPlayer gStuntPlayer[4];
extern tText gText;
extern s32 wrongWayPos[4][4][2];
extern s32 stuntPos[4][4][2], stuntCoinPos[4][4][2];
extern s32 bonuscnt[4];
extern tStuntCoin stuntCoin[4][10];
extern char stunt_coin_count_format[], stunt_points_format[], stunt_bonus_format[], stunt_landed_bonus_format[], stunt_total_points_format[];

extern void HAL_SetTextDepth(f32);                       /* HAL_SetTextDepth */
extern s32 HAL_SetFont(s32);                        /* HAL_SetFont */
extern void TB_SetJust(u32, u32);                  /* TB_SetJust */
extern void HAL_SetTextTranslucency(f32);                       /* HAL_SetTextTranslucency */
extern void TB_Sprintf(void *, void *, ...);        /* TB_Sprintf */
extern void TB_SetTextColor(s32);                    /* TB_SetTextColor */
extern s16 TB_CenterText(void *, s16);      /* TB_CenterText */
extern void TB_DrawText(s16, s16, void *);          /* TB_DrawText */
extern s16 TB_GetFontLineHeight(void);             /* TB_GetFontLineHeight */

int animStuntPoints(void *blt) /* animStuntPoints */
{
    u8 str[32];
    int i, j;
    int x, y;
    tStuntPlayer *p;
    tStuntCoin *coin;
    int showBonus;
    int unused0, unused1;
    if (gGameState & 0x7c03fffe) return 1;
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
                int x = wrongWayPos[gNumWindows-1][i][0];
                int y = wrongWayPos[gNumWindows-1][i][1];
                TB_Sprintf(str, gText.single[181]);
                TB_Sprintf(str, stunt_bonus_format, str, p->bonusHud);
                TB_DrawText(TB_CenterText(str, x), y, str);
                PC_EACH30(bonuscnt[i]--;)
            }
        } else bonuscnt[i] = -2;
        if (!p->done) p->pointsHud = p->points;
        else if (!p->crash) {
            x = wrongWayPos[gNumWindows-1][i][0];
            y = wrongWayPos[gNumWindows-1][i][1];
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
