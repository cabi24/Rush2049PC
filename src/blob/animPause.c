/* Hidden and animPause: show or hide a blit, and the pause screen's dimming (MAT3 hud.c and pausescr.c). */
#include "game/game.h"

/* The blit as these read it (tBlit's layout; frontend.h's prototype of animPause takes a struct Blit). */
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

/* Hides (hide 1) or shows blt, updating it only when that changes; returns whether it is hidden. */
s32 Hidden(Blit *blt, s32 hide)
{
    if (hide != blt->Hide) {
        blt->Hide = hide;
        UpdateBlit(blt);
    }
    return blt->Hide;
}
/* The pause screen's dimming: hidden unless the pause menu's first item is chosen; else it takes the dim colour
 * (animPause_color) once and stops animating. Returns 1. */
s32 animPause(Blit *blt)
{
    if (Hidden(blt, pauseMenuSelect != 0))
        return 1;
    blt->image = &animPause_color;
    UpdateBlit(blt);
    blt->AnimFunc = 0;
    return 1;
}
