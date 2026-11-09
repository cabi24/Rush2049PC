/* BlitClearColors: forget the blitter's colour cache (MAT3 blit_pc.c). */
#include "game/game.h"

VIEW(intptr_t, blitColors);
#define blitColors blitColors_view

/* Forgets which palette or colours the blitter loaded last (blitColors), so the next BlitColors loads them again even
 * when they are the same. Called where something other than the blitter has changed the RDP's colours. */
void BlitClearColors(void) {
    blitColors = 0;
}
