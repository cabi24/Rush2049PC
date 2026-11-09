/* texcmp.c: the comparator of the texture and palette name searches (MAT3 mbox.c, texcmp()).
 *
 * MBOX_FindTexture_WORK and MBOX_FindPalette_WORK search the models' sorted TXHD and PLHD name tables with
 * game_bsearch and this. */
#include "game/game.h"

/* Compare the texture or palette records ta and tb (the name first) on their whole names: game_strcmp's result.
 * On the N64 the function returned nothing and passed game_strcmp's result on by leaving it in the return
 * register; on the PC it returns it. */
s32 texcmp(u8 *ta, u8 *tb) {
    return game_strcmp(ta, tb);
}
