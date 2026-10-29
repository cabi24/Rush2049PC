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
| Rules for tables only pointers reach | `port/data/reached.txt` (section 7) |
| Game data the PC owns, with no N64 address | `port/data/decoupled.txt` ([DECOUPLE.md](DECOUPLE.md)) |
| PC data that must sit in the window, in ranges decoupled items freed | `port/data/reserved.txt` ([MORECARS.md](MORECARS.md) 8) |
| Tests | `port/data/test_pc_data.c` (synthetic tables, partly from gen_data's rules on `test_pc_data_decls.c`), `port/data/datacheck.c` (real tables: `--synthetic`, or your ROM), `port/data/test_overlay_data.c` (a runtime image's C data on reload), `port/data/test_reach.c` (the reach rules) |
| Overlay loads | `src/blob/groups/{heap_release,score_insert}/overlay_loader.c`, `{carcolor,state}/ctx_overlay_loader.c` |
| The runtime images' C data, one range each | `port/data/overlays.ld` (linked with `-T`), section 5 |

## How it works

### 1. Types from DWARF

The game objects are compiled with `-g`, so each object's DWARF holds its own declaration of every external
variable it uses. `gen_data.py` reads them with `readelf --debug-dump=info` (25 MB of text for the 1,346 objects;
the whole generator runs in about 3 s on the Pi 5). Typedefs and qualifiers are dropped; a struct one file leaves
incomplete is completed from a file that defines it.

The objects must be unoptimized: an optimized object's DWARF leaves out the variables whose uses the optimizer
removed, and the types nothing left uses. So gen_data reads `types_objects.txt`, the Debug build's game objects or,
in any other build type, a second copy compiled at `-O0 -g` with the game's own flags (`game_types`), and it refuses
DWARF whose producer line records optimization. The tables are then the same files in every build type
([OPTIMIZED.md](OPTIMIZED.md) 2.1); CI compares them.

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
| `gViewportOffsetX`, `gViewportOffsetY` (0x8002AFFC, 0x8002AFFE) | `s16[32]` (`m2c_types.h`) | `s16[9]`, 18 B, the PC declaration | one table `s16[3][4]` from `gViewportOffsetXExtra` (0x8002AFF8) to `gViewportPendingFrames` (0x8002B010), a row per TV type: `lib_21f0.c` reads `((s16 (*)[4])gViewportOffsetX)[gViewportX][0]` with `gViewportX` = `osTvType` (0-2). These two are its columns 2 and 3. The 64-byte views ran over `gViewportPendingFrames`, `gViewportDataPtr`, `gInitFlag` and `gDmaInitialized` (arena, section 3), and the loader swapped the two flags with the bytes after them |

### 2. Two layouts per type

Each canonical type is laid out twice. N64 (IDO, MIPS o32): `char` 1, `short` 2, `int`, `long`, `float`, pointers
and enums 4, `long long` and `double` 8 aligned 8, structs aligned to their widest member and padded to it. Host:
the DWARF offsets and sizes. The walk produces a *field program*: runs of (kind, count, N64 offset, host offset),
with a repeat count and two strides for arrays of structs. Kinds are u8, u16, u32, f32, u64, f64, pointer (4 to 8
bytes) and the N64's 32-bit `long` (4 to 8 bytes, signed or not). There are no bitfields or `long double`s in the
game's data declarations.

**Unions.** A union with a member made only of bytes (a `u8` array or a struct of `u8`s) that covers all of it is
copied as bytes, unless a member has a pointer or a `long` (whose host layout needs that member). Any other union
is laid out by its largest member with the most non-byte elements. The first kind is the game's colours
(`TextColor`, `tRGBA`, `RGBA`, `Color`, `Color4`: `{ u8 c[4]; u32 rgba; }` or `{ u32 word; struct { u8 r, g, b, a; }
rgba; }`): the ROM holds R, G, B, A, the game reads those bytes, and swapped as a u32 they came out A, B, G, R. Every
union in the layout was checked against the game's reads (`endian_check.py`'s accesses and value flow,
[ENDIAN.md](ENDIAN.md) 4). The 22 colour symbols in initialized data are read as bytes; their words are only
copied (into locals, scene nodes, the car records, by value) or tested for equality, and no code shifts them,
compares them with a constant or stores a word constant into them. So none is read both ways and none needs
`PC_RGBA`. The other unions keep their layout: `Vtx` (`{ Vtx_t v; long long }`: s16s and the `cn` bytes), `Gfx`,
`MenuSlot` (whose byte member is shorter than it), and the BSS ones (`__OSInodeUnit` is copied as bytes now too, but
BSS is zeroed). Loaded as bytes now, listed in `data_layout.txt` ("Unions copied as bytes"):

| Symbols | What |
|---|---|
| `D_80118E28`, `D_80118E2C` | the menus' flashing text colours (TB_FlashTextColors, read as bytes in `dispatch_handler`) |
| `D_801146BC` (`D_801146C0` is its second element, section 3) | their source pairs |
| `D_80116DB0` | the options screen's button colour |
| `D_8011AD8C`, `D_8011ADC0`, `D_8011AE58` | the shadow colour, the sheen colours, track 1's IR colours (GetCarsIRColor) |
| `D_8011B558`, `D_8011B578` | the arrows |
| `D_80394270`, `D_803942A0`, `D_803942A4`, `D_803942E8` | runtime image B (status scene, shots) |
| `D_803B3054`, `D_803B3058`, `D_803B3458`, `D_803B345C`, `D_803B8328`, `D_803B8354`, `D_803B8394`, `D_803B8398` | runtime image A (front-end colours) |

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
A **decoupled** symbol (`port/data/decoupled.txt`) has neither: the PC owns it with one declaration of its own, data.c
defines it outside the window at its host size, and its N64 name and every name inside its N64 extent are retired
([DECOUPLE.md](DECOUPLE.md)). The first is the race roster, `pc_links` (was `D_80153E88`), the second the model
records, `pc_models` (was `D_8014A250`), the third the car records, `pc_cars` (was `D_80152818`, which was own
storage: it leaves the loader's table). One that was initialized data (`init=image`: the cars' sounds-started flags,
engine phase and finish cameras, [DECOUPLE.md](DECOUPLE.md) 7) is filled from the image when the image loads
(section 4, step 4).
A **reserved** symbol (`port/data/reserved.txt`) is PC data with no N64 symbol that must still sit in the window,
because something keeps its address as a 32-bit N64 address: data.c defines it absolute at the address the rule
gives, at the size of its one declaration (laid out alike on the N64 and the host), in the game's BSS inside a range
a decoupled item freed, and over no placed symbol; it is zeroed with the arena's BSS, the loader's tables and `data_symbols.txt` don't
have it, `data_layout.txt` lists it under "Reserved", and `--check` verifies it in the executable. The first is
GetMatrix's spill, `pc_matrix_spill` at 0x8014A250 ([MORECARS.md](MORECARS.md) 8).

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

**Overlapping declarations.** In an arena every symbol's elements are swapped in place, so where declarations
overlap (one runs into the next symbol, or a name lies inside another's array) a byte can belong to two elements.
`arena_elements` takes the symbols in order, own storage first and then the most informative declaration (as the
ranking above: struct, pointer, words, bytes, then the larger), and leaves out an element over bytes an earlier
one already has at another start or width; `data_layout.txt` lists each under "Arena element conflicts". Bytes
take part: a word declaration that runs over a byte flag, or a word view of a byte union's element, is a conflict
too. (They did not before, so `gViewportOffsetX`'s `s16[32]` swapped `gInitFlag` with its neighbour unreported.)
The first in rank decides, and where that is the wrong one a size in the declaration is the fix (section 1). The
conflicts read and judged are `OVERLAPS_REVIEWED` in gen_data.py, listed apart with the reason; none is open:

| Symbol | Over | Kept | Why |
|---|---|---|---|
| `D_801146C0` (image A's `struct Color4 { u32 word; }`) | `D_801146BC[1]` | the bytes | a colour the game reads as bytes; `D_801146C0` is only passed by value |
| `D_801105A4`, `D_8011AC84` (`u8[]`) | `D_801102B4`, `D_8011A994` (`Button[]`) + 0x2F0 | the floats | `&buttonInfo[11].pos`, the audio and video screens' title positions, which ZOID_ProjectWindow reads as f32 |
| `D_803B5CFC` (`s32[][24]`) | `D_803B5D2C` (own `MultiBlit[]`) | own storage's | read as `[n][0]` for n = 1..4 players, inside `D_803B5D50`; row 0 never |
| `D_803B4679` (`s8[][7]`) | `D_803B467C` (own `Blit *`) | own storage's | read as `[n][i]` for n = 1..4 players, at 0x803B4680-0x803B469B (no symbol: bytes); row 0 never |

22 initialized symbols have a declared size that runs past the next known address ("Declared N64 size runs past
..."); apart from these, each agrees element for element with what it overlaps (nested names, the interleaved
viewport columns).

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
4. Each decoupled symbol whose N64 data the image initialized (`pc_inits`, [DECOUPLE.md](DECOUPLE.md) 7): its first
   elements converted from the image's bytes at the retired N64 address, the rest zero or continuing them with a
   period. It has no N64 address the tables resolve, and the arena's bytes there are left as the image has them.
5. Game-C data in a runtime image: its bytes are saved at startup and put back on every load (zeroed if BSS): the
   symbols the tables name, and each image's C range, which holds its file-local statics too (section 5).
6. After the first load, the runtime images' arenas are cleared (section 3).

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
symbol file's boot functions), held through weak references. 2,000 have game C (1,977 before the last 23 got theirs,
2026-10-27): every function of the game and both runtime images. The 134 without are the boot segment's: the
libultra, libc and inflate routines the PC layer replaces (port/ultra, the host's C library and zlib), 69 of them
under the same name, which the weak reference then reaches. A function with nothing linked is NULL there, not a
stub, unless some other file calls it (then the link test's stub is what it gets).

The last 23 were matched assembly with no C on the PC (`docs/game-asm-ceiling.md`); their C is in `nonmatching/`
(README there), and no code in any image calls them or builds their address, so the game reaches them only through
data:

| Function | What | Reached through |
|---|---|---|
| `StartWeaponIcon` | `_StartWeaponIcon`: a weapon icon's start function: set-up (hide it, one more power-up) or a car's pickup (weapon and shots, health, invisibility, shield) | the target info table `D_80117530` (its start slot): `engine_torque_calc` (flag 4, and flag 0x4000 without a NULL check), `race_setup_2` on a hit |
| `func_8010D680` | `_AnimateWeaponIcon`: spins a shown icon, shows a hidden one again once no car has its weapon | the table's visual slot: the environment visual `engine_torque_calc` grabs for the node |
| `animSpeedText` | the HUD's digital speedometer and its units | a MultiBlit entry of the HUD tables ShowHUD (`skid_mark_render`) passes to NewMultiBlit |
| `func_8010E8B4` | `animPanes@videoscr.c`: the video screen's title and menu frame | the video screen's MultiBlit tables `D_8011ACA0`/`D_8011ACE8` (video_initScreen) |
| `func_803A44FC` | `animText@carscr.c`: each player's name, ghosts and option values on the car-select screen | a callback of the car-select MultiBlit table `D_803B2AB8` (initScreen@carscr.c) |
| 18 stubs: 14 in image A (`func_8039A440` ... `func_803AECD4`), 4 in image B | the deleted bodies of inlined helpers: `jr ra; nop` on the N64, empty functions here | nothing (an address in data would now resolve) |

Without them, `engine_torque_calc` would call NULL for a weapon icon's record if it has flag 0x4000 (the ROM data
decides), and the other entries would be skipped (their callers test for NULL). The tests run each on synthetic
records: `assets_weapons`, `assets_hudtext`, `assets_carscr` (port/assets/tests).

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

**The images' C data.** On the N64 a load copies the image's `.data` and zeroes its `.bss`, so every variable its C
defines starts again from its initial value, file-local and function-local statics too. On the PC that C is linked
into the executable, and the tables name only symbols with N64 addresses. So each image's C data is gathered into
one range by `port/data/overlays.ld`, linked with `-T` (its `INSERT AFTER .data` adds to the default script; ELF and
PE ld alike): the `.data`, `.data.rel`, `.data.rel.local`, `.bss` and commons of the objects compiled from
`src/ovl_a`, `nonmatching/ovl_a` (`.ovl_a`, `pc_ovl_a_start`..`pc_ovl_a_end`) and `src/ovl_b`, `nonmatching/ovl_b`
(`.ovl_b`). data.c lists the two ranges (`PcDataCRange`), so every program that links data.c links the script.
The loader copies each range at startup, before the game runs, and puts it back on every later load of its image;
the other image's range is untouched. Read-only data stays where it was: it never changes.

| Image | File | C data (`data_layout.txt`, "Runtime-image C data") |
|---|---|---|
| A | `cleanup_colortrace/group.c` | `cursorSpeed.1`, `scrollSpeed.0`, `zoom.2`, `D_803B2858` (CompLight, global) |
| A | `runtime_a_c93c_closure/group.c` | `angle.8`, `cityPos.4`, `cursorSpeed.1`, `fogPos.7`, `fogTileSize.6`, `mapPos.2`, `mapPos.3`, `rotAngle.5`, `scrollSpeed.0` |
| A | `runtime_a_text_helpers/group.c` | `cursorSpeed.1`, `del.0` (a pointer: `.data.rel`), `scrollSpeed.2` |
| B | `shots.c` | `gShieldSizes`, `sWeaponFx` (and the const `CaseOffsets`, `ShotOffsets`) |
| B | `weapons.c` | `D_803943A4` (global) |

Of the 22 the report listed before, `CaseOffsets` and `ShotOffsets` are read-only, and `weapons.c`'s three
(`D_80399118` gCurWeaponType, `D_80399120` and `D_80399550`, the muzzle-flash records) were a second copy: on the N64
they are that file's statics at those addresses, and `func_8038CA24`, the weapon reset after a resurrect, writes
them by the addresses. On the PC the file had its own statics while `func_8038CA24` wrote image B's arena, so the
reset never reached the weapons. `weapons.c` declares them `extern` on the PC now (`WEAPON_STATE`; the N64 tokens are
unchanged), one storage in the BSS arena, which every load of image B zeroes. That leaves 17 statics, and the two
globals (`D_803B2858`, `D_803943A4`), restored already as game-C data and restored twice now, to the same bytes: 19
in the ranges. Each image's own storage (data.c, its N64 data with pointers or a host layout) needs nothing new: a
load converts it from the image again, or zeroes it if BSS (step 3 above).

Checks: `gen_data.py --check` (after linking `rush2049pc_datacheck`) finds each of the 19 inside its image's range,
by name and count. The ctest `pc_data_overlay` (`port/data/test_overlay_data.c`) links image B's `weapons.c` and a
stand-in for image A's C (`port/data/test_overlay/src/ovl_a/statics.c`: file-local and function-local statics, a
BSS array, a pointer to a static), both through the script, with the loader and tables of its own (image B's BSS
arena in host memory). It writes them all (the muzzle-flash records through `_RemoveCannMFlash` and
`_RemoveGattMFlash`), then checks that a load of one image puts back that image's and leaves the other's.

### 6. What the declarations miss: the report's text and undescribed lines

The retail cartridge showed two ways the declarations leave data in the ROM's byte order ([BRINGUP.md](BRINGUP.md)
2 and 8). The report says which:

- **`text-in-words <where> <value> -> <target> (<text>)`**: a pointer whose target lies in an element wider than a
  byte, in an arena or in own storage, while the image's bytes there read as text (three printable characters or
  more, then a NUL). That is a declaration that runs over a string, almost always an open-ended table sized by the gap
  to the next known address; the loader swaps the string with the table's elements. The fix is the table's size (in
  its C declaration under `TARGET_PC`, or `extents.txt`). The stats line counts them ("to text in words").
- **`undescribed <where> <value> -> <target>`**, with **`(text)`** when the bytes read as text. Text is fine as it is
  (bytes keep their order). A target without the mark is a table only pointers reach: a rule in `reached.txt`
  (section 7) or a declaration fixes it.

`pc_data_test` plants a string under an arena's u32 run and one under an own symbol's u32s, and an undescribed
string. The synthetic cartridge keeps its strings right after the tables the retail cartridge has them after
(`strings_after` in `tools/port/synth_rom.py`), and its generator refuses bytes a declaration covers there.

### 7. Tables only pointers reach (`port/data/reached.txt`)

Some data is reached only through pointers in other data: the track animations (per-track tables of sequences, each
with its frames, and of palette animations), the suspensions, the demo paths' start points. No symbol describes it,
so the loader left it in the ROM's byte order and the N64 layout, while the game reads it with the host layout. Each
line of `port/data/reached.txt` is a rule:

```
ROOT                        TYPE                 COUNT
D_8011A31C                  ObjectAnimation20    until count
ObjectAnimation20.frames    AnimationFrame12     parent count
D_80111560                  f32[9]               1
D_801173D8                  s16                  D_80117408[i]
```

- ROOT: a table of pointers with storage of its own (each of its pointers), or `Tag.field`, a pointer field of
  every element an earlier rule with that TYPE reaches.
- TYPE: a struct tag of the game C (gen_data takes its layout from the DWARF, as for any symbol), or `u8`, `s8`,
  `u16`, `s16`, `u32`, `s32`, `f32`, optionally `[N]`.
- COUNT: `N`; `until FIELD` (the element whose FIELD is zero ends the table and belongs to it); `parent FIELD` (that
  field of the element holding the pointer); `TABLE[i]` (element i of a table of integers, for the root's pointer i).

gen_data checks each rule against the DWARF (the type, its fields, the root's pointers) and emits it as a
`PcDataReach` with the type's runs. After every load of an image the loader follows that image's root rules: for each
pointer it reads the count from the ROM, then

- a type with pointers or a host layout of its own gets host storage of its own (`calloc`, once per table per load,
  shared by every pointer to it), converted element by element through the runs, its field rules followed, and the
  pointer in the root (or the parent's copy) rewritten to it. What the game writes there is lost on a reload of the
  image, as on the N64;
- a pointer into a symbol whose own storage holds that type already (gen_data lists them: the shared frames
  `D_8011905C` for `AnimationFrame12`) resolves as usual;
- any other type is swapped in place in its arena, unless an element there describes it already.

The report has a `reached` line per table (its address, count, and which of the three), and the stats line counts the
tables and elements. The counts come from the ROM at run time; the repository holds only the rules. `extents.txt`
bounds the two animation pointer tables to their 19 tracks, which by the gap ran over the palette animations.

Tests: `pc_reach_test` (`port/data/test_reach.c`: hand-written rules on a synthetic image: a sequence table ended by
a zero count, its frames by the parent's count, one table reached by two pointers, frames inside a compatible symbol,
s16 arrays swapped in place by a table of counts, the report, a reload); `datacheck --synthetic` plants NULL in the
rules' roots (their targets would be random); the synthetic cartridge's track 0 has a palette and an object
animation, and its race faults in `camera_shake_start` without the rules.

### 8. Declarations that cover part of a table: the sweep (2026-10-28)

Two bugs of one class were found the night before the beta: one typed declaration covered only part of a table,
and the loader left the rest in the ROM's byte order. The engine table `D_8010FD80` was `{s32 object; u8 rest[28];}`,
so its levels read as denormals and no engine played (AUDIO.md 9). The cheat codes `D_803B3460` had a byte-only
view that won the ranking (QA.md 1). This section is the sweep of the whole class.

**How.** Six passes, each over every initialized symbol:

1. **Every file's declaration against the loader's elements.** gen_data's own `generate` was run with its `emit`
   hooked, so the types, extents and choices were exactly the generator's. Each declaration's elements (open arrays
   sized as gen_data sizes them) were compared, offset by offset, with what the loader swaps there: the arena's
   resolved elements, or the own symbol's runs. 11 symbols have a declaration that reads something wider than the
   loader swaps. For each, a gdb script on `rush2049pc_datacheck` (stopped after both runtime images load) checked
   whether the ROM value at every such place is byte-order neutral.
2. **The loader's report** on the retail cartridge: `text-in-words` and the `undescribed` targets that aren't text.
3. **The converted memory.** The same stop, and every 4-byte place the loader left as bytes, outside read-only data.
   A place is a hit if its big-endian reading is a plausible float, an N64 address or a small integer and its
   little-endian reading isn't. Every f32 element that isn't plausible as loaded is also listed. Text, short
   strings, and the images' read-only data (jump tables, literal pools: unused on the PC) were left out.
4. Each hit was judged against the game C's reads of that address: by name, by index past a declared element, by
   pointer.
5. **The pointer graph.** For every pointer field of the converted tables (1,623 typed fields in 36 tables), the
   declared pointee's elements were taken from the DWARF of the file whose declaration was chosen. They were
   compared with what the loader made of the target at the value the pointer holds after loading: wider elements
   left as bytes, and bytes under a wider element. The untyped pointers (`void *`, `char *`) were grouped by their
   targets as well. Pointers into no symbol are the report's `undescribed` lines, all text or judged below. Into
   described data: the bodies (fixed above), zlib's tables, the car descriptors, and the respawn records (fixed
   below).
6. **Values plausible in both byte orders.** For every u16, u32 and f32 element the loader converts, in arenas and
   own storage, the test asks whether the byte-reversed reading is systematically simpler than the loaded one: an
   integer 16 (u16) or 256 (u32) times smaller, or a float with ten more trailing zero mantissa bits and a sane
   exponent. That is the shape of a wrong width that plausibility alone misses. The few tables where the reversed
   reading wins are the PVS bitmasks (no preference either way), display-list words (right as loaded), floats under
   the reviewed `s32[][24]` overlap (a u32 swap is an f32 swap), and the car's UV tables `D_8011B57C`..`D_8011B59C`,
   which every reader copies as u16, the width they are swapped at. endian_check's section 3 (ROM data accessed at
   another width than the loader's, through casts and `M2C_FIELD` too) is empty.

The scripts and their output stay out of the repository: they read the cartridge.

**Fixed** (ab7f592). Each table's rest was in the ROM's order. LOADER_EXPECT and datacheck's `known_widths` pin
them; `--check-loader` fails 7 places with the old tables. The synthetic cartridge sets the bodies (body 0 and the
last element), every car type's wings and the 36 rumble thresholds (small, so `rumble2p`'s motors still run). Its
generator refuses each of the three with its old declaration, so `synth_rom` fails and no synthetic path runs.

| Symbol | Found by | What the game reads | On the PC before | Fix |
|---|---|---|---|---|
| `D_80118C10` | pass 3: a `u8[]` whose every word is a float | the level objects' bodies, 32 bytes each: CollisionReckon (`random_float.c`) reads bounds and mass as f32 through `Matrix64.record` (level_objects, kind 4). Then the static state records of the categories `D_80118DDC` points at (three of the report's `undescribed` lines) | the bounds and mass of every kind-4 object were denormals: car-to-object collisions used a size and mass of about 0 | `f32[115]` in level_objects (`BODY_TABLE` keeps the record address in bytes) |
| `D_801163F8` | pass 3: small words after an `s32` | func_800E56F8 walks 36 rising thresholds by pointer (`0x24` steps) to set the rumble request `D_801440AD` from the car's speed, minus 4 | only the first was swapped, and the other 35 compared as byte-reversed words, so the count, and the rumble request, came out wrong: little or no rumble | `s32[36]` in func_800E56F8 (`RUMBLE_STEPS`) |
| `D_8011B4B4` | pass 3: floats after one row | the wings' place per car type: buffer_swap reads `[model->type][0..1]` (visual_tire_module); func_800B0A88 reads column 1 by the car's byte `b8` (`audio_frame`, `audio_frame_update`) | `D_8011B4B8` (its `[0][1]`) bounded the gap to one row, so every car type but the first put its wings at about 0, the car's centre | `extents.txt`: 13 rows, to `D_8011B550` |
| `D_801202F8` gravityArray | pass 3: f32 elements that are text | its four floats, by the gravity option | its floats were right, but the gap ran over the strings after it (the asm's `D_80120308`, `D_8012030C`, which no PC C uses) and swapped them as floats | `extents.txt`: 16 bytes |

**Judged and left.**

- Image A's button and model slots `D_803B6610`, `D_803B6B14`, `D_803B470C` (pass 1). A view with byte pads
  (`Slot60`, settings_values' `Slot`: `u8 unknown0C[36]` over the 3x3 matrix) ties with the record (`ButtonInfo`,
  `MenuObjectSlot`, `Slot64`) and wins by its name. That is the cheat codes' mistake with a struct that isn't all
  bytes. So the matrices, and `Slot64`'s word at +0x3C, stay in the ROM's order. On the cartridge every one of those
  places (162, 432 and 88 elements) reads the same in either byte order. The screens' init also copies the unit
  matrix into each slot whose handle is -1 before using it. If they ever need fixing, the fix is the records' fields
  in the padded views, as `D_803AF9A8`'s (ENDIAN.md 3.3), not a ranking rule. A rule that prefers more bytes in wider
  elements would rank settings_bar's `MenuSlot` (`float matrix[16]`) over the record, and swap the alpha byte at
  +0x3C, `D_803AF9A8`'s first bug.
- Pass 1's others: the colour unions `D_801146BC`, `D_801146C0`, `D_8011ADC0` (bytes by design, section 2);
  `D_8011B58C` and `D_8011B5AC` (`tUVPair`, read as u16) and `D_8011EA30` (a viewport, s16), whose `s32` views only
  take the address; `D_80116DE4`, an `s32` view over a byte place that is neutral; `D_803B5CFC` (section 3).
- The report: no `text-in-words`. The `undescribed` targets that aren't text are those BRINGUP.md "What's next"
  lists: `gTrackColors` (`D_8011AF90`) to the colours `D_8011AE58` (bytes); pointer tables of short labels shorter
  than the text mark (`D_801147C8`, `D_801149B4`, `D_80114A0C`, `D_801164D0`, `D_8011EAD0`, `D_80117530`); and the
  shared frames `D_8011905C`'s pointers into the bytes after `D_80118EE4`.
- Words after a declared table that read as floats, addresses or integers in the ROM's order, which no PC C reads:
  after `gSongNames` (`D_80110030`, 12 used: `songIdx < 12`), a table of string pointers with no name and no pointer
  to it; after `D_80115CA8`, `D_80117360`, `D_801140F0` (the asm's `D_801140F4`, `D_801140F8`), `D_80116198` (the
  asm's `D_801161B8`, `D_801161BC`), `D_80110F80`, `D_8011E7A8`, and image A's `D_803B3030`, `D_803B832C`,
  `D_803B837C`. A C reader added later must declare them.
- f32 elements not plausible as loaded: -1 handles under f32 views (a word either way), the car descriptors'
  `tire_info` words (`D_801108C8`, QA.md round 3), and read-only constants.
- Pass 5's other hits: the suspensions (`D_80111560`) and the s16 tables of `D_801173D8`, which reached.txt's rules
  swap in place (the report's `reached` lines); the shared frames' `resource` pointers (`D_8011905C`), which
  camera_shake_start sets from the WAD before any use; and byte pads in the `Car` and `Resource32` views, which
  nothing reads.
- The boot data: libultra's VI modes and zlib's tables, which port/ultra and zlib replace (datacheck checks the
  inflate tables in the N64 order), and the RSP microcode's data.

**Fixed later** (614c8f9, pass 5). `D_801106C4` was FSCLS's `f32[]`, sized by the gap. The gap holds the
tracks' respawn records, `{s8 flag; f32 pos[3]}`, which `D_801108B0` points at and world_object_destroy walks. The
loader swapped each flag byte as part of an f32, so the PC read every flag as 0: func_800E9C70 got mode 0 where
the track's point asked for 1 (it sets `D_8012E670[car]`, which no PC C names a reader of; what a player sees is
unverified). runtime_a_joint_transforms declares FSCLS and the 30 records up to `D_801108B0` under TARGET_PC
(`FSCLS` keeps the N64 tokens). LOADER_EXPECT, `known_widths`, and a flag byte on the synthetic cartridge pin it.

Not swept: BSS (zeroed), and the ROM files port/assets converts (the tracks' and cars' WADs: LOADERS.md). Debug and
RelWithDebInfo make the same `data.c` and `data_symbols.txt` with these fixes.

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
  against DWARF (0 differences). Element conflicts in the arenas, bytes included, are resolved by rank and listed:
  none open, 5 read and judged (section 3).
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
  loaded. One arena's runs come from gen_data's own rules: `gen_data.py --test-tables` reads the DWARF of
  `port/data/test_pc_data_decls.c` and writes them, with the element conflicts it finds, to
  `pc-data/test_pc_data_gen.h`. The declarations: two colour unions (bytes stay in order, also under a word view
  of one element), a union no byte member covers (swapped), one whose byte member is shorter (the widest member
  decides), two interleaved s16 columns at their true size and the flag after them (untouched), and a table
  declared too long, whose run over its flag the header must list.
- **Synthetic round trip with the real tables** (`rush2049pc_datacheck --synthetic`, which maps the window as
  port/ultra does): random data at the real image addresses, deflated into the real ROM slots; a known target
  planted in every pointer field (null, ROM offset, function, own storage, arena, heap). After the first load the
  runtime images' arenas must be clear; after both overlay loads an independent decoder checks all 4 arenas byte for
  byte, all 384 own-storage symbols element by element, and 3,217 planted pointers, before and after another
  overlay reload. It runs clean under AddressSanitizer (`data.c`, the loader and the check instrumented), and so
  does `pc_data_test`, leak checks included. Those checks follow the tables, so `datacheck.c` also has 24 places
  whose element width the game's reads fix, written down apart from them (`known_widths`: the colour unions,
  `Vtx` and `MenuSlot`, the boot's viewport table and the flags after it), checked against the images. With the
  union rule and the viewport declaration both undone, 18 of them fail (the 16 colour places and the two flags),
  and so do 28 checks of `pc_data_test` and 50 of `endian_check --check-loader`.
- **`rush2049pc --selftest`** checks that boot, game and runtime-image data names are at their N64 addresses
  (through the fixed-up GOT).
- **Determinism**: two runs with different `PYTHONHASHSEED` give identical outputs.

With the ROM, `rush2049pc_datacheck [path]` (default `baserom.us.z64` in the source tree; it passes with a
message when there is none) checks the SHA-1, the boot tail's build-info record ("07-17-00", "MERRILL",
"BLACKOUT"), the inflate tables against zlib's public constants (`gInflateBorder` u32, `gInflateCplens` and
`gInflateMaskBits` u16), that 90% of all f32 elements are finite and of sane size, the four file tables from
`ASSETS.md` §2.2 (kinds 0–2, model flags 0–1, offsets inside the ROM, sizes under 8 MB), that function pointers
resolve, `known_widths` against the images it inflates itself, and both overlay reloads (both are loaded first,
as the game would). On the retail cartridge (2026-10-27) every check passes, and the game runs on what the loader
makes of it: the front end, races in every mode tried, the attract demo ([BRINGUP.md](BRINGUP.md)), after the fixes
there to declarations the real data showed wrong (sections 6 and 7). The counts aren't written here: they describe
the ROM. Without a ROM the boot fills nothing but the (blank) boot tail, and the synthetic data is random.

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
- **Game build owner:** census.py could also leave test programs out; this branch removes them from
  `compile_commands.json`. (The 22 runtime-image statics are reset now: section 5.)
- **Graphics and audio:** the RSP microcode in the boot tail (`D_80027DD0`, `D_80028CC0`, `D_8002A050`) is in
  the boot arena with whatever element type its declarations give it, so don't expect the original byte order
  there.

## Untested

The retail data where the game hasn't run yet (BRINGUP.md, "What's next"). Also untested: the x86-64 and macOS
paths on the retail cartridge (thunks,
`N64_SYM`; on x86-64 the arena names are object-file absolute symbols reached through `-fPIC` GOT loads, like
`aliases.ld`'s, which only CI builds and runs), and whether the chosen declaration is right where files conflict.
