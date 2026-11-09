/* animate_pause_dim: the animation function of the pause screens' dimming backdrop (a pausescr.c animator; MAT3's
 * animControllerDimInternal without its Hidden test). The settings dialog uses it too, for the blit behind
 * PS_ErrorFunc's box (PS_ErrorFunc_dimMultiBlit). */
#include "game/game.h"

/* PC: the blit's host layout (NewMultiBlit.c's, docs/port/POINTERS.md 9.3): the Image is the pointer at +8, the
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

/* Make the blit a flat rectangle of pause_dim_color (a blit whose Image is a colour), update it once and take its
 * animation function away, so this runs only on the blit's first frame. 1 keeps the blit. */
s32 animate_pause_dim(void *blit) {
    HostBlit *blt = blit;

    blt->Image = &pause_dim_color;
    UpdateBlit(blt);
    blt->AnimFunc = 0;
    return 1;
}
