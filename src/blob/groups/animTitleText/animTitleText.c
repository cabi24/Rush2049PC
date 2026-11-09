/* The pause screen's title (MAT3 pausescr.c animTitleText): an animation function of a blit. */
#include "game/game.h"

/* The blit the animation function runs for (MAT3 _tBlit_2's leading fields). */
typedef struct MenuItem {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 dirty;
} MenuItem;

#define TEXT_PAUSE 56             /* gText.tab[]: the pause screen's strings: "paused", the menu, quit, restart */
#define TEXT_REMOVED 233          /* gText.single[]: "removed" (a controller) */

typedef struct PlayerRec {
    u8 pad0[0x48];
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
} PlayerRec;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */

extern PlayerRec gPlayers[];

/* Redraws item's blit once if it was marked dirty; returns the flag after (always 0). */
s32 item_refresh_B5D0(MenuItem *item)
{
    if (item->dirty) {
        item->dirty = 0;
        UpdateBlit(item);
    }
    return item->dirty;
}

/* The pause screen's title: "controller N removed" when a controller was pulled out; else the restart or quit
 * word (gPauseState 7, 8), nothing for the rumble screen (6), or the pausing player's name and "paused" in the
 * player's colour. Returns 1: the blit stays. */
s32 animTitleText(MenuItem *item) {
    s32 i;
    s32 x;
    s32 y;
    u8 *str;
    char buf[40];
    s32 v;

    if (item_refresh_B5D0(item)) {
        return 1;
    }
    HAL_SetTextDepth(0.0f);
    HAL_SetFont(13);
    x = pause_titlePos[0];
    y = pause_titlePos[1];
    str = (u8 *) buf;
    if (pause_gControllerRemoved) {
        TB_Sprintf(buf, pause_controller_removed_format, playerInPause + 1, gText.single[TEXT_REMOVED]);
        TB_SetTextColor(1);
    } else {
        v = gPauseState;
        if (v == 7 || v == 8) {
            str = gText.taba[gText.tab[TEXT_PAUSE] + v];
            TB_SetTextColor(1);
        } else if (v == 6) {
            buf[0] = 0;
        } else {
            TB_Sprintf(buf, pause_player_title_format, *gPlayers[playerInPause].name + PROFILE_OFS(20), gText.taba[gText.tab[TEXT_PAUSE]]);
            TB_SetTextColor(playerTextColor[playerInPause]);
        }
    }
    TB_DrawText(TB_CenterText(str, x), y, str);
    HAL_SetTextDepth(-1.0f);
    return 1;
}

