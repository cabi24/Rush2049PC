/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animSureText (0x8010B9C8, 700 B): two-choice menu page (header string centred, two option strings at x -/+ w/4,
 * selection pause_gYes highlighted), disabled while gPauseState >= 7 (item_disable returns the flag).
 * Levers: TB_CenterText inlined (GC TB_text.c) 108 -> 30 words; dropping frame devices (nested
 * font_select, unused `s32 i`: the inlined TB_CenterText sites now give retail's 152-byte frame) -> 3; `y =
 * pause_titlePos[1] + TB_GetFontLineHeight() * 2` operand order -> EQUAL. No unused locals.
 * MATCH in group / unit EQUAL. Shaping (disclosed): inlined helpers gfx_lock, gfx_unlock, font_set, item_disable, TB_CenterText (non-static *_B9C8);
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
void TB_Sprintf(char *, char *, ...);

typedef struct {
    u8 pad0[0x48];
    s32 *name;
} PlayerRec;

extern s8 pause_gControllerRemoved;
extern s8 pause_gYes;
extern s32 gPauseState;
extern s32 playerInPause;
extern char pause_controller_removed_format[];
extern char pause_player_title_format[];
extern PlayerRec gPlayers[];
extern s32 playerTextColor[];
void TB_DrawText(s16, s16, void *);

void gfx_lock_B9C8(void);
void gfx_unlock_B9C8(void);
s32 font_set_B9C8(s32 font)
{
    s32 old;

    gfx_lock_B9C8();
    old = HAL_SetFontInternal(font);
    gfx_unlock_B9C8();
    return old;
}

s32 item_disable_B9C8(MenuItem *item, s32 off)
{
    if (off != item->dirty) {
        item->dirty = off;
        UpdateBlit(item);
    }
    return item->dirty;
}

s16 TB_CenterText_B9C8(void *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}

s32 animSureText(MenuItem *item) {
    s32 w;
    s32 x;
    s32 y;
    intptr_t str;

    if (item_disable_B9C8(item, gPauseState < 7)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    font_set_B9C8(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1] + TB_GetFontLineHeight() * 2;
    font_set_B9C8(10);
    TB_SetTextColor(1);
    str = gText.hdr->unk360;
    TB_DrawText(TB_CenterText_B9C8((void *)str, x), y, (void *)str);
    y += TB_GetFontLineHeight();
    w = HAL_GetTextWidth((void *)gText.hdr->unk360, -1);
    x = pause_titlePos[0] - w / 4;
    str = gText.strings[gText.info->sel];
    TB_SetTextColor(pause_gYes ? 22 : 1);
    TB_DrawText(TB_CenterText_B9C8((void *)str, x), y, (void *)str);
    x = pause_titlePos[0] + w / 4;
    str = gText.strings[gText.info->sel + 1];
    TB_SetTextColor(pause_gYes ? 1 : 22);
    TB_DrawText(TB_CenterText_B9C8((void *)str, x), y, (void *)str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

void gfx_lock_B9C8(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_B9C8(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

