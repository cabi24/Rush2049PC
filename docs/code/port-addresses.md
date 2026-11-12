# The port's N64 addresses

The PC layer (`port/`) and the tools (`tools/port/*.py`) write N64 addresses as numbers: the window's base,
the images' bounds, game symbols reached by address, cartridge addresses, and the scratch places tests use. This is
their inventory, made for wave 2 of [PCONLY.md](../port/PCONLY.md): what each literal is, and what to write instead.
References by name (`func_8...`, `D_8...` and existing names: about 4,400 in port/, and tools such as
`tools/port/synth_rom.py` (345) and `endian_check.py` (77) that set or check data by symbol name) are not listed:
they follow the name maps, and the rename at merge converts them.

How it was made: every hexadecimal literal of 8 digits in port/ (C, headers, the data rule files) and in
tools/port Python, in the N64's address families (KSEG0 0x80000000-0x80FFFFFF, KSEG1 and the cartridge
0xA0000000-0xBFFFFFFF, physical 0x10000000-0x1FFFFFFF, and the KSEG masks), plus the images' ROM offsets the tools
bake in and the sample base's ROM offset in test_mods.c. Each was resolved against `asm/us/*/symbols.json`,
`symbol_addrs.us.txt`, `undefined_syms_auto.us.txt` and the loader's symbol report (`data_symbols.txt`: names,
sizes), then read in place. Symbol names are the current ones; the maps rename them. Comments that only mention an
address are not occurrences: there are 130 (118 KSEG0, 8 KSEG1 or cartridge, 4 other), the most in
port/include/pc_game_types.h, port/ultra/memory.c, port/data/test_reach.c, port/ultra/gsm.c and
port/data/test_pc_data.c. Physical addresses below 8 MiB (0x00100000, 0x00001000) were counted only where they sit
next to a KSEG0 one.

The sections after "Wave 2: done" are that inventory as it was made, before wave 2 (its line numbers are that tree's);
"Wave 2: done" says what became of each group, and what is left.

## Wave 2: done (2026-11-08)

In port/ (C, headers, the data rule files, its CMake), 8-digit literals in the N64's address families (0x80000000 to
0xBFFFFFFF, and the 0x1FFFFFFF mask), comments included: **663 in 71 files before, 117 in 19 files after**. Of the 117,
96 are the N64's memory map, each named once (the table below says where); the other 21 are not addresses (colours,
hash constants, test patterns, the float-to-unsigned sign bit in pc_prelude.h).

**One home for the N64's memory map: `port/include/pc_n64_map.h`.** Every number of the N64's address space the PC
layer needs is defined there, once, with what it is, its libultra name in a comment, and whether wave 3 removes it:

| Name | Value | What (libultra) | Wave 3 |
|---|---|---|---|
| `N64_KSEG0`, `N64_KSEG1`, `N64_KSEG2` | 0x80000000, 0xA0000000, 0xC0000000 | the MIPS segments (K0BASE, K1BASE, K2BASE) | goes once nothing carries a 32-bit N64 address |
| `N64_PHYS_MASK`, `N64_TO_PHYS(a)` | 0x1FFFFFFF | KSEG0/KSEG1 to physical (K0_TO_PHYS, K1_TO_PHYS) | as above |
| `N64_IS_KSEG0_OR_1(a)`, `N64_IS_KSEG1(a)`, `N64_KSEG1_TO_KSEG0(a)` | | the range tests and the alias fold | as above |
| `N64_SEGMENT_ID_MASK`, `N64_SEGMENT_OFFSET_MASK` | 0x0F000000, 0x00FFFFFF | a segmented address's parts (SEGMENT_NUMBER, SEGMENT_OFFSET) | stay while the renderer reads N64 display lists |
| `N64_RDRAM_SIZE`, `N64_RDRAM_SIZE_NO_PAK`, `N64_RDRAM_END` | 8 MiB, 4 MiB, 0x80800000 | the console's RDRAM | goes with the window |
| `N64_BOOT_PARAMS`, `N64_LOW_MEMORY_END` | 0x80000300, 0x80000400 | libultra's boot parameters (osTvType ...), the boot code's start | goes (ordinary variables) |
| `N64_CART_PHYS`, `N64_PIF_ROM_PHYS`, `N64_CART_KSEG1`, `N64_CART_SPAN` | 0x10000000, 0x1FC00000, 0xB0000000 | the cartridge on the PI (PI_DOM1_ADDR2, PIF_ROM_START, osRomBase) | **stays**: the game reads its cartridge through the PI |
| `N64_OVERLAY_BASE`, `N64_OVERLAY_BLOCK_END` | 0x8038A400, 0x803BB380 | where the overlay loader loads runtime image A or B, and the heap block it reserves | goes (the images become PC memory) |
| `PC_RDRAM_BASE`, `PC_RDRAM_SIZE`, `PC_RDRAM_MAX`, `PC_RCP_SEGMENT_SPAN` | KSEG0, 8 MiB, 256 MiB, 16 MiB | the PC's RDRAM window (moved here from pc_port.h, which includes this header) | goes with the window |

The names are N64_-prefixed rather than libultra's own because the header is also force-included into the game C
(pc_prelude.h includes it), where src/rom/rom_tu.h still defines K0BASE and the KSEG macros its own way. No system
headers, plain constants. The tools take the same numbers from it: tools/port/n64syms.py reads the header
(`N64_MAP`, and `KSEG0`, `KSEG1`, `KSEG2`, `PHYS_MASK`, `RDRAM_END`, `CART_KSEG1`, `OVERLAY_BASE`, ...), and the
commands that run gen_data.py and gen_stubs.py depend on it.

**Symbols by name.** `pc_data_addr_of(tables, name)` (port/data/pc_data.h) gives a symbol's N64 address from the data
loader's tables (own storage, game-C data, arena symbols, functions; 0 if none), so the port asks by name instead of
keeping a copy of the number. Where the port reached a game symbol by its address it now names it:

| Group (inventory section) | Count before | Now |
|---|---:|---|
| KSEG and window arithmetic (W1) | 40 | `N64_IS_KSEG0_OR_1`, `N64_TO_PHYS`, `N64_KSEG1_TO_KSEG0`, `PC_RDRAM_BASE`, the segment masks (pc_data.c, gfx_rsp.c, memory.c, rcpaddr.c, absfix.c, n64mem.c, pi.c, wad.c, check.c, misc.c, pc_prelude.h, statehash.c); memory.c's local `K1_BASE` gone |
| The RDRAM's extent (W2) | 5 | `N64_RDRAM_END`, `PC_RDRAM_MAX` |
| Image, arena and heap bounds (W3) | 4 | heap.c's block from `N64_OVERLAY_BASE`/`N64_OVERLAY_BLOCK_END`; pc_data.c tells a runtime image by its base being at or above the tables' heap start (the loader puts the images in the heap: the same answer for the game's tables and every test's), and low memory by `N64_LOW_MEMORY_END` |
| libultra's boot parameters (W4) | 8 | misc.c writes them through a `BootParams` struct (os.h's layout, a static assert on it) at `N64_BOOT_PARAMS`: the LOW_* enum gone |
| Game data named by its N64 address (W5) | 11 | main.c peeks `pak_charset`, `save_file_name_prefix`, `ghost_file_ext`, `player_file_ext_prefix` by name; mods.c takes the file table's four addresses from its caller (`PcModsTableAt`, which main.c fills by name); statehash.c's TRUNCATED and SCRATCH are names, resolved once |
| Cartridge and PI addresses (W6) | 9 | `N64_CART_PHYS`, `N64_PIF_ROM_PHYS`, `N64_CART_KSEG1`, `N64_CART_SPAN` (ultra.h's PC_ROM_MODS_MAX) |
| Audio capture file (W7) | 5 | CAP_* as `N64_KSEG0 + offset`: the capture file's own address space |
| Tests: the data loader on synthetic layouts (T1) | 152 | named bases and offsets: `TEST_BOOT`, `TEST_GAME`, `TEST_HEAP`, `TEST_OVL` and `BOOT(o)`, `GAME(o)` ... (`N64_KSEG0 + offset`); the report lines built with the address; test_overlay_data's stand-in arena from `N64_OVERLAY_BASE` + image B's offsets; test_pc_data checks `pc_data_addr_of` |
| Check: datacheck (T2) | 53 | `known_widths` is symbol name + offset (each checked against the generated tables: name + offset is the address it replaced); the file table and build-info by name; the inflate tables, which no table names, by name from the symbol file (`pc_symbol_file_addr`, port/include/pc_symbol_file.h); a check that every name resolves runs with and without the ROM |
| Check: the startup self-test (T3) | 27 | `PROBE_K0`, `RELOC_LOW`, `RELOC_HIGH` (KSEG0 + offset); the "at its N64 address" checks against `pc_data_tables.heap_lo`, `N64_OVERLAY_BASE`, `N64_BOOT_PARAMS` and `pc_data_addr_of` |
| Tests: assets and game C (T4) | 118 | the game data tests place at its N64 address: `TEST_ABS(name)`, `TEST_AT(name)`, `TEST_N64(name)` from one table in port/assets/tests/support.h (the ABS() macros of six files gone); scratch places `TEST_SCRATCH + n` (`PC_RDRAM_BASE` + 3 MiB); assetcheck's image bases and file table by name from the symbol file; test_mods' cartridge by its own named layout |
| Tests: the renderer and the audio interpreter (T5) | 29 | `N64_TO_PHYS`, `PC_RDRAM_BASE + n MiB` |
| Tests: the GSM kernels (T6) | 8 | test_gsm.c takes each kernel's and table's address from its label in asm/us/boot_data/kernels.s (gsm_rpe_decode, gsm_small_xmaxc_table, ...) |
| Tools (Y1-Y3) | 74 | n64syms.py's CODE, REGIONS and BSS_REGIONS derived from IMAGES and ARENAS (the same values, checked), the overlay rows from `OVERLAY_BASE`; gen_stubs.py, hash_compare.py, heap_check.py, nofixed_audit.py, names_check.py from n64syms; synth_rom.py's descriptors from `steady_leg_duration` + offset (since wave 3 step 2 `car_descriptors`, their own name); alias_check.py's note by name. gen_data.py untouched (another agent's), its literals left (HEAP_LO, WINDOW_LO/HI, WINDOW_SECTION, KSEG0 in comments and a range test) |

Comments: the addresses in port/'s comments became names (`game_bss_end`, `overlay_base`, `audio_ucode_text`,
`track1Parts`, the kernels' names, PC_RDRAM_BASE ...), and the labels that still said an old name now say the new one
(datacheck's labels, test_menu's `cheat_menu_unlocked`, test_gsm's table names).

### Left in port/, and where each is named

| Where | What | Why it stays |
|---|---|---|
| port/include/pc_n64_map.h | the N64's memory map (the table above) | the one place; wave 3 removes all but the cartridge's and the segment masks |
| port/assets/tests/support.h | (gone in [NATIVE.md](../port/NATIVE.md) step 11) `TEST_N64_*`: game symbols' N64 addresses that tests placed in their window (`TEST_ABS`) | the symbols are ordinary definitions in each test, the tests' memory a host block (`test_memory`) |
| port/data/reserved.txt | `pc_matrix_spill 0x8014A250`: PC data placed in the window where the decoupled model records were | wave 3 (the spill becomes PC data); the rule is the one place it is written |
| port/data/asm_functions.txt | 43 rows `name address source` (and 5 addresses in their notes): the assembly routines the loader's function table lists | read by gen_data.py (another agent's area), which keys rows by name and address; the addresses could come from the symbol tables by name once gen_data reads them so |
| port/data/decoupled.txt | 20 addresses in comments: where each decoupled record was on the N64 | documentation of the N64 layout the rules undo, in gen_data's input; left with gen_data |
| port/data/test_pc_data_decls.c | the fixtures named by their address (D_80010020 ...) | they test the loader's address parsing (as the brief says) |
| port/assets/tests/test_musyx.c | `NODE_TOKEN(i)`, 0x80050A50 + 16 i | the token src/rom/sound/voices.c keeps for a voice node (its N64 address); the game C defines the number, the test mirrors it (see below) |
| port/main.c, test_mods.c, test_musyx.c, test_casts.c | MusyX's sample base, ROM offset 0x39B40 (`MODS_SAMPLE_BASE`, `SAMPLE_BASE`) | a ROM offset, named; the game C's `sample_bank_rom` |

Names still spelled as addresses in port/ (224, from 251): the game symbols no map named, which tests define or name
because the game C uses them (D_80114C58, D_8015F740, D_80120E74 ...: since named by tools/port/names/wave2.tsv,
RaceMultiBlit, gMatrices, drone_sched), the N64 names in decoupled.txt and asm_functions.txt (the loader's keys), and
the fixtures above. They go when the maps name them in src/.

### For src/ (the lead)

- `src/blob/groups/heap_release/overlay_loader.c` passes `N64PTR(u8 *, 0x8038A400)` and `0x803BB380` to
  load_overlay: pc_prelude.h now gives the game C `N64_OVERLAY_BASE` and `N64_OVERLAY_BLOCK_END`, the same numbers.
- `src/rom/sound/voices.c` writes 0x80050A50 three times (the node token base); a named constant there (or in a shared
  header) would let test_musyx.c use it.
- `src/rom/rom_tu.h` defines K0BASE, IS_KSEG0, IS_KSEG1, K0_TO_PHYS, K1_TO_PHYS its own way; pc_n64_map.h's names
  could replace them.
- `src/blob/groups/front_end/group.c:3782` names 0x8038A400 in a comment.

## Totals

620 literal occurrences: 544 are N64 addresses or address arithmetic (83 in production code, 387 in tests and checks, 74 in tools), 76 are not addresses.

| Group | Count | Files | Replacement |
|---|---:|---|---|
| [KSEG and window arithmetic](#kseg-and-window-arithmetic) | 40 | 12 files | libultra address macros (K0BASE, K1BASE, K2BASE, K0_TO_PHYS, IS_KSEG0, ...) in one shared header; PC_RDRAM_BASE where it means the window. Stays while 32-bit N64 addresses exist |
| [The RDRAM's extent](#the-rdrams-extent) | 5 | `data/pc_data.c`, `gfx/gfx_rsp.c`, `include/pc_port.h`, `include/pc_prelude.h` | PC_RDRAM_BASE + PC_RDRAM_SIZE, PC_RDRAM_MAX. Goes with the window |
| [Image, arena and heap bounds](#image-arena-and-heap-bounds) | 4 | `data/pc_data.c`, `ultra/heap.c` | one shared definition of the overlay load address and block end (the game's loader passes them as literals); the loader's image table for the rest. Goes in wave 3 |
| [libultra's boot parameters in low memory](#libultras-boot-parameters-in-low-memory) | 8 | `ultra/misc.c` | the variables by name (`osTvType = 1`); wave 3: ordinary variables, the LOW_* enum goes |
| [Game data named by its N64 address](#game-data-named-by-its-n64-address) | 11 | `main.c`, `mods/mods.c`, `ultra/statehash.c` | the symbol's name: a loader lookup by name (`pc_data_peek` by name) or `&name` |
| [Cartridge and PI addresses](#cartridge-and-pi-addresses) | 9 | `ultra/memory.c`, `ultra/misc.c`, `ultra/pi.c`, `ultra/ultra.h` | PI_DOM1_ADDR2, PIF_ROM_START, PHYS_TO_K1 (libultra). Stays: the cartridge is still read |
| [Addresses of a file format kept in N64 form](#addresses-of-a-file-format-kept-in-n64-form) | 5 | `audio/musyx.c` | keep, named: an outside file format. Survives wave 3 |
| [PC data placed at an N64 address](#pc-data-placed-at-an-n64-address) | 1 | `data/reserved.txt` | goes in wave 3 (the spill becomes ordinary PC data) |
| [Tests: the data loader on synthetic layouts](#tests-the-data-loader-on-synthetic-layouts) | 152 | `data/test_overlay_data.c`, `data/test_pc_data.c`, `data/test_reach.c` | named synthetic bases + offsets. Survives while the loader resolves N64 addresses |
| [Check: datacheck on the real tables](#check-datacheck-on-the-real-tables) | 53 | `data/datacheck.c` | symbol names looked up in the loader's table. Survives with the loader; the window checks go |
| [Check: the startup self-test](#check-the-startup-self-test) | 27 | `ultra/selftest.c` | by name; the "at its N64 address" checks go in wave 3, the window checks with the window |
| [Tests: assets and game C](#tests-assets-and-game-c) | 118 | 18 files | names for game symbols (the ABS lines go in wave 3); named scratch constants |
| [Tests: the renderer and the audio interpreter](#tests-the-renderer-and-the-audio-interpreter) | 29 | `audio/tests/stubs.c`, `audio/tests/test_window.c`, `gfx/tests/gfx_test.c`, `gfx/tests/interp_test.c`, `gfx/tests/tex_test.c` | named scratch constants, PC_RDRAM_BASE + n MiB. Stay while the renderer reads N64 addresses |
| [Tests: the boot segment's GSM kernels](#tests-the-boot-segments-gsm-kernels) | 8 | `ultra/tests/test_gsm.c` | the kernels' names (rom.tsv) from the loader's table |
| [Tools: the image layout (n64syms.py)](#tools-the-image-layout-n64symspy) | 48 | `n64syms.py` | one image/arena table read from the symbol tables; the overlay rows go in wave 3 |
| [Tools: the generators (gen_data.py, gen_stubs.py)](#tools-the-generators-gen_datapy-gen_stubspy) | 11 | `gen_data.py`, `gen_stubs.py` | from n64syms (one place) |
| [Tools: checks and audits](#tools-checks-and-audits) | 15 | `alias_check.py`, `hash_compare.py`, `heap_check.py`, `nofixed_audit.py`, `synth_rom.py` | K0BASE and names from n64syms; base + offset |
| [Not addresses](#not-addresses) | 76 | 41 files | nothing |

## What wave 2 writes

- **libultra's address macros, in one header the game C and port/ share** (R4300.h names): `K0BASE`, `K1BASE`,
  `K2BASE`, `K0_TO_PHYS`, `K1_TO_PHYS`, `PHYS_TO_K0`, `PHYS_TO_K1`, `IS_KSEG0`, `IS_KSEG1`; rcp.h's `PI_DOM1_ADDR2` and
  `PIF_ROM_START`; `SEGMENT_OFFSET` and `SEGMENT_NUMBER` (include/macros.h has them). Today the game C defines some of
  them in four places (src/rom/rom_tu.h, src/rom/lib_10a00.c, lib_10b60.c, lib_e9a0.c) and port/ none.
- **The window's own constants stay**: `PC_RDRAM_BASE`, `PC_RDRAM_SIZE`, `PC_RDRAM_MAX`, `PC_RCP_SEGMENT_SPAN`
  (port/include/pc_port.h). Use them where a literal means the window rather than KSEG0.
- **One definition of the overlay block**: the game's overlay loader passes the load address and the block's end
  to load_overlay as literals, and port/ultra/heap.c, test_heap.c and docs/port/DATA.md copy them. Proposed:
  `OVERLAY_LOAD_ADDR`, `OVERLAY_BLOCK_END` (no collision in the tree), in a header both include.
- **Symbols by name**: the loader's tables hold every symbol's name and N64 address, so port code that needs one
  asks by name (a `pc_data_addr_of(tables, name)` lookup, or `pc_data_peek` taking a name) instead of carrying a
  copy of the address; datacheck's labels already hold the names.
- **Tests**: named bases for synthetic layouts and scratch places (`PC_RDRAM_BASE + offset`), names for game
  symbols.
- **Tools**: one memory map (n64syms.py) read from the symbol tables, and every other script takes its numbers
  from it.

What wave 3 removes: the game symbols at their N64 addresses (W5, the ABS lines of T4, the "at its N64 address"
checks of T3), the overlay block (W3, the overlay rows of Y1), reserved.txt's rule (W8). What stays while anything
carries a 32-bit N64 address (display lists, converted files, the audio task records): the window and its
arithmetic (W1, W2), the cartridge's PI addresses (W6, which outlive the window), and the capture format (W7).

## KSEG and window arithmetic

The window: port/ultra maps a host range for the N64's RDRAM, at host 0x80000000 where the OS allows it, so a
32-bit KSEG0 address (0x80000000 | physical) is a host address. These literals are the KSEG0 and KSEG1 bases, the
0x1FFFFFFF mask that turns either into a physical address, the KSEG0..KSEG2 range test that tells an RDRAM address
from anything else, and the 24-bit offset of a segmented display-list address. They are not the game's data but the
N64's address space itself, which the renderer (display lists), the audio interpreter (task records), the converted
files (relocated WADs) and the loader still speak.

**Wave 2:** one header, shared by the game C and port/, with libultra's own names for these (R4300.h): `K0BASE`,
`K1BASE`, `K2BASE`, `K0_TO_PHYS`, `K1_TO_PHYS`, `PHYS_TO_K0`, `PHYS_TO_K1`, `IS_KSEG0`, `IS_KSEG1`. The game C already
spells some of them (src/rom/rom_tu.h: `K0BASE`, `IS_KSEG0`, `IS_KSEG1`, `K0_TO_PHYS`, `K1_TO_PHYS`; two src/rom files
define `PHYS_TO_K1`), and include/macros.h has `SEGMENT_OFFSET` and `SEGMENT_NUMBER`. Where 0x80000000 means "the
window's base" rather than "KSEG0", `PC_RDRAM_BASE` (port/include/pc_port.h) is already the name. memory.c's local
`K1_BASE` becomes `K1BASE`. **Wave 3:** the window and these go together, once nothing carries a 32-bit N64 address;
the GBI's segmented addresses stay as long as the renderer interprets N64 display lists.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/assets/check.c:128` | `0x80000000u` | KSEG0 bit: a WAD's table-of-contents word already relocated to an N64 address | IS_KSEG0 (libultra); stays while converted files carry N64 addresses |
| `port/assets/wad.c:213` | `0x80000000u` | KSEG0 bit: a WAD's table-of-contents word already relocated to an N64 address | IS_KSEG0 (libultra); stays while converted files carry N64 addresses |
| `port/data/pc_data.c:217` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/data/pc_data.c:232` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/data/pc_data.c:234` | `0x1FFFFFFFu` | KSEG mask: an RDRAM address to a window offset | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/data/pc_data.c:337` | `0xA0000000u` | KSEG1 range: an uncached address folded to KSEG0 | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound and K1_TO_PHYS |
| `port/data/pc_data.c:339` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/data/pc_data.c:535` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/data/pc_data.c:695` | `0xA0000000u` | KSEG1 range: an uncached address folded to KSEG0 | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound and K1_TO_PHYS |
| `port/data/pc_data.c:714` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/data/pc_data.c:869` | `0x80000000u` | KSEG0 base / range (the window, a value that looks like an address) | K0BASE (libultra; PC_RDRAM_BASE where it means the window); IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound |
| `port/gfx/gfx_rsp.c:46` | `0x1FFFFFFFu` | KSEG mask: a physical RDRAM address | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/gfx/gfx_rsp.c:67` | `0x80000000u` | KSEG0 test: an RDRAM address versus a segmented one | IS_KSEG0 (libultra) |
| `port/gfx/gfx_rsp.c:78` | `0x80000000u` | KSEG0 test: an RDRAM address versus a segmented one | IS_KSEG0 (libultra) |
| `port/gfx/gfx_rsp.c:89` | `0x00FFFFFF` | a segmented address's 24-bit offset | SEGMENT_OFFSET macro |
| `port/gfx/gfx_rsp.c:90` | `0x00FFFFFF` | a segmented address's 24-bit offset | SEGMENT_OFFSET macro |
| `port/include/pc_port.h:31` | `0x80000000u` | PC_RDRAM_BASE: the window's N64 base (KSEG0) | keep: the one definition (the named constant the rest should use) |
| `port/include/pc_prelude.h:75` | `0x1FFFFFFFu` | the KSEG mask in pc_n64p (an N64 address to a window pointer) | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant; goes with the window (wave 3) |
| `port/ultra/absfix.c:75` | `0x80000000u` | KSEG0..KSEG1 range: relocations whose addend is an N64 address | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound; goes with the window |
| `port/ultra/memory.c:55` | `0xA0000000u` | KSEG1 base (K1_BASE, the uncached alias) | K1BASE (libultra) (K1_BASE is the local spelling) |
| `port/ultra/memory.c:127` | `0xA0000000` | KSEG1 base (K1_BASE, the uncached alias) in a message | K1BASE (libultra) (K1_BASE is the local spelling) |
| `port/ultra/memory.c:257` | `0x80000000`, `0xBFFFFFFF` | KSEG0..KSEG1 range in a message; `0xBFFFFFFF`: end of KSEG1 in a message | K0BASE / K2BASE - 1 in the format |
| `port/ultra/memory.c:288` | `0x1FFFFFFFu` | KSEG mask: K0/K1 to physical | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/ultra/memory.c:344` | `0xA0000000` | KSEG1 base (K1_BASE, the uncached alias) in a message | K1BASE (libultra) (K1_BASE is the local spelling) |
| `port/ultra/memory.c:385` | `0x80000000u` | KSEG0 base / range (pc_n64_ptr, pc_n64_addr) | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound; K0BASE (libultra; PC_RDRAM_BASE where it means the window) |
| `port/ultra/memory.c:386` | `0x1FFFFFFFu` | KSEG mask: K0/K1 to physical | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/ultra/memory.c:398` | `0x80000000u` | KSEG0 base / range (pc_n64_ptr, pc_n64_addr) | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound; K0BASE (libultra; PC_RDRAM_BASE where it means the window) |
| `port/ultra/memory.c:400` | `0x80000000u` | KSEG0 base / range (pc_n64_ptr, pc_n64_addr) | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound; K0BASE (libultra; PC_RDRAM_BASE where it means the window) |
| `port/ultra/memory.c:415` | `0x1FFFFFFFu` | KSEG mask: K0/K1 to physical | K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/ultra/misc.c:101` | `0x1FFFFFFFu` | KSEG mask in __osTLBLookup | K0_TO_PHYS (libultra) |
| `port/ultra/n64mem.c:17` | `0x80000000u` | KSEG0 base: the default pc_window_base, and pc_n64_identity | PC_RDRAM_BASE; goes with the window |
| `port/ultra/n64mem.c:31` | `0x80000000u` | KSEG0 base: the default pc_window_base, and pc_n64_identity | PC_RDRAM_BASE; goes with the window |
| `port/ultra/rcpaddr.c:21` | `0x80000000u`, `0x1FFFFFFFu` | KSEG0..KSEG1 range test; `0x1FFFFFFFu`: KSEG mask: K0/K1 to physical | IS_KSEG0 / IS_KSEG1 (libultra), K2BASE for the 0xC0000000 bound; `0x1FFFFFFFu`: K0_TO_PHYS / K1_TO_PHYS (libultra), or a PC_PHYS_MASK constant |
| `port/ultra/rcpaddr.c:38` | `0x00FFFFFFu` | a segmented address's 24-bit offset | a SEGMENT_OFFSET macro (with SEGMENT_NUMBER for bits 24-27) |
| `port/ultra/statehash.c:643` | `0x80000000u` | KSEG0 base: a window offset to its N64 address | PC_RDRAM_BASE (or pc_n64_addr) |
| `port/ultra/statehash.c:771` | `0x80000000u` | KSEG0 base: a window offset to its N64 address | PC_RDRAM_BASE (or pc_n64_addr) |
| `port/ultra/statehash.c:929` | `0x80000000u` | KSEG0 base: a window offset to its N64 address | PC_RDRAM_BASE (or pc_n64_addr) |

## The RDRAM's extent

The N64's 8 MiB (0x80800000 as the end of KSEG0 RDRAM) and the window's largest size (256 MiB, 0x10000000).
The loader treats a word in [0x80000000, 0x80800000) as an address because the cartridge's images never point past
8 MiB; gfx_rsp tells a physical address past 256 MiB from a segmented one.

**Wave 2:** `PC_RDRAM_BASE + PC_RDRAM_SIZE`, `PC_RDRAM_MAX` (both exist). **Wave 3:** they go with the window.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/data/pc_data.c:339` | `0x80800000u` | the 8 MiB RDRAM's end: words in [K0BASE, +8 MiB) are taken for addresses | PC_RDRAM_BASE + PC_RDRAM_SIZE |
| `port/data/pc_data.c:535` | `0x80800000u` | the 8 MiB RDRAM's end: words in [K0BASE, +8 MiB) are taken for addresses | PC_RDRAM_BASE + PC_RDRAM_SIZE |
| `port/gfx/gfx_rsp.c:85` | `0x10000000u` | past 256 MiB: bits 28-30, which no segment has | PC_RDRAM_MAX |
| `port/include/pc_port.h:38` | `0x10000000u` | PC_RDRAM_MAX: the largest window (256 MiB) | keep: the one definition |
| `port/include/pc_prelude.h:118` | `0x10000000u` | the window's largest span (PC_RDRAM_MAX) in the cast check | PC_RDRAM_MAX |

## Image, arena and heap bounds

Where the images sit. `IMAGES_LO`/`IMAGES_HI` copy the two literals the game's own overlay loader passes to
load_overlay (src/blob/groups/*/overlay_loader.c: the runtime images' load address and the end of the heap block it
reserves for them, past image A's BSS); pc_data.c tells an overlay image by a base at or above 0x80300000, and low
memory by an address below the boot code's first word.

**Wave 2:** one definition of the load address and block end, shared by overlay_loader.c, heap.c and the tests
(proposed `OVERLAY_LOAD_ADDR`, `OVERLAY_BLOCK_END`; the overlay base symbol's name from ovl.tsv where a symbol is
meant); an explicit overlay flag in the loader's image table instead of the 0x80300000 threshold; a named bound for
low memory. **Wave 3:** the overlays become PC memory and the heap's reservation, the threshold and the bounds go.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/data/pc_data.c:145` | `0x80300000u` | a threshold telling an overlay image by its base (anything at or above 0x80300000) | a flag in the image table (PC_IMG_OVERLAY) or a compare with the overlay base symbol |
| `port/data/pc_data.c:258` | `0x80000400u` | the end of low memory (`entrypoint`, the first boot code address) | a named bound (N64_LOW_MEMORY_END) or the loader table's boot image base |
| `port/ultra/heap.c:23` | `0x8038A400u` | IMAGES_LO: the runtime images' load address (`overlay_base`, the base of both overlays) | the overlay base symbol's name (from the overlay loader), not a copy; wave 3: overlays as PC memory, the reservation goes |
| `port/ultra/heap.c:24` | `0x803BB380u` | IMAGES_HI: the end of the heap block the overlay loader reserves for the images (past image A's BSS end `start_overlay_bss_end`) | a constant shared with the loader (the reservation's size is game-defined in overlay_loader.c); wave 3: goes |

## libultra's boot parameters in low memory

libultra's boot parameters live at fixed low-memory addresses (osTvType at 0x80000300 through osAppNMIBuffer at
0x8000031C); the IPL fills them on the N64, pc_os_initialize (port/ultra/misc.c) through an enum of their addresses
here. The game reads them by name.

**Wave 2:** write them by name (`osTvType = 1;`), as the game reads them; the `LOW_*` enum goes. **Wave 3:** ordinary
variables.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/ultra/misc.c:51` | `0x80000300`, `0x80000304`, `0x80000308`, `0x8000030C` | `osTvType` (enum LOW_*, written by pc_os_initialize); `0x80000304`: `osRomType` (enum LOW_*, written by pc_os_initialize); `0x80000308`: `osRomBase` (enum LOW_*, written by pc_os_initialize); `0x8000030C`: `osResetType` (enum LOW_*, written by pc_os_initialize) | the variable by name (`osTvType = ...`); wave 3: an ordinary variable, the enum goes; `0x80000304`: the variable by name (`osRomType = ...`); wave 3: an ordinary variable, the enum goes; `0x80000308`: the variable by name (`osRomBase = ...`); wave 3: an ordinary variable, the enum goes; `0x8000030C`: the variable by name (`osResetType = ...`); wave 3: an ordinary variable, the enum goes |
| `port/ultra/misc.c:52` | `0x80000310`, `0x80000314`, `0x80000318`, `0x8000031C` | `osCicId` (enum LOW_*, written by pc_os_initialize); `0x80000314`: `osVersion` (enum LOW_*, written by pc_os_initialize); `0x80000318`: `osMemSize` (enum LOW_*, written by pc_os_initialize); `0x8000031C`: `osAppNMIBuffer` (enum LOW_*, written by pc_os_initialize) | the variable by name (`osCicId = ...`); wave 3: an ordinary variable, the enum goes; `0x80000314`: the variable by name (`osVersion = ...`); wave 3: an ordinary variable, the enum goes; `0x80000318`: the variable by name (`osMemSize = ...`); wave 3: an ordinary variable, the enum goes; `0x8000031C`: the variable by name (`osAppNMIBuffer = ...`); wave 3: an ordinary variable, the enum goes |

## Game data named by its N64 address

Production code that reaches a game symbol by its N64 address: main.c reads four of the game image's data
from the cartridge image before the data loads (the imported ghosts' names: the pak's character table, a prefix, a
file extension, a profile byte); mods.c keeps the file table's four symbols; statehash.c leaves two libultra SI
scratch buffers and one word that holds a window pointer's low half out of the state hash.

**Wave 2:** by name. The loader's tables know every symbol's name and address, so a lookup by name
(`pc_data_peek` taking a name, or a `pc_data_addr_of(tables, name)`: no collision in the tree) replaces each
literal; statehash's tables take `&symbol, sizeof symbol`. **Wave 3:** `&symbol` directly; statehash's truncated-word
entry goes once that word holds a host pointer.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/main.c:745` | `0x8011EAECu` | `pak_charset` (pc_data_peek reads it from the cartridge image before the data loads) | pc_data_peek by the symbol's name (the loader's table has it): `pak_charset` |
| `port/main.c:746` | `0x801211BCu` | `save_file_name_prefix` (pc_data_peek reads it from the cartridge image before the data loads) | pc_data_peek by the symbol's name (the loader's table has it): `save_file_name_prefix` |
| `port/main.c:747` | `0x801211F4u` | `ghost_file_ext` (pc_data_peek reads it from the cartridge image before the data loads) | pc_data_peek by the symbol's name (the loader's table has it): `ghost_file_ext` |
| `port/main.c:748` | `0x801211C8u` | `player_file_ext_prefix` (pc_data_peek reads it from the cartridge image before the data loads) | pc_data_peek by the symbol's name (the loader's table has it): `player_file_ext_prefix` |
| `port/mods/mods.c:27` | `0x8002E580u` | `gWaveSizes` (the file table: gWaveSizes, the files' sizes) | the symbol by name through the loader (pc_data_peek by name), or `&name` once wave 3 owns the data |
| `port/mods/mods.c:28` | `0x8002E860u` | `gLargestTextWave` (the file table: the first text bank) | the symbol by name through the loader (pc_data_peek by name), or `&name` once wave 3 owns the data |
| `port/mods/mods.c:29` | `0x8011B5BCu` | `wave_rom_offsets` (the file table: the files' ROM offsets) | the symbol by name through the loader (pc_data_peek by name), or `&name` once wave 3 owns the data |
| `port/mods/mods.c:30` | `0x80123564u` | `wave_storage_kinds` (the file table: the files' storage kinds) | the symbol by name through the loader (pc_data_peek by name), or `&name` once wave 3 owns the data |
| `port/ultra/statehash.c:411` | `0x80156BCCu` | `last_frame_text_dl_end` (a word holding a window pointer's low half: left out of the hash) | the symbol by name (`&last_frame_text_dl_end`); wave 3: goes when the word holds a host pointer |
| `port/ultra/statehash.c:422` | `0x80037B30u` | `__osMotorPifBuf` (libultra SI scratch: hashed as zeros) | the symbol by name (`&__osMotorPifBuf`, with sizeof) |
| `port/ultra/statehash.c:423` | `0x80037F60u` | `__osPfsPifRam` (libultra SI scratch: hashed as zeros) | the symbol by name (`&__osPfsPifRam`, with sizeof) |

## Cartridge and PI addresses

Cartridge addresses: osRomBase's value (the cartridge's KSEG1 address), pi.c's test that a PI device address
lies in domain 1 (the cartridge ROM, physical 0x10000000 up to the PIF at 0x1FC00000), memory.c's fault note, and
the PC's own region for mods' files as a ROM offset.

**Wave 2:** libultra's names (rcp.h): `PI_DOM1_ADDR2`, `PIF_ROM_START`, and `PHYS_TO_K1(PI_DOM1_ADDR2)` for 0xB0000000;
`PC_ROM_MODS_BASE` stays as defined. **Wave 3:** these stay: the game still reads its cartridge through the PI.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/ultra/memory.c:277` | `0xB0000000u` | KSEG1 address of PI domain 1 (the cartridge) in the fault note | PHYS_TO_K1(PI_DOM1_ADDR2) (libultra), or a PC_CART_K1 constant |
| `port/ultra/memory.c:289` | `0xB0000000u` | KSEG1 address of PI domain 1 (the cartridge) in the fault note | PHYS_TO_K1(PI_DOM1_ADDR2) (libultra), or a PC_CART_K1 constant |
| `port/ultra/misc.c:72` | `0xB0000000u` | osRomBase's value: the cartridge's KSEG1 address | PHYS_TO_K1(PI_DOM1_ADDR2) (libultra) |
| `port/ultra/pi.c:132` | `0x1FFFFFFFu` | KSEG mask: a PI device address to physical | K1_TO_PHYS (libultra) |
| `port/ultra/pi.c:133` | `0x10000000u`, `0x1FC00000u` | PI domain 1 (the cartridge ROM) physical start; `0x1FC00000u`: the PIF ROM: the end of the cartridge range | PI_DOM1_ADDR2 (libultra); `0x1FC00000u`: PIF_ROM_START (libultra) |
| `port/ultra/ultra.h:171` | `0x04000000u` | PC_ROM_MODS_BASE: the mods' region of the cartridge, as a ROM offset (PC-defined) | keep: the one definition |
| `port/ultra/ultra.h:172` | `0x0FC00000u` | the end of the cartridge's PI range, as a ROM offset | PIF_ROM_START - PI_DOM1_ADDR2 (libultra names) |

## Addresses of a file format kept in N64 form

The audio task capture (RUSH2049_AUDIO_CAPTURE) writes each task as a self-contained N64-form image for replay
on another implementation of the microcode; these five name places in that file's own address space, not the
game's.

**Wave 2 and 3:** keep, named as they are (CAP_STATE, CAP_SFD, CAP_OUT, CAP_ARENA, CAP_ARENA_END).

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/audio/musyx.c:702` | `0x80010000u` | CAP_STATE: a place in the audio capture file's own N64-form address space (not the game's data) | keep, named (it defines an outside file format; survives wave 3) |
| `port/audio/musyx.c:703` | `0x80100000u` | CAP_SFD: a place in the audio capture file's own N64-form address space (not the game's data) | keep, named (it defines an outside file format; survives wave 3) |
| `port/audio/musyx.c:704` | `0x80180000u` | CAP_OUT: a place in the audio capture file's own N64-form address space (not the game's data) | keep, named (it defines an outside file format; survives wave 3) |
| `port/audio/musyx.c:705` | `0x80200000u` | CAP_ARENA: a place in the audio capture file's own N64-form address space (not the game's data) | keep, named (it defines an outside file format; survives wave 3) |
| `port/audio/musyx.c:706` | `0x80400000u` | CAP_ARENA_END: a place in the audio capture file's own N64-form address space (not the game's data) | keep, named (it defines an outside file format; survives wave 3) |

## PC data placed at an N64 address

port/data/reserved.txt places PC data that must still sit in the window, where a decoupled item freed room
(docs/port/DATA.md 3). Its one rule puts GetMatrix's spill at the old start of the model records.

**Wave 3:** goes; the spill becomes ordinary PC data. Until then the rule's address is the one place it is written.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/data/reserved.txt:14` | `0x8014A250` | pc_matrix_spill placed at the old start of the model records (`D_8014A250`, decoupled as pc_models) | nothing once wave 3 takes the window away: the spill becomes ordinary PC data; until then the rule's address is the one place it is written |

## Tests: the data loader on synthetic layouts

port/data/test_pc_data.c, test_reach.c and test_overlay_data.c test the data loader (pc_data.c) on tables of
their own: fake images based at 0x80001000 (boot), 0x80010000 (game), 0x80300000 (both runtime images), a heap at
0x80020000, and words that point into them. None of these is a game symbol (the resolver's nearest names are
coincidences). test_overlay_data.c uses image B's real bounds for its stand-in.

**Wave 2:** named test bases (`TEST_BOOT`, `TEST_GAME`, `TEST_OVL`, `TEST_HEAP`, plus offsets), the real bounds by
name. **Wave 3:** the tests survive as long as the loader translates N64 addresses (PCONLY.md 3: the symbol tables
keep them while the data comes from the cartridge); the window-mapping part goes with the window.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/data/test_overlay_data.c:35` | `0x80399118u` | `gCurWeaponType`: a stand-in arena for the weapons' state, a slice of image B's BSS (its real arena is 0x80394F70-0x8039B440) | the two symbols by name (first, and last + its size); goes with overlays as PC memory (wave 3) |
| `port/data/test_overlay_data.c:36` | `0x80399A70u` | `DamagePolys`: a stand-in arena for the weapons' state, a slice of image B's BSS (its real arena is 0x80394F70-0x8039B440) | the two symbols by name (first, and last + its size); goes with overlays as PC memory (wave 3) |
| `port/data/test_overlay_data.c:68` | `0x8038A400u` | the runtime images' base `overlay_base` (both images in the test tables) | the overlay base symbol by name |
| `port/data/test_overlay_data.c:71` | `0x8038A400` | the runtime images' base `overlay_base` (both images in the test tables) | the overlay base symbol by name |
| `port/data/test_overlay_data.c:72` | `0x8038A400` | the runtime images' base `overlay_base` (both images in the test tables) | the overlay base symbol by name |
| `port/data/test_overlay_data.c:77` | `0x8017A640` | `game_bss_end` / `game_bss_end`: the heap's start in the test tables | game_bss_end by name, or a test constant |
| `port/data/test_pc_data.c:64` | `0x80001000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:65` | `0x80010000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:66` | `0x80300000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:67` | `0x80300000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:75` | `0x80001080`, `0x80001100`, `0x80001140` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:76` | `0x80010100`, `0x80010200`, `0x80010300` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:77` | `0x80300080`, `0x80300100`, `0x80300180` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:78` | `0x80300040`, `0x80300060`, `0x80300080` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:120` | `0x80010100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:121` | `0x80010120` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:122` | `0x80010160` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:123` | `0x80010170` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:124` | `0x80300040` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:125` | `0x80300080` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:126` | `0x803000F0` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:130` | `0x80010000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:131` | `0x80010010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:132` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:133` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:136` | `0x80001080`, `0x80010180`, `0x80300090` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:143` | `0x800101A0` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:144` | `0x800101B0` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:150` | `0x80020000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:167` | `0x80001084` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:169` | `0x80010000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:172` | `0x80010104` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:173` | `0x80010110` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:174` | `0x8001F000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:176` | `0x80010010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:177` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:178` | `0x8001016C` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:179` | `0x80020040` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:180` | `0x80300040` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:181` | `0x80300120` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:182` | `0x803FF000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:184` | `0x80001098` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:185` | `0x80010190` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:186` | `0x80010170` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:189` | `0x80001084` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:191` | `0x80010100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:193` | `0x80010100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:194` | `0x80001088` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:203` | `0x80010000u` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:206` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:209` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:220` | `0x80010000u` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:278` | `0x80010020`, `0x8001002C` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:280` | `0x8001002C` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:283` | `0x80010030`, `0x80010032` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:284` | `0x80010034`, `0x80010038` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:286` | `0x80010040`, `0x80010054` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:288` | `0x80010054`, `0x80010055` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:303` | `0x8001F000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:308` | `0x80020040` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:311` | `0x803FF000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:327` | `0x80001084`, `0x80300120` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:369` | `0x80010108` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:370` | `0x80300010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:371` | `0x80001088` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:372` | `0x800101A0` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:382` | `0x80010100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:383` | `0x80001088` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:384` | `0x8001F000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:385` | `0x80010010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:386` | `0x80010190` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:387` | `0x80010170` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:388` | `0x80001098` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_pc_data.c:402` | `0x80000000u` | the window's fixed host address (the test maps its own window) | PC_RDRAM_BASE |
| `port/data/test_pc_data.c:407` | `0x80000000` | the window's fixed host address (the test maps its own window) | PC_RDRAM_BASE |
| `port/data/test_reach.c:36` | `0x80001000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:37` | `0x80001000`, `0x80001200` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:53` | `0x80001200` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:54` | `0x80001210` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:55` | `0x80001220` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:62` | `0x80001230` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:67` | `0x80020000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:69` | `0x80001000u` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:74` | `0x80001000`, `0x80001002`, `0x80001004` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:75` | `0x80001008`, `0x80001100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:76` | `0x8000100C`, `0x8000100E`, `0x80001010` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:77` | `0x80001014`, `0x80001228` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:78` | `0x80001018`, `0x8000101A` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:80` | `0x80001040`, `0x80001042`, `0x80001044` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:81` | `0x80001048`, `0x80001100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:83` | `0x80001100`, `0x80001180`, `0x80001104` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:84` | `0x80001108`, `0x80001188`, `0x8000110C` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:85` | `0x80001180`, `0x80001188` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:87` | `0x80001140`, `0x80001142`, `0x80001144` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:88` | `0x80001148` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:89` | `0x80001150`, `0x80001152` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:91` | `0x80001200`, `0x80001000`, `0x80001204`, `0x80001040`, `0x80001208` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:92` | `0x80001210`, `0x80001140`, `0x80001214`, `0x80001150` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:93` | `0x80001220`, `0x80001180`, `0x80001224` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:94` | `0x80001228`, `0x80001188`, `0x8000122C` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:95` | `0x80001230`, `0x80001232` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:140` | `0x80001000` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:141` | `0x80001100` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |
| `port/data/test_reach.c:142` | `0x80001150` | an address in the synthetic layout (fake images at 0x80001000, 0x80010000, 0x80300000, heap 0x80020000) | named test bases (TEST_BOOT, TEST_GAME, TEST_OVL, TEST_HEAP) + offsets; survives while the loader resolves N64 addresses |

## Check: datacheck on the real tables

datacheck (`--synthetic`, or the player's ROM) checks the real tables: the colour unions loaded as bytes, the
viewport table's extent, the flags after it, the cheat codes, the car descriptors and stat factors, extents of
tables only pointers reach, the inflate tables, the file table, and the function both runtime images have at
their base. It names each by its N64 address, with the symbol's name in the label string.

**Wave 2:** the label's name, looked up in the loader's table, replaces each address; offsets inside a symbol
become `name + offset`. **Wave 3:** the symbol checks survive with the loader; the window checks go with the window.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/data/datacheck.c:97` | `0x80000000u` | the window's host address | PC_RDRAM_BASE |
| `port/data/datacheck.c:102` | `0x80000000` | the window's host address | PC_RDRAM_BASE |
| `port/data/datacheck.c:130` | `0x80118E28` | `gFlashColor1` | the symbol's name, looked up in the loader's table (`gFlashColor1`) |
| `port/data/datacheck.c:131` | `0x801146BC` | `flashColors` | the symbol's name, looked up in the loader's table (`flashColors`) |
| `port/data/datacheck.c:132` | `0x80116DB0` | `options_buttonColor` | the symbol's name, looked up in the loader's table (`options_buttonColor`) |
| `port/data/datacheck.c:133` | `0x8011AD8C` | `gShadowColor` | the symbol's name, looked up in the loader's table (`gShadowColor`) |
| `port/data/datacheck.c:134` | `0x8011ADC0` | `SheenColors` | the symbol's name, looked up in the loader's table (`SheenColors`) |
| `port/data/datacheck.c:135` | `0x8011AE58` | `Track1Colors` | the symbol's name, looked up in the loader's table (`Track1Colors`) |
| `port/data/datacheck.c:136` | `0x8011B558` | `arrowColors` | the symbol's name, looked up in the loader's table (`arrowColors`) |
| `port/data/datacheck.c:137` | `0x8011B578` | `arrow_base_color` | the symbol's name, looked up in the loader's table (`arrow_base_color`) |
| `port/data/datacheck.c:138` | `0x8011EF10` | `cull_box_vertices_a` | the symbol's name, looked up in the loader's table (`cull_box_vertices_a`) |
| `port/data/datacheck.c:139` | `0x8011EF1C` | `cull_box_vertices_a` + 0xC | the symbol's name, looked up in the loader's table (`cull_box_vertices_a`) |
| `port/data/datacheck.c:140` | `0x803B3054` | `carscr_solidColor` | the symbol's name, looked up in the loader's table (`carscr_solidColor`) |
| `port/data/datacheck.c:141` | `0x803B3458` | `circuitscr_init_button_color` | the symbol's name, looked up in the loader's table (`circuitscr_init_button_color`) |
| `port/data/datacheck.c:142` | `0x803B8328` | `loadTrackScreen_backColor` | the symbol's name, looked up in the loader's table (`loadTrackScreen_backColor`) |
| `port/data/datacheck.c:143` | `0x803B8354` | `race_map_vertex_color` | the symbol's name, looked up in the loader's table (`race_map_vertex_color`) |
| `port/data/datacheck.c:144` | `0x803B8394` | `trackscr_init_button_color` | the symbol's name, looked up in the loader's table (`trackscr_init_button_color`) |
| `port/data/datacheck.c:145` | `0x803AF9A8` | `carscr_buttonInfo` | the symbol's name, looked up in the loader's table (`carscr_buttonInfo`) |
| `port/data/datacheck.c:146` | `0x803AF9E4` | `carscr_buttonInfo` + 0x3C | the symbol's name, looked up in the loader's table (`carscr_buttonInfo`) |
| `port/data/datacheck.c:147` | `0x80394270` | `hudextra_playerColor` | the symbol's name, looked up in the loader's table (`hudextra_playerColor`) |
| `port/data/datacheck.c:148` | `0x803942A0` | `stunt_coin_color` | the symbol's name, looked up in the loader's table (`stunt_coin_color`) |
| `port/data/datacheck.c:149` | `0x803942E8` | `hitColor` | the symbol's name, looked up in the loader's table (`hitColor`) |
| `port/data/datacheck.c:150` | `0x8002AFF8` | `video_default_extra_width` | the symbol's name, looked up in the loader's table (`video_default_extra_width`) |
| `port/data/datacheck.c:151` | `0x8002B010` | `video_pending_frames` | the symbol's name, looked up in the loader's table (`video_pending_frames`) |
| `port/data/datacheck.c:152` | `0x8002B028` | `gInitFlag` | the symbol's name, looked up in the loader's table (`gInitFlag`) |
| `port/data/datacheck.c:153` | `0x8002B030` | `decompress_lock_ready` | the symbol's name, looked up in the loader's table (`decompress_lock_ready`) |
| `port/data/datacheck.c:156` | `0x803B3460` | `gEggs` | the symbol's name, looked up in the loader's table (`gEggs`) |
| `port/data/datacheck.c:157` | `0x803B3464` | `gEggs` + 0x4 | the symbol's name, looked up in the loader's table (`gEggs`) |
| `port/data/datacheck.c:158` | `0x803B4134` | `gEggs` + 0xCD4 | the symbol's name, looked up in the loader's table (`gEggs`) |
| `port/data/datacheck.c:159` | `0x803B41D8` | `start_screen_egg` + 0x4 | the symbol's name, looked up in the loader's table (`start_screen_egg`) |
| `port/data/datacheck.c:163` | `0x801108D0` | `steady_leg_duration` + 0x8 | the symbol's name, looked up in the loader's table (`steady_leg_duration`; since wave 3 step 2 `car_descriptors`) |
| `port/data/datacheck.c:164` | `0x803B28C8` | `gTransStats` | the symbol's name, looked up in the loader's table (`gTransStats`) |
| `port/data/datacheck.c:169` | `0x80118C10` | `BBoxCollInfo` | the symbol's name, looked up in the loader's table (`BBoxCollInfo`) |
| `port/data/datacheck.c:170` | `0x801163F8` | `percentTable` | the symbol's name, looked up in the loader's table (`percentTable`) |
| `port/data/datacheck.c:171` | `0x8011B4B4` | `wingOffsets` | the symbol's name, looked up in the loader's table (`wingOffsets`) |
| `port/data/datacheck.c:172` | `0x801202F8` | `gravityArray` | the symbol's name, looked up in the loader's table (`gravityArray`) |
| `port/data/datacheck.c:173` | `0x80120308` | `D_80120308` | the symbol's name, looked up in the loader's table (`D_80120308`) |
| `port/data/datacheck.c:175` | `0x801106D0` | `FSCLS` + 0xC | the symbol's name, looked up in the loader's table (`FSCLS`) |
| `port/data/datacheck.c:176` | `0x801106D4` | `FSCLS` + 0x10 | the symbol's name, looked up in the loader's table (`FSCLS`) |
| `port/data/datacheck.c:177` | `0x801108A0` | `FSCLS` + 0x1DC | the symbol's name, looked up in the loader's table (`FSCLS`) |
| `port/data/datacheck.c:262` | `0x8038A400u` | the runtime images' base: random heap addresses below it; the function both images have there | the overlay base by name |
| `port/data/datacheck.c:263` | `0x1FFFFFFFu` | KSEG mask | K0_TO_PHYS |
| `port/data/datacheck.c:537` | `0x80000000u` | the window's host address | PC_RDRAM_BASE |
| `port/data/datacheck.c:547` | `0x8038A400u` | the runtime images' base: random heap addresses below it; the function both images have there | the overlay base by name |
| `port/data/datacheck.c:550` | `0x8038A400` | the runtime images' base: random heap addresses below it; the function both images have there | the overlay base by name |
| `port/data/datacheck.c:587` | `0x8002E440` | `build_date_text` | the symbol's name, looked up in the loader's table (`build_date_text`) |
| `port/data/datacheck.c:597` | `0x8002B040`, `0x8002B08C`, `0x8002B184` | `unzip_border`; `0x8002B08C`: `unzip_cplens`; `0x8002B184`: `unzip_mask_bits` | the symbol's name, looked up in the loader's table (`unzip_border`); `0x8002B08C`: the symbol's name, looked up in the loader's table (`unzip_cplens`); `0x8002B184`: the symbol's name, looked up in the loader's table (`unzip_mask_bits`) |
| `port/data/datacheck.c:638` | `0x8011B5BC`, `0x8002E580` | `wave_rom_offsets`; `0x8002E580`: `gWaveSizes` | the symbol's name, looked up in the loader's table (`wave_rom_offsets`); `0x8002E580`: the symbol's name, looked up in the loader's table (`gWaveSizes`) |
| `port/data/datacheck.c:639` | `0x80123564` | `wave_storage_kinds` | the symbol's name, looked up in the loader's table (`wave_storage_kinds`) |
| `port/data/datacheck.c:640` | `0x801234AC` | `wave_is_model` | the symbol's name, looked up in the loader's table (`wave_is_model`) |

## Check: the startup self-test

port/ultra/selftest.c (`--selftest`) checks the window (K0, K1 and physical aliases, the 16 MiB rule, the
relocation), that game data sits at its N64 address (the game's BSS end, the overlay base, osTvType, the file
table, image A's BSS start), and the fault note.

**Wave 2:** symbols by name; the scratch addresses as `PC_RDRAM_BASE + offset`. **Wave 3:** "at its N64 address" is
exactly what wave 3 ends: those checks go; the window checks go with the window.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/ultra/selftest.c:76` | `0x80000000u` | KSEG0 base | PC_RDRAM_BASE |
| `port/ultra/selftest.c:397` | `0x80001000`, `0xA0001000` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:400` | `0x80001000` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:401` | `0x80001000`, `0xA0001000` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:405` | `0x80000000u` | KSEG0 base | PC_RDRAM_BASE |
| `port/ultra/selftest.c:415` | `0x81000000u`, `0x80300040u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:418` | `0x80300000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:419` | `0x81000000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:421` | `0x80300000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:422` | `0x80300000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:423` | `0x80300000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:425` | `0x81000000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:427` | `0x81000000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:429` | `0x81000000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:433` | `0x8017A640u`, `0x8038A400u` | `game_bss_end / game_bss_end` at its N64 address in the window; `0x8038A400u`: `overlay_base / findDamageSlot` at its N64 address in the window | the symbol by name; this check goes in wave 3 (data off its N64 address) |
| `port/ultra/selftest.c:434` | `0x80000300u` | `osTvType / osTvType` at its N64 address in the window | the symbol by name; this check goes in wave 3 (data off its N64 address) |
| `port/ultra/selftest.c:444` | `0x80001000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:445` | `0x80001000` | a window address for the K0/K1/physical, 16 MiB and relocation checks | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:447` | `0x80001000`, `0x80000000u` | a window address for the K0/K1/physical, 16 MiB and relocation checks; `0x80000000u`: KSEG0 base | PC_RDRAM_BASE + offset (named), K1BASE; stays while the window exists |
| `port/ultra/selftest.c:451` | `0x8002E580u`, `0x8011B5BCu` | `gWaveSizes` at its N64 address in the window; `0x8011B5BCu`: `wave_rom_offsets` at its N64 address in the window | the symbol by name; this check goes in wave 3 (data off its N64 address) |
| `port/ultra/selftest.c:452` | `0x803B9A50u` | `gTrophyMat` at its N64 address in the window | the symbol by name; this check goes in wave 3 (data off its N64 address) |

## Tests: assets and game C

port/assets/tests link game C with stand-ins. Three kinds of address: game symbols placed at their N64 address
with `ABS(name, addr)` (an assembler `.set`) or read there (`WP(0x8014D280 + ...)`); scratch places in the window
for test data (0x80300000 as the heap, 0x80321000 for input packets, 0x80200000/0x80201000 for palettes; the
symbol of the N64 framebuffer at 0x80200000 is a coincidence); and the cartridge's sample base.
test_heap.c copies the overlay loader's two literals.

**Wave 2:** game symbols by name (`&gTargetArray` becomes its map name); scratch places as named test constants
(`PC_RDRAM_BASE + offset`); the sample base by the game's symbol name. **Wave 3:** the ABS lines go (the symbols
become ordinary definitions); scratch places become host buffers once the code under test takes host pointers.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/assets/tests/assetcheck.c:40` | `0x80086A50u` | the game image's base (`gDecompressedDataStart`) | the loader table's image bases (by image name) |
| `port/assets/tests/assetcheck.c:43` | `0x800277D0u` | the boot data's base (`gsm_rpe_decode`, the start of the boot tail) | the loader table's image bases (by image name) |
| `port/assets/tests/assetcheck.c:470` | `0x8011B5BC`, `0x8002E580` | `wave_rom_offsets` (the file table) read by its N64 address; `0x8002E580`: `gWaveSizes` (the file table) read by its N64 address | the symbol by name through the loader |
| `port/assets/tests/assetcheck.c:471` | `0x80123564` | `wave_storage_kinds` (the file table) read by its N64 address | the symbol by name through the loader |
| `port/assets/tests/support.c:161` | `0x80000000u`, `0x1FFFFFFFu` | KSEG0 base / mask in the test's own pc_n64_ptr | K0BASE, K0_TO_PHYS (libultra) |
| `port/assets/tests/test_battle.c:33` | `0x8014D280u` | `gTargetArray` placed at (or read at) its N64 address | gone (wave 3.1, NATIVE.md step 3): the nodes are host data, `pc_target_nodes`, which the tests define |
| `port/assets/tests/test_battle.c:35` | `0x8014D280` | `gTargetArray` placed at (or read at) its N64 address | gone (wave 3.1, NATIVE.md step 3): the nodes are host data, `pc_target_nodes`, which the tests define |
| `port/assets/tests/test_battle.c:36` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_cars.c:58` | `0x80151CE8` | `gCheckpoints` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:59` | `0x80151CEE` | `checkpoints_start_pt` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:60` | `0x80151D18` | `gCheckpoints_pathPts` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:61` | `0x80151CE8` | `gCheckpoints` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:62` | `0x80139320` | `gCarParts` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:63` | `0x80139320` | `gCarParts` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:64` | `0x801116D0` | `gTune` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:65` | `0x8011F724` | `stdtorquecurve` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:66` | `0x8011F814` | `frontTire` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:67` | `0x801116D0` | `gTune` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:68` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_cars.c:69` | `0x80301000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_cars.c:91` | `0x8014254C` | `gObjList_biased` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_cars.c:447` | `0x801116D0u`, `0x8011F724u` | `gTune`: a model field holds its N64 address; `0x8011F724u`: `stdtorquecurve`: a model field holds its N64 address | N64A(&symbol) by name; wave 3: a host pointer |
| `port/assets/tests/test_cars.c:502` | `0x80302000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_casts.c:96` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_casts.c:117` | `0x80124FD0` | `skyMatrices` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_casts.c:118` | `0x80150B70` | `gCamera / gEffectEmittersArray / gCamera` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_casts.c:119` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_casts.c:131` | `0x80150B70u` | `gCamera / gEffectEmittersArray / gCamera` read at its N64 address | the symbol by name; goes in wave 3 |
| `port/assets/tests/test_casts.c:133` | `0x80124FD0u` | `skyMatrices` read at its N64 address | the symbol by name; goes in wave 3 |
| `port/assets/tests/test_casts.c:139` | `0x80124FD0u` | `skyMatrices` read at its N64 address | the symbol by name; goes in wave 3 |
| `port/assets/tests/test_casts.c:324` | `0x8013F1F8` | `ghost_file_handles` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_casts.c:325` | `0x8013F390` | `ghost_file_records` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_casts.c:397` | `0x80310000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_casts.c:522` | `0x80123456u` | an arbitrary window address (PC_N64W identity) | PC_RDRAM_BASE + offset (named) |
| `port/assets/tests/test_casts.c:523` | `0xB0039B40u` | a cartridge address: MusyX's sample base (KSEG1 / the game's sample offset) | PHYS_TO_K1(PI_DOM1_ADDR2) + the sample base offset (named as the game names it) |
| `port/assets/tests/test_endian.c:182` | `0x80200000u` | a scratch place in the window (the N64 framebuffer's address, unused by the test) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_endian.c:183` | `0x80201000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_font.c:101` | `0x80123450u` | an arbitrary window address (inside `random_bit_masks` + 0x38, unrelated) | a named test constant |
| `port/assets/tests/test_heap.c:26` | `0x80000000u`, `0x80800000u` | IN_WINDOW: the 8 MiB window range | PC_RDRAM_BASE, PC_RDRAM_BASE + PC_RDRAM_SIZE |
| `port/assets/tests/test_heap.c:41` | `0x8017A640` | `game_bss_end / game_bss_end` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_heap.c:42` | `0x80000318` | `osMemSize / osMemSize` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_heap.c:115` | `0x80400000u` | the heap's end in a 4 MiB window | PC_RDRAM_BASE + 4 MiB (named) |
| `port/assets/tests/test_heap.c:132` | `0x82000000u`, `0x81100000u` | the heap's end and floor with a 32 MiB window (game.memory) | PC_RDRAM_BASE + size (named) |
| `port/assets/tests/test_heap.c:135` | `0x81100000u` | the heap's end and floor with a 32 MiB window (game.memory) | PC_RDRAM_BASE + size (named) |
| `port/assets/tests/test_heap.c:137` | `0x8038A400u` | the runtime images' block (`overlay_base` to the loader's reservation end) | the overlay base by name and the loader's constant; goes in wave 3 |
| `port/assets/tests/test_heap.c:138` | `0x803BB380u`, `0x8038A400u` | the runtime images' block (`overlay_base` to the loader's reservation end) | the overlay base by name and the loader's constant; goes in wave 3 |
| `port/assets/tests/test_heap.c:139` | `0x8038A400u`, `0x803BB380u` | the runtime images' block (`overlay_base` to the loader's reservation end) | the overlay base by name and the loader's constant; goes in wave 3 |
| `port/assets/tests/test_heap.c:142` | `0x81100000u` | the heap's end and floor with a 32 MiB window (game.memory) | PC_RDRAM_BASE + size (named) |
| `port/assets/tests/test_input.c:61` | `0x80321000u`, `0x80321040u`, `0x80321080u`, `0x803210C0u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_input.c:169` | `0x80321000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_input.c:170` | `0x80321040u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_input.c:173` | `0x80321000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_input.c:174` | `0x80321040u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_input.c:177` | `0x80321000u`, `0x80321040u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_mods.c:402` | `0x8002E000u` | the base of a synthetic boot/game data slice around the file table | derive from the file table symbols' names |
| `port/assets/tests/test_mods.c:403` | `0x8011B000u` | the base of a synthetic boot/game data slice around the file table | derive from the file table symbols' names |
| `port/assets/tests/test_mods.c:434` | `0x80123564u` | `wave_storage_kinds` (the file table) read by its N64 address | the symbol by name through the loader |
| `port/assets/tests/test_mods.c:776` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:810` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:815` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:829` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:834` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:837` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:840` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:848` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:868` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_mods.c:882` | `0xB0039B40u`, `0xB0000000u` | a cartridge address: MusyX's sample base (KSEG1 / the game's sample offset); `0xB0000000u`: KSEG1 base of the cartridge | PHYS_TO_K1(PI_DOM1_ADDR2) + the sample base offset (named as the game names it) |
| `port/assets/tests/test_mods.c:883` | `0xB0039B40u`, `0xB0000000u` | a cartridge address: MusyX's sample base (KSEG1 / the game's sample offset); `0xB0000000u`: KSEG1 base of the cartridge | PHYS_TO_K1(PI_DOM1_ADDR2) + the sample base offset (named as the game names it) |
| `port/assets/tests/test_mods.c:893` | `0x39B40` | MusyX's sample base as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name, or a named test constant |
| `port/assets/tests/test_musyx.c:242` | `0xB0039B40u` | a cartridge address: MusyX's sample base (KSEG1 / the game's sample offset) | PHYS_TO_K1(PI_DOM1_ADDR2) + the sample base offset (named as the game names it) |
| `port/assets/tests/test_musyx.c:647` | `0x80000000UL` | KSEG0 base (a mapping of the window at its fixed address) | PC_RDRAM_BASE |
| `port/assets/tests/test_musyx.c:761` | `0x80050A50u` | `sound_handle_nodes` (now pc_mx_nodes): the voice's node link, a 32-bit token the game C keeps as the old N64 address (src/rom/sound/voices.c) | a named token base from the game C (its PC_MX node encoding), not the literal |
| `port/assets/tests/test_musyx.c:773` | `0x80050A50u` | `sound_handle_nodes` (now pc_mx_nodes): the voice's node link, a 32-bit token the game C keeps as the old N64 address (src/rom/sound/voices.c) | a named token base from the game C (its PC_MX node encoding), not the literal |
| `port/assets/tests/test_save.c:36` | `0x80000000u`, `0x80800000u` | IN_WINDOW: the 8 MiB window range | PC_RDRAM_BASE, PC_RDRAM_BASE + PC_RDRAM_SIZE |
| `port/assets/tests/test_save.c:191` | `0x8017A640` | `game_bss_end / game_bss_end` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_save.c:192` | `0x80000318` | `osMemSize / osMemSize` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_targets.c:59` | `0x8014D280u` | `gTargetArray` placed at (or read at) its N64 address | gone (wave 3.1, NATIVE.md step 3): the nodes are host data, `pc_target_nodes`, which the tests define |
| `port/assets/tests/test_targets.c:61` | `0x8014D280` | `gTargetArray` placed at (or read at) its N64 address | gone (wave 3.1, NATIVE.md step 3): the nodes are host data, `pc_target_nodes`, which the tests define |
| `port/assets/tests/test_targets.c:62` | `0x80118C10` | `BBoxCollInfo` placed at (or read at) its N64 address | the symbol by name; the ABS() lines go in wave 3 (ordinary definitions) |
| `port/assets/tests/test_targets.c:65` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_targets.c:321` | `0x80118C10u` | `BBoxCollInfo`: a node field holds its N64 address | N64A(&symbol) by name; wave 3: a host pointer |
| `port/assets/tests/test_targets.c:564` | `0x80118C10u` | `BBoxCollInfo`: a node field holds its N64 address | N64A(&symbol) by name; wave 3: a host pointer |
| `port/assets/tests/test_wad.c:129` | `0x80001000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_wad.c:132` | `0x80001000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_weapons.c:40` | `0x8014D280u` | `gTargetArray` placed at (or read at) its N64 address | gone (wave 3.1, NATIVE.md step 3): the nodes are host data, `pc_target_nodes`, which the tests define |
| `port/assets/tests/test_weapons.c:41` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_windows.c:56` | `0x1FFFFFFF` | KSEG mask (a stub osVirtualToPhysical) | K0_TO_PHYS |
| `port/assets/tests/test_windows.c:152` | `0x80300000u`, `0x80340000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_world.c:93` | `0x80001234u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_world.c:159` | `0x80001234u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/test_world.c:226` | `0x80001234u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/wad_game.c:100` | `0x80000000u`, `0x1FFFFFFFu` | KSEG0 base / mask in the test's own pc_n64_ptr | K0BASE, K0_TO_PHYS (libultra) |
| `port/assets/tests/wad_game.c:103` | `0x80000000u` | KSEG0 base / mask in the test's own pc_n64_ptr | K0BASE, K0_TO_PHYS (libultra) |
| `port/assets/tests/wad_game.c:121` | `0x1FFFFFFFu` | KSEG0 base / mask in the test's own pc_n64_ptr | K0BASE, K0_TO_PHYS (libultra) |
| `port/assets/tests/wad_game.c:179` | `0x80200000u` | a scratch place in the window (the N64 framebuffer's address, unused by the test) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/wad_game.c:241` | `0x80300000u` | a scratch place in the window (test heap or buffer) | a named test constant (PC_RDRAM_BASE + offset), or a host buffer once the code takes host pointers |
| `port/assets/tests/wad_game.c:266` | `0x81200000u` | a window address past 8 MiB (game.memory) | PC_RDRAM_BASE + size (named) |

## Tests: the renderer and the audio interpreter

The renderer's and the audio interpreter's tests build display lists, matrices, textures and task records at
window addresses (0x80100000, 0x80200000) and check window sizes past 16 MiB (0x81200000, 0x83F00000); stubs.c,
gfx_test.c, interp_test.c, tex_test.c and test_window.c translate addresses themselves.

**Wave 2:** named constants (`PC_RDRAM_BASE + n MiB`), libultra's macros for the translation. **Wave 3:** stay while
the renderer and the interpreter read N64 addresses.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/audio/tests/stubs.c:43` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |
| `port/audio/tests/stubs.c:52` | `0x80000000u` | KSEG0 base in the test's own pc_n64_addr | K0BASE |
| `port/audio/tests/test_window.c:59` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |
| `port/audio/tests/test_window.c:202` | `0x80100000u`, `0x81200000u`, `0x83F00000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:61` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |
| `port/gfx/tests/gfx_test.c:1161` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/gfx_test.c:1189` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/gfx_test.c:1215` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/gfx_test.c:1222` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/gfx_test.c:1251` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |
| `port/gfx/tests/gfx_test.c:1342` | `0x80100000u`, `0x81200000u`, `0x83F00000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1347` | `0x81200000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1348` | `0x81200000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1349` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/gfx_test.c:1350` | `0x81200000` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1351` | `0x81200000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1352` | `0x81200000u` | a window address at 1 MiB, 18 MiB or 63 MiB (window-size cases) | PC_RDRAM_BASE + n MiB (named) |
| `port/gfx/tests/gfx_test.c:1353` | `0x80100000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/interp_test.c:34` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |
| `port/gfx/tests/interp_test.c:210` | `0x80200000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/interp_test.c:214` | `0x80200000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/interp_test.c:227` | `0x80201000u` | a scratch place in the window (display lists, matrices, textures, models) | PC_RDRAM_BASE + offset (named), or host buffers once the renderer takes host pointers |
| `port/gfx/tests/tex_test.c:49` | `0x1FFFFFFFu` | KSEG mask in the test's own address translation | K0_TO_PHYS |

## Tests: the boot segment's GSM kernels

test_gsm.c runs the boot tail's hand-written GSM kernels (MIPS code read from the cartridge) in a small
emulator and compares them with port/ultra/gsm.c: the kernels' entry points and two tables inside them, the
emulator's RAM, and a return address that only has to be no kernel's.

**Wave 2:** the kernels' names from the boot map (rom.tsv), their addresses from the loader's table; named test
constants for the rest.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/ultra/tests/test_gsm.c:59` | `0x800277D0` | `gsm_rpe_decode`: a hand-written GSM kernel in the boot tail | the kernel's name from the boot data map (rom.tsv), its address from the loader's table |
| `port/ultra/tests/test_gsm.c:60` | `0x800277D0`, `0x800278C4`, `0x80027B44`, `0x800278BC`, `0x80027978` | `gsm_rpe_decode`: a hand-written GSM kernel in the boot tail; `0x800278C4`: `gsm_long_term_synthesis`: a hand-written GSM kernel in the boot tail; `0x80027B44`: `gsm_short_term_filter`: a hand-written GSM kernel in the boot tail; `0x800278BC`: gsm_rpe_decode + 0xEC: a table inside the kernels; `0x80027978`: gsm_long_term_synthesis + 0xB4: a table inside the kernels | the kernel's name from the boot data map (rom.tsv), its address from the loader's table; `0x800278BC`: a named offset from the kernel's symbol |
| `port/ultra/tests/test_gsm.c:61` | `0x80100000`, `0x80000180` | the emulator's scratch RAM base; `0x80000180`: the general exception vector address, used as a return sentinel | a named test constant; `0x80000180`: a named sentinel (it only has to be no kernel address) |

## Tools: the image layout (n64syms.py)

tools/port/n64syms.py is the tools' memory map: IMAGES (each image's ROM offset, sizes and load address),
ARENAS (data start, BSS start, end), CODE ranges, REGIONS (names for every range) and BSS_REGIONS. The same
numbers sit in the symbol tables (asm/us/*/symbols.json: `gBssStart`, `game_bss_end`, the overlay bounds), in the
loader's generated tables and in docs/port/DATA.md.

**Wave 2:** read them from the symbol tables (one source) instead of copying; ARENAS, CODE, REGIONS and BSS_REGIONS
derived from one table. **Wave 3:** the overlay rows go with overlays as PC memory.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `tools/port/n64syms.py:16` | `0x800277D0`, `0x283D0` | IMAGES: an image's ROM offset and load address (boot data, game image, runtime images A and B); `0x283D0`: the boot data's ROM offset | one image table (the loader's), read by every tool; the A/B rows go with overlays as PC memory (wave 3); `0x283D0`: one image table (the loader's) shared by the tools |
| `tools/port/n64syms.py:17` | `0x80086A50`, `0xB0CB10` | IMAGES: an image's ROM offset and load address (boot data, game image, runtime images A and B); `0xB0CB10`: the game image's ROM offset | one image table (the loader's), read by every tool; the A/B rows go with overlays as PC memory (wave 3); `0xB0CB10`: one image table shared by the tools |
| `tools/port/n64syms.py:18` | `0x8038A400`, `0xB5C534` | IMAGES: an image's ROM offset and load address (boot data, game image, runtime images A and B); `0xB5C534`: runtime image A's ROM offset | one image table (the loader's), read by every tool; the A/B rows go with overlays as PC memory (wave 3); `0xB5C534`: one image table shared by the tools |
| `tools/port/n64syms.py:19` | `0x8038A400`, `0xB6FEC4` | IMAGES: an image's ROM offset and load address (boot data, game image, runtime images A and B); `0xB6FEC4`: runtime image B's ROM offset | one image table (the loader's), read by every tool; the A/B rows go with overlays as PC memory (wave 3); `0xB6FEC4`: one image table shared by the tools |
| `tools/port/n64syms.py:24` | `0x800277D0`, `0x8002E8E0`, `0x80086A50` | ARENAS: an arena's data start, BSS start and end (docs/port/DATA.md 3) | from the bound symbols in asm/us/*/symbols.json (gBssStart, game_bss_end, the overlay bounds) instead of copies |
| `tools/port/n64syms.py:25` | `0x8010FD7C`, `0x801249F0`, `0x8017A640` | ARENAS: an arena's data start, BSS start and end (docs/port/DATA.md 3) | from the bound symbols in asm/us/*/symbols.json (gBssStart, game_bss_end, the overlay bounds) instead of copies |
| `tools/port/n64syms.py:26` | `0x803AF978`, `0x803B9A50`, `0x803BAE20` | ARENAS: an arena's data start, BSS start and end (docs/port/DATA.md 3) | from the bound symbols in asm/us/*/symbols.json (gBssStart, game_bss_end, the overlay bounds) instead of copies |
| `tools/port/n64syms.py:27` | `0x80393B34`, `0x80394F70`, `0x8039B440` | ARENAS: an arena's data start, BSS start and end (docs/port/DATA.md 3) | from the bound symbols in asm/us/*/symbols.json (gBssStart, game_bss_end, the overlay bounds) instead of copies |
| `tools/port/n64syms.py:30` | `0x80000400`, `0x800277D0`, `0x80086A50`, `0x8010FD7C` | CODE: an image's code range | from the symbol tables |
| `tools/port/n64syms.py:31` | `0x8038A400`, `0x803AF978`, `0x80393B34` | CODE: an image's code range | from the symbol tables |
| `tools/port/n64syms.py:66` | `0x80000000`, `0x80000400` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:67` | `0x80000400`, `0x800277D0` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:68` | `0x800277D0`, `0x8002E8E0` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:69` | `0x8002E8E0`, `0x80086A50` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:70` | `0x80086A50`, `0x8010FD7C` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:71` | `0x8010FD7C`, `0x801249F0` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:72` | `0x801249F0`, `0x8017A640` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:73` | `0x8038A400`, `0x80400000` | REGIONS: the memory map's names (low memory, boot code/data/BSS, game code/data/BSS, overlay) | from IMAGES and ARENAS; the 0x80400000 overlay end is the 4 MiB mark, not a bound of the images |
| `tools/port/n64syms.py:75` | `0x8002E8E0`, `0x80086A50`, `0x801249F0`, `0x8017A640` | BSS_REGIONS: the boot and game BSS | from ARENAS (one copy) |

## Tools: the generators (gen_data.py, gen_stubs.py)

gen_data.py (the loader's generator) and gen_stubs.py copy the window's range and the heap's start.

**Wave 2:** from n64syms (one place).

| Where | Literal | What | Replacement |
|---|---|---|---|
| `tools/port/gen_data.py:86` | `0x8017A640` | HEAP_LO: the heap's start (`game_bss_end`) | from n64syms (ARENAS[game] end) |
| `tools/port/gen_data.py:89` | `0x80000000`, `0x80800000` | WINDOW_LO/HI: the 8 MiB window | from n64syms (one place), PC_RDRAM_BASE/SIZE |
| `tools/port/gen_data.py:98` | `0x10000000` | WINDOW_SECTION: the window section's size (PC_RDRAM_MAX) | from one constant shared with pc_port.h |
| `tools/port/gen_data.py:1539` | `0x80000000` | KSEG0 base in generated comments and a range test | K0BASE from n64syms |
| `tools/port/gen_data.py:1542` | `0x80000000` | KSEG0 base in generated comments and a range test | K0BASE from n64syms |
| `tools/port/gen_data.py:1718` | `0x80000000` | KSEG0 base in generated comments and a range test | K0BASE from n64syms |
| `tools/port/gen_data.py:1857` | `0x80000000` | KSEG0 base in generated comments and a range test | K0BASE from n64syms |
| `tools/port/gen_stubs.py:105` | `0x80000000`, `0x80800000` | the 8 MiB window range | from n64syms |
| `tools/port/gen_stubs.py:108` | `0xB0000000` | a cartridge address: KSEG1 / ROM offset for a symbol below the window | PHYS_TO_K1(PI_DOM1_ADDR2) from n64syms |

## Tools: checks and audits

Audits and checks: nofixed_audit.py's KSEG, RCP and PIF ranges; hash_compare.py and heap_check.py turning a
window offset into an N64 address; alias_check.py's message about a row that runs over its neighbours; synth_rom.py
(the synthetic cartridge) writing the car descriptors at fixed offsets inside their symbol and the sample base.

**Wave 2:** K0BASE and the ranges from n64syms; offsets from the symbol it already looks up by name.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `tools/port/alias_check.py:42` | `0x803B5D5C` | `gPlayerRects_biased` + 0x60 (its second row, which runs over `playerscr_gPlayerRects`), in a message | the symbol by name with the offset (rename converts the name; the literal can be computed) |
| `tools/port/hash_compare.py:179` | `0x80000000` | KSEG0 base: a window offset printed as its N64 address | K0BASE from n64syms |
| `tools/port/heap_check.py:57` | `0x80000000` | KSEG0 base: the heap's end as a window address | K0BASE from n64syms |
| `tools/port/nofixed_audit.py:166` | `0xA4000000`, `0xA4900000`, `0xBFC00000` | the RCP registers' KSEG1 range; `0xBFC00000`: the PIF's KSEG1 range | named range constants in n64syms; `0xBFC00000`: named constant in n64syms |
| `tools/port/nofixed_audit.py:168` | `0xA0000000` | KSEG1 base: RDRAM versus cartridge aliases | K1BASE from n64syms |
| `tools/port/nofixed_audit.py:172` | `0x80000000` | KSEG0 base / range (the audit of window uses) | K0BASE from n64syms; the audit goes with the window |
| `tools/port/nofixed_audit.py:277` | `0x80000000` | KSEG0 base / range (the audit of window uses) | K0BASE from n64syms; the audit goes with the window |
| `tools/port/nofixed_audit.py:443` | `0xA0000000` | KSEG1 base: RDRAM versus cartridge aliases | K1BASE from n64syms |
| `tools/port/nofixed_audit.py:522` | `0x80000000` | KSEG0 base / range (the audit of window uses) | K0BASE from n64syms; the audit goes with the window |
| `tools/port/synth_rom.py:445` | `0x39B40` | MusyX's sample base, as a ROM offset (the game C's `sample_bank_rom`) | the game's symbol by name |
| `tools/port/synth_rom.py:1645` | `0x801108D0`, `0x80110A00` | `steady_leg_duration` + 0x8: the car physics descriptors; `0x80110A00`: `steady_leg_duration` + 0x138: the descriptors' suspension part | base + offset (the base is already looked up by name two lines up); since wave 3 step 2 `car_descriptors` and `car_descriptors` + 0x130 |

## Not addresses

Literals that look like addresses and are not: colours (menu.c, netwait.c, the UI test), hash constants
(SHA-1 in romcheck.c and datacheck.c, BLAKE2s's IV), the pak ID's repair words, sign bits (MIPS float-to-unsigned in
pc_prelude.h), flag words with the top bit set (a blit's animation id, a weapon record, world node flags), masks of
RDP and geometry words, PE resource flags, test patterns, and the .z64 header's first word. Nothing to do.

| Where | Literal | What | Replacement |
|---|---|---|---|
| `port/app/netwait.c:46` | `0x1C2C4CFFu` | an RGBA colour | nothing: not an address |
| `port/app/netwait.c:49` | `0x8CC8FFFFu` | an RGBA colour | nothing: not an address |
| `port/assets/check.c:221` | `0xFF000000u` | a G_VTX word's segment byte (not a place in the file) | nothing: not an address |
| `port/assets/tests/test_blit.c:147` | `0x80000003u` | a blit's animation id with its top bit set | nothing: not an address |
| `port/assets/tests/test_carscr.c:140` | `0x11223344` | a colour test pattern | nothing: not an address |
| `port/assets/tests/test_carscr.c:178` | `0x11223344` | a colour test pattern | nothing: not an address |
| `port/assets/tests/test_casts.c:502` | `0x80000000u` | the sign bit (float-to-unsigned test) | nothing: not an address |
| `port/assets/tests/test_casts.c:506` | `0x80000001u` | a float-to-unsigned result | nothing: not an address |
| `port/assets/tests/test_dispatch.c:12` | `0x11223344u` | a test pattern | nothing: not an address |
| `port/assets/tests/test_dispatch.c:14` | `0x11223344u` | a test pattern | nothing: not an address |
| `port/assets/tests/test_dispatch.c:23` | `0x11223344u` | a test pattern | nothing: not an address |
| `port/assets/tests/test_effects.c:42` | `0x15000000` | a record flag word | nothing: not an address |
| `port/assets/tests/test_input.c:123` | `0x12345678u` | a test pattern (a packet word) | nothing: not an address |
| `port/assets/tests/test_input.c:127` | `0x12345678u` | a test pattern (a packet word) | nothing: not an address |
| `port/assets/tests/test_musyx.c:688` | `0x80000000u` | a sequencer request's result word (top bit set) | nothing: not an address |
| `port/assets/tests/test_roster.c:74` | `0x7FFFFFFF` | a stub's free byte count (INT32_MAX) | nothing: not an address |
| `port/assets/tests/test_save.c:588` | `0x11112222` | a test pattern (a player ID) | nothing: not an address |
| `port/assets/tests/test_save.c:602` | `0x11112222` | a test pattern (a player ID) | nothing: not an address |
| `port/assets/tests/test_save.c:607` | `0x11112222` | a test pattern (a player ID) | nothing: not an address |
| `port/assets/tests/test_save.c:626` | `0x12345678` | a test pattern | nothing: not an address |
| `port/assets/tests/test_save.c:639` | `0x11112222` | a test pattern (a player ID) | nothing: not an address |
| `port/assets/tests/test_save.c:648` | `0x12345678` | a test pattern | nothing: not an address |
| `port/assets/tests/test_targets.c:527` | `0xFF000000u` | a mask of a flags word's top byte | nothing: not an address |
| `port/assets/tests/test_wad.c:42` | `0xA0B0C0D0u` | a test pattern (xflags) | nothing: not an address |
| `port/assets/tests/test_weapons.c:181` | `0x80000000u` | a weapon record's flag word, top bit set | nothing: not an address |
| `port/assets/tests/test_windows.c:159` | `0x12341234u` | a test pattern (a display-list word) | nothing: not an address |
| `port/assets/tests/test_world.c:52` | `0xA0000000u` | a world node's flag bits (bits 29 and 31) | nothing: not an address |
| `port/assets/tests/test_world.c:122` | `0xA0000000u` | a world node's flag bits (bits 29 and 31) | nothing: not an address |
| `port/assets/tests/test_world.c:147` | `0xA0000004u` | a world node's flag bits | nothing: not an address |
| `port/assets/tests/test_zoid.c:93` | `0x11223344`, `0xAABBCCDD` | a colour test pattern | nothing: not an address |
| `port/assets/tests/wad_build.h:105` | `0xA0B0C0D0u` | a test pattern (xflags) | nothing: not an address |
| `port/assets/tests/wad_game.c:260` | `0x12345678` | a test pattern (a missing name) | nothing: not an address |
| `port/audio/tests/test_envmix.c:119` | `0x80000000u` | INT32_MIN (an envelope level) | nothing: not an address |
| `port/audio/tests/test_rsp.c:93` | `0x80000000u` | INT32_MIN (an envelope level) | nothing: not an address |
| `port/data/datacheck.c:52` | `0x10325476` | a SHA-1 constant | nothing: not an address |
| `port/data/datacheck.c:74` | `0x8F1BBCDC` | a SHA-1 constant | nothing: not an address |
| `port/data/test_pc_data.c:166` | `0x11223344` | a test pattern | nothing: not an address |
| `port/data/test_pc_data.c:295` | `0x11223344` | a test pattern | nothing: not an address |
| `port/gfx/gfx_rdp.c:900` | `0x00FFFFFF` | a mask of an RDP command word | nothing: not an address |
| `port/gfx/gfx_rdp.c:1350` | `0x00FFFFFF` | a mask of an RDP command word | nothing: not an address |
| `port/gfx/gfx_rsp.c:980` | `0x00FFFFFF` | a mask of the geometry-mode word | nothing: not an address |
| `port/gfx/tests/ui_test.c:200` | `0x10203040u` | an RGBA colour | nothing: not an address |
| `port/gfx/tests/ui_test.c:201` | `0x10C030FFu` | an RGBA colour | nothing: not an address |
| `port/include/pc_prelude.h:79` | `0x80000000u` | the sign bit (MIPS float-to-unsigned conversion) | nothing: not an address |
| `port/include/pc_prelude.h:235` | `0x80000000u` | the sign bit (MIPS float-to-unsigned conversion) | nothing: not an address |
| `port/include/pc_prelude.h:245` | `0x80000000u` | the sign bit (MIPS float-to-unsigned conversion) | nothing: not an address |
| `port/input/menu.c:508` | `0x1C2C4CFFu` | an RGBA colour | nothing: not an address |
| `port/input/menu.c:512` | `0x8CC8FFFFu` | an RGBA colour | nothing: not an address |
| `port/input/menu.c:1001` | `0x101828E8u` | an RGBA colour | nothing: not an address |
| `port/net/net_blake2s.c:8` | `0xBB67AE85u`, `0xA54FF53Au` | BLAKE2s's initial vector | nothing: not an address |
| `port/net/net_blake2s.c:9` | `0x1F83D9ABu` | BLAKE2s's initial vector | nothing: not an address |
| `port/net/tests/net_test.c:206` | `0xABCD0000u` | a test pattern | nothing: not an address |
| `port/net/tests/net_test.c:275` | `0xABCD0001u` | a test pattern | nothing: not an address |
| `port/ultra/pakfile.c:69` | `0x135F1A05u` | a pak ID's random word (libultra's repair value) | nothing: not an address |
| `port/ultra/romcheck.c:31` | `0x8F1BBCDC` | a SHA-1 constant | nothing: not an address |
| `port/ultra/romcheck.c:42` | `0x10325476` | a SHA-1 constant | nothing: not an address |
| `port/ultra/statehash.c:516` | `0x7FFFFFFFu` | a float's sign mask | nothing: not an address |
| `port/ultra/tests/test_gsm.c:130` | `0xBAD00000u` | a register poison pattern | nothing: not an address |
| `port/ultra/tests/test_pakstore.c:250` | `0x12345678` | a test pattern | nothing: not an address |
| `port/ultra/tests/test_pakstore.c:409` | `0x12345678` | a test pattern | nothing: not an address |
| `tools/port/firstrun_check.py:61` | `0x135F1A05` | a pak ID's random word | nothing: not an address |
| `tools/port/ghost.py:209` | `0x135F1A05` | a pak ID's random word | nothing: not an address |
| `tools/port/ghost.py:258` | `0x135F1A05` | a pak ID's random word | nothing: not an address |
| `tools/port/pe_check.py:80` | `0x80000000`, `0x7FFFFFFF` | a PE resource directory's high-bit flag and its mask | nothing: not an address |
| `tools/port/pe_check.py:84` | `0x80000000` | a PE resource directory's high-bit flag and its mask | nothing: not an address |
| `tools/port/pe_check.py:86` | `0x7FFFFFFF` | a PE resource directory's high-bit flag and its mask | nothing: not an address |
| `tools/port/pe_check.py:87` | `0x7FFFFFFF` | a PE resource directory's high-bit flag and its mask | nothing: not an address |
| `tools/port/saves_check.py:151` | `0x12345678` | a test pattern (an XOR to break a checksum) | nothing: not an address |
| `tools/port/synth_rom.py:279` | `0x80371240` | the .z64 header's first word (PI timing and byte-order mark) | nothing: not an address |
| `tools/port/test_ghost.py:163` | `0x135F1A05` | a pak ID's random word or a test pattern | nothing: not an address |
| `tools/port/test_ghost.py:188` | `0x12345678` | a pak ID's random word or a test pattern | nothing: not an address |
| `tools/port/test_ghost.py:291` | `0x12345678` | a pak ID's random word or a test pattern | nothing: not an address |
| `tools/port/test_ghost.py:293` | `0x12345678` | a pak ID's random word or a test pattern | nothing: not an address |
