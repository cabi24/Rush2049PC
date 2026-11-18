/* flip_triangle_winding.c: reverse the winding of a display list's triangles, for mirrored tracks.
 *
 * MirrorWaveInstance mirrors a model's vertices for the mirror mode and walks its display lists with
 * display_list_traverse and this callback, so the mirrored triangles still face the camera. */
#include "game/game.h"

/* A triangle's second and third vertex swapped, which reverses its winding. */
static void swap_last_two_vertices(u8 *v) {
    u8 second;

    second = v[1];
    v[1] = v[2];
    v[2] = second;
}

/* display_list_traverse's callback for the record at gfx: in a triangle record, reverse each triangle's winding.
 * Returns the record's length, 1. */
s32 flip_triangle_winding(Gfx *gfx) {
    if (gfx->op == GFX_OP_TRIANGLE) {
        swap_last_two_vertices(&gfx->triangles.v[0]);
    } else if (gfx->op == GFX_OP_TRIANGLES) {
        swap_last_two_vertices(&gfx->triangles.v[0]);
        swap_last_two_vertices(&gfx->triangles.v[3]);
    }
    return 1;
}
