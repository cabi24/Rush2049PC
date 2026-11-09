/* The race's "no Rumble Pak" text: an animation function of a blit, drawn during the countdown in each window
 * whose player has no Rumble Pak. */
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
    u8 init;
    s16 top, bot, left, right, color;
    u16 reserved;
    intptr_t state;     /* AnimFunc */
} Blit;

typedef struct Player {   /* gPlayers' record (tPadState) */
    u8 b0;
    u8 control;
    u8 pad2[0x48 - 0x2]; void *ref;
} Player;

/* A Controller Pak's record (gPaks[4]); the PC's layout. */
typedef struct Control {
    u8 pad0[6];
    s8 rumble;          /* a Rumble Pak is in */
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Control;


extern Player gPlayers[];

/* The text's horizontal and vertical justification (gJust), as TB_SetJust sets it. */
void TB_SetJust_6B3C(u32 first, u32 second)
{
    TextJust *p = &gJust;
    p->h = first;
    p->v = second;
}



/* Whether controller ctl has a Rumble Pak in. */
s32 getB6_6B3C(s32 ctl)
{
    return gPaks[ctl].rumble;
}

/* Until the race starts (hidden from GS_RACING on, when the blit's animation is dropped): during the
 * countdown and not paused, message 226 ("no Rumble Pak") centred at each window's place for a player without
 * one, in the player's colour; the smaller font when the message is wider than a window. */
s32 anim_no_rumble_text(Blit *blt)
{
    u32 width;
    s32 limit;
    s32 i;
    s32 x;
    s32 y;
    s32 ctl;

    if (Hidden(blt, gGameState & GS_RACING)) {
        blt->state = 0;
        return 0;
    }
    TB_SetJust_6B3C(1, 1);
    HAL_SetFont(11);
    width = HAL_GetTextWidth(gText.single[226], -1);
    limit = (gNumWindows >= 3) ? 160 : ((gNumWindows >= 2) ? 240 : 320);
    if (limit + 8 < width) {
        text_lock();
        HAL_SetFontInternal(12);
        text_unlock();
    }
    for (i = 0; i < gNumWindows; i++) {
        if ((gGameState & GS_COUNTDOWN) && !gPauseState) {
            ctl = gPlayers[i].control;
            if (!getB6_6B3C(ctl)) {
                x = no_rumble_text_pos[gNumWindows - 1][i].x;
                y = no_rumble_text_pos[gNumWindows - 1][i].y;
                TB_SetTextColor(playerTextColor[i]);
                TB_DrawText(x, y, gText.single[226]);
            }
        }
    }
    TB_SetJust_6B3C(0, 3);
    return 1;
}




