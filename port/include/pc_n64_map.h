/* port/include/pc_n64_map.h: the N64's memory map, the one place the PC layer writes its numbers.
 *
 * The game still speaks the N64's 32-bit addresses. Its data sits at its N64 addresses in an RDRAM window that the PC
 * maps at host address 0x80000000 (docs/port/MEMORY.md, NOFIXED.md); display lists and the audio task's records carry
 * such addresses; the converted files are relocated to them; the data loader's tables say where each datum lives. So
 * the PC layer still needs the N64's address space: the MIPS segments, the size of its RDRAM, where libultra keeps its
 * boot parameters, where the cartridge sits on the bus, where the game loads its runtime images. Each of those numbers
 * is named here and written nowhere else in port/; code elsewhere uses the names, or asks the data loader for a
 * symbol's address by its name (pc_data_addr_of, port/data/pc_data.h).
 *
 * Everything here describes the N64, not the PC. Wave 3 of docs/port/PCONLY.md moves the game's data out of the window
 * into ordinary PC storage and makes the runtime images PC memory: the window's constants, the low-memory addresses and
 * the runtime images' block go then, and the segments once nothing carries a 32-bit N64 address. What outlives it: the
 * cartridge's bus addresses (the game still reads its cartridge through the PI) and the segmented addresses of the
 * display lists the renderer still interprets.
 *
 * libultra's names for these (R4300.h, rcp.h, os.h) are in the comments. No system headers and only plain constants:
 * pc_prelude.h, force-included into the game C, includes this file too, so the game's C can use the same names. */
#ifndef PC_N64_MAP_H
#define PC_N64_MAP_H

/* ---------------------------------------------------------------- the MIPS address segments */

/* The R4300's two unmapped segments both see the first 512 MiB of the physical address space (RDRAM from physical 0,
 * then the RCP's registers and the cartridge): KSEG0 through the cache, KSEG1 around it. The game reaches RDRAM through
 * KSEG0 and the devices through KSEG1. Above them are the mapped segments, which the game never uses. */
#define N64_KSEG0 0x80000000u       /* K0BASE: the cached segment; RDRAM's first byte is KSEG0's first address */
#define N64_KSEG1 0xA0000000u       /* K1BASE: the uncached segment, the same physical memory again */
#define N64_KSEG2 0xC0000000u       /* K2BASE: the first mapped segment, so the end of KSEG1 */
#define N64_PHYS_MASK 0x1FFFFFFFu   /* K0_TO_PHYS, K1_TO_PHYS: keeps a KSEG0 or KSEG1 address's physical address */

/* An address in KSEG0 or in KSEG1: a CPU address of RDRAM or a device, rather than a physical or segmented address (or
 * the low half of a host pointer). */
#define N64_IS_KSEG0_OR_1(a) ((a) >= N64_KSEG0 && (a) < N64_KSEG2)
/* An address in KSEG1, the uncached alias. */
#define N64_IS_KSEG1(a) ((a) >= N64_KSEG1 && (a) < N64_KSEG2)
/* The same memory's KSEG0 address for a KSEG1 address. */
#define N64_KSEG1_TO_KSEG0(a) ((a) - (N64_KSEG1 - N64_KSEG0))
/* The physical address of a KSEG0 or KSEG1 address (K0_TO_PHYS, K1_TO_PHYS). */
#define N64_TO_PHYS(a) ((a) & N64_PHYS_MASK)

/* A segmented address, as the RSP reads the addresses in a display list or an audio command list: below KSEG0, bits
 * 24-27 are a segment number (whose base the list set with G_SEGMENT) and the low 24 bits an offset in it. */
#define N64_SEGMENT_ID_MASK 0x0F000000u       /* SEGMENT_NUMBER's bits, in place */
#define N64_SEGMENT_OFFSET_MASK 0x00FFFFFFu   /* SEGMENT_OFFSET */

/* ---------------------------------------------------------------- RDRAM */

#define N64_RDRAM_SIZE 0x00800000u          /* the console's RDRAM with the Expansion Pak: 8 MiB */
#define N64_RDRAM_SIZE_NO_PAK 0x00400000u   /* and without it: 4 MiB */
#define N64_RDRAM_END (N64_KSEG0 + N64_RDRAM_SIZE)   /* KSEG0's first address past the 8 MiB */

/* libultra's low memory: the exception vectors at RDRAM's start, then the boot parameters the IPL leaves for the game
 * at N64_BOOT_PARAMS (osTvType, osRomType, osRomBase, osResetType, osCicId, osVersion, osMemSize, osAppNMIBuffer: the
 * PC's pc_os_initialize writes them, port/ultra/misc.c), then the cartridge's boot code from N64_LOW_MEMORY_END. */
#define N64_BOOT_PARAMS 0x80000300u      /* osTvType's address, the first boot parameter */
#define N64_LOW_MEMORY_END 0x80000400u   /* the boot code's first address (the cartridge's entry point) */

/* ---------------------------------------------------------------- the cartridge on the PI bus */

/* The cartridge ROM is PI domain 1, from physical N64_CART_PHYS up to the PIF's boot ROM. A ROM offset is an address
 * there less N64_CART_PHYS; the game reads its cartridge through KSEG1 (osRomBase is N64_CART_KSEG1). These stay after
 * wave 3: the game still reads its cartridge through the PI. */
#define N64_CART_PHYS 0x10000000u      /* PI_DOM1_ADDR2: the cartridge ROM's first byte, physical */
#define N64_PIF_ROM_PHYS 0x1FC00000u   /* PIF_ROM_START: the PIF's boot ROM, where the cartridge's range ends */
#define N64_CART_KSEG1 (N64_KSEG1 | N64_CART_PHYS)        /* the cartridge's first byte through KSEG1 (osRomBase) */
#define N64_CART_SPAN (N64_PIF_ROM_PHYS - N64_CART_PHYS)   /* the bytes of ROM the bus can address */

/* ---------------------------------------------------------------- the game's heap */

/* The game's main heap started at the end of its image's BSS (game_bss_end on the N64) and ran to the end of RDRAM
 * (osMemSize). The PC gives the game's heap those bytes of the window (port/ultra/heap.c, pc_heap_memory) until wave 3
 * makes the heap a block of PC memory (docs/port/NATIVE.md step 12). */
#define N64_HEAP_BASE 0x8017A640u

/* ---------------------------------------------------------------- the runtime images (overlays) */

/* The game's overlay loader (src/blob/groups/heap_release/overlay_loader.c) loads runtime image A (the front end) or
 * image B (the race's extras) at N64_OVERLAY_BASE, in a block of the game heap that it reserves up to
 * N64_OVERLAY_BLOCK_END (a little past image A's BSS, the larger image). The PC links both images in and a load resets
 * one image's data (pc_data_overlay_load), but the game's heap keeps the block reserved as on the N64
 * (port/ultra/heap.c). Wave 3 makes the images PC memory, and the block goes. */
#define N64_OVERLAY_BASE 0x8038A400u        /* both runtime images' load address (the symbol overlay_base) */
#define N64_OVERLAY_BLOCK_END 0x803BB380u   /* the end of the heap block reserved for them */

/* ---------------------------------------------------------------- the PC's RDRAM window */

/* The N64's RDRAM as the game addresses it outside its own symbols (the heap, frame buffers, the runtime images' block
 * at N64_OVERLAY_BASE): mapped at host address PC_RDRAM_BASE where the OS allows it, so the N64's KSEG0 addresses in
 * that range are host addresses; elsewhere (a RUSH_NOFIXED build, the guard mode) they translate (pc_n64_ptr,
 * pc_n64_addr; pc_prelude.h's PC_N64P and PC_N64A for the game's C).
 *
 * game.memory (docs/port/MEMORY.md): PC_RDRAM_SIZE is the N64's 8 MiB, the window at `original`; a size gives the game
 * a window (and osMemSize) of up to PC_RDRAM_MAX. From PC_RCP_SEGMENT_SPAN on, a physical address would read as a
 * segmented one to the RSP (its segment number is bits 24-27), so the PC gives the RCP the KSEG0 address there. */
#define PC_RDRAM_BASE N64_KSEG0              /* the window's first byte: KSEG0's, and its host address when fixed */
#define PC_RDRAM_SIZE N64_RDRAM_SIZE         /* the window at game.memory = original */
#define PC_RDRAM_MAX 0x10000000u             /* the largest window: 256 MiB */
#define PC_RCP_SEGMENT_SPAN 0x01000000u      /* 16 MiB: the offsets a segmented address can hold */

#endif
