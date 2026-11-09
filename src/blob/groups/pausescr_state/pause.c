#include "game/game.h"

typedef struct Blit Blit;
typedef struct MultiBlit MultiBlit;
typedef struct TextState { void *word0; char **single; void *word8; u16 *tab; char **taba; } TextState;
typedef struct { s16 x, y; } Pos;
typedef struct { s32 x, y, w, h, cx, cy; } Rect;   /* 0x18 */

VIEW(TextState, gText);
#define gText gText_view

VIEW(Blit *, pause_DimBlit);
#define pause_DimBlit pause_DimBlit_view
VIEW(Blit *, pause_TitleBlit);
#define pause_TitleBlit pause_TitleBlit_view
VIEW(Blit *, pause_PauseBlit);
#define pause_PauseBlit pause_PauseBlit_view
VIEW(Blit *, pause_SureBlit);
#define pause_SureBlit pause_SureBlit_view
VIEW(Blit *, pause_RumbleBlit);
#define pause_RumbleBlit pause_RumbleBlit_view
VIEW(const MultiBlit, DimMultiBlit);
#define DimMultiBlit DimMultiBlit_view
VIEW(const MultiBlit, TitleMultiBlit);
#define TitleMultiBlit TitleMultiBlit_view
VIEW(const MultiBlit, PauseMultiBlit);
#define PauseMultiBlit PauseMultiBlit_view
VIEW(const MultiBlit, SureMultiBlit);
#define SureMultiBlit SureMultiBlit_view
VIEW(const MultiBlit, RumbleMultiBlit);
#define RumbleMultiBlit RumbleMultiBlit_view
            VIEW(Pos, pause_titlePos);
#define pause_titlePos pause_titlePos_view

VIEW(Rect[][4], pause_gPlayerRects);
#define pause_gPlayerRects pause_gPlayerRects_view

void *InitPane(s32, s32, s32, s32, s32, void *, s32, s32);
Blit *NewMultiBlit(s16, s16, const MultiBlit *, s16);

void ps_SetFont(s32 font)
{
    HAL_SetFont(font);
}

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
        x = pause_titlePos.x;
        y = pause_titlePos.y + 2 * TB_GetFontLineHeight();
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
        x = pause_titlePos.x;
        y = pause_titlePos.y + 2 * TB_GetFontLineHeight();
        ps_SetFont(10);
        width = HAL_GetTextWidth(gText.single[216], -1);
        InitPane(x - width / 2 - 8, y - 4, x + width / 2 + 8, y + 2 * TB_GetFontLineHeight() + 4, 176, 0, 0, 0);
        pause_TitleBlit = NewMultiBlit(0, 0, &TitleMultiBlit, 1);
        pause_SureBlit = NewMultiBlit(0, 0, &SureMultiBlit, 1);
        break;
    }
}

void setPauseState(s32 state)
{
    gPauseState = state;
    updatePauseStateCleanup();
    ps_updatePauseStateCreate();
}
