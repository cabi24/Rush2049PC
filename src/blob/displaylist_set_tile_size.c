/* displaylist_set_tile_size.c: change the tile rectangle a texture's load display list sets.
 *
 * TextureAnimation's texture scrolls move a texture by rewriting the corner of its G_SETTILESIZE commands. */
#include "game/game.h"

/* In every G_SETTILESIZE of the display list `commands` up to its G_ENDDL, set the tile's corners: uls and ult the
 * upper left (bits 12-23 and 0-11 of the first word), lrs and lrt the lower right (of the second word), in the
 * RDP's quarter texels; a negative value leaves its field as it is. The first tile's size (lrs + 4, lrt + 4: its
 * width and height in quarter texels) is the reference: for each later, smaller tile (the smaller levels of a
 * mipmap) uls and ult are halved as its size is. Each field is read again after the one before it is written, as on
 * the N64. */
void displaylist_set_tile_size(Gfx *commands, s32 uls, s32 ult, s32 lrs, s32 lrt) {
    s32 width = -1, height = -1, size;
    u8 opcode;

    for (;; commands++) {
        opcode = commands->words.w0 >> 24;
        if (opcode == G_SETTILESIZE) {
            if (uls >= 0) {
                size = (((u32)commands->words.w1 & 0x00fff000) >> 12) + 4;
                if (width < 0) {
                    width = size;
                } else {
                    while (size < width) {
                        width >>= 1;
                        uls >>= 1;
                    }
                }
                commands->words.w0 = (commands->words.w0 & 0xff000fff) | (uls << 12);
            }
            if (ult >= 0) {
                size = ((u32)commands->words.w1 & 0xfff) + 4;
                if (height < 0) {
                    height = size;
                } else {
                    while (size < height) {
                        height >>= 1;
                        ult >>= 1;
                    }
                }
                commands->words.w0 = (commands->words.w0 & 0xfffff000) | ult;
            }
            if (lrs >= 0) {
                commands->words.w1 = ((u32)commands->words.w1 & 0xff000fff) | (lrs << 12);
            }
            if (lrt >= 0) {
                commands->words.w1 = ((u32)commands->words.w1 & 0xfffff000) | lrt;
            }
        } else if (opcode == G_ENDDL) {
            break;
        }
    }
}
