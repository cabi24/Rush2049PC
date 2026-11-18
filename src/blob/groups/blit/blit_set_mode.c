/* blit_set_mode: the RDP modes of the blitter's rectangles (MAT3 blit_pc.c; the N64 side of it, which MAT3 lacks). */
#include "game/game.h"

/* Puts the RDP into one of the blitter's modes (BLIT_MODE_*): its cycle type, render mode and colour combiner, the
 * combiner and render mode also chosen by BLIT_ATTR_XLU (the alpha from the primitive colour) and BLIT_ATTR_ZDIST
 * (depth-tested render modes). The cycle type is set only when the mode before (blitMode) had another; blitMode is
 * then the new mode. BlitSetAttribute and BlitClearAttribute call it again with blitMode when those two bits change. */
void blit_set_mode(s32 mode)
{
    switch (mode) {
    case BLIT_MODE_COPY:
        if (blitMode != BLIT_MODE_COPY) {
            gDPSetCycleType(blit_dl++, G_CYC_COPY);
        }
        gDPSetRenderMode(blit_dl++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPSetCombineMode(blit_dl++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        break;
    case BLIT_MODE_TEXTURE:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderMode(blit_dl++, G_RM_ZB_XLU_SURF | Z_UPD, G_RM_ZB_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderMode(blit_dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderMode(blit_dl++, G_RM_ZB_XLU_SURF | Z_UPD, G_RM_ZB_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
        else {
            gDPSetRenderMode(blit_dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
        break;
    case BLIT_MODE_INTENSITY:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderMode(blit_dl++, G_RM_ZB_XLU_SURF | Z_UPD, G_RM_ZB_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderMode(blit_dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderMode(blit_dl++, G_RM_ZB_XLU_SURF | Z_UPD, G_RM_ZB_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        }
        else {
            gDPSetRenderMode(blit_dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        }
        break;
    case BLIT_MODE_INTENSITY_ALPHA:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderMode(blit_dl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderMode(blit_dl++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderMode(blit_dl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        }
        else {
            gDPSetRenderMode(blit_dl++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(blit_dl++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        }
        break;
    case BLIT_MODE_TWO_TILE:
        if (blitMode < BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_2CYCLE);
        }
        gDPSetRenderMode(blit_dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(blit_dl++, G_CC_DECALRGB_LERPA_ENV, G_CC_PASS2);
        gDPSetEnvColor(blit_dl++, 0, 0, 0, 0x55);
        break;
    }
    blitMode = mode;
}
