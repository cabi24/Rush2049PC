#include "game/game.h"
#define Control ControlView   /* this file's own Control: include/game's Control is another type */

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

typedef struct Control {
    u8 pad0[6];
    s8 b6;
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Control;

extern Player gPlayers[];
VIEW(Control[], gPaks);
#define gPaks gPaks_view

VIEW(StrTable, gText);
#define gText gText_view

s32 getB6_BC84(s32 ctl)
{
    return gPaks[ctl].b6;
}

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
