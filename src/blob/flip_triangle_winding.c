/* flip_triangle_winding.c: reverse the winding of a display list's triangles, for mirrored tracks.
 *
 * MirrorWaveInstance mirrors a model's vertices for the mirror mode and walks its display lists with
 * display_list_traverse and this callback, so the mirrored triangles still face the camera. */
#include "game/game.h"

/* The F3DEX2 triangle commands, in a command's top byte: one triangle (three vertex indices in the first word), two
 * triangles or a quad (three indices in each word). */
#define GBI_TRI1 0x05000000
#define GBI_TRI2 0x06000000
#define GBI_QUAD 0x07000000

/* A triangle's word with its second and third vertex index (bits 8-15 and 0-7) swapped, which reverses its winding. */
static u32 swap_last_two_vertices(u32 word) {
    u32 mid;
    u32 low;

    mid = word & 0xFF00;
    low = word & 0xFF;
    return (word & 0xFFFF0000) | (mid >> 8) | (low << 8);
}

/* display_list_traverse's callback for the command at gfx: in a triangle command, reverse each triangle's winding.
 * Returns the command's length, 1. */
s32 flip_triangle_winding(Gfx *gfx) {
    u32 op;

    op = gfx->words.w0 & 0xFF000000;
    if (op == GBI_TRI1) {
        gfx->words.w0 = swap_last_two_vertices(gfx->words.w0);
    } else if (op == GBI_QUAD || op == GBI_TRI2) {
        gfx->words.w0 = swap_last_two_vertices(gfx->words.w0);
        gfx->words.w1 = swap_last_two_vertices((u32)gfx->words.w1);
    }
    return 1;
}
