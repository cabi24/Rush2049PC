/* The HUD's race results (MAT3 hud.c animResults): an animation function, run each frame at the end of a race,
 * that draws each finished player's result in their window and, with two or more windows, the winner. */
#include "game/game.h"

#define TEXT_PLACE_SUFFIXES 55            /* gText.tab[]: the places' suffixes ("st", "nd" ...) */

/* On the PC the model records are pc_models, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

/* HAL_SetFont without its result. */
void ar_SetFont(s32 font)
{
    ar_font_set(font);
}

/* Draws, for each window whose player has finished (in death mode, done every lap), a pane in the player's colour
 * with their name and result: stunt points (gRace 4), obstacle time or "failed" (5), battle points (6), else
 * their place and time or laps. With two or more windows all finished, a middle pane names the winner (or the
 * drone, or a tie of n players): resultPane[4], the fifth of resultPane's panes (MAT3 hud.c resultPane[5]). */
s32 animResults(s32 unused)
{
    s32 playerIdx;
    s32 x;
    s32 y;
    char str[128];
    s32 numFinished;
    s32 aFinished;

    numFinished = 0;
    aFinished = 0;
    for (playerIdx = 0; playerIdx < gNumWindows; playerIdx++) {
        if (gNumLaps == pc_models[playerIdx].laps) aFinished = 1;
        if (pc_cars[pc_models[playerIdx].slot].place_locked == 1) numFinished++;
    }
    HAL_SetTextDepth(0.0f);
    TB_SetJust(1, 1);
    ar_SetFont(10);
    if (gNumWindows >= 2 && numFinished == gNumWindows && (!gDeathMode || (gDeathMode && aFinished))) {
        if (resultPane[4] == 0) {
            s32 h = TB_GetFontLineHeight() / 2;
            resultPane[4] = InitPane(91, 106 - h, 229, h + 114, 192, 0, 0, 1);
        }
        TB_SetTextColor(22);
        if (pc_models[pc_models[gRankedPlayer[0]].slot].drone_type == 1) {
            TB_Sprintf(str, gText.single[200]);
        } else if (ties) {
            TB_Sprintf(str, gText.single[201], ties + 1);
        } else {
            TB_Sprintf(str, gText.single[199], (*gPlayers[gRankedPlayer[0]].save)->name);
        }
        TB_DrawTextScrollBox(160, 110, 160, 110, -1, 0, str);
    }
    TB_SetTextColor(1);
    for (playerIdx = 0; playerIdx < gNumWindows; playerIdx++) {
        if (gDeathMode && gNumLaps != pc_models[playerIdx].laps) continue;
        if (pc_cars[pc_models[playerIdx].slot].place_locked != 1) continue;
        x = wrongWayPos[gNumWindows - 1][playerIdx].x;
        y = wrongWayPos[gNumWindows - 1][playerIdx].y;
        if (gNumWindows == 4 && playerIdx >= 2) y += 12;
        if (resultPane[playerIdx] == 0) {
            s32 h = TB_GetFontLineHeight() * 3 / 2;
            resultPane[playerIdx] = InitPane(x - 69, y - h - 4, x + 69, h + y + 4, 192, 0, playerIdx << 4, 1);
            ChangePaneColor(resultPane[playerIdx], hud_playerPaneColors[playerIdx].pane.c,
                            hud_playerPaneColors[playerIdx].border.c);
        }
        if (gRace == kStunt) {
            TB_Sprintf(str, "%s\n%d %s", (*gPlayers[playerIdx].save)->name, gStuntPlayer[playerIdx].totalPoints,
                         gText.single[130]);
        } else if (gRace == kObstacle) {
            if (gLap[playerIdx] != 1) {
                TB_Sprintf(str, "%s\n%s", (*gPlayers[playerIdx].save)->name, gText.single[214]);
            } else {
                u32 min;
                u32 sec;
                u32 thou;

                min = (u32)pc_cars[gPlayers[playerIdx].slot].score;
                min = min / 60 % 100;
                sec = (u32)pc_cars[gPlayers[playerIdx].slot].score % 60;
                thou = (u32)(1000.0f * pc_cars[gPlayers[playerIdx].slot].score);
                thou %= 1000;
                TB_Sprintf(str, "%s\n%s %d:%02d.%03d", (*gPlayers[playerIdx].save)->name, gText.single[208],
                             min, sec, thou);
            }
        } else if (gRace == kBattle) {
            TB_Sprintf(str, "%s\n%d %s", (*gPlayers[playerIdx].save)->name, pc_cars[playerIdx].battle.Points,
                         gText.single[130]);
        } else {
            char str1[32], str2[32];
            s32 place = pc_cars[gPlayers[playerIdx].slot].place;

            TB_Sprintf(str1, gText.single[198], place + 1,
                         gText.taba[gText.tab[TEXT_PLACE_SUFFIXES] + place]);
            if (gNumLaps != pc_models[gPlayers[playerIdx].slot].laps) {
                TB_Sprintf(str2, gText.single[207], pc_models[gPlayers[playerIdx].slot].laps, gNumLaps);
            } else {
                u32 min;
                u32 sec;
                u32 thou;

                min = (u32)pc_cars[gPlayers[playerIdx].slot].score;
                min = min / 60 % 100;
                sec = (u32)pc_cars[gPlayers[playerIdx].slot].score % 60;
                thou = (u32)(1000.0f * pc_cars[gPlayers[playerIdx].slot].score);
                thou %= 1000;
                TB_Sprintf(str2, "%s %d:%02d.%03d", gText.single[208], min, sec, thou);
            }
            TB_Sprintf(str, "%s\n%s\n%s", (*gPlayers[playerIdx].save)->name, str1, str2);
        }
        TB_DrawTextScrollBox(x, y, 160, 110, -1, 0, str);
    }
    HAL_SetTextDepth(-1.0f);
    TB_SetJust(0, 3);
    return 1;
}

/* HAL_SetFont: the text font, under the text lock; returns the one before. */
s32 ar_font_set(s32 font)
{
    s32 old;
    lock_take(&text_lock_queue);
    old = HAL_SetFontInternal(font);
    lock_give(&text_lock_queue);
    return old;
}

