/* BlitColors: load a texture's palette or colours into the RDP (MAT3 blit_pc.c; the N64 side of it). */
#include "game/game.h"

/* Loads the colours a texture of this format draws with, unless they are the ones loaded last (blitColors):
 *   colors      a colour-indexed texture's palette (RGBA16 entries), or for intensity textures two RGBA colours
 *   format      the texture's format: 2 colour-indexed (5: the two-tile form), 3 IA, 4 I (G_IM_FMT_*)
 *   texel_size  1 for 8-bit texels (a 256-colour palette), 0 for 4-bit (16 colours, in bank palette_bank)
 * An intensity texture is tinted by the primitive colour (colors[0..3]) over the environment colour (colors[4..7]).
 * BlitRect passes -1 for both, which loads nothing and only remembers the colour. */
void BlitColors(u8 *colors, s32 format, s32 texel_size, s32 palette_bank) {
    if (colors == blitColors) {
        return;
    }
    if ((format == G_IM_FMT_CI || format == 5) && texel_size == 1) {
        gDPLoadTLUT_pal256(blit_dl++, colors);
    } else if (format == G_IM_FMT_CI && texel_size == 0) {
        gDPLoadTLUT_pal16(blit_dl++, palette_bank, colors);
    } else if (format == G_IM_FMT_I || format == G_IM_FMT_IA) {
        gDPSetPrimColor(blit_dl++, 0, 0, colors[0], colors[1], colors[2], colors[3]);
        gDPSetEnvColor(blit_dl++, colors[4], colors[5], colors[6], colors[7]);
    }
    blitColors = colors;
}
