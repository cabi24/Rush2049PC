/* blit_set_mode: the RDP modes of the blitter's rectangles (MAT3 blit_pc.c; the N64 side of it, which MAT3 lacks). */
#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union {
    Gwords words;
    long long int force_structure_alignment;
} Gfx;

#define _SHIFTL(v, s, w) ((u32)(((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define G_SETCOMBINE 0xFC
#define G_SETENVCOLOR 0xFB
#define G_SETPRIMDEPTH 0xEE
#define G_SETOTHERMODE_L 0xE2
#define G_SETOTHERMODE_H 0xE3
#define G_MDSFT_ALPHACOMPARE 0
#define G_MDSFT_ZSRCSEL 2
#define G_MDSFT_RENDERMODE 3
#define G_MDSFT_CYCLETYPE 20
#define G_CYC_1CYCLE (0 << G_MDSFT_CYCLETYPE)
#define G_CYC_2CYCLE (1 << G_MDSFT_CYCLETYPE)
#define G_CYC_COPY (2 << G_MDSFT_CYCLETYPE)
#define G_AC_NONE (0 << G_MDSFT_ALPHACOMPARE)
#define G_AC_THRESHOLD (1 << G_MDSFT_ALPHACOMPARE)
#define G_ZS_PIXEL (0 << G_MDSFT_ZSRCSEL)
#define G_ZS_PRIM (1 << G_MDSFT_ZSRCSEL)

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
#define gDPSetColor(pkt, c, d) \
{ \
    Gfx *_g = (Gfx *)(pkt); \
    _g->words.w0 = _SHIFTL(c, 24, 8); \
    _g->words.w1 = (unsigned int)(d); \
}
#define sDPRGBColor(pkt, cmd, r, g, b, a) \
    gDPSetColor(pkt, cmd, (_SHIFTL(r, 24, 8) | _SHIFTL(g, 16, 8) | _SHIFTL(b, 8, 8) | _SHIFTL(a, 0, 8)))
#define gDPSetEnvColor(pkt, r, g, b, a) sDPRGBColor(pkt, G_SETENVCOLOR, r, g, b, a)

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

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
        gDPSetRenderModeRaw(blit_dl++, 0x0F0A4000);
        gDPSetCombine(blit_dl++, 0xFFFFFF, 0xFFFCF279);
        break;
    case BLIT_MODE_TEXTURE:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504A70);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504240);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504A70);
            gDPSetCombine(blit_dl++, 0xFFFFFF, 0xFFFCF279);
        }
        else {
            gDPSetRenderModeRaw(blit_dl++, 0x00504240);
            gDPSetCombine(blit_dl++, 0xFFFFFF, 0xFFFCF279);
        }
        break;
    case BLIT_MODE_INTENSITY:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504A70);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504240);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderModeRaw(blit_dl++, 0x00504A70);
            gDPSetCombine(blit_dl++, 0x11FE23, 0xFFFFF3F9);
        }
        else {
            gDPSetRenderModeRaw(blit_dl++, 0x00504240);
            gDPSetCombine(blit_dl++, 0x11FE23, 0xFFFFF3F9);
        }
        break;
    case BLIT_MODE_INTENSITY_ALPHA:
        if (blitMode <= BLIT_MODE_COPY || blitMode >= BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
        }
        if ((blitAttribute & BLIT_ATTR_ZDIST) && (blitAttribute & BLIT_ATTR_XLU)) {
            gDPSetRenderModeRaw(blit_dl++, 0x00553078);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_XLU) {
            gDPSetRenderModeRaw(blit_dl++, 0x0F0A7008);
            gDPSetCombine(blit_dl++, 0x119623, 0xFF2FFFFF);
        }
        else if (blitAttribute & BLIT_ATTR_ZDIST) {
            gDPSetRenderModeRaw(blit_dl++, 0x00553078);
            gDPSetCombine(blit_dl++, 0x11FE23, 0xFFFFF3F9);
        }
        else {
            gDPSetRenderModeRaw(blit_dl++, 0x0F0A7008);
            gDPSetCombine(blit_dl++, 0x11FE23, 0xFFFFF3F9);
        }
        break;
    case BLIT_MODE_TWO_TILE:
        if (blitMode < BLIT_MODE_TWO_TILE) {
            gDPSetCycleType(blit_dl++, G_CYC_2CYCLE);
        }
        gDPSetRenderModeRaw(blit_dl++, 0x00504240);
        gDPSetCombine(blit_dl++, 0xFFABFF, 0xFFFC9238);
        gDPSetEnvColor(blit_dl++, 0, 0, 0, 0x55);
        break;
    }
    blitMode = mode;
}
