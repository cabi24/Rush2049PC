# Native memory: the game out of the N64's RDRAM window (wave 3)

Written 2026-11-09, on 254c4b01, as the plan for wave 3 of the PC-only work ([PCONLY.md](PCONLY.md) 4, item 3). The
user's words: "No more memory address referrals from the n64 architecture. PC native the things." This page is the
inventory, what depends on what, the steps, and the measuring tool that shows each step's progress.

**The end state.**

- No N64 address in the running game or the port. N64 addresses exist only as a format inside the user's ROM (the
  cartridge images' link addresses, pointers in the ROM's data, the files' relocations), and two parts of the port
  read that format at load time: the data loader (`port/data`) and the asset converters (`port/assets`). What they
  hand the game holds host pointers.
- Nothing is mapped at 0x80000000 or 0xA0000000: no RDRAM window, no GOT fix-up, no `MAP_FIXED`. AddressSanitizer runs
  on x86-64 and macOS arm64 can run the game ([SANITIZE.md](SANITIZE.md) 1.1, [NOFIXED.md](NOFIXED.md)).
- The game's data are ordinary C objects with native pointers; its heap is a block of PC memory that the game's own
  allocator manages with native pointers; the runtime images are PC code and data, reset by the loader.
- Files loaded from the ROM become native records and native display lists, with host pointers, when they load.
- The game plays exactly as before at default settings (the gate, [PCONLY.md](PCONLY.md) 2).

| Piece | Where |
|---|---|
| The census: what is still tied to the window, by kind | `tools/port/window_census.py` (section 1), its test `tools/port/tests/test_window_census.py` |
| The crossing counter and the arena guard: what still crosses, by site, along every path | `cmake -DRUSH_COUNT_CROSSINGS=ON`, `tools/port/crossings.py` (section 9) |
| The window today | [ULTRA.md](ULTRA.md) 2, [DATA.md](DATA.md) 3, [MEMORY.md](MEMORY.md), [NOFIXED.md](NOFIXED.md) |
| The experiments behind this plan | section 2; scratch code on Rocky in `~/rush2049pc/w3-plan-exp` (not in the repository) |

## 1. The census

```
python3 tools/port/window_census.py [--build build/pc] [--no-compile] [--report FILE] [--sites KIND,...] [--json F]
    [--baseline F] [--write-baseline F]
```

One line per kind, always the same kinds in the same order, so two runs compare line by line; `-` where the source
isn't there. The docstring says where each count comes from: the generated data tables (`data_layout.json`,
`aliases.ld`), `nm` over the objects, the sources with comments and strings told from code, the compiler pass of
`nofixed_audit.py` (casts and implicit conversions between pointers and integers), and the loader's report.
`--sites KIND` lists every site; `--baseline` fails when a kind grows (CI can hold the line as it does with
`nofixed_audit.py`). ctest runs it on a made-up tree (`window_census_tool`) and over the build without the compiler
pass (`window_census`). The goal is every count at 0 but the `context.` ones.

The counts on 254c4b01 (Rocky, Debug, with the compiler pass; the `runtime.` lines from the synthetic cartridge's
report):

| Kind | Count (files) | What it is |
|---|---:|---|
| `data.arena.boot` / `game` / `ovl_a` / `ovl_b` | 192 / 970 / 430 / 48 | names placed at their N64 address in an arena (data.c's absolute `.set`s) |
| `data.inside_own` | 12 | interior labels: names at an offset inside another datum's storage |
| `data.reserved` | 1 | `pc_matrix_spill`, PC data placed in the window because a 32-bit address of it is kept |
| `data.window_aliases` | 4 | `osTvType`, `osMemSize`, `game_bss_end`, `overlay_base` (aliases.ld, absolute) |
| `data.by_name.game` / `port` | 609 (609) / 4 | objects that reach a name placed in the window, by name |
| `conv.PC_N64P.game` / `PC_N64A` / `PC_N64W` / `PC_SLOT` | 27 / 4 / 10 / 0 | the prelude's translations used directly in game C |
| `conv.file_macros.defs` / `uses` | 157 (87) / 497 (89) | the files' own conversion macros (`N64P`, `N64W`, `N64A`, `N64PTR`, `CURVE`, `DATA` ...) and their uses |
| `conv.pc_n64_ptr.game` / `port` | 3 / 32 (10) | calls of `pc_n64_ptr` / `pc_n64_addr` |
| `conv.virt_to_phys.game` | 4 (2) | `osVirtualToPhysical`: the colour and depth images |
| `conv.silent.trunc` / `zext` | 3 / 22 (8) | casts through `uintptr_t` the compiler doesn't flag |
| `conv.cc.ptr_to_int` / `int_to_ptr` / `implicit` | 35 (12) / 8 (1) / 85 (41) | the compiler's conversions (most in `hardware.c` and `sequencer.c`) |
| `fields.link_enum` / `link_enum_types` | 87 (32) / 62 (32) | 4-byte N64 address fields typed with a link enum (`enum n64p_tTune`, `enum ChunkLink`) |
| `fields.u32_address` | 38 (19) | integer fields that hold a window address: by their comment, or a loaded file's address fields by name |
| `fields.statll_n64` | 13 (4) | the StatLL lists' N64-linked form (the level-object nodes) |
| `api.u32_address` | 19 (6) | prototypes taking a 32-bit address: the heap's API, the WAD and display-list helpers |
| `lit.address.game` / `region` / `k1` / `hw_register` / `mask` / `reloc` | 8 / 2 / 1 / 8 / 6 / 7 | literal window addresses, region tests, the cartridge's KSEG1 base, `rom_tu.h`'s registers, `& 0x1FFFFFFF`-style masks, 24-bit relocations |
| `lit.window.port`, `n64map.uses.port` | 4, 115 (19) | the PC layer's literal window addresses and its uses of `pc_n64_map.h` |
| `heap.window_symbols` / `allocate_at` / `ovl.image_block` | 8 / 1 / 4 | the heap's start and end and the runtime images' block, by N64 address |
| `port.below_4g` / `n64_reads` / `mapping` / `rdram_base` | 12 / 13 / 21 / 51 | the "below 4 GiB is an N64 address" rule, the renderer's and audio's RDRAM reads, the fixed mappings, `PC_RDRAM_*` |
| `tests.window` | 75 (20) | test programs with windows of their own |
| `be.accessors.game` / `be.ranges` / `be.rgba.game` | 19 (10) / 39 (9) / 10 (4) | big-endian ROM data read in game C, the big-endian ranges, colour words in the N64's register order |
| `text.window.game` | 237 (139) | comment lines that speak of window, K0 or physical addresses |
| `runtime.undescribed` / `reached` (synthetic) | 288 / 5 | pointers in ROM data into arena bytes no symbol describes; tables only pointers reach |

Not counted: N64 addresses inside the ROM's data and the loader's symbol tables (`asm/us/*/symbols.json`), which
describe the cartridge, and the cartridge's bus addresses (`0xB0000000 | offset`, `context.cartridge_aliases`):
section 7.

## 2. The experiments

Three scratch builds on Rocky (`~/rush2049pc/w3-plan-exp`, not committed), all along the 63 synthetic paths.

**A probe of every crossing** (`build/probe`). The prelude's `PC_N64P`, `PC_N64A`, `PC_N64W`, `PC_SLOT_N64/SET`
counted by file and line, and `pc_n64_ptr`, `pc_n64_addr`, the renderer's `pcgfx_resolve` and
`pcgfx_segment_value`, and the audio interpreter's address reads counted by caller, each value classed by where it
points (an arena, the heap, the image block, low memory, physical, a host pointer). The program is the same with the
probe: 63 of 63 paths `same`, screenshots included. What runs:

| Crossing | Live sites | Crossings | Where they point |
|---|---:|---:|---|
| `PC_N64P` | 119 | 32.1 M | game arena 23.2 M (the car descriptors, `gTune`, `stdtorquecurve`, `frontTire`, `gMatrices`, `gTargetArray`), heap 8.8 M (the heap's own headers, the road's tree nodes, WAD records, font handles), runtime images' arenas 6 k (two palettes) |
| `PC_N64A` / `PC_N64W` | 58 / 31 | 1.7 M / 2.0 M | the heap almost entirely; into arenas: the sky matrices, the car's descriptor, tune and torque curve, the target nodes, the two palettes |
| `PC_SLOT_*` | 7 | 1.7 M | heap handles (font data, wave slots) |
| `pc_n64_ptr` (port) | 8 | 18.2 M | the renderer, the port's heap walker, the audio interpreter, the mirror pass, the world's chunks, low memory |
| renderer addresses | 8 | 22.0 M | host pointers 7.1 M; physical 6.9 M (the ROM lists' relocated vertices and the colour and depth images); K0 in the heap 6.3 M (relocated display lists, texture images, the frame buffer); K0 in the game arena 1.6 M |
| audio interpreter | 3 | 1.6 M | heap 1.0 M, null 0.6 M: every one a native pointer field holding a heap address; no N64-form record |

The game-arena addresses the renderer sees (`cull_box_vertices_a`, `default_lights`) are `(uintptr_t)symbol` in
native lists: the data's own address, which becomes a host pointer by itself when the data moves. The audio
interpreter's are native pointer fields too. So the renderer and the audio interpreter get N64 addresses from
two places only: the ROM's display lists (relocated in place with 32-bit sums) and `osVirtualToPhysical`. Everything
else they get is already a pointer that happens to point into the window.

**Every datum in its own storage** (`build/allown`: gen_data with every arena symbol given own storage, as
`decoupled.txt` does for one). Every path stops at once: `InitRushControls` walks `gNewControls` until it reaches
`&gFrontDistance`, the next symbol on the N64, which own storage's padding never meets. With that walk ended by the
array's own size, the front end faults in `buildTrackMap` (the track map's buffers). The shared headers declare most
arrays open-ended (`EXTENT(n)` counts only in `port/data/game_decls.c`), so the fix that should be obvious,
`sizeof gNewControls`, doesn't even compile. Splitting the arenas into separate objects is per-datum work that needs a
tool which sees walks (AddressSanitizer's global redzones), not a switch.

**The arenas as host blocks** (`build/blocks`: gen_data puts each of the four arenas in a host block of its own, its
N64 layout kept, every arena name relative to its block; the explicit crossings translate between an arena's N64
range and its block; the window's arena ranges unused). The game runs: 62 of 63 paths `same` against the base with
screenshots and audio; the 63rd differed only in its last audio subframe, which the base's own two runs disagree on
too ([PCONLY.md](PCONLY.md) 2's known variation), and rerun it is `same`. With the window's arena pages mapped with
no access (a fault logged by its instruction, the page opened, every page closed again every 10 ms), not one access
on any of the 63 paths, and every path's trace the base's. The same mode on the normal layout logs 202 instructions
and 9,770 faults on the `menu` path alone, so it sees what it should. The probe in this build: the renderer's 1.6 M
game-arena values became host pointers (8.7 M host pointers, the rest as before) with no change to any list, and
`pc_n64_ptr` sees no arena address.

What they settle:

1. **The arenas can leave the window as whole blocks** with no change to the game's code that reaches data by name
   (609 objects): walks from one datum into its neighbour keep working, because a block keeps the N64 layout.
2. **The only N64 addresses of arena data in use are the explicit crossings** the probe lists, about 50 sites in 25
   files: the car record's descriptor, tune, torque curve and tyre fields, the sky's matrix slots, the level-object
   nodes and their lists, two palettes. Convert those to pointers first and the blocks need no translation at all.
3. **The renderer's "below 4 GiB" test is hit by data pointers only because the data is in the window.** Moving
   the data makes those host pointers with no change to the lists. What must change for the renderer is the ROM's
   display lists and the two image addresses.
4. **The audio interpreter needs nothing new**: its fields are native pointers already; they stop being window
   addresses when the heap stops being the window.
5. **Separate objects per datum come last**, once AddressSanitizer can watch them (section 5, step 13).

## 3. What depends on what

Four things lived in the window when this plan was written (the arenas left it with step 5):

| | What | Who keeps 32-bit addresses of it |
|---|---|---|
| the arenas | the game's pointer-free data and BSS, the runtime images' too, at their N64 addresses; since step 5 blocks of host memory | the car record's u32 fields, the level-object nodes and lists (section 2; the sky matrix slots, two palettes and `pc_matrix_spill`'s users were step 4's; all done, so nothing since step 5) |
| the heap | everything allocated: the loaded files, frame and depth buffers, the audio's memory | the heap's own headers and API (native since step 6); the files' relocated fields and display lists (native beside the file since steps 7, 8 and 10); the road's tree nodes in the car record; the targets' records; MusyX's registries, macro and song pointers and the players' listeners (pointers since step 9); ghosts, palettes; `osVirtualToPhysical` |
| the image block | the heap block `load_overlay` reserves at 0x8038A400, which holds the runtime images' arenas | nothing since step 6: an offset in the heap (`runtime_image_block`); was the overlay loader's `RUNTIME_IMAGE_*`, `overlay_base` |
| low memory | libultra's boot parameters at 0x80000300 (`osTvType`, `osMemSize` ...) | `pc_os_initialize`, the aliases |

So the order is forced:

```
   car records, targets, sky + palettes  ──►  arenas as blocks  ──►  (later) one object per datum, under ASan
   (their u32 fields into arenas)               (step 5)                (step 13)
                                                     │
   the heap's code native (step 6) ────────────┐     │
   files: WAD records, other formats (7, 8) ───┤     │
   display lists native (10) ──────────────────┤     │
   MusyX native (9) ───────────────────────────┼──►  the heap out of the window, the window gone (12)
   the image addresses (4), other holders (11) ┤     │
   the tests' own windows (11) ────────────────┘     ▼
                                     the renderer's and audio's 4 GiB rule go with it
```

- The arenas can't leave while a u32 field holds an address in them: the field can't hold a host pointer.
- The heap is last because almost everything that keeps a 32-bit address keeps one into the heap. It can only
  leave once the files, display lists, audio data and records that hold heap addresses hold pointers.
- The renderer can't stop treating values below 4 GiB as N64 addresses while display lists still hold them: the
  ROM's lists (step 10) and the colour and depth images (step 4, done: they are pointers).
- The image block is heap accounting: it goes with the heap's addresses (step 6 makes it an offset).
- Separate objects per datum need AddressSanitizer to find walks, and AddressSanitizer on x86-64 needs nothing at
  0x80000000 (or the moved build, [NOFIXED.md](NOFIXED.md) 9).

## 4. The design

### 4.1 The arenas: blocks first, objects later

Step 5 gives each arena a host block of its own in data.c (zero-filled, aligned, its size the arena's), and every
name in it is `.set name, pc_arena_game + (address - 0x8010FD7C)`: the same relative form the moved build uses with
`pc_window` ([NOFIXED.md](NOFIXED.md) 2.2), but per arena and outside any window. The loader's tables give each
arena's host as its block; the loader fills it as it fills the window today. Walks keep working (the layout is the
N64's), pointers in ROM data into an arena resolve to the block (the loader's "arena + offset"), the runtime images'
arenas reset on every load as now. The executable's own relocations move the names (ELF, PE and Mach-O alike), so
the GOT fix-up has nothing left to fix for them. The game's objects don't change at all: they reach every name
through the GOT as before (objsame: every unit `same`).

What replaces fixed placement when code walks from one datum into the next: for now, the block keeps the placement.
Step 13 replaces it datum by datum: each walk ends at its own array's end (with complete declarations in the shared
headers, so `sizeof` works) or the data walked together become one struct; AddressSanitizer's redzones around each
object are the check that none is left.

### 4.2 Records with 32-bit address fields: pointers

Every record kept in the N64 layout only because it holds 32-bit addresses (the 87 link-enum fields, the 38 u32
address fields, the StatLL N64 form) gets its host layout: pointer fields, the rest as before. The work per record:
its one declaration in `include/game` (or `pc_game_types.h`), the readers' views (`view_check.py`, `view_audit.py`),
code that steps through it by a hard N64 stride or reaches it by raw offset (`offset_check.py`, `layout_check.py`),
the data loader's layout if ROM data initializes it (gen_data takes the new layout from the declaration), and the
macros that converted (`N64P`, `N64W`) deleted. The gate shows the result is the same game.

### 4.3 The heap: native pointers, the same accounting

Done in step 6 as written here, but for the block itself, which stays where it is until step 12 (section 12).

The game's allocator decides what fits, and so what the game loads and when: its arithmetic must stay what it is at
default settings (the N64's, but for the handle slots, 8 bytes on the PC since POINTERS.md 9.10). It does if each
header keeps its size. A chunk header stays 32 bytes with native pointers:
`next`, `prev` and `master` (8 bytes each), `size` (4), then `used`, `internal`, `locked` and a one-byte magic.
The heap header (MAT3 `tHeap`) must stay within the 64 bytes before the first chunk (`(heap + 67) & ~31`): its four
pointers (`next`, `chunks`, `lastChunk`, `endRam`), its first master table's two, `numMasters`,
`numAdditionalMasters` and the magic fill exactly 64 if the unused `emergencyFunc` goes. A master table's header
(`tMasterBlock`, 12 bytes, 24 with pointers) is a chunk of its own, and a chunk's data is rounded to 32 bytes, so it
takes what it took. Handle slots are 8 bytes already. The heap's start and end are a block of
PC memory sized as today (`osMemSize` less what lies below the heap on the N64, so `game.memory` sizes it as before),
aligned 64 KiB; `expansion_pak_present` tests the size, not `endRam >= 0x80400001`. The image block is reserved at
the same offset from the heap's start as on the N64 (`N64_OVERLAY_BASE - 0x8017A640`, named for what it is), so the
allocator's choices are unchanged. The API (`DeallocateMemory`, `PurgeableMemory`, `FindHeap`, `GetPtrSize`,
`allocate_memory_at` ...) takes and returns pointers; its 42 calling files drop their `N64A(p)`. A few places
compare or subtract block addresses (the compaction's moves, `FindChunk`'s range test, the free's merge with its
neighbours): with one block of PC memory, pointer comparisons there mean what the N64 addresses meant.

The heap's block stays where it is (in the window, at 0x8017A640) until step 12: a pointer into it is a window
address below 4 GiB, so every 32-bit holder not converted yet keeps working while the heap's own code goes native.

### 4.4 Files from the ROM: native records and native display lists

The converters (`port/assets`) run when a file loads (`pc_wave_loaded`), as now. They keep the heap block the game
allocated, with the file's N64 bytes, for accounting and for what stays a file format (texels, TLUTs), and build
beside it, in PC memory, what the game reads as records: OBHD, TXHD, PLHD and PTHD records with pointer fields, the
text bank's string pointers, the font letters, the world's name table, the paths' headers. The relocation the game
did with `+= (u32)base` is the converter's: file offsets become host pointers. The game's tables
(`gRomObjDefPtrs` ...) point at the native records; the hard strides (`* 88`, `* 36`, `idx * 8`) become the record
types. When the game frees the block (`pc_heap_block_freed`, already reported), the converter frees what it built.
A handle file that the heap's compaction moves (the font data) has nothing to relocate any more: the native side
didn't move, so `MBOffsetWad`'s delta goes.

**How a display list carries a 64-bit pointer**: in the PC's own command form. `Gfx` is `{u32 w0; uintptr_t w1;}`
already ([POINTERS.md](POINTERS.md) 2, [GFX.md](GFX.md) "Command formats"), and every list the game builds is in it.
The ROM's lists are the exception: 8-byte big-endian commands with 32-bit addresses. Step 10's converter turns
each into a native list: every command is one native command, opcode for opcode, with `w1` a host pointer for the
address-carrying ones (VTX, DL, BRANCH, the E1/04 pair, MTX, MOVEMEM, SETTIMG) and the value for the rest. A list
offset maps linearly (command i of the chunk is native command i), so the chunk's lists are converted as a whole and
the converter needs no list boundaries; the bytes that are vertices, not commands, are never executed in the native
copy, and VTX points at the vertices themselves, swapped to native byte order (the converter knows them from the
VTX commands it walks from the OBHD entries). The game's walkers (`relocate_model_dl`'s level-of-detail markers,
`display_list_traverse`, `flip_triangle_winding`, `mirror_vertex_texcoords`, the texture animation's four helpers)
then work on `Gfx` and `Vtx` with plain loads and stores; their big-endian accessors and N64 address rules go.
Texels and TLUTs stay in the N64's texel formats: that is a format the renderer decodes (a 16-bit texel load may
carry 4- or 8-bit texels, so swapping them by the texture's declared format would be wrong), tagged by range as
today, and holds no address.

Weighed and not chosen:
- **Segments** (the ROM lists keep 32-bit segment-relative offsets; each model draw sets a segment to its file's
  base): the game's own lists grow by a command a draw (the frame list's room is sized from measured high-water
  marks, [MORECARS.md](MORECARS.md) 8), the texture animation copies image addresses between textures, and the lists
  stay big-endian N64 data.
- **32-bit file references** (a file's slot and offset in the word, resolved through a table): small and safe, but it
  invents an address space of 32-bit numbers that stand for memory, which is what the user asked to be rid of. It
  is the fallback if step 10's renderer comparison can't be made exact.

### 4.5 The audio

The interpreter's input is native already (section 2). What holds 32-bit addresses is the MusyX library itself: the
registries' payloads (`{payload, id, refs}`), the macro interpreter's command pointers (`MacroState*`'s `addr00`,
`curAddr04` ...), the voices' nodes (`pc_voice_node`), the sequencer's song pointers (`(u32)arr + offset`), and
`hardware.c`'s sample addresses. They become pointers; the registry records grow (the library's 8-byte keymap and
macro records, [MUSYX_DATA.md](MUSYX_DATA.md) 4, become structs). The samples stay where they are: in the ROM, read
by offset. The synthetic cartridge has no songs, so the sequencer's paths need the retail cartridge (the lead's
runs on the Pi, with `--audio` captures compared byte for byte). Done in step 9 (section 14), which gave the
`--sounds` cartridge a song.

### 4.6 The runtime images

They are PC code and data already: both images are linked in and a load resets one image's data
([DATA.md](DATA.md) 5). What ties them to the window is their arenas (step 5 makes those blocks) and the heap block
the loader reserves (an offset in the heap since step 6). `RUNTIME_IMAGE_START` and `overlay_base` went with step 6;
`N64_OVERLAY_*` stay while the data loader reads the images' link addresses from the ROM's format.

### 4.7 Low memory

`osTvType`, `osMemSize` and the other boot parameters become ordinary variables port/ultra defines and sets
(`pc_os_initialize`); `game_bss_end` and `overlay_base` went with the heap's code (step 6).

### 4.8 New checks

| Check | What | Built in |
|---|---|---|
| `window_census.py` | the counts by kind; `--baseline` in CI holds the line, each step lowers it | this commit |
| **A crossing counter** (`RUSH_COUNT_CROSSINGS`, a build option) | the probe of section 2 made a tool: every `PC_N64P/A/W/SLOT` crossing and the renderer's and audio's N64 addresses counted by site and target at exit; a step's proof includes "these sites cross 0 times along every path". The scratch probe (`w3probe.c`, `apply_probe.py` in the scratch tree) is the design: a weak hook from the prelude's macros, a table by site, a dump at exit, `addr2line` for the PC layer's callers | step 1: built (section 9) |
| **The arena guard** (`RUSH2049_WINDOW_GUARD=arenas`) | after step 5 the window's arena ranges hold nothing: mapped with no access, any use faults with a note naming the datum and the instruction; after step 12 the whole range 0x80000000-0xBFFFFFFF is reserved with no access in this mode, so a stray N64 address faults wherever it comes from | step 1 (arenas): built in the counting build (section 9); step 12 (all) |
| AddressSanitizer on x86-64 | a CI job (ASan and UBSan, every test, the synthetic paths); its redzones are step 13's proof | step 12 |
| The gate | objsame, synth_compare 64 of 64 `same` (shots, audio; 63 until step 8 added `language`), ctest and the realtime tests, the retail scripts on the Pi | every step |

The moved build (`RUSH_NOFIXED`) and its CI job stay as they are during wave 3: every crossing still translates
there, so a step that removes crossings must keep it building and its paths `same`. Step 12 retires it with the
window it moves.

## 5. The steps

Each step is one agent's, one to three sessions, with files no other step of its wave edits. Sizes: small (under a
session, a few files), medium (a session, 10-30 files), large (two or three sessions, 30-60 files). Every step's
proof is the gate (objsame where code is unchanged, else synth_compare 64 of 64 `same` with shots and audio, ctest
with the realtime tests, the lead's retail scripts on the Pi) plus what the step names, and the census's lines it
targets at 0 (with `--sites` showing what's left of them). Each step updates the pages its subject has (DATA.md,
CARS.md, POINTERS.md, ASSETS.md, GFX.md, AUDIO.md, MEMORY.md, ULTRA.md) and the comments it makes untrue
(`text.window.game`).

### Step 1. The crossing counter and the arena guard (small; port only): done

Done 2026-11-09 (wave 3.0): how to run it, what it counts and the baseline it measured are section 9. The plan was:

`RUSH_COUNT_CROSSINGS` and the guard mode of 4.8. Files: `port/include/pc_prelude.h` (the option's block),
`port/ultra/crossings.c` (new), `port/ultra/memory.c`, `port/gfx/gfx_rsp.c`, `port/audio/musyx.c` (one hook each),
`port/main.c`, `port/data/pc_data.c` (the guard paused while an image loads), `CMakeLists.txt`, a report script.
Proof: objsame `same` (the default build doesn't change); the counting build's paths `same` as the default's (the
probe's were); the guard mode on the normal layout finds the by-name accesses (as the experiment did). Depends on
nothing. Risk: none to the game.

### Step 2. The car record's pointers (medium-large): done

Done 2026-11-09 (wave 3.1), as [CARS.md](CARS.md) 4 describes. What was done:

- The descriptors first (9.5): `steady_leg_duration` is MAT3's `legDuration[2]` (8 bytes), and the table after it is
  `car_descriptors[6]` of MAT3's `_tCar` (`include/game/types.h`, `cars.h`; MAT3's six descriptors at
  0x801108D0..0x80110D08), named for the loader by carsetup's `pc_car_setup_data` with `frontTire` (`TireDesc[2]`).
  Its `tires[4]` are host pointers, so it is own storage (6 of 0xC8 bytes).
- `struct pc_model`: `car`, `tune`, `lasttp[4]` (the plan's `node[4]`; MAT3's name), `torquecurve` and
  `movingSurface` are host pointers; the record is 0x830 bytes, MAT3's field order. Every view of it (143 of its
  whole size, the partial ones of the physics stages: 106 game files, a script split their padding over the
  pointers) and of the descriptor (157) declares the pointers; the 0x808 strides (six, in five files) index the
  records; the 17 files with arena crossings and the plan's 9 others read and store the pointers (no `N64P`,
  `PC_N64W`, link enum or `CURVE` left); one raw MAT3 offset found (UpdateCarSounds' `+ 0x22C`, layout_check) is its
  view's field.
- The tests that name model fields by offset map them (`MODEL_OFS`, `port/assets/tests/support.h`); `assets_model`
  runs the m2c readers on host records without a window.

The numbers (the 63 synthetic paths): the crossings 243 sites / 73,179,946 → 199 / 44,476,345; `PC_N64P` 118 /
32,127,082 → 80 / 5,310,101 (into the game arena 23,210,656 → 104,371: what is left is step 3's and 4's), `PC_N64W`
30 / 1,975,908 → 25 / 106,968 (none into an arena), `PC_N64A` 58 → 57 (init_collision's `stree_nodes`); every
other kind as before; `--zero-site` gives 0 for every one of the 26 files and for `road.c`, `copy_tire_info.c`,
`selectcarload.c`. The census: `fields.link_enum` 87 → 63, `link_enum_types` 62 → 36, `fields.u32_address` 39 → 34,
`conv.file_macros.uses` 227 → 172, `conv.PC_N64P/W.game` 64/90 → 59/84, `text.window.game` 205 → 175,
`context.own` +1. The loader's summary line changes by the moved pointers (13 `own`, 9 fewer `arena`, 20 more
`null` on the synthetic cartridge): the gate ran with `--data-moved` (synth_compare.py, pconly_gate.sh), which
compares that line without its counts by class.

The plan was:

`struct pc_model` (`pc_models`, MAT3 `tModelData`, 0x808 bytes in the N64 layout today, [CARS.md](CARS.md) 4): `car`,
`tune`, `torquecurve`, the road nodes `node[4]` and `movingSurface` become pointers; the car descriptor's `tires[4]`
too. The 26 physics files that read them through `N64P` (`forces1.c`, `forces2.c`, `antispin.c`, `autoshift.c`,
`dotireforce.c`, `enginetorque.c`, `torques.c`, `calcaa.c`, `transmission.c`, `find_best_gear.c`,
`steer_constants.c`, `positions.c`, `init_cars.c`, `init_road.c`, `init_collision.c`, `engine.c`,
`MovingSurfaceForce.c`, `whatslips/group.c`, `update_game_data/update_game_data.c`, `carsetup/group.c`,
`stree_collide/CollideCarBody.c`, `stree_tiresurf/TireSurf.c`, `visuals/AnimateTire.c`, `UpdateSmoke.c`,
`stunt/group.c`, `visuals/AnimateWing.c`), `port/include/pc_game_types.h` (the record), the view check
`views_model_road`, the asset tests that build model records. The record grows, so every stride by 0x808 must be
the type's. Proof: the gate; `view_audit.py`; the crossing counter shows the car sites at 0; the census's
`fields.link_enum` down by the car files'. Depends on: nothing. Can run with 3 and 4. Risk: a raw offset or stride
nothing caught (offset_check and layout_check are the net; races on every path).

### Step 3. The level objects (medium-large): done

Done 2026-11-09 (wave 3.1): what changed, the crossings before and after and the proof are section 10. The plan was:

The level-object nodes (`Target`: `next`, `data`) get their host layout: the node pool `gTargetArray` (130 nodes of
0x70 bytes in the game arena, [POINTERS.md](POINTERS.md) 10.2) becomes own storage of the host type, the StatLL
lists lose their N64-linked form (`pc_statll_link`, the `n64` flag), the visuals' `data` fields and the kinds'
records (coins, mines, flags, cones, knockdowns, windows, bodies) hold pointers, the world's GTLD keys become a
native side table (ASSETS.md 4 #14). Files: `targets/group.c`, `targetpaths/group.c`, `initTargetStructs.c`,
`ClearStatLL.c`, `FreeStatLLE.c`, `GetStatLLE.c`, `InitStatLL.c`, `PauseTargets.c`, `GetTargetVelocity.c`,
`TargetCollBBox.c`, `TargetCollTrigger.c`, `CheckTargetCollision.c`, `setMiniFBCollisionForce.c`, the `Animate*`
and `Start*` visuals (`Coin`, `Flag`, `Mine`, `VelRotGeneric`, `RotGeneric`, `Knockdown`, `ObjSwapGeneric`, `Cone`,
`Cactus`, `Fence`, `Window`), `nonmatching/game/AnimateWeaponIcon.c`, `src/ovl_b/InitBattlePowerUps.c`, the
target records in `include/game/types.h`, `pc_statll_link` in `port/include/pc_prelude.h`, the GTLD part of
`port/assets/world.c`. The PTHD records (the paths' `pts`, `trigPtr`, `parent`) stay for step 7. Proof: the gate
(battle, stunt and race paths reach the targets; the retail battle scripts); `assets_targets`, `assets_battle`.
Depends on: nothing. Can run with 2 and 4. Risk: the pool's element size is 112 in `pool_init` and in walks.

### Step 4. The sky, the palettes, the spill and the image addresses (small-medium): done

Done 2026-11-09 (wave 3.1). What no longer keeps an arena address, and what the renderer gets in place of physical
images:

| What | Before | Now |
|---|---|---|
| each window's sky dome matrix (`SkyInit.c`, `SkyUpdate.c`, `SkyClear.c`) | `skyMatrices`, `s32[4]` in the game arena: the matrix's `PC_N64A` stored, `PC_N64P` read back (104,356 crossings into `gMatrices`, 70 the other way) | `struct tUVMatrix *[4]`, MAT3's type: own storage, read and written as pointers |
| GetMatrix's matrices past the N64's 150 | `pc_matrix_spill`, reserved in the window at 0x8014A250 (`reserved.txt`) because of the sky's u32s | PC storage `ZOID_NewMatrix.c` defines; `reserved.txt` is empty (gen_data's rules for it go with step 5) |
| the battle HUD's health bars' palette (`animHealth.c`) and the track select's fog palette (`trackscr.c`) | `healthPal`, `n64FogPal`: MAT3's `tPalette`, the WAD's PLHD layout, holding the colours' 32-bit address (`PAL_BASE`, `FOG_TABLE`: `PC_N64W`, 6,020 crossings into the runtime images' arenas), which the blit draw read with `PC_N64P` (`zoid_draw/group.c`, 6,018) | `tBlitPalette` (`include/game/types.h`): `palBase` a pointer, which the blit draw reads as one; own storage. The WAD's palettes (`tPalette`, the draw's `Palette`) keep the N64 layout until step 7 |
| the colour and depth images (`StartGraphics.c`, `ClearScreen.c`) | `osVirtualToPhysical(buffer)` in SETCIMG and SETZIMG (3 sites, 307,446 calls, each through `pc_n64_addr`; `pcgfx_rdp_command` read 292,204 physical images) | the buffers' pointers. `osVirtualToPhysical` and `osPhysicalToVirtual` had no other caller and are gone from port/ultra and `PR/os.h`; the counter keeps the kind, at 0 |

The renderer resolves a buffer's pointer and the physical address it replaces to the same host pointer, so its
targets, the screenshots and the high frame rate's copies are what they were: the gate's screenshots byte for byte,
the `gfx_*` and `hfr_*` tests.

The crossings along the 63 synthetic paths (`crossings.py --guard`, 1f592ce8 against this step; the last digits of
the per-frame counts vary between runs, 9.4):

| Kind | Before: sites, crossings | After | What went |
|---|---:|---:|---|
| `PC_N64P` | 118, 32,127,279 | 115, 32,016,705 | `SkyUpdate.c` (2), the blit palette's read in `zoid_draw/group.c` |
| `PC_N64A` | 58, 1,651,346 | 57, 1,651,276 | `SkyInit.c` |
| `PC_N64W` | 30, 1,975,924 | 28, 1,969,888 | the fog palette (`trackscr.c`), `animHealth.c` |
| `pc_n64_addr` | 4, 332,729 | 3, 25,283 | `osVirtualToPhysical`'s |
| `osVirtualToPhysical` | 3, 307,446 | 0, 0 | all |
| `gfx.native_list` | 7, 7,895,283 (phys 292,204) | 7, 7,895,283 (phys 0) | no physical image: the buffers are heap pointers in the window until step 12 |
| total | 243, 73,180,159 | 233, 72,448,587 | |

Into the arenas: `PC_N64P` game arena 23,210,820 to 23,106,300 and the runtime images' arenas 6,018 to 0, `PC_N64A`
game arena 70 to 0, `PC_N64W` runtime images' arenas 6,020 to 0 (`--check --zero-region ovl_a_arena,ovl_b_arena`
passes, and `--zero-site` for the step's files, `--zero-kind osVirtualToPhysical`). What still crosses into an arena is
steps 2's and 3's (the car record, the level objects) and the renderer's pointers to arena data in native lists
(9.4). The census: `data.reserved` 1 to 0, `conv.virt_to_phys.game` 4 to 0, `conv.PC_N64P.game` 64 to 61,
`conv.PC_N64A.game` 84 to 83, `conv.file_macros.defs` 64 to 62, `data.arena.game`, `ovl_a`, `ovl_b` one name fewer
each (`context.own` 390 to 393).

**Left for step 6: the scheduler's pending swap.** `__osScPendingSwap` (`src/rom/scheduler.c:233, 317`) keeps the
frame buffer waiting to be swapped in as a 32-bit address (`PC_N64A`, `PC_N64P`, 101,426 crossings each, into the
heap). As a `void *` it plays the same, but it is initialized boot data: own storage the loader fills from the image,
so the loader's summary line counts one more null pointer (3,198 to 3,199 on the synthetic cartridge), and that line
is the first event of the gate's trace: every path `DIFFERS`. Decoupled (`port/data/decoupled.txt`, without `init`:
the PC's own zero, as the image's word must be, or the first retrace would swap in a frame buffer that isn't one) it
keeps the summary as it is. Step 6's, with the heap's other holders.

The plan was:

What else keeps an arena address, and the renderer's two image addresses: the sky's matrix slots (`SkyInit.c`,
`SkyUpdate.c`: `skyMatrices` as pointers), which lets `pc_matrix_spill` be ordinary PC storage (`reserved.txt` empty,
[MORECARS.md](MORECARS.md) 2.7); the fog and health palettes (`trackscr.c`'s `FOG_TABLE`, `animHealth.c`'s
`PAL_BASE`, the palette word `zoid_draw/group.c` reads); the colour and depth images as host pointers
(`StartGraphics.c`, `ClearScreen.c`, `src/rom/scheduler.c`'s frame buffer, `osVirtualToPhysical` in
`port/ultra/memory.c`, `pc_gfx_present`). Proof: the gate; the crossing counter: no crossing into an arena left
(with steps 2 and 3); the renderer sees no physical colour image. Depends on: nothing. Can run with 2 and 3 (step 1
edits memory.c's other functions: rebase). Risk: the colour image's address also reaches the renderer's copies of
the frame (screenshots, the high frame rate's targets, `gfx_api.c`), so the screenshots are the check.

### Step 5. The arenas out of the window, as blocks (medium): done

Done 2026-11-09 (wave 3.2): what changed, the crossings and the guard before and after, the retail cartridge's words
and the proof are section 11. In short: each arena is a block of host memory `data.c` defines, in the N64 layout,
every name in it relative to its block; nothing reaches an arena by an N64 address along the 63 paths (the guard: 0
faults, none stopped); objsame `same` for every unit; the census's `data.arena.*` and `data.reserved` at 0. The plan
was:

4.1. Files: `tools/port/gen_data.py` (the layout, `--check`, the reserved rules gone), `tools/port/n64syms.py`,
`port/data/pc_data.c` and `pc_data.h` (an arena's host is its block; `at_n64_address` goes), `port/data/datacheck.c`,
`port/data/test_pc_data.c`, `port/ultra/selftest.c` (the "at its N64 address" checks), `port/ultra/absfix.c` (fewer
names to fix: the aliases'), the test programs that place arenas in their own windows, `CMakeLists.txt`,
[DATA.md](DATA.md) 3. Proof: objsame `same` for every unit (the objects reach names through the GOT either way);
data.c's tables the base's but the arenas' hosts; the gate; the arena guard (step 1) with no fault along every path
(the experiment's); the Windows cross build (the relative `.set` in PE: [NOFIXED.md](NOFIXED.md) 2.2) and the
moved build. Census: `data.arena.*`, `data.reserved` at 0. Depends on: 2, 3, 4 (else the blocks need the experiment's
translation). Can run with 6 and 8. Risk: the retail cartridge's u32 words into arenas (`runtime.addr_word`, which
the synthetic cartridge has none of): the lead runs the census with `--report` on the Pi's report first; each one is a
declaration to type as a pointer.

### Step 6. The heap's own code native (large): done

Done 2026-11-09 (wave 3.2): what changed, the crossings before and after and the proof are section 12. The plan
was:

4.3. The headers (`include/game/heap.h`), the allocator (`memsys/group.c`, `heap_release/{group,alloc_at,
ClearMemoryHeap,heap_move_block_low,initsystem,overlay_loader,UnloadWaveInstance,zip,AudioShutdown,
audio_sfx_shutdown}.c`, `FindChunk.c`, `FindHeap.c`, `AllocateMemoryHeapHigh.c`, `memsys_free_total/group.c`,
`largest_heap_chunk/heap_max.c`, `destroy_heap/destroy_heap.c`, `pointer_counters/group.c`,
`purgeable_memory/group.c`), the API in `include/game/memory.h`, `save.h`, `system.h` and its callers (42 files:
`state.c`, `loadwave.c`, `initgfx.c`, the save files, `SetupCarColor`, `car_palette_init.c`, `musyx_alloc.c`,
`AudInit3D.c`, `AudioSfxInit.c`, `paksysdc`, `trackscr.c`, `InitBattlePowerUps.c` ...), the image block as an offset,
`port/ultra/heap.c` (its walker reads the headers), and the scheduler's pending swap `__osScPendingSwap`, which
step 4 left (its section says why and how). The block stays at 0x8017A640 in the window until step 12.
Proof: the gate, including the memory paths (`mem64`, `membig`, `memhigh`); the heap's exit line (peak, least free,
allocations) the base's on every path; `assets_heap`; the crossing counter: no heap crossing in the heap's files.
Depends on: nothing in this list, but its 42 caller files must be free: run it where steps 2-4 and 7-9 don't edit
them (after step 3, which edits `targets/group.c` and `initTargetStructs.c`, and before steps 7-9). Can run with 5
and 8. Risk: the allocator's arithmetic must come out the same with pointers (the 32-byte header, the 64 bytes
before the first chunk, alignment from a block 64 KiB aligned); the save code's views of the heap.

### Step 7. The WAD's records native (large): done

Done 2026-11-09 (wave 3.3): what changed, the crossings before and after, the proof and what it changes for the
later steps are section 13. In short: the WAD's converter builds the OBHD, TXHD, PLHD and PTHD records beside the file,
MAT3's fields with host pointers, and the game reads only those (one declaration each, in `pc_game_types.h`); the
font's converter builds its letters; a path target's record names its path by index; the car palettes and the
blits' palettes are the WAD's palette record. The plan was:

4.4 for the records: OBHD, TXHD, PLHD, PTHD (and the PATH points they reach) built native by `port/assets/wad.c`;
the relocation of `SetupWaveModel` (`waves.c`), `MBSetupWad.c`, `MBOffsetWad.c`, `MBGetFromWad.c` moved into the
converter; the readers' strides and views: `zoid_draw/group.c` (`LodSet`, the texture and palette loads),
`trackscr.c`'s `MapLod`, `slot_sound/group.c` and `port/assets/font.c` (the font textures and the font metric
handles), `StartSmoke.c`, `ZOID_FindObjInstance.c`, `MBOX_GetObjectRadius.c`, `GetTargetVelocity.c` and
`targetpaths/group.c` (PTHD), the texture animation's records (`InitTextureAnimation.c`, `TextureAnimation.c`),
`gRomObjDefPtrs`/`gTexDefPtrs`/`gPalDefPtrs`/`gTargetPaths`. The display lists stay N64 form for step 10 (their
addresses inside still window addresses: the heap block is still in the window). Proof: the gate; `assets_wad`, `assets_model`, the mods paths
(textures replaced by mods are looked up through these records). Depends on: 3 (PTHD readers), 6 (the heap API in
`slot_sound`, `loadwave`). Can run with 8 and 9. Risk: the many views of these records; mods.

### Step 8. The other formats (medium): done

Done 2026-11-09 (wave 3.2). Each format's converter builds the records the game reads beside the file when it loads,
with host pointers into the file; the game asks for them by the file's heap block. The block keeps the file's bytes
and size (converted to the host's byte order, as before), so the heap holds what it held (4.3, 7): the records live
in PC memory, kept by block in `port/assets/records.c` and released when the heap frees the block
(`pc_heap_block_freed`), on `pc_wave_released`, or when another file is loaded over its memory.

| Format | The N64 wrote into the file | Now: the records (`pc_game_types.h`), built by | The game |
|---|---|---|---|
| text bank (waves 0-5) | each string's address over its offset (`AdjustTextWavePointers`) | `struct TextTables` (the one declaration, moved from `include/game/types.h`): the string tables native `char *` arrays, the 16-bit tables the file's; `port/assets/text.c` | `AdjustTextWavePointers` copies them into `gText` (`pc_text_bank`); `pc_text_strings` is gone |
| world WAD (120-138) | the file's address into each WHDR `woff` (`LoadTrack`), the targets over GTLD's keys (`SetTargetDefs`) | `struct pc_world_records`: the names (`struct NameEntry`, 24 bytes, the root a host pointer), GDAT in the file with its count word, GTLD's side table (step 3's, now built at load); `port/assets/world.c` | `LoadTrack` points the world's globals at them: no `MBSetupWad`/`MBGetFromWad` for the world, no `PC_N64A`; `NewWorld` bsearches the native table by the name, no `PC_N64P`; `pc_world_chunk` is gone |
| path file (158-182) | the branches' `pts` in place; headers copied out by byte offsets (`ReadPathData`) | `struct pc_path_records`: the resurrection path, its branches, the drone paths, the points host pointers into the file (the path types moved from `include/game/types.h`); `port/assets/path.c` | `ReadPathData` copies them into `gResPath`, `gDronePaths` and its own array of branches |

The text bank's tables are rebuilt where they were when `ReplaceWave` reads another language into the block (the
plan's risk: `gText`'s tables never point at freed memory between the read and `AdjustTextWavePointers`); they move
only for a bank with more strings than the first loaded, the largest. The four views of `gText` the plan feared were
already one (wave 2). `ReadPathData` keeps copying the branches into an array of its own, so what `SyncAltPaths`
writes into them stays the game's and each read starts from the file's, as before; the own storage it fills
(`gResPath`, `gDronePaths`) gets the same fields, and the data loader's tables (data.c, data_layout.json) are the
base's.

The crossings along the 63 synthetic paths (`crossings.py --guard`, 786dec37 before; the last digits of per-frame
counts vary, 9.4):

| Kind | Before: sites, crossings | After | What went |
|---|---:|---:|---|
| `PC_N64P` | 74, 5,199,707 | 73, 5,199,594 | `NewWorld.c` (57); `MBSetupWad.c:24` 8,570 to 8,513 (the world's TOC, no longer rebased) |
| `PC_N64A` | 56, 1,633,596 | 55, 1,633,197 | `loadtrack.c`'s `woff` (57); `MBSetupWad.c` 24, 25 and 28 (57, 57, 228: the world's) |
| `pc_n64_ptr` | 8, 18,224,444 | 7, 18,224,273 | `pc_world_chunk` (171: WHDR, GDAT, GTLD) |
| the other eleven kinds | 43, 18,679,788 | 43, 18,679,788 | none |
| total | 181, 43,737,535 | 178, 43,736,852 | 3 sites, 683 crossings |

`--zero-site` passes for `NewWorld.c`, `world_name_compare.c`, `loadtrack.c`, `ReadPathData.c`,
`AdjustTextWavePointers.c`, `LoadTextWave.c`, `InitTextWave.c` and the converters (`world.c`, `text.c`, `path.c`,
`records.c`), and for `pc_world_chunk`; the control (`FindChunk.c`) fails. The text bank and the paths had no crossing
before (their pointers were host pointers into the heap block); what went for them is the game's walk of the files'
layouts. The guard (counting): 4,428,376 faults at 449 instructions before, 4,428,397 at 450 after: `game_strcmp` now
reads the world's name in the game arena where `game_strcpy` copied it (2 paths), the name compared as it is. The
census: `conv.PC_N64P.game` 43 to 42, `conv.PC_N64A.game` 82 to 81, `conv.pc_n64_ptr.port` 27 to 26,
`conv.silent.trunc` 3 to 2, `conv.cc.implicit` 46 to 40, `fields.u32_address` 27 to 26 (`woff`), `tests.window` 72
to 69 (`assets_world`'s window addresses: the three tests need no window now), `text.window.game` 134 to 132.

**The proof.**

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3b-ref --skip realtime --data-moved` (Rocky, Debug, against 786dec37) | PASS: ctest 572 of 572, synth_compare `--same --shots --audio` 63 of 63 `same` (first runs) |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio --data-moved -j6`) | 63 of 63 `same` |
| `ctest --test-dir build/opt -LE realtime` | 572 of 572 (`assets_rom` skipped without a ROM) |
| the language change: an options-screen run that switches the language three times, against the base | `same` (the text bank read into the same block three times; `assets_text` covers the switch, a larger bank and the release). Then committed as the 64th synthetic path, `language` (below) |
| `assets_text`, `assets_world`, `assets_path` (the records, the game's readers on them, no window); `assets_wad`, `assets_mods`, `assets_dispatch` | pass |
| the data loader's tables (`data.c`, `data_layout.json`, `aliases.ld`, `stubs.c`) | the base's, byte for byte: the moved types have the same layouts |
| `offset_check.py --strict` (Debug) | no open finding (3 reviewed, the base's); its rule for `pc_text_strings` (unused) goes with the function |
| `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | none open; one new key reviewed: `ReadPathData`'s copy loop on `pc_alt_paths` (`tmr`, i below a u8 count, of 256) |
| `census.py` | 1,071 of 1,071 compile |
| `nofixed_audit.py --baseline` | no class grew; lowered by this step's moves: `cc.implicit.int_to_ptr` 24 to 18, `context.cc_warnings.int-conversion` 90 to 84, `port.rdram_base.tests` 40 to 37, `port.translate` 32 to 31, `port.trunc.tests` 19 to 17, `src.trunc` 3 to 2 |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug): CI's subset (`ultra_selftest.*`, `pc_data_*`, `synth_.*`) and against the fixed build (`--runs 1 --same`) | builds; 93 of 93; 63 of 63 `same` |
| boot without a ROM (CI's) | reaches wave 0 and stops where the base stops (`AudioSfxInit`'s allocation on the blank cartridge), the same log |

**The `language` path** (`tools/port/synth/language.press`, added after the step, so the gate covers `ReplaceWave` and
the converter's rebuild from now on): the options screen's language changed three times (Spanish, Italian, Spanish
again: banks 1, 2 and 1 read into the first bank's block), then a single race in Spanish, its HUD drawn from bank 1's
strings. Its checks: the wave loads in its log (`RUSH_TRACE_WAVES`: bank 1 twice, bank 2 once, no converter message),
the race's screenshot drawn (`synth_shot.py`) with the HUD's text (491 black pixels, as `drive`'s). The synthetic
front end draws no menu text (its screens are black but for one bar), and every language's strings are as long as
English's with every letter the same box, so the picture can't tell the languages apart; the trace and the
screenshot byte for byte against the base are the proof. Against 786dec37's builds: `same` (Debug and
RelWithDebInfo, `--same --shots --audio --data-moved`); `synth_boot_language`, `replay_same_language` and
`paced_same_language` pass (Debug, RelWithDebInfo); the moved build's `synth_boot_language` passes and its run is
`same` as the fixed build's. ctest has 575 tests without the realtime ones now (572 before).

**What this changes for later steps.**

- Step 7 has `port/assets/records.c` for the WAD's records (OBHD, TXHD, PLHD, PTHD): keep, find, detach, release by
  block (handle blocks move: the font data would need a move hook there).
- `MBSetupWad` and `MBGetFromWad` no longer run on the world WADs: their callers are the model WADs' (`waves.c`)
  only.
- The game's text, world and path readers take their records from the PC layer's accessors (`pc_text_bank`,
  `pc_world_records`, `pc_path_records`); a file the converter refused (it says so) gives empty records.

The plan was:

4.4 for the text bank (`AdjustTextWavePointers` and its 17 readers, `ReplaceWave`, `port/assets/text.c`), the world
(`NewWorld.c`'s name table, `loadtrack.c`'s `woff`, `port/assets/world.c` but its GTLD part), the paths
(`ReadPathData.c`, `port/assets/path.c`). The road needs nothing: its file links by index (`port/assets/road.c`), and
the car record's pointers into its tree are step 2's. Proof: the gate (every path loads a track and text);
`assets_text`, `assets_world`, `assets_path`. Depends on: 3 (the GTLD part of world.c). Can run with 5, 6 (files
apart) or 7 and 9. Risk: the text bank has four views in 17 files (ASSETS.md 4 #15), and `ReplaceWave` swaps the
bank under them when the language changes.

### Step 9. MusyX native (large): done

Done 2026-11-09 (wave 3.3): what changed, the crossings before and after, the synthetic song and the proof are
section 14. The plan was:

4.5. Files: `src/rom/sound/*.c` (`registry.c`, `macros.c`, `voices.c`, `sequencer.c`, `synth.c`, `hardware.c`,
`dma_queue.c`, `groups.c`), `src/rom/speech/streams.c`, `AudioInitSys.c`, `musyx_cart_address.c`,
`port/assets/musyx.c`, `port/audio/musyx.c` (the N64-form record path and `n64ptr` go; `resolve` takes pointers),
the audio manager's listener records (`UpdateListeners.c`, `StartListeners.c`, `AudRemoveListener.c`). Proof: the
gate with `--audio` byte for byte; the retail cartridge's music (the synthetic cartridge has no songs: the lead's
audio captures on the Pi, base against step); `assets_musyx`, the audio tests. Depends on: 6 (the audio heap's
allocations). Can run with 7 and 8. Risk: the largest library with the least synthetic coverage.

### Step 10. Display lists native (large): done

Done 2026-11-11 (wave 3.4): what changed, the crossings before and after, the proof and what it changes for the later
steps are section 15. In short: the WAD converter builds a file's display lists beside it when it loads, command for
command in the PC's form (`Gfx`, w1 a host pointer where it carries an address), and the vertices they load in the
host's byte order; the game's walkers and the renderer read only those; nothing relocates a list, and the renderer's
N64-form list path is gone. The plan was:

4.4's display lists: `port/assets/wad.c` (the lists converted, the vertices swapped), the walkers
(`relocate_model_dl.c`, `relocate_texture_loads.c`, `display_list_traverse.c`, `flip_triangle_winding.c`,
`mirror_vertex_texcoords.c`, `MirrorWaveInstance.c`, `displaylist_get_texture_image.c`,
`displaylist_set_texture_image.c`, `displaylist_set_tile_size.c`, `TextureAnimation.c`), `port/ultra/rcpaddr.c`
(`pc_rcp_reloc`, `pc_rdram_k0_high` go), and in the renderer only what reads a ROM list's address (the N64-form
list path can go once no ROM list reaches it). Proof: the gate's screenshots byte for byte on every path (mirror,
reverse, every detail level, the high frame rate's paths, the mods' texture paths); the renderer's tests; the
census's `be.accessors.game`, `lit.reloc.game`, `lit.mask.game` at 0. Depends on: 7. Can run with 11. Risk: the
highest: the renderer's output must stay exact; the texture animation patches lists every frame; mods and the
high frame rate key on what lists hold.

### Step 11. The rest that holds heap addresses, and the tests' windows (medium)

The holders steps 2-10 don't cover: the ghosts and saves (`GhostUpdate.c`, `GhostStart.c`, `GhostEnd.c`,
`save/{ctx_newghost,ctx_slot,group,save}.c`, `SaveGhostInLimbo.c`, `PS_ReadPartialFile.c`, `paksysdc`), the car
palettes (`StartCar`, `car_palette_init.c`, `SetupCarColor`, `AnimateBrakeLights.c`), `hud_animTrackMap.c`,
`hal_text/drawtext.c`, `HAL_GetTextWidth.c`, `updatePauseScreen.c`, `zlib_deflate`, `TB_TextOncePerFrame.c`,
`hudextra_icons.c`; and the test programs' own windows (the 20 files of `tests.window`: `port/assets/tests/support.*`,
`wad_game.c`, the gfx, audio and input tests; the WAD tests that step 10 rewrites are its), each test's data in host
memory. Proof: the gate (the ghost and pak paths); every test. Depends on: 6. Can run with 10. Risk: the ghosts and
the saves run on few synthetic paths (`ghost`, `pak`, `profile`); the retail pak scripts are the check.

### Step 12. The window gone (medium-large)

The heap's block becomes PC memory (a mapping anywhere, sized by `game.memory`); `port/ultra/memory.c`'s window
(the memfd, the views at 0x80000000 and 0xA0000000, Windows' sections), `absfix.c`, aliases.ld's absolute names,
`pc_n64_ptr`/`pc_n64_addr` (the loader keeps its own decoding), the prelude's `PC_N64P`, `PC_N64A`, `PC_N64W`,
`PC_SLOT_*` and whatever file macros remain, the renderer's and audio interpreter's 4 GiB rule (`is_host`,
`rdram`, `resolve`), `RUSH_NOFIXED` and its CI job, the x86-64 `-fPIC`/no-relaxation flags, `pc_n64_map.h` down to
the cartridge's and the display lists' segments, `n64syms.py`'s window constants; the guard mode reserves
0x80000000-0xBFFFFFFF with no access; `cmake/sanitize.cmake` allows `address` on x86-64 and CI gets the job. Proof:
the census at 0 but `context.`; the gate; the guard mode along every path and the retail scripts; ASan on x86-64
over ctest and every synthetic path; the Windows and (first build) macOS jobs. Depends on: everything above.
Risk: what the synthetic paths never reach (retail-only data, the songs): the guard is the net, so the lead's retail
scripts run in the guard mode too.

### Step 13. One object per datum (large; may be split by subsystem)

4.1's second half, with ASan's redzones as the check: gen_data gives each datum its own storage
(`decoupled.txt`'s recipe without a list: every symbol), each walk found is ended at its own array or its data made
one struct, the shared headers' arrays get complete declarations, the 12 interior labels fold into fields,
`runtime.undescribed`'s targets get declarations or `reached.txt` rules (else they point into nothing). Proof: ASan
along every path with no report; the gate. Depends on: 12 (ASan on x86-64). Can run with 14. Risk: walks no path
reaches stay hidden from ASan; the static tools (`bounds_check.py`, `index_check.py`, `offset_check.py`) and the
retail scripts under ASan on the Pi (aarch64 ASan works today, SANITIZE.md 1.1) are the rest of the net.

### Step 14. The last of the N64's byte order (small)

`PC_RGBA` and the colour words (`be.rgba.game`), the records the data sweep left in the ROM's order because every
value read the same either way (the menu screens' button matrices, [DATA.md](DATA.md) 8 "Judged and left"), the
big-endian ranges narrowed to the N64 texel formats and renamed for what they are. Depends on: 10. Can run with 13.
Risk: small; `endian_check.py --check-loader` and datacheck's `known_widths` pin the widths.

## 6. Order, waves and agents

| Wave | Steps (agents) | Why together |
|---|---|---|
| 3.0 | 1 (one agent, short) | the counter is every later step's proof |
| 3.1 | 2, 3, 4 (three) | the arena holders; their files are apart (pc_game_types.h, types.h and the prelude are shared: each edits its own hunks) |
| 3.2 | 5, 6, 8 (three) | the data layout, the heap's code, the text and world formats: no file in common |
| 3.3 | 7, 9 (two) | the WAD records and MusyX, both on the native heap (`port/assets/assets.c`'s dispatch: each its own line) |
| 3.4 | 10, 11 (two) | the display lists, and everything else holding heap addresses |
| 3.5 | 12 (one) | the window itself |
| 3.6 | 13, 14 (two, or 13 split by subsystem into three) | under ASan |

Three agents at a time at most, on Rocky's shared limits ([PCONLY.md](PCONLY.md) 1). Steps 2-4 and 5-6 can start as
soon as wave 2's last agents have merged (they edit the same game files wave 2's names and views did); run
`rename.py --apply` and `to_headers.py` on each step's tree first if it began before a merge (PCONLY.md 4 "After
merging").

## 7. What I would not do, and why

- **No 32-bit offsets into a PC block standing for pointers.** Offsets from the heap's base would keep every record
  small and every relocation as it is, but they are the window again under another name: an address space of 32-bit
  numbers that stand for memory. The records get pointers.
- **No change to the heap's accounting.** Bigger headers or native files in the heap would change what fits, and so
  what the game loads when memory is short: the default game would no longer play as it does. The heap keeps its
  sizes; the native forms live beside it.
- **No conversion of every file at startup.** The converters run when the game loads a file, as now: the game's heap
  decides what is loaded when, and the native data follows it. A startup cache of decompressed files would be an
  optimization with nothing visible to show for it.
- **No texels or TLUTs swapped.** They are texel formats the renderer decodes, they hold no address, and a 16-bit
  load may carry 4- or 8-bit texels, so a swap by the texture's declared format could be wrong.
- **No objects per datum before AddressSanitizer can watch them.** The experiment stops at the first walk, and a walk
  that doesn't fault reads the wrong neighbour silently.
- **No translation at every use as the end state.** The moved build's translation (`PC_N64P` to `pc_window`) is the
  safety net while steps remove crossings; it stays only until step 12.
- **Not the cartridge's bus addresses or the loader's tables.** `0xB0000000 | offset` is a device address libultra's
  PI interface takes, not memory, and replacing the PI with a ROM file interface is its own project (port/ultra's).
  The loader's symbol tables give where each datum lies in the cartridge's images: the ROM's format, read once at
  startup. Both are named for what they are; neither puts an N64 address in the running game.

## 8. What the numbers don't show

- The retail cartridge's data: `runtime.addr_word` (u32 words in ROM data holding N64 addresses) and
  `runtime.unresolved` are 0 on the synthetic cartridge; the retail one has some ([DATA.md](DATA.md) 4, "agent D's
  work list"). The lead runs `window_census.py --report build/pc/data_report.txt` on the Pi; the counts stay there.
- The paths the synthetic cartridge can't take: songs (no song data), some battle and stunt objects. The crossing
  counter on the retail scripts (the lead, on the Pi) gives the sites they add.
- `fields.u32_address` and `text.window.game` read comments; `api.u32_address` reads parameter names. They are
  pointers to the work, not proofs of its end; the crossing counter and the guard mode are.

## 9. The crossing counter and the arena guard

Built 2026-11-09 (step 1, wave 3.0) from the probe of section 2. The census (section 1) reads the sources; this runs
the game and says which crossings happen, where, how often and into what. It is every later step's proof: "these
sites cross 0 times along every path", and after step 5 "nothing reaches an arena by its N64 address".

| Piece | Where |
|---|---|
| The option: a Debug build with `-DRUSH_COUNT_CROSSINGS=ON`, compiled with `-fno-inline` so that the PC layer's sites are their real callers (Linux; not with `RUSH_NOFIXED` or `RUSH_CHECK_CASTS`) | `CMakeLists.txt` |
| The hooks and the kinds; every hook is nothing without the option | `port/include/pc_crossings.h` |
| The game C's translations counted at their lines | `port/include/pc_prelude.h` (the block before `pc_game_types.h`) |
| The counter, the dump, the guard (built only with the option) | `port/ultra/crossings.c` |
| The PC layer's hooks | `port/ultra/memory.c` (`pc_n64_ptr`, `pc_n64_addr`, `osVirtualToPhysical`), `port/gfx/gfx_rsp.c` (`pcgfx_resolve`, `pcgfx_segment_value`, the list's form, the guard's frame), `port/audio/musyx.c` (`n64ptr`, `resolve`), `port/data/pc_data.c` (the guard paused while an image loads), `port/main.c` (start, finish) |
| The runs and the report | `tools/port/crossings.py`, its test `tools/port/tests/test_crossings.py` |
| ctest | `crossings_tool` (every build); in the counting build `crossings_paths` and `crossings_guard_stop` (`ctest -L crossings`) |

The default build doesn't change: objsame gives `same` for all 966 game units in Debug and RelWithDebInfo, and all
865 other objects of the build compare `same` with a build of the base at a path of the same length (they hold
`__FILE__`, the checkout's path). The counting build plays the same game: `synth_compare.py --same --shots --audio`
gives 63 of 63 `same` against the default build, and so does the counting build with the guard on (`--env-b
RUSH2049_WINDOW_GUARD=arenas,count`).

### 9.1 How to run it

The whole measure, on Rocky or any Linux host, in about a minute and a half once built:

    python3 tools/port/crossings.py --build build/count --guard -j 6 --out build/crossings/report.txt

configures `build/count` the first time (Ninja, Debug, `-DRUSH_COUNT_CROSSINGS=ON`), builds it, makes its synthetic
cartridge if it has none, runs the synthetic paths (synth_compare.py's runs: on the virtual clock, its options,
`pcsetup` left out; 64 since step 8 added `language`, 63 in the measures of this page), then each again in the guard's counting mode, and prints the report: one line per kind (always
all of them, in the same order), then each kind's sites sorted by name, then the guard's instructions. A site is a
source line for the game C, `function file:line` (the call) for the PC layer. With the same tree, two runs give the
same report but for a few counts (below), so a step's report diffs against the base's line by line. `--sort count`
puts the largest first, `--top N` keeps each kind's N largest, `--json FILE` writes the numbers, `--logs DIR` keeps the
runs' logs and dumps (default `build/crossings/paths`), `--exe` runs a counting build that exists, `--report DUMPS`
reports on dumps already written.

A step's proof is a run with checks, each failing the command (exit 1) with the sites it found:

    python3 tools/port/crossings.py --build build/count --check \
        --zero-region game_arena,boot_arena,ovl_a_arena,ovl_b_arena       steps 2-4 together: nothing into an arena
    python3 tools/port/crossings.py --build build/count --check --zero-site src/blob/forces1.c     one file's sites
    python3 tools/port/crossings.py --build build/count --check --zero-kind pc_statll_link,pc_statll_set_link
    python3 tools/port/crossings.py --build build/count --guard --stop --check --no-faults          step 5
    python3 tools/port/crossings.py --build build/count --baseline FILE      no kind grew (--write-baseline FILE)

`--check` also makes each run end as its press script says and write its dump, and each guarded run's trace be its
plain run's. `--stop` runs every path once more in the guard's stopping mode and lists where each stopped.

By hand: `RUSH2049_CROSSINGS=FILE` makes a counting build write its counts to FILE at exit (`%p` in it becomes the
process id; without it, one line on stderr gives the totals), `crossings.py --report FILE --exe build/count/rush2049pc`
reads it. `RUSH2049_WINDOW_GUARD=arenas` stops at the first access with a note and the stack; on today's layout the
first access by name is the PC layer's own, as the mods are installed:

    rush2049pc: window guard: the datum at 0x8002E580 (gWaveSizes+0x0, the boot_arena) reached by its N64 address,
    by the instruction at rush2049pc+0x1e04b6 (RUSH2049_WINDOW_GUARD=arenas, docs/port/NATIVE.md 9)

    rush2049pc: Segmentation fault at address 0x8002e580 in N64 thread main
      #1  on_fault at port/ultra/crossings.c:392
      #2  /lib64/libc.so.6+0x411bf
      #3  pc_mods_install at port/mods/mods.c:1196
      #4  mods_install at port/main.c:656 ...

The retail cartridge (the lead, on the Pi; the counts stay there): build the counting build on the Pi, then

    RUSH2049_CROSSINGS=build/crossings/retail-%p.crossings build/count/rush2049pc --rom ROM ... (a retail script)
    RUSH2049_CROSSINGS=build/crossings/retail-guard-%p.crossings RUSH2049_WINDOW_GUARD=arenas,count \
        build/count/rush2049pc --rom ROM ...
    python3 tools/port/crossings.py --report build/crossings --exe build/count/rush2049pc

### 9.2 What it counts

| Kind | Where it is counted | Site |
|---|---|---|
| `PC_N64P`, `PC_N64A`, `PC_N64W` | the prelude's translations, and so each file's own macros (`N64P`, `N64W`, `LINK` ...) | the line that uses it |
| `PC_SLOT_N64`, `PC_SLOT_SET` | a heap handle's slot read or written as an N64 address | the line |
| `pc_statll_link`, `pc_statll_set_link` | a StatLL list's link, when the list is in its N64-linked form | the list function's line |
| `pc_n64_ptr`, `pc_n64_addr` | port/ultra's translations | the caller |
| `osVirtualToPhysical` | the colour and depth images' addresses | the caller |
| `gfx.rom_list`, `gfx.native_list` | a value below 4 GiB the renderer takes for an N64 address (`pcgfx_resolve`, `pcgfx_segment_value`), by the form of the list the command came from: a ROM list (N64 form, big-endian) or a native one; since step 10 every list is native, and `gfx.rom_list` stays at 0. Host pointers aren't counted | the caller (`dl_at`, `movemem`, the VTX case, `pcgfx_rdp_command`'s image commands, `pc_gfx_present`'s frame) |
| `audio.n64ptr` | every N64 address the audio interpreter reads (`n64ptr`): an N64-form record's decoder, or `resolve` for a native field (null pointers aren't counted) | the caller |
| `audio.field` | `resolve`'s share of those: a native record's pointer field holding an N64 address | the record's reader |

Each count is also split by where the address points (a host pointer for the kinds that go from a pointer to an
address): `game_arena`, `boot_arena`, `ovl_a_arena`, `ovl_b_arena` (the data loader's arenas), `image_block` (the heap
block reserved for the runtime images, outside their arenas), `heap`, `low_memory` (the boot parameters), `window` (the
window's other bytes: the N64's code ranges, where nothing lives on the PC), `kseg1`, `cart` (the cartridge through
KSEG1), `phys`, `segmented`, `other`, `null`, `host` (a pointer above 4 GiB). For RDRAM outside the heap the report
names the data (the loader's name for the first address in each region; offsets left out).

Not counted: the loader's and the asset converters' own decoding of the ROM's addresses (they are where N64
addresses belong); `pc_rcp_reloc` and `pc_rdram_k0_high` (`rcpaddr.c`, the display lists' relocation, gone with step 10,
`lit.reloc` in the census); the PC layer's own `pc_rdram() + offset` arithmetic (`port.rdram_base`); `pc_n64_identity`
(the state hash's ids). And whatever builds an address with integer arithmetic and no macro: the guard sees those
once step 5 has emptied the arenas' range.

### 9.3 The arena guard

`RUSH2049_WINDOW_GUARD=arenas` (stop) or `arenas,count`, in the counting build. Once the game's data is loaded
(`main.c`), the pages of the boot and game arenas in the window, in both views (KSEG0 and KSEG1), are mapped with no
access:

- **stop**: the first access stops the program: the note names the datum, its region and the instruction, then the
  fault handler (`misc.c`) the N64 thread and the stack.
- **count**: every fault is counted by its instruction (a library's, such as `memcpy`'s, with the program's call of
  it when that is the return address the routine left) and the page is opened; at the start of every graphics task
  all the pages close again. So a run counts each page's first access in a frame, which on the virtual clock is the
  same every run; an access to a page another instruction opened earlier in the frame isn't seen. The dump lists the
  instructions; the report names them.

A fault on a guarded page outside the arenas' bytes only opens the page if it is the heap's (the heap's first chunk
shares the game arena's last page: 2,909 such faults along the 63 paths), and is counted as a stray access if it is
anywhere else (the N64 code ranges around the arenas). The guard is paused while the data loader fills an image
(`pc_data_load_image`). The runtime images' arenas are not guarded: they lie in the heap block the game reserves for
an image, which is heap memory (filled by the heap's free, then reused) whenever the image isn't loaded, so their
crossings are the counter's `ovl_a_arena` and `ovl_b_arena`; step 12's guard of the whole window covers them. A system
call given an arena buffer would fail (EFAULT) rather than fault: none happens along the synthetic paths (every
guarded run's trace, screenshots and sound included, is the default build's).

Until step 5 the arenas were at their N64 addresses, so **every access by name faulted too**: on that layout the
guard proved it sees what it should, and its sites were the game's by-name accesses, which step 5 moved with no change
to the code. It couldn't tell those from a crossing into an arena (both were the same address): steps 2-4's targets
were the counter's arena regions (9.4). Since step 5 the arenas are blocks of host memory and the window's pages over
their N64 ranges hold nothing, so the two are one: the guard, counting or stopping, finds nothing along the 63 paths
(section 5, step 5), and anything it finds would be a crossing into an arena (ctest `crossings_paths` and
`crossings_guard_stop` check `--no-faults`).

### 9.4 The baseline (68069845, the 63 synthetic paths)

| Kind | Sites | Crossings | Where they point |
|---|---:|---:|---|
| `PC_N64P` | 118 | 32,127,099 | game_arena 23,210,666, heap 8,814,247, null 96,046, ovl_a_arena 5,085, ovl_b_arena 933, image_block 122 |
| `PC_N64A` | 58 | 1,651,346 | heap 1,650,904, image_block 309, game_arena 70, cart 63 |
| `PC_N64W` | 30 | 1,975,912 | heap 1,932,064, null 24,236, game_arena 13,592, ovl_a_arena 5,085, ovl_b_arena 935 |
| `PC_SLOT_N64` | 5 | 1,682,453 | heap |
| `PC_SLOT_SET` | 2 | 216 | null 210, heap 6 |
| `pc_statll_link` | 1 | 8 | game_arena (`gTargetArray`) |
| `pc_statll_set_link` | 3 | 7,418 | game_arena 7,359 (`gTargetArray`), null 59 |
| `pc_n64_ptr` | 8 | 18,224,444 | heap 16,614,536, game_arena 1,609,782, low_memory 126 |
| `pc_n64_addr` | 4 | 332,729 | heap 332,725, host 4 (the state hash's ids) |
| `osVirtualToPhysical` | 3 | 307,446 | heap (`StartGraphics.c`: the colour and depth images) |
| `gfx.rom_list` | 1 | 7,001,069 | phys 6,657,479, heap 343,590 (the VTX case: the ROM lists' relocated vertices) |
| `gfx.native_list` | 7 | 7,895,283 | heap 5,993,297, game_arena 1,609,782, phys 292,204 |
| `audio.n64ptr` | 1 | 987,272 | heap (all through `resolve`: no N64-form record) |
| `audio.field` | 2 | 987,272 | heap (`read_sfd`, `voice_output`) |
| total | 243 | 73,179,967 | |

The crossings into the arenas, steps 2-4's targets: 52 sites in 27 game files.

| Step | Sites (crossings) | What |
|---|---|---|
| 2, the car record | `forces1.c` (11 sites, 9.65 M), `update_game_data.c` (5, 3.73 M), `positions.c` (1.86 M), `autoshift.c`, `dotireforce.c` (1.84 M each), `torques.c`, `transmission.c` (0.92 M each), `whatslips/group.c` (0.92 M), `calcaa.c`, `engine.c`, `enginetorque.c` (0.46 M each), `carsetup/group.c` (the PC_N64W stores of `tune`, `car` and `torquecurve`, a tyre's PC_N64P), `AnimateTire.c`, `init_road.c`, `steer_constants.c`, `find_best_gear.c`, `init_cars.c` | `m->car` (the car descriptors), `gTune`, `stdtorquecurve`, `frontTire` |
| 3, the level objects | `ClearStatLL.c` (7,353), `GetStatLLE.c` (14), `PauseTargets.c` (3), `targets/group.c` (6), `InitBattlePowerUps.c` (6) | `gTargetArray` |
| 4, sky and palettes | `SkyUpdate.c` (2 sites, 104,356), `SkyInit.c` (70), `trackscr.c` (5,085), `animHealth.c` (935), `zoid_draw/group.c` (6,018) | `gMatrices`, `trackscr_fogColor`, `healthColor` |

The rest of the game-arena counts are the renderer's: `default_lights` (MOVEMEM, `light_read`) and
`cull_box_vertices_a`, `cull_vertices` (VTX), pointers to arena data in the game's own native lists, which become host
pointers by themselves when the data leaves the window (section 2, point 3): 1,609,782 in `gfx.native_list` and again
in `pc_n64_ptr`.

The guard on this layout (counting, the 63 paths): 4,429,021 faults at 452 instructions in 266 functions and 208
files, into 249 data, 3,318,405 in the game arena and 1,110,616 in the boot arena; by tree `src/blob` 2.54 M,
`src/rom` 1.11 M, `port/main.c` 0.49 M (the log's state lines read `gGameState`, `gRace`, `gNumWindows`,
`gNumActiveCars`, `gTrackNum` by name), `port/gfx` 0.21 M (`default_lights`, `cull_vertices`,
`zoid_poly_vertex_buffers` through the lists' pointers), `src/ovl_a` 70 k, the C library 4,704 (`sprintf` reading
`car_part_name_format`), `port/ultra` 189 (`pools.c`'s report), `port/mods` 62 (`gWaveSizes`). The most touched:
`gNumActiveCars`, `gTrackNum`, `gNumChannels`, `gStoppedChannels`, `zoid_poly_vertex_buffers`, `cull_vertices`,
`sound_initialized`, `sound_output_frozen`, `__osScSwapCount`, `frame_cycle_count`: each about once a frame. The full
list is the report's last section.

Runs of one tree give the same sites and the same guard instructions; a few counts differ by a handful, in sites
that run every frame (`scheduler.c:233` and `autoshift.c` by 2, the total by up to 20 of 73 M; in the guard the audio's
and the scheduler's per-frame instructions, its total by about 25 of 4.4 M): the frames around the exit, which the
traces leave out. Compare kinds and sites, not the last digits; `--baseline` allows them 0.1%.

### 9.5 What this changes in the plan

- **Step 2: the car descriptors have no declaration.** `m->car` points at 0x801108D0, inside `steady_leg_duration`,
  which `include/game/camera.h` declares `f32[EXTENT(272)]`: its extent runs over the descriptor table, so the loader
  names those crossings `steady_leg_duration+0x8...`. Step 2 needs the table declared with its own name and type (and
  `steady_leg_duration` its own extent) before `car` can become a pointer to it. Done: `car_descriptors[6]` (step 2).
- Step 2's files with arena crossings are 17 of the 26 the plan lists; the other 9 (`forces2.c`, `antispin.c`,
  `init_collision.c`, `MovingSurfaceForce.c`, `CollideCarBody.c`, `TireSurf.c`, `UpdateSmoke.c`, `stunt/group.c`,
  `AnimateWing.c`) reach the heap (the road's nodes) or aren't reached on these paths: their sites are the counter's
  heap lines, and they still change with the record.
- Step 4: on these paths only `StartGraphics.c` calls `osVirtualToPhysical` (3 sites, the colour and depth images);
  the renderer's physical image addresses are `pcgfx_rdp_command`'s (292,204 in `gfx.native_list`). `scheduler.c`
  reaches the frame buffer through `PC_N64P`/`PC_N64A` (heap), not through an image address. (Step 4 is done: its
  numbers and what it left are in section 5.)
- The runtime images' block (`image_block`: `overlay_loader.c`, `alloc_at.c`, `state.c`, 431 crossings) is step 6's.
- The guard can't give steps 2-4 their targets before step 5 (9.3); the counter's arena regions do, and step 5's
  guard run with no fault is the proof that they found them all on these paths.

## 10. Step 3: the level objects (done 2026-11-09)

Wave 3.1's step 3: the level-object nodes, the StatLL lists, the visuals' data word and the target groups' list are
host data with host pointers. Three commits: the visuals' data word first (a layout change alone), then the nodes,
then the views of a visual spelled out for the checks and the checks' rules.

| What | Before | After |
|---|---|---|
| a node (MAT3 `tTarget`) | 0x70 bytes, the N64 layout in the game arena block `gTargetArray` (0x8014D280): `next` and `data` 32-bit window addresses | `struct tTarget` (`pc_game_types.h`; the game C's `tTarget`), 0x78 bytes, MAT3's fields in MAT3's order, `next` and `data` pointers |
| the pool | `gTargetArray`, 130 x 0x70 in the arena | `pc_target_nodes[PC_TARGET_NODES]` (130), decoupled storage data.c defines ([DECOUPLE.md](DECOUPLE.md) 10); `gTargetArray` retired |
| the StatLL lists | the node pool's header flagged `n64`; `pc_statll_link`/`pc_statll_set_link` in the four list functions | every list linked by host pointers (`tStatList`, `tStatLLE`, `tDLLE`); the flag and both helpers gone |
| a visual's data word (`struct pc_visual`) | a u32: a target's visual held its node as a window address (`PC_N64W`) | pointer-sized (`uintptr_t`): a visual is 0x28 bytes, a car record 0x510 (`CAR_OFS`, `VIS_OFS`; [CARS.md](CARS.md) 2, 3) |
| the readers of a node | 19 files with views of their own at N64 offsets (`Target`, `Model`, `CollisionObject` ...), `N64P` on the link and the record | every one reads `tTarget` |
| the target groups' list (GTLD) | a native array of the keys as `void *`, replaced in place by the targets | a native side table of keys and targets (`struct pc_target_group`, [ASSETS.md](ASSETS.md) 4 #14) |
| a bounding box model's box (`tMiniModelData`) | `bboxInfo`, `BBoxCollInfo`'s row as a window address (`PC_N64W`) | `bboxRow`, the row (the model is a heap block of the N64's 64 bytes: 4.3); `MINI_MODEL_BOX` |
| `CollisionTargetPtrs` | `intptr_t[30]` (and `struct Camera *[30]` in the declarations) | `tTarget *[30]` |

The kinds' records stay what they were: heap blocks of the ROM's sizes (or the game's static tables), with no
pointer but a path target's record's path (`tTargetStructPath.pathData`, a PTHD record), which keeps its window
address with the PTHD records' own for step 7. What holds a record is the node's `data`, a pointer now.

**The crossings** (`crossings.py --guard`, the 63 synthetic paths; 1f592ce8 before, the step after):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64P` | 118 | 32,127,083 | 115 | 32,127,059 | the walks of the node list: `PauseTargets.c` (4), `SetTargetDefs` (8), `InitBattlePowerUps.c` (8): 15 into `gTargetArray`, 5 null; the other 4 are the frames around the exit (9.4) |
| `PC_N64W` | 30 | 1,975,908 | 29 | 1,975,900 | `set_target_data`'s record into the node's data (8, heap) |
| `pc_statll_link` | 1 | 8 | 0 | 0 | `GetStatLLE.c` (8, `gTargetArray`) |
| `pc_statll_set_link` | 3 | 7,418 | 0 | 0 | `ClearStatLL.c` (7,353 into `gTargetArray`, 57 null), `GetStatLLE.c` (8) |
| the other ten kinds | 91 | 39,069,530 | 91 | 39,069,530 | none |
| total | 243 | 73,179,947 | 235 | 73,172,489 | 8 sites, 7,458 crossings |

The 8 sites were the step's 7,454 crossings: every one the synthetic paths make into the node pool (the report names
`gTargetArray` nowhere now), and the record a set-up stores. The rest of what the step changed the synthetic
cartridge doesn't reach: it has 8 targets (the battle arena's power-up spots, on 2 paths), no visual of a
target, no box, no target group; the tests run those (`assets_targets`, `assets_battle`, `assets_weapons`), and the
lead's retail scripts. The guard (counting): 4,428,996 faults at 452 instructions before, 4,428,364 at 449 after:
the node reads of `PauseTargets`, `UpdateBattleHealth` and `ClearStatLL` are gone (the guard sees a page's first
access in a frame, so `initTargetStructs`'s own by-name reads show where `ClearStatLL`'s were).

The step's checks, on the counting build's dumps (`crossings.py --report build/crossings/paths --exe
build/count/rush2049pc`): `--zero-kind pc_statll_link,pc_statll_set_link` passes, and so does `--zero-site` for each
of the step's 28 game files (the StatLL functions, `targets/group.c`, `initTargetStructs.c`, `PauseTargets.c`,
`InitBattlePowerUps.c`, `CheckTargetCollision.c`, the collision tests and response, the target visuals' 14
`Animate*`/`Start*` files, the weapon icons' two) and for `gTargetArray` and `BBoxCollInfo`; the control,
`--zero-site src/blob/FindChunk.c`, fails. What the step leaves in its files is the PTHD records' (step 7's):
`targetpaths/group.c`, `GetTargetVelocity.c` and `TargetCollTrigger.c` read a path record's path and a path's points
by `PC_N64P`, which no synthetic path reaches; and `pc_world_chunk` reads GTLD's keys from the file by the chunk's
window address (`pc_n64_ptr`, 171 crossings into the heap, as before).

**The proof.**

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3-ref --skip realtime` (Rocky, Debug, against 68069845) | PASS: ctest 572 of 572, synth_compare `--same --shots --audio` 63 of 63 `same` |
| the visuals' commit alone (Debug): ctest; synth_compare against the base on `battle2p`, `battlequit`, `stunt`, `race`, `demo`, `race12`, `circuit` | 572 of 572; 7 of 7 `same` |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio -j6`) | 63 of 63 `same` |
| `ctest --test-dir build/opt -LE realtime` | 572 of 572 |
| the counting build (`crossings.py --guard --check`) | every run as its script expects, every guarded run's trace its plain run's; the zero checks above |
| the synthetic cartridge | the same file as the base's |
| `gen_data.py --check` (in the build) | OK, 39 decoupled |
| `offset_check.py --strict` (Debug) | no open finding; 3 reviewed: the flat polygons, which the flow types through `FreeStatLLE`'s back link (LAYOUT.md 9.3) |
| `layout_check.py` (Debug) | the base's open findings (3 layout, 1 width: `pc_sample_dma_list`, `sound_seq_players`, the heap's chunk size), none new; the StatLL functions' back link reviewed (LAYOUT.md 7, the rule `pc_statll_link` had) |
| `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | none open; none new |
| `nofixed_audit.py --baseline` | no class grew (`symbols.decoupled` 39 in the baseline) |
| `census.py` | 1,070 of 1,070 compile |
| the views spelled out (the third commit) | each unit's code the same (objdump, Debug and RelWithDebInfo) |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug) against the fixed one (`synth_compare.py --runs 1 --same`, as CI's job) | builds; 63 of 63 `same` |
| the tools' tests (`layout_check`, `offset_check`, `endian_check`, `crossings_tool`, `window_census_tool`, `hash_decoupled_list`, the view checks) after the rules changed | pass |

**What this changes for the later steps.**

- **The plan's "records hold pointers" meets 4.3's accounting.** A record the game allocates on its heap can't grow
  without changing what the heap holds (the bounding box models, `gBBoxModelCount * 64`; the path targets' records,
  `gTargetStructSizes[4]` bytes each). Step 3 keeps the box as its row (an index into a table, as `infoIdx` is), not
  the row's address. Step 7 meets the same with `tTargetStructPath.pathData` (a PTHD record, 72-byte heap records):
  the path's index (the target's `owner` names it at set-up) or a native side record, not a wider heap record.
- **Pointers to PC data in the window's heap blocks.** The battle's always-colliding list (`gAlwaysCol`) and power-up
  slots (`gBattlePowerUps.tS`) are heap blocks that hold nodes, now host pointers into `pc_target_nodes`, which differ
  from one process to the next: the state hash names them by `statehash.c`'s `g_record_arrays` (each node's first
  byte a start, as a named datum's is), or `hash_aslr` fails. A later step that puts pointers to other PC data in
  the heap lists the array there.
- **The car record is 0x510 bytes and a visual 0x28** (`CAR_OFS`, `VIS_OFS`): steps 2 and 4 rebase on it; views of
  the car with `struct pc_visual visuals[21]` and N64-offset padding after it follow by themselves.
- **GTLD's keys are still read from the file by its window address**: `pc_world_chunk` gets the chunk from
  `MBGetFromWad`, a 32-bit address it translates (`pc_n64_ptr`, 171 crossings into the heap); the side table goes
  native with the WAD's records (steps 7, 8). Done in step 8: the world's converter builds it when the file loads.
- `pc_statll_link` and `pc_statll_set_link` stay in the counter's kinds (always 0 now), so a report still lists every
  kind; `port/assets/tests/support.h`'s `TEST_N64_gTargetArray` is unused (left to spare step 2's edits beside it a
  merge conflict).

## 11. Step 5: the arenas as blocks (done 2026-11-09)

Wave 3.2's step 5: each image's data and BSS is a block of host memory, no longer at its N64 addresses in the RDRAM
window. Two commits: the state hash first (it finds the arenas through the loader's tables, so it hashes the same game
on either layout), then the layout, the loader, their tests and checks.

| What | Before | After |
|---|---|---|
| the four arenas | at their N64 addresses in the window: boot 0x800277D0-0x80086A50, game 0x8010FD7C-0x8017A640, the runtime images' in their heap block | blocks in `data.c`'s BSS: `pc_arena_boot` (0x5FA50 bytes, from 0x80027000), `pc_arena_game` (0x6B640, from 0x8010F000), `pc_arena_ovl_a` (0xBE20, from 0x803AF000), `pc_arena_ovl_b` (0x8440, from 0x80393000). Each is 4 KiB-aligned and starts at the page of its arena's first N64 address, so every datum keeps its N64 address's page offset, and so its alignment ([DATA.md](DATA.md) 3) |
| the 1,633 names in them | `.set name, 0x8010xxxx`, absolute (`pc_window + off` in the moved build) | `.set name, pc_arena_game + (address - 0x8010F000)`, relocatable, in every build; the overlays' BSS ends one past their blocks. Each of `data.c`'s 1,645 `.set`s comes to the base's N64 address (block + offset) |
| the loader's tables | an arena's host its N64 address | its block + the page offset; every other table the base's (`data.c` from `pc_runs` on, `data_symbols.txt`, `data_layout.txt`) |
| the loader (`pc_data.c`) | `at_n64_address`, `arena_host` (the guard mode's window), a check that the window is mapped; a word holding an arena's address `[arena]`, reachable | all gone; a pointer into an arena is its block + the offset (undescribed bytes too); every arena above 4 GiB; such a word is "to host storage or code"; the runtime images' blocks cleared after the first load, as their window ranges were |
| `port/data/reserved.txt`, gen_data's reserved rules | (empty since step 4) | gone |
| the GOT fix-up (`absfix.c`) | `aliases.ld`'s and `data.c`'s absolute names (1,802 slots on aarch64) | `aliases.ld`'s alone |
| the state hash (`statehash.c`) | the arenas were the window's bytes | a full-hash region `arenas`, by the window's rules; a host pointer into an arena hashed as its N64 address (the bytes the window held); the curated regions in an arena by the N64 rules; `RUSH_SIM_DESYNC` and `pc_n64_identity` through the tables; the report's `.window` with the arenas at their N64 places |
| the self-test | "game data at its N64 addresses"; the controller and pak checks skipped in the guard mode (`RUSH2049_NOFIXED=guard`) | "game data in its arena's block"; those checks run in the guard mode too (41 of 41) |
| the test programs | `pc_data_test` ran its tables twice, the second time with the arenas in a window it mapped at 0x80000000; `rush2049pc_datacheck` mapped the window | `pc_data_test` once, with blocks; `datacheck` maps nothing and checks each arena's page offset |
| ctest `crossings_paths`, `crossings_guard_stop` | `--expect-faults` | `--check --no-faults` |
| `endian_check.py --check-loader` | read an arena's row of `data.c` with its N64 address (or `pc_window` + offset) as its host | with its block + offset too |

The renderer's game-arena values (`default_lights` in MOVEMEM, `cull_vertices` and `cull_box_vertices_a` in VTX: the
data's own address in the game's native lists, 9.4) are host pointers now by themselves, with no change to any list:
`gfx.native_list` lost the two sites that saw only those, and `pc_n64_ptr` (the renderer's `pcgfx_resolve`) its
game-arena share.

**The crossings** (`crossings.py --guard`, the 63 synthetic paths; 786dec37 before, the step after):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `pc_n64_ptr` | 8 | 18,224,444 | 8 | 16,614,662 | game_arena 1,609,782: the renderer resolving the native lists' arena pointers |
| `gfx.native_list` | 7 | 7,895,283 | 5 | 6,285,501 | game_arena 1,609,782: `default_lights`, `cull_vertices`, `cull_box_vertices_a` |
| the other twelve kinds | 166 | 17,617,809 | 166 | 17,617,808 | none (the last digit: 9.4's frames around the exit) |
| total | 181 | 43,737,536 | 179 | 40,517,971 | |

No kind has an arena region left (`crossings.py --report build/crossings/paths --check --zero-region
game_arena,boot_arena,ovl_a_arena,ovl_b_arena` passes), and no translation sees a host pointer it would cut to 32
bits: `PC_N64A`, `PC_N64W` and `PC_SLOT_SET` have no `host` region.

**The guard** (`RUSH2049_WINDOW_GUARD=arenas,count`, then `arenas`): before, 4,428,357 faults at 449 instructions, the
game's every access by name (9.3); after, 0 faults at 0 instructions, and 0 of the 63 stopping runs stopped
(`crossings.py --build build/count --guard --stop --check --no-faults`: every run as its script expects, every guarded
run's trace its plain run's). The heap's first page is the game arena's last on the N64 (0x8017A000): the heap's own
accesses open it each frame, 2,909 such faults on "the guarded pages' other bytes" before and 46,595 after, since no
access by name opens it first now. They aren't crossings (9.3).

**The retail cartridge's words** (the lead's loader report, on the Pi). 8 u32 data words hold an N64 address into an
arena. Six are numbers that look like addresses: the build date's and time's text, a colour word of
`trophySheenColor`, a word of `track1Parts` that points at an unaligned place. The others are in `rsp_init_dl`
(`Gfx[11]`, the gap to `gWaveSizes`): its words from +0x38 to +0x48 say where `frame_command_buffer_a`,
`rsp_dram_stack`, `rsp_yield_buffer`, `rdp_fifo_buffer` and `rdp_fifo_buffer_end` were on the N64, an RSP task's
buffers. Nothing on the PC reads them:

- no N64 code names that place: no image's symbol table has a symbol from `rsp_init_dl` to `gWaveSizes`;
- the renderer runs the list (`StartGraphics`' first call) to its G_ENDDL, which comes before them; were it to read
  on, their first byte, 0x80, is no F3DEX2 opcode, which it counts as unknown (`--verbose`'s `gfx: task:` lines) and
  never follows;
- the frame's graphics task takes its buffers by name (`EndGraphics`: `rsp_dram_stack`, `rdp_fifo_buffer` ...), and
  the scheduler and the audio task use their own records.

So they stay the numbers the loader converted, which point at nothing on the PC; `include/game/text.h` says so at the
declaration. The report still classes them `[arena]`, now counted "to host storage or code". The retail report's 1,358
undescribed pointers resolve to their block + the offset as they resolved to the window (the loader's arena rule
doesn't look at the description; `pc_data_test` checks one), and the 28 into low memory are kept as they were. For the
lead's runs on the Pi: the census with `--report` as before; the retail scripts in the counting build with `--check
--zero-region game_arena,boot_arena,ovl_a_arena,ovl_b_arena` and in the guard's modes (`--no-faults`), which see what
the synthetic paths don't (songs, objects); and `--verbose`, whose `gfx: task:` lines count no unknown command if
`rsp_init_dl` ends where it should.

**The proof.**

| Check | Result |
|---|---|
| objsame, Debug and RelWithDebInfo, against 786dec37's builds (`~/rush2049pc/w3b-ref`) | 966 of 966 units `same`, and of their -O0 copies (`types_objects.txt`) |
| `data.c` against the base's | the 1,645 `.set`s each at the base's N64 address; from `pc_runs` on the same but `pc_arenas`' hosts; `data_symbols.txt` and `data_layout.txt` the same files |
| `gen_data.py --check` (Debug) | 1,633 in 4 arenas' blocks, 12 inside own storage, 394 own in N64 order, 20 runtime-image C data in their C ranges, 39 decoupled, nothing else in the window: OK (RelWithDebInfo: 14 C data, as before) |
| the gate: `pconly_gate.sh --base ~/rush2049pc/w3b-ref --skip realtime --data-moved` (Rocky, Debug) | PASS: ctest 572 of 572, synth_compare `--same --shots --audio` 63 of 63 `same` (`pause12` varied, alone `same`). The first run had found `endian_check` reading the arena rows by the old form (fixed in the step's commit) |
| RelWithDebInfo against the base's (`synth_compare.py --same --shots --audio --data-moved -j6`) | 63 of 63 `same` (on the final tree `mods_big` varied once, its audio's last subframe between the step's own two runs, PCONLY.md 2's known variation; alone, `same`) |
| `ctest --test-dir build/opt -LE realtime` | 572 of 572 (`assets_rom` skipped: no ROM on Rocky) |
| the loader's summary line, the synthetic cartridge | the base's, its counts by class too: the paths are `same` without `--data-moved` |
| the counting build | above: 0 faults, 0 of 63 stopped, no arena region, the zero checks |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug) | builds; `gen_data --check` OK (the 5 names the window holds at `pc_window` + their offsets); CI's ctest selection 93 of 93; 63 of 63 `same` against the fixed build (`synth_compare.py --runs 1 --same`, as CI's job) |
| the guard mode (`RUSH2049_NOFIXED=guard rush2049pc --selftest --fast`) | 41 of 41, the controller and pak checks among them |
| `offset_check.py --strict` (Debug), `alias_check.py`, `bounds_check.py` (RelWithDebInfo), `census.py` | no open finding (3 reviewed, as before); none open; none new; 1,070 of 1,070 compile |
| PE (no mingw-w64 on Rocky; CI's cross build is the check) | `data.c` assembled for x86_64-w64-windows-gnu with clang: the blocks in a 4 KiB-aligned `.bss`, every name a `B` symbol at its block + offset (`gWaveSizes` at `pc_arena_boot` + 0x7580, `start_overlay_bss_end` one past `pc_arena_ovl_a`) |
| `nofixed_audit.py --baseline` | no class grew; the baseline lowered for the classes the step moved (below) |

The census (`window_census.py`, no compiler pass, the base's build against the step's): `data.arena.boot`, `game`,
`ovl_a`, `ovl_b` 192, 965, 429, 47 to 0; `data.reserved` 0 (since step 4); `data.by_name.game` 608 to 7 (the objects
that reach `aliases.ld`'s names); `data.by_name.port` 4 to 1; `conv.pc_n64_ptr.port` 27 to 25; `n64map.uses.port` 125
to 120; `port.mapping` 22 to 19; `port.rdram_base` 51 to 46; `tests.window` 72 to 67; every other line the same.
`nofixed_baseline.json` (`nofixed_audit.py`, the base's build measured against the step's): `symbols.arena.*` gone
(192, 966, 429, 47), `symbols.by_name.game_objects` 608 to 8, `port_objects` 4 to 1, `symbols.names_reached.*` 1,639
and 33 to 5 and 4, `port.mapping` 27 to 24 (`.tests` 19 to 16), `port.rdram_base` 48 to 45 (`.tests` 40 to 38),
`port.translate` 32 to 30, `port.zext` 21 to 18 (`.tests` 42 to 32).

**What this changes for the later steps.**

- The RDRAM window holds the heap and low memory only; its pages over the arenas' N64 ranges are never touched. Step
  12 takes it away with the heap.
- A pointer to arena data is a host pointer above 4 GiB like any other: a `PC_N64A` or `PC_N64W` of one would cut it
  to its low half, with nothing to fault. The counter shows that as the `host` region of `PC_N64A`, `PC_N64W` or
  `PC_SLOT_SET` (none on these paths): a later step's run should keep those at 0, and so should the lead's retail runs.
- The heap block `load_overlay` reserves for the runtime images (0x8038A400-0x803BB380) has nothing copied into it
  now: it is the heap's accounting alone, which step 6 makes an offset.
- The state hash's `arenas` region: data that later leaves the arenas (step 13's objects) joins the hash as decoupled
  data does.
- `window_census.py` and `nofixed_audit.py` read `data_layout.json`'s `absolute`, which gen_data keeps (empty); its
  `blocks` and `arena_names` lists name the blocks and the names in them, for a tool that wants them.
- Step 13 starts from the blocks: each datum its own object, each walk ended at its array or made a struct, under
  AddressSanitizer.
- The test programs that place game data at N64 addresses in windows of their own (`port/assets/tests/support.*`, the
  20 files of `tests.window`, now 19) link `absfix.c` for that, and are step 11's.
- PE: the blocks' names are section-relative symbols in `.bss`, as the names inside own storage always were there,
  reached through `.refptr` slots that now get base relocations ([WINDOWS.md](WINDOWS.md) 3); CI's Windows cross build
  links and checks them (`gen_data.py --check --pe`). Mach-O (never built): the relative `.set`s need `.alt_entry` as
  [NOFIXED.md](NOFIXED.md) 2.2 says.
- `n64syms.py` needed nothing: its `ARENAS` are the images' N64 ranges, the ROM's format.


## 12. Step 6: the heap's own code native (done 2026-11-09)

Wave 3.2's step 6: the heap's headers, its allocator and its interface hold and take host pointers, with the N64's
sizes; the main heap's memory and the runtime images' block are handed to the game, not named by N64 address; the
scheduler's pending swap is a pointer. Three commits: the heap, the pending swap, then comments and the audit's
baseline.

| What | Before | After |
|---|---|---|
| a chunk's header (MAT3 `tMemoryChunk`, include/game/heap.h) | 32 bytes, N64 layout: `magic`, then `next`, `prev`, `master` as 32-bit window addresses (link enums, the typed `N64P`, `PC_N64W`) | 32 bytes: `next`, `prev` (`tMemoryChunk *`), `master` (`void **`), `size`, `used`, `internal`, `locked`, and the magic as one byte (`HEAP_CHUNK_MAGIC` 0x98, MAT3's word's low byte) at the end |
| a heap's header (`tHeap`) | 0x28 bytes, N64 layout, in the 64 bytes before the first chunk | 64 bytes exactly: `magic`, `numAdditionalMasters` beside it, `next`, `chunks`, `lastChunk`, `endRam` (`u8 *`), the first `tMasterBlock`; MAT3's `emergencyFunc`, which the N64 never set, left out |
| a master table's header (`tMasterBlock`) | 12 bytes, a chunk of 32 | 24 bytes, a chunk of 32; the slots 8 bytes as since POINTERS.md 9.10, now read and written as pointers everywhere |
| the allocator's files | five views of their own (`Block`, `Heap`, `Pool` in alloc_at, ClearMemoryHeap, heap_move_block_low, FindChunk, the free total) and `PC_N64P/A/W` on every link | heap.h's types in all 13; `_Static_assert`s on the three sizes (memsys/group.c) |
| the interface (memory.h, save.h, system.h, frontend.h) | `DeallocateMemory(u32)`, `PurgeableMemory(u32, s32)`, `FindHeap(u32)`, `FindChunk(heap, u32, s32)`, `void *GetPtrSize(u32)` (the size read as a pointer), `ReallocateMemory(u32, u32)`, `lock/unlock_memory(u32)`, `allocate_memory_at(u32, u32)`, `musyx_free(u32)`, the callers' wrappers | MAT3's: `void *` in, `struct Heap *FindHeap`, `u32 GetPtrSize`; the callers pass their pointers (18 game files outside the heap's own drop their `PC_N64A` and slot reads) |
| a handle's slot | read as the N64 address in its first four bytes (`PC_SLOT_N64`: the font's wave, the wave loader, FindChunk, the ghost's buffer; `PC_SLOT_SET`: the free, the compaction) | read and written as the pointer it is; `PC_SLOT_N64/SET` gone from the prelude (the counter keeps the kinds, at 0) |
| the main heap's place | `align32(game_bss_end)` to `osMemSize \| 0x80000000` (an absolute alias, and a K0 address made a pointer) | `pc_heap_memory(osMemSize, &size)` (port/ultra/heap.c): the window's bytes from `N64_HEAP_BASE` (0x8017A640) to osMemSize, the same bytes; `expansion_pak_present` tests osMemSize past 4 MiB (the N64 tested the end past 0x80400000) |
| the runtime images' block | 0x8038A400 to 0x803BB380 (`RUNTIME_IMAGE_*`, `overlay_base`) | `runtime_image_block()`: `PC_RUNTIME_IMAGE_OFFSET` (0x20FDC0) past the main heap's header, `PC_RUNTIME_IMAGE_SIZE` (0x30F80) long (pc_game_types.h): the same place |
| `game_bss_end`, `overlay_base` | absolute aliases (aliases.ld; `data.window_aliases`) | gone: no game object names them |
| the frame buffers | `framebuffer_a`, `framebuffer_b`: s32 window addresses in the arena, `PC_ZEXT`'d into the frame records | `void *`, own storage, as `zbuffer` was |
| the pending swap `__osScPendingSwap` | an s32 window address (`PC_N64A` stored, `PC_N64P` read) | a `void *`, own storage |
| port/ultra/heap.c | walked the headers by window address (`pc_n64_ptr` on every chunk, `pc_n64_addr` on every report) | walks them by pointer; the exit line still gives the heap's window addresses (`window_address`) until step 12 |
| `free_car_palette` | heap_release/group.c, passing the palette record's u32 as the heap's address | car_palette_init.c, beside the palettes: it reads the record's PLHD-layout word (`PC_N64P`, steps 7 and 11) |

**The accounting.** Each header keeps its size, the first chunk is `(heap + 67) & ~31` as before, every size is
rounded to 32, a split leaves at least 64, and the heap's memory is the same bytes at the same addresses: every
block the allocator hands out is where the base's is. The heap's exit line (peak, least free with its largest block,
allocations, the window addresses) is the base's on every path of the gate, and `assets_heap` checks the layout:
the heap at `N64_HEAP_BASE`, its first chunk 64 bytes past it, the 16 handles' table a 128-byte chunk, the splits
and the merges of ReallocateMemory, and the runtime images' block at 0x8038A400 (`PC_RUNTIME_IMAGE_OFFSET` past the
heap) with the test floor's blocks around it.

**The crossings** (`crossings.py --guard`, the 63 synthetic paths; 786dec37 before, the step after):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64P` | 74 | 5,199,708 | 23 | 437,973 | the heap's files (47 sites, 2,986,227), the font's and the wave loader's handle reads (4, 1,680,232), the pending swap (101,426); a new site in `free_car_palette` (car_palette_init.c, 6,150), which passed the palette word to the heap uncounted before |
| `PC_N64A` | 56 | 1,633,596 | 10 | 112,072 | the heap's files (20, 1,398,748), the callers' arguments (25, 21,350), the pending swap (101,426); into the image block 309 to 0 |
| `PC_N64W` | 22 | 100,940 | 1 | 3 | the heap's links (21); left: a ghost header's start (GhostStart.c) |
| `PC_SLOT_N64` | 5 | 1,682,453 | 0 | 0 | all |
| `PC_SLOT_SET` | 2 | 216 | 0 | 0 | all |
| `pc_n64_ptr` | 8 | 18,224,444 | 6 | 15,900,340 | port/ultra/heap.c's walk and its floor (2,324,104) |
| `pc_n64_addr` | 3 | 25,283 | 1 | 4 | port/ultra/heap.c's reports (25,279) |
| the other seven kinds | 11 | 16,870,896 | 11 | 16,870,896 | none |
| total | 181 | 43,737,536 | 52 | 33,321,288 | 129 sites, 10,416,248 crossings |

`--check --zero-kind PC_SLOT_N64,PC_SLOT_SET` passes, and `--zero-site` gives 0 for each of the heap's files
(memsys, heap_release, FindChunk.c, FindHeap.c, AllocateMemoryHeapHigh.c, memsys_free_total, largest_heap_chunk,
destroy_heap, pointer_counters, purgeable_memory, port/ultra/heap.c), for scheduler.c, and for the callers whose only
crossings were the heap's (slot_sound, loadwave.c, musyx_alloc.c, AudInit3D.c, AudioSfxInit.c, save/AudioInit.c,
save/save.c, state.c, trackscr.c); `--zero-region image_block` passes; the control, SetupCarColor.c, fails (its two
palette words). The guard (counting): 4,428,386 faults at 449 instructions before, 4,428,420 at 449 after (the
per-frame variance of 9.4).

What crosses into the heap now is what the game keeps in its blocks, not the heap: the WAD's records and their
relocation (`MBSetupWad.c`, `waves.c`, `InitTextureAnimation.c`: step 7), the world's chunk (`NewWorld.c`,
`loadtrack.c`: step 8), the listeners (`UpdateListeners.c`, `StartListeners.c`, `AudRemoveListener.c`: step 9),
the car palettes' word (`AnimateBrakeLights.c`, `startcar.c`, `SetupCarColor.c`, `car_palette_init.c`) and a ghost
header's start (`GhostUpdate.c`, `GhostStart.c`, `NewGhost.c`, the ghost's buffer in `ghost_files.c`): step 11.

**The proof** (Rocky, x86-64, GCC 14; the base is 786dec37's builds in `~/rush2049pc/w3b-ref`):

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3b-ref --skip realtime --data-moved` (Debug) | PASS: ctest 572 of 572, synth_compare `--same --shots --audio` 63 of 63 `same` (`solo` varied once in its last subframe and was `same` alone, PCONLY.md 2) |
| the heap's exit line (peak, least free and its largest block, allocations, the window addresses), base against step, both runs of every one of the 63 paths | the same, 63 of 63, the memory paths (`mem64`, `membig`, `membig8`, `memhigh`, `memhigh2p`) among them |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio --data-moved -j6`) | 63 of 63 `same`; the heap's exit line the base's on all 63 |
| `ctest --test-dir build/opt -LE realtime` | 572 of 572 |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug): its CI subset (`ultra_selftest.*`, `pc_data_*`, `synth_.*`), and against the fixed build (`synth_compare.py --runs 1 --same`) | builds; 93 of 93; 63 of 63 `same` |
| the counting build (`crossings.py --guard --check --zero-kind PC_SLOT_N64,PC_SLOT_SET`) | every run as its script expects, every guarded run's trace its plain run's; the zero checks above |
| `offset_check.py --strict` (Debug) | no open finding (3 reviewed, as before) |
| `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | none open; none new |
| `census.py` | 1,070 of 1,070 compile |
| `nofixed_audit.py --baseline` | no class grew past the baseline, which this step raised for `symbols.own` (394 to 397: the frame buffers and the pending swap), `port.rdram_base` (48 to 52) and `.tests` (40 to 41): the heap's place in the window is port/ultra/heap.c's now. Down: `symbols.aliases.rdram` 4 to 2, `src.n64p` 77 to 6, `src.n64p_field` 43 to 14, `src.n64w` 74 to 23, `src.def.translate` 19 to 10, `cc.implicit.ptr_to_int` 25 to 14 |
| the loader's summary line (synthetic cartridge) | 3 more symbols (own storage), 1 more null pointer (the pending swap's word): `--data-moved` |
| the int/pointer conversions of the heap's callers (each recompiled with `-Wint-conversion`) | none from the heap's interface; what is left is the records' (a ghost header's `start = NULL`, the track map's LOD words) |

**The census** (`window_census.py`, Debug, with the compiler pass; 786dec37 before): `api.u32_address` 20 to 5 (left:
the WAD's, the display lists' and a sound helper's: steps 7, 9, 10), `heap.window_symbols` 8 to 2 (the boot's image
inflate, `gDecompressedDataStart`), `ovl.image_block` 4 to 0, `data.window_aliases` 4 to 2, `fields.link_enum` 52 to
15 and `link_enum_types` 26 to 3, `conv.PC_N64A.game` 82 to 19, `conv.PC_N64P.game` 43 to 35, `conv.PC_N64W.game` 73
to 22, `conv.file_macros.defs` and `uses` 32 and 128 to 16 and 38, `conv.cc.implicit` 46 to 35, `conv.pc_n64_ptr.port`
27 to 22, `text.window.game` 134 to 118, `data.arena.game` 965 to 963 and `boot` 192 to 191 (`context.own` 394 to
397). Up: `port.rdram_base` 51 to 52 (port/ultra/heap.c), `tests.window` 72 to 73 (test_heap's check of the heap's
place), `context.n64map.defs` 25 to 26 (`N64_HEAP_BASE`). `heap.allocate_at` stays at 1: the overlay loader's call,
now with the block's pointer.

**What this changes for the later steps.**

- **Step 12 has one place to change for the heap's memory**: `pc_heap_memory` (port/ultra/heap.c) hands the game its
  heap; nothing in the game names where it lies. To keep the accounting, the block it hands out must keep the
  N64's alignment of the heap's header (0x8017A640 is 64-byte aligned, and the frame buffers align to 64 inside their
  blocks, `ALIGN_64`; a block 64 KiB aligned with the header at the same offset in it keeps every alignment the N64
  had), and its size is `osMemSize` less `N64_HEAP_BASE`'s offset, as now. The exit line's window addresses
  (`window_address`, `PcHeapStats.base`/`end`/`floor`) and `heap_check.py`'s `--memory` test of the end go with it,
  and the test floor (`RUSH2049_TEST_HEAP_FLOOR`) is a place in RDRAM only while the heap is in the window.
- **The state hash meets the heap's pointers in step 12.** The headers now hold pointers into the heap (and the
  master slots pointers to their data, the frame buffers' and the pending swap's own storage pointers into it): in
  the window they are the same in every process; out of it they differ with ASLR, so `statehash.c` must hash the
  heap's headers and slots as offsets in the heap (`hash_aslr` is the check).
- **What the game keeps in its heap blocks** still holds 32-bit addresses, each its step's: the WAD records and their
  relocation, `MBOffsetWad`'s delta, the font letters' kern rows (`FONT_SLOT`), the track map's LOD records (step 7);
  the world (step 8); the listeners, the registries (step 9); the car palettes' word (`car_palettes[].data`, a PLHD
  record's layout, now freed by `free_car_palette` in car_palette_init.c), a ghost header's `start` and the ghost's
  buffer (`MenuData.buffer`, set by `PC_N64W` from the handle's slot) (step 11). Each of those reads its block's
  pointer with `PC_N64P` and hands the heap the pointer.
- **A heap record that holds pointers to PC data** still needs `statehash.c`'s `g_record_arrays` (section 10); the
  heap itself adds none.
- `heap.allocate_at` in the census counts the overlay loader's call of `allocate_memory_at`, which now takes the
  block's pointer: the kind stays at 1 while the heap reserves the images' block. `port/assets/tests/support.h`'s
  `TEST_N64_game_bss_end` is unused (left to spare step 5's edits of the test windows a conflict).

## 13. Step 7: the WAD's records native (done 2026-11-09)

Wave 3.3's step 7: the game reads the model WADs' records (OBHD, TXHD, PLHD, PTHD) and the fonts' letters as native
records the converters build beside the file, MAT3's fields with host pointers, one declaration each; it no longer
rebases a file's records in place. Three commits: the records and their readers, the audit's baseline, the documents.

| What | Before | After |
|---|---|---|
| a WAD's records (`port/assets/wad.c`) | swapped in place in the file, the N64's sizes (0x58, 0x24, 0x18, 0x24) with 32-bit offsets, which `SetupWaveModel` rebased in place to window addresses (`MBSetupWad`'s table, `MBGetFromWad`, `+= (u32)base`; `MBOffsetWad` for a moved handle) | built beside the file when it loads, `struct pc_wad_records` (`pc_game_types.h`): `struct tModelDef` (0x78, its four `struct tLod` of 0x18), `struct tTexture` (0x28), `struct tPaletteDef` (0x20), `struct tBasicTargetPath` (0x38), MAT3's fields in MAT3's order, host pointers into the file (display lists, vertices, points; texels and colours past IMAG in 32-bit arithmetic) or to the trigger's record; kept by the file's heap block (`records.c`). The file's table of contents and record chunks stay as the file has them (big-endian); the PATH points are swapped in place, as before |
| `SetupWaveModel` (`waves.c`) | the four tag lookups and rebases, `lastPtr` for `MBOffsetWad` | the tables from `pc_wad_records(file)`; it relocates only the texture loads and the display lists, in the file, in the N64's form, with the file's and IMAG's N64 addresses (`PC_N64A`: step 10's) |
| `MBSetupWad`, `MBGetFromWad`, `MBOffsetWad`, `MBFindInWad`, `relocate_wave_model`, `WadHandle`/`WadEntry`/`WadFileHeader`, `WADP`, `PART_DL` | the N64's WAD functions and types | gone (`relocate_wave_model`, `SetupWaveModel` with `first` clear, had no caller; a model WAD is never a handle) |
| the records' views | 42 game files read them through views of their own: `tTexture` of 36 bytes or its first fields (the blits, the fonts, the track map, the effects' strides), `tModelDef` of 88 (`LodDef`, `Part`, `MapLod`, `RomObjDef`), `Palette`, `Resource24`, `Resource32`, `tPalette`, `Texture`, `TexDef`, three `tBasicTargetPath`s, `TargetPath`/`PathData` (TargetCollTrigger) | every one reads the shared type; the strides (36, 24, 88) are the types' |
| the track map's levels (`trackscr.c`) | `mapObj`, `saved_map_lods`: `MapLod`, 0x10 with two u32 pointers, in overlay A's arena | `tLod` (0x18), own storage (BSS, cleared on the image's load as before); copied whole into and out of the map models' records |
| the car palettes `car_palettes` | `CarPalette` in the PLHD layout (24 bytes, `data` a u32 window address: `PC_N64A` stored, `PC_N64P` read in five files), in the game arena, declared `EXTENT(1)` | `tPaletteDef` (MAT3's palette record, `palBase` a pointer), own storage of 52: a race's slots or the car select's 4 players x 13 cars (the N64's range to `gRealEdges`, as `gCarParts` and `gCarPaintJobInited`; with 12 the car select faulted) |
| the blits' palettes `healthPal`, `n64FogPal` | `tBlitPalette` (step 4's, the native layout already) | `tPaletteDef`: the WAD's type |
| the TLUT cache `zoid_current_tlut` | `s32`, compared with a palette's u32 word | `void *` (own storage) |
| a path target's record (`tTargetStructPath`, a heap record of `gTargetStructSizes[4]` = 0x48 bytes) | `pathData`, the PTHD record's window address (`PC_N64W` stored, `PC_N64P` read in 4 files) | `pathIdx`, the path's index (`TARGET_PATH`, track.h): the heap's accounting keeps the record's size (section 10) |
| a PTHD record's `parent` | the key the world's target groups know the path by, until `ResetPath` wrote the target's record's address over it | `parent` the record (NULL until set), `key` the file's word, which `SetupTarget` gives the target. On the N64 a restart's `SetupTarget` read the record's address as the key: only `SetTargetDefs`' group search reads a target's key, and a restart skips it |
| a path's trigger (`trigPtr`) | the file's offset plus the file's address: 0 became the file's own address | the record of the same chunk it names; NULL for 0 (rush2049pc_assetcheck checks that every path whose trigger the game reads has one) |
| a font's letters (`port/assets/font.c`, `SetupFontData`) | the file's 12-byte letters, the first word a slot `SetupFontData` filled with the kerning row's window address (`FONT_SLOT` = `PC_N64A`; `PC_N64P` in the text draw and `HAL_GetTextWidth`), again whenever the heap moved the file | `struct FontLetter` (0x10) built beside the file with a copy of the kerning table, `kerns` a pointer to its row (`pc_font_letters`); nothing written into the file |
| a handle's block moved by the heap (`ClearMemoryHeap`, `heap_move_block_low`) | the converters' records were kept by the old block and dropped when it was freed | `pc_heap_block_moved(from, to, size)` (heap.h, `assets.c`): the records follow the block (`records.c`); only the font data is a handle, and its letters hold no pointer into it |
| `records.c` | a file's records of one kind | a file may have two kinds (a world WAD: its WAD's and its world's): a keep drops what an earlier file left in the memory, not the same block's other kind |
| the texture animation's flipbooks | a frame's image copied as a 32-bit word between a texture's `texBase` and a load list's `G_SETTIMG` | the same in the fixed build; a texel frame's pointer goes into a list as `PC_N64A` of it (`FRAME_IMAGE_IN_LIST`), so the moved build writes the window address and not a cut host pointer |

**The crossings** (`crossings.py --guard`, the 64 synthetic paths; 6543cff7 before, the step after):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64P` | 22 | 444,726 | 9 | 119,224 | `waves.c`'s record lookups and rebases (137, 138, 139, 141, 145, 157: 68,676), `MBSetupWad.c:24` (8,655), the car palettes' word (`AnimateBrakeLights.c` 237,007, `SetupCarColor.c` 13,176, `car_palette_init.c` 6,250, `startcar.c` 338), `InitTextureAnimation.c` (55) |
| `PC_N64A` | 9 | 113,513 | 3 | 8,790 | `MBSetupWad.c` (24, 25, 28: 56,598), `waves.c`'s rebases (155, 156, 157: 50,064), `car_palette_init.c:36` (6,716); new: `waves.c:93`, the file's N64 address for the display lists' relocation, once a wave (8,655; step 10's) |
| the other twelve kinds | 16 | 30,004,733 | 16 | 30,004,733 | none |
| total | 47 | 30,562,972 | 28 | 30,132,747 | 19 sites, 430,225 crossings |

No crossing went to the arenas or the image block (none did before), and the guard
(`RUSH2049_WINDOW_GUARD=arenas,count`) found 0 faults at 0 instructions before and after; each of the 128 runs ended
as its script expects, every guarded run's trace its plain run's. `--check --no-faults --zero-site` passes for each of
the step's 41 game files but `waves.c` (whose one site is the display lists' relocation, step 10), and for
`MBSetupWad.c`, `wad.c`, `font.c` and `records.c`; the control, `UpdateListeners.c` (step 9's listeners), fails. The
synthetic cartridge's WADs have no texture loads (TXLD), no track paths (PTHD) and no flipbook of two kinds, so the
texture loads' relocation (`PC_N64A` of IMAG, `waves.c:91`), the path targets and `FRAME_IMAGE_IN_LIST` make no
crossing here; the tests run them (`assets_wad_game`, `assets_targets`), and the lead's retail scripts.

**The census** (`window_census.py`, the counting builds, with the compiler pass; 6543cff7 before): `conv.PC_N64P.game`
34 to 6, `conv.PC_N64A.game` 18 to 8, `conv.PC_N64W.game` 22 to 20, `conv.file_macros.defs` and `uses` 16 and 38 to 7
and 22, `conv.cc.implicit` 28 to 13, `conv.cc.ptr_to_int` 29 to 28, `fields.link_enum` 15 to 14, `link_enum_types` 3 to
2, `fields.u32_address` 25 to 2 (the two left: a frame record's colour buffer and a ghost's start, step 11),
`api.u32_address` 5 to 4 (the WAD's API gone), `n64map.uses.port` 118 to 116, `port.rdram_base` 47 to 46,
`tests.window` 65 to 63, `text.window.game` 116 to 83, `context.lit_bit.game` 19 to 17; up: `conv.silent.trunc` 2 to 3
and `conv.silent.zext` 14 to 15 (the texture animation's frame images in a load list's N64 form, explicit casts where
the compiler converted silently before: `conv.cc.implicit` fell by 15), `context.own` 397 to 401 (`car_palettes`,
`zoid_current_tlut`, `mapObj`, `saved_map_lods`).

**The proof.**

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3c-ref --skip realtime --data-moved` (Rocky, Debug, against 6543cff7) | PASS: ctest 575 of 575, synth_compare `--same --shots --audio` 64 of 64 `same` |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio --data-moved -j6`) | 64 of 64 `same` |
| `ctest --test-dir build/opt -LE realtime` | 575 of 575 (`assets_rom` skipped) |
| the whole Debug ctest before the gate (`-LE realtime`) | 575 of 575 (`assets_rom` skipped: no ROM on Rocky) |
| the counting build (`crossings.py --guard`, 128 runs) | every run as its script expects, every guarded run's trace its plain run's; 0 faults; the zero checks above |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug): CI's subset (`ultra_selftest.*`, `pc_data_rom`, `pc_data_synthetic`, `synth_.*`), and against the fixed build (`synth_compare.py --runs 1 --same`) | builds; 94 of 94; 64 of 64 `same` |
| `offset_check.py --strict` (Debug) | no open finding (3 reviewed, as before) |
| `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | none open; none new |
| `census.py` | 1,067 of 1,067 compile (the four WAD functions' files deleted) |
| `nofixed_audit.py --baseline` | no class grew past the baseline, which this step raised for `symbols.own` (397 to 401: the four own storages above), `src.trunc` (2 to 3) and `src.zext` (14 to 15: the base had 15 already, `AudioInitSys.c:62` from 02c32122; this step swaps `MBSetupWad.c:9` for the texture animation's frame image), and lowered for the classes it moved (`cc.implicit.int_to_ptr` 18 to 5, `cc.implicit.ptr_to_int` 66 to 8, `src.n64p` 6 to 5, `src.n64w` 23 to 21, `src.def.translate` 10 to 3 ...: the commit lists each) |
| the game C's pointer/integer warnings (every game unit compiled again with `-Wint-conversion -Wincompatible-pointer-types`, the base's against the step's) | none new; 25 of the base's gone (the records' views, `MBSetupWad`, the font slot, the track map's `u32` fields, the opaque `TexDef`/`Texture` views) |
| boot without a ROM (CI's) | reaches wave 0 and stops where the base stops (`DrawNoControllers` on the blank cartridge), with the same wave loads |
| the loader's tables (`data.c`, `data_layout.json`) | the base's but for the four symbols now own storage (`gen_data --check`: 1,626 in the arenas' blocks, 401 own); the loader's summary line counts them (`--data-moved`) |
| `assets_wad`, `assets_wad_game`, `assets_font`, `assets_targets`, `assets_cars`, `assets_endian`, `assets_fxcars`, `assets_mods`, `assets_dispatch`, `assets_world`, `hash_aslr`, `replay_same_*`, `net_session_*` | pass |

**What this changes for the later steps.**

- **Step 10 (the display lists) starts from the records.** A level of detail's `displayList` and a listed texture's
  `texBase` (no `TEX_IMAGE_DIRECT`) are host pointers to lists in the file in the N64's form; when step 10's converter
  builds the native lists it can point the records at them as it builds both (one pass over the file).
  `SetupWaveModel` keeps the two N64 addresses the lists' relocation needs (`PC_N64A` of the file and of IMAG, once a
  wave), and the texture animation two places that move an image between a record and a list's `G_SETTIMG`: a listed
  frame's image is the N64 word its own list had (`displaylist_get_texture_image`, kept in a pointer), and a texel
  frame's is written into a list as `PC_N64A` of its pointer (`FRAME_IMAGE_IN_LIST`). Native lists make both plain
  pointers. The track map's own lists, which it puts into its models' first level, are native already.
- **Step 11 has less.** The car palettes (`car_palette_init.c`, `SetupCarColor.c`, `startcar.c`,
  `AnimateBrakeLights.c`), the font's kerning rows (`HAL_GetTextWidth.c`, `hal_text/drawtext.c`) and the track map's
  levels (`trackscr.c`) hold pointers now; what step 11 has of those files is their other holders.
- **The native records are in no hashed region.** The state hash covered the WAD's records as heap bytes (the window);
  the converters' records live in host memory the hash doesn't walk (as step 8's do). What changes in them during a
  race is a path's flags, its target's record, a texture's `texBase` (the flipbooks) and a level's texture (sparks:
  `ZOID_ChangeTexture`); the targets' positions and records, which follow from the paths, are hashed (the target
  nodes, the heap). The tables that point at the records (`gRomObjDefPtrs` ...: own storage) hash them as "a heap
  pointer", without the address. `hash_aslr` passes. A later step can hash the records as the curated regions are
  (records.c knows them all).
- **No heap block holds a pointer to a native record**: a path target's record names its path by index; every other
  holder (the scene nodes, the blits' list, the font textures, the wheel textures, the flipbook frames) is own storage
  with typed pointers or PC data.
- **Step 12**: the records' pointers into the file are host pointers already; when the heap leaves the window nothing in
  them changes. A model WAD loaded as a handle (none is) would need its records' pointers moved with it
  (`pc_records_move` only re-keys: the font's letters have no pointer into their block).
- `SetupWaveModel`'s `first` stays (MAT3's) and is always 1; `gWaveInstance[].lastPtr` is unused.
- The census's WAD rules (`WADP`, `MBGetFromWad`) find nothing left; `fields.u32_address` and `api.u32_address` are
  down to steps 9-11's.



## 14. Step 9: MusyX native (done 2026-11-09)

Wave 3.3's step 9: what in the MusyX library and the sound manager held 32-bit window addresses holds pointers, the
audio interpreter reads the game's records as they are, and the samples, which stay in the cartridge, are named by
their cartridge address apart from anything in RAM. Eight commits: the synthetic cartridge's song first (so the
sequencer runs on the synthetic paths), then the interpreter, the registries, the voices, the songs and samples, the
listeners, the song made to fit the music's voices, and the tables that make the cartridge's sound heard (below).

| What | Before | After |
|---|---|---|
| the audio interpreter (`port/audio/musyx.c`) | two forms: N64-form records (32-bit addresses through `pc_n64_ptr`, maybe big-endian), and native ones whose pointer fields below 4 GiB were taken for N64 addresses (`resolve`, `n64ptr`); `PC_AUDIO_TASK_FORM`, `pc_audio_set_task_form` | the game's records only (`PcMusyx*`, hardware.c's structs), every pointer read as it is: no translation, no 4 GiB rule. The task capture still writes the microcode's N64 form ([AUDIO.md](AUDIO.md) 8); `test_audio_window`, which tested the N64 form in a window of its own, went |
| the registries (`registry.c`) | `{u32 payload; u16 id; u16 refs}` (8 bytes; layers 12): the payload a window address (`PC_N64A` in, `PC_N64P` out); searched with a stride of 8 and the id read at +4 | `ResourceRecord {void *payload; u16 id; u16 references}` (macros, tables, keymaps) and `LayerRecord {void *payload; u16 id; u16 count; u16 references}`, 16 bytes, searched as those types; own storage of the host layout, each its N64 room (2048, 256, 256, 2048: `include/game/audio.h`'s extents, which had stopped at two stale labels of the symbol file) |
| a synth voice (`pc_mx_voice`) | 0x1A0 bytes, the N64 layout: the macro's steps, GOSUB's return and the key-off trap at +0, +4, +8, +0xC, +0x1C, +0x20 as window addresses (`enum n64p_MacroCommand`, `N64P`, `PC_N64W`, `N64_NEXT`), the handle node at +0x18 as its N64 address (`HANDLE_NODES_N64`, `pc_voice_node`) | every view (56, in macros.c, synth.c, voices.c, controllers.c, api.c) begins with `src/rom/sound/musyx.h`'s `SynthVoiceHead`: six `MacroCommand` pointers, the chain's two voice ids, the handle node (`struct HandleNode *`), 0x40 bytes; the fields after it keep their N64-offset names, 0x1C further on; a voice is 0x1C0 bytes (`PC_MX_VOICE_SIZE`). The one raw offset (`sound_synth_get_tick_rate`'s byte 0x4B) reads its view's field. ctest `views_musyx_voice` (`view_check.py`, given `--ref`) checks every view against macros.c's whole one |
| the sequencer (`sequencer.c`) | the song's offsets added to its address cut to 32 bits (`(u32)arr + offset`); `sound_start_song` took the song and options as `uintptr_t`; a slot kept a deferred request's result word as a `uintptr_t` | the song's pointer plus the offsets (`(u8 *)song + offset`); pointers (`SongHeader *`, `SequencePlayParams *`, `unsigned int *pendingFF0`) |
| a sample | one pointer-sized field held a cartridge address or a RAM pointer (the sample directory's u32 `data`, `RegisteredSamples.base`, four views of the sample info, `SoundHwVoice.data`), told apart by a range test (below 4 GiB and not KSEG0); the bank's base `PC_N64A(sample_bank_rom)`, a cartridge alias | named for what it is: a sample in the cartridge by its 32-bit cartridge address (`SampleRecord.cart_address`, `RegisteredSamples.cart_base`, `SoundHwVoice.cart_address`), a stream's buffer in RAM by its pointer (`data`), carried apart by one record (`include/sound_sample_record.h`'s `SoundSampleInfo`); the hardware layer fetches by the cartridge address, the caches keyed by it, and no range test says which. AudioInitSys hands MusyX the bank's ROM offset (`SAMPLE_BANK_ROM_OFFSET`, `musyx_samples` a u32), which the library makes the cartridge address through the game's service (`sound_hw_cart_address`, `musyx_cart_address` on u32s); `sample_bank_rom` and its alias went |
| the speech bank (`speech_set_bank`, unused by the game) | a cartridge bank told from one in RAM by `(u32)source & 0xF0000000 != 0x80000000`, which a host pointer would fail | by the cartridge's own ranges (`sound_hw_is_cartridge`: an offset below 0x10000000 or a cartridge address); the speech code still carries the cartridge addresses it reads from in pointers (`dma_queue.c`) |
| a player's listener (`UpdateListeners.c`, `StartListeners.c`, `AudRemoveListener.c`) | the listener's window address in the record's 4 bytes at +0x44 (`PC_N64A`, `PC_N64P`) | the listener's number in `gListeners`, -1 for none (MAT3's pointer doesn't fit the 80-byte record's 4 bytes there) |

**The synthetic song** (`tools/port/synth_rom.py --sounds`, [SYNTH.md](SYNTH.md) 1). The default cartridge has no
sounds, so the gate's `--audio` compares silence and the library's voices never run on it; the `--sounds` cartridge
(ctest's `rush2049_synth_snd.z64`, the voices and engines tests') had sfx but no songs, and was silent too: MusyX's
volume and pan curves and semitone ratios (gains.c) were zero, so every gain was 0 and every note but a sample's own
key played at rate 0. It now has those tables, computed from what they mean (2^(n/12), (v/127)^2, 1, -3 dB, 0), so a
race on it is heard (`race`: 10% of the WAV's samples nonzero, peak 23,640), and a song group (16)
that every song uses: five macros (a note that branches by key with SPLIT_KEY, sets an envelope from a table, traps
its key off, calls a subroutine that starts the sample and RETURNs, starts a child voice with PLAY_MACRO, LOOPs a
WAIT, then waits for the key off; the release, the subroutine, the child and the high note), an envelope table, a
keymap and a layer list of two, normal and drum pages, a MIDI setup for each song id; and every song wave holds one
song: five tracks (the note macro, the keymap, the layer list, the drums on channel 10, drums once and an end),
three patterns with notes, a program change, controllers and channel pressure, pitch-bend and modulation streams, a
tempo track (120, then 140 beats a minute) and loops every two bars. The races play it (57 of the 64 paths). Along
the 64 paths it runs every voice-head pointer the interpreter stores (SPLIT_KEY, GOSUB, TRAP_KEY_OFF, LOOP, the step
loop, the macro start), the keymap, layer, table and macro registries' adds and finds, the sample lookup, and the
sequencer's tracks, patterns, streams, tempo track and loops. RETURN, the key-off trap's jump and the chain's links
run but cross nothing countable. No track holds two notes at once and only the note macro's and the layers' tracks
start child voices, so the song stays within the music's 16 voices: the voices tests (`voices_check.py`) count every
start MusyX refuses, and a song that needed more (before the tables, when a child voice's sample never ended) failed
`synth_voices_race12` (the 96-voice run refused as many as the 64-voice one: the songs' quota doesn't grow with the
voices). The base and the step play it the same (below).

**The crossings** (`crossings.py`, the 64 paths; 6543cff7 before, the step after). The default cartridge, with the
guard:

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64P` | 22 | 444,726 | 20 | 346,625 | `UpdateListeners.c` (98,092), `AudRemoveListener.c` (9) |
| `PC_N64A` | 9 | 113,513 | 7 | 113,378 | `AudioInitSys.c` (64, `cart`: the sample bank), `StartListeners.c` (71) |
| `pc_n64_ptr` | 5 | 14,508,300 | 4 | 13,503,358 | the interpreter's `n64ptr` (1,004,942) |
| `audio.n64ptr` | 1 | 1,004,942 | 0 | 0 | all (`resolve`) |
| `audio.field` | 2 | 1,004,942 | 0 | 0 | all (`read_sfd`, `voice_output`) |
| the other nine kinds | 8 | 14,494,549 | 8 | 14,486,549 | none (the frames around the exit, 9.4) |
| total | 47 | 30,562,972 | 39 | 27,449,910 | 8 sites, 3,113,062 crossings |

The `--sounds` cartridge (the song and the tables), every path (the base's counting build and the step's, both on
it):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64P` | 30 | 821,276 | 20 | 346,625 | the macro registry's find and the table, keymap and layer finds (`registry.c` 561, 615, 629, 650: 35,870), the interpreter's step fetch (`macros.c` 1482-1484: 335,655), LOOP's step (481: 5,025), the listeners (98,101) |
| `PC_N64A` | 14 | 114,305 | 7 | 113,378 | the registries' adds (`registry.c` 156, 217, 278, 499), a sample's cartridge address (375, `cart` 128), the bank's (`AudioInitSys.c`, `cart` 64), `StartListeners.c` |
| `PC_N64W` | 11 | 174,268 | 1 | 3 | the voice head's stores: SPLIT_KEY, GOSUB, TRAP_KEY_OFF, LOOP, the step loop, the macro start (`macros.c` 346, 347, 416, 417, 433, 434, 481, 1484, 1736, 1737: 174,265); left the ghost's (step 11) |
| `pc_n64_ptr` | 5 | 15,981,538 | 4 | 13,486,906 | `n64ptr` (2,494,632) |
| `audio.n64ptr` / `audio.field` | 1 / 6 | 2,494,632 each | 0 / 0 | 0 | all: `read_sfd`, `read_voice` (its four pointer fields), `voice_output` |
| the other kinds | 7 | 13,470,094 | 7 | 13,470,094 | none |
| total | 74 | 35,550,745 | 39 | 27,417,006 | 35 sites, 8,133,739 crossings |

What the step still crosses on it is the same sites as on the default cartridge (the renderer's, and the holders
steps 7, 10 and 11 take): none is the sound's.

`--check --zero-kind audio.n64ptr,audio.field` passes on both (and `--no-faults`: the guard 0 faults at 0
instructions, as before), and `--zero-site` gives 0 on both for `src/rom/sound/`, `src/rom/speech/`,
`UpdateListeners.c`, `StartListeners.c`, `AudRemoveListener.c`, `AudAddListener.c`, `AudioInitSys.c`,
`AudioSongPlay.c`, `musyx_cart_address.c`, `port/audio/musyx.c` and `n64ptr`; the control, `car_palette_init.c`,
fails. No region `cart` is left in any kind.

**The proof.**

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3c-ref --skip realtime --data-moved` (Rocky, Debug, against 6543cff7) | PASS: builds, ctest 575 of 575 (`assets_rom` skipped: no ROM on Rocky), 64 of 64 `same` |
| the `--sounds` cartridge (the song and the tables), base against step: `synth_compare.py --rom <it> --same --shots --audio --data-moved -j6`, Debug and RelWithDebInfo | Debug: 64 of 64 agree, 63 `same` and `mods_off` `varies` (the base's own two runs, apart at the exit); `mods_off` alone varied once more the same way (one build's two WAVs, 768 bytes apart in length), then was `same` in every later run: the base against the step three times (the last with three runs a side), the base against itself twice and the step against itself once. RelWithDebInfo: 64 of 64 `same` |
| the same at the voices commit and at the songs-and-samples commit, on the first song (silent, before the tables) and on the default cartridge, Debug | 64 of 64 `same` each |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio --data-moved -j6`, the default cartridge) | 64 of 64 `same` |
| `ctest --test-dir build/opt -LE realtime` | 575 of 575 (`assets_rom` skipped: no ROM on Rocky) |
| the realtime tests that run the sound (`audio_realtime`, `audio_realtime_game60`, `paced_audio`, `paced_realtime`), alone under `flock ~/rush2049pc/.realtime.lock`, Debug and RelWithDebInfo | RelWithDebInfo: 4 of 4. Debug: `audio_realtime`, `audio_realtime_game60`, `paced_realtime` pass; `paced_audio` failed once (one underrun after the first second, the host's lag; its trace and sound the same as `--fast`'s), then passed twice, and the base's build failed it once the same way (one underrun) between them |
| the voices and engines tests on the `--sounds` cartridge (`synth_voices_race12`, `_race2p12`, `synth_engines_*`), Debug and RelWithDebInfo | pass, 7 of 7 in each (the cartridge's test with them); no run refuses a start, 96 voices or 64 |
| `assets_musyx` (the game's readers in ordinary host memory now: no mapping below 4 GiB), `assets_mxvoices`, `assets_sound`, `assets_input`, `assets_casts`, the 15 `audio_*`, `views_musyx_voice` (56 views against macros.c's whole one) | pass |
| the counting build (`crossings.py --build build/count --guard --check --zero-kind audio.n64ptr,audio.field --no-faults`) and the `--zero-site` checks above | pass |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug): CI's subset (`ultra_selftest.*`, `pc_data_*`, `synth_.*`) and against the fixed build (`synth_compare.py --runs 1 --same`) | builds; 94 of 94; 64 of 64 `same` |
| `offset_check.py --strict` (Debug) | no open finding (3 reviewed, as before) |
| `alias_check.py` (RelWithDebInfo) | none open |
| `bounds_check.py` (RelWithDebInfo) | one new key, reviewed (`bounds_reviewed.txt`): `sound_track_volumes` in `sound_seq_start`, indexed by the loop optimizer, j below the declaration's 64 |
| `census.py` | 1,071 of 1,071 compile |
| `nofixed_audit.py --baseline` | no class grew past the baseline, which this step moved: up `src.trunc` 2 to 3 and `src.zext` 14 to 16 (`speech_set_bank`'s two conversions; 15 was already the base's, 02c32122's zero extension of the sample bank, which went), `symbols.own` 397 to 400; down `src.n64p` 6 to 1, `src.n64p_field` 14 to 1, `src.n64w` 23 to 6, `src.def.n64p.generic` 2 to 1, `src.def.translate` 10 to 9, `src.lit.annotation` 3 to 2, `src.lit.comment` 11 to 10, `src.lit.code.k1` and `.region` 2 to 0, `symbols.aliases.cartridge` 1 to 0, `symbols.by_name.game_objects` 6 to 5, `symbols.names_reached.game_objects` 3 to 2, `port.host_test` 21 to 19 (`.tests` 3 to 2), `port.mapping.tests` 16 to 12, `port.rdram_base.tests` 36 to 33, `port.translate` 24 to 23 (`.tests` 48 to 43), `port.trunc.tests` 9 to 7, `port.zext` 18 to 17 (`.tests` 30 to 20); the compiler's classes (`cc.*`) left as they were, for CI's GCC 13 |
| the loader's summary line (the synthetic cartridge) | 4 more symbols with storage of their own (the registries), 1 fewer (`musyx_samples`, a u32 now): `--data-moved` |

**The census** (`window_census.py`, Debug, with the compiler pass; 6543cff7's build before): `conv.PC_N64P.game` 34
to 28, `conv.PC_N64A.game` 18 to 12, `conv.PC_N64W.game` 22 to 6, `conv.file_macros.defs` and `uses` 16 and 38 to 13
and 32, `conv.pc_n64_ptr.port` 19 to 18, `conv.cc.ptr_to_int` 29 to 10, `conv.cc.int_to_ptr` 8 to 0,
`conv.cc.implicit` 28 to 25, `fields.link_enum` 15 to 1 and `link_enum_types` 3 to 1, `lit.region.game` 2 to 0,
`lit.k1.game` 1 to 0, `lit.reloc.game` 7 to 6 (`pc_rdram_k0_high` in the sample bank's test), `port.below_4g` 12 to
11, `port.n64_reads` 13 to 3, `tests.window` 65 to 59, `be.ranges` 39 to 38, `text.window.game` 116 to 98,
`context.cartridge_aliases` 1 to 0; up: `conv.silent.trunc` 2 to 3 and `zext` 14 to 15 (`speech_set_bank`'s
conversions of the cartridge addresses the speech code carries in pointers), `context.own` 397 to 400 (the four
registries, less `musyx_samples`).

**What this changes for later steps.**

- **Nothing in the sound reads the window any more but through the heap's own pointers**: the interpreter, the
  library and the sound manager hold pointers (into the heap, which is still the window until step 12) and the
  samples' 32-bit cartridge addresses. Step 12 needs no change in the sound for the heap to leave the window; the
  state hash meets the voices' heap pointers then (`pc_mx_voice`'s head: macro pointers into the pool, each 8-byte
  aligned in a 0x1C0-byte voice; the node pointers already point into `pc_mx_nodes`, which it names).
- **The sound's coverage**: the gate's default cartridge plays no sound. A comparison on the `--sounds` cartridge
  (`synth_compare.py --rom build/pc/synth/rush2049_synth_snd.z64 --same --audio`, base against step) covers the
  library's voices, registries, sequencer and samples byte for byte; a step that touches the sound should run it as
  well as the gate.
- **Left in the sound** (none an N64 address of RAM): the hardware layer's D-cache bookkeeping, which counts the
  N64's record sizes from the low 32 bits of the records' addresses (`dcache_create_dirty_lines` is arithmetic on
  the PC: nothing reads memory by them) and the microcode sizes' pointer differences; the speech module's cartridge
  addresses carried in pointers (`dma_queue.c`, `streams.c`; the game never starts speech); the task capture's
  `pc_n64_addr`, which asks whether a region is in RDRAM to copy its neighbours as the RSP reads them (step 12).
- `include/game/types.h`'s `tPadState.listener`, and every view of `gPlayers` at +0x44, hold a listener's number;
  `port/assets/tests/test_input.c` and `test_casts.c` define `gListeners` for it.


## 15. Step 10: display lists native (done 2026-11-11)

Wave 3.4's step 10: the files' display lists, which the N64 relocated in place and the renderer read in the N64's form,
are built again by the WAD converter when the file loads, in the PC's own form with host pointers, and the vertices
they load beside them in the host's byte order; the game's walkers and the renderer read only those. Three commits:
the lists and their readers; the walkers' care for a list the converter couldn't build, the synthetic cartridge's
`--lists` and the tools' stale rules; the documents and the audit's baseline.

| What | Before | After |
|---|---|---|
| a WAD's display lists (`port/assets/wad.c`) | in the file, N64 form (8-byte big-endian commands), each address an offset the game made a 32-bit address in place: `relocate_model_dl` (each model's levels, by the file's N64 address) and `relocate_texture_loads` (TXLD, by IMAG's), `(w1 + base) & 0xFFFFFF \| segment` (`pc_rcp_reloc`, the K0 address past 16 MiB) | built beside the file when it loads, command for command in the PC's form (`PcGfxWords`, the game's `Gfx`): w0 the file's, w1 a host pointer in the commands that carry an address (VTX, G_DL, G_RDPHALF_1 before G_BRANCH_Z, MTX, MOVEMEM, LOAD_UCODE, SETxIMG; a LOD marker's section end) and the value in the rest ([ASSETS.md](ASSETS.md) 3.2). Every command a walk of the file reaches is built, in the file's order; one G_ENDDL after the last. The N64's relocation is played on the file's offsets to know each address's origin (the file's or IMAG's): w1 points at `(origin + w1) & 0xFFFFFF` in the file, where the renderer read the relocated word; a word the N64 made an address not exactly once pointed into no file, and is NULL, said (`display lists:`). The file keeps its bytes |
| the vertices | the file's, big-endian, which the renderer read by their range and `mirror_vertex_texcoords` changed in place through `pc_n64_ptr` | the runs the VTX commands load and `mirror_vertex_texcoords` walks, copied beside the file in the host's byte order, each run of the file in one piece with 16 zero bytes after it; registered as model data (`pc_mem_register_model_data`, `port/ultra/memory.c`) |
| the records' pointers | a level's `displayList` (`unsigned int *`) and a listed texture's `texBase`: the lists in the file | the converter's lists (`union Gfx *`, `texBase` a `Gfx *` for a listed texture) |
| `SetupWaveModel` (`waves.c`) | `relocate_texture_loads(…, PC_N64A(IMAG))`, `relocate_model_dl(dl, PC_N64A(file), -1, mirrored, keep)` | `relocate_model_dl(dl, mirrored, keep)`: the race's part only (a LOD marker whose bit isn't kept becomes `DE010000` to the list its w1 names; the first VTX's vertex pass); no N64 address |
| the game's walkers | big-endian accessors (`DLW`, `DLW_SET`, `DL_AT`, `pc_be16/32_get/set`), N64 address rules (`SEGMENTED`, `pc_rdram_k0_high`, `addr += 0x80000000`, `\| 0x80000000`), a callback stepping by words | `Gfx` and `Vtx` as they are: `display_list_traverse` (its callback steps by commands), `flip_triangle_winding`, `mirror_vertex_texcoords`, `displaylist_get_texture_image` (a pointer: NULL where the N64 returned garbage), `displaylist_set_texture_image`, `displaylist_set_tile_size` |
| the texture animation | a frame's image as the list's N64 word (`FRAME_IMAGE_IN_LIST`: `PC_N64A` of a texel frame's pointer); the TLUT colours written with `pc_be16_set` | images are pointers, moved as they are; the colours written with `pc_tlut_set` (the files' texel format: the prelude's `pc_tlut_get`/`pc_tlut_set` replace its big-endian accessors), as is the car paint's copy (`SetupCarColor.c`) |
| `relocate_texture_loads`, `port/ultra/rcpaddr.c` (`pc_rcp_reloc`, `pc_rdram_k0_high`), scene.h's `DLW`/`DLW_SET`/`DL_AT`, `rom_tu.h`'s `K0_TO_PHYS`/`K1_TO_PHYS`, the self-test's "RCP addresses" | | gone |
| the renderer (`gfx_rsp.c`) | two list forms, told apart by `pcgfx_is_be(list)` at the task's start and every list entered; vertices read big-endian in a big-endian range; the high frame rate's static vertices told by that range | one form: `GfxDlPtr` is a `const PcGfxWords *`; vertices in the host's byte order; the recorder asks `pc_mem_is_model_data` (only while it records). MTX and MOVEMEM data, texels and TLUTs are still read by their range. The counter's `gfx.rom_list` stays at 0 |
| `check.c`, `rushmods.py` | | a listed texture's load list must be 8-aligned (the converter builds lists by 8-byte commands) |

**The crossings** (`crossings.py --guard`, the 64 synthetic paths; 90979a43 before, the step after):

| Kind | Sites before | Crossings before | Sites after | Crossings after | What went |
|---|---:|---:|---:|---:|---|
| `PC_N64A` | 1 | 8,655 | 0 | 0 | `waves.c:93`, the file's N64 address for the lists' relocation, once a wave |
| `pc_n64_ptr` | 4 | 13,503,358 | 2 | 978,693 | the renderer's `rdram` (6,760,217: the ROM lists' relocated physical VTX addresses), `pcgfx_resolve`'s share of the ROM lists (5,747,760: the game's G_DL to a list in the heap, `dl_at`, and the VTX case's K0 addresses past 16 MiB), `mirror_vertex_texcoords` (16,688) |
| `gfx.rom_list` | 1 | 7,103,807 | 0 | 0 | all (the VTX case: physical 6,760,217, heap 343,590) |
| `gfx.native_list` | 5 | 6,382,735 | 4 | 978,565 | `dl_at` (5,404,170: the game's `gSPDisplayList` of a level's or a texture's list in the file) |
| the other ten kinds | 9 | 21,130 | 9 | 21,130 | none |
| total | 20 | 27,019,685 | 15 | 1,978,388 | 5 sites, 25,041,297 crossings |

What the renderer still takes for an N64 address is the heap's blocks the game names by pointer while the heap is the
window: a file's texels in its texture loads (`pcgfx_rdp_command`'s SETTIMG, 565,209), the depth and colour images
(SETZIMG 103,371, SETCIMG 206,742) and the frame `pc_gfx_present` shows (103,243): step 12's. The guard
(`RUSH2049_WINDOW_GUARD=arenas,count`) found 0 faults at 0 instructions, before and after. `--check --no-faults
--zero-kind gfx.rom_list,PC_N64A` passes, and `--zero-site` for `relocate_model_dl.c`, `display_list_traverse.c`,
`flip_triangle_winding.c`, `mirror_vertex_texcoords.c`, `MirrorWaveInstance.c`, the `displaylist_*` helpers,
`TextureAnimation.c`, `InitTextureAnimation.c`, `waves.c`, `SetupCarColor.c`, `port/assets/wad.c`, the renderer's
`rdram` and `dl_at`; the control, `pcgfx_resolve`, fails.

**The census** (`window_census.py`, Debug, with the compiler pass; 90979a43's build before): the step's three targets
at 0, `be.accessors.game` 7 to 0 (scene.h's `DLW`/`DLW_SET`, `mirror_vertex_texcoords`, the texture animation's and
the car paint's TLUT accessors, now `pc_tlut_get`/`pc_tlut_set`), `lit.mask.game` 6 to 0 (`display_list_traverse`,
`relocate_model_dl`, `rom_tu.h`'s unused `K0_TO_PHYS`/`K1_TO_PHYS`), `lit.reloc.game` 6 to 0 (`pc_rcp_reloc`,
`pc_rdram_k0_high`); and `conv.PC_N64A.game` 2 to 0, `conv.pc_n64_ptr.game` 2 to 0 (`DL_AT`, `mirror_vertex_texcoords`),
`conv.file_macros.defs` and `uses` 4 and 16 to 2 and 15, `conv.silent.trunc` 4 to 3 and `zext` 16 to 15 (the texture
animation's frame images), `api.u32_address` 4 to 1 (left: a sound helper's, `sound_hw_fetch_sample_range`, a
cartridge address), `lit.address.game` 4 to 1 (the walkers' K0 base; left: `rom_tu.h`'s `K0BASE`),
`n64map.uses.port` 116 to 105, `port.rdram_base` 46 to 44,
`tests.window` 57 to 49 (`assets_wad_game` needs no window), `be.ranges` 38 to 35, `text.window.game` 65 to 57.

**The proof** (Rocky, x86-64, GCC 14; the base is 90979a43's builds in `~/rush2049pc/w3d-ref`):

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w3d-ref --skip realtime --data-moved` (Debug, the step's first commit) | PASS: ctest 575 of 575 (`assets_rom` skipped: no ROM on Rocky), synth_compare `--same --shots --audio` 64 of 64 `same` |
| the final tree against the base (Debug): `drive`, `race2p`, `circuit`, `detail_high`, `memhigh`, `mods_texture`, `demo` | `same` |
| RelWithDebInfo against the base's RelWithDebInfo (`synth_compare.py --same --shots --audio --data-moved -j6`) | 64 of 64 `same` (a first run beside the moved build's had `mods_off` vary, the base's own two runs apart in the exit's audio, PCONLY.md 2's known variation, and one more path vary; run again, 64 of 64) |
| `ctest --test-dir build/opt -LE realtime` | 575 of 575 (`assets_rom` skipped) |
| the high frame rate's realtime tests (`hfr_*`, alone under `flock ~/rush2049pc/.realtime.lock`) | Debug 7 of 7; RelWithDebInfo 7 of 7 |
| the `--lists` cartridge (`synth_rom.py --lists`: every model's list calls a TXLD state list with an IMAG-relative G_SETTIMG, goes through a G_BRANCH_Z and holds LOD markers), base against step, Debug, `--same --shots --audio --data-moved` | 64 of 64 `same`; the default cartridge `synth_rom.py` makes is the base's file, byte for byte |
| `rush2049pc_assetcheck` on the synthetic cartridges (default and `--lists`): the native lists against `wad_lists_check.h`'s replay of the N64's relocation, every WAD | no list unlike the N64's, no `display lists:` message (82 WADs and 19 worlds each; the only failures the waves 69-75 no code loads, which the synthetic cartridge leaves empty) |
| `assets_mods`: every mutated WAD `check.c` passes (2,017 of 3,000 car WADs, 1,985 track WADs, 1,388 worlds) through the converter and the replay | 0 unlike the N64's, 0 converters refusing or cutting |
| the moved build (`-DRUSH_NOFIXED=ON`, Debug): CI's subset (`ultra_selftest.*`, `pc_data_rom`, `pc_data_synthetic`, `synth_.*`), and against the fixed build (`synth_compare.py --runs 1 --same`) | builds; 94 of 94; 64 of 64 `same` |
| the counting build (`crossings.py --guard --check --no-faults --zero-kind gfx.rom_list,PC_N64A` and the `--zero-site` checks above) | pass |
| `offset_check.py --strict` (Debug) | no open finding (3 reviewed, as before) |
| `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | none open; none new |
| `census.py` | 1,065 of 1,065 compile (`relocate_texture_loads.c` and `rcpaddr.c` gone) |
| `nofixed_audit.py --baseline` | no class grew; the baseline lowered for the classes this step moved: `port.rdram_base` 48 to 46, `.tests` 31 to 24, `port.translate.tests` 43 to 21, `port.zext.tests` 19 to 16, `port.mapping.tests` 12 to 11 (the WAD tests' windows and `rcpaddr.c`), `src.lit.annotation` 2 to 1, `src.lit.code.address` 4 to 1, `src.trunc` 4 to 3, `src.zext` 16 to 15 (the walkers' K0 rules, the texture animation's frame images); the compiler's classes (`cc.*`) left as they were, for CI's GCC 13 |
| `assets_wad`, `assets_wad_game`, `assets_mods`, `mods_tool`, `gfx.bedata`, `gfx.window.*`, `gfx.interp.*`, `hash_aslr` | pass |

**On the retail cartridge** (the lead's, on the Pi): first `rush2049pc_assetcheck` (`assets_rom`), which runs every
WAD's native lists against the replay of the N64's relocation and fails on any `display lists:` message; then the
screenshots (`--shots`), base against step, where the lists the synthetic cartridge lacks are drawn: every track
forward and backward (the LOD markers' sections), one and two players (the player-count sections), mirrored (the
winding flip and the mirrored texture coordinates, which no synthetic path reaches), the tracks with texture
animations (traffic lights, lamp chasers, scrolls and flipbooks: the load lists' SETTIMG and SETTILESIZE patched each
frame), track 5 at night (the Expansion Pak's), the track select's map (its own lists in a model's first level), the
attract demo, battle and stunt arenas, the cars' models, and a mod's texture pack (`mods_texture`), with
`video.frame_rate` 120 for the high frame rate's pairing of the models' transforms.

**What this changes for the later steps.**

- **Step 12 meets no display list.** The lists and vertices the game draws hold host pointers wherever their file lies;
  what the renderer still takes for an N64 address is the heap's own blocks, named by pointer (a file's texels and
  TLUTs in the game's texture loads, the depth and colour images, the frame shown): they become host pointers when
  the heap leaves the window, with no change to any list. The renderer's physical rule (`rdram`, `pcgfx_resolve`'s
  past-16-MiB case) and `pcgfx_segment_value` then serve nothing the game makes: step 12 can drop them with the 4 GiB
  rule. `pcgfx_is_be` is left for data only (a file's matrices, texels and TLUTs).
- **The texels and TLUTs** are the last of a file's bytes the renderer reads in the N64's order (by
  `pc_mem_register_be`'s range); the game reads and writes the colours in that format through `pc_tlut_get` and
  `pc_tlut_set` only. Step 14 can narrow the big-endian ranges to them: no list or vertex is in one any more.
- **The native lists and vertices are in no hashed region**, as the WAD's records (section 13): the state hash never
  saw the lists' relocated words as anything but heap bytes, and a texture animation's pointer into a list (the
  scrolls' `PaletteLists` rows) lies in the loader's own copy of those tables, which the hash doesn't walk either.
  `hash_aslr` passes.
- **A file whose lists the N64 relocated badly** (a level's list reached twice, a G_DL in a texture's load list, a
  marker's section end outside the file) gets NULL where the N64's word pointed nowhere, and the converter says so;
  `relocate_model_dl`, `display_list_traverse` and the renderer skip a NULL. The retail cartridge's lists must have
  none (`rush2049pc_assetcheck`: the lead's run on the Pi), and `assets_mods` shows that every mod file `check.c`
  passes converts to exactly what the N64's relocation makes of it.
- **The names**: `relocate_model_dl` relocates nothing any more (it keeps the race's sections and prepares the
  vertices for a mirrored track); a rename (`prepare_model_dl`, through `tools/port/names`) is left to the lead.
