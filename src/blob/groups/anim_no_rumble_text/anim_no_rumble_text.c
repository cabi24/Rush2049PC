/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * anim_no_rumble_text (0x80106B3C, 600 B): per-window HUD title text (StrTable +0x388), hidden unless gGameState bit 22;
 * font 11, or font 12 when the text is wider than the window limit + 8; drawn for each window whose player's
 * controller record (gPaks[ctl], stride 772) has b6 == 0, while gGameState bit 21 is set and gPauseState == 0.
 * MATCH in group / unit EQUAL (draft was 50/150, EQUIVALENT 4,832 calls on race + pause_menus).
 * Levers: TB_SetJust(1,1)/(0,3) = the locked TB_SetJust body inlined (pointer to gJust; GC name);
 * `ctl = gPlayers[i].control` as a named local (fixes s5-s7); the b6 test through an inlined getter whose
 * value return takes v0 (w15 inlined-value-return mechanism; a named local `b` got v1); the lock/font helpers
 * DEFINED AFTER the function (prototypes before): as1 breaks the osRecvMesg-argument vs TB_SetJust-store tie by
 * .loc line, retail's stores come first.
 * Shaping (disclosed): inlined helpers Hidden, TB_SetJust, getB6, gfx_lock, gfx_unlock, font_set (non-static, *_6B3C) (identity
 * getB6/TB_SetJust as separate source helpers is a hypothesis; the font_set family is the established idiom);
 * helper definition placement after the function (as1 line-number tie).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef struct { u32 opaque[6]; } OSMesgQueue;

/* PC: the Blit's host layout (NewMultiBlit.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    void *name, *image, *info;
    u16 texIndex;
    s16 x, y;
    u16 z;
    s16 width, height;
    u8 alpha, flip;
    s8 hidden;
    u8 init;
    s16 top, bot, left, right, color;
    u16 reserved;
    intptr_t state;     /* AnimFunc */
} Blit;

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0x388 / 4];
    char *text;
} TblHdr;

typedef struct Player {
    u8 b0;
    u8 control;
    u8 pad2[0x48 - 0x2]; void *ref;
} Player;

/* PC: the controller/pak record's host layout (paksys' PakRecord, docs/port/POINTERS.md 9): its OSPfs at +0x0C
 * holds a pointer, so from there on the record is 4..12 bytes later and 0x310 bytes long. */
typedef struct { s32 status; void *queue; s32 channel; u8 id[32], label[32]; s32 version, dir_size, inode_table,
    minode_table, dir_table, inode_start_page; u8 banks, activebank; } PcPfs;   /* OSPfs */
typedef struct Control {
    u8 pad0[6];
    s8 b6;
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Control;

typedef struct Pos {
    s32 x, y;
} Pos;

typedef struct Pair {
    s32 a, b;
} Pair;

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern Pair gJust;
extern TblHdr *gText_single;
extern s16 gNumWindows;
extern s32 gPauseState;
extern Player gPlayers[];
extern Control gPaks[];
extern Pos no_rumble_text_pos[][4];
extern s32 playerTextColor[];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
u32 HAL_GetTextWidth(void *, s32);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);

void TB_SetJust_6B3C(u32 first, u32 second)
{
    Pair *p = &gJust;
    p->a = first;
    p->b = second;
}

s32 Hidden_6B3C(Blit *blt, s32 hide)
{
    if (blt->hidden != hide) {
        blt->hidden = hide;
        UpdateBlit(blt);
    }
    return blt->hidden;
}


s32 getB6_6B3C(s32 ctl)
{
    return gPaks[ctl].b6;
}

s32 HAL_SetFont(s32 fontID);
void text_lock(void);
void text_unlock(void);
s32 anim_no_rumble_text(Blit *blt)
{
    u32 width;
    s32 limit;
    s32 i;
    s32 x;
    s32 y;
    s32 ctl;

    if (Hidden_6B3C(blt, gGameState & 0x400000)) {
        blt->state = 0;
        return 0;
    }
    TB_SetJust_6B3C(1, 1);
    HAL_SetFont(11);
    width = HAL_GetTextWidth(gText_single->text, -1);
    limit = (gNumWindows >= 3) ? 160 : ((gNumWindows >= 2) ? 240 : 320);
    if (limit + 8 < width) {
        text_lock();
        HAL_SetFontInternal(12);
        text_unlock();
    }
    for (i = 0; i < gNumWindows; i++) {
        if ((gGameState & 0x200000) && !gPauseState) {
            ctl = gPlayers[i].control;
            if (!getB6_6B3C(ctl)) {
                x = no_rumble_text_pos[gNumWindows - 1][i].x;
                y = no_rumble_text_pos[gNumWindows - 1][i].y;
                TB_SetTextColor(playerTextColor[i]);
                TB_DrawText(x, y, gText_single->text);
            }
        }
    }
    TB_SetJust_6B3C(0, 3);
    return 1;
}




