/* port/include/pc_n64_map.h: what is left of the N64's memory map in the PC layer, the one place it writes those numbers.
 *
 * Nothing of the game is at an N64 address any more (docs/port/NATIVE.md): its data are host storage, its heap a block
 * of PC memory, its display lists are records with host pointers (pc_gfx_list.h), and it reads the cartridge by ROM
 * offset (docs/port/ULTRA.md 11). One number of the N64 remains, and it is named here: how far the cartridge reaches,
 * which every ROM offset is below (the ROM's and the mods' region's).
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

#endif
