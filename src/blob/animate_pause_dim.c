#include "game/game.h"

VIEW(BlitColor, pause_dim_color);
#define pause_dim_color pause_dim_color_view

/* PC: the settings dialog's blit (PS_ErrorFunc, multiblit PS_ErrorFunc_dimMultiBlit) takes its colour as its Image and clears its
 * AnimFunc, over the Blit's host layout (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): the Image at +8, the
 * AnimFunc slot pointer-sized at +0x38. */
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

s32 animate_pause_dim(void *arg0) {
    HostBlit *blt = arg0;

    blt->Image = &pause_dim_color;
    UpdateBlit(blt);
    blt->AnimFunc = 0;
    return 1;
}
