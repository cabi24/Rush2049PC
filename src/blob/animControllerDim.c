/* animControllerDim: the animation function of the screen-dimming blit GameLoop puts up after 300 seconds without
 * a button (idle_warning_multiblit) (MAT3 animControllerDim, a game.c animator). */
#include "game/game.h"

/* Each frame: make the blit a flat rectangle of controller_dim_color (a blit whose palette is a colour) and update
 * it. 1 keeps the blit. */
s32 animControllerDim(void *blit)
{
    tBlit *blt = blit;

    blt->palette = &controller_dim_color;
    UpdateBlit(blt);
    return 1;
}
