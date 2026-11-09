/* musyx_cart_address: the MusyX sound library's cartridge-address service (one of its eight services, docs/code/r5.md
 * "MusyX"; N64 only, no MAT3 counterpart). */
#include "game/game.h"

/* The cartridge's bus: the PI reads the ROM at this address plus the offset. */
#define CART_BUS_BASE 0xB0000000U
#define CART_OFFSET_MASK 0x0FFFFFFFU

/* Turns an offset in the ROM (the sample bank's, sound_hw_cart_address) into its cartridge address, which the DMA
 * service reads from (docs/port/MUSYX_DATA.md 8.1). */
u32 musyx_cart_address(u32 offset) {
    return (offset & CART_OFFSET_MASK) | CART_BUS_BASE;
}
