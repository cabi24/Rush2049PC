# Game data on the PC (port/data)

The game C declares about 2,200 data symbols at fixed N64 addresses that nothing defines: the initialized data
of the boot segment, the game image and the two runtime images, and their BSS. The PC build gives each one host
storage with a correct host layout, and fills the initialized ones from the player's ROM at startup. Neither the
executable nor the repository holds ROM bytes: the build knows only addresses, sizes and layouts, and every value
is read from the ROM at run time. An overlay "load" is the same fill run again for one image.

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

Files often disagree. The generator ranks each distinct declaration of a symbol: struct with pointers, struct,
pointer, words, bytes, unknown; then the larger N64 size, then the number of files that agree. The winner sets the
layout. Disagreements that change where pointers are, or element widths, are listed in `data_layout.txt` (below).

Sizes: the symbol file's `size:` comment, else `port/data/extents.txt`, else for an open-ended array (`T x[]`) the
gap to the next known address (or the largest complete declaration if that is longer). Complete declarations
keep their own size.

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

Each image's data and BSS is one zeroed **arena** laid out as on the N64:

| Arena | N64 range | Image |
|---|---|---|
| `pc_n64_boot` | 0x800277D0–0x80086A50 (data to 0x8002E8E0) | boot segment tail, a plain copy of ROM 0x283D0 |
| `pc_n64_game` | 0x8010FD7C–0x8017A640 (data to 0x801249F0) | game image, DEFLATE at ROM 0xB0CB10 |
| `pc_n64_ovl_a` | 0x803AF978–0x803BAE20 (data to 0x803B9A50) | runtime image A, DEFLATE at ROM 0xB5C534 |
| `pc_n64_ovl_b` | 0x80393B34–0x8039B440 (data to 0x80394F70) | runtime image B, DEFLATE at ROM 0xB6FEC4 |

A symbol with no pointers whose two layouts agree is defined at its N64 offset in its arena (an assembler `.set`),
so code that indexes from one symbol into its neighbor still works. Each arena starts at the same address modulo
16 as on the N64, so 8-byte data stays aligned. A symbol whose type has pointers, a `long`, or a host layout that
differs gets its **own storage** (16-aligned, host size), and every symbol whose address lies inside its N64
extent is defined at the matching host offset (`input_rec1 = D_8014A118 + 0x50`). Names at one address are
aliases. Tentative definitions (common symbols) in game and runtime-image C get the same treatment; the linker
prefers the strong definition. The two names one past an overlay's BSS (`D_803BAE20`, `D_8039B440`, the
loaders' `bssEnd`) point one past its arena.

Result: **2,241 names at 2,203 addresses**. 1,725 addresses are in arenas (999 data, 726 BSS) and 365 have their
own storage (138 data, 227 BSS). 111 more lie inside own storage, and 2 are end markers. 1,138 addresses are
filled from the ROM and 1,063 are zeroed.

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
2. Each arena: copy the initialized part, zero the BSS, byte-swap every described element in place. Pointers
   stay N64 addresses there (in host byte order).
3. Each symbol with its own storage: convert from the image element by element through its runs.
4. Game-C data in a runtime image: its bytes are saved at startup and put back on every load (zeroed if BSS).

Pointer values are turned into host pointers in this order:

| N64 value | Becomes | Count kind |
|---|---|---|
| 0 | NULL | null |
| not an RDRAM address (ROM offsets such as `D_8002B020` = 0xB5C534, segment addresses) | the value, unchanged | raw |
| ≥ 0x8017A640 (end of the game's BSS) and outside both runtime images | **the value, unchanged** | heap (kept) |
| a function entry: the data's own image first (A and B share code addresses), any image from game or boot data | the host function | functions |
| a function with no C yet | NULL, logged once | no C |
| inside own storage or game-C data | its storage plus the offset mapped through its runs | own, C data |
| inside an arena | arena + offset | arena |
| anything else | the value, logged once | unresolved |

The heap rule follows `docs/port/ASSETS.md`: the game heap is the RDRAM window at host 0x80000000
(`PC_RDRAM_BASE`), so heap addresses are already valid host addresses, and the renderer reads values below 4 GB
as N64 addresses. The runtime images are the exception inside the heap: on the PC their code is linked in and
their data lives in the arenas, so nothing is inflated into the window at 0x8038A400. Addresses in their code,
data or BSS resolve to host functions and arenas; the rest of the reserved block (0x803BAE20–0x803BB380) is kept.
Host storage must sit above 4 GB, so the executable must be PIE: `pc_data_init` fails and says so otherwise.
Function pointers come from a table of 2,134 functions by address (`layout/*.json`, `manifest/static.json`, the
symbol file's boot functions), held through weak references. 1,976 have C. A function with no C is NULL there,
not a stub, unless some other file calls it (then the link test's stub is what it gets).

The first load writes a report, to `$PC_DATA_REPORT` or else `build/pc/data_report.txt` (`PC_DATA_REPORT=""`
turns it off):

- `unresolved <symbol>+<offset> <value>`: a pointer below the heap that is in no symbol;
- `no-code <symbol>+<offset> <value> <function>`: a pointer to a function with no C yet;
- `addr-word <symbol>+<offset> <value> -> <target>`: a word in a non-pointer (u32) field whose value looks like an
  N64 address (0x80000000–0x80800000). These are likely mistyped declarations: agent D's work list.

### 5. Overlays

Both runtime images are linked in. An overlay load is `pc_data_overlay_load(PC_OVERLAY_A | PC_OVERLAY_B)`: it
refills that image's arena and own storage from its cached inflated image, zeroes its BSS, and restores its
game-C data. The other image is untouched.

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
| `D_8014A250_alias` | 0x8014A250 | BSS | own (also D_8014A250) | `tModelData[]` |
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
| `player_array` | 0x80152818 | BSS | own (also D_80152818) | `tCarData[]` |
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
  (0 failures; the generator fails the build otherwise); N64 sizes against the symbol file (1 difference:
  `gViMgrState`, `OSDevMgr` 28 B vs 32 with padding); pointer-free struct layouts against DWARF (0 differences).
  Element overlaps in the arenas are resolved first-come and listed (8).
- **Post-link check**: `gen_data.py --check` reads the linked `rush2049pc_datacheck` with nm. All 1,876
  placements sit at their base plus offset, and all 365 own storages and 4 arenas have their sizes.
- **Unit test** `pc_data_test`: hand-written tables over a synthetic ROM with a raw and three DEFLATE images.
  It covers byte order per kind, a struct whose host layout differs (pointer, `long`), struct arrays (strides),
  pointers into own storage, arenas and the heap, per-image functions at a shared address, functions with no C,
  raw values, an overlay reload (data, BSS, game-C data), the report, and a stream of the wrong size.
- **Synthetic round trip with the real tables** (`rush2049pc_datacheck --synthetic`): random data at the real
  image addresses, deflated into the real ROM slots; a known target planted in every pointer field (null, ROM
  offset, function, own storage, arena, heap). An independent decoder then checks all 4 arenas byte for byte,
  all 365 own-storage symbols element by element, and 3,218 planted pointers, before and after an overlay reload.
  It also ran clean under AddressSanitizer (`data.c`, the loader and the check instrumented).
- **Determinism**: two runs with different `PYTHONHASHSEED` give identical outputs.

With the ROM, `rush2049pc_datacheck [path]` (default `baserom.us.z64` in the source tree; it passes with a
message when there is none) checks the SHA-1, the boot tail's build-info record ("07-17-00", "MERRILL",
"BLACKOUT"), the inflate tables against zlib's public constants (`gInflateBorder` u32, `gInflateCplens` and
`gInflateMaskBits` u16), that 90% of all f32 elements are finite and of sane size, the four file tables from
`ASSETS.md` §2.2 (kinds 0–2, model flags 0–1, offsets inside the ROM, sizes under 8 MB), that function pointers
resolve, and both overlay reloads. **The ROM was not present, so none of this has run yet**, and there are no
resolved and unresolved pointer counts for the real data.

## Conflicts and the work list (agent D)

From `data_layout.txt`:

- **Pointer conflicts: 107 symbols** whose files disagree on where pointers are (401 declaration pairs), for
  example `D_80110244` (`Entity *` in 7 files, `u32` in 2) and `D_8002EBB0` (`void *` vs `s32`/`u32`). The chosen
  layout follows the pointer, so the integer-declaring files read half a pointer.
- **Width conflicts: 80 symbols** (191 pairs), such as `s32` vs `s8` for `D_8011028C`, `u64[]` vs `u8[4096]` for
  `gInflateBufferA`, or `Rec164A4` vs `s16[]`. For initialized data, a word layout over what is really bytes
  byte-swaps text, so these are worth reading.
- **Gap extents crossing symbol-table addresses: 47.** Open-ended arrays whose size came from the gap, but which
  run over an address the image's symbol table knows. Most are field addresses inside struct arrays
  (`player_array`, `Camera[]`). Some may be separate tables that no C declares: the file table's
  `D_8011B5BC` ran 12,720 B into the PVS tables until `extents.txt` gave it its 183 entries. A size in the C
  declaration is the fix, and `extents.txt` is the stopgap.
- `data_symbols.txt` lists every placed symbol with its storage, sizes, element kinds and chosen type.
- At run time, `data_report.txt`'s `addr-word` lines.

## What other parts need

- **Agent A (main, libultra, RDRAM):** link `pcdata_rt` (`port/data/pc_data.c`) and zlib into the real
  executable, and call `pc_data_init(pc_rom_bytes(), pc_rom_size())` before the game's main. `data.c` reaches the
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

Everything that needs the ROM: the real values and the real pointer counts. Also untested: an overlay load
called by the game itself (nothing runs the game yet), the x86-64 and macOS paths (thunks, `N64_SYM`), and
whether the chosen declaration is right where files conflict.
