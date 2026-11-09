/* The high-score screen's drawing (MAT3 highscorescr.c DrawUnlocked, DrawName, DrawCircuitCode, DrawScores,
 * DrawGhost and animText): highscore_animText is the screen's text blit callback and draws the page
 * gHighScoreState is on. */
#include "game/game.h"

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

/* The "unlocked" page: in each window of a player who unlocked something, centred and its box sized to fit, a
 * two-line heading, then (flashing) each track, car, circuit, transmission, engine, tyre and frame the race
 * unlocked (unlocked now, not before the race). */
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
        TB_SetTextColor(kTB_WhiteText);
        TB_DrawText(TB_CenterText(gText.single[55], x), y, gText.single[55]);
        y += TB_GetFontLineHeight();
        TB_DrawText(TB_CenterText(gText.single[56], x), y, gText.single[56]);
        y += TB_GetFontLineHeight() * 2;
        TB_SetTextColor(kTB_FlashText);
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
                TB_Sprintf(str, unlocked_circuit_format, gText.taba[gText.tab[44] + i], gText.single[42]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 8; i++) {
            if (gUnlockedTrans[p][i] && !gPrevUnlockedTrans[p][i]) {
                TB_Sprintf(str, unlocked_trans_format, gText.taba[gText.tab[4] + i], gText.taba[gText.tab[2] + 1]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 9; i++) {
            if (gUnlockedEngines[p][i] && !gPrevUnlockedEngines[p][i]) {
                TB_Sprintf(str, unlocked_engine_format, gText.taba[gText.tab[6] + i], gText.taba[gText.tab[2] + 3]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 5; i++) {
            if (gUnlockedTires[p][i] && !gPrevUnlockedTires[p][i]) {
                TB_Sprintf(str, unlocked_tires_format, gText.taba[gText.tab[7] + i], gText.taba[gText.tab[2] + 4]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
        for (i = 0; i < 6; i++) {
            if (gUnlockedFrames[p][i] && !gPrevUnlockedFrames[p][i]) {
                TB_Sprintf(str, unlocked_frame_format, gText.taba[gText.tab[8] + i], gText.taba[gText.tab[2] + 5]);
                TB_DrawText(TB_CenterText(str, x), y, str);
                y += TB_GetFontLineHeight();
            }
        }
    }
}


#define TEXT_SCORE_TITLES 27        /* gText.tab[]: the tables' titles */
#define TEXT_SCORE_PLACES 55        /* gText.tab[]: the places' suffixes ("st", "nd" ...) */

VIEW(s8[][5], gNewTotalScore);
#define gNewTotalScore gNewTotalScore_view

extern s32 sprintf(char *, const char *, ...);

/* SaveInit' Ghost: its link and file handle are pointer-sized on the PC (docs/port/CASTS.md). */
typedef struct Ghost { void *ll; void **fileh; u8 track, backward; char name[22]; f32 time; } Ghost;
VIEW(Ghost **, gSavedGhost);
#define gSavedGhost gSavedGhost_view

#define NAME_SEL_END (-1)           /* highscore_gNameSelect[]: the buttons under the letter grid */
#define NAME_SEL_DELETE (-2)
#define NAME_SEL_SPACE (-3)

/* HAL_SetFont, called through a file static of its own (MAT3's DrawName and DrawGhost each inline one). */
void hs_SetFont(s32 font)
{
    HAL_SetFont(font);
}

/* Player p's name entry: the heading, the name so far (flashing), the 6 x 9 grid of letters (the selected one
 * flashing) and the space, delete and end buttons under it. */
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
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(gText.single[150], x), y, gText.single[150]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 15;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx;
    TB_SetTextColor(kTB_FlashText);
    TB_DrawText(TB_CenterText(gScoreName[p], x), y, gScoreName[p]);
    y = highscore_gPlayerRects[gNumWindows - 1][p].y + 30;
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
    for (i = 0; i < 54; i++) {
        ltr[0] = gPlayerNameChars[i];
        TB_SetTextColor(i == highscore_gNameSelect[p] ? kTB_FlashText : kTB_WhiteText);
        TB_DrawText(TB_CenterText(ltr, x), y, ltr);
        if ((i + 1) % 9 == 0) {
            y += 10;
            x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 48;
        } else {
            x += 12;
        }
    }
    x = highscore_gPlayerRects[gNumWindows - 1][p].cx - 36;
    TB_SetTextColor(highscore_gNameSelect[p] == NAME_SEL_SPACE ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_space_label, x), y, name_entry_space_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == NAME_SEL_DELETE ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_del_label, x), y, name_entry_del_label);
    x += 36;
    TB_SetTextColor(highscore_gNameSelect[p] == NAME_SEL_END ? 22 : 16);
    TB_DrawText(TB_CenterText(name_entry_end_label, x), y, name_entry_end_label);
}

/* The circuit code page: two lines of text with the code between them, in two rows of up to 11 characters. */
void DrawCircuitCode(void)
{
    s32 x;
    s32 y;
    s32 i;
    s32 length;
    char str[64];

    HAL_SetFont(11);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(gText.single[64], 160), 80, gText.single[64]);
    TB_DrawText(TB_CenterText(gText.single[65], 160), 120, gText.single[65]);
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

/* The tables page: the table's title, then its five rows (place, name, time), each in the colour of the player
 * whose new score it is, white for an old one. */
void DrawScores(void)
{
    s32 i;
    s32 numWid;
    char str[96];
    char label[256];

    HAL_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    TB_DrawText(TB_CenterText(gText.taba[gText.tab[TEXT_SCORE_TITLES] + gTable], 160), 10, gText.taba[gText.tab[TEXT_SCORE_TITLES] + gTable]);
    for (i = 0; i < 5; i++) {
        sprintf(str, "%d", i + 1);
        HAL_SetFont(5);
        TB_SetTextColor(gNewTotalScore[gTable][i] == -1 ? 1 : playerTextColor[gNewTotalScore[gTable][i]]);
        numWid = HAL_GetTextWidth(str, -1);
        TB_DrawText(16, 76 + i * 24, str);
        HAL_SetFont(11);
        TB_DrawText(numWid + 16, 78 + i * 24, gText.taba[gText.tab[TEXT_SCORE_PLACES] + i]);
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

/* HAL_SetFont, called through a file static of its own (see hs_SetFont). */
void gh_SetFont(s32 font)
{
    HAL_SetFont(font);
}

/* The ghost save page: the title, the ghost's name and time, then either a notice (another profile's ghost is on
 * the pak, the ghost was saved) or the overwrite question (yes / no), or the menu: the pak, its status (an error,
 * no ghost, or the ghost there with its time; red when it is another profile's), save (grey when not offered) and
 * don't save. */
void DrawGhost(void)
{
    s32 i;
    s32 y;
    char *str;
    s16 gFreePages;
    char str1[128];
    char time[16];

    gh_SetFont(13);
    TB_SetTextColor(kTB_WhiteText);
    str = gText.single[219];
    TB_DrawText(TB_CenterText(str, 160), 10, str);
    gh_SetFont(11);
    str = gText.single[220];
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
        TB_Sprintf(str1, gText.single[223], (*gPakGhosts[gGhostPak])->name);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
    } else if (gGhostMenuConfirm == 1) {
        gh_SetFont(10);
        y = TB_GetFontLineHeight();
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(160, 110, 130, y * 4, -1, 0, gText.single[221]);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
        y = TB_GetFontLineHeight() * 2 + 110;
        TB_SetTextColor(highscore_gYes ? kTB_FlashText : kTB_WhiteText);
        str = gText.taba[gText.tab[24]];
        TB_DrawText(TB_CenterText(str, 145), y, str);
        TB_SetTextColor(highscore_gYes ? kTB_WhiteText : kTB_FlashText);
        str = gText.taba[gText.tab[24] + 1];
        TB_DrawText(TB_CenterText(str, 175), y, str);
    } else if (gGhostMenuNotify == 1) {
        gh_SetFont(10);
        TB_Sprintf(str1, gText.single[222], gPS_PakName[gGhostPak]);
        TB_SetJust(kTB_JustCenter, kTB_JustTop);
        TB_DrawTextScrollBox(160, 110, 130, TB_GetFontLineHeight() * 6, -1, 0, str1);
        TB_SetJust(kTB_JustLeft, kTB_JustTop);
    } else {
        gh_SetFont(10);
        y = 110;
        for (i = 0; i < 4; i++) {
            TB_SetTextColor(i == gGhostMenuSelect ? kTB_FlashText : kTB_WhiteText);
            time[0] = 0;
            if (i == kGhostMenuStatus) {
                switch (gGhostStatus[gGhostPak]) {
                case -4:
                    str = gText.single[165];
                    break;
                case -3:
                    str = gText.single[169];
                    break;
                case -2:
                    str = gText.single[166];
                    break;
                case -1:
                    str = gText.single[168];
                    break;
                case -5:
                    str = gText.single[160];
                    break;
                case kGhostNone:
                    str = gText.taba[gText.tab[46] + i];
                    break;
                case kGhostMine:
                case kGhostOther:
                    str = (*gPakGhosts[gGhostPak])->name;
                    TB_Sprintf(time, "%01d:%02d.%03d", (s32)((*gPakGhosts[gGhostPak])->time / 60.0f),
                                 (s32)(*gPakGhosts[gGhostPak])->time % 60,
                                 (s32)((*gPakGhosts[gGhostPak])->time * 1000.0f) % 1000);
                    break;
                }
                if (gGhostStatus[gGhostPak] == kGhostOther) TB_SetTextColor(5);
            } else {
                str = gText.taba[gText.tab[46] + i];
            }
            if (i == kGhostMenuSave && gGhostStatus[gGhostPak] != kGhostNone && gGhostStatus[gGhostPak] != kGhostMine) TB_SetTextColor(4);
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
            TB_SetTextColor(kTB_WhiteText);
            y += TB_GetFontLineHeight();
        }
    }
}

/* The screen's text blit callback: draw the page it is on, in front of everything. Always 1. */
s32 highscore_animText(s32 blit)
{
    s32 p;

    HAL_SetTextDepth(0.0f);
    switch (gHighScoreState) {
    case kHighScoreUnlocked:
        DrawUnlocked();
        break;
    case kHighScoreCircuitCode:
        DrawCircuitCode();
        break;
    case kHighScoreNames:
        for (p = 0; p < gNumPlayers; p++) {
            if (gDrawName[p] == 1) DrawName(p);
        }
        break;
    case kHighScoreGhost:
        DrawGhost();
        break;
    case kHighScoreScores:
        DrawScores();
        break;
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
