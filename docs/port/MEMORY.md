# The game's memory: game.memory (port/ultra, the renderer, the game C)

Written 2026-11-03; since [NATIVE.md](NATIVE.md) step 12 (2026-11-12) the heap is a block of PC memory. The N64 game
has 8 MiB of RDRAM with the Expansion Pak, 4 without. On the PC the game's heap is one mapping of PC memory, wherever the
OS puts it (`port/ultra/memory.c`, `pc_mem_map_heap`): osMemSize less what lay below the heap on the N64 (libultra's
low memory, the boot segment, the game image and its BSS: `PC_HEAP_OFFSET`, 0x17A640 bytes), which `pc_heap_memory` gives the game
([NATIVE.md](NATIVE.md) steps 6 and 12). `[game] memory` gives osMemSize and so the heap any size from 16 to 256 MiB; at
`original`, the default, nothing is different from before. This document says what the setting does, why 256 MiB is its
limit, which addresses had to change for memory past 16 MiB (until step 12) and where, how the heap's use is shown, what
the game does with the room, what a later "mods" feature can rely on, and how it was tested. Until step 12 the game's
RDRAM was a window mapped at host 0x80000000 and again at 0xA0000000, the heap that window's bytes from the end of the
game's BSS to `osMemSize | K0BASE`; sections 2, 3 and 7 keep what that took.

| Piece | Where |
|---|---|
| The heap's block | `port/ultra/memory.c` (`pc_mem_map_heap`, `pc_mem_heap`); `port/ultra/heap.c` (`pc_heap_block`, `pc_heap_memory`) |
| osMemSize | `port/ultra/misc.c` (`pc_os_set_mem_size`, the variable `osMemSize`) |
| The heap's high-water marks, the exit line, the test floor | `port/ultra/heap.c` |
| The setting, its warning, its range | `port/config/config.c` (`game.memory`, `pc_settings_memory_conflict`), `port/include/pc_port.h` (`PC_MEMORY_MIN_MIB`, `PC_MEMORY_MAX_MIB`), `port/main.c` |
| The overlay | `port/input/menu.c`: Game > Memory, About > Memory |
| The game C | the heap's set-up `memsys/group.c` (`locate_main_heap`); its reports in `memsys/group.c`, `heap_release/alloc_at.c`, `heap_release/group.c`, `loadcars/LoadCars.c`, `car_palette_init.c` |

## 1. The setting

`[game] memory` in rush2049pc.ini, `--set game.memory=N`, or F1 > Game > Memory. Like the other `[game]` keys it has
no environment variable.

| Value | osMemSize | The heap | The Expansion Pak flag (`expansion_pak_present`) |
|---|---|---|---|
| `original` (the default) | 8 MiB; with `expansion_pak = off` 4 MiB | 6.5 MiB (6,838,720 bytes; on the N64 0x8017A640-0x80800000), or 2.5 MiB | set; clear with `expansion_pak = off` |
| 16-256 (whole MiB) | N MiB | N MiB less 0x17A640 bytes | set |

- **It applies at the next start.** The heap's block is mapped once, after the ROM is read (main: `pc_os_set_mem_size`,
  then `pc_mem_map_heap`), and can't grow while the game holds pointers into it. The overlay's item is marked `*`
  ("applies after a restart") and its change is saved like the others.
- **What the game sees.** It reads its memory from osMemSize in one place, the heap's set-up
  (`locate_main_heap`, `memsys/group.c`): the heap's memory is what `pc_heap_memory(osMemSize)` gives
  (`port/ultra/heap.c`: the block, osMemSize less `PC_HEAP_OFFSET` bytes, the N64's room for the heap), and the
  Expansion Pak flag is set when osMemSize is past 4 MiB (the N64 tested the heap's end past 0x80400000: the same
  thing). Nothing else in the game reads the size (every `osMemSize` in the game C is an `extern` but that one). The
  block's first byte is as far past a 64 KiB boundary as the N64 heap's was (0xA640), so every address the game rounds
  (the heap's 32-byte chunks, the frame buffers' 64 bytes) comes out as on the N64.
- **`expansion_pak = off` with a size.** The game decides the Expansion Pak flag from its memory, as a console with
  that much memory would, so with `memory = 64` it uses its Expansion Pak features (the sixth race track, the tracks'
  animated objects, the front end's song: [QA.md](QA.md)) and `expansion_pak = off` does nothing. That combination is
  the one that makes no sense: the settings' resolution warns about it ("game.expansion_pak = off does nothing with
  game.memory = 64 ..."; `pc_settings_memory_conflict`, `config.c`), on every run and with `--print-config`. A
  console without the pak is `memory = original` with `expansion_pak = off`. Keeping the flag clear with a large heap
  would take a change to the game's own set-up for a combination no N64 had.
- **The cost.** On Linux the block is an anonymous mapping with `MAP_NORESERVE`, which is sparse: a page takes memory
  once the game first writes it, so 256 MiB costs what the heap uses (the synthetic race's peak: 0.9 MiB). On Windows
  it is committed when it is made (`VirtualAlloc`, top-down: the commit charge, not RAM; pages are made as they are
  touched), so a system that can't commit 256 MiB more fails at start with the error's number. Under LeakSanitizer the
  block is a root region, which LSan reads whole at exit: there every page becomes resident then.
- **Any host.** Nothing is at a fixed address, so the block maps wherever the OS allows, on every OS.
- **The sanitizers.** AddressSanitizer runs on x86-64 and aarch64 at any size ([SANITIZE.md](SANITIZE.md)): nothing
  sits in its shadow's range any more.

## 2. Why 256 MiB

Since [NATIVE.md](NATIVE.md) step 12 nothing ties the heap's size to the N64's address space: 256 MiB is the setting's
range (`PC_MEMORY_MAX_MIB`), which nothing in this game needs to pass (the whole cartridge is 32 MiB). Until then the
hard ceiling was 512 MiB: the window's KSEG0 view at 0x80000000 had to end before its KSEG1 view at 0xA0000000. Two
things made 256 MiB the limit below it (the history):

- **K0 addresses from 0x90000000.** The MusyX library tests RAM by the address's top nibble: its voice set-up
  (`sound_hw_set_voice_sample`, `hardware.c:1735`: `(data & 0xF0000000) != 0x80000000` marks a sample as in the cartridge) and
  the stream table (`speech_set_bank`, `streams.c:486`). A K0 address past 256 MiB is 0x9xxxxxxx and would be taken
  for a cartridge one. Up to 256 MiB every K0 address in the window is 0x8xxxxxxx, so both are right as they are.
- **KSEG1 from 0xB0000000.** The game names the cartridge by its KSEG1 address, `0xB0000000 | offset` (osRomBase,
  `musyx_cart_address.c:11`, the MusyX DMA's sources), which is the KSEG1 view of physical 0x10000000. Past 256 MiB the
  window's KSEG1 view would reach those values, and a cartridge address and a RAM one would be the same number.

Both could be changed (the two tests, and nothing in the PC hands a cartridge address to `pc_n64_ptr`), but nothing
in this game needs more than 256 MiB: the whole cartridge is 32 MiB. Each size from 16 to 256 MiB uses the same
rules, and 64 and 256 are the ones tested (section 6).

## 3. Addresses past 16 MiB

History: since [NATIVE.md](NATIVE.md) step 12 the game gives the renderer host pointers only, and its heap is no N64
address, so none of this applies; the renderer keeps the microcode's segments, an address below 256 MiB naming a
segment a list gave a base. Before:

The RSP reads an address below 0x80000000 as a segmented one: the segment's number in bits 24-27, an offset in the
low 24 bits. On the N64 every physical address fits the offset (RDRAM is at most 8 MiB), so a physical address is
segment 0's offset, and the game's display-list relocations keep 24 bits of the sum. In a window of more than 16 MiB
a physical address past 16 MiB has bits 24-27 set and would read as segment 1-15. The PC never gives the RCP such an
address: past 16 MiB it gives the K0 address, which the renderer and the audio interpreter take as they take a
physical one. Below 16 MiB, and at every address at `original`, each rule is the N64's (none of the conditions below
can be met when the window is 8 MiB):

| Site | The N64's rule | Past 16 MiB on the PC |
|---|---|---|
| (`osVirtualToPhysical`: the colour and depth images, `StartGraphics.c`, `ClearScreen.c`. Gone since [NATIVE.md](NATIVE.md) step 4: the game gives the renderer the buffers' pointers) | `addr & 0x1FFFFFFF` | (was the K0 address) |
| (`prepare_model_dl`: SetupWaveModel's relocation of a model's display lists: G_VTX, G_DL, the matrices, the image pointers, the LOD markers' branches. Gone since [NATIVE.md](NATIVE.md) step 10: the WAD converter builds the lists with host pointers when the file loads, wherever the file lies) | `((w1 + base) & 0xFFFFFF) \| segment` | (was `pc_rcp_reloc`, `rcpaddr.c`: the K0 address of `w1 + base` when the segment is 0) |
| (`relocate_texture_loads`: the TXLD lists' SETTIMG and BRANCH_Z, rebased by the IMAG chunk. Gone since step 10, as the line above) | `(w1 & 0x0F000000) \| ((w1 + base) & 0xFFFFFF)` | (was the same, `pc_rcp_reloc`) |
| (`display_list_traverse`: the mirror pass and the game's other walks of ROM lists, a G_DL or BRANCH_Z target. Since step 10 the target is a pointer) | bits 24-27 set: segmented, taken as segment 0 (`&= 0xF0FFFFFF`); else physical, made K0 | (was: a K0 address past 16 MiB is not segmented, `pc_rdram_k0_high`) |
| `sound_hw_translate_address` (`hardware.c:1956`: is a sample bank's address RAM, or a cartridge offset to convert?) | RAM if its top byte is 0x80 (or 0xB0, the cartridge) | a K0 address in the window past 16 MiB is RAM too (a guard: this game's banks are cartridge addresses, section 6) |
| `pcgfx_resolve` (`gfx_rsp.c:79-90`) | physical with no segment base: `rdram(a & 0xFFFFFF)` | a physical address past 16 MiB whose segment has no base, or past 256 MiB, in the window: that RAM (outside the window `pc_n64_ptr` is NULL and the old rule stands) |

Since step 10 no display list holds an address the game relocated, so what is left past 16 MiB is the renderer's rule,
for code that strips K0 itself: none was found (section 7), so it is there in case. The one physical address past
16 MiB that would still be ambiguous is one whose bits 24-27 name a segment with a base: the game sets only segment 0
(to 0, `StartGraphics.c:174`).

## 4. Seeing the heap

**The exit line.** At the end of every run, beside the pools' line:

    rush2049pc: heap: peak 920896 of 6838720 bytes, free-low 5917824 (largest block 4194272), osMemSize 8 MiB, the
    heap from 1549888 bytes in; 416 allocations, short 0, none 0

(the synthetic race, `original`, 2026-11-12; "from 1549888 bytes in" is `PC_HEAP_OFFSET`, where the N64 had the heap's header in
RDRAM: the line gives sizes and offsets, never where the block lies in this process). `peak` is the most the heap held, `size - free-low`: everything not in a free
block, so the heap's own header, the handle tables and the blocks' 32-byte headers count. `free-low` is the least the
heap had free, as the game sums it (`heap_total_free`), and `largest block` the largest free block at that
moment: the allocator is first-fit, and a block is what one load needs. `allocations` counts the blocks taken,
`short` the game's fallbacks below, `none` the allocations no free block could hold. With RUSH2049_TEST_HEAP_FLOOR
the line ends `(test floor at 17 MiB)`, counted as the N64 counted RDRAM.

**How.** `port/ultra/heap.c`. The game's allocator calls it under the heap's lock: when the heap is made
(`pc_heap_created`, `InitMemory`, `memsys/group.c:230`) and after each block it takes (`pc_heap_allocated`:
`AllocateMemory` `:82`, `allocate_memory_at` `alloc_at.c:82`, a growing `ReallocateMemory` `heap_release/group.c:178`).
Each call walks the default heap's blocks (`gMainHeap`; a sub-heap's own blocks are inside one of its blocks) and keeps
the least free. The walk reads the headers as include/game/heap.h has them (host pointers in the N64's sizes,
[NATIVE.md](NATIVE.md) step 6), is bounded and checks that each header lies in the heap, in order, with its magic. Frees
aren't followed: they can't lower the least free. The cost: a race takes a few hundred blocks from the start (the
synthetic one 416, the retail one fewer), and the runs of section 6 took as long with the walks as without.

**The game's fallbacks, logged.** The game checks the heap in two places, and falls back silently:

| Where | The check | The fallback | The log line, once |
|---|---|---|---|
| LoadCars, a drone's car (`loadcars/LoadCars.c:87`) | `heap_total_free(0) < gWaveSizes[88 + type] + 512` | another type not loaded that fits, then any type that fits, then a loaded car of an earlier slot ([MORECARS.md](MORECARS.md) 5) | `heap: LoadCars, a drone's car: N bytes wanted, F free: the game falls back ...` |
| LoadCars, a player's car (`:108`) | the same | any type that fits, then slot 0's | `heap: LoadCars, a player's car: ...` |
| a car's paint (`car_palette_init.c:62`) | fewer than 512 bytes free | another car's paint of the same type | `heap: a car's paint: ...` |
| `AllocateMemory` (`memsys/group.c:60`) | none: the game takes the first free block that fits, and none fitting leaves it a null block, which the N64 game dereferences | (the game faults, as on the N64) | `heap: no free block of N bytes (F free, the largest L): the game takes a null block, as on the N64; game.memory gives it more` |

Each logs the first time and counts in the exit line's `short` and `none`. The choice is the game's: nothing it does
changes (the hooks are `PC_ONLY` statements on lines the functions already have; `n64_neutral` passes).

**The overlay.** About's Memory row: osMemSize and the heap's peak so far ("64 MiB; heap 3.5 of 63.5 MiB at most in
use").

## 5. Using the room

**What the game does with spare heap.** Only what section 4's table says. The game sizes nothing from its memory:
the heap's end is the one place osMemSize is read, and the free total is asked for in the two places above (every
call of `heap_total_free` in the game C). The sound banks, the MusyX DMA caches and voices, the frame buffers,
the per-frame lists and the matrix pools have fixed sizes, or sizes the Expansion Pak flag picks (the sound
manager's fx tags: 80 with it, 16 without, [MORECARS.md](MORECARS.md) 10.4), and the flag is set at every size;
nothing caches files by the room left; heap compaction (`ClearMemoryHeap` = ClearMemory) runs where InitWaves calls
it (`InitWaves.c:92`), not when the heap is short. So at the N64's 8 MiB:

- **Races** fit with room to spare. On the retail cartridge at `original` (the heap's 6.5 MiB), a one- or two-player
  race or a battle holds about half of the heap at its peak, a stunt about a third, and the least free is one block
  of about 3 MB (the retail runs of section 6, read from the exit line). The synthetic race's peak is 0.9 MiB.
- **LoadCars' fallbacks** happen only when the car files don't fit: without the Expansion Pak, or with files larger
  than the cartridge's. On the retail cartridge without the pak, a race of 12 cars shares car types four or five
  times and leaves the 4 MiB heap a few KB free (section 6). With the car files each 1 MB larger (`membig8`, at
  `original`) the heap is short for a drone's car once and LoadCars shares a type; with `memory = 64` and files 3 MB
  larger (`membig`) no car is refused and the heap's peak is 18.9 MiB.
- **A finding: the free total, not the largest block.** LoadCars compares the heap's free total with the file's size,
  but a file needs one block. At 8 MiB the heap's free space is split by the runtime images' block at 0x8038A400 (2 MiB
  in; `PC_RUNTIME_IMAGE_OFFSET` past the heap's header), so with car files 3 MB larger the check passes (2.98 MB free in
  all) while the largest free block is 1.79 MB: the allocator finds no block, and the N64 game would fault at the null
  block, as the PC does (with the log line first). With more memory the largest block is the heap's whole rest.

**What a "mods" feature (files from a mods folder in place of the cartridge's, possibly larger) can rely on:**

- **The path.** A wave's file is loaded by LoadWave (`loadcars/loadwave.c`): `AllocateMemoryHeap` or
  `AllocateHandleHeap` of `(gWaveSizes[wave] + 31) & ~31` bytes (`gWaveSizes`, boot data filled from the ROM's file
  table), then `_ReadWave` (`waves/waves.c`) reads `gWaveSizes[wave]` bytes from the ROM offset
  `wave_rom_offsets[wave]` by the wave's kind (`wave_storage_kinds`: 0 raw PI DMA, 1 LZSS, 2 inflate), the converters take the
  block (`pc_wave_loaded`, [ASSETS.md](ASSETS.md) 5), and SetupWaveModel relocates it. A larger file needs its size in
  `gWaveSizes[wave]` before the load; the block, the read and the conversion follow it. RUSH2049_TEST_WAVE_GROW
  (port/main.c) does exactly that for tests: `membig` loads six car files 3 MB larger than the cartridge's through
  this path at `memory = 64`.
- **No 8 MiB limit is left on the way.** PI DMA copies any size anywhere (`pi.c`); inflate's room is the heap's end
  from its destination (`pc_heap_room`; 8 MiB for a destination outside the heap); LZSS has no limit; the big-endian
  registry takes any range; the converters take the block's size; the renderer and the audio interpreter take host
  pointers.
- **The room.** At `memory = 64` the heap has 62.5 MiB, 56 more than the N64's. The exit line's `free-low` and
  `largest block` say what a run left; About shows the peak while playing. A file must fit one free block (the
  finding above): with a large heap the free space is one block past the runtime images'.
- **What it must still do itself.** Read its files (a mods loader stands in for the ROM read of that wave, e.g. in
  `pc_rom_copy`'s caller for the wave's offset, or with its own read in `_ReadWave`'s PC side) and set the size;
  `gWaveSizes` is the N64's u32 table, so a file is at most 4 GiB in principle and the heap's room in practice.
  LoadCars' checks see the larger sizes and fall back as they do; at `original` a larger file can trip the free-total
  finding above.

## 6. Tests and proof

**The tests** (each in ctest, so in every CI job that runs ctest):

| Test | What |
|---|---|
| `frontend.memory` | `game.memory`: the default `original`, 16-256 each set and read back (16, 17, 32, 64, 100, 128, 255, 256), 15, 8, 4, 0, -1, 257, 512, 1024, `64M`, words and an empty value refused with the range in the message, any case, `--set`, the file written and read back, a bad line skipped, the warning only for `expansion_pak = off` with a size |
| `frontend.menu.live`, `frontend.menu.pages` | the Game page's Memory: original to 64 and 256, wrapping, Defaults; About's Memory row |
| `ultra_selftest_memory64`, `_memory256` | the self-test at 64 and 256 MiB: the heap's block osMemSize less `PC_HEAP_OFFSET`, its first byte at the N64 heap's place modulo 64 KiB, its last byte writable, osMemSize (and at 8 MiB, `ultra_selftest`). Until [NATIVE.md](NATIVE.md) step 12: the window's last byte through K0, K1 and physical |
| (`gfx.window.highmem`) | until [NATIVE.md](NATIVE.md) step 12, gfx_test on port/ultra's own window: a scene at 1, 18 and 63 MiB at K0 and physical addresses, each picture byte for byte the first's. Gone with the window: the renderer takes host pointers |
| `audio_window` | (until [NATIVE.md](NATIVE.md) step 9, which took the interpreter's N64-form records away) test_forms' scene (PCM16 big-endian, ADPCM with a big-endian book, native PCM16, the delay effect) as N64-form records and samples in that window, at the same three places and both kinds of address: every render, the state and the delay line the first's; the scene is loud enough to prove it |
| `assets_wad_game` | (until [NATIVE.md](NATIVE.md) step 10) the game's own WAD loader with the WAD at 0x81200000 too, its relocated words K0 addresses there; since step 10 the lists hold host pointers wherever the file lies, and the test runs in ordinary host memory |
| `assets_heap` | `port/ultra/heap.c` on the game's own heap: its size, the least free as the game sums it, the allocations counted, a fallback counted; at 32 MiB with the floor at 17 MiB, the game's next blocks at 17 MiB (heap.c's report: 0x81100000), the runtime images' block taken and locked where `allocate_memory_at` finds it. The heap is in the tests' memory, a host block at no fixed place, at the N64's offsets ([NATIVE.md](NATIVE.md) step 11) |
| `synth_boot_mem64` | drive.press at `memory = 64`: the heap to the end of the 64 MiB (`tools/port/heap_check.py --memory 64`) |
| `synth_boot_memhigh`, `memhigh2p` | drive and race2p at 64 with everything the game allocates above 17 MiB (RUSH2049_TEST_HEAP_FLOOR=17): its files, their lists, textures and vertices, the frame and depth buffers, the sound's buffers |
| `synth_boot_membig`, `membig8` | the car files 3 MB larger at 64 (no car refused; the peak 18.9 MiB, files past 16 MiB), 1 MB larger at `original` (LoadCars falls back once, logged; the race runs) |
| `synth_shot_same_mem64`, `_memhigh`, `_membig`, `_memhigh2p` | their screenshots byte for byte drive's and race2p's |
| `synth_memory_same_<path>` | 11 paths (drive, race2p, race4p, demo, battle2p, stunt, circuit, pause, raceend, race12, race2p12), a test each, at `original` against `memory = 64` with the floor at 17 MiB: the same trace (`synth_compare.py --set-b ... --env-b ...`); an ASan or TSan build runs drive, race2p and race12 (all 22 runs under ASan on CI's arm runner took more than the job's 90 minutes) |

**Each fails without its fix** (build host, each fix taken out alone in a copy of the branch; 2026-11-03: the fixes and
the tests of the window are gone with it since [NATIVE.md](NATIVE.md) step 12):

| Fix taken out | Fails |
|---|---|
| `pc_n64_ptr` at 8 MiB (until step 12) | `gfx.window.highmem` (no RAM at 0x81200000), `audio_window` |
| the renderer's physical rule (until step 12) | `gfx.window.highmem` (the renderer runs off into the wrong RAM and faults) |
| `pc_rcp_reloc` with the N64's 24 bits | `assets_wad_game`: the relocated VTX, G_DL and SETTIMG words 16 MiB low |
| `display_list_traverse`'s N64 segment test | `assets_wad_game`: the called list's and the branch's windings not flipped |
| `sound_hw_translate_address`'s test of a K0 address past 16 MiB | nothing: on the retail cartridge, `drive` at 64 MiB with the floor gives the original's sound byte for byte without it too. The game hands that function its sample banks' cartridge addresses (the samples stay in the cartridge and stream through the MusyX DMA caches, which are in the heap and reached through the interpreter), so the test is a guard for a bank loaded into RAM, which this game doesn't do |

**The proof.** "Base" is a build of 0d7d016 (main before this work).

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, base against the branch at `original`, Debug, build host (x86-64, GCC 14): every path the base has (25) | the same trace, every run: 25 of 25 (48.5 s and 48.4 s of runs) |
| the same, Pi 5 (aarch64, GCC 12) | 25 of 25 (98.6 s and 92.6 s) |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run, each reaching its state: 6 of 6; the six took as long with the branch as with the base (their totals 2% apart) |
| `data.c`, `data_symbols.txt`, the synthetic cartridge | the same files as the base's (build host and Pi) |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 272 of 272 each (17 new; `assets_rom` skipped: no ROM there) |
| `n64_neutral.py --base 09da2f0` | passes (1,015 files: 92 code-exact, 923 shifted) |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, aa28919) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check`, build host | no open findings, base and branch; offset_check 59,463 accesses (5 more, the new statements), layout_check 48,208; endian_check's 2 open value conflicts in 1 storage and 3,525 puns, as the base's |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`; the base's counts |
| `memory = 256`, the most, with everything the game allocates in the window's last 6 MiB (RUSH2049_TEST_HEAP_FLOOR=250: from 0x8FA00000) against `original`: the 11 paths of `synth_memory_same`, RelWithDebInfo, build host | the same trace, 11 of 11; race2p's screenshot byte for byte the same; the exit line's heap to 0x90000000 |
| Windows cross build (Pi, mingw-w64 12, fresh configure, SDL2 fetched) | builds, `gfx_window_test.exe` and `test_audio_window.exe` with it; gen_data's check of the PE: OK, 26 decoupled, 1 reserved in the window. (It first failed to link two asset tests: weak definitions, which mingw's linker doesn't bind, and `setenv`; fixed in 87d781e and 00b5001) |
| ASan + UBSan, aarch64 (Pi 5, `-DRUSH_SANITIZE=address,undefined`, CI's arm sanitize job's build): the self-test at 64 and 256 MiB, `gfx.window.*`, `audio_window`, `assets_heap`, `assets_wad_game`, `frontend.memory`, the synthetic `mem*` paths and their screenshot comparisons (21 tests) | all pass, no ASan or UBSan report: the window of 64 and 256 MiB at 0x80000000 is under ASan's shadow, which covers everything below 64 GiB there |

**On the retail cartridge** (Pi 5, `--fast`, one game at a time, the branch's Debug build; the press scripts, logs,
screenshots and sound in the ignored `build/scratch/`; tracks 4 and 5 are locked on a fresh profile, so for them gdb
set the two players' unlock bytes (`gUnlockedTracks`) as the track select first asked, as MORECARS.md 10.4 did):

| Run | Result |
|---|---|
| `memory = 64`, `cars = 12`: each race track, 0-5, one player and 11 drones, then two players and 10 drones, 60 s of racing with gas (12 runs) | each reaches its state with 12 cars, the count 12, places 0-11 once each, every car moved (`cars_check.py`); no fault; the exit line: a heap of 62.5 MiB, about 4 MB at its peak, no fallback, nothing refused |
| the same race at `original` (track 0, one player, 12 cars) | the same state at the same retrace, the first car at the same place to the bit; the exit line: the N64's 6.5 MiB heap, a little more than half of it at its peak |
| the branch at `original` against itself at `memory = 64` with everything the game allocates above 17 MiB (RUSH2049_TEST_HEAP_FLOOR=17): `drive`, `race2p`, `demo`, `raceend`, `screens` | the same trace (`synth_compare.py --set-b --env-b`), 5 of 5: the races, the attract demo, the race to the results and back to the main menu, the front end's screens |
| the same two programs, `drive` and `race2p`, with a screenshot and `--dump-audio` | the screenshots and the whole sound (5 MB and 4 MB of WAV), byte for byte the same: every model, texture, frame buffer, sound bank and voice drawn and played from past 17 MiB |
| `memory = 64`, 12 cars, two players, track 0, with the floor at 17 MiB | the same state, the first car at the same place as without the floor; the heap's peak past the floor |
| `memory = 64`: the attract demo of 12 (`demo12`), the front end (`screens`), a race retired to the results, the game over screen and back to the main menu (`raceend`) | each reaches its state, the first car where `original` has it |
| without the Expansion Pak (`original`, `expansion_pak = off`: 4 MiB), 12 cars: tracks 0 and 2 for one player, track 0 for two | each reaches its state with 12 cars; LoadCars finds the heap short for a drone's car four or five times a race and shares a loaded type (the log line, the exit line's `short`), and the heap's least free comes down to a few KB. This answers [MORECARS.md](MORECARS.md) 11's "whether LoadCars fell back" |
| `expansion_pak = off` with `memory = 64`, 12 cars | the warning; the game uses its Expansion Pak features, and nothing is short |

## 7. The address inventory

History (the tree of 2026-11-03): most of these places went with [NATIVE.md](NATIVE.md) steps 4, 10 and 12, the window,
`pc_n64_ptr` and the renderer's rule among them. Every place that holds, masks or compares an address or a size against
the N64's 4 or 8 MiB or its 24-bit offsets,
found by searching port/ and the game C (every file the PC compiles: src/blob, src/ovl_a, src/ovl_b and the src/rom
units the PC doesn't replace) for 0x7FFFFF, 0x3FFFFF, 0xFFFFFF, 0x00FFFFFF, 0x1FFFFFFF, 0x0FFFFFFF, 0x0F000000,
0xF0000000, 0xFF000000, 0x80400000, 0x80800000, 0x00800000, 0x00400000, `PC_RDRAM_SIZE`, `osVirtualToPhysical`,
`osMemSize` and fixed addresses 0x804xxxxx-0x807xxxxx, and reading each:

| Site | What | Verdict |
|---|---|---|
| `memory.c:52-64, 88-99, 134-168` | the window's size, its two views (memfd; the Windows section; the fallback), the LSan root | follows `game.memory` |
| `memory.c:186-206` | `pc_n64_ptr`, `pc_n64_addr` | follow the window's size |
| (`memory.c`) | `osVirtualToPhysical`, `osPhysicalToVirtual` | gone since [NATIVE.md](NATIVE.md) step 4 (no caller: the images are pointers); were K0 past 16 MiB (section 3) |
| `misc.c:59-84` | osMemSize | `original`: 8 or 4 MiB; a size: the window's |
| `misc.c` `__osTLBLookup` | `vaddr & 0x1FFFFFFF` | no caller in the PC build |
| `pi.c:127-129` | `rom_offset`: a device address 0x10000000-0x1FBFFFFF is the cartridge | device addresses, not RAM: right |
| `pi.c:170-176`, `:258-266`, `:277` | PI DMA, inflate from the ROM, LZSS into any destination | no limit but inflate's |
| `pi.c:229-237` | inflate's output room | was 8 MiB from any destination; now the window's end from a window destination, 8 MiB from a host one |
| `pc_mem_register_be` and its users | the big-endian registry | any range: right |
| `gfx_rsp.c:45-47, 62-69` | `rdram`, `pcgfx_segment_value` | `phys & 0x1FFFFFFF` through `pc_n64_ptr`: the whole window |
| `gfx_rsp.c:79-90` | `pcgfx_resolve`'s physical and segmented addresses | section 3's rule past 16 MiB |
| `gfx_rsp.c:878` | `pcgfx.geom & (w0 & 0x00FFFFFF)` | geometry mode bits, not an address |
| `gfx_interp.c:362`, `gfx_tex.c:60-66`, `gfx_api.c:419-450` | the high frame rate's signature, texture names, `RUSH_GFX_DUMP` | host pointers, content hashes, GL targets: no window address kept |
| `gfx_api.c:153, 575, 725` | the render targets, `pc_gfx_present`, screenshots | through `pcgfx_resolve`: any window address |
| `musyx.c:95-108` | the audio interpreter's addresses (SFD, voices, samples, books, outputs, the delay line) | `pc_n64_ptr`: the whole window |
| `musyx.c:595-606` | `RUSH2049_AUDIO_CAPTURE`'s capture arena (0x80010000-0x80400000) | the capture file's own address space, for the microcode check: unrelated to the window |
| `font.c:21` | a font block in the window | the largest window's range |
| `wad.c:213` | a TOC already relocated (bit 31) | any K0 address: right |
| `pc_data.c:289, 456` | the loader's "an RDRAM address" for pointer values in ROM data, and the address-word scan | N64 values in the ROM's data, where 0x80800000 is the N64's end; a larger value is kept as it is either way: right |
| `datacheck.c:643` | the file table's sizes under 8 MiB | a check of the ROM's own table |
| `gen_data.py:83`, `gen_stubs.py:101`, `n64syms.py:73` | the arenas' and absolute symbols' range, the overlay region | the N64 images' layout, inside the first 8 MiB at any size |
| `absfix.c` | GOT slots of absolute symbols: addends at 0x80000000 or more | the symbols are the N64's: right |
| `memsys/group.c:198-216` (`locate_main_heap`), `heap.c` (`pc_heap_memory`) | the heap's end `osMemSize \| 0x80000000` (the window's bytes to osMemSize); the Expansion Pak flag past 4 MiB (the N64's end past 0x80400000) | follows osMemSize (section 1) |
| `prepare_model_dl.c:107, 111, 129, 134`; `relocate_texture_loads.c:42, 47` | 24-bit relocations | `pc_rcp_reloc` (section 3) |
| `waves/group.c:3854-3887` | a second `prepare_model_dl`, 24-bit | a context copy for IDO's optimizer, weak on the PC: the linked one is `src/blob/prepare_model_dl.c` |
| `display_list_traverse.c:33, 52` | segmented or physical | section 3 |
| `mirror_vertex_texcoords.c:3808` | the vertex pass, `pc_n64_ptr(arg0 \| 0x80000000)` | K0 or physical: right |
| `hardware.c:1956` (`sound_hw_translate_address`) | RAM if the top byte is 0x80 | section 3 |
| `hardware.c:1735`, `streams.c:486` | RAM if the top nibble is 8 | right up to 256 MiB (section 2) |
| `musyx_cart_address.c:11` | `(a & 0x0FFFFFFF) \| 0xB0000000`: the cartridge address | cartridge, not RAM |
| `registry.c:664`, `macros.c:968`, `streams.c:249`, `decode.c:74` | `& 0xFFFFFF` | a sample's length, a frequency, offsets in a stream table: not addresses |
| `flip_triangle_winding.c`, `displaylist_set_texture_image.c`, `displaylist_get_texture_image.c`, `InitTextureAnimation.c`, `TextureAnimation.c` | the other walkers of ROM lists and TXLD | opcodes only; the addresses they copy are the relocated words |
| `waves.c` (SetupWaveModel), the loaders' `+= base` | 32-bit sums of K0 addresses | right at any address |
| `loadcars/LoadCars.c:87, 108`, `car_palette_init.c:62` | the heap's free total | real sizes: right (section 5's finding stands as on the N64) |
| state, flags and colours: `0x400000`, `0x800000`, `0x7FFFFFFF` in `gGameState`, node flags, `AnimID`, `gDPSetCombine`'s words | not addresses | |

## 8. Not done, or not tested

- Sizes past 256 MiB (section 2): the two MusyX tests and the KSEG1 overlap would need changes, for no use in this
  game.
- Windows: the cross build compiles (section 6), but nothing ran on Windows: the heap's block committed at 256 MiB
  (`VirtualAlloc`, top-down), and the new tests there (CI's windows jobs run them).
- UBSan on x86-64 (CI's other sanitize job): the build host has no UBSan runtime, so it ran only on aarch64, with ASan.
- A host other than Linux and Windows: not built; the heap's block maps anywhere, so nothing in the memory model
  stands in the way since step 12.
- Finishing a race at `memory = 64` on the retail cartridge: the 12-car races ran 60 s each, and `raceend` retires a
  race to the results; MORECARS.md 10.4's races to the finish line were at `original`.
- `expansion_pak = off` with a large heap and the pak's features off: not offered (section 1).
