/* anim_show_on_high_score.c: a blit animator that shows its blit on the high score screen. */
#include "game/game.h"

/* PC: a blit's AnimFunc that shows it on the results screen (gGameState bit 25), over the Blit's host layout
 * (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): Hide is at +0x26, where the N64's +0x1A is the low byte of X. */
typedef struct HostBlit {
    void *Name, *Image, *Info;
    u16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide, Init;
    s16 Top, Bot, Left, Right, color;
    u16 reserved;
    intptr_t AnimFunc;
    u32 AnimID;
} HostBlit;

/* The blit's animation function, each frame: once the game is in the high score screen (GS_HIGH_SCORE,
 * 0x02000000) a hidden blit is shown (and redrawn with UpdateBlit). Returns 1 (keep animating). */
s32 anim_show_on_high_score(void *blit) {
    HostBlit *blt = blit;

    if ((gGameState & 0x02000000) && (blt->Hide != 0)) {
        blt->Hide = 0;
        UpdateBlit(blt);
    }
    return 1;
}
