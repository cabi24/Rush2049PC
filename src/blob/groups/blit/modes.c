/* BlitSetAttribute and BlitClearAttribute: the blitter's state bits (MAT3 blit_pc.c; the N64 side, with the RDP
 * commands each bit needs). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))

#define gSPSetOtherMode(pkt, cmd, sft, len, data) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = (_SHIFTL(cmd, 24, 8) | _SHIFTL(32 - (sft) - (len), 8, 8) | _SHIFTL((len) - 1, 0, 8)); \
    _g->words.w1 = (unsigned int)(data); \
}
#define gDPSetCycleType(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_H, G_MDSFT_CYCLETYPE, 2, type)
#define gDPSetAlphaCompare(pkt, type) gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_ALPHACOMPARE, 2, type)
#define gDPSetDepthSource(pkt, src) gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_ZSRCSEL, 1, src)
#define gDPSetRenderModeRaw(pkt, m) gSPSetOtherMode(pkt, G_SETOTHERMODE_L, G_MDSFT_RENDERMODE, 29, m)
#define gDPSetCombine(pkt, muxs0, muxs1) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(G_SETCOMBINE, 24, 8) | _SHIFTL(muxs0, 0, 24); \
    _g->words.w1 = (unsigned int)(muxs1); \
}

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
            gDPSetCombine(blit_dl++, 0xFFFFFF, 0xFFFDF6FB);
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
