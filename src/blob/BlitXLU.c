/* BlitXLU: draw translucent blits (MAT3 blit_pc.c). */
#include "game/game.h"

/* Makes what the blitter draws next translucent: alpha (0..255) becomes the primitive colour's alpha, loaded into the
 * RDP's primitive and environment colours (BlitColors with the I format), and BLIT_ATTR_XLU is set. A new alpha
 * forgets the colour cache first, so BlitColors loads it even though the colour's address is the same. */
void BlitXLU(u16 alpha) {
    if (blitPrimColor.a != alpha) {
        blitPrimColor.a = alpha;
        blitColors = 0;
    }
    BlitColors(&blitPrimColor, 4, 1, 0);
    BlitSetAttribute(BLIT_ATTR_XLU);
}
