/* The pause screen's rumble page (MAT3 pausescr.c's rumble screen text): an animation function of a blit. */
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

#define TEXT_RUMBLE_OFF 170       /* gText.single[]: the rumble page's "off" */
#define TEXT_RUMBLE_ON 171        /* "on" */
#define TEXT_RUMBLE_TITLE 226     /* its title */

typedef struct Player {   /* gPlayers' record (tPadState) */
    u8 b0;
    u8 control;
    u8 pad2[70];
    intptr_t *name;     /* the profile handle: its first word is the profile record, a pointer (POINTERS.md 10) */
} Player;
/* The profile record (save.c's SaveSlot) is 64 bytes on the PC, its name at +0x20 (PROFILE_OFS, pc_prelude.h;
 * docs/port/POINTERS.md 9.10). */

/* A Controller Pak's record (gPaks[4]); the PC's layout. */
typedef struct Control {
    u8 pad0[6];
    s8 rumble;          /* a Rumble Pak is in */
    u8 pad7[5];
    PcPfs pfs;
    u8 pad74[0x304 - 0x74];
} Control;

extern Player gPlayers[];

/* Whether controller ctl has a Rumble Pak in. */
s32 getB6_BC84(s32 ctl)
{
    return gPaks[ctl].rumble;
}

/* On the pause screen's rumble page (gPauseState 6, MAT3 kPauseRumbleScreen): a title (message 226) when some
 * player has no Rumble Pak, then each player's name in their colour at their place on the page with their rumble
 * state under it. Hidden on the other pages. Returns 1: the blit stays. */
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
        x = pause_gPlayerRects[0][1].cx;
        y = pause_gPlayerRects[0][1].cy - TB_GetFontLineHeight() / 2;
        TB_SetTextColor(22);
        str = gText.single[TEXT_RUMBLE_TITLE];
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetFont(11);
    TB_SetTextColor(1);
    for (i = 0; i < gNumPlayers; i++) {
        x = pause_gPlayerRects[gNumPlayers - 1][i].cx;
        y = pause_gPlayerRects[gNumPlayers - 1][i].cy - TB_GetFontLineHeight();
        TB_SetTextColor(playerTextColor[i]);
        str = (char *)(*gPlayers[i].name + PROFILE_OFS(20));
        TB_DrawText(TB_CenterText(str, x), y, str);
        y += TB_GetFontLineHeight();
        ctl = gPlayers[i].control;
        if (getB6_BC84(ctl)) {
            str = gText.single[TEXT_RUMBLE_ON];
        } else {
            str = gText.single[TEXT_RUMBLE_OFF];
        }
        TB_SetTextColor(1);
        TB_DrawText(TB_CenterText(str, x), y, str);
    }
    HAL_SetTextDepth(-1.0f);
    return 1;
}
