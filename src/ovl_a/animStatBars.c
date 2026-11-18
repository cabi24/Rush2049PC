/* animStatBars: the car select screen's stat bars (MAT3 carscr.c animStatBars): four bars (the car's stats, gStats)
 * beside the car in a one-player race. */
#include "game/game.h"

/* One bar segment, each frame. The blit's animID packs its segment (bits 0-3: 0 the empty bar, 1 the filled part),
 * its bar (4-7), its player (8-11) and an "initialised" bit (31). The first frame places it (shown only in a
 * one-player race other than a ghost race; else hidden for good) as a strip of the bars texture; then the filled
 * part's width follows the stat (at least 3 pixels). Hidden while the car is locked. Always 1. */
s32 animStatBars(tBlit *blt)
{
    s32 segment = blt->animID & 15;
    s32 bar = (blt->animID >> 4) & 15;
    s32 player = (blt->animID >> 8) & 15;
    s32 height;
    s32 initialized = blt->animID >> 31;
    if (!initialized) {
        if (gNumPlayers >= 3 && player < 2)
            RenameBlit(blt, carscr_stat_bars_texture_name, 0);
        blt->animID |= 0x80000000;
        if (player < gNumPlayers && gNumPlayers == 1 && gRace != kGhost) {
            blt->x = 10;
            height = blt->height / 8;
            blt->y = 48 + bar * 2 * height;
        } else {
            Hidden(blt, 1);
            blt->animFunc = 0;
            return 1;
        }
        blt->left = 0;
        blt->top = (bar * 2 + (segment == 0)) * height;
        blt->bot = blt->top + height - 1;
    }
    if (Hidden(blt, gCarLocked[player] == 1))
        return 1;
    if (segment == 1) {
        blt->right = gStats[player][bar] * blt->width - 1;
        blt->right = blt->right < 2 ? 2 : blt->right;
    } else
        blt->right = blt->width - 1;
    UpdateBlit(blt);
    return 1;
}
