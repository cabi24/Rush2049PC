/* port/include/pc_n64_map.h: what is left of the N64's memory map in the PC layer, the one place it writes those numbers.
 *
 * Nothing of the game is at an N64 address any more (docs/port/NATIVE.md): its data are host storage, its heap a block
 * of PC memory, its display lists hold host pointers. Two N64 formats remain, and their numbers are named here:
 *   - the cartridge on the PI bus: the game still reads its cartridge through libultra's PI, by the cartridge's bus
 *     addresses (osRomBase is N64_CART_KSEG1), and the MusyX library names its samples by them;
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

/* ---------------------------------------------------------------- the cartridge on the PI bus */

/* The cartridge ROM is PI domain 1, from bus address N64_CART_PHYS up to the PIF's boot ROM. A ROM offset is a bus
 * address there less N64_CART_PHYS; the game names the cartridge through the CPU's uncached segment, KSEG1
 * (N64_CART_KSEG1, osRomBase), whose addresses are the bus address with 0xA0000000 added. */
#define N64_CART_PHYS 0x10000000u      /* PI_DOM1_ADDR2: the cartridge ROM's first byte on the bus */
#define N64_PIF_ROM_PHYS 0x1FC00000u   /* PIF_ROM_START: the PIF's boot ROM, where the cartridge's range ends */
#define N64_CART_KSEG1 0xB0000000u     /* the cartridge's first byte through KSEG1 (osRomBase) */
#define N64_CART_SPAN (N64_PIF_ROM_PHYS - N64_CART_PHYS)   /* the bytes of ROM the bus can address */
/* The bus address of a CPU address in KSEG0 or KSEG1 (K0_TO_PHYS, K1_TO_PHYS): a cartridge address's. */
#define N64_TO_PHYS(a) ((a) & 0x1FFFFFFFu)

/* ---------------------------------------------------------------- a display list's segments */

/* A segmented address, as the RSP reads the addresses in a display list: below N64_SEGMENT_SPAN, bits 24-27 are a
 * segment number (whose base a G_MOVEWORD of G_MW_SEGMENT set) and the low 24 bits an offset in it. */
#define N64_SEGMENT_ID_MASK 0x0F000000u       /* SEGMENT_NUMBER's bits, in place */
#define N64_SEGMENT_OFFSET_MASK 0x00FFFFFFu   /* SEGMENT_OFFSET */
#define N64_SEGMENT_SPAN 0x10000000u          /* 16 segments of 16 MiB */

#endif
