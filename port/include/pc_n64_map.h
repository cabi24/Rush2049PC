/* port/include/pc_n64_map.h: what is left of the N64's memory map in the PC layer, the one place it writes those numbers.
 *
 * Nothing of the game is at an N64 address any more (docs/port/NATIVE.md): its data are host storage, its heap a block
 * of PC memory, its display lists hold host pointers, and it reads the cartridge by ROM offset (docs/port/ULTRA.md 11).
 * Two numbers of the N64 remain, and they are named here:
 *   - how far the cartridge reaches: every ROM offset is below it (the ROM's and the mods' region's);
 *   - a display list's segments: the microcode's segment table, which the renderer keeps (an address below
 *     N64_SEGMENT_SPAN can name a segment and an offset in it).
 * The data loader decodes the N64 addresses in the cartridge images' data itself (port/data/pc_data.c), and the N64
 * guard reserves the N64's address range with numbers of its own (port/ultra/memory.c): both read the ROM's format, not
 * the game's memory.
 *
 * libultra's names for these (R4300.h, rcp.h, os.h, gbi.h) are in the comments. No system headers and only plain
 * constants: pc_prelude.h, force-included into the game C, includes this file too. */
#ifndef PC_N64_MAP_H
#define PC_N64_MAP_H

/* ---------------------------------------------------------------- the cartridge */

/* The bytes of cartridge the N64 could address (its ROM ran from bus address 0x10000000 to the PIF's boot ROM at
 * 0x1FC00000): the ROM offsets the game reads are below it, the mods' region's too (port/ultra/ultra.h's
 * PC_ROM_MODS_MAX), so the sound library tells a place in the cartridge from a pointer by its size. */
#define N64_CART_SPAN 0x0FC00000u

/* ---------------------------------------------------------------- a display list's segments */

/* A segmented address, as the RSP reads the addresses in a display list: below N64_SEGMENT_SPAN, bits 24-27 are a
 * segment number (whose base a G_MOVEWORD of G_MW_SEGMENT set) and the low 24 bits an offset in it. */
#define N64_SEGMENT_ID_MASK 0x0F000000u       /* SEGMENT_NUMBER's bits, in place */
#define N64_SEGMENT_OFFSET_MASK 0x00FFFFFFu   /* SEGMENT_OFFSET */
#define N64_SEGMENT_SPAN 0x10000000u          /* 16 segments of 16 MiB */

#endif
