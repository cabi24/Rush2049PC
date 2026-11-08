/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * animRumbleText (0x8010BC84, 928 B): pause-screen controller status text (gPauseState == 6). If any player's
 * controller record (gPaks[ctl].b6, stride 772) is 0, a centred title (StrTable +0x388, font 10, colour 22) at
 * pause_gPlayerRects[0][1] - lineheight/2; then per player (count gNumPlayers) the player name (record +72 -> +20) and
 * "OK"/"NOT OK" (StrTable +0x2A8/+0x2AC), each centred on pause_gPlayerRects[n-1][i].x, font 11.
 * MATCH in group / unit EQUAL (draft 160/232, EQUIVALENT 90 calls on pause_menus).
 * Levers: TB_CenterText(text, centerX) inlined (GC TB_text.c: s16 TB_CenterText(void *text, s16 centerX) =
 * centerX - width/2 narrowed at return; narrowed again for TB_DrawText = TB_DrawText's s16 x: this is the
 * "x - (w >> 1) narrowing through a temp" family residual of B5D0/B7FC/E8B4) 160 -> 99; `ctl` named local + b6 via
 * an inlined getter (value return in v0, as anim_no_rumble_text) in both loops -> EQUAL.
 * Shaping (disclosed): inlined helpers gfx_lock, gfx_unlock, font_set, Hidden, getB6, TB_CenterText (non-static *_BC84).
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
} Blit;

/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0x2A8 / 4];
    char *ok;
    char *notOk;
    char *pad2B0[(0x388 - 0x2B0) / 4];
    char *title;
} TblHdr;

typedef struct StrTable {
    u8 pad0[4];
    TblHdr *hdr;
} StrTable;

typedef struct Player {
    u8 b0;
    u8 control;
    u8 pad2[70];
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
} Player;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */

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

/* PC: gPlayerRects' layout (pause.c's Rect: x, y, w, h, cx, cy as s32). x reads the low half of the s32 centre cx on
 * the N64; here the whole word, which the s16 local takes the same. The PC loader fills initialized data by its
 * declarations, and the bytes of the N64 view would stay big-endian (docs/port/ENDIAN.md). */
typedef struct TPos {
    s32 pad0[4];
    s32 x;
    s32 y;
} TPos;

extern OSMesgQueue text_lock_queue;
extern s32 gPauseState;
extern s16 gNumPlayers;
extern Player gPlayers[];
extern Control gPaks[];
extern TPos pause_gPlayerRects[][4];
extern StrTable gText;
extern s32 playerTextColor[];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
void HAL_SetTextDepth(f32);
s16 TB_GetFontLineHeight(void);
u32 HAL_GetTextWidth(void *, s32);
void TB_SetTextColor(s32);
void TB_DrawText(s16, s16, void *);





s32 getB6_BC84(s32 ctl)
{
    return gPaks[ctl].b6;
}


s32 HAL_SetFont(s32 fontID);
s16 TB_CenterText(void *text, s16 centerX);
s32 Hidden(Blit *blt, s32 hide);
s32 animRumbleText(Blit *blt)
{
    u16 i;
    s32 ok;
    s16 x;
    s16 y;
    char *str;
    s32 ctl;

    ok = 1;
    if (Hidden(blt, gPauseState != 6)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    for (i = 0; i < gNumPlayers; i++) {
        ctl = gPlayers[i].control;
        if (!getB6_BC84(ctl)) {
            ok = 0;
        }
    }
    if (!ok) {
        HAL_SetFont(10);
        x = pause_gPlayerRects[0][1].x;
        y = pause_gPlayerRects[0][1].y - TB_GetFontLineHeight() / 2;
        TB_SetTextColor(22);
        str = gText.hdr->title;
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetFont(11);
    TB_SetTextColor(1);
    for (i = 0; i < gNumPlayers; i++) {
        x = pause_gPlayerRects[gNumPlayers - 1][i].x;
        y = pause_gPlayerRects[gNumPlayers - 1][i].y - TB_GetFontLineHeight();
        TB_SetTextColor(playerTextColor[i]);
        str = (char *)(*gPlayers[i].name + PROFILE_OFS(20));
        TB_DrawText(TB_CenterText(str, x), y, str);
        y += TB_GetFontLineHeight();
        ctl = gPlayers[i].control;
        if (getB6_BC84(ctl)) {
            str = gText.hdr->notOk;
        } else {
            str = gText.hdr->ok;
        }
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
