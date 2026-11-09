#include "game/game.h"

typedef struct Blit {
    const char *Name;
    void *image;
    void *Info;
    s16 TexIndex;
    s16 X, Y;
    u16 Z;
    s16 Width, Height;
    u8 Alpha, Flip;
    s8 Hide;
    u8 Init;
    s16 Top, Bot, Left, Right;
    s16 color;
    u16 unknown26;
    s32 (*AnimFunc)(struct Blit *);
    u32 AnimID;
} Blit;

/* MAT3 hud.c Hidden: the blit hidden or shown, updated when that changes. */
s32 Hidden(Blit *blt, s32 hide)
{
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    return blt->Hide;
}
s32 animPause(Blit *blt)
{
    if (Hidden(blt, pauseMenuSelect != 0))
        return 1;
    blt->image = &animPause_color;
    UpdateBlit(blt);
    blt->AnimFunc = 0;
    return 1;
}
