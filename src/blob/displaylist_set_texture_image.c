/* displaylist_set_texture_image.c: change the image a texture's load display list sets.
 *
 * TextureAnimation's flipbooks swap a texture's image this way (displaylist_get_texture_image reads it). */
#include "game/game.h"

/* Makes `image` the image of the first G_SETTIMG of the display list dl, if it has one before its G_ENDDL. */
void displaylist_set_texture_image(Gfx *dl, void *image) {
    u8 op;

    for (;; dl++) {
        op = dl->words.w0 >> 24;
        if (op == G_SETTIMG) {
            dl->words.w1 = (uintptr_t)image;
            return;
        } else if (op == G_ENDDL) {
            return;
        }
    }
}
