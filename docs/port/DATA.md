# Game data on the PC (port/data)

The game C declares about 2,200 data symbols at fixed N64 addresses that nothing defines: the initialized data
of the boot segment, the game image and the two runtime images, and their BSS. The PC build gives each one
storage with a correct host layout, and fills the initialized ones from the player's ROM at startup. Data without
pointers sits at its N64 address in the RDRAM window, so a 32-bit N64 address of it is a host address; data with
pointers has storage of its own in the executable. Neither the executable nor the repository holds ROM bytes: the
build knows only addresses, sizes and layouts, and every value is read from the ROM at run time. An overlay "load"
is the same fill run again for one image.

| Piece | Where |
|---|---|
| Generator: types, layouts, storage, descriptors | `tools/port/gen_data.py` (replaces `gen_bss.py`), helpers in `tools/port/n64syms.py` |
| Generated storage and tables (git-ignored) | `build/pc/pc-data/data.c` |
| Generator reports | `build/pc/pc-data/data_layout.txt` (decisions, conflicts, what is left), `data_symbols.txt` (every symbol) |
| Loader | `port/data/pc_data.c`, descriptor types in `port/data/pc_data.h` |
| Size overrides for open-ended arrays | `port/data/extents.txt` |
| Tests | `port/data/test_pc_data.c` (synthetic tables), `port/data/datacheck.c` (real tables: `--synthetic`, or your ROM) |
| Overlay loads | `src/blob/groups/{heap_release,score_insert}/overlay_loader.c`, `{carcolor,state}/ctx_overlay_loader.c` |

## How it works

### 1. Types from DWARF

The game objects are compiled with `-g`, so each object's DWARF holds its own declaration of every external
variable it uses. `gen_data.py` reads them with `readelf --debug-dump=info` (25 MB of text for the 1,346 objects;
the whole generator runs in about 3 s on the Pi 5). Typedefs and qualifiers are dropped; a struct one file leaves
incomplete is completed from a file that defines it.

Files often disagree. The generator ranks each distinct declaration of a symbol: with a known size (below), one of
exactly that size first, then a larger one, then a smaller; then struct with pointers, struct, pointer, words,
bytes, unknown; then the larger N64 size, then the number of files that agree. The winner sets the layout.
Disagreements that change where pointers are, or element widths, are listed in `data_layout.txt` (below).

Sizes: the symbol file's `size:` comment, else `port/data/extents.txt`, else the game C's own (tentative)
definition of the symbol, which is also the size the linker compares `data.c`'s with. A known size bounds every
declaration: an open-ended array (`T x[]`) takes it, and a longer array is cut to it (listed). Without one, an
open-ended array runs to the next known address (or as far as the largest complete declaration if that is
longer), and complete declarations keep their own size.

A declaration that runs over the next object makes that object part of its storage (section 3), so a view that
is too long is worse than one that is too short. That is what the bound fixes:

| Symbol | Was | Is | Evidence |
|---|---|---|---|
| `player_array` (`D_80152818`) | `GameCar[8]`, 17 files | 6 cars, 0x1650 B | `D_80153E68` (an `OSMesgQueue`) at +0x1650; the symbol file says 6 entries; `Car D_80152818[6]` in `path_D169C`. `gAltPaths` (`D_80153E80`), `gPathPtList` (`D_80153EF8`), `gPathWave` (`D_80153F20`) and the rest were inside its storage, the pointer-sized ones overlapping their neighbours |
| `gResPath` (`D_801407F0`) | 28-byte `Header` | 16 B | `{numPts, pts, numAlt, alt}`; `D_80140800` (`GhostHeader *`) at +0x10. The 28-byte views were written to the PC layout, and `D_801407F4` (`pts`) sat at host +4 instead of +8 |
| the ghost headers (`D_80142530`) | 1 of 0x58 (the gap) | 4 | `GhostStart`: `D_80142500[i] = &D_80142530[i]`, i < 4 |
| `D_80144C50` | ap.c's 20-byte `Node` | 16 B | the list head of 6 files' `List`/`tDLList`; `D_80144C60` (`PakHandle[]`, 64 pointers) follows |
| `D_80110680` | 2-byte `Flags` | 6 B (`u8[6]`) | per car, cleared for 6 by `render_post_process`, which defines it |
| `D_80146180` | `s8[]`, 8 B by the gap | `s8[4]` | `func_800DE45C`'s definition |

### 2. Two layouts per type

Each canonical type is laid out twice. N64 (IDO, MIPS o32): `char` 1, `short` 2, `int`, `long`, `float`, pointers
and enums 4, `long long` and `double` 8 aligned 8, structs aligned to their widest member and padded to it. Host:
the DWARF offsets and sizes. The walk produces a *field program*: runs of (kind, count, N64 offset, host offset),
with a repeat count and two strides for arrays of structs. Kinds are u8, u16, u32, f32, u64, f64, pointer (4 to 8
bytes) and the N64's 32-bit `long` (4 to 8 bytes, signed or not). There are no bitfields or `long double`s in the
game's data declarations.

Check of the N64 rules: for every struct without pointers or `long`, the computed N64 offsets must equal the
host's, since the two ABIs agree there. All of them do (0 listed under "Layouts that differ without pointers").
The libultra structures also come out at their known sizes (`OSThread` 432, `OSMesgQueue` 24, `OSTask` 64,
`OSIoMesg` 24, `OSDevMgr` 28).

### 3. Storage

Each image's data and BSS is an **arena** laid out as on the N64, at its N64 addresses in the RDRAM window that
port/ultra maps at host 0x80000000 ([ULTRA.md](ULTRA.md) §2):

| Arena | N64 range | Image |
|---|---|---|
| boot | 0x800277D0–0x80086A50 (data to 0x8002E8E0) | boot segment tail, a plain copy of ROM 0x283D0 |
| game | 0x8010FD7C–0x8017A640 (data to 0x801249F0) | game image, DEFLATE at ROM 0xB0CB10 |
| ovl_a | 0x803AF978–0x803BAE20 (data to 0x803B9A50) | runtime image A, DEFLATE at ROM 0xB5C534 |
| ovl_b | 0x80393B34–0x8039B440 (data to 0x80394F70) | runtime image B, DEFLATE at ROM 0xB6FEC4 |

A symbol with no pointers whose two layouts agree is an absolute symbol at its N64 address (an assembler `.set`
in `data.c`). Code that indexes from one symbol into its neighbor still works, 8-byte data keeps the N64
alignment, and a 32-bit N64 address of it is its host address: N64-layout records, u32 fields and the converted
files ([ASSETS.md](ASSETS.md)) can hold it, and the renderer and the audio interpreter, which read every value below
4 GB as an N64 address, reach it through the window. A symbol whose type has pointers, a `long`, or a host layout
that differs gets its **own storage** in the executable (host size, aligned 16, or 32 from 32 bytes as x86-64
aligns such a common), and every symbol whose address lies inside its N64 extent is defined at the matching host
offset (`input_rec1 = D_8014A118 + 0x50`). Names at one address are aliases. Tentative definitions (common
symbols) in game and runtime-image C get the same treatment; the linker prefers the strong definition. The two
names one past an overlay's BSS (`D_803BAE20`, `D_8039B440`, the loaders' `bssEnd`) are absolute one past its
arena. The window bytes under own storage hold that data's N64 form and are not used.

**Why the window can hold them.**
- Those ranges are free on the PC. No code is linked there (the game's code is host code), the heap starts at
  `align32(0x8017A640)` after the game's BSS, and the runtime images' data ranges are disjoint from each other and
  inside the heap block `PrevMaxPath` reserves for the images (0x8038A400–0x803BB380).
- Absolute symbols in a PIE. Every code reference to them is a GOT load: aarch64 PIE code makes one for every
  extern and common, and on x86-64 everything is compiled `-fPIC` without GOT relaxation (CMakeLists.txt). In the
  game and PC-layer objects that is 18,896 `ADR_GOT_PAGE`/`LD64_GOT_LO12_NC` pairs and no PC-relative reference.
  Of the 11 `ABS64` relocations against them, 10 are `.debug_info` and one an initialized pointer (overlay A's
  `del = D_803B8758`), which the linker resolves to the address itself. binutils 2.40 on aarch64 gives the GOT
  slots RELATIVE relocations, and `pc_mem_fix_absolute` puts back 1,802 of them first thing in `main`
  ([ULTRA.md](ULTRA.md) §2). Its rule (an
  addend at 0x80000000 or above is an absolute symbol) needs nothing else linked that high, which
  `gen_data.py --check` verifies.
- The order. `main` fixes the GOT slots, maps the window, loads the ROM and then calls `pc_data_init`. Nothing
  touches game data before that (there are no constructors). `pc_data_load_all` refuses an arena at its N64
  address that isn't mapped, so the translated-window fallback of a host without the fixed mapping can't run the
  game's data.
- The runtime images' arenas are in the heap. The first load fills them, for the report and the startup snapshot
  of game-C data, and then clears them, so the heap starts out zero as before. The game's own load of an image
  (`PrevMaxPath`, after it has reserved the block) fills that image's arena again. As on the N64, an image's data
  is valid only while the image is loaded; its own-storage symbols keep their values in the executable.

Result: **2,258 names at 2,221 addresses**. 1,775 addresses are in arenas (1,007 data, 768 BSS) and 384 have their
own storage (141 data, 243 BSS). 60 more lie inside own storage, and 2 are end markers. 1,149 addresses are
filled from the ROM and 1,070 are zeroed.

The image of a name decides its address. The runtime images' code overlaps (both at 0x8038A400) and image B's data
lies inside image A's code range, but the two images' data ranges are disjoint and their D_ names never collide.
Addresses come from the referencing image's symbol table (`asm/us/<image>/symbols.json`, what the matching build
links against), then `symbol_addrs.us.txt` and `undefined_syms_auto.us.txt`, then the D_XXXXXXXX name. The
symbol file is stale for a few game names: `gstate` is 0x801174B4 there but 0x801146EC in the game's table,
`audio_channel_alloc` is 0x800B0580 there but 0x800BA00C, and four functions are off by 8 bytes. The image tables
and `layout/game.json` agree with each other.

### 4. The loader

`pc_data_init(rom, size)` (`pc_port.h`, defined in `data.c`) calls `pc_data_load_all` with the game's tables:

1. Each image's bytes: the boot tail is a slice of the ROM; the other three inflate with zlib (raw DEFLATE). The
   inflated size must equal the image size. The runtime images stay cached for reloads; the game image is freed.
2. Each arena, in place in the window: copy the initialized part, zero the BSS, byte-swap every described element.
   Pointers stay N64 addresses there (in host byte order).
3. Each symbol with its own storage: convert from the image element by element through its runs.
4. Game-C data in a runtime image: its bytes are saved at startup and put back on every load (zeroed if BSS).
5. After the first load, the runtime images' arenas are cleared (section 3).

Pointer values are turned into host pointers in this order:

| N64 value | Becomes | Count kind |
|---|---|---|
| 0 | NULL | null |
| not an RDRAM address (ROM offsets such as `D_8002B020` = 0xB5C534, segment addresses) | the value, unchanged | raw |
| ≥ 0x8017A640 (end of the game's BSS) and outside both runtime images | **the value, unchanged** | heap (kept) |
| a function entry: the data's own image first (A and B share code addresses), any image from game or boot data | the host function | functions |
| a function with no C yet | NULL, logged once | no C |
| inside own storage or game-C data | its storage plus the offset mapped through its runs | own, C data |
| inside an arena | arena + offset: the value itself, since the arena is at its N64 address | arena |
| anything else | the value, logged once | unresolved |

The heap rule follows `docs/port/ASSETS.md`: the game heap is the RDRAM window at host 0x80000000
(`PC_RDRAM_BASE`), so heap addresses are already valid host addresses, and the renderer reads values below 4 GB
as N64 addresses. The runtime images are the exception inside the heap: on the PC their code is linked in, so
nothing is inflated into the window at 0x8038A400. Addresses in their code resolve to host functions, addresses in
their data or BSS to their own storage or their arenas (there, the window); the rest of the reserved block
(0x803BAE20–0x803BB380) is kept. Own storage must sit above 4 GB, so the executable must be PIE: `pc_data_init`
fails and says so otherwise. The arenas are below 4 GB on purpose: in the window an N64 address and a host address
are the same. (The loader takes an arena's host address from the tables, so the unit test can also keep its arenas
in host memory.)
Function pointers come from a table of 2,134 functions by address (`layout/*.json`, `manifest/static.json`, the
symbol file's boot functions), held through weak references. 1,976 have C. A function with no C is NULL there,
not a stub, unless some other file calls it (then the link test's stub is what it gets).

The first load writes a report, to `$PC_DATA_REPORT` or else `build/pc/data_report.txt` (`PC_DATA_REPORT=""`
turns it off):

- `unresolved <symbol>+<offset> <value>`: a pointer below the heap that is in no symbol;
- `no-code <symbol>+<offset> <value> <function>`: a pointer to a function with no C yet;
- `addr-word <symbol>+<offset> <value> -> <target> [<class>]`: a word in a non-pointer (u32) field whose value
  looks like an N64 address (0x80000000–0x80800000), and what it points at. `[arena]`, `[heap]` and
  `[low memory]` are window data, which the word reaches as it is: with the arenas in the window most such words
  are now right. `[own]`, `[C data]`, `[function]` and `[code]` it can't reach. Those are likely mistyped
  declarations, agent D's work list, and the summary counts them apart ("to host storage or code").

### 5. Overlays

Both runtime images are linked in. An overlay load is `pc_data_overlay_load(PC_OVERLAY_A | PC_OVERLAY_B)`: it
refills that image's arena (in the heap block the game's loader has just reserved) and own storage from its cached
inflated image, zeroes its BSS, and restores its game-C data. The other image is untouched.

- **"Start" is image A** (ROM 0xB5C534 through `D_8002B020`; BSS 0x803B9A50–0x803BAE20; loaded flag
  `D_8011ED00`; loaded by `sync_maxpath_to_checkpoint`, which `display_enable` = `LoadFrontEndOverlay` calls).
- **"Extra" is image B** (ROM 0xB6FEC4 through `D_8002B024`; BSS 0x80394F70–0x8039B440; flag `D_8011ED04`; loaded
  by `InitMaxPath`).

`PrevMaxPath` (the loader, 0x800A11E4) keeps the game-heap reservation at 0x8038A400 and `PurgeableMemory`. Under
`TARGET_PC` the inflate and the BSS clear become `pc_data_overlay_load(loaded == &D_8011ED04)`, and the two
wrappers pass 0 for the ROM addresses, which the PC doesn't use. The unload path (`display_enable` in
`groups/state/state.c` and its context copies) only frees the heap block and clears the flag, so it needs no
change. The four copies of `overlay_loader.c` carry the same edit. Preprocessed without `TARGET_PC`, each gives
exactly the tokens of the original, so the N64 build is unchanged. That is checked with gcc's preprocessor here;
the matching build itself (IDO, Rocky host) has not been run.

Not reset yet: **22 file-local statics in runtime-image C** (`cursorSpeed.1`, `gShieldSizes`, `D_80399120`...,
listed in `data_layout.txt`). They're local symbols, so the tables can't name them. A fix belongs to the game
build: rename the runtime-image objects' `.data`/`.bss` sections (`objcopy --rename-section`) so the linker
gathers them between `__start_`/`__stop_` symbols, and snapshot that range like the globals.

## The names missing from the symbol file

All resolved from the image symbol tables. Data (each is placed like any other symbol):

| Name | N64 | Region | Storage | Chosen type |
|---|---|---|---|---|
| `D_8014A250_alias` | 0x8014A250 | BSS | arena (also D_8014A250) | `tModelData[]`, N64 layout ([CARS.md](CARS.md)) |
| `gCamera` | 0x80150B70 | BSS | arena (also D_80150B70) | `Camera[]` |
| `gControllerData` | 0x80156CF0 | BSS | arena (also D_80156CF0) | `Pad16[]` |
| `gCarTeam` | 0x8012E67C | BSS | arena (also D_8012E67C) | `s8[]` |
| `gCountDown` | 0x80152032 | BSS | arena | `s16` |
| `gDefStartTimes` | 0x8002E870 | boot data | arena | `f32[]` |
| `gFogColors` | 0x80114658 | data | arena (also D_80114658) | `RGB[]` |
| `gHighScoreState` | 0x80140B20 | BSS | arena | `s32` |
| `gNumModels` | 0x80140BDC | BSS | arena (also D_80140BDC) | `u8[]` |
| `gNumWindows` | 0x80151AD0 | BSS | arena (also D_80151AD0) | `s16` |
| `gPalDefPtrs` | 0x80138670 | BSS | own | `PalTable[]` |
| `gRomObjDefPtrs` | 0x801161F4 | data | own (also D_801161F4) | `ModelList[]` |
| `gSkipToGameState` | 0x801174BC | data | arena (also D_801174BC) | `u32` |
| `gTexDefPtrs` | 0x80151AE8 | BSS | own (also D_80151AE8) | `TextureList8[]` |
| `gTotalNames` | 0x80151690 | BSS | own | `Owner[]` |
| `gWaveInstance` | 0x80156D38 | BSS | own (also D_80156D38) | 64 × 0x14 records |
| `codePane` | 0x80146208 | BSS | own | `s8 *` |
| `namePanes` | 0x80149418 | BSS | own | `s8 *[]` |
| `countdown_object` | 0x8017A4E4 | BSS | inside D_8017A4E0 + 0x8 | `TblHdr *` |
| `countdown_state` | 0x8017A4E0 | BSS | own (also D_8017A4E0) | `ModelTables` |
| `frame_counter` | 0x80142AFC | BSS | arena | `volatile s32` |
| `game_state_flags` | 0x801146E8 | data | own | `void *` |
| `gameplay_mode` | 0x8014A110 | BSS | arena (also D_8014A110) | `s32` |
| `input_rec1` | 0x8014A164 | BSS | inside D_8014A118 + 0x50 | `InputRecord` |
| `msgq_ptr` | 0x801497C8 | BSS | own (also D_801497C8) | `Gfx *` |
| `pad_config` | 0x80140BF0 | BSS | own (also D_80140BF0) | `Sprite[]` |
| `player_array` | 0x80152818 | BSS | own (also D_80152818) | `GameCar[]`, 0x468 bytes a car ([CARS.md](CARS.md)) |
| `state_word_a` | 0x801174B4 | data | arena (also D_801174B4) | `u32` (gGameState) |
| `state_word_b` | 0x801174B8 | data | arena (also D_801174B8) | `s32` (gNewGameState) |
| `active_player_count` | 0x8014A108 | BSS | arena (also D_8014A108) | `s16` (gNumPlayers) |

Functions: labels of a defined function, so `data.c` defines each as a one-instruction branch thunk (aarch64 `b`,
x86-64 `jmp`):

| Name | N64 | Is |
|---|---|---|
| `audio_bus_mix` | 0x800B4818 | `SavePlayerOption` |
| `audio_dma_sync` | 0x80097470 | `AllocateMemoryHeap` |
| `audio_interrupt_handler` | 0x800BB140 | `LoadCars` |
| `object_byte9_set` | 0x800B41C0 | `HAL_SetFontFixedWidth` |
| `player_mode_set` | 0x800C84C0 | `SetControllerStickEdges` |
| `player_state_set` | 0x800C9158 | `SetControllerActive` |
| `sound_handles_array_clear` | 0x800DC720 | `RemovePane` |
| `visual_objects_update` | 0x800B55FC | `SaveFlush` |
| `init_state_begin` | 0x800C9BE0 | `SetOptions` |
| `func_80090F44`, `func_8009EA68`, `func_800A5A40`, `func_800CD104` | | `PitchUV`, `RollUV`, `ZOID_ResetWindows`, `CreatePlayer` |

Left, with no C yet at their address: `display_list_flush` (0x800FB2C8) and `particle_lifetime_set`
(0x800B8374 = `createBG`). The `rom_symbol_aliases.ld` names (`osEPiRaw*`, `__osPi*`, `__osTLBLookup`,
`__osPfsDataChecksum`) are left to the libultra layer.

## Verification

Without the ROM (all run by `ctest` in `build/pc`, and the first two by every build):

- **The build links.** Census: undefined symbols **1,515 → 166**. Data placeholders in the link test's
  `stubs.c`: 1,351 → 15. Function stubs: 159 → 146 (the thunks). No strong duplicates.
- **Generator self-checks** (`data_layout.txt`): every run ends inside both the N64 and the host size of its symbol
  (0 failures; the generator fails the build otherwise); N64 sizes against the known size (1 difference:
  `D_8002E8E8`, the PC scheduler's `PcSched`, 672 B against the symbol file's 80); pointer-free struct layouts
  against DWARF (0 differences). Element overlaps in the arenas are resolved first-come and listed (8).
- **Link warnings**: the size and alignment warnings of game commons against `data.c` are gone (`D_80152818`, the
  last one, with the car layout of [CARS.md](CARS.md)). Before: `D_80152818` (5712 vs 8960), `D_80110680` (6 vs 2), `D_80146180` (4 vs 8),
  `input_rec0` (304 vs 320), and the alignments of `D_80149B64`, `D_80149B74` (aarch64: 4 vs a common's 8) and
  `D_80156958` (x86-64: 8 vs 32). The first three were the sizes above. `input_rec0`'s 76-byte records have a
  pointer at +0x48 (`Player76.ref`, into `D_80146150`), so its defining file's view (`func_800DE45C.c`) gets it under
  `TARGET_PC` and its records are 80 bytes there as in the storage. The last three are at N64 addresses less
  aligned than the host gives a common, so their tentative definitions (`controller_poll/group.c`,
  `audio_occlusion.c`) are declarations on PC (`PC_ONLY(extern)`; the N64 tokens are unchanged).
- **Post-link check**: `gen_data.py --check` reads the linked `rush2049pc_datacheck` with nm. All 1,797 arena
  names are absolute at their N64 addresses, the 77 names inside own storage sit at their base plus offset, all 384
  own storages have their sizes, and nothing else is linked at or above 0x80000000.
- **Unit test** `pc_data_test`: hand-written tables over a synthetic ROM with a raw and three DEFLATE images.
  It covers byte order per kind, a struct whose host layout differs (pointer, `long`), struct arrays (strides),
  pointers into own storage, arenas and the heap, per-image functions at a shared address, functions with no C,
  raw values, an overlay reload (data, BSS, game-C data), the report with the address-like words' targets, and a
  stream of the wrong size. It runs twice: with its arenas in host memory, and at their N64 addresses in a window
  it maps at 0x80000000, where an arena pointer is its own value and the runtime images' arenas stay clear until
  loaded.
- **Synthetic round trip with the real tables** (`rush2049pc_datacheck --synthetic`, which maps the window as
  port/ultra does): random data at the real image addresses, deflated into the real ROM slots; a known target
  planted in every pointer field (null, ROM offset, function, own storage, arena, heap). After the first load the
  runtime images' arenas must be clear; after both overlay loads an independent decoder checks all 4 arenas byte for
  byte, all 384 own-storage symbols element by element, and 3,217 planted pointers, before and after another
  overlay reload. It runs clean under AddressSanitizer (`data.c`, the loader and the check instrumented), and so
  does `pc_data_test`, leak checks included.
- **`rush2049pc --selftest`** checks that boot, game and runtime-image data names are at their N64 addresses
  (through the fixed-up GOT).
- **Determinism**: two runs with different `PYTHONHASHSEED` give identical outputs.

With the ROM, `rush2049pc_datacheck [path]` (default `baserom.us.z64` in the source tree; it passes with a
message when there is none) checks the SHA-1, the boot tail's build-info record ("07-17-00", "MERRILL",
"BLACKOUT"), the inflate tables against zlib's public constants (`gInflateBorder` u32, `gInflateCplens` and
`gInflateMaskBits` u16), that 90% of all f32 elements are finite and of sane size, the four file tables from
`ASSETS.md` §2.2 (kinds 0–2, model flags 0–1, offsets inside the ROM, sizes under 8 MB), that function pointers
resolve, and both overlay reloads (both are loaded first, as the game would). **The ROM was not present, so none of
this has run yet**, and there are no resolved and unresolved pointer counts, nor `addr-word` counts, for the real
data. Without a ROM the boot fills nothing but the (blank) boot tail, and the synthetic data is random.

## Conflicts and the work list (agent D)

From `data_layout.txt`:

- **Pointer conflicts: 106 symbols** whose files disagree on where pointers are (400 declaration pairs), for
  example `D_80110244` (`Entity *` in 7 files, `u32` in 2) and `D_8002EBB0` (`void *` vs `s32`/`u32`). The chosen
  layout follows the pointer, so the integer-declaring files read half a pointer. Two that the linker shows:
  - `player_array`: was 5712 B in `path_D169C`'s definition (6 × 952) against 6720 B of storage, with views that
    disagreed on which visual fields were pointers. Now every view has the PC car layout (0x468 bytes, 43
    pointers: the 21 visuals' `next` and `func`, and the input pointer) and the warning is gone; the model array
    `D_8014A250` keeps its N64 layout with u32 pointer fields and sits in the arena ([CARS.md](CARS.md)).
  - `input_rec0`: every view now has the 80-byte record, and the m2c code that added 76-byte strides uses
    `INPUT_SIZE` ([POINTERS.md](POINTERS.md) 10.3).
- **Width conflicts: 85 symbols** (198 pairs), such as `s32` vs `s8` for `D_8011028C`, `u64[]` vs `u8[4096]` for
  `gInflateBufferA`, or `Rec164A4` vs `s16[]`. For initialized data, a word layout over what is really bytes
  byte-swaps text, so these are worth reading.
- **Inside own storage, but with pointers the storage lacks: 10**, all in other parts' structures: MusyX's
  `D_80038000` hooks and stream state `D_80056230` (`lib_11640.c`, `lib_25bb0.c`), the scene-node table
  `D_8012E700`, and the object pool `D_80143FC8`, whose three `Pool` views disagree on its pointers. (The pool's
  views now agree, and its nodes keep their N64 layout in the arena block `D_8014D280`: POINTERS.md 10.2.)
- **Gap extents crossing symbol-table addresses: 44.** Open-ended arrays whose size came from the gap, but which
  run over an address the image's symbol table knows. Most are field addresses inside struct arrays
  (`player_array`, `Camera[]`). Some may be separate tables that no C declares: the file table's
  `D_8011B5BC` ran 12,720 B into the PVS tables until `extents.txt` gave it its 183 entries. A size in the C
  declaration is the fix, and `extents.txt` is the stopgap.
- `data_symbols.txt` lists every placed symbol with its storage, sizes, element kinds and chosen type.
- At run time, `data_report.txt`'s `addr-word` lines that point at host storage or code.

## What other parts need

- **Agent A (main, libultra, RDRAM):** link `pcdata_rt` (`port/data/pc_data.c`) and zlib into the real
  executable, and call `pc_data_init(pc_rom_bytes(), pc_rom_size())` before the game's main, after
  `pc_mem_fix_absolute` and the window (both done in `port/main.c`). `data.c` reaches the
  loader through weak references, so the link test still links without it (`pc_data_init` returns -1). Keep the
  executable PIE. The game heap must start at the end of the game's BSS, 0x8017A640 in the window: `D_8017A640`
  and `gBssEnd` are left undefined for that, along with the low-memory boot parameters (`osTvType`...), the ROM
  offset symbols (`gRomCompressedData`, `D_00039B40`), `gDecompressedDataStart`, `D_8008BEA4` (an address in the
  game's code used as data) and `D_8038A400` (the runtime images' base, used as the heap block's address). Boot
  commons (tentative definitions in `src/rom`, such as `gScheduler`) are left as they are for the libultra layer.
- **Agent D:** the conflict lists above. Declaring sizes for open-ended arrays makes extents exact.
- **Game build owner:** the 22 runtime-image statics (above). census.py could also leave test programs out; this
  branch removes them from `compile_commands.json`.
- **Graphics and audio:** the RSP microcode in the boot tail (`D_80027DD0`, `D_80028CC0`, `D_8002A050`) is in
  the boot arena with whatever element type its declarations give it, so don't expect the original byte order
  there.

## Untested

Everything that needs the ROM: the real values and the real pointer and `addr-word` counts. Also untested: an
overlay load called by the game itself (nothing runs the game that far yet), the x86-64 and macOS paths (thunks,
`N64_SYM`; on x86-64 the arena names are object-file absolute symbols reached through `-fPIC` GOT loads, like
`aliases.ld`'s, which only CI builds and runs), and whether the chosen declaration is right where files conflict.
