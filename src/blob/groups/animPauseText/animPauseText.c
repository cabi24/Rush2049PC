#include "game/game.h"

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

s32 item_refresh_B7FC(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        UpdateBlit(item);
    }
    return item->dirty;
}

s32 animPauseText(MenuItem *item) {
    s32 i;
    s32 x;
    s32 y;
    intptr_t str;

    if (item_refresh_B7FC(item)) {
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
