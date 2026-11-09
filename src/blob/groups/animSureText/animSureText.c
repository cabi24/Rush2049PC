/* The pause screen's "are you sure" text (MAT3 pausescr.c animSureText): an animation function of a blit. */
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
    u8 pad0[0x30];
    u16 sel;
    u8 pad32[0x70 - 0x32];
    u16 lang;
} LangInfo;

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct {
    char *pad0[0x360 / 4];
    intptr_t unk360;
    char *pad364[(0x3A4 - 0x364) / 4];
    intptr_t unk3A4;
} TblHdr;

typedef struct {
    u8 pad0[4];
    TblHdr *hdr;
    u8 pad8[4];
    LangInfo *info;
    intptr_t *strings;
} StrTable;

VIEW(s16[2], pause_titlePos);
#define pause_titlePos pause_titlePos_view

VIEW(StrTable, gText);
#define gText gText_view

/* Sets item's hidden flag to off, redrawing its blit when that changes; returns the flag. */
s32 item_disable_B9C8(MenuItem *item, s32 off)
{
    if (off != item->dirty) {
        item->dirty = off;
        UpdateBlit(item);
    }
    return item->dirty;
}

/* For restart and quit (gPauseState 7 and 8: MAT3 kPauseRestart, kPauseQuit): the question (message 216) under
 * the title, and yes and no beside each other a quarter of its width from the centre, the chosen one (pause_gYes)
 * highlighted. Hidden in the other pause states. Returns 1: the blit stays. */
s32 animSureText(MenuItem *item) {
    s32 w;
    s32 x;
    s32 y;
    intptr_t str;

    if (item_disable_B9C8(item, gPauseState < 7)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    HAL_SetFont(10);
    TB_SetTextColor(1);
    str = gText.hdr->unk360;
    TB_DrawText(TB_CenterText((void *)str, x), y, (void *)str);
    y += TB_GetFontLineHeight();
    w = HAL_GetTextWidth((void *)gText.hdr->unk360, -1);
    x = pause_titlePos[0] - w / 4;
    str = gText.strings[gText.info->sel];
    TB_SetTextColor(pause_gYes ? 22 : 1);
    TB_DrawText(TB_CenterText((void *)str, x), y, (void *)str);
    x = pause_titlePos[0] + w / 4;
    str = gText.strings[gText.info->sel + 1];
    TB_SetTextColor(pause_gYes ? 1 : 22);
    TB_DrawText(TB_CenterText((void *)str, x), y, (void *)str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}



