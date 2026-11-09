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

VIEW(Gfx *, blit_dl);
#define blit_dl blit_dl_view

void BlitClearAttribute(u32 mask)
{
    if (blitAttribute & mask) {
        blitAttribute &= ~mask;
        if (mask & 1) {
            gDPSetAlphaCompare(blit_dl++, G_AC_NONE);
        }
        if (mask & 0x10) {
            gDPSetDepthSource(blit_dl++, G_ZS_PIXEL);
            blit_set_mode(blitMode);
        }
        if (mask & 0x20) {
            blit_set_mode(blitMode);
        }
    }
}

void BlitSetAttribute(u32 mask)
{
    if ((blitAttribute & mask) != mask) {
        blitAttribute |= mask;
        if (mask & 0x4000) {
            gDPSetCombine(blit_dl++, 0xFFFFFF, 0xFFFDF6FB);
            if (blitMode <= 0 || blitMode >= 4) {
                gDPSetCycleType(blit_dl++, G_CYC_1CYCLE);
            }
            blitMode = -1;
        }
        if (mask & 1) {
            gDPSetAlphaCompare(blit_dl++, G_AC_THRESHOLD);
        }
        if (mask & 0x10) {
            gDPSetDepthSource(blit_dl++, G_ZS_PRIM);
            blit_set_mode(blitMode);
        }
        if (mask & 0x20) {
            blit_set_mode(blitMode);
        }
    }
}
