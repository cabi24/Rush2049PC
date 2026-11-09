/* RemovePane: remove a pane (MAT3 pane.c). */
#include "game/game.h"

/* Removes the pane's blits and frees it; nothing for NULL or a free pane. */
void RemovePane(tBlitSet *pane) {
    s32 i;

    if (pane != NULL && pane->used != 0) {
        for (i = 0; i < PANE_BLITS; i++) {
            if (pane->blits[i] != NULL) {
                RemoveBlit(pane->blits[i]);
                pane->blits[i] = NULL;
            }
        }
        pane->used = 0;
    }
}
