/* The pause screen's menu text (MAT3 pausescr.c animPauseText): an animation function of a blit, run each frame
 * the blit is drawn. */
#include "game/game.h"

/* The blit the animation function runs for (MAT3 _tBlit_2's leading fields). */
typedef struct MenuItem {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 dirty;
} MenuItem;

typedef struct {
    u8 pad0[0x70];
    u16 lang;
} LangInfo;

/* PC: gText's header has host pointers (port/assets/text.c), so its fields sit at host offsets */
typedef struct {
    void *pad0[3];
    LangInfo *info;
    intptr_t *strings;
} StrTable;

VIEW(s16[2], pause_titlePos);
#define pause_titlePos pause_titlePos_view

VIEW(StrTable, gText);
#define gText gText_view

/* Draws the pause menu's eight lines (the language's pause words) under the title, centred, the chosen one
 * (pauseMenuSelect) highlighted. Returns 1: the blit stays. */
s32 animPauseText(MenuItem *item) {
    s32 i;
    s32 x;
    s32 y;
    intptr_t str;

    if (Hidden((struct Blit *)item, 0)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    for (i = 0; i < 8; i++) {
        TB_SetTextColor(i == pauseMenuSelect ? 22 : 1);
        str = gText.strings[gText.info->lang + i + 1];
        TB_DrawText(TB_CenterText((void *)str, x), y, (void *)str);
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
