/* anim_show_on_high_score.c: a blit animator that shows its blit on the high score screen. */
#include "game/game.h"

/* The blit's animation function, each frame: once the game is in the high score screen (GS_HIGH_SCORE,
 * 0x02000000) a hidden blit is shown (and redrawn with UpdateBlit). Returns 1 (keep animating). */
s32 anim_show_on_high_score(void *blit)
{
    tBlit *blt = blit;

    if ((gGameState & GS_HIGH_SCORE) && (blt->hide != 0)) {
        blt->hide = 0;
        UpdateBlit(blt);
    }
    return 1;
}
