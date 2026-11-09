/* SelectBlit: show one frame of a texture laid out as a grid of frames (MAT3 blit.c). */
#include "game/game.h"

/* Crops blt to frame number index of its texture, a grid of frames hSize by vSize texels, numbered across then down
 * (the HUD's digits, gears, place numbers). A flipped blit counts its columns from the right, starting where the
 * texture's width leaves off. Finds the texture first if the blit has none yet (RenameBlit with preserve). */
void SelectBlit(tBlit *blt, int index, int hSize, int vSize)
{
    int row, col, secPerRow;

    if (!blt)
        return;
    if (!blt->texture || !blt->texture->width) {
        RenameBlit(blt, blt->name, 1);
        if (!blt->texture)
            return;
    }
    secPerRow = blt->texture->width / hSize;
    if (!secPerRow)
        secPerRow = 1;
    row = index / secPerRow;
    col = index % secPerRow;
    blt->top = row * vSize;
    blt->bot = blt->top + vSize - 1;
    if (blt->flip) {
        col = secPerRow - col - 1;
        blt->left = (blt->texture->width % hSize) + col * hSize;
        blt->right = blt->left + hSize - 1;
    } else {
        blt->left = col * hSize;
        blt->right = blt->left + hSize - 1;
    }
    UpdateBlit(blt);
}
