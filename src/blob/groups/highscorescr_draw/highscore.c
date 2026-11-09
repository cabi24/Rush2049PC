#include "game/game.h"

typedef struct Rect { s16 x, y, w, h; s16 cx, cy; } Rect;
typedef struct UIdx { u16 h[48]; } UIdx;
typedef struct TextState { void *word0; char **fixed; void *word8; UIdx *indices; char **strings; } TextState;

VIEW(TextState, gText);
#define gText gText_view

VIEW(Rect[][4], highscore_gPlayerRects);
#define highscore_gPlayerRects highscore_gPlayerRects_view

VIEW(s8[][19], gUnlockedTracks);
#define gUnlockedTracks gUnlockedTracks_view
VIEW(s8[][19], gPrevUnlockedTracks);
#define gPrevUnlockedTracks gPrevUnlockedTracks_view
VIEW(s8[][13], gUnlockedCars);
#define gUnlockedCars gUnlockedCars_view
VIEW(s8[][13], gPrevUnlockedCars);
#define gPrevUnlockedCars gPrevUnlockedCars_view
VIEW(s8[][4], gPrevUnlockedCircuits);
#define gPrevUnlockedCircuits gPrevUnlockedCircuits_view
VIEW(s8[][8], gPrevUnlockedTrans);
#define gPrevUnlockedTrans gPrevUnlockedTrans_view
VIEW(s8[][9], gPrevUnlockedEngines);
#define gPrevUnlockedEngines gPrevUnlockedEngines_view
VIEW(s8[][5], gPrevUnlockedTires);
#define gPrevUnlockedTires gPrevUnlockedTires_view
VIEW(s8[][6], gUnlockedFrames);
#define gUnlockedFrames gUnlockedFrames_view
VIEW(s8[][6], gPrevUnlockedFrames);
#define gPrevUnlockedFrames gPrevUnlockedFrames_view

extern void ChangePaneSize(void *, s32, s32, s32, s32);

void DrawUnlocked(void)
{
    s32 x;
    s32 y;
    s32 i;
    s32 p;
    char str[64];
    s32 stuffCount;

    HAL_SetFont(11);
    for (p = 0; p < gNumPlayers; p++) {
        if (p) {}   /* : disclosed compiled-out holder (p over &str priority) */
        if (gStuffUnlocked[p] == 0) continue;
        stuffCount = 3;
        for (i = 0; i < 19; i++) if (gUnlockedTracks[p][i] && !gPrevUnlockedTracks[p][i]) stuffCount++;
        for (i = 0; i < 13; i++) if (gUnlockedCars[p][i] && !gPrevUnlockedCars[p][i]) stuffCount++;
        for (i = 0; i < 4; i++) if (gUnlockedCircuits[p][i] && !gPrevUnlockedCircuits[p][i]) stuffCount++;
        for (i = 0; i < 8; i++) if (gUnlockedTrans[p][i] && !gPrevUnlockedTrans[p][i]) stuffCount++;
        for (i = 0; i < 9; i++) if (gUnlockedEngines[p][i] && !gPrevUnlockedEngines[p][i]) stuffCount++;
        for (i = 0; i < 5; i++) if (gUnlockedTires[p][i] && !gPrevUnlockedTires[p][i]) stuffCount++;
        for (i = 0; i < 6; i++) if (gUnlockedFrames[p][i] && !gPrevUnlockedFrames[p][i]) stuffCount++;
        x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
        y = highscore_gPlayerRects[gNumWindows - 1][p].cy - stuffCount * TB_GetFontLineHeight() / 2;
        if (namePanes[p] != 0) {
            ChangePaneSize(namePanes[p], highscore_gPlayerRects[gNumWindows - 1][p].x, y - 4,
                highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                y + stuffCount * TB_GetFontLineHeight() + 4);
        }
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(gText.fixed[55], x), y, gText.fixed[55]);
        y += TB_GetFontLineHeight();
        TB_DrawText(TB_CenterText(gText.fixed[56], x), y, gText.fixed[56]);
        y += TB_GetFontLineHeight() * 2;
        TB_SetTextColor(22);
        for (i = 0; i < 19; i++) {
            if (gUnlockedTracks[p][i] && !gPrevUnlockedTracks[p][i]) {
                TB_DrawText(TB_CenterText(gTrackTitles[i], x), y, gTrackTitles[i]);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 13; i++) {
            if (gUnlockedCars[p][i] && !gPrevUnlockedCars[p][i]) {
                TB_DrawText(TB_CenterText(CarTitles[i], x), y, CarTitles[i]);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 4; i++) {
            if (gUnlockedCircuits[p][i] && !gPrevUnlockedCircuits[p][i]) {
                TB_Sprintf(str, unlocked_circuit_format, gText.strings[gText.indices->h[44] + i], gText.fixed[42]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 8; i++) {
            if (gUnlockedTrans[p][i] && !gPrevUnlockedTrans[p][i]) {
                TB_Sprintf(str, unlocked_trans_format, gText.strings[gText.indices->h[4] + i], gText.strings[gText.indices->h[2] + 1]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 9; i++) {
            if (gUnlockedEngines[p][i] && !gPrevUnlockedEngines[p][i]) {
                TB_Sprintf(str, unlocked_engine_format, gText.strings[gText.indices->h[6] + i], gText.strings[gText.indices->h[2] + 3]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 5; i++) {
            if (gUnlockedTires[p][i] && !gPrevUnlockedTires[p][i]) {
                TB_Sprintf(str, unlocked_tires_format, gText.strings[gText.indices->h[7] + i], gText.strings[gText.indices->h[2] + 4]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 6; i++) {
            if (gUnlockedFrames[p][i] && !gPrevUnlockedFrames[p][i]) {
                TB_Sprintf(str, unlocked_frame_format, gText.strings[gText.indices->h[8] + i], gText.strings[gText.indices->h[2] + 5]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
    }
}


/* ---- highscore_animText = GC animText@highscorescr.c (draft) ---- */
typedef struct HsIdx { u8 pad54[54]; u16 tableNames; u8 pad56[54]; u16 scoreNames; } HsIdx;

VIEW(char[], highscore_gCircuitCode);
#define highscore_gCircuitCode highscore_gCircuitCode_view

VIEW(s8[][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

extern s32 sprintf(char *, const char *, ...);

VIEW(char *, gPlayerNameChars);
#define gPlayerNameChars gPlayerNameChars_view

/* SaveInit' Ghost: its link and file handle are pointer-sized on the PC (docs/port/CASTS.md). */
typedef struct Ghost { void *ll; void **fileh; u8 track, backward; char name[22]; f32 time; } Ghost;
VIEW(Ghost **, gSavedGhost);
#define gSavedGhost gSavedGhost_view

void hs_SetFont(s32 font)
{
    HAL_SetFont(font);
}

/* DrawName = GC DrawName(p)@highscorescr.c: name-entry grid for player p (internal; p arrives in s5). */
void DrawName(s32 p)
{
    char ltr[2];
    s32 x;
    s32 y;
    s32 i;

    hs_SetFont(12);
    ltr[1] = 0;
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 4;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(gText.fixed[150], x), y, gText.fixed[150]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 15;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
    TB_SetTextColor(22);
    TB_DrawText(TB_CenterText(gScoreName[p], x), y, gScoreName[p]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 30;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
    for (i = 0; i < 54; i++) {
        ltr[0] = gPlayerNameChars[i];
        TB_SetTextColor(i == highscore_gNameSelect[p] ? 22 : 1);
        TB_DrawText(TB_CenterText(ltr, x), y, ltr);
        if ((i + 1) % 9 == 0) {
            y += 10;
            x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
        } else {
            x += 12;
        }
    }
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -3 ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_space_label, x), y, name_entry_space_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -2 ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_del_label, x), y, name_entry_del_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == -1 ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_end_label, x), y, name_entry_end_label);
}

void DrawCircuitCode(void)
{
    s32 x;
    s32 y;
    s32 i;
    s32 length;
    char str[64];

    HAL_SetFont(11);
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(gText.fixed[64], 160), 80, gText.fixed[64]);
    TB_DrawText(TB_CenterText(gText.fixed[65], 160), 120, gText.fixed[65]);
    if (TB_Strlen(highscore_gCircuitCode) > 22) length = 22;
    else length = TB_Strlen(highscore_gCircuitCode);
    str[1] = 0;
    x = 160 - (length + 1) / 2 * 10 / 2;
    y = 95;
    for (i = 0; i < length; i++) {
        str[0] = highscore_gCircuitCode[i];
        TB_DrawText(TB_CenterText(str, x + 5), y, str);
        if ((i + 1) % ((length + 1) / 2) == 0) {
            x = 160 - (length + 1) / 2 * 10 / 2;
            y += 10;
        } else {
            x += 10;
        }
    }
}

void DrawScores(void)
{
    s32 i;
    s32 numWid;
    char str[96];
    char label[256];

    HAL_SetFont(13);
    TB_SetTextColor(1);
    TB_DrawText(TB_CenterText(gText.strings[((HsIdx *)gText.indices)->tableNames + gTable], 160), 10, gText.strings[((HsIdx *)gText.indices)->tableNames + gTable]);
    for (i = 0; i < 5; i++) {
        sprintf(str, "%d", i + 1);
        HAL_SetFont(5);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        numWid = HAL_GetTextWidth(str, -1);
        TB_DrawText(16, 76 + i * 24, str);
        HAL_SetFont(11);
        TB_DrawText(numWid + 16, 78 + i * 24, gText.strings[((HsIdx *)gText.indices)->scoreNames + i]);
    }
    for (i = 0; i < 5; i++) {
        HAL_SetFont(10);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        TB_EllipseText(label, gScoreNames[i], 160);
        TB_DrawText(69, 80 + i * 24, label);
    }
    for (i = 0; i < 5; i++) {
        HAL_SetFont(2);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        TB_DrawText(TB_RightText(gScoreTimes[i], 304), 80 + i * 24, gScoreTimes[i]);
    }
}

/* DrawGhost = GC DrawGhost@highscorescr.c (internal; only caller highscore_animText state 4). */


void gh_SetFont(s32 font)
{
    HAL_SetFont(font);
}

void DrawGhost(void)
{
    s32 i;
    s32 y;
    char *str;
    s16 gFreePages;
    char str1[128];
    char time[16];

    gh_SetFont(13);
    TB_SetTextColor(1);
    str = gText.fixed[219];
    TB_DrawText(TB_CenterText(str, 160), 10, str);
    gh_SetFont(11);
    str = gText.fixed[220];
    TB_DrawText(TB_CenterText(str, 160), 55, str);
    gh_SetFont(10);
    y = TB_GetFontLineHeight() + 55;
    str = (*gSavedGhost)->name;
    TB_DrawText(TB_CenterText(str, 160), y, str);
    y += TB_GetFontLineHeight();
    gh_SetFont(2);
    TB_Sprintf(time, "%01d:%02d.%03d", (s32)((*gSavedGhost)->time / 60.0f), (s32)(*gSavedGhost)->time % 60,
                 (s32)((*gSavedGhost)->time * 1000.0f) % 1000);
    TB_DrawText(TB_CenterText(time, 160), y, time);
    if (gGhostMenuConflict == 1) {
        gh_SetFont(10);
        TB_Sprintf(str1, gText.fixed[223], (*gPakGhosts[gGhostPak])->name);
        TB_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        TB_SetJust(0, 3);
    } else if (gGhostMenuConfirm == 1) {
        gh_SetFont(10);
        y = TB_GetFontLineHeight();
        TB_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, y * 4, -1, 0, gText.fixed[221]);
        TB_SetJust(0, 3);
        y = TB_GetFontLineHeight() * 2 + 110;
        TB_SetTextColor(highscore_gYes ? 22 : 1);
        str = gText.strings[gText.indices->h[24]];
        TB_DrawText(TB_CenterText(str, 145), y, str);
        TB_SetTextColor(highscore_gYes ? 1 : 22);
        str = gText.strings[gText.indices->h[24] + 1];
        TB_DrawText(TB_CenterText(str, 175), y, str);
    } else if (gGhostMenuNotify == 1) {
        gh_SetFont(10);
        TB_Sprintf(str1, gText.fixed[222], gPS_PakName[gGhostPak]);
        TB_SetJust(1, 3);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        TB_SetJust(0, 3);
    } else {
        gh_SetFont(10);
        y = 110;
        for (i = 0; i < 4; i++) {
            TB_SetTextColor(i == gGhostMenuSelect ? 22 : 1);
            time[0] = 0;
            if (i == 1) {
                switch (gGhostStatus[gGhostPak]) {
                case -4:
                    str = gText.fixed[165];
                    break;
                case -3:
                    str = gText.fixed[169];
                    break;
                case -2:
                    str = gText.fixed[166];
                    break;
                case -1:
                    str = gText.fixed[168];
                    break;
                case -5:
                    str = gText.fixed[160];
                    break;
                case 0:
                    str = gText.strings[gText.indices->h[46] + i];
                    break;
                case 1:
                case 2:
                    str = (*gPakGhosts[gGhostPak])->name;
                    TB_Sprintf(time, "%01d:%02d.%03d", (s32)((*gPakGhosts[gGhostPak])->time / 60.0f),
                                 (s32)(*gPakGhosts[gGhostPak])->time % 60,
                                 (s32)((*gPakGhosts[gGhostPak])->time * 1000.0f) % 1000);
                    do {} while (0);
                    break;
                }
                if (gGhostStatus[gGhostPak] == 2) TB_SetTextColor(5);
            } else {
                str = gText.strings[gText.indices->h[46] + i];
            }
            if (i == 2 && gGhostStatus[gGhostPak] != 0 && gGhostStatus[gGhostPak] != 1) TB_SetTextColor(4);
            if (i == 1) {
                TB_DrawText(TB_CenterText(str, 160), y, str);
                y += TB_GetFontLineHeight();
                if (time[0]) {
                    gh_SetFont(2);
                    TB_DrawText(TB_CenterText(time, 160), y, time);
                    gh_SetFont(10);
                }
            } else {
                TB_DrawText(95, y, str);
                if (i == 0) TB_DrawText(TB_RightText(gPS_PakName[gGhostPak], 225), y, gPS_PakName[gGhostPak]);
            }
            TB_SetTextColor(1);
            y += TB_GetFontLineHeight();
        }
    }
}

s32 highscore_animText(s32 arg0)
{
    s32 p;

    HAL_SetTextDepth(0.0f);
    switch (gHighScoreState) {
    case 1:
        DrawUnlocked();
        break;
    case 2:
        DrawCircuitCode();
        break;
    case 3:
        for (p = 0; p < gNumPlayers; p++) {
            if (gDrawName[p] == 1) DrawName(p);
        }
        break;
    case 4:
        DrawGhost();
        break;
    case 5:
        DrawScores();
        break;
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
