#include "game/game.h"

typedef struct Rect { s16 x,y,w,h; s16 unknown[2]; } Rect;
typedef struct TextInfo { char unknown[54]; u16 index; } TextInfo;
/* PC: gText's header has host pointers (port/assets/text.c), so info and strings sit at host offsets. */
typedef struct TextState { void *unknown[3]; TextInfo *info; char **strings; } TextState;
VIEW(TextState, gText);
#define gText gText_view

VIEW(s8 *[3], ghostPanes);
#define ghostPanes ghostPanes_view
VIEW(s8 *[2], scorePanes);
#define scorePanes scorePanes_view

VIEW(Rect[][4], highscore_gPlayerRects);
#define highscore_gPlayerRects highscore_gPlayerRects_view

VIEW(struct pc_multiblit[1], highscore_mblit);
#define highscore_mblit highscore_mblit_view
extern void RemoveBlit(intptr_t);

extern void *InitPane(s32,s32,s32,s32,s32,intptr_t,s32,s32);

extern intptr_t NewMultiBlit(s32,s32,void *,s32);

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
    case 1: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case 2:
        if (codePane != 0) {
            RemovePane(codePane);
            codePane = 0;
        }
        break;
    case 3: {
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] != 0) {
                RemovePane(namePanes[p]);
                namePanes[p] = 0;
            }
        }
        break;
    }
    case 4: {
        s32 i;
        for (i = 0; i < 3; i++) {
            if (ghostPanes[i] != 0) {
                RemovePane(ghostPanes[i]);
                ghostPanes[i] = 0;
            }
        }
        break;
    }
    case 5: {
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
    case 0:
        highscore_UninitScreen();
        break;
    case 1: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, 176, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case 2:
        codePane = InitPane(83, 70, 237, 136, 192, 0, 0, 0);
        SetCountdownTimer(600.0f);
        break;
    case 3: {
        s32 p;
        for (p = 0; p < gNumPlayers; p++) {
            if (namePanes[p] == 0) {
                namePanes[p] = InitPane(highscore_gPlayerRects[gNumWindows - 1][p].x,
                    highscore_gPlayerRects[gNumWindows - 1][p].y,
                    highscore_gPlayerRects[gNumWindows - 1][p].x + highscore_gPlayerRects[gNumWindows - 1][p].w,
                    highscore_gPlayerRects[gNumWindows - 1][p].y + highscore_gPlayerRects[gNumWindows - 1][p].h, 176, 0, 0, 0);
            }
        }
        SetCountdownTimer(600.0f);
        break;
    }
    case 4: {
        s32 pak;
        if (ghostPanes[0] == 0) {
            HAL_SetFont(13);
            width = HAL_GetTextWidth(ghost_menu_title_text, -1);
            ghostPanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
        }
        if (ghostPanes[1] == 0) {
            HAL_SetFont(10);
            ghostPanes[1] = InitPane(91, 51, 229, TB_GetFontLineHeight() * 3 + 59, 176, 0, 0, 0);
        }
        if (ghostPanes[2] == 0) {
            HAL_SetFont(10);
            ghostPanes[2] = InitPane(91, 106, 229, TB_GetFontLineHeight() * 5 + 114, 176, 0, 0, 0);
        }
        SetCountdownTimer(600.0f);
        gGhostMenuSelect = 0;
        gGhostPak = 0;
        for (pak = 0; pak < 4; pak++) {
            FindGhosts(pak);
        }
        break;
    }
    case 5:
        gTable = (gRace == 2) ? 2 : 0;
        PrintScores();
        if (scorePanes[0] == 0) {
            HAL_SetFont(13);
            width = HAL_GetTextWidth(gText.strings[gText.info->index + gTable], -1);
            scorePanes[0] = InitPane(160 - width / 2 - 8, 6, width / 2 + 168, TB_GetFontLineHeight() + 14, 176, 0, 0, 0);
        }
        if (scorePanes[1] == 0) {
            scorePanes[1] = InitPane(8, 75, 312, 200, 176, 0, 0, 0);
        }
        if (gRace == 2) {
            SetCountdownTimer(30.0f);
        } else {
            SetCountdownTimer(15.0f);
        }
        break;
    }
    if (gHighScoreState != 0) {
        highscore_blit = NewMultiBlit(0, 0, highscore_mblit, 1);
    }
}
