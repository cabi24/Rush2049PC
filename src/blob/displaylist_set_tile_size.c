/* displaylist_set_tile_size.c: change the tile rectangle a texture's load display list sets.
 *
 * TextureAnimation's texture scrolls move a texture by rewriting the corner of its tile size records. */
#include "game/game.h"

/* In every tile size record (GFX_OP_TILE_SIZE) of the display list `commands` up to its end, set the tile's corners:
 * uls and ult the upper left, lrs and lrt the lower right, in the RDP's quarter texels; a negative value leaves its
 * field as it is. The first tile's size (lrs + 4, lrt + 4: its width and height in quarter texels) is the reference:
 * for each later, smaller tile (the smaller levels of a mipmap) uls and ult are halved as its size is. Each corner is
 * cut to the RDP's 12 bits, as the N64's command held it (a scroll stays below 4096 quarter texels; past it the N64's
 * word would have spilled into the field beside it). */
void displaylist_set_tile_size(Gfx *commands, s32 uls, s32 ult, s32 lrs, s32 lrt) {
    s32 width = -1, height = -1, size;

    for (;; commands++) {
        if (commands->op == GFX_OP_TILE_SIZE) {
            if (uls >= 0) {
                size = commands->tile_rect.lrs + 4;
                if (width < 0) {
                    width = size;
                } else {
                    while (size < width) {
                        width >>= 1;
                        uls >>= 1;
                    }
                }
                commands->tile_rect.uls = GBI_FIELD(uls, 12);
            }
            if (ult >= 0) {
                size = commands->tile_rect.lrt + 4;
                if (height < 0) {
                    height = size;
                } else {
                    while (size < height) {
                        height >>= 1;
                        ult >>= 1;
                    }
                }
                commands->tile_rect.ult = GBI_FIELD(ult, 12);
            }
            if (lrs >= 0) {
                commands->tile_rect.lrs = GBI_FIELD(lrs, 12);
            }
            if (lrt >= 0) {
                commands->tile_rect.lrt = GBI_FIELD(lrt, 12);
            }
        } else if (commands->op == GFX_OP_END_LIST) {
            break;
        }
    }
}
