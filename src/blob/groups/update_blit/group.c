/* UpdateBlit: copy a blit into its zoid blit list entry (MAT3 blit.c). */
#include "game/game.h"

/* Copies blit's texture, colours, position, size, crop, visibility, alpha, depth, flip and whether it is a text blit
 * into its entry of the zoid blit list, which is what the renderer draws. Called after any change to a blit. */
void UpdateBlit(tBlit *blit) {
    ZOID_UpdateBlit(blit->blidx, blit->texIndex, blit->texture, blit->palette, blit->x, blit->y, blit->width, blit->height);
    ZOID_CropBlit(blit->blidx, blit->top, blit->bot, blit->left, blit->right);
    ZOID_SetBlitData(blit->blidx, blit->hide, blit->alpha, blit->z);
    ZOID_FlipBlit(blit->blidx, blit->flip);
    ZOID_TextBlit(blit->blidx, blit->name == BLIT_NO_TEXTURE);
}
