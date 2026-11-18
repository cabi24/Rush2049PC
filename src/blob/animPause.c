/* Hidden and animPause: show or hide a blit, and the pause screen's dimming (MAT3 hud.c and pausescr.c). */
#include "game/game.h"

/* Hides (hide 1) or shows blt, updating it only when that changes; returns whether it is hidden. The flag, 0 or 1,
 * is read as a signed byte, as the original reads it here. */
s32 Hidden(tBlit *blt, s32 hide)
{
    if (hide != (s8)blt->hide) {
        blt->hide = hide;
        UpdateBlit(blt);
    }
    return (s8)blt->hide;
}
/* The pause screen's dimming: hidden unless the pause menu's first item is chosen; else it takes the dim colour
 * (animPause_color) once and stops animating. Returns 1. */
s32 animPause(tBlit *blt)
{
    if (Hidden(blt, pauseMenuSelect != 0))
        return 1;
    blt->palette = &animPause_color;
    UpdateBlit(blt);
    blt->animFunc = 0;
    return 1;
}
