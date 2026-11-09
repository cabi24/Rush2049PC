#include "game/game.h"

typedef struct { u8 a; u8 b; u8 c; u8 d; } S;
VIEW(S, blitPrimColor);
#define blitPrimColor blitPrimColor_view
VIEW(intptr_t, blitColors);
#define blitColors blitColors_view

void BlitXLU(u16 arg0) {
    if (blitPrimColor.d != arg0) {
        blitPrimColor.d = arg0;
        blitColors = 0;
    }
    BlitColors(&blitPrimColor, 4, 1, 0);
    BlitSetAttribute(32);
}
