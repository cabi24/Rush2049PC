/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animTitleText (0x8010B5D0, 556 B): pause/option menu-page callback drawing one text line centred on
 * pause_titlePos (fcvt "%d"-style controller line when pause_gControllerRemoved, language string for modes 7/8, empty for 6,
 * else the formatted player-name line in its colour playerTextColor[..]).
 * draft -> 2/139 -> unit EQUAL.
 * Shaping devices (disclosed):
 * - inlined helpers (non-static, uniquely named, not on keep: stub rule) gfx_lock/gfx_unlock/font_set/item_refresh/TB_CenterText (GC TB_text.c `s16
 *    TB_CenterText(void *text, s16 centerX)`); item_refresh returns the dirty flag.
 * - the three lock/font helpers are written on ONE source line (lever, as video_TitlePane): with one
 *    inlined font_set site umerge keeps the helpers' own .loc lines, and as1 then breaks the tie between
 *    font_set's `sw v0,68(sp)` (old) and gfx_unlock's osSendMesg argument moves by source line. On one line the
 *    tie falls back to ugen order: lui/addiu a0, sw v0,68(sp), move a1, jal, move a2 (retail).
 *  - `s32 i` is an UNUSED local = exact frame residual: retail frame 152; without it 144 (16 words differ,
 *    every named home 4/8 B off); one 4-byte slot at the top of the locals.
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

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct {
    char *pad0[0x3A4 / 4];
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
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
} PlayerRec;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */

extern s8 pause_gControllerRemoved;
extern s32 gPauseState;
extern s32 playerInPause;
extern char pause_controller_removed_format[];
extern char pause_player_title_format[];
extern PlayerRec gPlayers[];
extern s32 playerTextColor[];
void TB_DrawText(s16, s16, void *);

void gfx_lock_B5D0(void) { osRecvMesg(&text_lock_queue, (void *)0, 1); } void gfx_unlock_B5D0(void) { osSendMesg(&text_lock_queue, (void *)0, 0); } s32 font_set_B5D0(s32 font) { s32 old; gfx_lock_B5D0(); old = HAL_SetFontInternal(font); gfx_unlock_B5D0(); return old; }

s32 item_refresh_B5D0(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        UpdateBlit(item);
    }
    return item->dirty;
}

s16 TB_CenterText_B5D0(void *text, s16 centerX)
{
    return centerX - (HAL_GetTextWidth(text, -1) >> 1);
}

s32 animTitleText(MenuItem *item) {
    s32 i;
    s32 x;
    s32 y;
    intptr_t str;
    char buf[40];
    s32 v;

    if (item_refresh_B5D0(item)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    font_set_B5D0(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1];
    str = (intptr_t)buf;
    if (pause_gControllerRemoved) {
        TB_Sprintf(buf, pause_controller_removed_format, playerInPause + 1, gText.hdr->unk3A4);
        TB_SetTextColor(1);
    } else {
        v = gPauseState;
        if (v == 7 || v == 8) {
            str = gText.strings[gText.info->lang + v];
            TB_SetTextColor(1);
        } else if (v == 6) {
            buf[0] = 0;
        } else {
            TB_Sprintf(buf, pause_player_title_format, *gPlayers[playerInPause].name + PROFILE_OFS(20), gText.strings[gText.info->lang]);
            TB_SetTextColor(playerTextColor[playerInPause]);
        }
    }
    TB_DrawText(TB_CenterText_B5D0((void *)str, x), y, (void *)str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

