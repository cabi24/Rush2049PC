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
   `init=image`: the loader then fills the PC symbol from the image's bytes at the retired address (section 6).
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

## 6. The cars' sounds, checkpoints and finish cameras

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

With them, CalcPlaces' locals (`func_800D1AB0`: `s16 place[6]`, `s32 dist_tab[6]`, the second indexed by slot;
MORECARS.md 2.5: at 12 cars the spike read back indexes -11, -10 and 65544 from its stack) and func_800EB90C's drone
cameras (`j < 6`, MORECARS.md 3.1).

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
clears 6. So each is `PC_MAX_LINKS` (12) long on the PC, and so are CalcPlaces' two locals (`PLACE_SLOTS`).
render_post_process clears every slot it has (`FINISH_CAM_SLOTS`, `PC_MAX_LINKS`: a clear, MORECARS.md 3.2) and
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
`PC_CP_PLANE(i)`, `PC_FINISH_CAM(i)`, `PC_FINISH_CAM_TIME(i)`. The 13 files and CalcPlaces' two copies use them, each
defining the N64 side with its own view (`#define PC_ENGINE_SOUND(i) &D_80140420[i]`, `#define
PC_ENGINE_SOUND_HANDLE(i, k) D_80140420[i].rec[k].handle`, `#define PC_SCREECH_LEVEL(i) D_80140640[i].level`, `#define
FINISH_CAM_FIRST D_80110680.first` in mode_flags_clear, and func_800FBE60's strides, `#define PC_CP_LAST_POS_X(i)
*(f32 *) ((u8 *) &D_80152218 + i * 12)`); render_post_process defines `D_80110680` on the N64 only (on the PC it was a
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
| `synth_compare.py --same --runs 2`, the build before (main, 8ff5be1) against after, Debug, Pi 5 (aarch64, GCC 12), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6; every run reached its state |
| `synth_compare.py --same --runs 2`, the same, build host (x86-64, GCC 14) | 19 of 19 paths |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 243 of 243 each (`assets_rom` skipped: no ROM there) |
| Debug and RelWithDebInfo `data.c`, `data_symbols.txt` | the same files (build host); before against after, `data_symbols.txt` 8 lines, `data.c` 1,317 (56 normalized) |
| the synthetic cartridge, before against after | the same file (Pi and build host) |
| `datacheck` with the retail cartridge (Pi); `datacheck --synthetic` | all checks passed; the 4 the image initializes hold its elements, the phase continued (0 differ); the same of the random images |
| `n64_neutral.py --base 09da2f0` | passes (1,013 files: 92 code-exact, 921 shifted); 15 changed game files under `src/` (`git diff --name-only 8ff5be1 -- src`) |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after; offset_check's report the same file; layout_check's differs by one count (N64-layout storages 561 to 560); alias_check counts 8 placed names fewer (1,727 to 1,719); endian_check the same counts but its puns, 3,486 to 3,498 (the files' views of the sound records and the PC structs on one storage), and the same 2 open value conflicts in 1 storage (CARS.md 6); `--check-loader`: 96 places, 0 wrong |
| `layout_check.py --storage` on each, before (N64 name) and after | the same accesses, none off a reference: 36 on the engine sound sets (`struct pc_engine_sound[12]` after, `SndSet[]` before), 22 on the screeches, 9 on the positions, 8 on the cameras' state, 4 each on the flags, the phase and the times, 2 on the plane distances |
| `view_audit.py --symbols pc_engine_sounds --size 84 --tags Player84,SndSet,pc_engine_sound --ref d5e64.c`; the same for `pc_screech_sounds` (`--size 20`, `State20,SndRec,LayerState,pc_sound_rec`) | 5 views, 0 disagree; 6 views, 0 disagree (before, on the N64 names: 4 and 1, the `slot18_alloc` copy's; 4 and 0) |
| `bounds_check`, build host (GCC 14) | none new, 7 kept (as before); `bounds_reviewed.txt` had no entry on the eight names |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | {{WINDOWS}} |
| `assets_percar` (16 slots declared, 12 cars) | passes; with the phase read as the sounds-started flag, 73 checks fail; with the engine sound sets at the N64's stride of 84, 131; the last positions 6 long, 12; the plane distances 6 long, 18; the screech records 6 long, 12; the finish times 6 long, 4; render_post_process clearing 6, 1; the drone cameras for 6 cars, 5; StartCarSounds' flag 6 long, 6. CalcPlaces' arrays 6 long overrun its stack, which depends on the build (12 checks failed in one, none in another); the UBSan build (build host, SANITIZE.md 1.4) stops it: index 6 out of bounds for type `s16 [6]` |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 061a59b: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_percar` builds StartCarSounds, the engine update (func_800E05F0, func_800E0050), StopCarSounds,
player_conditional_call, player_conditional_check, func_800FBE60, CheckCPs, CalcPlaces, race_countdown_display, func_800E9E2C, func_800EB90C,
render_post_process and mode_flags_clear with `PC_MAX_LINKS` 16, every slot a sentinel, and runs them on 12 cars, a
human in slot 0 and 11 drones: 12 engine sound sets started (the table, the two layers' handles from
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
