# Decoupling game data from its N64 address

Most of the game's data sits at its N64 address in the RDRAM window ([DATA.md](DATA.md) 3), so the decompiled C runs
unchanged. That costs the port: an array can't grow (its neighbours follow it), address arithmetic between symbols
is baked in, AddressSanitizer can't run on x86-64, and macOS arm64 can't map the address. Decoupling moves one item
to storage the PC owns outright, with one PC declaration and no N64 address. The first item is the race roster
(`D_80153E88`, arcade `gLink`), the second the model records (`D_8014A250`, MAT3 `model[]`): steps toward 12 cars a
race (`PC_MAX_LINKS`, 8 until 2026-10-29). 2026-10-28.

| Piece | Where |
|---|---|
| The PC declarations and accessors | `port/include/pc_game_types.h` (included by `pc_prelude.h`) |
| The rules: PC symbol, retired N64 symbol and size | `port/data/decoupled.txt` |
| Storage, checks, report | `tools/port/gen_data.py` (`load_decoupled`, `decoupled_type`, `emit`, `--check`) |
| Tests | `port/assets/tests/test_roster.c` (`assets_roster`), `test_models.c` (`assets_models`) |

## 1. The recipe

1. **Inventory.** Every object that references the symbol or any name inside its N64 extent (nm over
   `game_objects.txt`; the image symbol tables name the addresses the original code reached, `asm/us/*/symbols.json`);
   each declaration's type (the objects' DWARF, as gen_data reads it); each access's offset, width, signedness, and
   the indices it walks; arithmetic from a neighbour into it or from it out (`bounds_check`, `offset_check`,
   `layout_check`); and whatever holds its N64 address: ROM data words, `PC_N64W` stores, absolute addresses in C.
   That includes the address one past its end, which is the next symbol's: a loop may name that symbol for the
   array's end (the model records' `other < D_8014D280`, section 3), so the code references to the end address
   (`lui`/`lo` pairs in the images) and the C that names the next symbol are part of the inventory.
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
   the accessor of the field it named (`D_80153E8F[slot * 8]` is `PC_LINK_OWNER(slot)`). Where the record keeps its
   N64 layout, code that takes a record's address may keep reading it through its own view (`PC_MODEL(i)`, a
   `void *`); `view_audit.py` checks every such view against the PC declaration.
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
race may hold on the PC, then; 12 since 2026-10-29); with 8 or 12 it is 13, 104 bytes, and the game still activates 6.

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

## 3. The model records

**What it is.** One 0x808-byte record per car, 6 of them, at 0x8014A250-0x8014D280 in the game's BSS: MAT3's `model[]`
(`tModelData`), the car's physics state, its controls and its race state. It keeps its N64 layout on the PC: its five
pointer fields are u32 window addresses whose pointees stay in the window ([CARS.md](CARS.md) 4), so the host size is
0x808 too and code that steps through it by 0x808 still lands on the fields. `struct pc_model` names the 45 fields the
game reaches by index or through a name inside the array (resurrect.c's `tModelData` gives MAT3 names for some);
everything else is reached through a record pointer in each file's own view:

| Offset | Fields (accessor suffix) | Reached by index from |
|---|---|---|
| +0x000, +0x004 | `CAR`, `tune` (u32 window addresses) | update_game_data (through its view) |
| +0x008..+0x00F | `BODY` (u8), `INPUT_FLAGS`, `AUTOTRANS`, `B00F` | 21 uses: the HUD, entity spawns, LoadCars, music_tempo_set, save_load_data (through `D_8014A258`) |
| +0x220, +0x22C | `RWV`, `RWR` (f32[3]: velocity, position) | race_tail, main_menu_render; the state log (port/main.c) |
| +0x3F0..+0x75C | `SPEED`, `SCALE`, `F544`, `VISCODE[4]`, `CRASHFLAG`, `TOPSCRAPE`, `RESURRECT_POS`, `MOVING_STATE`, `ITERATION`, `MODEL_TIME`, `DT`, `MODELRUN`, `STEER`, `BRAKE`, `GAS`, `GEAR`, `FLAG_B`, `FLAG_A`, `SUSPENSION[4]` | the ghost playback (func_800E5D64), the model clock (func_800E762C), resurrect, the HUD, m2c strides on `D_8014A640`, `D_8014A890`, `D_8014A914`, `D_8014A964`, `D_8014A96C` |
| +0x794 | `RECKON_RWR` (f32[3]) | maxpath |
| +0x7C4..+0x7F0 | `OB_RES_PT`, `SLOT`, `ACTIVE`, `WE_CONTROL`, `DRONE_TYPE`, `COLOR0..2`, `S7D0`, `FLAGS`, `STUCK_TIME`, `CAN_CHEAT`, `HIDE_CAR`, `LAST_CP`, `DRONE_TARGET`, `LAP`, `COLLIDABLE`, `DRONE_SCALE`, `TIME_BOOST` | 55 uses of `SLOT` alone; the race end, the drones, the HUD, through `D_8014AA14`, `D_8014AA16`, `D_8014AA32`, `D_8014AA3A` |

**Inventory.** 112 objects reference the names (nm): 108 game files under `src/`, context copies among them (three each
of func_800C54F0, camera_scene_manager, render_large_objects and path_D169C, two each of race_tail, car_setup_confirm,
func_800F8EC8, func_800D1AB0, func_800D5828 and players_frame_update), and four under `nonmatching/`; and the state log
in `port/main.c`. They declare it through 122 views, all 0x808 bytes but m2c's 2,026-byte `D_8014A250_Record`, whose
code steps by 0x808 explicitly. Only two offsets have readers that disagree on signedness (+0x7CC, +0x7D4), and those
are stores and bit operations, so one type reads the same values. The image symbol tables name 24 addresses inside it,
12 of which the C used: `D_8014A250_alias` (a second symbol for the base), and [0]'s fields `D_8014A258` (+0x8),
`D_8014A640` (+0x3F0), `D_8014A890` (+0x640), `D_8014A914` (+0x6C4), `D_8014A964` (+0x714), `D_8014A96C` (+0x71C),
`D_8014AA14` (+0x7C4), `D_8014AA16` (+0x7C6), `D_8014AA32` (+0x7E2), `D_8014AA3A` (+0x7EA), read with a stride of 0x808
or as record 0's field.

**What holds a record's address.** Nothing in memory. A scan of the four images inflated from the retail cartridge found
no data word in 0x8014A250-0x8014D280, and 177 code references (`lui 0x8015` with an offset in range: 162 in the game
image in 99 functions, 15 in image B in 11), every function one whose C is among the 112 objects. No `PC_N64W` or
integer store takes a record's address: layout_check's analysis of every live game file finds no pointer store whose
value reaches the storage (it finds the car record's `&D_8014A118[i]`, the check's control), and a `RUSH_CHECK_CASTS`
build ran the race and the demo without a stop. The 342 call arguments that pass a record's address (to about 150
parameters: the physics, the cameras, the sounds' positions) use it and don't keep it; `camera_target_track` copies the
position. `Gfx` words hold host pointers anyway (`w1` is pointer-sized).

**What it reaches past its end.** `menu_audio_settings`, the car-to-car collision, walks the cars after one up to
`D_8014D280`, the next symbol (the level-object nodes), which is the array's end on the N64. Its storage moved, that
comparison was false from the first car on, and no two cars collided: the first synth_compare run differed on 12 of 19
paths, cars 0 and 5 overlapping at the start. It is `PC_MODELS_END` now, the end of the N64's 6. The retail code has
three references to 0x8014D280: that one, camera_lerp_position's node pool (the neighbour's own) and func_800E762C's `i
< 6`, which IDO strength-reduced to an end pointer. Nothing reaches the old range any more: filled with 0x5A after the
loader ran, it left the race path's trace the same, and nothing wrote to it.

`layout_check.py --storage` before: 4,243 accesses on `D_8014A250` and 17 on 9 of the names inside, 3,434 of them
through the parameters above; after: 4,271 on `pc_models`, none off the reference (`struct pc_model[8]`).

**The size.** Every walk stops at 6: loops to 6 or `MAX_LINKS` (func_800E762C, music_control, update_game_data,
func_800DE860, func_800EB90C, battle_mode_setup, menu_controller_remap, func_800CFCA8, players_race_update,
func_800E56F8's group, LoadCars' `i < 6` guard), to the cars in the race (`D_801543CA`, `D_80152744`), the players or
the windows, and the collision's end pointer. So the storage is `PC_MODEL_SLOTS` = the larger of 6
(`PC_N64_MODEL_SLOTS`) and `PC_MAX_LINKS` (8): 8 records, 16,448 bytes, and the game still activates 6. With
`PC_MAX_LINKS` 12 (2026-10-29) it is 12 records, 24,672 bytes.

**The change.** `pc_game_types.h`: `struct pc_model`, `pc_models[PC_MODEL_SLOTS]`, the accessors `PC_MODEL_<FIELD>(i)`
for the indexed fields, and three `void *` ones for code that keeps a record pointer in its own view: `PC_MODEL(i)` (a
record), `PC_MODELS` (the array, for m2c's pointer walks of 0x808) and `PC_MODELS_END`. The 112 files use them, about
340 of the uses rewritten by a script from each file's DWARF (the view's field to its N64 offset to the accessor) and
about 60 by hand (the m2c strides and the aliases); each defines the N64 side with its own view (`#define
PC_MODEL_SLOT(i) D_8014A250[i].half1990`, `#define PC_MODEL_SLOT(i) *(s16 *)((u8 *)&D_8014AA16 + i * 0x808)`, `#define
PC_MODEL(i) (D_8014A250_Record *) ((i * 0x808) + ((u8 *) (&D_8014A250)))`). func_800D1248's debug guard reads
`D_8014A96C` as a scalar, so on the PC it is `#define D_8014A96C PC_MODEL_MODELRUN(0)`. `decoupled.txt`: `pc_models
D_8014A250 0x3030` and the 11 aliases. The state log reads `pc_models[0]`; ten asset tests and test_overlay_data define
`pc_models` where they defined `D_8014A250` (test_cars and test_targets had it as an absolute symbol in their window).
gen_data needed nothing new.

**What the tables say.** `data_symbols.txt` loses the 11 rows of `D_8014A250` and the names inside; `data.c` loses their
12 `.set` lines and 11 report names, and gains `pc_models` (16,448 bytes, `.bss`); `data_layout.txt` lists it under
"Decoupled" with the 24 retired names. The loader's symbol table is the same (419 entries) and no ROM word pointed into
the range, so its summary line is too. The synthetic cartridge is the same file. The arena's 0x8014A250-0x8014D280 has
no symbol now: 12 KB nothing uses.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths (before the end-pointer fix: 7 of 19) |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 241 of 241 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 11 lines, `data.c` 28 |
| the synthetic cartridge, before against after | the same file (Pi and build host) |
| `n64_neutral.py --base 09da2f0` | passes (1,010 files: 92 code-exact, 918 shifted); 108 changed game files under `src/` |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after (endian_check's 2 open value conflicts in 1 storage are the same before and after); the reports differ only by line numbers and 11 storages fewer (`D_8014A250` and 11 names inside it are one root, `pc_models`); endian_check counts 2,839 puns, 491 more (each file's view and `struct pc_model` on one storage) |
| `layout_check.py --storage pc_models` | 4,271 accesses, none off the reference (`struct pc_model[8]`) |
| `view_audit.py --symbols pc_models --size 0x808 --ref world_physics_tick.c` | 165 views (`struct pc_model` among them), 0 disagree (before, on `D_8014A250`: 122, 0) |
| `bounds_check`, build host (GCC 14) | 3 new, none `clamped` or `two offsets`, added to `bounds_reviewed.txt` as `[gcc14]` (two `view` on `pc_models`, one `tmr` on `pc_links` in world_physics_tick); 19 entries on the retired names removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): race, demo | no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 2 decoupled |
| `assets_models` (16 records declared) | passes; with `PC_MODEL_MODELRUN` reading `ob_res_pt`, 2 checks fail; with `PC_MODELS_END` at the storage's 16, 3; with a stride of 0x800, 22 |

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) has not been run; it covers the 109 changed files under
`src/` (`git diff --name-only db0fb90 -- src`: the 108 that reached the names and menu_audio_settings).

**The test.** `assets_models` builds func_800E762C, func_800D1248, func_800D0380, func_800D18C8 (path_D169C),
func_800D60AC, steering_apply and menu_audio_settings with `PC_MAX_LINKS` 16, every record a sentinel. Each must write
and read the fields at their N64 offsets, step 0x808 and stop at the N64's 6: the 6 records' model clock; a resurrect's
end through a record pointer and another record's modelrun through what was `D_8014A96C`, in two spellings; record 0's
obResPt through `D_8014AA14`; the pointer walk from the array's start; collidable through `D_8014AA3A`; the collision's
walk from record 3 visiting 4 and 5 and nothing after. It checks `struct pc_model`'s offsets too.

## 4. Next, toward 12 cars

A quick inventory: objects referencing each name and its aliases (nm, context copies included), sizes from
`data_symbols.txt`. The model records, which were first here, are done (section 3): 112 objects, not 100 (the names
inside it count), and 24 names, not 10. Each item's own inventory should look for an end pointer too.

| Rank | Data | Size (N64, host) | Objects | Notes |
|---|---|---|---|---|
| 1 | the car records `player_array` (`D_80152818`) | 6 x 0x3B8 = 5,712 B, 6 x 0x468 = 6,768 B | 134 | own storage already (host layout, `CAR_OFS`), so no N64 address to lose, but 11 names inside it and about 140 views (CARS.md 2.2); `extents.txt` bounds it to 6 cars because `D_80153E68` follows (no C names that as an end); holds `&D_8014A118[i]` at +0x380 |
| 2 | the input records `D_8014A118` (`input_rec0`) | 6 x 76 = 304 B, 6 x 80 = 320 B | 73 | own storage; 5 names inside; `INPUT_SIZE` strides (POINTERS.md 10.3); the car record holds its address (a host pointer already); per player, so 8 only if 8 humans |
| 3 | the car parts `D_80139320` (`tCarParts`) | 3,328 B by the gap, 0x40 a car | 20 | arena BSS; the extent is the gap, so the real count first |
| 4 | the race's per-car words: `D_801526E0`, `D_801526F8` (f32), `D_801526A8` (12-byte V3), `D_80152218` (f32[3]) | 16-72 B each, by the gap | 3-5 each | arena BSS; declared whole for OPTIMIZED.md 2.7; sizes by the gap, so the true counts first |
| 5 | the maxpath weights `D_80150F88` (`WeightRecord[12]`), stats `D_80151578` (`StatsC[8]`) | 1,152 B, 96 B | 9, 4 | arena BSS, already 12 and 8 long |
| 6 | the 13-slot arrays beside the roster: `D_80142B08` (s16[13], each slot's engine wave), `D_8013FE90` (u8, 52 B by the gap) | 26 B, 52 B | 4, 3 | arena BSS; walked with the roster's 13 |
| 7 | the human and drone lists `D_80143A40`, `D_801527D8`, `D_80152808` (s16) | 12-16 B by the gap | 1-3 | arena BSS; small, sized by the gap |
| 8 | per car, initialized: `D_80110680` (u8[6]), the car palette descriptors `D_8013FEE0` (`Slot24`, a u32 window address inside) | 6 B, 24 B | 6, 3 | `D_80110680` is ROM data, which decoupled.txt doesn't take yet |

Beyond data, the loops: the 6-car bounds in the code (`i < 6`, `MAX_LINKS` 6 in about a dozen files, `D_801543CA`)
are what activates cars, and the per-window and per-player arrays (cameras, HUD) follow the players, not the cars.
For the model records they are listed in section 3 ("The size"), with `PC_MODELS_END`, the collision's end, which is
the N64's 6 too. Two other things there stay as the N64 has them: path_D169C raises only record 0's obResPt (through
`D_8014AA14`), and car_setup_confirm's human count compares a record's address with record 0's.
