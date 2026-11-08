/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animPauseText (0x8010B7FC, 460 B): menu page listing 8 language strings (StrTable strings[lang + i + 1]) centred
 * on pause_titlePos[0], starting at pause_titlePos[1] + 2 line heights, selection pauseMenuSelect in colour 22; font 13 then 10.
 * Levers: TB_CenterText inlined (GC TB_text.c; the family's narrowing-through-temp residual) 42 -> 2 words;
 * `y = pause_titlePos[1] + TB_GetFontLineHeight() * 2` operand order (ugen ring t6 = the shift) -> EQUAL.
 * Body otherwise (item_refresh returns the dirty flag, kept in v1 by retail).
 * MATCH in group / unit EQUAL. Shaping (disclosed): inlined helpers gfx_lock, gfx_unlock, font_set, item_refresh, TB_CenterText (non-static *_B7FC);
 * gfx_lock/gfx_unlock definitions placement as in (as1 line tie).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the page's blit (the pause screen's NewMultiBlit set; the page runs as its AnimFunc), over the Blit's host
 * layout (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): `dirty` is its Hide, at +0x26. The N64's +0x1A is the low
 * byte of X on the PC, so the page moved its blit and never showed or hid it (docs/port/LAYOUT.md 9). */
typedef struct {
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

extern OSMesgQueue text_lock_queue;
extern s16 pause_titlePos[2];
extern s32 pauseMenuSelect;
extern StrTable gText;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
void HAL_SetTextDepth(f32);
s16 TB_GetFontLineHeight(void);
u32 HAL_GetTextWidth(void *, s32);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);

void gfx_lock_B7FC(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_B7FC(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_B7FC(s32 font)
{
    s32 old;

    gfx_lock_B7FC();
    old = HAL_SetFontInternal(font);
    gfx_unlock_B7FC();
    return old;
}

s32 item_refresh_B7FC(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        UpdateBlit(item);
    }
    return item->dirty;
}

s16 TB_CenterText_B7FC(void *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
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
    font_set_B7FC(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    font_set_B7FC(10);
    for (i = 0; i < 8; i++) {
        TB_SetTextColor(i == pauseMenuSelect ? 22 : 1);
        str = gText.strings[gText.info->lang + i + 1];
        TB_DrawText(TB_CenterText_B7FC((void *)str, x), y, (void *)str);
        y += TB_GetFontLineHeight();
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
