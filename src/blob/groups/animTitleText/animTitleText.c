#include "game/game.h"

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

VIEW(s16[2], pause_titlePos);
#define pause_titlePos pause_titlePos_view

VIEW(StrTable, gText);
#define gText gText_view

typedef struct PlayerRec {
    u8 pad0[0x48];
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
} PlayerRec;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */

extern PlayerRec gPlayers[];

s32 item_refresh_B5D0(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        UpdateBlit(item);
    }
    return item->dirty;
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
    HAL_SetFont(13);
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
    TB_DrawText(TB_CenterText((void *)str, x), y, (void *)str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

