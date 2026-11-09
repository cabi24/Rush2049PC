/* musyx_cart_address: the MusyX sound library's cartridge-address service (one of its eight services, docs/code/r5.md
 * "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* PC: the MusyX library calls it through its service table as void *(*)(void *), so the word is pointer-sized
 * (docs/port/MUSYX_DATA.md 8.1). */
#define CART_WORD intptr_t

/* Turn a sample's cartridge offset into its address in the N64's uncached cartridge window (0xB0000000 up), which
 * the DMA service reads from. */
CART_WORD musyx_cart_address(CART_WORD offset) {
    return (offset & 0x0FFFFFFF) | 0xB0000000;
}
