/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * anim_no_rumble_panes (0x80106874, 712 B): per-window background pane behind the HUD title text that the neighbour
 * anim_no_rumble_text draws (StrTable +0x388, font 11, or font 12 when wider than the window limit + 8). N64-only
 * hud.c Blit AnimFunc (GC hud.c has nothing between animResults and animOdometer); written in the
 * GC CheckResults pane idiom (MAT3 hud.c CheckResults: InitPane(..., 192, 0, 0, 1) + ChangePaneColor(pane,
 * playerPaneColors[p][0], playerPaneColors[p][1]) + RemovePane). InitPane, ChangePaneColor =
 * tournament_unlock_check, RemovePane = sound_handles_array_clear, HAL_GetStringWidth = HAL_GetTextWidth,
 * HAL_GetFontHeight = TB_GetFontLineHeight, playerPaneColors = no_rumble_pane_colors (u8 [4][2][4]), the pane handles =
 * resultPane[4], the window positions = no_rumble_text_pos (shared with anim_no_rumble_text), gGameState = gGameState (bit 21
 * shows the panes, bit 22 retires the blit), gPauseState, gPlayers[i].control /
 * gPaks[ctl].b6 as in anim_no_rumble_text. HAL_SetFont = HAL_SetFontInternal (s2 IPA parameter).
 * Lever (from the anim_no_rumble_text match): the b6 test through an inlined getter (its value return takes v0).
 * Helpers gfx_lock/gfx_unlock/font_set/getB6/Hidden_6874 are inlined, non-static, not on keep (stub rule).
 * No other shaping.
 */typedef signed char s8;
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

extern OSMesgQueue text_lock_queue;
extern u32 gGameState;
extern TblHdr *gText_single;
extern s16 gNumWindows;
extern s32 gPauseState;
extern Player gPlayers[];
extern Control gPaks[];
extern Pos no_rumble_text_pos[][4];
extern u8 no_rumble_pane_colors[4][2][4];
extern void *resultPane[4];

s32 osRecvMesg(OSMesgQueue *, void **, s32);
s32 osSendMesg(OSMesgQueue *, void *, s32);
s32 HAL_SetFontInternal(s32 selection);
void UpdateBlit(void *);
s32 HAL_GetTextWidth(void *, s32);
s16 TB_GetFontLineHeight(void);
/* PC: pointer-sized, as the definition (Windows x64 passes it on the stack; docs/port/PROTOTYPES.md) */
void *InitPane(s32, s32, s32, s32, s32, intptr_t, s32, s32);
void ChangePaneColor(void *, u8 *, u8 *);
void RemovePane(void *);

void gfx_lock_6874(void)
{
    osRecvMesg(&text_lock_queue, (void *)0, 1);
}

void gfx_unlock_6874(void)
{
    osSendMesg(&text_lock_queue, (void *)0, 0);
}

s32 font_set_6874(s32 font)
{
    s32 old;

    gfx_lock_6874();
    old = HAL_SetFontInternal(font);
    gfx_unlock_6874();
    return old;
}

s32 getB6_6874(s32 ctl)
{
    return gPaks[ctl].b6;
}

s32 Hidden_6874(Blit *blt, s32 hide)
{
    if (blt->hidden != hide) {
        blt->hidden = hide;
        UpdateBlit(blt);
    }
    return blt->hidden;
}

s32 anim_no_rumble_panes(Blit *blt)
{
    s32 w;
    s32 h;
    s32 limit;
    s32 i;
    s32 x;
    s32 y;

    font_set_6874(11);
    w = HAL_GetTextWidth(gText_single->text, -1);
    limit = (gNumWindows >= 3) ? 160 : ((gNumWindows >= 2) ? 240 : 320);
    if (limit + 8 < w) {
        font_set_6874(12);
        w = HAL_GetTextWidth(gText_single->text, -1);
    }
    h = TB_GetFontLineHeight();
    for (i = 0; i < gNumWindows; i++) {
        if ((gGameState & 0x200000) && !gPauseState && !getB6_6874(gPlayers[i].control)) {
            if (!resultPane[i]) {
                x = no_rumble_text_pos[gNumWindows - 1][i].x;
                y = no_rumble_text_pos[gNumWindows - 1][i].y;
                resultPane[i] = InitPane(x - w / 2 - 8, y - h / 2 - 4, x + w / 2 + 8, y + h / 2 + 4,
                                                  255, 0, i, 1);
                ChangePaneColor(resultPane[i], no_rumble_pane_colors[i][0], no_rumble_pane_colors[i][1]);
            }
        } else if (resultPane[i]) {
            RemovePane(resultPane[i]);
            resultPane[i] = 0;
        }
    }
    if (gGameState & 0x400000) {
        blt->state = 0;
        Hidden_6874(blt, 1);
        return 0;
    }
    return 1;
}
