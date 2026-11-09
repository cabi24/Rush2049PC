#include "game/game.h"

VIEW(BlitColor, controller_dim_color);
#define controller_dim_color controller_dim_color_view

/* PC: the idle overlay's blit (GameLoop's, after 300 s without a button, multiblit idle_warning_multiblit) takes its colour as
 * its Image, over the Blit's host layout (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): the Image is the pointer at
 * +8. The m2c body's 8-byte store at the N64 +4 split the colour's address over the Name and the Image, and the
 * draw read the colour from NULL. */
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

s32 animControllerDim(void *arg0) {
    HostBlit *blt = arg0;

    blt->Image = &controller_dim_color;
    UpdateBlit(blt);
    return 1;
}
