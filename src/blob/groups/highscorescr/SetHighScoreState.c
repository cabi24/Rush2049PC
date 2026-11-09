/* SetHighScoreState: the high-score screen's page changes (MAT3 highscorescr.c SetHighScoreState). */
#include "game/game.h"

VIEW(s8 *[3], ghostPanes);
#define ghostPanes ghostPanes_view
VIEW(s8 *[2], scorePanes);
#define scorePanes scorePanes_view

VIEW(struct pc_multiblit[1], highscore_mblit);
#define highscore_mblit highscore_mblit_view
extern void RemoveBlit(intptr_t);

extern void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);

extern intptr_t NewMultiBlit(s32,s32,void *,s32);

#define PANE_STYLE_CODE 192         /* the circuit code's box */
#define TEXT_SCORE_TITLES 27        /* gText.tab[]: the tables' titles (gTable 0-2) */

/* Go to page newState (kHighScore*): the text blit goes, the old page's boxes go, the new page's come up and its
 * time limit starts (CountdownTimeOut): the unlocked notices and name entry a box per player, the circuit code one
 * box, the ghost menu three (its title, the pak's status, the choices: all four paks are searched for ghosts), the
 * tables two (a ghost race shows its table 2 for 30 s, any other race its table 0 for 15 s). kHighScoreNone tears the
 * screen down. Every other page gets a new text blit. */
void SetHighScoreState(s32 newState)
{
    s32 p;
    s32 width;

    if (newState == gHighScoreState) return;
    if (highscore_blit != 0) {
        RemoveBlit(highscore_blit);
        highscore_blit = 0;
    }
    switch (gHighScoreState) {
    case kHighScoreUnlocked: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case kHighScoreCircuitCode:
        if (codePane != 0) {
            RemovePane(codePane);
            codePane = 0;
        }
        break;
    case kHighScoreNames: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case kHighScoreGhost: {
        s32 i;
        for (i = 0; i < 3; i++) {
            if (ghostPanes[i] != 0) {
                RemovePane(ghostPanes[i]);
                ghostPanes[i] = 0;
            }
        }
        break;
    }
    case kHighScoreScores: {
        s32 i;
        for (i = 0; i < 2; i++) {
            if (scorePanes[i] != 0) {
                RemovePane(scorePanes[i]);
                scorePanes[i] = 0;
            }
        }
        break;
    }
    }
    gHighScoreState = newState;
    switch (gHighScoreState) {
    case kHighScoreNone:
        highscore_UninitScreen();
        break;
    case kHighScoreUnlocked: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, PANE_STYLE, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case kHighScoreCircuitCode:
        codePane = InitPane(83, 70, 237, 136, PANE_STYLE_CODE, 0, 0, 0);
        SetCountdownTimer(600.0f);
        break;
    case kHighScoreNames: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, PANE_STYLE, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case kHighScoreGhost: {
        s32 pak;
        if (ghostPanes[0] == 0) {
            HAL_SetFont(13);
            width = HAL_GetTextWidth(ghost_menu_title_text, -1);
            ghostPanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, PANE_STYLE, 0, 0, 0);
        }
        if (ghostPanes[1] == 0) {
            HAL_SetFont(10);
            ghostPanes[1] = InitPane(91, 51, 229, TB_GetFontLineHeight() * 3 + 59, PANE_STYLE, 0, 0, 0);
        }
        if (ghostPanes[2] == 0) {
            HAL_SetFont(10);
            ghostPanes[2] = InitPane(91, 106, 229, TB_GetFontLineHeight() * 5 + 114, PANE_STYLE, 0, 0, 0);
        }
        SetCountdownTimer(600.0f);
        gGhostMenuSelect = 0;
        gGhostPak = 0;
        for (pak = 0; pak < 4; pak++) {
            FindGhosts(pak);
        }
        break;
    }
    case kHighScoreScores:
        gTable = (gRace == kGhost) ? 2 : 0;
        PrintScores();
        if (scorePanes[0] == 0) {
            HAL_SetFont(13);
            width = HAL_GetTextWidth(gText.taba[gText.tab[TEXT_SCORE_TITLES] + gTable], -1);
            scorePanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, PANE_STYLE, 0, 0, 0);
        }
        if (scorePanes[1] == 0) {
            scorePanes[1] = InitPane(8, 75, 312, 200, PANE_STYLE, 0, 0, 0);
        }
        if (gRace == kGhost) {
            SetCountdownTimer(30.0f);
        } else {
            SetCountdownTimer(15.0f);
        }
        break;
    }
    if (gHighScoreState != kHighScoreNone) {
        highscore_blit = NewMultiBlit(0, 0, highscore_mblit, 1);
    }
}
