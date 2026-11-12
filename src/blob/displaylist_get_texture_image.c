/* displaylist_get_texture_image.c: the image a texture's load display list sets.
 *
 * A WAD texture (TXHD) without its texels at hand is drawn by its load display list (TXLD): its G_SETTIMG command
 * holds the image. InitTextureAnimation reads it for the texture flipbooks' frames; displaylist_set_texture_image
 * writes it. */
#include "game/game.h"

/* The image (the pointer in w1) of the first G_SETTIMG of the display list dl, read up to its G_ENDDL.
 * Original game: a list with no G_SETTIMG before its end returns no value (on the N64, whatever the return register
 * held); here NULL. */
void *displaylist_get_texture_image(Gfx *dl) {
    u8 op;

    for (;; dl++) {
        op = dl->words.w0 >> 24;
        if (op == G_SETTIMG) {
            return (void *)dl->words.w1;
        } else if (op == G_ENDDL) {
            return NULL;
        }
    }
}
