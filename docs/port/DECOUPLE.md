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
   size or is in the window, or if a retired name is defined. Initialized data of the game or the boot segment takes
   `init=image`: the loader then fills the PC symbol from the image's bytes at the retired address (section 7).
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

## 7. The cars' sounds, checkpoints and finish cameras

**What it is.** Eight of [MORECARS.md](MORECARS.md) 2.3's per-car arrays, items 1a (sound), 1c (checkpoints and
places) and 1e (the finish cameras) of its 10.2, 6 slots each on the N64, indexed by a car's slot. Four are the game
image's initialized data, which a decoupled rule couldn't take before (below).

| N64 | What | N64 storage | PC |
|---|---|---|---|
| `D_80140420` | the engine sound sets (`Player84`, `SndSet`): the car type's engine table, `&D_8010FD80[type]`, and four sound records: the two engine layers, one nothing uses, and the random sound (MORECARS.md 6) | 6 x 84 B at 0x80140420, BSS; own storage before (its pointer: 88 B a car) | `pc_engine_sounds`, `struct pc_engine_sound[12]`, 1,056 B |
| `D_80140640` | the tire screech's record (`State20`, `SndRec`, `LayerState`: a handle and the last level and style sent) | 6 x 20 B at 0x80140640, BSS | `pc_screech_sounds`, `struct pc_sound_rec[12]`, 240 B |
| `D_8010FFC4` | the car's sounds started: StartCarSounds sets it, StopCarSounds clears it, the engine update needs it | 6 B at 0x8010FFC4 (8 by the gap), game image data | `pc_car_sounds_on`, `signed char[12]`, from the image |
| `D_8010FFCC` | the engine update's phase: func_800E05F0 skips a car while it is 1 and toggles it, so each car's engine is updated every other frame and half the cars each frame; StopCarSounds sets it to `(slot & 1) == 0` | 6 B at 0x8010FFCC (8 by the gap), game image data | `pc_engine_phase`, `signed char[12]`, from the image, continued |
| `D_80152218` | each car's position at the last CheckCPs, for the distance it drove; func_800FBE60 sets it at the race's start | 6 x 12 B at 0x80152218, BSS | `pc_cp_last_pos`, `float[12][3]`, 144 B |
| `D_801527E8` | each car's distance to its next checkpoint's plane at the last CheckCPs, which times a crossing | 6 x 4 B at 0x801527E8, BSS | `pc_cp_plane`, `float[12]`, 48 B |
| `D_80110680` | a human's finish camera: 1 circling since the finish (race_countdown_display), 2 frozen 10 s later (func_800E9E2C); func_800EB90C turns the window's view to it; render_post_process defined it and clears it, mode_flags_clear clears the first two | 6 B at 0x80110680 (8 by the gap), game image data | `pc_finish_cam`, `unsigned char[12]`, from the image |
| `D_80110668` | the finish camera's start time, then the time it froze at | 6 x 4 B at 0x80110668, game image data | `pc_finish_cam_time`, `float[12]`, from the image |

With them, func_800EB90C's drone cameras (`j < 6`, MORECARS.md 3.1). CalcPlaces' locals (MORECARS.md 2.5), which
this item had too, are item 3's on main (`CARS_N`, MORECARS.md 3.1 "Done"); its test runs CalcPlaces with 12 cars.

**Inventory.** 13 objects reference the eight names (nm over `game_objects.txt`, build host, 8ff5be1): for the sound
`mode_select/d5e64.c` (StartCarSounds), `mode_select/mode.c` (the engine update) and `func_800D5524.c` (StopCarSounds)
with its `slot18_alloc` copy; for the checkpoints `car_checkpoints_func_800F8EC8.c` (CheckCPs) with its `demo_pair`
copy and `func_800FBE60.c`; for the cameras `race_countdown_display.c`, `func_800E9E2C.c`, `func_800EB90C.c`,
`render_post_process.c` and `mode_flags_clear.c` with its `loadtrack` copy (the three copies are weak). Their views:
of `D_80140420` four, all 84 bytes on the N64 and 88 here but one: the table is a `Definitions64 *` (d5e64.c), an
`EngTbl *` (mode.c), an `intptr_t` (func_800D5524.c) and an `s32` in the `slot18_alloc` copy, which never runs
(view_audit's one disagreement before); the sound records are 20 bytes everywhere, a handle and four floats (mode.c
names +4 `level` and +8 `style` and pads the rest); the flags and the phase are `s8`; the cameras' state is `s8` in
three files and `u8` in two (render_post_process's `u8[6]`, mode_flags_clear's `Flags {u8 first, second;}`), stores
and tests of 0, 1 and 2, so one `unsigned char` reads the same; the floats are `f32`, func_800FBE60's `s32` with
m2c's 12-byte stride and `f32` stores. The image symbol tables name one more address inside them, `D_80110681` (`D_80110680[1]`, mode_flags_clear's
`.second`), which no C names. No file reaches a neighbour from them or them from a neighbour (bounds_check,
offset_check and layout_check list none).

**What they reach past their ends.** The next symbols are each their users' own: `D_80140618` (s16, the lights mode:
InitWaves, func_800A54FC, func_800A5B3C, Input_ProcessGameplayPad's two copies, display_list_flush) after the engine
sound sets, `D_801406B8` (`Lights2 *`, the same files) after the screeches, `D_8010FFD4` (each track's song, s32[19]:
func_800D6160 in game_music and three copies, mode_init) after the phase, `D_80152260` (zlib's `dist_code`) after the
positions, `D_80152800` (the route's length: route_cross, world_gravity_apply) after the plane distances, `D_80110688`
(a camera height, func_800EB028) after the cameras; the phase and the cameras' state follow the flags and the times.
No C names an array plus 6 or the next symbol as an end.

**What holds an address.** Nothing in memory. A scan of the four images inflated from the retail cartridge (on the
Pi) found no data word pointing into the eight ranges or at their ends, and, following each `lui` register along
straight-line code (an index added to it or not), 31 code references into them, all in the game image, in 12
functions, each one whose C is among the 13 objects (LoadTrack has mode_flags_clear inlined, which the PC calls), and
24 at the end addresses, in the neighbours' users above. No `PC_N64W` store or u32 field takes an address of them:
layout_check finds no pointer store whose value reaches them; the records' addresses go to player_conditional_call
and player_conditional_check (which reset and release a record), func_800E0048 and mode_select_input (which pass its
handle on), and they keep none; CheckCPs' `prev` is a local; the sound manager is given handles and positions (the
model's +0x22C, `D_801141B0`), not records. d5e64.c's `object = (s32)&D_8010FFC4[player]` truncates the flag's
address into an int that is overwritten before any use ([CASTS.md](CASTS.md) 5's dead store, the same on the PC).
A `RUSH_CHECK_CASTS` build ran every synthetic path without a stop.

**The size.** On the N64 every reader indexes them by a slot below 6: the sound by the model's slot (StartCarSounds for
each car, the engine update for each every frame, StopCarSounds), the checkpoints by CheckCPs' `index < D_801543CA`
and func_800FBE60's `i < D_80152744`, the finish cameras by a human's slot (race_countdown_display, owner 6), a
camera's car (func_800E9E2C's `object->car`) or a window's player (func_800EB90C's `pl->index`); render_post_process
clears 6. So each is `PC_MAX_LINKS` (12) long on the PC. render_post_process clears every slot it has (`FINISH_CAM_SLOTS`, `PC_MAX_LINKS`: a clear, MORECARS.md 3.2) and
func_800EB90C places the drone cameras of `PC_RACE_CARS` cars (`RACE_CARS`); with 6 cars the slots after 6 are never
set, so the default is the N64's.

**The four the image initializes.** `D_8010FFC4`, `D_8010FFCC`, `D_80110668` and `D_80110680` lie in the game image's
initialized data, which the loader copies into the arena from the image. On the retail cartridge (checked on the Pi)
the image holds zeros in the flags, the times and the cameras' state, and in the phase the alternation by slot that
starts with 0: the opposite of StopCarSounds' `(slot & 1) == 0`, so at the first race the odd slots skip their first
update and after any StopCarSounds the even ones do. The synthetic cartridge writes none of them (zeros). Nothing loads
the game image's data again: pc_data_load_all loads it once and frees the image, the game's own inflate of it is a
no-op on the PC (port/ultra/pi.c), an overlay load refills only image A or B (DATA.md 5), and the soft reset
(`gSoftReset`) is a change of state, not a load. Rather than take the zeros for BSS, a decoupled rule now takes the
image's initialized data (`init=image`, gen_data and the loader, below): the loader fills the PC symbol's first
elements from the image's bytes at the retired address, converted through the runs of the N64 part, so the four start
as the N64's from whatever the image holds, and no value of it is in the build. The rest of the storage is zero, or
continues the loaded elements with a period (`period=K`: element i is element i - K): the phase's slots 6 to 11
continue the image's alternation with `period=2`, so with 12 cars 6 update each frame, as 3 of 6 do on the N64.

**gen_data and the loader.** `decoupled.txt` takes `init=image [period=K]` after a rule's aliases; such a rule must
name initialized data of the game or the boot segment (a BSS rule must not), and its N64_SIZE must be whole elements of
the PC type's array (or the type's N64 size). gen_data emits `pc_inits` in data.c, a `PcDataInit` per rule (the PC
storage, the N64 address and size, the host bytes the N64 part fills, the runs of that part, appended after the
others, the period in host bytes, the image), and `PcDataTables` ends with `inits`, `n_inits`; without such a rule
data.c is the same file as before. pc_data_load_image fills them after the image's own storage. They are not in the
loader's symbol table: no N64 address resolves to them (the arena's bytes there stay the image's, which nothing reads),
and they add nothing to its summary line. `datacheck --synthetic` decodes them from its random images like own storage and
checks the continuation; `datacheck` with the retail cartridge checks them against the images it inflates itself.
`pc_data_test` loads two such items, f32 and bytes, one continued with a period of 2.

**The change.** `pc_game_types.h`: `struct pc_sound_rec`, `struct pc_engine_sound`, the eight arrays and their
accessors: `void *` ones for the records, which each file reads through its own view (`PC_ENGINE_SOUND(i)`,
`PC_ENGINE_SOUND_REC(i, k)`, `PC_SCREECH(i)`), and `PC_ENGINE_SOUND_HANDLE(i, k)`, `PC_SCREECH_LEVEL(i)`,
`PC_SCREECH_STYLE(i)`, `PC_CAR_SOUNDS_ON(i)`, `PC_ENGINE_PHASE(i)`, `PC_CP_LAST_POS(i)` (and `_X`, `_Y`, `_Z`),
`PC_CP_PLANE(i)`, `PC_FINISH_CAM(i)`, `PC_FINISH_CAM_TIME(i)`. The 13 files use them, each defining the N64 side
with its own view (`#define PC_ENGINE_SOUND(i) &D_80140420[i]`, `#define PC_ENGINE_SOUND_HANDLE(i, k)
D_80140420[i].rec[k].handle`, `#define PC_SCREECH_LEVEL(i) D_80140640[i].level`, `#define FINISH_CAM_FIRST
D_80110680.first` in mode_flags_clear, and func_800FBE60's strides, `#define PC_CP_LAST_POS_X(i) *(f32 *) ((u8 *)
&D_80152218 + i * 12)`); render_post_process defines `D_80110680` on the N64 only (on the PC it was a
declaration, WINDOWS.md). func_800FBE60 had `D_80152218` open-ended for [OPTIMIZED.md](OPTIMIZED.md) 2.7 (three
stores a car in a loop, the stick's shape); `pc_cp_last_pos` has its true size, which GCC reads correctly.
`decoupled.txt`: the eight rules, four with `init=image`, the phase's with `period=2`; `extents.txt` loses
`D_80110680`'s 6. `assets_cars` and `assets_carrecs` define `pc_cp_last_pos` where they defined `D_80152218`.

**What the tables say.** `data_symbols.txt` loses the 8 rows (2,209 lines to 2,201); `data.c` loses `D_80140420`'s own
storage (528 B), its loader entry (418 to 417) and 24 runs, the 7 `.set` lines of the others, `D_80110668`'s arena run
and the 8 report names, and gains the 8 storages (1,572 B, `.bss`), `pc_inits` (4 rows) and its 4 runs; the later own
storage's C names and the run indices are renumbered: 1,317 changed lines, 56 with those normalized. `data_layout.txt`:
one own storage fewer, two pointer and two width conflicts fewer (the views of `D_80140420` and `D_80140640` above),
the 8 under "Decoupled", and `pc_race_cars` under "Not placed" (func_800EB90C calls it, as `pc_os_idle` and the other
port functions the game C calls are). The loader's summary line differs only by its table's size, which synth_compare
leaves out. The synthetic cartridge is the same file.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before (main, 597098e) against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6; every run reached its state |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 243 of 243 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 8 lines, `data.c` 1,317 (56 normalized) |
| the synthetic cartridge, before against after | the same file (Pi and build host) |
| `datacheck` with the retail cartridge (Pi); `datacheck --synthetic` | all checks passed; the 4 the image initializes hold its elements, the phase continued (0 differ); the same of the random images |
| `n64_neutral.py --base 09da2f0` | passes (1,013 files: 92 code-exact, 921 shifted); 13 changed game files under `src/` (`git diff --name-only 597098e -- src`) |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after; offset_check's report the same file; layout_check's differs by one count (N64-layout storages 561 to 560); alias_check counts 8 placed names fewer (1,727 to 1,719); endian_check the same counts but its puns, 3,486 to 3,498 (the files' views of the sound records and the PC structs on one storage), and the same 2 open value conflicts in 1 storage (CARS.md 6); `--check-loader`: 96 places, 0 wrong |
| `layout_check.py --storage` on each, before (N64 name) and after | the same accesses, none off a reference: 36 on the engine sound sets (`struct pc_engine_sound[12]` after, `SndSet[]` before), 22 on the screeches, 9 on the positions, 8 on the cameras' state, 4 each on the flags, the phase and the times, 2 on the plane distances |
| `view_audit.py --symbols pc_engine_sounds --size 84 --tags Player84,SndSet,pc_engine_sound --ref d5e64.c`; the same for `pc_screech_sounds` (`--size 20`, `State20,SndRec,LayerState,pc_sound_rec`) | 5 views, 0 disagree; 6 views, 0 disagree (before, on the N64 names: 4 and 1, the `slot18_alloc` copy's; 4 and 0) |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`, 9 reviewed keys kept (as on main); `bounds_reviewed.txt` had no entry on the eight names. GCC 13 and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure, headless: no SDL2 fetched) | builds; `gen_data.py --check --pe`: OK, 11 decoupled. (Before d616933 the PE link of `assets_percar` failed: a weak copy, func_800D5524.c's player_conditional_check, can't stand for the symbol on PE; the test links the owner now) |
| `assets_percar` (16 slots declared, 12 cars) | passes; with the phase read as the sounds-started flag, 73 checks fail; with the engine sound sets at the N64's stride of 84, 131; the last positions 6 long, 12; the plane distances 6 long, 18; the screech records 6 long, 12; the finish times 6 long, 4; render_post_process clearing 6, 1; the drone cameras for 6 cars, 5; StartCarSounds' flag 6 long, 6. CalcPlaces' arrays 6 long overrun its stack, which depends on the build (12 checks failed in one, none in another); the UBSan build (build host, SANITIZE.md 1.4) stops it, with main's `CARS_N` at 6: index 6 out of bounds for type `s16 [6]` |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 7cbf1d4: the branch's game C, merged with main) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_percar` builds StartCarSounds, the engine update (func_800E05F0, func_800E0050), StopCarSounds,
player_conditional_call, player_conditional_check, func_800FBE60, CheckCPs, CalcPlaces, race_countdown_display,
func_800E9E2C, func_800EB90C, render_post_process and mode_flags_clear with `PC_MAX_LINKS` 16, every slot a sentinel,
and runs them on 12 cars, a human in slot 0 and 11 drones: 12 engine sound sets started (the table, the two layers' handles from
high_scores_display or camera_target_track, record 3 and the screech reset, record 2 untouched); two frames of the
engine update, each car run in one of them by the image's alternation, a drone's run starting its random sound and
sending each layer's pitch and volume, the human's starting its screech; 12 stopped (records 0, 1 and 3 and the screech
released and reset, the flag cleared, the phase `(slot & 1) == 0`); 12 positions copied at the start; 12 checkpoint
crossings reported with their interpolated times, the last lap ended, the humans (slots 0 and 9, a slot past the N64's
6 among them) given the finish camera, the 12 ranked by distance; a finish camera circled and frozen, a window turned
to it, the drone cameras placed for 12 cars and then for the N64's 6; every finish camera cleared, then the first two.
Nothing past car 11 is touched.

**Left to the other items.** Section 5's rank 3 (`D_80152218` among the race's per-car words) and rank 7 (`D_80110680`,
initialized data) are done here; the other per-car words there (`D_801526E0`, `D_801526F8`, `D_801526A8`) are per
window (MORECARS.md 2.4). The 6s left in these files are not car counts: the owner codes (`== 6`) in CheckCPs and
race_countdown_display, and mode_flags_clear's two slots. The finish camera is only ever set for a human (owner 6),
and the humans are slots 0-3, so it stays per player in practice; func_800E9E2C reads it for any car whose camera
shows view 5. The engines of 12 cars ask MusyX for about twice the voices (MORECARS.md 6); the sound records only hold
their handles.

## 8. The drones

**What it is.** [MORECARS.md](MORECARS.md) 10.2 items 1b and 5: the arrays the drones keep per car, the locals that
order them, and the schedule that updates them. Eight arrays in the game's BSS, 6 entries on the N64, each a plain array
(no record, no pointer):

| N64 | PC | Type | MAT3 / arcade | Indexed by | Users (C) |
|---|---|---|---|---|---|
| `D_801527D8` (0x801527D8, 12 B; the own `D_801527E4` at +12) | `pc_drones` | short[12] | `drones[]` | the drones in the race, `D_80152768` of them | munge (func_800EC914) and display_list_flush's restart write it; MP_IntervalPos (func_800E451C) reads it for the camera's drone pick (`D_80154182`) |
| `D_80152808` (0x80152808, 12 B, 16 by the gap; the car records after it) | `pc_our_drones` | short[12] | `our_drones[]` | the drones this node drives, `D_8015274C` | munge and display_list_flush write it; update_drone_models walks it; func_800E56F8 takes a retired drone out |
| `D_80153F28` (24 B; `D_80153F40`, update_drone_models' cursor, at +24) | `pc_mp_steer` | float[12] | AdjustSteer's steer | car slot | maxpath (func_800E397C); audio_effect_remove clears 6 |
| `D_80153F48`, `D_80153F68` (24 B each) | `pc_mp_brake`, `pc_mp_gas` | float[12] | AdjustSpeed's brake, gas | car slot | maxpath (func_800E3984); audio_effect_remove clears 6 |
| `D_80153F88` (72 B; HAL_DrawText's lastFontXpos, then `num_humans`, after it) | `pc_mp_dir` | float[12][3] | avoid_areas' last direction | car slot | maxpath (func_800E398C) |
| `D_80154138` (72 B) | `pc_mp_save_dir` | float[12][3] | avoid_areas' last direction to the target | car slot | maxpath (func_800E398C) |
| `D_801439F8` (6 B, 8 by the gap; the controllers' `D_80143A00` at +8) | `pc_carsetup_flags` | signed char[12] | carsetup_flag | car slot | CarSetup sets a car's when it starts the car's visuals; display_list_flush clears 6 |

The last one isn't in MORECARS.md 2.3: display_list_flush's `for (i = 0; i < 6; i++) D_801439F8[i] = 0` (its line 374
at c1ab1d7) is a per-car bound, and CarSetup reaches the flag as `(u8 *) &D_801439F8 + slot` for every active car, so a
9th car's flag would write the first controller's pressed buttons. The image symbol tables give the N64 extents: past
the brake, the gas and the two directions sit HAL_DrawText's statics (menu_input_process, `hal_drawtext.c`:
lastFontColor at 0x80153F60, lastTrans 0x80153F80, lastFontXpos 0x80153FD0, lastFontYpos 0x80154180), which are host
statics of that file on the PC, so `data_symbols.txt`'s 32 bytes for `D_80153F48` and `D_80153F68` were the gap, not the
arrays; on the N64 a 7th car's brake would have changed the text's colour state. Not moved: the human list
`D_80143A40` (16 B by the gap, written by munge, read by nothing) holds the players, 4 at most; the counts
(`D_80152768`, `D_8015274C`, `D_80153FD2`: s16) hold 12; the schedule's state (`D_80153E84`, `D_80153F08`,
`D_80153F24`, `D_80153F40`) is scalars.

**Inventory.** nm over `game_objects.txt` (main, 8ff5be1, build host; 597098e and c1ab1d7 change no use of them):
`D_801527D8` 3 objects (func_800EC914, maxpath_controls/group.c, nonmatching/display_list_flush.c), `D_80152808` 3
(func_800EC914, func_800E56F8/group.c, display_list_flush), `D_80153F28`, `D_80153F48`, `D_80153F68` 2 each
(audio_effect_remove, maxpath_controls), `D_80153F88`, `D_80154138` 1 each (maxpath_controls), `D_801439F8` 2 (CarSetup,
display_list_flush): 6 objects, no context copies. Every file declares them as what they are (s16 lists, f32 and f32[3]
by slot, bytes), but two through a scalar and a cast: audio_effect_remove's `((f32 *) &D_80153F28)[i]` (its m2c block
declares `D_80153F48` and `D_80153F68` as s32, stored as f32) and CarSetup's `(u8 *) &D_801439F8 + slot` (declared s32,
stored as s8). The image symbol tables name one address inside the extents, `D_801439FE` (+6 of the flags:
display_list_flush's loop end), which no C names. The other names nearby are neighbours used by their own names:
`D_80153F40` (the cursor), the HAL_DrawText statics, `D_80153FD2`, `D_801527E4` (a message queue's buffer, 11 objects),
`D_80154138` as display_settings' end of `D_80153FD8` (by index on the PC), `D_801439F8` as func_80105B74's end of
`D_801439D8` (likewise).

**What holds an address.** Nothing in memory. A scan of the four images inflated from the retail cartridge (in memory,
on the Pi) found no data word in any of the ranges, and code references (a `lui` of the high half and an offset in
range) only in functions whose C is among the 6 objects: munge, MP_IntervalPos, MaxPathControls (AdjustSpeed and
AdjustSteer inlined), avoid_areas, func_800E56F8, model_iteration (update_drone_models inlined), audio_effect_remove,
CarSetup, display_list_flush; besides them, the ends of the arrays before two of them (display_settings' of
`D_80153FD8` at 0x80154138, func_80105B74's of `D_801439D8` at 0x801439F8) and HAL_DrawText's 4 references to each of
its statics.
No C takes an address to keep: the casts are local, and no `PC_N64W` or u32 field names them. A `RUSH_CHECK_CASTS` build
ran every synthetic path without a stop (below). So nothing needs a window address (MORECARS.md 2.7).

**What it reaches past its end.** Nothing: no C names an array plus 6, or the next symbol as an end. The ends are
`D_801527E4`, 0x80152814 (no name), `D_80153F40`, the HAL_DrawText statics and `D_801439FE`; the retail references to
0x80153F40 (6) are update_drone_models' cursor's (world_physics_tick, display_list_flush, func_800E56F8,
model_iteration), and those to the statics HAL_DrawText's. bounds_check, offset_check and layout_check list no access
from a neighbour into the ranges or out of them (below).

**The size.** The lists hold every drone of a race of `PC_MAX_LINKS` cars (the attract demo's are all drones), the
controls and flags one entry per car slot: `PC_MAX_LINKS` (12) each, 492 bytes in all. The walks: munge and
display_list_flush append to the lists for the active cars (`D_80152744`), update_drone_models walks `D_8015274C`,
MP_IntervalPos `D_80152768`, maxpath indexes by the model's slot, CarSetup by the active cars' slots; the two clears
(audio_effect_remove, display_list_flush) were `i < 6` and are `i < PC_MAX_LINKS`, every slot.

**The change.** `pc_game_types.h`: the 8 declarations and their accessors (`PC_DRONE(j)`, `PC_OUR_DRONE(j)`,
`PC_MP_STEER(slot)`, `PC_MP_BRAKE`, `PC_MP_GAS`, `PC_MP_DIR(slot)` and `PC_MP_SAVE_DIR(slot)` (a row of 3),
`PC_CARSETUP_FLAG(slot)`). The 6 files use them, each defining the N64 side (`#define PC_MP_DIR(slot)
D_80153F88[slot]`, `#define PC_MP_STEER(i) ((f32 *) &D_80153F28)[i]`); CarSetup keeps its spelling and on the PC
defines `D_801439F8` as `PC_CARSETUP_FLAG(0)`, the first flag (func_800D1248's way with `D_8014A96C`, section 3).
`decoupled.txt`: the 8 lines, sizes the N64 extents (0xC, 0xC, 0x18 three times, 0x48 twice, 0x8). In the same files,
the bounds that walk them or the cars (MORECARS.md 3.1, 2.5): munge's clear of the model records after the race's,
`i < MAX_LINKS`, is `PC_MODEL_SLOTS` (its owner test, `owner < MAX_LINKS`, keeps 6: an owner code, MORECARS.md 1.4);
model_iteration's speed walk `v != 6` is `PC_RACE_CARS`; DoDrones (render_large_objects and its car_checkpoints and
demo_pair copies) sizes its locals (`humans`, `drones`, `total_drones`, `car_in_conflict`, `cars_in_order`, `place`,
`car_distance[][]`) by its `MAX_LINKS`, which is `PC_MAX_LINKS` on the PC (it has no owner-code use; its loops walk
`D_80152744`). assets_roster, assets_audiocmds define the PC storage where they defined the N64 names.

**The drone schedule.** `update_drone_models` updates, each model iteration, the next `drone_sched[row][k]` of our
drones round the list (`D_80153F40`), k counting up until -1, which ends a round and takes the next round's row
(`D_80153F08`). `drone_sched` is `D_80120E74`, ROM data, `Row3[12]` of s16. world_physics_tick picks row
`D_8015274C`, plus 7 with two humans or more; a drone's retirement takes 1 off the next round's row (func_800E56F8),
and a human's, of two, 7, to the one-human rows. Rows 0-6 are 0-6 drones with one human or none,
7-11 0-4 drones with two: 7-12 drones with one human would read the two-human rows, and 5-10 drones with two would
read past the table (the spike's UBSan, MORECARS.md 2.6). In the retail cartridge no row has more than two groups, so a
round is at most two model iterations, and the rows with two split the drones as evenly as they can, the larger part
first (read on the Pi with gdb; the values stay there). The PC keeps every N64 row as the game's data has it and adds a
rule past them (func_800E56F8/group.c, `pc_drone_sched`):

- **Rows.** The second kind starts at `PC_DRONE_SCHED_TWO` = `PC_MAX_LINKS` + 1 (13), so a row is n for n drones with
  one human or none and 13 + n with two, n up to 12, and the two kinds never meet. world_physics_tick's `+= 7` and
  func_800E56F8's `>= 7`, `-= 7` use it; display_list_flush's restart (every car a drone, row `D_8015274C`) and the
  drone's `-= 1` stay as they are.
- **The N64's rows.** For n up to 6 (one human) and up to 4 (two), entry k is `D_80120E74[n].v[k]` and
  `D_80120E74[7 + n].v[k]`, read as the N64 reads it, k unbounded as there.
- **Past them.** A round has as many iterations as the N64's last row of the kind (row 6, row 11) has groups before its
  -1; iteration k updates n / groups drones, plus one for the first n % groups: each drone once a round, and a round as
  long as the N64's longest, so a drone's model step (the ticks since its last update) stays what the N64 gives its
  most crowded race. With the retail cartridge's rows, 7-12 drones are two iterations a round, half the drones each;
  with the synthetic cartridge's ({n, -1}), one, every drone.

So the rule keeps a drone's step, not the number of drones an iteration: up to 6 an iteration on the PC. It reads
only the game's data, so nothing from the cartridge is in the build. world_physics_tick.c is item 3's file for its
other loops; the row choice at its line 388 (c1ab1d7) changed here (the car records' item left it, and item 3 doesn't
touch the line). With 6 cars every row is one the N64 has, read as before: the default is the N64's schedule.

**What the tables say.** `data_symbols.txt` loses the 8 rows; `data.c` loses their 8 `.set` lines and 8 report names
(2,170 to 2,162 at c1ab1d7) and gains the 8 storages (`.bss`, 492 bytes); 45 diff lines. The loader's tables are the
same (416 entries, 7,704 runs), and no ROM word pointed into the ranges. Debug and RelWithDebInfo give the same files.

**Proof.**

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, the build before (main, c1ab1d7) against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run, each reaching its state: 6 of 6 |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 245 of 245 each (`assets_rom` skipped: no ROM there); in Debug `pak_realtime`, which measures the host's clock, failed under the other tests of `ctest -j3` (8 retraces caught up, 0 dropped) and passed alone, as CI runs it (`-L realtime`, one at a time, up to three tries) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 8 lines, `data.c` 45 |
| the synthetic cartridge, before against after | the same file (build host and Pi) |
| `n64_neutral.py --base 09da2f0` | passes (1,014 files: 92 code-exact, 922 shifted); against c1ab1d7, 9 changed game files under `src/`, all shifted |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after, and the same counts: offset_check 59,461 accesses on 3,468 storages, layout_check 48,102, endian_check its 2 open value conflicts in 1 storage (the same) and 3,518 puns, with one live function more (`pc_drone_sched`: 2,545); `--check-loader` 96 places, 0 wrong |
| `layout_check.py --storage` on the 8 | 58 accesses before (on the N64 names) and after (on the PC names), the same per storage at the same offsets (one, display_list_flush's clear of the flags, unplaced in both) |
| `bounds_check`, build host (GCC 14) | 1 new, not `clamped` or `two offsets`: CarSetup's `view` on `pc_carsetup_flags`, added to `bounds_reviewed.txt` as `[gcc14]`; the 7 entries on the retired names (CarSetup's `D_801439F8`, audio_effect_remove's three, `view` and `warn`) removed; then none new, 9 kept (as before); pairs found 96 through a variable and 44 with warnings before, 93 and 41 after (the constant and indexed ones the same). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| `view_audit.py` | nothing to audit: the 8 are plain arrays, no record views |
| the old window ranges filled with 0x5A at the race's start (gdb, build host, Debug): race, race2p, demo, circuit, pause | each the same trace as without; every byte of the 8 ranges still 0x5A at the end |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64 12, RelWithDebInfo, fresh configure) | builds; `gen_data.py --check --pe`: OK, 26 decoupled |
| the PC rule on the retail cartridge's table (gdb, Pi, `pc_drone_sched` called for every row) | every PC row, 7-12 drones with one human and 5-12 with two, is two iterations whose counts add up to its drones; each of the 12 N64 rows adds up to its drones too |
| `assets_drones`, `assets_roster` (16 slots declared) | pass, in Debug, under UBSan (`-DRUSH_SANITIZE=undefined`, build host) and under ASan + UBSan (Pi 5, GCC 12); the mutations below |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, a356a95: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The tests.** `assets_drones` (`port/assets/tests/test_drones.c`) builds func_800E56F8's group, maxpath_controls,
audio_effect_remove and render_large_objects with `PC_MAX_LINKS` 16, so the storages have 16 entries and the
schedule's second kind starts at 17:
- MaxPathControls for each of 12 cars on a straight path, 400 feet apart: the steer, brake and gas land in the car's
  slot and in its model record (+0x720, +0x728, +0x72C), the direction's x is half the slot's last one and the
  direction to the target is kept in the slot, and every other slot's five values stay as they were; the brakes of
  slots 7-11 differ (the gas fell below -0.75 there). audio_effect_remove clears all 16 slots' controls.
- the camera's pick among 11 drones (slots 1-11, slot 7 not this node's): MP_IntervalPos moves it to list indices 0-5,
  7-10 and round again, once each half second.
- update_drone_models on the test's own table (rows 0-2 and 7-8 one group, the rest two: one drone then the others
  with one human, the others then one with two; neither cartridge's), then on the synthetic cartridge's kind
  ({n, -1}): for 0-6 drones with one human and 0-4 with two, 60 iterations give the same slots in the same order as
  the N64's code on the same table (a copy of it in the test); for 7-12 and 5-10, each of 5 rounds updates every drone
  once, in 2 iterations (1 with the second table) of n / 2 and the rest, and ends with the cursor at the first
  drone. Then a drone done at the cursor in a race of 11 drones and two humans (6 asked, 5 updated, then 5: every
  other drone once), and a human done (the next round on the one-human row of 10).
- model_iteration's speed for every car with no input and no node driving it: the first 6 records with
  `PC_RACE_CARS` 6, the first 12 with 12, never the rest.
- DoDrones for one human (slot 4, in place 6) and 11 drones: each drone's weight is its order by place, the nearer of
  the human's two neighbours (behind) targets the human, every other drone itself, and every drone's catch-up scale
  moves below 1. Its locals at 6 for 12 cars pass these checks in a plain build (the overrun lands where no check
  looks); the sanitizer job's UBSan stops the test (`index 6 out of bounds for type 'S16 [6]'`, checked on the build
  host).

`assets_roster`'s new case runs munge for 12 slots: one human and 11 drones (slot 7 a drone another node drives: in
`pc_drones`, not in `pc_our_drones`), then the attract demo's 12 drones; every active slot is counted and listed in
order, the lists' entries after the last stay as they were, and the model records 12-15 are cleared.

| Mutation | Checks that fail |
|---|---|
| `PC_MP_BRAKE` reading the gas | 38 (assets_drones) |
| `PC_MP_DIR` reading the direction to the target | 23 |
| the second kind of row at 7 (`PC_DRONE_SCHED_TWO`) | 21 |
| every drone each iteration past the N64's rows | 36 |
| past them, the smaller group first | 8 |
| model_iteration's walk at 6 | 6 |
| audio_effect_remove clearing 6 | 10 |
| munge's clear up to `MAX_LINKS` (6) | 8 (assets_roster) |
| munge listing the drones in `pc_our_drones` | 36 (assets_roster) |
| DoDrones' locals at 6 | none in Debug; the UBSan build stops at the first overrun |

**Left to others.** Nothing in these files: item 3's first part (597098e) did the rest of MORECARS.md 3.1 around them
(world_physics_tick's first distances and its reset of the slots after the race's, set_catchup, car_setup_confirm's
locals), and relies on munge's clear reaching `PC_MODEL_SLOTS` (above) so that LoadCars, which reads every record's
drone type below the race's cars, doesn't load cars for records an earlier race of more cars left as drones. DoDrones
reads the circuit's per-slot ratings `D_80154484[slot][38]` in mode 3 only: the circuit stays 6 cars (MORECARS.md
3.3). MORECARS.md 2.3 lists `D_80153F48` and `D_80153F68` as 32 bytes, 8 by the gap: they are 24, with HAL_DrawText's
statics after them.
