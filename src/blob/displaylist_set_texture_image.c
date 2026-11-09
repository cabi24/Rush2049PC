/* displaylist_set_texture_image.c: change the image a texture's load display list sets.
 *
 * TextureAnimation's flipbooks swap a texture's image this way (displaylist_get_texture_image reads it). */
#include "game/game.h"

/* PC: the WAD's TXLD lists stay in N64 form, 8-byte big-endian commands (port/assets/wad.c). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)

/* The F3DEX2 commands looked for, in a command's top byte. */
#define GBI_SETTIMG 0xFD
#define GBI_ENDDL   0xDF

/* Write `value` as the image address of the first G_SETTIMG of the display list dl, if it has one before its
 * G_ENDDL. */
void displaylist_set_texture_image(u32 *dl,u32 value) {
    u8 op;
    for (;;dl+=2) {
        op=(DLW(dl[0])&0xFF000000)>>24;
        if ((op&0xC0)==0x40 || (op&0xC0)==0x80 || (op>=9 && op<0x40) || (op>=0xC0 && op<0xD6)) {
            continue;
        } else if (op==GBI_SETTIMG) {
            DLW_SET(dl[1], value);
            return;
        } else if (op==GBI_ENDDL) {
            return;
        } else {
            continue;
        }
    }
}
