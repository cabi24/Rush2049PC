#include "game/game.h"

VIEW(s32, gGameState);
#define gGameState gGameState_view

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

s32 anim_show_on_high_score(void *arg0) {
    HostBlit *blt = arg0;

    if ((gGameState & 0x02000000) && (blt->Hide != 0)) {
        blt->Hide = 0;
        UpdateBlit(blt);
    }
    return 1;
}
