/* The pause screen's pages (MAT3 pausescr.c setPauseState and _updatePauseStateCreate): the panes and blits each
 * page is made of. */
#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;

void *InitPane(s32, s32, s32, s32, s32, void *, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);

/* HAL_SetFont without its result. */
void ps_SetFont(s32 font)
{
    HAL_SetFont(font);
}

/* Makes the page of the pause state just entered (gPauseState, MAT3 kPause*): the main page (1) dims the screen
 * and puts a pane behind the eight lines of the menu, as wide as the widest; the rumble page (6) a pane for its
 * title and one for each player's rumble state; restart and quit (7, 8) a pane behind the question. Each has the
 * title pane and blit. updatePauseStateCleanup removed the last page's first. */
void ps_updatePauseStateCreate(void)
{
    s32 max;
    s32 width;
    s32 i;
    s32 x;
    s32 y;

    switch (gPauseState) {
    case 1:
        if (!pause_DimBlit) pause_DimBlit = NewMultiBlit(0, 0, &DimMultiBlit, 1);
        pause_TitlePane();
        ps_SetFont(13);
        x = pause_titlePos[0];
        y = pause_titlePos[1] + 2 * TB_GetFontLineHeight();
        ps_SetFont(10);
        max = 0;
        for (i = 0; i < 8; i++) {
            width = HAL_GetTextWidth(gText.taba[i + 1 + gText.tab[56]], -1);
            if (max < width) max = width;
        }
        InitPane(x - max / 2 - 8, y - 4, x + max / 2 + 8, y + 8 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_PauseBlit = NewMultiBlit(0, 0, &PauseMultiBlit, 1);
        break;
    case 6:
        pause_gYes = 0;
        pause_TitlePane();
        ps_SetFont(10);
        x = pause_gPlayerRects[0][1].cx;
        y = pause_gPlayerRects[0][1].cy - TB_GetFontLineHeight() / 2;
        width = HAL_GetTextWidth(gText.single[226], -1);
        InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + TB_GetFontLineHeight() + 4, 176, animControllerDimInternal, 0, 0);
        ps_SetFont(11);
        width = HAL_GetTextWidth(gText.single[171], -1);
        width = (u32)width > HAL_GetTextWidth(gText.single[170], -1) ? width : HAL_GetTextWidth(gText.single[170], -1);
        for (i = 0; i < gNumPlayers; i++) {
            x = pause_gPlayerRects[gNumPlayers - 1][i].cx;
            y = pause_gPlayerRects[gNumPlayers - 1][i].cy - TB_GetFontLineHeight();
            InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + 2 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        }
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_RumbleBlit = NewMultiBlit(0, 0, &RumbleMultiBlit, 1);
        break;
    case 7:
    case 8:
        pause_gYes = 0;
        pause_TitlePane();
        ps_SetFont(13);
        x = pause_titlePos[0];
        y = pause_titlePos[1] + 2 * TB_GetFontLineHeight();
        ps_SetFont(10);
        width = HAL_GetTextWidth(gText.single[216], -1);
        InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + 2 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_SureBlit = NewMultiBlit(0, 0, &SureMultiBlit, 1);
        break;
    }
}

/* Goes to pause state state (MAT3 kNoPause ... kPauseQuit): the old page's panes and blits go, the new one's are
 * made. */
void setPauseState(s32 state)
{
    gPauseState = state;
    updatePauseStateCleanup();
    ps_updatePauseStateCreate();
}
