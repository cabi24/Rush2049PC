/* BlitSetAttribute and BlitClearAttribute: the blitter's state bits (MAT3 blit_pc.c; the N64 side, with the RDP
 * commands each bit needs). */
#include "game/game.h"

/* Clears the BLIT_ATTR_* bits of mask that are set, undoing what each did: the alpha threshold off, the depth from the
 * pixels again; the depth and translucency bits choose the render mode and combiner, so blit_set_mode sets the current
 * mode again. */
void BlitClearAttribute(u32 mask)
{
    if (blitAttribute & mask) {
        blitAttribute &= ~mask;
        if (mask & BLIT_ATTR_ALPHA_COMPARE) {
            gDPSetAlphaCompare(blit_dl++, G_AC_NONE);
        }
        if (mask & BLIT_ATTR_ZDIST) {
            gDPSetDepthSource(blit_dl++, G_ZS_PIXEL);
            blit_set_mode(blitMode);
        }
        if (mask & BLIT_ATTR_XLU) {
            blit_set_mode(blitMode);
        }
    }
}

/* Sets the BLIT_ATTR_* bits of mask, unless all are set already, with the RDP commands each needs: BLIT_ATTR_FILL a
 * combiner of the primitive colour alone in one-cycle mode (blitMode -1 then: the next blit_set_mode sets everything
 * again), the alpha threshold on, the depth from the primitive depth (BlitZDist); the depth and translucency bits
 * choose the render mode and combiner, so blit_set_mode sets the current mode again. The flip and tall bits only
 * change how BlitDraw computes its texture coordinates. */
void BlitSetAttribute(u32 mask)
{
    if ((blitAttribute & mask) != mask) {
        blitAttribute |= mask;
        if (mask & BLIT_ATTR_FILL) {
            gDPSetCombineMode(blit_dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
                gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
            }
            blitMode = -1;
        }
        if (mask & BLIT_ATTR_ALPHA_COMPARE) {
            gDPSetAlphaCompare(blit_dl++, G_AC_THRESHOLD);
        }
        if (mask & BLIT_ATTR_ZDIST) {
            gDPSetDepthSource(blit_dl++, G_ZS_PRIM);
            blit_set_mode(blitMode);
        }
        if (mask & BLIT_ATTR_XLU) {
            blit_set_mode(blitMode);
        }
    }
}
