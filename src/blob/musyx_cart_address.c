/* musyx_cart_address: the MusyX sound library's cartridge-address service (one of its eight services, docs/code/r5.md
 * "MusyX"; no MAT3 counterpart). */
#include "game/game.h"

/* Where the sound library reads a place in the ROM from (the sample bank's, sound_hw_cart_address): on the PC the ROM
 * offset itself, which the cartridge's reads take (pc_cart_read; docs/port/MUSYX_DATA.md 8.1). The N64's turned it
 * into the address the cartridge has on the CPU's bus, 0xB0000000 + the offset. */
u32 musyx_cart_address(u32 offset) {
    return offset;
}
