/* displaylist_set_texture_image.c: change the image a texture's load display list sets.
 *
 * TextureAnimation's flipbooks swap a texture's image this way (displaylist_get_texture_image reads it). */
#include "game/game.h"

/* Makes `image` the image of the display list dl's first texture image record, if it has one before its end. */
void displaylist_set_texture_image(Gfx *dl, void *image) {
    for (;; dl++) {
        if (dl->op == GFX_OP_TEXTURE_IMAGE) {
            dl->image.image = image;
            return;
        } else if (dl->op == GFX_OP_END_LIST) {
            return;
        }
    }
}
