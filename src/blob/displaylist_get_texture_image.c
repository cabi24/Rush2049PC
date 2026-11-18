/* displaylist_get_texture_image.c: the image a texture's load display list sets.
 *
 * A WAD texture (TXHD) without its texels at hand is drawn by its load display list (TXLD): its texture image record
 * holds the image. InitTextureAnimation reads it for the texture flipbooks' frames; displaylist_set_texture_image
 * writes it. */
#include "game/game.h"

/* The image of the display list dl's first texture image record (GFX_OP_TEXTURE_IMAGE), read up to its end.
 * Original game: a list with none before its end returns no value (on the N64, whatever the return register held);
 * here NULL. */
void *displaylist_get_texture_image(Gfx *dl) {
    for (;; dl++) {
        if (dl->op == GFX_OP_TEXTURE_IMAGE) {
            return dl->image.image;
        } else if (dl->op == GFX_OP_END_LIST) {
            return NULL;
        }
    }
}
