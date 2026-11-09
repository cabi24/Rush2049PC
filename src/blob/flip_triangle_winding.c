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

/* display_list_traverse's callback for the command at gfx: in a triangle command, swap the second and third vertex
 * index of each triangle (bits 8-15 and 0-7 of its word), which reverses its winding. Returns the command's length
 * in words, 2. */
s32 flip_triangle_winding(u32 *gfx) {
    u32 mid;
    u32 low;

    if ((DLW(*gfx) & 0xFF000000) == GBI_TRI1) {
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
    } else if ((DLW(*gfx) & 0xFF000000) == GBI_QUAD || (DLW(*gfx) & 0xFF000000) == GBI_TRI2) {
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
        gfx++;
        mid = DLW(*gfx) & 0xFF00;
        low = DLW(*gfx) & 0xFF;
        DLW_SET(*gfx, (DLW(*gfx) & 0xFFFF0000) | (mid >> 8) | (low << 8));
    }
    return 2;
}
