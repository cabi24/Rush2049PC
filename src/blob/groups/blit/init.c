/* BlitInit: start the blitter's display list for a frame (MAT3 blit_pc.c; the N64 side of it), and the PC's storage
 * for the two lists. */
#include "game/game.h"

/* The two display lists the blitter fills in turn (blit_display_lists on the N64), 2,400 records each. */
Gfx pc_text_dl[2][2400];
#define TEXT_DL pc_text_dl

/* Starts the blitter for a frame, once (nothing when blit_dl is already set): resets its state (no attributes, the
 * scissor box the whole screen, depth 0, opaque, no mode, no palette, nothing cached), switches to the other of its two
 * display lists (blitDLnum), and opens it with the RDP set-up every blit relies on: the blend colour's alpha 16
 * (the alpha threshold), no perspective correction, no geometry modes, then BLIT_MODE_TEXTURE. On a screen taller
 * than 220 lines BLIT_ATTR_TALL draws textures at double height. BlitFinish closes the list. */
void BlitInit(void)
{
    if (blit_dl == 0) {
        blitAttribute = 0;
        blitXmin = 0;
        blitYmin = 0;
        blitXmax = screen_width - 1;
        blitYmax = screen_height - 1;
        blitZDist = 0;
        blitPrimColor.prim.a = 255;
        blit_cache_image = 0;
        blit_cache_tile = 0;
        if (++blitDLnum >= 2) blitDLnum = 0;
        blit_dl = TEXT_DL[blitDLnum];
        blitMode = -1;          /* no mode yet: the first blit_set_mode sets the cycle type */
        blitLUT = -1;
        blitColors = 0;
        gDPSetBlendColor(blit_dl++, 0, 0, 0, 16);
        gDPPipeSync(blit_dl++);
        gDPSetTexturePersp(blit_dl++, G_TP_NONE);
        gSPClearGeometryMode(blit_dl++, -1);
        blit_set_mode(BLIT_MODE_TEXTURE);
        if (screen_height >= 221) BlitSetAttribute(BLIT_ATTR_TALL);
    }
}
