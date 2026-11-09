/* The race's "no Rumble Pak" text: an animation function of a blit, drawn during the countdown in each window
 * whose player has no Rumble Pak. */
#include "game/game.h"

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

typedef struct Pos {
    s32 x, y;
} Pos;

extern Player gPlayers[];
VIEW(Pos[][4], no_rumble_text_pos);
#define no_rumble_text_pos no_rumble_text_pos_view

/* Until the race starts (hidden from GAME_STATE_PLAY on, when the blit's animation is dropped): during the
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

    if (Hidden(blt, gGameState & GAME_STATE_PLAY)) {
        blt->state = 0;
        return 0;
    }
    TB_SetJust(1, 1);
    HAL_SetFont(11);
    width = HAL_GetTextWidth(gText.single[226], -1);
    limit = (gNumWindows >= 3) ? 160 : ((gNumWindows >= 2) ? 240 : 320);
    if (limit + 8 < width) {
        text_lock();
        HAL_SetFontInternal(12);
        text_unlock();
    }
    for (i = 0; i < gNumWindows; i++) {
        if ((gGameState & GAME_STATE_COUNTDOWN) && !gPauseState) {
            ctl = gPlayers[i].control;
            if (!PS_PakRumble(ctl)) {
                x = no_rumble_text_pos[gNumWindows - 1][i].x;
                y = no_rumble_text_pos[gNumWindows - 1][i].y;
                TB_SetTextColor(playerTextColor[i]);
                TB_DrawText(x, y, gText.single[226]);
            }
        }
    }
    TB_SetJust(0, 3);
    return 1;
}




