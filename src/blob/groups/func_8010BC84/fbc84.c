/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * func_8010BC84 (0x8010BC84, 928 B): pause-screen controller status text (D_801170FC == 6). If any player's
 * controller record (D_80144030[ctl].b6, stride 772) is 0, a centred title (StrTable +0x388, font 10, colour 22) at
 * D_80117100[0][1] - lineheight/2; then per player (count D_8014A108) the player name (record +72 -> +20) and
 * "OK"/"NOT OK" (StrTable +0x2A8/+0x2AC), each centred on D_80117100[n-1][i].x, font 11.
 * MATCH in group / unit EQUAL (draft 160/232, EQUIVALENT 90 calls on pause_menus).
 * Levers: TB_CenterText(text, centerX) inlined (GC TB_text.c: s16 TB_CenterText(void *text, s16 centerX) =
 * centerX - width/2 narrowed at return; narrowed again for state_utility = TB_DrawText's s16 x: this is the
 * "x - (w >> 1) narrowing through a temp" family residual of B5D0/B7FC/E8B4) 160 -> 99; `ctl` named local + b6 via
 * an inlined getter (value return in v0, as func_80106B3C) in both loops -> EQUAL.
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

#ifdef TARGET_PC
/* PC: the Blit's host layout (sound_control.c), as every view has it (docs/port/POINTERS.md 9). */
typedef struct Blit {
    void *name, *image, *info;
    u16 texIndex;
    s16 x, y;
    u16 z;
    s16 width, height;
    u8 alpha, flip;
    s8 hidden;
} Blit;
#else
typedef struct Blit {
    u8 pad0[0x1A];
    s8 hidden;
} Blit;
#endif

#ifdef TARGET_PC
/* PC: gText's string tables hold host pointers (port/assets/text.c), so N64 byte offsets / 4 are indices */
typedef struct TblHdr {
    char *pad0[0x2A8 / 4];
    char *ok;
    char *notOk;
    char *pad2B0[(0x388 - 0x2B0) / 4];
    char *title;
} TblHdr;
#else
typedef struct TblHdr {
    u8 pad0[0x2A8];
    char *ok;
    char *notOk;
    u8 pad2B0[0x388 - 0x2B0];
    char *title;
} TblHdr;
#endif

typedef struct StrTable {
    u8 pad0[4];
    TblHdr *hdr;
} StrTable;

typedef struct Player {
    u8 b0;
    u8 control;
    u8 pad2[70];
#ifdef TARGET_PC
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
#else
    s32 *name;
#endif
} Player;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */
#ifndef TARGET_PC
#define PROFILE_OFS(n) n
#endif

#ifdef TARGET_PC
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
#else
typedef struct Control {
    u8 pad0[6];
    s8 b6;
    u8 pad7[772 - 7];
} Control;
#endif

#ifdef TARGET_PC
/* PC: gPlayerRects' layout (pause.c's Rect: x, y, w, h, cx, cy as s32). x reads the low half of the s32 centre cx on
 * the N64; here the whole word, which the s16 local takes the same. The PC loader fills initialized data by its
 * declarations, and the bytes of the N64 view would stay big-endian (docs/port/ENDIAN.md). */
typedef struct TPos {
    s32 pad0[4];
    s32 x;
    s32 y;
} TPos;
#else
typedef struct TPos {
    u8 pad0[18];
    s16 x;
    s32 y;
} TPos;
#endif

extern OSMesgQueue D_801461D0;
extern s32 D_801170FC;
extern s16 D_8014A108;
extern Player D_8014A118[];
extern Control D_80144030[];
extern TPos D_80117100[][4];
extern StrTable D_8017A4E0;
extern s32 D_801146AC[];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 slot_state_setup(s32 selection);
void Input_ApplyPadConfig(void *);
void render_helper(f32);
s16 object_bytes_sum_global(void);
u32 object_manager_update(void *, s32);
void dispatch_handler(s32);
void state_utility(s16, s16, void *);

void gfx_lock_BC84(void)
{
    osRecvMesg(&D_801461D0, (void *)0, 1);
}

void gfx_unlock_BC84(void)
{
    osSendMesg(&D_801461D0, (void *)0, 0);
}

s32 font_set_BC84(s32 font)
{
    s32 old;

    gfx_lock_BC84();
    old = slot_state_setup(font);
    gfx_unlock_BC84();
    return old;
}

s32 Hidden_BC84(Blit *blt, s32 hide)
{
    if (blt->hidden != hide) {
        blt->hidden = hide;
        Input_ApplyPadConfig(blt);
    }
    return blt->hidden;
}

s32 getB6_BC84(s32 ctl)
{
    return D_80144030[ctl].b6;
}

s16 TB_CenterText_BC84(void *text, s16 centerX)
{
    return centerX - (object_manager_update(text, -1) >> 1);
}

s32 func_8010BC84(Blit *blt)
{
    u16 i;
    s32 ok;
    s16 x;
    s16 y;
    char *str;
    s32 ctl;

    ok = 1;
    if (Hidden_BC84(blt, D_801170FC != 6)) {
        return 1;
    }
    render_helper(0.0f);
    for (i = 0; i < D_8014A108; i++) {
        ctl = D_8014A118[i].control;
        if (!getB6_BC84(ctl)) {
            ok = 0;
        }
    }
    if (!ok) {
        font_set_BC84(10);
        x = D_80117100[0][1].x;
        y = D_80117100[0][1].y - object_bytes_sum_global() / 2;
        dispatch_handler(22);
        str = D_8017A4E0.hdr->title;
        state_utility(TB_CenterText_BC84(str, x), y, str);
    }
    font_set_BC84(11);
    dispatch_handler(1);
    for (i = 0; i < D_8014A108; i++) {
        x = D_80117100[D_8014A108 - 1][i].x;
        y = D_80117100[D_8014A108 - 1][i].y - object_bytes_sum_global();
        dispatch_handler(D_801146AC[i]);
        str = (char *)(*D_8014A118[i].name + PROFILE_OFS(20));
        state_utility(TB_CenterText_BC84(str, x), y, str);
        y += object_bytes_sum_global();
        ctl = D_8014A118[i].control;
        if (getB6_BC84(ctl)) {
            str = D_8017A4E0.hdr->notOk;
        } else {
            str = D_8017A4E0.hdr->ok;
        }
        dispatch_handler(1);
        state_utility(TB_CenterText_BC84(str, x), y, str);
    }
    render_helper(-1.0f);
    return 1;
}
