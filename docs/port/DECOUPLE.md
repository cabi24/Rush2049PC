# Decoupling game data from its N64 address

Most of the game's data sits at its N64 address in the RDRAM window ([DATA.md](DATA.md) 3), so the decompiled C runs
unchanged. That costs the port: an array can't grow (its neighbours follow it), address arithmetic between symbols
is baked in, AddressSanitizer can't run on x86-64, and macOS arm64 can't map the address. Decoupling moves one item
to storage the PC owns outright, with one PC declaration and no N64 address. The first item is the race roster
(`D_80153E88`, arcade `gLink`), a first step toward 8 cars a race. 2026-10-28.

| Piece | Where |
|---|---|
| The PC declarations and accessors | `port/include/pc_game_types.h` (included by `pc_prelude.h`) |
| The rules: PC symbol, retired N64 symbol and size | `port/data/decoupled.txt` |
| Storage, checks, report | `tools/port/gen_data.py` (`load_decoupled`, `decoupled_type`, `emit`, `--check`) |
| Test | `port/assets/tests/test_roster.c` (`assets_roster`) |

## 1. The recipe

1. **Inventory.** Every object that references the symbol or any name inside its N64 extent (nm over
   `game_objects.txt`; the image symbol tables name the addresses the original code reached, `asm/us/*/symbols.json`);
   each declaration's type (the objects' DWARF, as gen_data reads it); each access's offset, width, signedness, and
   the indices it walks; arithmetic from a neighbour into it or from it out (`bounds_check`, `offset_check`,
   `layout_check`); and whatever holds its N64 address: ROM data words, `PC_N64W` stores, absolute addresses in C.
   The 410 m2c extern blocks that only declare a name don't count: without a use there is no reference and no DWARF.
2. **Type.** One PC declaration in `pc_game_types.h`, with the fields the accesses show and the size the code walks.
   A size from a `PC_` constant, never shorter than what the code walks. The declaration is then the storage's true
   size, which an optimizing GCC may reason from correctly ([OPTIMIZED.md](OPTIMIZED.md) 2.7); `bounds_check` lists
   the loops it indexes (*tmr*), which go in `bounds_reviewed.txt` with that verdict.
3. **Storage.** A line in `decoupled.txt`. gen_data then defines the PC symbol in `data.c` (zero at startup, aligned
   as own storage, outside the window, not in the loader's tables) at the host size of the one type every reader
   declares, and retires the N64 symbol, its aliases and every symbol-table name inside its N64 extent. The build
   fails if any game object still references a retired name, if the readers' types disagree, or if the PC symbol is
   defined by game C. `--check` (after linking `rush2049pc_datacheck`) fails if the PC symbol is missing, has another
   size or is in the window, or if a retired name is defined. Only BSS so far: initialized data would also need the
   loader to convert it from the ROM into storage it has no address for.
4. **Readers and aliases.** Each use goes through an accessor (`PC_LINK_OWNER(i)`) whose PC side
   `pc_game_types.h` defines. Each game file defines the N64 side under `#else`, with its own view and its own field
   names, so the N64 tokens are the original ones and `n64_neutral.py` passes. An alias inside the extent becomes
   the accessor of the field it named (`D_80153E8F[slot * 8]` is `PC_LINK_OWNER(slot)`).
5. **Proof.** `data.c` and `data_symbols.txt` change only by the retired names and the new storage (Debug and
   RelWithDebInfo still the same files); the synthetic cartridge is the same file; `synth_compare.py --same` with the
   build before and after on every synthetic path, and on the retail cartridge; the five checks; a test that runs
   the item's own readers and writers with more slots declared than the game walks.

## 2. The pilot: the race roster

**What it is.** One 8-byte record per car slot, 13 slots, at 0x80153E88-0x80153EF0 in the game's BSS: who drives the
car and how it starts. The arcade's `LinkInfo` (`include/game/pdu.h`) has the same purpose but another layout (u32
times, a version, a model pointer); the N64's is bytes only:

| Offset | Field | Values | Written by | Read by |
|---|---|---|---|---|
| +0 | `track` | the track (`D_8014978C`) | func_800EC190, CalcPolePositions, props.c | nothing |
| +1 | `car` | car model 0-12 | func_8039156C (= slot, all 13), LoadCars, props.c, cleanup_colortrace | world_physics_tick, LoadCars, func_800BB02C, sfx_position_3d, music_seq_load, props.c |
| +2..+4 | `color[3]` | the car's colours | LoadCars, props.c, cleanup_colortrace, func_800BB02C | world_physics_tick (into the model's +0x7CD..+0x7CF), func_800BB02C |
| +5 | `place` | start place | func_800EC190, CalcPolePositions, props.c | world_physics_tick, func_800EC914 (the car's +0xEE), init_mdrive (the camera), CalcPolePositions |
| +6 | `flags` | 0x80 active (`LINK_ACTIVE`), 0xB0 in a race | func_800EC190, CalcPolePositions, props.c, cleanup_colortrace | world_physics_tick, func_800EC914 |
| +7 | `owner` | 0-5 a drone that node drives (0 here), 6 a human, 7 empty | func_800EC190, CalcPolePositions, props.c, display_list_flush (`-= 6`) | 20 files: the race, camera, music, menus, resurrect, checkpoints, stat_lap_split (through `D_80153E8F`) |

**Inventory.** 30 objects reference the two names (nm): 29 game files under `src/`, some of them copies of one
function in another group (`loadcars/setup.c` of func_800BB02C, `input_aux/ctx_init_state_continue.c`, `demo_pair`'s
two), and `nonmatching/display_list_flush.c`.
They had 13 views of the record (`tLink`, `Link`, `Config`, `Slot`, `Player`, `Selection8`...), all 8 bytes, all
bytes; the signed ones (`s8 owner` at +0 in func_800EC190, `s8 type` in CalcPolePositions, the colours in LoadCars,
props.c and cleanup_colortrace) are only stored to, so one `unsigned char` type reads the same values. Every access
is one byte through an index or a record pointer: no file reaches a neighbour from it or it from a neighbour
(`bounds_reviewed.txt` and the data tables have no view across it). The image symbol tables name five addresses in
it: `D_80153E88`, `D_80153E89` (`[0].car`), `D_80153E8F` (`[0].owner`, read by `stat_lap_split` and func_800E56F8's
group with a stride of 8), `D_80153E97` (`[1].owner`) and `D_80153EB8` (`&[6]`, likely the end pointer of a
6-slot loop); only the first and third had C users. No PC code stores its address (`PC_N64W`, a u32 field, an
absolute address). A scan of the four images inflated from the retail cartridge found no data word in
0x80153E88-0x80153EF0, and 38 code references (`lui 0x8015` plus an offset in range: 36 in the game image, 2 in image
A), which are the C above.
`layout_check.py --storage` before: 123 accesses on `D_80153E88` and 2 on `D_80153E8F`, which it could not place in
the record (`N64 ?`); after: 125 on `pc_links`, every one a byte at the offset of the reference layout, the two
through the alias at +7.

**The size.** A race on the N64 activates `MAX_LINKS` 6 slots (the per-car arrays are 6 long; gen_data had the 104
bytes by the gap). But three functions walk 13 slots: LoadCars (slots 6-12 copy slot 0's car), func_8039156C (each
slot's car is its index) and sfx_position_3d (an engine sound per slot, with `D_80142B08[13]`). The gap is exactly 13
records. So the storage is `PC_LINK_SLOTS` = the larger of 13 (`PC_N64_LINK_SLOTS`) and `PC_MAX_LINKS` (8, the cars a
race may hold on the PC); with 8 it is 13, 104 bytes, and the game still activates 6.

**The change.** `pc_game_types.h`: `struct pc_link`, `pc_links[PC_LINK_SLOTS]`, accessors `PC_LINK_TRACK(i)` ...
`PC_LINK_OWNER(i)`, `PC_LINKP_*(p)` for record pointers, `PC_LINK` and `PC_LINKS`. The 30 files use them; each
defines the N64 side with its own view (`#define PC_LINK_OWNER(i) D_80153E88[i].state`, `#define PC_LINK_OWNER(i)
*(&D_80153E8F + (i * 8))`). `decoupled.txt`: `pc_links D_80153E88 0x68 D_80153E8F`. `test_cars` and
`test_audiocmds` define `pc_links` where they defined `D_80153E88`.

**What the tables say.** `data_symbols.txt` loses its two rows; `data.c` loses their two `.set` lines and two report
names, and gains `pc_links` (104 bytes, own-storage style, `.bss`); `data_layout.txt` lists it under "Decoupled". The
loader's symbol table is the same (419 entries), so its summary line, part of every trace, is too. The synthetic
cartridge, generated from `data_symbols.txt`, is the same file byte for byte.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before against after, Debug, Pi 5 (aarch64), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| ctest, build host (x86-64, GCC 14): Debug `build/pc`, RelWithDebInfo `build/opt` | 240 of 240 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files |
| `n64_neutral.py --base 09da2f0` | passes (29 changed game files, all "shifted") |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after; the reports differ only by line numbers and one storage fewer (two roots, `D_80153E88` and `D_80153E8F`, are one, `pc_links`) |
| `bounds_check`, build host (GCC 14) | 5 new *tmr* entries on `pc_links`, none `clamped` or `two offsets`, added to `bounds_reviewed.txt` as `[gcc14]`; the `D_80153E8F` entry, which nothing can find now, removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 1 decoupled |
| `assets_roster` (16 slots declared) | passes; with `PC_LINK_OWNER` reading `flags`, 20 checks fail |

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) has not been run; it covers the 29 changed files
under `src/` (`git diff --name-only 0aaa349 -- src`).

**The test.** `assets_roster` builds func_8039156C, func_800EC190, CalcPolePositions, func_800EC914 and
stat_lap_split with `PC_MAX_LINKS` 16, every slot a sentinel byte whose flags have `LINK_ACTIVE`. Each must write
and read the fields at their N64 offsets in the slots it walks and leave the rest alone: the 13 cars named, a race's
6 slots reset, a race of 2 players and 2 drones laid out (places, owners, flags, track), 4 active cars counted into
`D_80152744` with the human and drone lists, and the owner read where `D_80153E8F` reached it.

## 3. Next, toward 8 cars

A quick inventory: objects referencing each name and its aliases (nm, context copies included), sizes from
`data_symbols.txt`. None is done.

| Rank | Data | Size (N64, host) | Objects | Notes |
|---|---|---|---|---|
| 1 | the model records `D_8014A250` (`tModelData[6]`) | 6 x 0x808 = 12,336 B, the same | 100 | arena BSS; 10 names inside it, about 120 views, m2c strides `i * 0x808`; its five pointer fields are u32 window addresses (CARS.md 4), which can stay so outside the window if nothing holds the record's own address: to inventory |
| 2 | the car records `player_array` (`D_80152818`) | 6 x 0x3B8 = 5,712 B, 6 x 0x468 = 6,768 B | 134 | own storage already (host layout, `CAR_OFS`); 11 names inside it; `extents.txt` bounds it to 6 cars because `D_80153E68` follows |
| 3 | the input records `D_8014A118` (`input_rec0`) | 6 x 76 = 304 B, 6 x 80 = 320 B | 73 | own storage; 5 names inside; `INPUT_SIZE` strides (POINTERS.md 10.3); per player, so 8 only if 8 humans |
| 4 | the car parts `D_80139320` (`tCarParts`) | 3,328 B by the gap, 0x40 a car | 20 | arena BSS; the extent is the gap, so the real count first |
| 5 | the race's per-car words: `D_801526E0`, `D_801526F8` (f32), `D_801526A8` (12-byte V3), `D_80152218` (f32[3]) | 16-72 B each, by the gap | 3-5 each | arena BSS; declared whole for OPTIMIZED.md 2.7; sizes by the gap, so the true counts first |
| 6 | the maxpath weights `D_80150F88` (`WeightRecord[12]`), stats `D_80151578` (`StatsC[8]`) | 1,152 B, 96 B | 9, 4 | arena BSS, already 12 and 8 long |
| 7 | the 13-slot arrays beside the roster: `D_80142B08` (s16[13], each slot's engine wave), `D_8013FE90` (u8, 52 B by the gap) | 26 B, 52 B | 4, 3 | arena BSS; walked with the roster's 13 |
| 8 | the human and drone lists `D_80143A40`, `D_801527D8`, `D_80152808` (s16) | 12-16 B by the gap | 1-3 | arena BSS; small, sized by the gap |
| 9 | per car, initialized: `D_80110680` (u8[6]), the car palette descriptors `D_8013FEE0` (`Slot24`, a u32 window address inside) | 6 B, 24 B | 6, 3 | `D_80110680` is ROM data, which decoupled.txt doesn't take yet |

Beyond data, the loops: the 6-car bounds in the code (`i < 6`, `MAX_LINKS` 6 in about a dozen files, `D_801543CA`)
are what activates cars, and the per-window and per-player arrays (cameras, HUD) follow the players, not the cars.
