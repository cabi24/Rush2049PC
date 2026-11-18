/* animArrows: the track select screen's arrows (MAT3 trackscr.c animArrows): left and right beside the race option
 * being changed, up and down when the options list scrolls. */
#include "game/game.h"

typedef struct Point { s16 x, y; } Point;

VIEW(Point[][4], trackscr_arrowPos);
#define trackscr_arrowPos trackscr_arrowPos_view

#define TEXT_TRACK_OPTIONS 1        /* gText.tab[]: the race options' names (trackscr_gOpts indexes them) */
#define ARROW_LEFT 0                /* the item (animID bits 4-7) */
#define ARROW_RIGHT 1
#define ARROW_UP 2
#define ARROW_DOWN 3
#define SPRITE_FLIP_Y 8             /* gBlitInstanceList[].flags: drawn upside down */

/* One arrow, each frame. The blit's animID packs whether it pulses (bits 0-3), its item (4-7: left, right, up,
 * down), its group (8-11: 0 the options list, 1 a second place with no up arrow) and its texture (12-15). Hidden while
 * the 3D buttons are up; up and down only when the list can scroll that way (5 options show). Its texture is the
 * active or the dim one (trackscr_arrowNames), it pulses with the flashing text, and it sits at its place
 * (trackscr_arrowPos): in the list the left arrow before the option's name and both on the active option's row.
 * The right arrow is the left one flipped, the down arrow the up one upside down. Always 1. */
s32 animArrows(tBlit *blt)
{
    s16 pulse = blt->animID & 15;
    s16 item = (blt->animID >> 4) & 15;
    s16 group = (blt->animID >> 8) & 15;
    s16 texture = (blt->animID >> 12) & 15;
    s16 active;
    if (Hidden(blt, trackscr_buttonsInited == 1)) return 1;
    if (group == 1) {
        if (Hidden(blt, item == 2)) return 1;
    }
    if (group == 0) {
        if (item == 2) {
            if (Hidden(blt, trackscr_gFirstOption == 0)) return 1;
        } else if (item == 3) {
            if (Hidden(blt, trackscr_gNumOptions - trackscr_gFirstOption < 5)) return 1;
        }
    }
    active = trackscr_gActiveOptionEnabled || item >= 2;
    RenameBlit(blt, trackscr_arrowNames[texture][active], 1);
    if (pulse) blt->alpha = (u32)(TB_GetFlashTextCycle() * 255.0f);
    blt->x = trackscr_arrowPos[group][item].x - blt->width / 2;
    blt->y = trackscr_arrowPos[group][item].y - blt->height / 2;
    if (group == 0) {
        HAL_SetFont(11);
        if (item == 0) {
            blt->x -= HAL_GetTextWidth(gText.taba[trackscr_gOpts[trackscr_gActiveOption] + gText.tab[TEXT_TRACK_OPTIONS]], -1);
        }
        if (item == 0 || item == 1) blt->y += (trackscr_gActiveOption - trackscr_gFirstOption) * 13;
    }
    if (item == ARROW_RIGHT) blt->flip = 1;
    else if (item == ARROW_DOWN) gBlitInstanceList[blt->blidx].flags |= SPRITE_FLIP_Y;
    UpdateBlit(blt);
    return 1;
}
