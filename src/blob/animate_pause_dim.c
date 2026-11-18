/* animate_pause_dim: the animation function of the pause screens' dimming backdrop (a pausescr.c animator; MAT3's
 * animControllerDimInternal without its Hidden test). The settings dialog uses it too, for the blit behind
 * PS_ErrorFunc's box (PS_ErrorFunc_dimMultiBlit). */
#include "game/game.h"

/* Make the blit a flat rectangle of pause_dim_color (a blit whose palette is a colour), update it once and take its
 * animation function away, so this runs only on the blit's first frame. 1 keeps the blit. */
s32 animate_pause_dim(void *blit)
{
    tBlit *blt = blit;

    blt->palette = &pause_dim_color;
    UpdateBlit(blt);
    blt->animFunc = 0;
    return 1;
}
