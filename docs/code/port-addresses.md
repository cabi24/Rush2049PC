# The N64 addresses left in the port

The game keeps no N64 address: its data are host storage, each datum an object of its own, its heap a block of PC
memory wherever the OS puts it, its display lists' pointers host pointers ([NATIVE.md](../port/NATIVE.md), waves 3 and
3.6). What is left of the N64's memory map is in the PC layer (`port/`) and the tools (`tools/port/`), for the formats
the PC still reads: the cartridge and what is in it. This page lists them, as of wave 4 (2026-11-18).

Until then this page was the inventory wave 2 of [PCONLY.md](../port/PCONLY.md) made of every N64 address `port/` and
the tools spelled as a number (663 in port/ before wave 2, 117 after it, the cartridge's and the segments' alone after
wave 3's step 12), with what became of each. That inventory is in the history (this file before 2026-11-18);
[NATIVE.md](../port/NATIVE.md) 17 says what step 12 deleted and kept.

| Where | Names | What | Why it stays |
|---|---|---|---|
| `port/include/pc_n64_map.h` | `N64_CART_SPAN` | how much cartridge the N64 could address (its ROM from bus address 0x10000000 to the PIF's boot ROM) | every ROM offset the game reads is below it, the mods' region's too (`PC_ROM_MODS_MAX`, port/ultra/ultra.h), so the sound library tells a ROM offset from a pointer by its size |
| `port/data/pc_data.h`, `tools/port/n64syms.py` | `N64_KSEG0`, `N64_KSEG1`, `N64_KSEG2`, `N64_RDRAM_END`, `N64_LOW_MEMORY_END`, `N64_OVERLAY_BASE`, `N64_IS_KSEG0_OR_1`, `N64_IS_KSEG1`, `N64_KSEG1_TO_KSEG0`; n64syms's `KSEG0` ... `OVERLAY_BASE` | the cartridge images' link addresses | the data loader decodes the pointers in the ROM's data and the runtime images' relocations, once, at start-up; the tools (gen_data, gen_stubs, the checks) read the symbol files, which are keyed by address |
| `port/data/asm_functions.txt`, `decoupled.txt`, `test_pc_data_decls.c` | 43 routines by address; the decoupled records' N64 ranges, in comments; the loader test's fixtures (`D_80010020` ...) | the boot code's assembly routines for gen_data's function table; where each record the loader moves out of the ROM's layout was; names the loader's address parsing is tested on | the ROM's format: gen_data's input and its test |
| `port/ultra/memory.c` | `GUARD_LO`, `GUARD_HI`, `GUARD_KSEG1`, `GUARD_TRAP` | the range the N64 guard (`RUSH2049_N64_GUARD=1`) reserves with no access, and the trap for a heap pointer cut to 32 bits | a debugging mode: any N64 address used as a pointer faults |
| `port/ultra/statehash.c` | `RUSH_SIM_DESYNC=RETRACE:WORD` | a test's desync: WORD is a word of the game's data by its N64 address, found through the loader's tables (or `heap+OFFSET`) | the online-play tests name the word they change that way (`net_session_check.py`, LOCKSTEP.md 5.4) |
| `port/include/pc_game_types.h` | `PC_HEAP_OFFSET` (0x17A640), `PC_RUNTIME_IMAGE_OFFSET`, `PC_RUNTIME_IMAGE_SIZE` | offsets, not addresses: the bytes the N64 had below the main heap's header, and the runtime images' block in the heap | the heap's accounting is the N64's: its room at every `game.memory`, its alignment modulo 64 KiB, the images' block at the same place ([MEMORY.md](../port/MEMORY.md)) |

Wave 4 ([RECORDS.md](../port/RECORDS.md)) took two rows away: the cartridge's bus addresses (`N64_CART_PHYS`,
`N64_PIF_ROM_PHYS`, `N64_CART_KSEG1`, `N64_TO_PHYS`), now that the game reads its cartridge by ROM offset
([ULTRA.md](../port/ULTRA.md) 11), and a display list's segments (`N64_SEGMENT_*`), now that its lists are command
records with host pointers ([GFX.md](../port/GFX.md) "The command records").
