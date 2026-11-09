#include "game/game.h"

VIEW(intptr_t, blitColors);
#define blitColors blitColors_view

void BlitClearColors(void) {
    blitColors = 0;
}
