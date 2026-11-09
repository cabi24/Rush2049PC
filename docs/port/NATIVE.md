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

Four things live in the window today:

| | What | Who keeps 32-bit addresses of it |
|---|---|---|
| the arenas | the game's pointer-free data and BSS, the runtime images' too, at their N64 addresses | the car record's u32 fields, the level-object nodes and lists (section 2; the sky matrix slots, two palettes and `pc_matrix_spill`'s users were step 4's, done) |
| the heap | everything allocated: the loaded files, frame and depth buffers, the audio's memory | the heap's own headers and API; the files' relocated fields and display lists; the road's tree nodes in the car record; the targets' records; MusyX's registries, macro and song pointers; ghosts, palettes, listeners; `osVirtualToPhysical` |
| the image block | the heap block `load_overlay` reserves at 0x8038A400, which holds the runtime images' arenas | the overlay loader's `RUNTIME_IMAGE_*`, `overlay_base` |
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
runs on the Pi, with `--audio` captures compared byte for byte).

### 4.6 The runtime images

They are PC code and data already: both images are linked in and a load resets one image's data
([DATA.md](DATA.md) 5). What ties them to the window is their arenas (step 5 makes those blocks) and the heap block
the loader reserves (step 6 makes it an offset). `RUNTIME_IMAGE_START`, `overlay_base` and `N64_OVERLAY_*` go.

### 4.7 Low memory

`osTvType`, `osMemSize` and the other boot parameters become ordinary variables port/ultra defines and sets
(`pc_os_initialize`); `game_bss_end` and `overlay_base` go with the heap (step 6).

### 4.8 New checks

| Check | What | Built in |
|---|---|---|
| `window_census.py` | the counts by kind; `--baseline` in CI holds the line, each step lowers it | this commit |
| **A crossing counter** (`RUSH_COUNT_CROSSINGS`, a build option) | the probe of section 2 made a tool: every `PC_N64P/A/W/SLOT` crossing and the renderer's and audio's N64 addresses counted by site and target at exit; a step's proof includes "these sites cross 0 times along every path". The scratch probe (`w3probe.c`, `apply_probe.py` in the scratch tree) is the design: a weak hook from the prelude's macros, a table by site, a dump at exit, `addr2line` for the PC layer's callers | step 1: built (section 9) |
| **The arena guard** (`RUSH2049_WINDOW_GUARD=arenas`) | after step 5 the window's arena ranges hold nothing: mapped with no access, any use faults with a note naming the datum and the instruction; after step 12 the whole range 0x80000000-0xBFFFFFFF is reserved with no access in this mode, so a stray N64 address faults wherever it comes from | step 1 (arenas): built in the counting build (section 9); step 12 (all) |
| AddressSanitizer on x86-64 | a CI job (ASan and UBSan, every test, the synthetic paths); its redzones are step 13's proof | step 12 |
| The gate | objsame, synth_compare 63 of 63 `same` (shots, audio), ctest and the realtime tests, the retail scripts on the Pi | every step |

The moved build (`RUSH_NOFIXED`) and its CI job stay as they are during wave 3: every crossing still translates
there, so a step that removes crossings must keep it building and its paths `same`. Step 12 retires it with the
window it moves.

## 5. The steps

Each step is one agent's, one to three sessions, with files no other step of its wave edits. Sizes: small (under a
session, a few files), medium (a session, 10-30 files), large (two or three sessions, 30-60 files). Every step's
proof is the gate (objsame where code is unchanged, else synth_compare 63 of 63 `same` with shots and audio, ctest
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

### Step 3. The level objects (medium-large)

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

### Step 5. The arenas out of the window, as blocks (medium)

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

### Step 6. The heap's own code native (large)

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

### Step 7. The WAD's records native (large)

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

### Step 8. The other formats (medium)

4.4 for the text bank (`AdjustTextWavePointers` and its 17 readers, `ReplaceWave`, `port/assets/text.c`), the world
(`NewWorld.c`'s name table, `loadtrack.c`'s `woff`, `port/assets/world.c` but its GTLD part), the paths
(`ReadPathData.c`, `port/assets/path.c`). The road needs nothing: its file links by index (`port/assets/road.c`), and
the car record's pointers into its tree are step 2's. Proof: the gate (every path loads a track and text);
`assets_text`, `assets_world`, `assets_path`. Depends on: 3 (the GTLD part of world.c). Can run with 5, 6 (files
apart) or 7 and 9. Risk: the text bank has four views in 17 files (ASSETS.md 4 #15), and `ReplaceWave` swaps the
bank under them when the language changes.

### Step 9. MusyX native (large)

4.5. Files: `src/rom/sound/*.c` (`registry.c`, `macros.c`, `voices.c`, `sequencer.c`, `synth.c`, `hardware.c`,
`dma_queue.c`, `groups.c`), `src/rom/speech/streams.c`, `AudioInitSys.c`, `musyx_cart_address.c`,
`port/assets/musyx.c`, `port/audio/musyx.c` (the N64-form record path and `n64ptr` go; `resolve` takes pointers),
the audio manager's listener records (`UpdateListeners.c`, `StartListeners.c`, `AudRemoveListener.c`). Proof: the
gate with `--audio` byte for byte; the retail cartridge's music (the synthetic cartridge has no songs: the lead's
audio captures on the Pi, base against step); `assets_musyx`, the audio tests. Depends on: 6 (the audio heap's
allocations). Can run with 7 and 8. Risk: the largest library with the least synthetic coverage.

### Step 10. Display lists native (large)

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
cartridge if it has none, runs the 63 synthetic paths (synth_compare.py's runs: on the virtual clock, its options,
`pcsetup` left out), then each again in the guard's counting mode, and prints the report: one line per kind (always
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
| `gfx.rom_list`, `gfx.native_list` | a value below 4 GiB the renderer takes for an N64 address (`pcgfx_resolve`, `pcgfx_segment_value`), by the form of the list the command came from: a ROM list (N64 form, big-endian) or a native one. Host pointers aren't counted | the caller (`dl_at`, `movemem`, the VTX case, `pcgfx_rdp_command`'s image commands, `pc_gfx_present`'s frame) |
| `audio.n64ptr` | every N64 address the audio interpreter reads (`n64ptr`): an N64-form record's decoder, or `resolve` for a native field (null pointers aren't counted) | the caller |
| `audio.field` | `resolve`'s share of those: a native record's pointer field holding an N64 address | the record's reader |

Each count is also split by where the address points (a host pointer for the kinds that go from a pointer to an
address): `game_arena`, `boot_arena`, `ovl_a_arena`, `ovl_b_arena` (the data loader's arenas), `image_block` (the heap
block reserved for the runtime images, outside their arenas), `heap`, `low_memory` (the boot parameters), `window` (the
window's other bytes: the N64's code ranges, where nothing lives on the PC), `kseg1`, `cart` (the cartridge through
KSEG1), `phys`, `segmented`, `other`, `null`, `host` (a pointer above 4 GiB). For RDRAM outside the heap the report
names the data (the loader's name for the first address in each region; offsets left out).

Not counted: the loader's and the asset converters' own decoding of the ROM's addresses (they are where N64
addresses belong); `pc_rcp_reloc` and `pc_rdram_k0_high` (`rcpaddr.c`, the display lists' relocation: step 10's,
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

Today the arenas are at their N64 addresses, so **every access by name faults too**: on this layout the guard proves
it sees what it should, and its sites are the game's by-name accesses, which step 5 moves with no change to the code.
It can't tell those from a crossing into an arena (both are the same address): steps 2-4's targets are the counter's
arena regions (9.4). After step 5 the two are one: the guard, counting or stopping, must find nothing, and anything it
finds is a crossing steps 2-4 left.

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
