#include "game/game.h"

/* PC: the WAD's TXLD lists stay in N64 form, 8-byte big-endian commands (port/assets/wad.c). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)
void displaylist_set_texture_image(u32 *dl,u32 value) {
    u8 op;
    for (;;dl+=2) {
        op=(DLW(dl[0])&0xFF000000)>>24;
        if ((op&0xC0)==0x40 || (op&0xC0)==0x80 || (op>=9 && op<0x40) || (op>=0xC0 && op<0xD6)) {
            continue;
        } else if (op==0xFD) {
            DLW_SET(dl[1], value);
            return;
        } else if (op==0xDF) {
            return;
        } else {
            continue;
        }
    }
}
