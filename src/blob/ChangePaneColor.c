/* ChangePaneColor: recolour a pane (MAT3 pane.c). */
#include "game/game.h"

/* Gives the pane the RGBA colours paneColor (the inside) and borderColor (the four edges), copied into the pane, the
 * alpha of each blit its colour's. */
void ChangePaneColor(tBlitSet *pane, u8 *paneColor, u8 *borderColor) {
    tBlit *blit;
    s32 i;

    for (i = 0; i < 4; i++) {
        pane->blackColor[i] = paneColor[i];
        pane->grayColor[i] = borderColor[i];
    }
    for (i = 0; i < PANE_BLITS; i++) {
        blit = pane->blits[i];
        if (i == 2) {
            blit->palette = pane->blackColor;
            blit->alpha = paneColor[3];
        } else {
            blit->palette = pane->grayColor;
            blit->alpha = borderColor[3];
        }
        UpdateBlit(blit);
    }
}
