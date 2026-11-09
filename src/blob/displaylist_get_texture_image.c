/* displaylist_get_texture_image.c: the image address a texture's load display list sets.
 *
 * A WAD texture (TXHD) is drawn by its load display list (TXLD): its G_SETTIMG command holds the image's address.
 * InitTextureAnimation reads it for the texture flipbooks' frames; displaylist_set_texture_image writes it. */
#include "game/game.h"

/* The image address (the second word) of the first G_SETTIMG of the display list dl, read up to its G_ENDDL; the
 * opcode ranges skipped first hold no command the search wants either.
 * Original game: a list with no G_SETTIMG before its end returns no value (on the N64, whatever the return register
 * held). */
u32 displaylist_get_texture_image(u32 *dl) {
    u8 op;
    for (;;dl+=2) {
        op=(DLW(dl[0])&0xFF000000)>>24;
        if ((op&0xC0)==0x40 || (op&0xC0)==0x80 || (op>=9 && op<0x40) || (op>=0xC0 && op<0xD6)) {
            continue;
        } else if (op==G_SETTIMG) {
            return DLW(dl[1]);
        } else if (op==G_ENDDL) {
            return;
        } else {
            continue;
        }
    }
}
