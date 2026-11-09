/* The panes behind the race's "no Rumble Pak" text (anim_no_rumble_text.c draws the text): an animation function
 * of a blit. */
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

typedef struct Pos {
    s32 x, y;
} Pos;

extern Player gPlayers[];
VIEW(Control[], gPaks);
#define gPaks gPaks_view
VIEW(Pos[][4], no_rumble_text_pos);
#define no_rumble_text_pos no_rumble_text_pos_view

/* Whether controller ctl has a Rumble Pak in. */
s32 getB6_6874(s32 ctl)
{
    return gPaks[ctl].rumble;
}

/* During the countdown and not paused, a pane (in the window's colours) around the "no Rumble Pak" text of each
 * window whose player has none; the panes go otherwise. From the race on the blit's animation is dropped and it
 * hides. */
s32 anim_no_rumble_panes(Blit *blt)
{
    s32 w;
    s32 h;
    s32 limit;
    s32 i;
    s32 x;
    s32 y;

    HAL_SetFont(11);
    w = HAL_GetTextWidth(gText.single[226], -1);
    limit = (gNumWindows >= 3) ? 160 : ((gNumWindows >= 2) ? 240 : 320);
    if (limit + 8 < w) {
        HAL_SetFont(12);
        w = HAL_GetTextWidth(gText.single[226], -1);
    }
    h = TB_GetFontLineHeight();
    for (i = 0; i < gNumWindows; i++) {
        if ((gGameState & GS_COUNTDOWN) && !gPauseState && !getB6_6874(gPlayers[i].control)) {
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
    if (gGameState & GS_RACING) {
        blt->state = 0;
        Hidden(blt, 1);
        return 0;
    }
    return 1;
}
