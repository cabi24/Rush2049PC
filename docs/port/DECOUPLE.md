# Decoupling game data from its N64 address

Most of the game's data sits at its N64 address in the RDRAM window ([DATA.md](DATA.md) 3), so the decompiled C runs
unchanged. That costs the port: an array can't grow (its neighbours follow it), address arithmetic between symbols
is baked in, AddressSanitizer can't run on x86-64, and macOS arm64 can't map the address. Decoupling moves one item
to storage the PC owns outright, with one PC declaration and no N64 address. The first item is the race roster
(`D_80153E88`, arcade `gLink`), the second the model records (`D_8014A250`, MAT3 `model[]`), the third the car records
(`D_80152818`, MAT3 `gGameCar`), which had host storage of their own already: steps toward 12 cars a race
(`PC_MAX_LINKS`, 8 until 2026-10-29). 2026-10-28, the car records 2026-10-29.

| Piece | Where |
|---|---|
| The PC declarations and accessors | `port/include/pc_game_types.h` (included by `pc_prelude.h`) |
| The rules: PC symbol, retired N64 symbol and size | `port/data/decoupled.txt` |
| Storage, checks, report | `tools/port/gen_data.py` (`load_decoupled`, `decoupled_type`, `emit`, `--check`) |
| Tests | `port/assets/tests/test_roster.c` (`assets_roster`), `test_models.c` (`assets_models`), `test_carrecs.c` (`assets_carrecs`) |

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

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) had not been run; it covers the 29 changed files
under `src/` (`git diff --name-only 0aaa349 -- src`). Run with section 4's change (2026-10-29), it finds every object
the same as 09da2f0's but in its debug sections.

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

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) had not been run; it covers the 109 changed files under
`src/` (`git diff --name-only db0fb90 -- src`: the 108 that reached the names and menu_audio_settings). Run with section
4's change (2026-10-29), it finds every object the same as 09da2f0's but in its debug sections.

**The test.** `assets_models` builds func_800E762C, func_800D1248, func_800D0380, func_800D18C8 (path_D169C),
func_800D60AC, steering_apply and menu_audio_settings with `PC_MAX_LINKS` 16, every record a sentinel. Each must write
and read the fields at their N64 offsets, step 0x808 and stop at the N64's 6: the 6 records' model clock; a resurrect's
end through a record pointer and another record's modelrun through what was `D_8014A96C`, in two spellings; record 0's
obResPt through `D_8014AA14`; the pointer walk from the array's start; collidable through `D_8014AA3A`; the collision's
walk from record 3 visiting 4 and 5 and nothing after. It checks `struct pc_model`'s offsets too.

## 4. The car records

**What it is.** One 0x3B8-byte record per car, 6 of them, at 0x80152818-0x80153E68 in the game's BSS: MAT3's
`gGameCar` (`tCarData`, `player_array`; `include/game_types.h`'s `GameCar`), the car as the race shows and follows it:
its position and orientations, its 21 visuals, its place, finish and distance, its input record, its battle state (the
physics is the model record's, section 3). Unlike the roster and the model records it was own storage already: its
visuals and its input pointer hold host pointers, so a record is 0x468 bytes on the PC ([CARS.md](CARS.md) 2) and it
had no window address to lose. What decoupling changes is the rest: it leaves the loader's table of own storage, the
names inside its N64 extent are retired, it has one declaration, and its size is the PC's. `struct pc_car` names the
36 fields the game reaches by index or through a name inside the array, with N64-sized padding between pointer-sized
members as every view has, so each field is at `CAR_OFS` of its N64 offset; everything else is reached through a
record pointer in each file's own view:

| N64 offset (PC) | Fields (accessor suffix) | Reached by index from |
|---|---|---|
| +0x008..+0x0A4 | `POS` (f32[3]; `POS_X`..`POS_Z` for a `Vec3` view), `UVS`, `UVS50` (f32[3][3]), `WHEEL_POS` (f32[4][3]) | 45 uses of `POS`: the cameras, the effects (blast, entity_update_callback), path_D169C, the drones' order (render_large_objects); the skid marks; race_init_helper through `D_80152BD8` and `D_80152C20` |
| +0x0E8..+0x108 | `FLAGS`, `PLACE`, `FINISHED`, `TIME`, `NODE`, `MPH`, `DISTANCE`, `F108` | the race order and end (car_setup_confirm, func_800D1AB0, FindWinner), the HUD and the results (results_table through `D_80152906`..`D_80152908`), the countdown, the brake lights through `D_80152900`, the cameras; m2c strides on `D_80152907` |
| +0x110 | `VISUALS` (struct pc_visual[21]) | music_control, startcar, func_8038C910 (the weapon's visual) |
| +0x314..+0x364 (PC +0x3BC..+0x40C) | `NAV`, `FWD_PROGRESS`, `IR_COLOR`, `PREV_IR_COLOR`, `WEIGHT_INDEX`, `B358`, `STATE`, `DISABLED`, `SLOT`, `VIEW`, `SHEEN_ALPHA`, `HEADING` | maxpath, the stuck check (menu_controller_remap), render_large_objects, the deaths race (the countdown), m2c strides on `D_80152B70` and `D_80152B74`, GameCar's `pad0EC` windows |
| +0x380 (PC +0x428) | `INPUT` (void *; `INPUT_AS(T, i)` reads through it in the file's view) | model_iteration, the countdown, display_list_flush, buffer_swap |
| +0x384..+0x3B4 (PC +0x430..+0x460) | `KIND`, `LIVES`, `HEALTH`, `BATTLE_FLAGS`, `B3A0`, `B3A2`, `POINTS`, `RELOADING_TIME`, `F3B0`, `F3B4` | battle mode: image B's status and points (runtime_b_status_scene, func_803914B4, func_803936A8, func_80393518), the battle reset func_800D1004, FindWinner, results_table through `D_80152BBB` |

**Inventory.** 143 objects reference the names (nm): 138 game files under `src/`, context copies among them (three each
of camera_scene_manager, render_large_objects, path_D169C and func_800C54F0, two each of car_setup_confirm,
func_800D1AB0, func_800F8EC8, race_tail, the countdown, gameover_validOption, the audio_frame groups, func_8038C910,
func_8038DDDC and image B's radius effect), and five under `nonmatching/`. They declare it through 133 views, all 0x468
bytes on the PC (view_audit; 137 with the structs of N64 size 0x3B8 reached through pointers). Only three offsets have
readers that disagree on signedness (+0xE8 and +0x38C int and unsigned, +0x358 signed and unsigned char), and those are
stores, bit tests and masks, so one type reads the same values; at +0x008 one view (entity_collision_detect) has a
`Vec3` where the others have f32[3]. The image symbol tables name 141 addresses inside it (120 of them `g` names the
symbol file gives arcade globals, `gCarMass` at car 0's +0x7C, `gSkidPool` at car 3's +0xC0), 11 of which the C used,
and the C one more by its name's address, `D_801528F0`: `player_array` (a second symbol for the base), [0]'s fields
`D_801528F0` (seen from +0xD8), `D_80152900` (+0xE8), `D_80152906` (+0xEE), `D_80152907` (+0xEF), `D_80152908` (+0xF0),
`D_80152B70` (+0x358), `D_80152B74` (+0x35C) and `D_80152BBB` (+0x3A3), and car 1's `D_80152BD0` (+0), `D_80152BD8`
(+0x8) and `D_80152C20` (+0x50), read with a stride of 0x3B8 (`CAR_OFS(0x3B8)` on the PC) or as an array of cars seen
from that offset.

**What holds a record's address.** Nothing in memory. A scan of the four images inflated from the retail cartridge found
no data word in 0x80152818-0x80153E68, and 231 code references (`lui 0x8015` with an offset in range: 205 in the game
image in 133 functions, 26 in image B in 17), every function one whose C is among the 143 objects. layout_check's
analysis of every live game file finds no pointer store whose value reaches the storage; the 123 call arguments that
pass a record's or a field's address (to 57 parameters: the cameras, the visuals, the shots, the sounds' positions) use
it and don't keep it. And a host address couldn't be kept in a u32 anyway: the storage was above 4 GB before too
(`PC_N64W` would stop a `RUSH_CHECK_CASTS` build). The record holds an address itself, its input record's
(`&D_8014A118[i]` at +0x380, world_physics_tick), a host pointer already, which the input records' decoupling has to
keep (section 5).

**What it reaches past its end.** Nothing. The next symbol, `D_80153E68`, is a message queue (osPiStartDma's done queue
in func_8008ABE4, task_complete_signal's); the three objects whose C names it are those two functions (one twice, in its
context copy), and the retail code's two references to 0x80153E68 are theirs. No C names the array plus 6 or
`&D_80152818[6]`; the walks of the array end at the cars in the race (`D_80152818 + D_801543CA`, image B's homing and
radius effects, the shots), which is `PC_CARS_END(n)` now. The array had storage of its own before, so a walk from a
neighbour (`D_80152808`, `D_801527E8`) into it would have missed it before as after: bounds_check, offset_check and
layout_check list none. The window's 0x80152818-0x80153E68 was unused before (own storage leaves the window bytes alone)
and is still: filled with 0x5A after the loader ran (a throwaway build on the build host), it left 7 synthetic paths'
traces the same (race, race4p, battle2p, demo, stunt, pause, circuit), and none of its 5,712 bytes changed by the end of
any.

`layout_check.py --storage` before: 1,592 accesses on `D_80152818` (the names inside are on it, at their offsets), 69 of
them with no N64 position; after: 1,595 on `pc_cars`, 62 with none, and none of either off the reference (gen_data's
`GameCar[8]` before, `struct pc_car[12]` after).

**The size.** Every walk stops at 6: loops to 6 (battle_mode_setup, func_800CFCA8, func_800DE860, func_800EB90C,
model_iteration, menu_controller_remap, players_race_update, track_texture_load, update_game_data), to the cars in the
race (`D_801543CA`: world_physics_tick's init_cars, the shots and image B's walks; `D_80152744`: func_800FBE60,
race_tail, world_gravity_apply, the race order), the players (`D_8014A108`, `active_player_count`) or the windows. So
the storage is `PC_CAR_SLOTS` = the larger of 6 (`PC_N64_CAR_SLOTS`) and `PC_MAX_LINKS` (12): 12 records, 13,536
bytes, and the game still activates 6.

**The change.** `pc_game_types.h`: `struct pc_car` (and `struct pc_visual`, moved there from `pc_prelude.h` so that
port code including `pc_game_types.h` alone sees it), `pc_cars[PC_CAR_SLOTS]`, the accessors `PC_CAR_<FIELD>(i)` for
the indexed fields, `PC_CAR_INPUT_AS(T, i)` for the input pointer read through a view, and three `void *` ones for code
that keeps a record pointer in its own view: `PC_CAR(i)` (a record), `PC_CARS` (the array, for the pointer walks and
m2c's byte strides) and `PC_CARS_END(n)` (the end of the first n). The 143 files use them: about 280 field uses, 144
record addresses, 23 of the array and 4 ends rewritten by a script from each file's DWARF (the view's member path to
its N64 offset to the accessor: `D_80152818[i].battle.type` is `PC_CAR_KIND(i)`, `D_80152818[i].position.x`
`PC_CAR_POS_X(i)`), and 18 by hand: the m2c strides on the names inside (`*((s8 *) &D_80152907 + (i *
CAR_OFS(0x3B8)))` is `PC_CAR_FINISHED(i)`), GameCar's `pad0EC` windows (`PAD0EC(player_array[i].pad0EC, 0x26D)` is
`PC_CAR_STATE(i)`), the three reads through the input pointer, and a second spelling of a record's address in
func_800E56F8's group. Each file defines the N64 side with its own view (`#define PC_CAR_FINISHED(i)
D_80152818[i].place_locked`, `#define PC_CAR_FINISHED(i) *((&D_80152907) + (i * CAR_OFS(0x3B8)))`, `#define
PC_CARS_END(n) D_80152818 + n`). race_init_helper steps from car 1's names to car j + 1 by `CAR_OFS(0x3B8)`, so on the
PC they are car 1's record, position and +0x50 (`#define D_80152BD8 (*(s32 *) PC_CAR_POS(1))`). func_800FBE60 walks
the array as bytes (`PC_CARS + i * CAR_OFS(952) + 264`: GNU C's arithmetic on a `void *`). path_D169C's three copies
defined the array (a tentative definition, `Car D_80152818[6];`); on the PC they don't. `decoupled.txt`: `pc_cars
D_80152818 0x1650`, `player_array` and the 11 names; `extents.txt` loses the array's size. Eleven asset tests and
test_overlay_data define `pc_cars` where they defined `D_80152818` (test_cars and test_targets gave names inside it as
aliases, test_hudblit defined `D_80152907` alone). gen_data needed nothing new.

**What the tables say.** `data_symbols.txt` loses the 12 rows of `D_80152818` and the names inside; `data.c` loses its
own storage (6,768 bytes), its 12 `.set` lines, its 12 report names, its loader entry (419 to 418) and its 66 field
runs (7,803 to 7,737), and gains `pc_cars` (13,536 bytes, `.bss`); the later own storage's C names (`pc_own_N`) and
loader entries' run indices are renumbered, which is most of data.c's 521 changed lines. `data_layout.txt` lists it
under "Decoupled" with the 148 retired names (the image tables' 141, `D_80152818`, `D_801528F0` and 5 the symbol file
alone names). No ROM word pointed into the range, so the loader's summary line differs only by its table's size, which
`synth_compare.py` leaves out of the trace now (the table is data.c's, which is compared as a file). The synthetic
cartridge is the same file.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before (main, 25f43c8) against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 242 of 242 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 12 lines, `data.c` 521 (319 with the own storage's C names normalized) |
| the synthetic cartridge, before against after | the same file (Pi and build host) |
| `n64_neutral.py --base 09da2f0` | passes (1,010 files: 92 code-exact, 918 shifted); 138 changed game files under `src/` |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after (endian_check's 2 open value conflicts in 1 storage, func_800E9234's light-flash bytes, CARS.md 6, are the same, on `pc_cars` now); offset_check lists 332 untyped raw accesses, 9 more, all func_8010DF90's on its level-object node (whose lines lost `CAR_OFS` to the accessor; the node keeps its N64 layout, LAYOUT.md 9.4); layout_check's reviewed race_init_helper read of the node handle's low half is no finding now (it reads through `struct pc_car`'s node); endian_check counts 3,486 puns, 647 more (each file's view and `struct pc_car` on one storage) |
| `layout_check.py --storage pc_cars` | 1,595 accesses, none off the reference (`struct pc_car[12]`) |
| `view_audit.py --symbols pc_cars --size 952 --ref func_8038C910.c` | 222 views (`struct pc_car` in the 143 objects, and the views reached through record pointers), 0 disagree (before, on `D_80152818,player_array`: 137, 0) |
| `bounds_check`, build host (GCC 14) | 8 new, none `clamped` or `two offsets`, all `view` on `pc_cars`, added to `bounds_reviewed.txt` as `[gcc14]`; 8 entries on the retired names and players_race_update's `pad0EC` warning (gone with it) removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 3 decoupled |
| `assets_carrecs` (16 records declared) | passes; with `PC_CARS_END(n)` at the storage's 16, 4 checks fail; with +0x358 read as the state, 3; with a stride of 0x3B8, 8; with the finish read as the place, 1; with race_init_helper's `D_80152BD8` at car 0, 3; with the health a byte early, 2 |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 277adc4: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_carrecs` builds func_800FBE60, menu_controller_remap with func_800CF604, image B's func_8038DDDC,
func_800D1004, race_init_helper and gameover_validOption with `PC_MAX_LINKS` 16, every record a sentinel. Each must
write and read the fields at their PC offsets, step 0x468 and stop where the N64 does: the race start's copy of
D_80152744 cars at m2c's byte strides from the array; 6 cars' progress cleared by index and car 9's (a slot past the
N64's 6) through its input record's node, a record pointer in the file's view, with +0x358 read through what was
`D_80152B70`; the homing walk over the race's `D_801543CA` cars to their end, the nearer cars after them ignored; car
10's battle reset through its model's slot; cars 1 to 3 to the cameras through what were car 1's names; a finish through
what was `D_80152907`. It checks `struct pc_car`'s offsets against `CAR_OFS` too.

## 5. Next, toward 12 cars

A quick inventory: objects referencing each name and its aliases (nm, context copies included), sizes from
`data_symbols.txt`. The model records, which were first here, are done (section 3): 112 objects, not 100 (the names
inside it count), and 24 names, not 10. The car records, first after them, are done too (section 4): 143 objects, not
134 (the names inside it count), and 148 names, 12 of them in the C. Each item's own inventory should look for an
end pointer too.

| Rank | Data | Size (N64, host) | Objects | Notes |
|---|---|---|---|---|
| 1 | the input records `D_8014A118` (`input_rec0`) | 4 x 76 = 304 B, 4 x 80 = 320 B | 73 | own storage; 5 names inside; `INPUT_SIZE` strides (POINTERS.md 10.3); the car record holds its address (`pc_cars[i].input`, a host pointer already): world_physics_tick gives a human's car `&D_8014A118[i]` by the car's slot i, and the humans have the first slots (0-3), so 4 records stay enough while they do |
| 2 | the car parts `D_80139320` (`tCarParts`) | 3,328 B by the gap, 0x40 a car | 20 | arena BSS; the extent is the gap, so the real count first |
| 3 | the race's per-car words: `D_801526E0`, `D_801526F8` (f32), `D_801526A8` (12-byte V3), `D_80152218` (f32[3]) | 16-72 B each, by the gap | 3-5 each | arena BSS; declared whole for OPTIMIZED.md 2.7; sizes by the gap, so the true counts first (`D_80152218` is 6 cars' positions, func_800FBE60 copies them from the car records) |
| 4 | the maxpath weights `D_80150F88` (`WeightRecord[12]`), stats `D_80151578` (`StatsC[8]`) | 1,152 B, 96 B | 9, 4 | arena BSS, already 12 and 8 long |
| 5 | the 13-slot arrays beside the roster: `D_80142B08` (s16[13], each slot's engine wave), `D_8013FE90` (u8, 52 B by the gap) | 26 B, 52 B | 4, 3 | arena BSS; walked with the roster's 13 |
| 6 | the human and drone lists `D_80143A40`, `D_801527D8`, `D_80152808` (s16) | 12-16 B by the gap | 1-3 | arena BSS; small, sized by the gap |
| 7 | per car, initialized: `D_80110680` (u8[6]), the car palette descriptors `D_8013FEE0` (`Slot24`, a u32 window address inside) | 6 B, 24 B | 6, 3 | `D_80110680` is ROM data, which decoupled.txt doesn't take yet |

Beyond data, the loops: the 6-car bounds in the code (`i < 6`, `MAX_LINKS` 6 in about a dozen files, `D_801543CA`)
are what activates cars, and the per-window and per-player arrays (cameras, HUD) follow the players, not the cars.
For the model records they are listed in section 3 ("The size"), with `PC_MODELS_END`, the collision's end, which is
the N64's 6 too; for the car records in section 4 ("The size"). Two other things there stay as the N64 has them:
path_D169C raises only record 0's obResPt (through `D_8014AA14`), and car_setup_confirm's human count compares a
record's address with record 0's. And the code that orders the cars keeps locals sized for the N64's 6, indexed by the
cars in the race: car_setup_confirm's `place[7]` and `scores[6]`, render_large_objects' `humans`, `drones`,
`total_drones` (`[MAX_LINKS]`) and `car_distance[MAX_LINKS][MAX_LINKS]`.

## 6. The effects' per-car arrays

**What they are.** Seven arrays of the effects, one entry per car slot, 6 on the N64 (game BSS), indexed by the car's
slot ([MORECARS.md](MORECARS.md) 2.3, item 1d of 10.2). None has pointers but the skid state, which was own storage
already for its segment pointer. 2026-10-29, on main 597098e.

| N64 | PC | N64 extent | PC storage | What | Read and written by (objects) |
|---|---|---|---|---|---|
| `D_801392D8` | `pc_tire_fx` | u32[6], 24 B | `unsigned int[12]`, 48 B | each car's tire-effect latches, a bit per tire and effect (0x1-0x8 and 0x100-0x800 the tire puffs, 0x10-0x80 and 0x80000 the smoke, 0x1000-0x8000 the trails) | entity_spawn_init, func_8008E408, hud_render, func_80092484, func_800CB718 (three copies), physics_collision_test (the reset) |
| `D_8013A020` | `pc_smoke_shade` | u8[6], 8 B by the gap | `unsigned char[12]`, 12 B | the grey of its smoke, 175 fading by 3 a puff to 53 | entity_spawn_init |
| `D_8013C098` | `pc_trail_pts` | f32[6][6], 144 B | `float[12][6]`, 288 B | its two trails' last points | entity_spawn_init |
| `D_80154660` | `pc_blasts` | `tBlastStruct[6]`, 2,400 B | `struct pc_blast[12]`, 4,800 B | MAT3 gBlastObjs: the blast after a crash, four blast objects and a glow, each a scene object on a matrix inside the record | blast.c (func_80090308, StartNewBlast), entity_update_callback (AnimateNewBlast), camera_smooth_lerp (InitBlasts) |
| `D_80154FD8` | `pc_blast_rings` | `tBlastRingStruct[6]`, 360 B to `D_80155140` | `struct pc_blast_ring[12]`, 720 B | MAT3 gBlastRingObjs: the ring, a scene object on its matrix | blast.c (StartNewBlastRing, inlined in save_write_data, StartBlast), entity_collision_detect, camera_smooth_lerp |
| `D_80155290` | `pc_skids` | `WheelSlot[6][4]`, 2,208 B (own storage, 2,304 host) | `struct pc_skid[12][4]`, 4,608 B | the arcade's NewSkid per tire: the skid segment drawn (a host pointer into the segment pool `D_80155B30`) and its edges | skid_marks/group.c (cpak_init, StartSkid, UpdateSkid, ContinueSkid), func_800B0580 (the reset; two copies) |
| `D_80161450` | `pc_resurrect_mid` | f32[6][3], 72 B | `float[12][3]`, 144 B | the resurrect's midpoint (the arcade's update_resurrecting_car) | track_select_handler |

**Inventory.** 16 objects reference the seven names (nm over `game_objects.txt`), all under `src/`, three of them weak
context copies (func_800CB718's in the car_checkpoints and demo_pair groups, func_800B0580's in slot18_alloc). They
declare the arrays through 13 types: `u32[]`, `int[6]` and m2c's scalar `s32` (read as `(&D_801392D8)[i]`) for the
latches, whose readers differ in signedness only in bit operations and stores of 0; `tBlastStruct` (blast.c,
entity_update_callback, the same layout) and `s32` (camera_smooth_lerp's byte offsets) for the blasts;
`tBlastRingStruct` (blast.c), `Effect` (entity_collision_detect: the handle read through an `Id` union whose halves
are swapped on the PC, [ENDIAN.md](ENDIAN.md)) and `s32` for the rings; `WheelSlot[][4]` and `char[]` (func_800B0580's
memset) for the skids; one each for the rest. The image symbol tables name no address inside any of them but the
rings', where the symbol file's `gCheckpointTimes` (+0x28), `gLapCounts` (+0x128), `gBestLapTimes` (+0x138) and
`gCurrentLapTimes` (+0x148) are guesses no C and no asm references; they are retired with the rings.
`layout_check.py --storage` before: 343 accesses on the seven (88, 8, 12, 136, 38, 55, 6), with the callees that
get one record's address and stay inside it (PitchUV and func_80090E9C a blast object's matrix, func_800CFDEC the
midpoint, and StopSkid, UpdateSkid, StartSkid, ContinueSkid and func_8008C074 a skid); after: the same 343 on the
seven PC names, none off the reference.

**What holds an address.** A scan of the four images inflated from the retail cartridge finds no data word in any of
the seven ranges, and 40 code references (`lui` with a following low half in range): 20 to the latches
(entity_spawn_init 7, func_8008E408 4, hud_render 4, func_80092484 2, physics_collision_test 1, func_800CB718 1 and
render_viewport_init 1, where the N64 build inlined func_800CB718; on the PC it calls it), 2 to the smoke's grey, 2 to
the trails, 6 to the blasts (func_80090308 3, entity_update_callback 2, camera_smooth_lerp 1), 6 to the rings
(entity_collision_detect 4, save_write_data 1, camera_smooth_lerp 1), 2 to the skids (cpak_init, func_800B0580) and 2
to the midpoint (track_select_handler): every function one whose C is among the objects above. What the game keeps
of them: MBOX_NewObject (func_8008E26C) keeps a blast object's, the glow's and the ring's matrix address as the
scene node's `owner`, which is pointer-sized in the node's PC layout ([POINTERS.md](POINTERS.md) 9) and read through
as a pointer; StartSkid keeps the segment's address in the skid state, a host pointer already. No `PC_N64W` takes
any of them, layout_check has no width finding (a pointer where the reference has a u32, or the reverse), and a
`RUSH_CHECK_CASTS` build runs every synthetic path without a stop. So unlike the matrix pool
([MORECARS.md](MORECARS.md) 2.7) none needs a window address.

**What it reaches past its end.** The retail code's references to each end address (the next symbol's) are that
symbol's own users (`D_801392F0`'s and `D_8013A028`'s menu_save_options, `D_8013C128`'s CreatePlayer and
func_800C7EC8, `D_80154FC0`'s func_800D816C, `D_80155140`'s net_session_update and stat_race_start, the segment pool
`D_80155B30` that func_800B0580 sets up, `D_80161498`'s Input_ProcessGameplayPad), but one: physics_collision_test's
`i < 6`, which IDO strength-reduced to an end pointer at `D_801392F0`; its C walks by index. No C names an array's
end. Nothing reaches the old ranges: the six that were arena BSS (3,008 bytes) filled with 0x5A after the loader ran
(a throwaway build on the build host) left every synthetic path's trace the same, and none of their bytes changed
by the end of any.

**The size.** Every use reaches one car's entry by the car's slot (the puff's `car`, the visual's `slot`, the node's
`index`, the model's net node) but three resets, which cleared the N64's 6: physics_collision_test the latches
(`i < 6`), InitBlasts the blasts and rings (`i < 6`, strides 0x190 and 0x3C: each blast object's and the glow's
handle, the ring's, to -1) and func_800B0580 the skids (2,208 bytes; 6 x 4 x 0x60 on the PC before). LoadTrack calls
all three (`loadtrack.c:237-239`). On the PC they clear every slot, `PC_MAX_LINKS` and `sizeof (pc_skids)`, not the
race's cars: a slot left out keeps a past race's latches, and a blast object InitBlasts left at 0 would have
StartNewBlast remove scene object 0 (`if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx)`). These are the two rows of
[MORECARS.md](MORECARS.md) 3.1 in these files (item 3 left them to this one); no loop in them walks the race's cars.
So the storage is `PC_MAX_LINKS` (12) entries each. InitBlasts takes 50 matrices from GetMatrix's pool whatever the
cars: they are StartBlast's ring of 50 (`D_8013C238`, `D_8013C094` wraps at 50), not per car, and a car's own blast
and ring matrices are inside its records; the pools' item needs nothing more from this one.

**`D_80111299`** ([MORECARS.md](MORECARS.md) 2.6) stays as it is. It is rows 1-4 of `D_8011128C` (s8[5][13]: 65 bytes
and 3 of padding to `D_801112D0`), which image A's car screen (func_803A44FC) reads as `D_8011128C[p + 1][...]` for
player p: a setting per player and car type. Its three readers index it by slot x 13 + the car type, but only for
humans: the wings' set-up (func_800B08FC, func_800B0A88 in the audio_frame groups) returns for a car this node drives
(`we_control`, set for a drone, owner 0), and the trails' colour (entity_spawn_init's types 6 and 7) comes only from
the wing's animator (buffer_swap) with one player. The humans have the first slots, 0-3, with any number of cars, so
no slot past 3 reads it (a slot 4 or 5 would read `D_801112D0`'s float bytes, which the PC holds swapped).

**The change.** `pc_game_types.h`: the four arrays, `struct pc_uv_matrix`, `struct pc_blast_obj`, `struct pc_blast`,
`struct pc_blast_ring` and `struct pc_skid` (with MAT3's and the arcade's field names, which blast.c's,
entity_update_callback's and skid_marks' views have), the record arrays, and `PC_BLASTS`, `PC_BLAST_RINGS` and
`PC_SKIDS` as `void *`. Each of the 16 files changes its declaration only: under `TARGET_PC` its N64 name is the PC
storage. In 8 files it is the PC array (`#define D_801392D8 pc_tire_fx`, `#define D_80154660 pc_blasts`: their views
have the PC declaration's element type, or its field names and layout); entity_collision_detect, whose `Effect` names
the ring's fields otherwise, sees the array as `PC_MAX_LINKS` of its own element type (`#define D_80154FD8 (*(Effect
(*)[PC_MAX_LINKS])PC_BLAST_RINGS)`); m2c's scalars in func_80092484, func_800CB718 and camera_smooth_lerp are the
array's first word (`#define D_801392D8 (*(s32 *)pc_tire_fx)`, read as `(&D_801392D8)[i]` or through a byte
pointer), and func_800B0580 passes `PC_SKIDS` to memset. Not per-use accessors as in sections 2-4: these arrays are
reached only by index, as whole records in each file's own view (one of them an endian-split union), so a field
accessor would need a name per view; this way every use keeps its N64 tokens and its line, and since each is an
array of `PC_MAX_LINKS` (the view too: an index past it traps, GCC 14 at -O0 with `-fsanitize=bounds`),
`-fsanitize=bounds` checks each index against the PC's count ([MORECARS.md](MORECARS.md) 10.3). A first try saw
blast.c and entity_update_callback through the view cast too; GCC folds `&view[slot].blastObjs[i]` there into a byte
offset from `&pc_blasts`, which layout_check doesn't follow (49 accesses on `pc_blasts`, against 136 on
`D_80154660`), so their structs got MAT3's names instead. Inside functions three lines change: the two resets' bounds
(`PC_MAX_LINKS`, which physics_collision_test and camera_smooth_lerp define as 6 on the N64) and the weak copy of
func_800B0580's byte count (`SKID_TABLE_BYTES`, as the function's own has; `sizeof (pc_skids)` on the PC).
`decoupled.txt`: seven rules, with the N64 extents above. test_casts and test_pool define the PC names where they
defined the N64 ones.

**What the tables say.** `data_symbols.txt` loses the seven rows; `data.c` loses six `.set` lines, the skid state's own
storage (2,304 bytes) with its loader entry and 12 field runs, and seven report names, and gains the seven (10,620
bytes, `.bss`); the later own storage's C names and the loader entries' run indices are renumbered, which is most of
data.c's 412 changed lines (234 with the own storage's C names normalized). gen_data's check: 1,770 symbols at their
N64 addresses (6 fewer), 396 own (1 fewer), 10 decoupled. The synthetic cartridge is the same file.

**What the paths run.** Counted with gdb (a breakpoint on each function that never stops, its hit count at the end)
on the branch's Debug program, Pi: on the retail cartridge the scripted races start skids (StartSkid 622 times in
`race`, 884 in `race2p`, 262 in `pause`) and smoke (entity_spawn_init 789, 992 and 462 times), and crash nowhere, so no
blast; on the synthetic cartridge the races crash (StartNewBlast and StartBlast 24-60 times in `race`, `demo`, `drive`
and `circuit`, the ring's animator 408-1,014 frames, the resurrect 36-108) and smoke (2,262-5,862), and start no
skid. So the retail scripts are the skids' proof and the synthetic paths the blasts'. Neither runs func_8008E408 (a
tire's puffs at speed on a surface of visual code 2); assets_fxcars does.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before (main, 597098e) against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 243 of 243 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 7 lines, `data.c` 412 (234 normalized) |
| the synthetic cartridge, before against after | the same file (Pi and build host) |
| `n64_neutral.py --base 09da2f0` | passes (1,011 files: 92 code-exact, 919 shifted); 16 changed game files under `src/` |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after; offset_check and layout_check the same counts (59,461 and 48,102 accesses, 332 untyped raw accesses); endian_check's 2 open value conflicts in 1 storage the same, 3,506 puns, 20 more (each file's view and the PC struct on one storage); check-loader 96 places, 0 wrong |
| `layout_check.py --storage` on the seven | 343 accesses (88, 8, 12, 136, 38, 55, 6), the same as on the N64 names before; none off the reference |
| `view_audit.py`: `pc_blasts --size 400 --tags tBlastStruct,pc_blast`, `pc_blast_rings --size 60 --tags tBlastRingStruct,Effect,pc_blast_ring`, `pc_skids --size 92` (reference: the PC structs) | 3 views, 0 disagree (before, on `D_80154660`: 2, 0); 4, 0 (2, 0); 22, 16 disagree, every one of them another record of 92 bytes (the tires', the menu's, the checkpoints' header...), as before (19, 16); skid_marks' `WheelSlot` agrees |
| `bounds_check`, build host (GCC 14), and the Pi's mingw-w64 12 (on the Windows build) | 4 new, the same on both, none `clamped` or `two offsets`, all `view` (camera_smooth_lerp's InitBlasts on `pc_blasts` and `pc_blast_rings`, func_80092484's and func_800CB718's word of `pc_tire_fx`), added to `bounds_reviewed.txt` as `[gcc14,mingw12]`; the 2 entries on the retired names (camera_smooth_lerp's `D_80154FD8`, func_800CB718's `D_801392D8`) removed; then none new on either (GCC 14 doesn't find 9 reviewed keys, as on the base, and mingw-w64 12 10: kept for the other compilers). GCC 13 (CI's runner) not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| the six old arena ranges filled with 0x5A after the loader (a throwaway build, build host), against the branch | 19 of 19 paths the same trace; 0 of the 3,008 bytes changed in any |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 10 decoupled |
| `assets_fxcars` (16 slots declared, 12 cars) | passes; with each mutation it fails: the latches' reset at 6 (41 checks), InitBlasts at 6 (78), the skid reset at the N64's 6 cars (1, then a fault on a stale segment pointer), the skid table one tire off (2), the blasts read as the rings (a fault), the rings one car off (2), func_8008E408's latches one car off (3), the smoke's grey one car off (3), the trails one car off (2), the midpoint one car off (2), func_80092484's latch one car off (1) |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, cba8da7: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_fxcars` builds the 13 files above that link (all but the weak copies), with StopSkid, the pools'
own get, free and set-up, and the game's Random, with `PC_MAX_LINKS` 16, and runs a race of 12 cars, every entry of
the seven arrays a sentinel first:
the race start's reset clears all 16 latches; InitBlasts takes 50 matrices and marks all 16 blasts' five objects and
rings empty, their matrices left alone; each of the 12 cars' tire puffs sets its own latch bit and takes two puffs, a
latched tire takes none, and hud_render ends all 24 and clears each car's bit; car 11's smoke sets its latches and its
grey (175, 172, two latched frames, 169) and no other car's; its trails keep their points in its own six floats, and
func_80092484 and func_800CB718 clear its trail and smoke bits; its blast's four objects and glow are scene objects
on matrices inside its own record, which AnimateNewBlast moves with the car; its ring the same, followed by
entity_collision_detect; func_800B0580 clears the whole skid table, and 12 cars x 4 tires start 48 distinct segments
at their tires, continue and stop them, slots 12-15 untouched; car 11's resurrect midpoint lands in its own slot.
test_casts checks InitBlasts' 50 matrices and its -1s in all `PC_MAX_LINKS` blasts and rings of the game build's 12.

**Not done, and for the other items.** GCC 13's bounds_check (CI's runner) not run. The pools (item 2): a car's blast
takes five scene objects and its ring one, from the scene node pool (`D_8012E700`, 600), its smoke and tire puffs
come from the 100 puffs of `D_8013F1E0` (a full pool gives none, and the caller stops), and the skids from the 100
segments of `D_80155B30` (48 at most with 12 cars x 4 tires; a full pool steals the farthest); none was measured with
12 cars. The weak copy of func_800B0580 in slot18_alloc keeps the N64's pool views (36-byte elements), as before: it
is never linked.
