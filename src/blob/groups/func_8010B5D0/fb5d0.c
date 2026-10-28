/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010B5D0 (0x8010B5D0, 556 B): pause/option menu-page callback drawing one text line centred on
 * D_80117280 (fcvt "%d"-style controller line when D_80117350, language string for modes 7/8, empty for 6,
 * else the formatted player-name line in its colour D_801146AC[..]).
 * draft -> 2/139 -> unit EQUAL.
 * Shaping devices (disclosed):
 * - inlined helpers (non-static, uniquely named, not on keep: stub rule) gfx_lock/gfx_unlock/font_set/item_refresh/TB_CenterText (GC TB_text.c `s16
 *    TB_CenterText(void *text, s16 centerX)`); item_refresh returns the dirty flag.
 * - the three lock/font helpers are written on ONE source line (lever, as TitlePane): with one
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
#ifndef TARGET_PC
typedef u32 uintptr_t;
typedef s32 intptr_t;
#endif
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

#ifdef TARGET_PC
/* PC: the page's blit (the pause screen's NewMultiBlit set; the page runs as its AnimFunc), over the Blit's host
 * layout (sound_control.c's, docs/port/POINTERS.md 9.3): `dirty` is its Hide, at +0x26. The N64's +0x1A is the low
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
#else
typedef struct {
    u8 pad0[0x1A];
    s8 dirty;
} MenuItem;
#endif

typedef struct {
    u8 pad0[0x70];
    u16 lang;
} LangInfo;

#ifdef TARGET_PC
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
#else
typedef struct {
    u8 pad0[0x3A4];
    s32 unk3A4;
} TblHdr;

typedef struct {
    u8 pad0[4];
    TblHdr *hdr;
    u8 pad8[4];
    LangInfo *info;
    s32 *strings;
} StrTable;
#endif

extern OSMesgQueue D_801461D0;
extern s16 D_80117280[2];
extern s32 D_80149D98;
extern StrTable D_8017A4E0;

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 slot_state_setup(s32 selection);
void Input_ApplyPadConfig(void *);
void render_helper(f32);
s16 object_bytes_sum_global(void);
u32 object_manager_update(void *, s32);
void dispatch_handler(s32);
void fcvt_wrapper(char *, char *, ...);

typedef struct {
    u8 pad0[0x48];
#ifdef TARGET_PC
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
#else
    s32 *name;
#endif
} PlayerRec;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */
#ifndef TARGET_PC
#define PROFILE_OFS(n) n
#endif

extern s8 D_80117350;
extern s32 D_801170FC;
extern s32 D_8015698C;
extern char D_801210CC[];
extern char D_801210E0[];
extern PlayerRec D_8014A118[];
extern s32 D_801146AC[];
void state_utility(s16, s16, void *);

void gfx_lock_B5D0(void) { osRecvMesg(&D_801461D0, (void *)0, 1); } void gfx_unlock_B5D0(void) { osSendMesg(&D_801461D0, (void *)0, 0); } s32 font_set_B5D0(s32 font) { s32 old; gfx_lock_B5D0(); old = slot_state_setup(font); gfx_unlock_B5D0(); return old; }

s32 item_refresh_B5D0(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        Input_ApplyPadConfig(item);
    }
    return item->dirty;
}

s16 TB_CenterText_B5D0(void *text, s16 centerX)
{
    return centerX - (object_manager_update(text, -1) >> 1);
}

s32 func_8010B5D0(MenuItem *item) {
    s32 i;
    s32 x;
    s32 y;
    intptr_t str;
    char buf[40];
    s32 v;

    if (item_refresh_B5D0(item)) {
        return 1;
    }
    render_helper(0.0f);
    font_set_B5D0(13);
    x = D_80117280[0];
    y = D_80117280[1];
    str = (intptr_t)buf;
    if (D_80117350) {
        fcvt_wrapper(buf, D_801210CC, D_8015698C + 1, D_8017A4E0.hdr->unk3A4);
        dispatch_handler(1);
    } else {
        v = D_801170FC;
        if (v == 7 || v == 8) {
            str = D_8017A4E0.strings[D_8017A4E0.info->lang + v];
            dispatch_handler(1);
        } else if (v == 6) {
            buf[0] = 0;
        } else {
            fcvt_wrapper(buf, D_801210E0, *D_8014A118[D_8015698C].name + PROFILE_OFS(20), D_8017A4E0.strings[D_8017A4E0.info->lang]);
            dispatch_handler(D_801146AC[D_8015698C]);
        }
    }
    state_utility(TB_CenterText_B5D0((void *)str, x), y, (void *)str);
    render_helper(-1.0f);
    return 1;
}

