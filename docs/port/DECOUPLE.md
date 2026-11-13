# Decoupling game data from its N64 address

Since [NATIVE.md](NATIVE.md) step 13 (section 18) every datum is an object of its own, filled from the image as the
N64 laid it out ([DATA.md](DATA.md) 3), with AddressSanitizer's redzones around it; the general case of this page's
recipe, without a per-item accessor where an item keeps its C name and type. This page is the recipe as it was done
item by item before that, when most of the game's data sat in its image's arena, laid out as the N64's data and BSS
were (a block of host memory from step 5, at the N64 addresses in the RDRAM window before), so the decompiled C ran
unchanged; that cost the port: an array couldn't grow (its neighbours followed it), and address arithmetic between
symbols was baked in. Decoupling moves one item
to storage the PC owns outright, with one PC declaration and no N64 address. The first item is the race roster
(the arcade's `gLink`), the second the model records (`D_8014A250`, MAT3 `model[]`), the third the car records
(`gGameCar`, MAT3 `gGameCar`), which had host storage of their own already: steps toward 12 cars a race
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
   array's end (the model records' `other < gTargetArray`, section 3), so the code references to the end address
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
4. **Readers and aliases.** Each use goes through an accessor (`PC_LINK_OWNER(i)`) that `pc_game_types.h` defines.
   (Until the N64 build was dropped on 2026-11-08, each game file also defined the N64 side under `#else`, with its
   own view, so the N64 tokens stayed the original ones; [PCONLY.md](PCONLY.md).) An alias inside the extent becomes
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
| +0 | `track` | the track (`gTrackNum`) | fake_init, CalcPolePositions, GhostStart.c | nothing |
| +1 | `car` | car model 0-12 | InitSelectCarLoad (= slot, all 13), LoadCars, GhostStart.c, cleanup_colortrace | init_cars, LoadCars, car_palette_init, InitVisuals, CreateCar, GhostStart.c |
| +2..+4 | `color[3]` | the car's colours | LoadCars, GhostStart.c, cleanup_colortrace, car_palette_init | init_cars (into the model's +0x7CD..+0x7CF), car_palette_init |
| +5 | `place` | start place | fake_init, CalcPolePositions, GhostStart.c | init_cars, munge_gLink_data (the car's +0xEE), init_mdrive (the camera), CalcPolePositions |
| +6 | `flags` | 0x80 active (`LINK_ACTIVE`), 0xB0 in a race | fake_init, CalcPolePositions, GhostStart.c, cleanup_colortrace | init_cars, munge_gLink_data |
| +7 | `owner` | 0-5 a drone that node drives (0 here), 6 a human, 7 empty | fake_init, CalcPolePositions, GhostStart.c, PrePlay (`-= 6`) | 20 files: the race, camera, music, menus, resurrect, checkpoints, PlayTargetSound (through `D_80153E8F`) |

**Inventory.** 30 objects reference the two names (nm): 29 game files under `src/`, some of them copies of one
function in another group (`loadcars/setup.c` of car_palette_init, `front_end/ctx_init_state_continue.c`, `demo_pair`'s
two), and `nonmatching/display_list_flush.c`.
They had 13 views of the record (`tLink`, `Link`, `Config`, `Slot`, `Player`, `Selection8`...), all 8 bytes, all
bytes; the signed ones (`s8 owner` at +0 in fake_init, `s8 type` in CalcPolePositions, the colours in LoadCars,
GhostStart.c and cleanup_colortrace) are only stored to, so one `unsigned char` type reads the same values. Every access
is one byte through an index or a record pointer: no file reaches a neighbour from it or it from a neighbour
(`bounds_reviewed.txt` and the data tables have no view across it). The image symbol tables name five addresses in
it: `gLink`, `D_80153E89` (`[0].car`), `D_80153E8F` (`[0].owner`, read by `PlayTargetSound` and UpdateModel's
group with a stride of 8), `D_80153E97` (`[1].owner`) and `D_80153EB8` (`&[6]`, likely the end pointer of a
6-slot loop); only the first and third had C users. No PC code stores its address (`PC_N64W`, a u32 field, an
absolute address). A scan of the four images inflated from the retail cartridge found no data word in
0x80153E88-0x80153EF0, and 38 code references (`lui 0x8015` plus an offset in range: 36 in the game image, 2 in image
A), which are the C above.
`layout_check.py --storage` before: 123 accesses on `gLink` and 2 on `D_80153E8F`, which it could not place in
the record (`N64 ?`); after: 125 on `pc_links`, every one a byte at the offset of the reference layout, the two
through the alias at +7.

**The size.** A race on the N64 activates `MAX_LINKS` 6 slots (the per-car arrays are 6 long; gen_data had the 104
bytes by the gap). But three functions walk 13 slots: LoadCars (slots 6-12 copy slot 0's car), InitSelectCarLoad (each
slot's car is its index) and InitVisuals (an engine sound per slot, with `gCarModelNums[13]`). The gap is exactly 13
records. So the storage is `PC_LINK_SLOTS` = the larger of 13 (`PC_N64_LINK_SLOTS`) and `PC_MAX_LINKS` (8, the cars a
race may hold on the PC, then; 12 since 2026-10-29); with 8 or 12 it is 13, 104 bytes, and the game still activates 6.

**The change.** `pc_game_types.h`: `struct pc_link`, `pc_links[PC_LINK_SLOTS]`, accessors `PC_LINK_TRACK(i)` ...
`PC_LINK_OWNER(i)`, `PC_LINKP_*(p)` for record pointers, `PC_LINK` and `PC_LINKS`. The 30 files use them; each
defines the N64 side with its own view (`#define PC_LINK_OWNER(i) gLink[i].state`, `#define PC_LINK_OWNER(i)
*(&D_80153E8F + (i * 8))`). `decoupled.txt`: `pc_links gLink 0x68 D_80153E8F`. `test_cars` and
`test_audiocmds` define `pc_links` where they defined `gLink`.

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
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after; the reports differ only by line numbers and one storage fewer (two roots, `gLink` and `D_80153E8F`, are one, `pc_links`) |
| `bounds_check`, build host (GCC 14) | 5 new *tmr* entries on `pc_links`, none `clamped` or `two offsets`, added to `bounds_reviewed.txt` as `[gcc14]`; the `D_80153E8F` entry, which nothing can find now, removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 1 decoupled |
| `assets_roster` (16 slots declared) | passes; with `PC_LINK_OWNER` reading `flags`, 20 checks fail |

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) had not been run; it covers the 29 changed files
under `src/` (`git diff --name-only 0aaa349 -- src`). Run with section 4's change (2026-10-29), it finds every object
the same as 09da2f0's but in its debug sections.

**The test.** `assets_roster` builds InitSelectCarLoad, fake_init, CalcPolePositions, munge_gLink_data and
PlayTargetSound with `PC_MAX_LINKS` 16, every slot a sentinel byte whose flags have `LINK_ACTIVE`. Each must write
and read the fields at their N64 offsets in the slots it walks and leave the rest alone: the 13 cars named, a race's
6 slots reset, a race of 2 players and 2 drones laid out (places, owners, flags, track), 4 active cars counted into
`gNumActiveCars` with the human and drone lists, and the owner read where `D_80153E8F` reached it.

## 3. The model records

(Since 2026-11-09, wave 3 step 2 of [NATIVE.md](NATIVE.md), the record's five pointer fields are host pointers and it
is 0x830 bytes on the PC: [CARS.md](CARS.md) 4. This section is the decoupling as it was done, in the N64 layout.)

**What it is.** One 0x808-byte record per car, 6 of them, at 0x8014A250-0x8014D280 in the game's BSS: MAT3's `model[]`
(`tModelData`), the car's physics state, its controls and its race state. It keeps its N64 layout on the PC: its five
pointer fields are u32 window addresses whose pointees stay in the window ([CARS.md](CARS.md) 4), so the host size is
0x808 too and code that steps through it by 0x808 still lands on the fields. `struct pc_model` names the 45 fields the
game reaches by index or through a name inside the array (resurrect.c's `tModelData` gives MAT3 names for some);
everything else is reached through a record pointer in each file's own view:

| Offset | Fields (accessor suffix) | Reached by index from |
|---|---|---|
| +0x000, +0x004 | `CAR`, `tune` (u32 window addresses) | update_game_data (through its view) |
| +0x008..+0x00F | `BODY` (u8), `INPUT_FLAGS`, `AUTOTRANS`, `B00F` | 21 uses: the HUD, entity spawns, LoadCars, SetupCar, StartFrame (through `D_8014A258`) |
| +0x220, +0x22C | `RWV`, `RWR` (f32[3]: velocity, position) | race_tail, UpdateListeners; the state log (port/main.c) |
| +0x3F0..+0x75C | `SPEED`, `SCALE`, `F544`, `VISCODE[4]`, `CRASHFLAG`, `TOPSCRAPE`, `RESURRECT_POS`, `MOVING_STATE`, `ITERATION`, `MODEL_TIME`, `DT`, `MODELRUN`, `STEER`, `BRAKE`, `GAS`, `GEAR`, `FLAG_B`, `FLAG_A`, `SUSPENSION[4]` | the ghost playback (GhostUpdate), the model clock (init_model_task), resurrect, the HUD, m2c strides on `D_8014A640`, `D_8014A890`, `D_8014A914`, `D_8014A964`, `model_modelrun` |
| +0x794 | `RECKON_RWR` (f32[3]) | maxpath |
| +0x7C4..+0x7F0 | `OB_RES_PT`, `SLOT`, `ACTIVE`, `WE_CONTROL`, `DRONE_TYPE`, `COLOR0..2`, `S7D0`, `FLAGS`, `STUCK_TIME`, `CAN_CHEAT`, `HIDE_CAR`, `LAST_CP`, `DRONE_TARGET`, `LAP`, `COLLIDABLE`, `DRONE_SCALE`, `TIME_BOOST` | 55 uses of `SLOT` alone; the race end, the drones, the HUD, through `D_8014AA14`, `D_8014AA16`, `D_8014AA32`, `D_8014AA3A` |

**Inventory.** 112 objects reference the names (nm): 108 game files under `src/`, context copies among them (three each
of AbortCar, CheckForStunt, DoDrones and path_D169C, two each of race_tail, CarReportsGameOver,
CheckCPs, PlaceUnlockedCars, End_MDrive and modelstop), and four under `nonmatching/`; and the state log
in `port/main.c`. They declare it through 122 views, all 0x808 bytes but m2c's 2,026-byte `ModelRecord`, whose
code steps by 0x808 explicitly. Only two offsets have readers that disagree on signedness (+0x7CC, +0x7D4), and those
are stores and bit operations, so one type reads the same values. The image symbol tables name 24 addresses inside it,
12 of which the C used: `D_8014A250_alias` (a second symbol for the base), and [0]'s fields `D_8014A258` (+0x8),
`D_8014A640` (+0x3F0), `D_8014A890` (+0x640), `D_8014A914` (+0x6C4), `D_8014A964` (+0x714), `model_modelrun` (+0x71C),
`D_8014AA14` (+0x7C4), `D_8014AA16` (+0x7C6), `D_8014AA32` (+0x7E2), `D_8014AA3A` (+0x7EA), read with a stride of 0x808
or as record 0's field.

**What holds a record's address.** Nothing in memory. A scan of the four images inflated from the retail cartridge found
no data word in 0x8014A250-0x8014D280, and 177 code references (`lui 0x8015` with an offset in range: 162 in the game
image in 99 functions, 15 in image B in 11), every function one whose C is among the 112 objects. No `PC_N64W` or
integer store takes a record's address: layout_check's analysis of every live game file finds no pointer store whose
value reaches the storage (it finds the car record's `&gPlayers[i]`, the check's control), and a `RUSH_CHECK_CASTS`
build ran the race and the demo without a stop. The 342 call arguments that pass a record's address (to about 150
parameters: the physics, the cameras, the sounds' positions) use it and don't keep it; `AudEmitEffect` copies the
position. `Gfx` words hold host pointers anyway (`w1` is pointer-sized).

**What it reaches past its end.** `collision`, the car-to-car collision, walks the cars after one up to
`gTargetArray`, the next symbol (the level-object nodes), which is the array's end on the N64. Its storage moved, that
comparison was false from the first car on, and no two cars collided: the first synth_compare run differed on 12 of 19
paths, cars 0 and 5 overlapping at the start. It is `PC_MODELS_END` now, the end of the N64's 6. The retail code has
three references to 0x8014D280: that one, initTargetStructs's node pool (the neighbour's own) and init_model_task's `i
< 6`, which IDO strength-reduced to an end pointer. Nothing reaches the old range any more: filled with 0x5A after the
loader ran, it left the race path's trace the same, and nothing wrote to it.

`layout_check.py --storage` before: 4,243 accesses on `model` and 17 on 9 of the names inside, 3,434 of them
through the parameters above; after: 4,271 on `pc_models`, none off the reference (`struct pc_model[8]`).

**The size.** Every walk stops at 6: loops to 6 or `MAX_LINKS` (init_model_task, StartEnvironment, update_game_data,
SetCatchup, CheckCameraView, game_reckon_all, DetectDroneAbort, BlinkResurrectingCars, dead_reckon_all,
UpdateModel's group, LoadCars' `i < 6` guard), to the cars in the race (`gNumCars`, `gNumActiveCars`), the players or
the windows, and the collision's end pointer. So the storage is `PC_MODEL_SLOTS` = the larger of 6
(`PC_N64_MODEL_SLOTS`) and `PC_MAX_LINKS` (8): 8 records, 16,448 bytes, and the game still activates 6. With
`PC_MAX_LINKS` 12 (2026-10-29) it is 12 records, 24,672 bytes.

**The change.** `pc_game_types.h`: `struct pc_model`, `pc_models[PC_MODEL_SLOTS]`, the accessors `PC_MODEL_<FIELD>(i)`
for the indexed fields, and three `void *` ones for code that keeps a record pointer in its own view: `PC_MODEL(i)` (a
record), `PC_MODELS` (the array, for m2c's pointer walks of 0x808) and `PC_MODELS_END`. The 112 files use them, about
340 of the uses rewritten by a script from each file's DWARF (the view's field to its N64 offset to the accessor) and
about 60 by hand (the m2c strides and the aliases); each defines the N64 side with its own view (`#define
PC_MODEL_SLOT(i) model[i].half1990`, `#define PC_MODEL_SLOT(i) *(s16 *)((u8 *)&D_8014AA16 + i * 0x808)`, `#define
PC_MODEL(i) (ModelRecord *) ((i * 0x808) + ((u8 *) (&model)))`). FinishResurrecting's debug guard reads
`model_modelrun` as a scalar, so on the PC it is `#define model_modelrun PC_MODEL_MODELRUN(0)`. `decoupled.txt`: `pc_models
model 0x3030` and the 11 aliases. The state log reads `pc_models[0]`; ten asset tests and test_overlay_data define
`pc_models` where they defined `model` (test_cars and test_targets had it as an absolute symbol in their window).
gen_data needed nothing new.

**What the tables say.** `data_symbols.txt` loses the 11 rows of `model` and the names inside; `data.c` loses their
12 `.set` lines and 11 report names, and gains `pc_models` (16,448 bytes, `.bss`); `data_layout.txt` lists it under
"Decoupled" with the 24 retired names. The loader's symbol table is the same (419 entries) and no ROM word pointed into
the range, so its summary line is too. The synthetic cartridge is the same file. The arena's 0x8014A250-0x8014D280 has
no symbol now: 12 KB nothing uses. (From [MORECARS.md](MORECARS.md) 8 until [NATIVE.md](NATIVE.md) step 4 its first
6,144 bytes were GetMatrix's spill, `pc_matrix_spill`, reserved in `port/data/reserved.txt`; the spill is PC storage
now, and the whole range is free again.)

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
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after (endian_check's 2 open value conflicts in 1 storage are the same before and after); the reports differ only by line numbers and 11 storages fewer (`model` and 11 names inside it are one root, `pc_models`); endian_check counts 2,839 puns, 491 more (each file's view and `struct pc_model` on one storage) |
| `layout_check.py --storage pc_models` | 4,271 accesses, none off the reference (`struct pc_model[8]`) |
| `view_audit.py --symbols pc_models --size 0x808 --ref init_cars.c` | 165 views (`struct pc_model` among them), 0 disagree (before, on `model`: 122, 0) |
| `bounds_check`, build host (GCC 14) | 3 new, none `clamped` or `two offsets`, added to `bounds_reviewed.txt` as `[gcc14]` (two `view` on `pc_models`, one `tmr` on `pc_links` in init_cars); 19 entries on the retired names removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): race, demo | no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 2 decoupled |
| `assets_models` (16 records declared) | passes; with `PC_MODEL_MODELRUN` reading `ob_res_pt`, 2 checks fail; with `PC_MODELS_END` at the storage's 16, 3; with a stride of 0x800, 22 |

The N64 IDO check (`tools/port/ido_check.sh`, x86-64 with IDO) had not been run; it covers the 109 changed files under
`src/` (`git diff --name-only db0fb90 -- src`: the 108 that reached the names and collision). Run with section
4's change (2026-10-29), it finds every object the same as 09da2f0's but in its debug sections.

**The test.** `assets_models` builds init_model_task, FinishResurrecting, multigo, raise_ob_res_pt (path_D169C),
modelgo, TargetCollRadius and collision with `PC_MAX_LINKS` 16, every record a sentinel. Each must write
and read the fields at their N64 offsets, step 0x808 and stop at the N64's 6: the 6 records' model clock; a resurrect's
end through a record pointer and another record's modelrun through what was `model_modelrun`, in two spellings; record 0's
obResPt through `D_8014AA14`; the pointer walk from the array's start; collidable through `D_8014AA3A`; the collision's
walk from record 3 visiting 4 and 5 and nothing after. It checks `struct pc_model`'s offsets too.

## 4. The car records

**What it is.** One 0x3B8-byte record per car, 6 of them, at 0x80152818-0x80153E68 in the game's BSS: MAT3's
`gGameCar` (`tCarData`, `gGameCar`; `include/game_types.h`'s `GameCar`), the car as the race shows and follows it:
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
| +0x008..+0x0A4 | `POS` (f32[3]; `POS_X`..`POS_Z` for a `Vec3` view), `UVS`, `UVS50` (f32[3][3]), `WHEEL_POS` (f32[4][3]) | 45 uses of `POS`: the cameras, the effects (blast, AnimateNewBlast), path_D169C, the drones' order (DoDrones); the skid marks; Update_MDrive through `car1_dr_pos` and `car1_dr_car_uvs` |
| +0x0E8..+0x108 | `FLAGS`, `PLACE`, `FINISHED`, `TIME`, `NODE`, `MPH`, `DISTANCE`, `F108` | the race order and end (CarReportsGameOver, PlaceUnlockedCars, FindWinner), the HUD and the results (results_table through `D_80152906`..`D_80152908`), the countdown, the brake lights through `D_80152900`, the cameras; m2c strides on `D_80152907` |
| +0x110 | `VISUALS` (struct pc_visual[21]) | StartEnvironment, startcar, StartWeapon (the weapon's visual) |
| +0x314..+0x364 (PC +0x3BC..+0x40C) | `NAV`, `FWD_PROGRESS`, `IR_COLOR`, `PREV_IR_COLOR`, `WEIGHT_INDEX`, `B358`, `STATE`, `DISABLED`, `SLOT`, `VIEW`, `SHEEN_ALPHA`, `HEADING` | maxpath, the stuck check (DetectDroneAbort), DoDrones, the deaths race (the countdown), m2c strides on `D_80152B70` and `D_80152B74`, GameCar's `pad0EC` windows |
| +0x380 (PC +0x428) | `INPUT` (void *; `INPUT_AS(T, i)` reads through it in the file's view) | model_iteration, the countdown, PrePlay, AnimateWing |
| +0x384..+0x3B4 (PC +0x430..+0x460) | `KIND`, `LIVES`, `HEALTH`, `BATTLE_FLAGS`, `B3A0`, `B3A2`, `POINTS`, `RELOADING_TIME`, `F3B0`, `F3B4` | battle mode: image B's status and points (runtime_b_status_scene, AddBattlePointInternal, animBattlePoints, animAmmoCounts), the battle reset initialize, FindWinner, results_table through `D_80152BBB` |

**Inventory.** 143 objects reference the names (nm): 138 game files under `src/`, context copies among them (three each
of CheckForStunt, DoDrones, path_D169C and AbortCar, two each of CarReportsGameOver,
PlaceUnlockedCars, CheckCPs, race_tail, the countdown, gameover_validOption, the audio_frame groups, StartWeapon,
MissileLock and image B's radius effect), and five under `nonmatching/`. They declare it through 133 views, all 0x468
bytes on the PC (view_audit; 137 with the structs of N64 size 0x3B8 reached through pointers). Only three offsets have
readers that disagree on signedness (+0xE8 and +0x38C int and unsigned, +0x358 signed and unsigned char), and those are
stores, bit tests and masks, so one type reads the same values; at +0x008 one view (AnimateNewBlastRing) has a
`Vec3` where the others have f32[3]. The image symbol tables name 141 addresses inside it (120 of them `g` names the
symbol file gives arcade globals, `gCarMass` at car 0's +0x7C, `gSkidPool` at car 3's +0xC0), 11 of which the C used,
and the C one more by its name's address, `D_801528F0`: `gGameCar` (a second symbol for the base), [0]'s fields
`D_801528F0` (seen from +0xD8), `D_80152900` (+0xE8), `D_80152906` (+0xEE), `D_80152907` (+0xEF), `D_80152908` (+0xF0),
`D_80152B70` (+0x358), `D_80152B74` (+0x35C) and `D_80152BBB` (+0x3A3), and car 1's `car1_objnum` (+0), `car1_dr_pos`
(+0x8) and `car1_dr_car_uvs` (+0x50), read with a stride of 0x3B8 (`CAR_OFS(0x3B8)` on the PC) or as an array of cars seen
from that offset.

**What holds a record's address.** Nothing in memory. A scan of the four images inflated from the retail cartridge found
no data word in 0x80152818-0x80153E68, and 231 code references (`lui 0x8015` with an offset in range: 205 in the game
image in 133 functions, 26 in image B in 17), every function one whose C is among the 143 objects. layout_check's
analysis of every live game file finds no pointer store whose value reaches the storage; the 123 call arguments that
pass a record's or a field's address (to 57 parameters: the cameras, the visuals, the shots, the sounds' positions) use
it and don't keep it. And a host address couldn't be kept in a u32 anyway: the storage was above 4 GB before too
(`PC_N64W` would stop a `RUSH_CHECK_CASTS` build). The record holds an address itself, its input record's
(`&gPlayers[i]` at +0x380, init_cars), a host pointer already, which the input records' decoupling has to
keep (section 5).

**What it reaches past its end.** Nothing. The next symbol, `sample_dma_pi_queue`, is a message queue (osPiStartDma's done queue
in sample_dma_next, sample_dma_thread_entry's); the three objects whose C names it are those two functions (one twice, in its
context copy), and the retail code's two references to 0x80153E68 are theirs. No C names the array plus 6 or
`&gGameCar[6]`; the walks of the array end at the cars in the race (`gGameCar + gNumCars`, image B's homing and
radius effects, the shots), which is `PC_CARS_END(n)` now. The array had storage of its own before, so a walk from a
neighbour (`our_drones`, `lastDotProd`) into it would have missed it before as after: bounds_check, offset_check and
layout_check list none. The window's 0x80152818-0x80153E68 was unused before (own storage leaves the window bytes alone)
and is still: filled with 0x5A after the loader ran (a throwaway build on the build host), it left 7 synthetic paths'
traces the same (race, race4p, battle2p, demo, stunt, pause, circuit), and none of its 5,712 bytes changed by the end of
any.

`layout_check.py --storage` before: 1,592 accesses on `gGameCar` (the names inside are on it, at their offsets), 69 of
them with no N64 position; after: 1,595 on `pc_cars`, 62 with none, and none of either off the reference (gen_data's
`GameCar[8]` before, `struct pc_car[12]` after).

**The size.** Every walk stops at 6: loops to 6 (game_reckon_all, BlinkResurrectingCars, SetCatchup, CheckCameraView,
model_iteration, DetectDroneAbort, dead_reckon_all, track_texture_load, update_game_data), to the cars in the
race (`gNumCars`: init_cars's init_cars, the shots and image B's walks; `gNumActiveCars`: InitCPTime,
race_tail, FindMaxpathIntervals, the race order), the players (`gNumPlayers`, `gNumPlayers`) or the windows. So
the storage is `PC_CAR_SLOTS` = the larger of 6 (`PC_N64_CAR_SLOTS`) and `PC_MAX_LINKS` (12): 12 records, 13,536
bytes, and the game still activates 6.

**The change.** `pc_game_types.h`: `struct pc_car` (and `struct pc_visual`, moved there from `pc_prelude.h` so that
port code including `pc_game_types.h` alone sees it), `pc_cars[PC_CAR_SLOTS]`, the accessors `PC_CAR_<FIELD>(i)` for
the indexed fields, `PC_CAR_INPUT_AS(T, i)` for the input pointer read through a view, and three `void *` ones for code
that keeps a record pointer in its own view: `PC_CAR(i)` (a record), `PC_CARS` (the array, for the pointer walks and
m2c's byte strides) and `PC_CARS_END(n)` (the end of the first n). The 143 files use them: about 280 field uses, 144
record addresses, 23 of the array and 4 ends rewritten by a script from each file's DWARF (the view's member path to
its N64 offset to the accessor: `gGameCar[i].battle.type` is `PC_CAR_WEAPON_TYPE(i)`, `gGameCar[i].position.x`
`PC_CAR_POS_X(i)`), and 18 by hand: the m2c strides on the names inside (`*((s8 *) &D_80152907 + (i *
CAR_OFS(0x3B8)))` is `PC_CAR_PLACE_LOCKED(i)`), GameCar's `pad0EC` windows (`PAD0EC(gGameCar[i].pad0EC, 0x26D)` is
`PC_CAR_DEAD(i)`), the three reads through the input pointer, and a second spelling of a record's address in
UpdateModel's group. Each file defines the N64 side with its own view (`#define PC_CAR_PLACE_LOCKED(i)
gGameCar[i].place_locked`, `#define PC_CAR_PLACE_LOCKED(i) *((&D_80152907) + (i * CAR_OFS(0x3B8)))`, `#define
PC_CARS_END(n) gGameCar + n`). Update_MDrive steps from car 1's names to car j + 1 by `CAR_OFS(0x3B8)`, so on the
PC they are car 1's record, position and +0x50 (`#define car1_dr_pos (*(s32 *) PC_CAR_POS(1))`). InitCPTime walks
the array as bytes (`PC_CARS + i * CAR_OFS(952) + 264`: GNU C's arithmetic on a `void *`). path_D169C's three copies
defined the array (a tentative definition, `Car gGameCar[6];`); on the PC they don't. `decoupled.txt`: `pc_cars
gGameCar 0x1650`, `gGameCar` and the 11 names; `extents.txt` loses the array's size. Eleven asset tests and
test_overlay_data define `pc_cars` where they defined `gGameCar` (test_cars and test_targets gave names inside it as
aliases, test_hudblit defined `D_80152907` alone). gen_data needed nothing new.

**What the tables say.** `data_symbols.txt` loses the 12 rows of `gGameCar` and the names inside; `data.c` loses its
own storage (6,768 bytes), its 12 `.set` lines, its 12 report names, its loader entry (419 to 418) and its 66 field
runs (7,803 to 7,737), and gains `pc_cars` (13,536 bytes, `.bss`); the later own storage's C names (`pc_own_N`) and
loader entries' run indices are renumbered, which is most of data.c's 521 changed lines. `data_layout.txt` lists it
under "Decoupled" with the 148 retired names (the image tables' 141, `gGameCar`, `D_801528F0` and 5 the symbol file
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
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, before and after (endian_check's 2 open value conflicts in 1 storage, init_view3's light-flash bytes, CARS.md 6, are the same, on `pc_cars` now); offset_check lists 332 untyped raw accesses, 9 more, all StartCoin's on its level-object node (whose lines lost `CAR_OFS` to the accessor; the node keeps its N64 layout, LAYOUT.md 9.4); layout_check's reviewed Update_MDrive read of the node handle's low half is no finding now (it reads through `struct pc_car`'s node); endian_check counts 3,486 puns, 647 more (each file's view and `struct pc_car` on one storage) |
| `layout_check.py --storage pc_cars` | 1,595 accesses, none off the reference (`struct pc_car[12]`) |
| `view_audit.py --symbols pc_cars --size 952 --ref StartWeapon.c` | 222 views (`struct pc_car` in the 143 objects, and the views reached through record pointers), 0 disagree (before, on `gGameCar,gGameCar`: 137, 0) |
| `bounds_check`, build host (GCC 14) | 8 new, none `clamped` or `two offsets`, all `view` on `pc_cars`, added to `bounds_reviewed.txt` as `[gcc14]`; 8 entries on the retired names and dead_reckon_all's `pad0EC` warning (gone with it) removed; then none new, 7 kept (as before). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 3 decoupled |
| `assets_carrecs` (16 records declared) | passes; with `PC_CARS_END(n)` at the storage's 16, 4 checks fail; with +0x358 read as the state, 3; with a stride of 0x3B8, 8; with the finish read as the place, 1; with Update_MDrive's `car1_dr_pos` at car 0, 3; with the health a byte early, 2 |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 277adc4: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_carrecs` builds InitCPTime, DetectDroneAbort with AreWeSolid, image B's MissileLock,
initialize, Update_MDrive and gameover_validOption with `PC_MAX_LINKS` 16, every record a sentinel. Each must
write and read the fields at their PC offsets, step 0x468 and stop where the N64 does: the race start's copy of
gNumActiveCars cars at m2c's byte strides from the array; 6 cars' progress cleared by index and car 9's (a slot past the
N64's 6) through its input record's node, a record pointer in the file's view, with +0x358 read through what was
`D_80152B70`; the homing walk over the race's `gNumCars` cars to their end, the nearer cars after them ignored; car
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
| 1 | the input records `gPlayers` | 4 x 76 = 304 B, 4 x 80 = 320 B | 73 | own storage; 5 names inside; `INPUT_SIZE` strides (POINTERS.md 10.3); the car record holds its address (`pc_cars[i].input`, a host pointer already): init_cars gives a human's car `&gPlayers[i]` by the car's slot i, and the humans have the first slots (0-3), so 4 records stay enough while they do |
| 2 | the car parts `gCarParts` (`tCarParts`) | 3,328 B by the gap, 0x40 a car | 20 | arena BSS; the extent is the gap, so the real count first |
| 3 | the race's per-car words: `rear_y`, `rear_z` (f32), `cur_acc` (12-byte V3), `gPrevPos` (f32[3]) | 16-72 B each, by the gap | 3-5 each | arena BSS; declared whole for OPTIMIZED.md 2.7; sizes by the gap, so the true counts first (`gPrevPos` is 6 cars' positions, InitCPTime copies them from the car records) |
| 4 | the maxpath weights `gTotalRaceStats` (`WeightRecord[12]`), stats `gTotalBattleStats` (`StatsC[8]`) | 1,152 B, 96 B | 9, 4 | arena BSS, already 12 and 8 long |
| 5 | the 13-slot arrays beside the roster: `gCarModelNums` (s16[13], each slot's engine wave), `gCarPaintJobInited` (u8, 52 B by the gap) | 26 B, 52 B | 4, 3 | arena BSS; walked with the roster's 13 |
| 6 | the human and drone lists `humans`, `drones`, `our_drones` (s16) | 12-16 B by the gap | 1-3 | arena BSS; small, sized by the gap |
| 7 | per car, initialized: `finish_cam` (u8[6]), the car palette descriptors `car_palettes` (`Slot24`, a u32 window address inside) | 6 B, 24 B | 6, 3 | `finish_cam` is ROM data, which decoupled.txt doesn't take yet |

Beyond data, the loops: the 6-car bounds in the code (`i < 6`, `MAX_LINKS` 6 in about a dozen files, `gNumCars`)
are what activates cars, and the per-window and per-player arrays (cameras, HUD) follow the players, not the cars.
For the model records they are listed in section 3 ("The size"), with `PC_MODELS_END`, the collision's end, which is
the N64's 6 too; for the car records in section 4 ("The size"). Two other things there stay as the N64 has them:
path_D169C raises only record 0's obResPt (through `D_8014AA14`), and CarReportsGameOver's human count compares a
record's address with record 0's. And the code that orders the cars keeps locals sized for the N64's 6, indexed by the
cars in the race: CarReportsGameOver's `place[7]` and `scores[6]`, DoDrones' `humans`, `drones`,
`total_drones` (`[MAX_LINKS]`) and `car_distance[MAX_LINKS][MAX_LINKS]`.

## 6. The effects' per-car arrays

**What they are.** Seven arrays of the effects, one entry per car slot, 6 on the N64 (game BSS), indexed by the car's
slot ([MORECARS.md](MORECARS.md) 2.3, item 1d of 10.2). None has pointers but the skid state, which was own storage
already for its segment pointer. 2026-10-29, on main 597098e.

| N64 | PC | N64 extent | PC storage | What | Read and written by (objects) |
|---|---|---|---|---|---|
| `gCarSmokeDir` | `pc_tire_fx` | u32[6], 24 B | `unsigned int[12]`, 48 B | each car's tire-effect latches, a bit per tire and effect (0x1-0x8 and 0x100-0x800 the tire puffs, 0x10-0x80 and 0x80000 the smoke, 0x1000-0x8000 the trails) | StartSmoke, StartWater, UpdateSmoke, InitWingSmoke, InitSmokeBlast (three copies), InitSmoke (the reset) |
| `gCarBlastSmokeClr` | `pc_smoke_shade` | u8[6], 8 B by the gap | `unsigned char[12]`, 12 B | the grey of its smoke, 175 fading by 3 a puff to 53 | StartSmoke |
| `gCarWingSmokePos` | `pc_trail_pts` | f32[6][6], 144 B | `float[12][6]`, 288 B | its two trails' last points | StartSmoke |
| `gBlastObjs` | `pc_blasts` | `tBlastStruct[6]`, 2,400 B | `struct pc_blast[12]`, 4,800 B | MAT3 gBlastObjs: the blast after a crash, four blast objects and a glow, each a scene object on a matrix inside the record | blast.c (StartNewBlast, StartNewBlast), AnimateNewBlast, InitBlast (InitBlasts) |
| `gBlastRingObjs` | `pc_blast_rings` | `tBlastRingStruct[6]`, 360 B to `gTempCircuitFirstTrack` | `struct pc_blast_ring[12]`, 720 B | MAT3 gBlastRingObjs: the ring, a scene object on its matrix | blast.c (StartNewBlastRing, inlined in StartBlast, StartBlast), AnimateNewBlastRing, InitBlast |
| `gNewSkid` | `pc_skids` | `WheelSlot[6][4]`, 2,208 B (own storage, 2,304 host) | `struct pc_skid[12][4]`, 4,608 B | the arcade's NewSkid per tire: the skid segment drawn (a host pointer into the segment pool `gSkidArray`) and its edges | skid_marks/group.c (UpdateSkids, StartSkid, UpdateSkid, ContinueSkid), InitSkids (the reset; two copies) |
| `resurrect_save_pos` | `pc_resurrect_mid` | f32[6][3], 72 B | `float[12][3]`, 144 B | the resurrect's midpoint (the arcade's update_resurrecting_car) | UpdateResurrectingCar |

**Inventory.** 16 objects reference the seven names (nm over `game_objects.txt`), all under `src/`, three of them weak
context copies (InitSmokeBlast's in the car_checkpoints and demo_pair groups, InitSkids's in slot18_alloc). They
declare the arrays through 13 types: `u32[]`, `int[6]` and m2c's scalar `s32` (read as `(&gCarSmokeDir)[i]`) for the
latches, whose readers differ in signedness only in bit operations and stores of 0; `tBlastStruct` (blast.c,
AnimateNewBlast, the same layout) and `s32` (InitBlast's byte offsets) for the blasts;
`tBlastRingStruct` (blast.c), `Effect` (AnimateNewBlastRing: the handle read through an `Id` union whose halves
are swapped on the PC, [ENDIAN.md](ENDIAN.md)) and `s32` for the rings; `WheelSlot[][4]` and `char[]` (InitSkids's
memset) for the skids; one each for the rest. The image symbol tables name no address inside any of them but the
rings', where the symbol file's `gCheckpointTimes` (+0x28), `gLapCounts` (+0x128), `gBestLapTimes` (+0x138) and
`gCurrentLapTimes` (+0x148) are guesses no C and no asm references; they are retired with the rings.
`layout_check.py --storage` before: 343 accesses on the seven (88, 8, 12, 136, 38, 55, 6), with the callees that
get one record's address and stay inside it (PitchUV and YawUV a blast object's matrix, Interpolate the
midpoint, and StopSkid, UpdateSkid, StartSkid, ContinueSkid and ZOID_UpdatePoly a skid); after: the same 343 on the
seven PC names, none off the reference.

**What holds an address.** A scan of the four images inflated from the retail cartridge finds no data word in any of
the seven ranges, and 40 code references (`lui` with a following low half in range): 20 to the latches
(StartSmoke 7, StartWater 4, UpdateSmoke 4, InitWingSmoke 2, InitSmoke 1, InitSmokeBlast 1 and
PlayGame 1, where the N64 build inlined InitSmokeBlast; on the PC it calls it), 2 to the smoke's grey, 2 to
the trails, 6 to the blasts (StartNewBlast 3, AnimateNewBlast 2, InitBlast 1), 6 to the rings
(AnimateNewBlastRing 4, StartBlast 1, InitBlast 1), 2 to the skids (UpdateSkids, InitSkids) and 2
to the midpoint (UpdateResurrectingCar): every function one whose C is among the objects above. What the game keeps
of them: MBOX_NewObject (ZOID_AddObject) keeps a blast object's, the glow's and the ring's matrix address as the
scene node's `owner`, which is pointer-sized in the node's PC layout ([POINTERS.md](POINTERS.md) 9) and read through
as a pointer; StartSkid keeps the segment's address in the skid state, a host pointer already. No `PC_N64W` takes
any of them, layout_check has no width finding (a pointer where the reference has a u32, or the reverse), and a
`RUSH_CHECK_CASTS` build runs every synthetic path without a stop. So unlike the matrix pool
([MORECARS.md](MORECARS.md) 2.7) none needs a window address.

**What it reaches past its end.** The retail code's references to each end address (the next symbol's) are that
symbol's own users (`gJustPlayerData`'s and `gRealJustPlayerData`'s SaveInit, `player_profile_handles`'s CreatePlayer and
ReadPlayer, `controls_scrollSpeed`'s controls_updateButtons, `gTempCircuitFirstTrack`'s ContinueCircuit and SetCircuitCode, the segment pool
`gSkidArray` that InitSkids sets up, `zoid_poly_vertex_buffers`'s ZOID_EndFrame), but one: InitSmoke's
`i < 6`, which IDO strength-reduced to an end pointer at `gJustPlayerData`; its C walks by index. No C names an array's
end. Nothing reaches the old ranges: the six that were arena BSS (3,008 bytes) filled with 0x5A after the loader ran
(a throwaway build on the build host) left every synthetic path's trace the same, and none of their bytes changed
by the end of any.

**The size.** Every use reaches one car's entry by the car's slot (the puff's `car`, the visual's `slot`, the node's
`index`, the model's net node) but three resets, which cleared the N64's 6: InitSmoke the latches
(`i < 6`), InitBlasts the blasts and rings (`i < 6`, strides 0x190 and 0x3C: each blast object's and the glow's
handle, the ring's, to -1) and InitSkids the skids (2,208 bytes; 6 x 4 x 0x60 on the PC before). LoadTrack calls
all three (`loadtrack.c:237-239`). On the PC they clear every slot, `PC_MAX_LINKS` and `sizeof (pc_skids)`, not the
race's cars: a slot left out keeps a past race's latches, and a blast object InitBlasts left at 0 would have
StartNewBlast remove scene object 0 (`if (b->mbIdx != -1) ZOID_RemoveObject(b->mbIdx)`). These are the two rows of
[MORECARS.md](MORECARS.md) 3.1 in these files (item 3 left them to this one); no loop in them walks the race's cars.
So the storage is `PC_MAX_LINKS` (12) entries each. InitBlasts takes 50 matrices from GetMatrix's pool whatever the
cars: they are StartBlast's ring of 50 (`gBlastMats`, `gBlastMatCnt` wraps at 50), not per car, and a car's own blast
and ring matrices are inside its records; the pools' item needs nothing more from this one.

**`gCarWings_players`** ([MORECARS.md](MORECARS.md) 2.6) stays as it is. It is rows 1-4 of `gCarWings` (s8[5][13]: 65 bytes
and 3 of padding to `gWingSizes`), which image A's car screen (carscr_animText) reads as `gCarWings[p + 1][...]` for
player p: a setting per player and car type. Its three readers index it by slot x 13 + the car type, but only for
humans: the wings' set-up (StartWingFlame, StartWing in the audio_frame groups) returns for a car this node drives
(`we_control`, set for a drone, owner 0), and the trails' colour (StartSmoke's types 6 and 7) comes only from
the wing's animator (AnimateWing) with one player. The humans have the first slots, 0-3, with any number of cars, so
no slot past 3 reads it (a slot 4 or 5 would read `gWingSizes`'s float bytes, which the PC holds swapped).

**The change.** `pc_game_types.h`: the four arrays, `struct pc_uv_matrix`, `struct pc_blast_obj`, `struct pc_blast`,
`struct pc_blast_ring` and `struct pc_skid` (with MAT3's and the arcade's field names, which blast.c's,
AnimateNewBlast's and skid_marks' views have), the record arrays, and `PC_BLASTS`, `PC_BLAST_RINGS` and
`PC_SKIDS` as `void *`. Each of the 16 files changes its declaration only: under `TARGET_PC` its N64 name is the PC
storage. In 8 files it is the PC array (`#define gCarSmokeDir pc_tire_fx`, `#define gBlastObjs pc_blasts`: their views
have the PC declaration's element type, or its field names and layout); AnimateNewBlastRing, whose `Effect` names
the ring's fields otherwise, sees the array as `PC_MAX_LINKS` of its own element type (`#define gBlastRingObjs (*(Effect
(*)[PC_MAX_LINKS])PC_BLAST_RINGS)`); m2c's scalars in InitWingSmoke, InitSmokeBlast and InitBlast are the
array's first word (`#define gCarSmokeDir (*(s32 *)pc_tire_fx)`, read as `(&gCarSmokeDir)[i]` or through a byte
pointer), and InitSkids passes `PC_SKIDS` to memset. Not per-use accessors as in sections 2-4: these arrays are
reached only by index, as whole records in each file's own view (one of them an endian-split union), so a field
accessor would need a name per view; this way every use keeps its N64 tokens and its line, and since each is an
array of `PC_MAX_LINKS` (the view too: an index past it traps, GCC 14 at -O0 with `-fsanitize=bounds`),
`-fsanitize=bounds` checks each index against the PC's count ([MORECARS.md](MORECARS.md) 10.3). A first try saw
blast.c and AnimateNewBlast through the view cast too; GCC folds `&view[slot].blastObjs[i]` there into a byte
offset from `&pc_blasts`, which layout_check doesn't follow (49 accesses on `pc_blasts`, against 136 on
`gBlastObjs`), so their structs got MAT3's names instead. Inside functions three lines change: the two resets' bounds
(`PC_MAX_LINKS`, which InitSmoke and InitBlast define as 6 on the N64) and the weak copy of
InitSkids's byte count (`SKID_TABLE_BYTES`, as the function's own has; `sizeof (pc_skids)` on the PC).
`decoupled.txt`: seven rules, with the N64 extents above. test_casts and test_pool define the PC names where they
defined the N64 ones.

**What the tables say.** `data_symbols.txt` loses the seven rows; `data.c` loses six `.set` lines, the skid state's own
storage (2,304 bytes) with its loader entry and 12 field runs, and seven report names, and gains the seven (10,620
bytes, `.bss`); the later own storage's C names and the loader entries' run indices are renumbered, which is most of
data.c's 412 changed lines (234 with the own storage's C names normalized). gen_data's check: 1,770 symbols at their
N64 addresses (6 fewer), 396 own (1 fewer), 10 decoupled. The synthetic cartridge is the same file.

**What the paths run.** Counted with gdb (a breakpoint on each function that never stops, its hit count at the end)
on the branch's Debug program, Pi: on the retail cartridge the scripted races start skids (StartSkid 622 times in
`race`, 884 in `race2p`, 262 in `pause`) and smoke (StartSmoke 789, 992 and 462 times), and crash nowhere, so no
blast; on the synthetic cartridge the races crash (StartNewBlast and StartBlast 24-60 times in `race`, `demo`, `drive`
and `circuit`, the ring's animator 408-1,014 frames, the resurrect 36-108) and smoke (2,262-5,862), and start no
skid. So the retail scripts are the skids' proof and the synthetic paths the blasts'. Neither runs StartWater (a
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
| `view_audit.py`: `pc_blasts --size 400 --tags tBlastStruct,pc_blast`, `pc_blast_rings --size 60 --tags tBlastRingStruct,Effect,pc_blast_ring`, `pc_skids --size 92` (reference: the PC structs) | 3 views, 0 disagree (before, on `gBlastObjs`: 2, 0); 4, 0 (2, 0); 22, 16 disagree, every one of them another record of 92 bytes (the tires', the menu's, the checkpoints' header...), as before (19, 16); skid_marks' `WheelSlot` agrees |
| `bounds_check`, build host (GCC 14), and the Pi's mingw-w64 12 (on the Windows build) | 4 new, the same on both, none `clamped` or `two offsets`, all `view` (InitBlast's InitBlasts on `pc_blasts` and `pc_blast_rings`, InitWingSmoke's and InitSmokeBlast's word of `pc_tire_fx`), added to `bounds_reviewed.txt` as `[gcc14,mingw12]`; the 2 entries on the retired names (InitBlast's `gBlastRingObjs`, InitSmokeBlast's `gCarSmokeDir`) removed; then none new on either (GCC 14 doesn't find 9 reviewed keys, as on the base, and mingw-w64 12 10: kept for the other compilers). GCC 13 (CI's runner) not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| the six old arena ranges filled with 0x5A after the loader (a throwaway build, build host), against the branch | 19 of 19 paths the same trace; 0 of the 3,008 bytes changed in any |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; `gen_data.py --check --pe`: OK, 10 decoupled |
| `assets_fxcars` (16 slots declared, 12 cars) | passes; with each mutation it fails: the latches' reset at 6 (41 checks), InitBlasts at 6 (78), the skid reset at the N64's 6 cars (1, then a fault on a stale segment pointer), the skid table one tire off (2), the blasts read as the rings (a fault), the rings one car off (2), StartWater's latches one car off (3), the smoke's grey one car off (3), the trails one car off (2), the midpoint one car off (2), InitWingSmoke's latch one car off (1) |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, cba8da7: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_fxcars` builds the 13 files above that link (all but the weak copies), with StopSkid, the pools'
own get, free and set-up, and the game's Random, with `PC_MAX_LINKS` 16, and runs a race of 12 cars, every entry of
the seven arrays a sentinel first:
the race start's reset clears all 16 latches; InitBlasts takes 50 matrices and marks all 16 blasts' five objects and
rings empty, their matrices left alone; each of the 12 cars' tire puffs sets its own latch bit and takes two puffs, a
latched tire takes none, and UpdateSmoke ends all 24 and clears each car's bit; car 11's smoke sets its latches and its
grey (175, 172, two latched frames, 169) and no other car's; its trails keep their points in its own six floats, and
InitWingSmoke and InitSmokeBlast clear its trail and smoke bits; its blast's four objects and glow are scene objects
on matrices inside its own record, which AnimateNewBlast moves with the car; its ring the same, followed by
AnimateNewBlastRing; InitSkids clears the whole skid table, and 12 cars x 4 tires start 48 distinct segments
at their tires, continue and stop them, slots 12-15 untouched; car 11's resurrect midpoint lands in its own slot.
test_casts checks InitBlasts' 50 matrices and its -1s in all `PC_MAX_LINKS` blasts and rings of the game build's 12.

**Not done, and for the other items.** GCC 13's bounds_check (CI's runner) not run. The pools (item 2): a car's blast
takes five scene objects and its ring one, from the scene node pool (`ObjInstanceList`, 600), its smoke and tire puffs
come from the 100 puffs of `gSmokeList` (a full pool gives none, and the caller stops), and the skids from the 100
segments of `gSkidArray` (48 at most with 12 cars x 4 tires; a full pool steals the farthest); none was measured with
12 cars. The weak copy of InitSkids in slot18_alloc keeps the N64's pool views (36-byte elements), as before: it
is never linked.

## 7. The cars' sounds, checkpoints and finish cameras

**What it is.** Eight of [MORECARS.md](MORECARS.md) 2.3's per-car arrays, items 1a (sound), 1c (checkpoints and
places) and 1e (the finish cameras) of its 10.2, 6 slots each on the N64, indexed by a car's slot. Four are the game
image's initialized data, which a decoupled rule couldn't take before (below).

| N64 | What | N64 storage | PC |
|---|---|---|---|
| `gEngineSounds` | the engine sound sets (`Player84`, `SndSet`): the car type's engine table, `&gEngineSpecs[type]`, and four sound records: the two engine layers, one nothing uses, and the random sound (MORECARS.md 6) | 6 x 84 B at 0x80140420, BSS; own storage before (its pointer: 88 B a car) | `pc_engine_sounds`, `struct pc_engine_sound[12]`, 1,056 B |
| `gPaveSkidSounds` | the tire screech's record (`State20`, `SndRec`, `LayerState`: a handle and the last level and style sent) | 6 x 20 B at 0x80140640, BSS | `pc_screech_sounds`, `struct pc_sound_rec[12]`, 240 B |
| `gCarActive` | the car's sounds started: StartCarSounds sets it, StopCarSounds clears it, the engine update needs it | 6 B at 0x8010FFC4 (8 by the gap), game image data | `pc_car_sounds_on`, `signed char[12]`, from the image |
| `gCarUpdatedLastTime` | the engine update's phase: UpdateCarSounds skips a car while it is 1 and toggles it, so each car's engine is updated every other frame and half the cars each frame; StopCarSounds sets it to `(slot & 1) == 0` | 6 B at 0x8010FFCC (8 by the gap), game image data | `pc_engine_phase`, `signed char[12]`, from the image, continued |
| `gPrevPos` | each car's position at the last CheckCPs, for the distance it drove; InitCPTime sets it at the race's start | 6 x 12 B at 0x80152218, BSS | `pc_cp_last_pos`, `float[12][3]`, 144 B |
| `lastDotProd` | each car's distance to its next checkpoint's plane at the last CheckCPs, which times a crossing | 6 x 4 B at 0x801527E8, BSS | `pc_cp_plane`, `float[12]`, 48 B |
| `finish_cam` | a human's finish camera: 1 circling since the finish (PassedCP), 2 frozen 10 s later (circle_camera_around_car); CheckCameraView turns the window's view to it; init_views defined it and clears it, InitCheckpointData clears the first two | 6 B at 0x80110680 (8 by the gap), game image data | `pc_finish_cam`, `unsigned char[12]`, from the image |
| `finish_cam_stime` | the finish camera's start time, then the time it froze at | 6 x 4 B at 0x80110668, game image data | `pc_finish_cam_time`, `float[12]`, from the image |

With them, CheckCameraView's drone cameras (`j < 6`, MORECARS.md 3.1). CalcPlaces' locals (MORECARS.md 2.5), which
this item had too, are item 3's on main (`CARS_N`, MORECARS.md 3.1 "Done"); its test runs CalcPlaces with 12 cars.

**Inventory.** 13 objects reference the eight names (nm over `game_objects.txt`, build host, 8ff5be1): for the sound
`car_sounds/StartCarSounds.c` (StartCarSounds), `car_sounds/UpdateCarSounds.c` (the engine update) and `StopCarSounds.c` (StopCarSounds)
with its `slot18_alloc` copy; for the checkpoints `CheckCPs.c` (CheckCPs) with its `demo_pair`
copy and `InitCPTime.c`; for the cameras `PassedCP.c`, `circle_camera_around_car.c`, `CheckCameraView.c`,
`init_views.c` and `InitCheckpointData.c` with its `loadtrack` copy (the three copies are weak). Their views:
of `gEngineSounds` four, all 84 bytes on the N64 and 88 here but one: the table is a `Definitions64 *` (StartCarSounds.c), an
`EngTbl *` (UpdateCarSounds.c), an `intptr_t` (StopCarSounds.c) and an `s32` in the `slot18_alloc` copy, which never runs
(view_audit's one disagreement before); the sound records are 20 bytes everywhere, a handle and four floats (UpdateCarSounds.c
names +4 `level` and +8 `style` and pads the rest); the flags and the phase are `s8`; the cameras' state is `s8` in
three files and `u8` in two (init_views's `u8[6]`, InitCheckpointData's `Flags {u8 first, second;}`), stores
and tests of 0, 1 and 2, so one `unsigned char` reads the same; the floats are `f32`, InitCPTime's `s32` with
m2c's 12-byte stride and `f32` stores. The image symbol tables name one more address inside them, `D_80110681` (`finish_cam[1]`, InitCheckpointData's
`.second`), which no C names. No file reaches a neighbour from them or them from a neighbour (bounds_check,
offset_check and layout_check list none).

**What they reach past their ends.** The next symbols are each their users' own: `gNumLights` (s16, the lights mode:
InitWaves, zoid_reset_lights, ZOID_Reset, ZOID_EndFrame's two copies, PrePlay) after the engine
sound sets, `gLights` (`Lights2 *`, the same files) after the screeches, `sTrackSongs` (each track's song, s32[19]:
StartGameMusicInternal in game_music and three copies, mode_init) after the phase, `D_80152260` (zlib's `dist_code`) after the
positions, `gFirstLapDistance` (the route's length: route_cross, FindMaxpathIntervals) after the plane distances, `view0_eye_height`
(a camera height, update_camera) after the cameras; the phase and the cameras' state follow the flags and the times.
No C names an array plus 6 or the next symbol as an end.

**What holds an address.** Nothing in memory. A scan of the four images inflated from the retail cartridge (on the
Pi) found no data word pointing into the eight ranges or at their ends, and, following each `lui` register along
straight-line code (an index added to it or not), 31 code references into them, all in the game image, in 12
functions, each one whose C is among the 13 objects (LoadTrack has InitCheckpointData inlined, which the PC calls), and
24 at the end addresses, in the neighbours' users above. No `PC_N64W` store or u32 field takes an address of them:
layout_check finds no pointer store whose value reaches them; the records' addresses go to InitSoundState
and StopSound (which reset and release a record), update_skid_emitter and SetSoundVP (which pass its
handle on), and they keep none; CheckCPs' `prev` is a local; the sound manager is given handles and positions (the
model's +0x22C, `unitmatrix_origin`), not records. StartCarSounds.c's `object = (s32)&gCarActive[player]` truncates the flag's
address into an int that is overwritten before any use ([CASTS.md](CASTS.md) 5's dead store, the same on the PC).
A `RUSH_CHECK_CASTS` build ran every synthetic path without a stop.

**The size.** On the N64 every reader indexes them by a slot below 6: the sound by the model's slot (StartCarSounds for
each car, the engine update for each every frame, StopCarSounds), the checkpoints by CheckCPs' `index < gNumCars`
and InitCPTime's `i < gNumActiveCars`, the finish cameras by a human's slot (PassedCP, owner 6), a
camera's car (circle_camera_around_car's `object->car`) or a window's player (CheckCameraView's `pl->index`); init_views
clears 6. So each is `PC_MAX_LINKS` (12) long on the PC. init_views clears every slot it has (`FINISH_CAM_SLOTS`, `PC_MAX_LINKS`: a clear, MORECARS.md 3.2) and
CheckCameraView places the drone cameras of `PC_RACE_CARS` cars (`RACE_CARS`); with 6 cars the slots after 6 are never
set, so the default is the N64's.

**The four the image initializes.** `gCarActive`, `gCarUpdatedLastTime`, `finish_cam_stime` and `finish_cam` lie in the game image's
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
with its own view (`#define PC_ENGINE_SOUND(i) &gEngineSounds[i]`, `#define PC_ENGINE_SOUND_HANDLE(i, k)
gEngineSounds[i].rec[k].handle`, `#define PC_SCREECH_LEVEL(i) gPaveSkidSounds[i].level`, `#define FINISH_CAM_FIRST
finish_cam.first` in InitCheckpointData, and InitCPTime's strides, `#define PC_CP_LAST_POS_X(i) *(f32 *) ((u8 *)
&gPrevPos + i * 12)`); init_views defines `finish_cam` on the N64 only (on the PC it was a
declaration, WINDOWS.md). InitCPTime had `gPrevPos` open-ended for [OPTIMIZED.md](OPTIMIZED.md) 2.7 (three
stores a car in a loop, the stick's shape); `pc_cp_last_pos` has its true size, which GCC reads correctly.
`decoupled.txt`: the eight rules, four with `init=image`, the phase's with `period=2`; `extents.txt` loses
`finish_cam`'s 6. `assets_cars` and `assets_carrecs` define `pc_cp_last_pos` where they defined `gPrevPos`.

**What the tables say.** `data_symbols.txt` loses the 8 rows (2,209 lines to 2,201); `data.c` loses `gEngineSounds`'s own
storage (528 B), its loader entry (418 to 417) and 24 runs, the 7 `.set` lines of the others, `finish_cam_stime`'s arena run
and the 8 report names, and gains the 8 storages (1,572 B, `.bss`), `pc_inits` (4 rows) and its 4 runs; the later own
storage's C names and the run indices are renumbered: 1,317 changed lines, 56 with those normalized. `data_layout.txt`:
one own storage fewer, two pointer and two width conflicts fewer (the views of `gEngineSounds` and `gPaveSkidSounds` above),
the 8 under "Decoupled", and `pc_race_cars` under "Not placed" (CheckCameraView calls it, as `pc_os_idle` and the other
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
| `view_audit.py --symbols pc_engine_sounds --size 84 --tags Player84,SndSet,pc_engine_sound --ref StartCarSounds.c`; the same for `pc_screech_sounds` (`--size 20`, `State20,SndRec,LayerState,pc_sound_rec`) | 5 views, 0 disagree; 6 views, 0 disagree (before, on the N64 names: 4 and 1, the `slot18_alloc` copy's; 4 and 0) |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`, 9 reviewed keys kept (as on main); `bounds_reviewed.txt` had no entry on the eight names. GCC 13 and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure, headless: no SDL2 fetched) | builds; `gen_data.py --check --pe`: OK, 11 decoupled. (Before d616933 the PE link of `assets_percar` failed: a weak copy, StopCarSounds.c's StopSound, can't stand for the symbol on PE; the test links the owner now) |
| `assets_percar` (16 slots declared, 12 cars) | passes; with the phase read as the sounds-started flag, 73 checks fail; with the engine sound sets at the N64's stride of 84, 131; the last positions 6 long, 12; the plane distances 6 long, 18; the screech records 6 long, 12; the finish times 6 long, 4; init_views clearing 6, 1; the drone cameras for 6 cars, 5; StartCarSounds' flag 6 long, 6. CalcPlaces' arrays 6 long overrun its stack, which depends on the build (12 checks failed in one, none in another); the UBSan build (build host, SANITIZE.md 1.4) stops it, with main's `CARS_N` at 6: index 6 out of bounds for type `s16 [6]` |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 7cbf1d4: the branch's game C, merged with main) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |

**The test.** `assets_percar` builds StartCarSounds, the engine update (UpdateCarSounds, UpdateEngineSound), StopCarSounds,
InitSoundState, StopSound, InitCPTime, CheckCPs, CalcPlaces, PassedCP,
circle_camera_around_car, CheckCameraView, init_views and InitCheckpointData with `PC_MAX_LINKS` 16, every slot a sentinel,
and runs them on 12 cars, a human in slot 0 and 11 drones: 12 engine sound sets started (the table, the two layers' handles from
AudPlayEffectVP or AudEmitEffect, record 3 and the screech reset, record 2 untouched); two frames of the
engine update, each car run in one of them by the image's alternation, a drone's run starting its random sound and
sending each layer's pitch and volume, the human's starting its screech; 12 stopped (records 0, 1 and 3 and the screech
released and reset, the flag cleared, the phase `(slot & 1) == 0`); 12 positions copied at the start; 12 checkpoint
crossings reported with their interpolated times, the last lap ended, the humans (slots 0 and 9, a slot past the N64's
6 among them) given the finish camera, the 12 ranked by distance; a finish camera circled and frozen, a window turned
to it, the drone cameras placed for 12 cars and then for the N64's 6; every finish camera cleared, then the first two.
Nothing past car 11 is touched.

**Left to the other items.** Section 5's rank 3 (`gPrevPos` among the race's per-car words) and rank 7 (`finish_cam`,
initialized data) are done here; the other per-car words there (`rear_y`, `rear_z`, `cur_acc`) are per
window (MORECARS.md 2.4). The 6s left in these files are not car counts: the owner codes (`== 6`) in CheckCPs and
PassedCP, and InitCheckpointData's two slots. The finish camera is only ever set for a human (owner 6),
and the humans are slots 0-3, so it stays per player in practice; circle_camera_around_car reads it for any car whose camera
shows view 5. The engines of 12 cars ask MusyX for about twice the voices (MORECARS.md 6); the sound records only hold
their handles.

## 8. The drones

**What it is.** [MORECARS.md](MORECARS.md) 10.2 items 1b and 5: the arrays the drones keep per car, the locals that
order them, and the schedule that updates them. Eight arrays in the game's BSS, 6 entries on the N64, each a plain array
(no record, no pointer):

| N64 | PC | Type | MAT3 / arcade | Indexed by | Users (C) |
|---|---|---|---|---|---|
| `D_801527D8` (0x801527D8, 12 B; the own `si_lock_msg` at +12) | `pc_drones` | short[12] | `drones[]` | the drones in the race, `num_drones` of them | munge (munge_gLink_data) and PrePlay's restart write it; MP_IntervalPos (IntervalPosChange) reads it for the camera's drone pick (`current_drone`) |
| `D_80152808` (0x80152808, 12 B, 16 by the gap; the car records after it) | `pc_our_drones` | short[12] | `our_drones[]` | the drones this node drives, `num_our_drones` | munge and PrePlay write it; update_drone_models walks it; UpdateModel takes a retired drone out |
| `lastwheel` (24 B; `cur_drone`, update_drone_models' cursor, at +24) | `pc_mp_steer` | float[12] | PS_PurgeFile's steer | car slot | maxpath (AdjustSteer); InitPathData clears 6 |
| `lastbrake`, `lastthrottle` (24 B each) | `pc_mp_brake`, `pc_mp_gas` | float[12] | AdjustSpeed's brake, gas | car slot | maxpath (AdjustSpeed); InitPathData clears 6 |
| `last_dir` (72 B; HAL_DrawText's lastFontXpos, then `num_humans`, after it) | `pc_mp_dir` | float[12][3] | avoid_areas' last direction | car slot | maxpath (AvoidAreas) |
| `last_save_dir` (72 B) | `pc_mp_save_dir` | float[12][3] | avoid_areas' last direction to the target | car slot | maxpath (AvoidAreas) |
| `carsetup_flag` (6 B, 8 by the gap; the controllers' `gAutoEdges` at +8) | `pc_carsetup_flags` | signed char[12] | carsetup_flag | car slot | CarSetup sets a car's when it starts the car's visuals; PrePlay clears 6 |

The last one isn't in MORECARS.md 2.3: PrePlay's `for (i = 0; i < 6; i++) carsetup_flag[i] = 0` (its line 374
at c1ab1d7) is a per-car bound, and CarSetup reaches the flag as `(u8 *) &carsetup_flag + slot` for every active car, so a
9th car's flag would write the first controller's pressed buttons. The image symbol tables give the N64 extents: past
the brake, the gas and the two directions sit HAL_DrawText's statics (HAL_DrawText, `hal_drawtext.c`:
lastFontColor at 0x80153F60, lastTrans 0x80153F80, lastFontXpos 0x80153FD0, lastFontYpos 0x80154180), which are host
statics of that file on the PC, so `data_symbols.txt`'s 32 bytes for `lastbrake` and `lastthrottle` were the gap, not the
arrays; on the N64 a 7th car's brake would have changed the text's colour state. Not moved: the human list
`humans` (16 B by the gap, written by munge, read by nothing) holds the players, 4 at most; the counts
(`num_drones`, `num_our_drones`, `num_humans`: s16) hold 12; the schedule's state (`sched_to_use`, `new_sched_to_use`,
`sched_index`, `cur_drone`) is scalars.

**Inventory.** nm over `game_objects.txt` (main, 8ff5be1, build host; 597098e and c1ab1d7 change no use of them):
`drones` 3 objects (munge_gLink_data, maxpath/group.c, nonmatching/display_list_flush.c), `our_drones` 3
(munge_gLink_data, update_model/group.c, PrePlay), `lastwheel`, `lastbrake`, `lastthrottle` 2 each
(InitPathData, maxpath_controls), `last_dir`, `last_save_dir` 1 each (maxpath_controls), `carsetup_flag` 2 (CarSetup,
PrePlay): 6 objects, no context copies. Every file declares them as what they are (s16 lists, f32 and f32[3]
by slot, bytes), but two through a scalar and a cast: InitPathData's `((f32 *) &lastwheel)[i]` (its m2c block
declares `lastbrake` and `lastthrottle` as s32, stored as f32) and CarSetup's `(u8 *) &carsetup_flag + slot` (declared s32,
stored as s8). The image symbol tables name one address inside the extents, `D_801439FE` (+6 of the flags:
PrePlay's loop end), which no C names. The other names nearby are neighbours used by their own names:
`cur_drone` (the cursor), the HAL_DrawText statics, `num_humans`, `si_lock_msg` (a message queue's buffer, 11 objects),
`last_save_dir` as SetPakError' end of `gPakError` (by index on the PC), `carsetup_flag` as hud_updateFrame's end of
`TrackTimeStr` (likewise).

**What holds an address.** Nothing in memory. A scan of the four images inflated from the retail cartridge (in memory,
on the Pi) found no data word in any of the ranges, and code references (a `lui` of the high half and an offset in
range) only in functions whose C is among the 6 objects: munge, MP_IntervalPos, MaxPathControls (AdjustSpeed and
PS_PurgeFile inlined), avoid_areas, UpdateModel, model_iteration (update_drone_models inlined), InitPathData,
CarSetup, PrePlay; besides them, the ends of the arrays before two of them (SetPakError' of
`gPakError` at 0x80154138, hud_updateFrame's of `TrackTimeStr` at 0x801439F8) and HAL_DrawText's 4 references to each of
its statics.
No C takes an address to keep: the casts are local, and no `PC_N64W` or u32 field names them. A `RUSH_CHECK_CASTS` build
ran every synthetic path without a stop (below). So nothing needs a window address (MORECARS.md 2.7).

**What it reaches past its end.** Nothing: no C names an array plus 6, or the next symbol as an end. The ends are
`si_lock_msg`, 0x80152814 (no name), `cur_drone`, the HAL_DrawText statics and `D_801439FE`; the retail references to
0x80153F40 (6) are update_drone_models' cursor's (init_cars, PrePlay, UpdateModel,
model_iteration), and those to the statics HAL_DrawText's. bounds_check, offset_check and layout_check list no access
from a neighbour into the ranges or out of them (below).

**The size.** The lists hold every drone of a race of `PC_MAX_LINKS` cars (the attract demo's are all drones), the
controls and flags one entry per car slot: `PC_MAX_LINKS` (12) each, 492 bytes in all. The walks: munge and
PrePlay append to the lists for the active cars (`gNumActiveCars`), update_drone_models walks `num_our_drones`,
MP_IntervalPos `num_drones`, maxpath indexes by the model's slot, CarSetup by the active cars' slots; the two clears
(InitPathData, PrePlay) were `i < 6` and are `i < PC_MAX_LINKS`, every slot.

**The change.** `pc_game_types.h`: the 8 declarations and their accessors (`PC_DRONE(j)`, `PC_OUR_DRONE(j)`,
`PC_MP_STEER(slot)`, `PC_MP_BRAKE`, `PC_MP_GAS`, `PC_MP_DIR(slot)` and `PC_MP_SAVE_DIR(slot)` (a row of 3),
`PC_CARSETUP_FLAG(slot)`). The 6 files use them, each defining the N64 side (`#define PC_MP_DIR(slot)
last_dir[slot]`, `#define PC_MP_STEER(i) ((f32 *) &lastwheel)[i]`); CarSetup keeps its spelling and on the PC
defines `carsetup_flag` as `PC_CARSETUP_FLAG(0)`, the first flag (FinishResurrecting's way with `model_modelrun`, section 3).
`decoupled.txt`: the 8 lines, sizes the N64 extents (0xC, 0xC, 0x18 three times, 0x48 twice, 0x8). In the same files,
the bounds that walk them or the cars (MORECARS.md 3.1, 2.5): munge's clear of the model records after the race's,
`i < MAX_LINKS`, is `PC_MODEL_SLOTS` (its owner test, `owner < MAX_LINKS`, keeps 6: an owner code, MORECARS.md 1.4);
model_iteration's speed walk `v != 6` is `PC_RACE_CARS`; DoDrones (DoDrones and its car_checkpoints and
demo_pair copies) sizes its locals (`humans`, `drones`, `total_drones`, `car_in_conflict`, `cars_in_order`, `place`,
`car_distance[][]`) by its `MAX_LINKS`, which is `PC_MAX_LINKS` on the PC (it has no owner-code use; its loops walk
`gNumActiveCars`). assets_roster, assets_audiocmds define the PC storage where they defined the N64 names.

**The drone schedule.** `update_drone_models` updates, each model iteration, the next `drone_sched[row][k]` of our
drones round the list (`cur_drone`), k counting up until -1, which ends a round and takes the next round's row
(`new_sched_to_use`). `drone_sched` is `D_80120E74`, ROM data, `Row3[12]` of s16. init_cars picks row
`num_our_drones`, plus 7 with two humans or more; a drone's retirement takes 1 off the next round's row (UpdateModel),
and a human's, of two, 7, to the one-human rows. Rows 0-6 are 0-6 drones with one human or none,
7-11 0-4 drones with two: 7-12 drones with one human would read the two-human rows, and 5-10 drones with two would
read past the table (the spike's UBSan, MORECARS.md 2.6). In the retail cartridge no row has more than two groups, so a
round is at most two model iterations, and the rows with two split the drones as evenly as they can, the larger part
first (read on the Pi with gdb; the values stay there). The PC keeps every N64 row as the game's data has it and adds a
rule past them (update_model/group.c, `pc_drone_sched`):

- **Rows.** The second kind starts at `PC_DRONE_SCHED_TWO` = `PC_MAX_LINKS` + 1 (13), so a row is n for n drones with
  one human or none and 13 + n with two, n up to 12, and the two kinds never meet. init_cars's `+= 7` and
  UpdateModel's `>= 7`, `-= 7` use it; PrePlay's restart (every car a drone, row `num_our_drones`) and the
  drone's `-= 1` stay as they are.
- **The N64's rows.** For n up to 6 (one human) and up to 4 (two), entry k is `drone_sched[n].v[k]` and
  `drone_sched[7 + n].v[k]`, read as the N64 reads it, k unbounded as there.
- **Past them.** A round has as many iterations as the N64's last row of the kind (row 6, row 11) has groups before its
  -1; iteration k updates n / groups drones, plus one for the first n % groups: each drone once a round, and a round as
  long as the N64's longest, so a drone's model step (the ticks since its last update) stays what the N64 gives its
  most crowded race. With the retail cartridge's rows, 7-12 drones are two iterations a round, half the drones each;
  with the synthetic cartridge's ({n, -1}), one, every drone.

So the rule keeps a drone's step, not the number of drones an iteration: up to 6 an iteration on the PC. It reads
only the game's data, so nothing from the cartridge is in the build. init_cars.c is item 3's file for its
other loops; the row choice at its line 388 (c1ab1d7) changed here (the car records' item left it, and item 3 doesn't
touch the line). With 6 cars every row is one the N64 has, read as before: the default is the N64's schedule.

**What the tables say.** `data_symbols.txt` loses the 8 rows; `data.c` loses their 8 `.set` lines and 8 report names
(2,170 to 2,162 at c1ab1d7) and gains the 8 storages (`.bss`, 492 bytes); 45 diff lines. The loader's tables are the
same (416 entries, 7,704 runs), and no ROM word pointed into the ranges. Debug and RelWithDebInfo give the same files.

**Proof** (`game.cars` at `original`; "before" is a build of main c1ab1d7 and "after" the branch merged with it,
unless a row says otherwise).

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
| `layout_check.py --storage` on the 8 | 58 accesses before (on the N64 names) and after (on the PC names), the same per storage at the same offsets (one, PrePlay's clear of the flags, unplaced in both) |
| `bounds_check`, build host (GCC 14) | 1 new, not `clamped` or `two offsets`: CarSetup's `view` on `pc_carsetup_flags`, added to `bounds_reviewed.txt` as `[gcc14]`; the 7 entries on the retired names (CarSetup's `carsetup_flag`, InitPathData's three, `view` and `warn`) removed; then none new, 9 kept (as before); pairs found 96 through a variable and 44 with warnings before, 93 and 41 after (the constant and indexed ones the same). GCC 13 (CI's runner) and mingw-w64 12 not run on it |
| `view_audit.py` | nothing to audit: the 8 are plain arrays, no record views |
| the old window ranges filled with 0x5A at the race's start (gdb, build host, Debug): race, race2p, demo, circuit, pause | each the same trace as without; every byte of the 8 ranges still 0x5A at the end |
| a `RUSH_CHECK_CASTS` build (build host): every synthetic path | 19 of 19 paths reached, no stop |
| Windows cross build (Pi, mingw-w64 12, RelWithDebInfo, fresh configure, a356a95) | builds; `gen_data.py --check --pe`: OK, 26 decoupled |
| the PC rule on the retail cartridge's table (gdb, Pi, `pc_drone_sched` called for every row) | every PC row, 7-12 drones with one human and 5-12 with two, is two iterations whose counts add up to its drones; each of the 12 N64 rows adds up to its drones too |
| `assets_drones`, `assets_roster` (16 slots declared) | pass, in Debug, under UBSan (`-DRUSH_SANITIZE=undefined`, build host) and under ASan + UBSan (Pi 5, GCC 12); the mutations below |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, a356a95: the branch's game C) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| after merging main 5e108ff (items 2, 4) and eb4511d (items 6, 7): ctest, build host, Debug and RelWithDebInfo, CI's way (the real-time tests alone, up to three tries) | 248 of 248 each at 5e108ff, 250 of 250 at eb4511d (`assets_rom` skipped) |
| the same: `synth_compare.py --same --runs 2`, build host, against a build of 5e108ff, then of eb4511d | 19 of 19 paths each time |
| the same: `data.c`, `data_symbols.txt` and the synthetic cartridge against eb4511d's | the same 8 rows and 45 lines; the same cartridge |
| the same on the Pi against eb4511d: synth_compare on every synthetic path, the six retail scripts | 19 of 19 paths; the retail scripts 6 of 6, each reaching its state; the synthetic cartridge the same file |

**The tests.** `assets_drones` (`port/assets/tests/test_drones.c`) builds UpdateModel's group, maxpath_controls,
InitPathData and DoDrones with `PC_MAX_LINKS` 16, so the storages have 16 entries and the
schedule's second kind starts at 17:
- MaxPathControls for each of 12 cars on a straight path, 400 feet apart: the steer, brake and gas land in the car's
  slot and in its model record (+0x720, +0x728, +0x72C), the direction's x is half the slot's last one and the
  direction to the target is kept in the slot, and every other slot's five values stay as they were; the brakes of
  slots 7-11 differ (the gas fell below -0.75 there). InitPathData clears all 16 slots' controls.
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
| InitPathData clearing 6 | 10 |
| munge's clear up to `MAX_LINKS` (6) | 8 (assets_roster) |
| munge listing the drones in `pc_our_drones` | 36 (assets_roster) |
| DoDrones' locals at 6 | none in Debug; the UBSan build stops at the first overrun |

**Races of 12 cars** (the branch merged with eb4511d, whose setting makes them possible: `--set game.cars=12`, and the
drones asked raised to fill the race, which gdb did by setting `gNumDrones` to 12 minus the players as CalcPolePositions
starts, as six presses of RIGHT on the Drones option would; Debug, Pi, the retail cartridge). One player and 11 drones
on `race` and on `drive` (gas and steering, to 107 s of game time, then to 147 s), two players and 10 drones on
`race2p`, and the attract demo's 12 drones: each reached its state with no fault, the schedule's rows 11, 13 + 10 and
12, every car at a place of its own, 0 to 11, and every car moving at the end, but on `drive` at 107 s a drone just
crashed, moving again at 147 s; no pool ran out (item 2's marks). The same four under ASan + UBSan (the whole game, Pi,
GCC 12): no fault, and one UBSan report in each, `pathcommon_sync/group.c:164`'s `s16 [17]` at 17, which 6 cars give too
(MORECARS.md 9). On 5e108ff, without this item, the same race faulted in the model thread as the countdown began
(MORECARS.md 7.1). The synthetic cartridge (build host) ran `race`, `race2p`, `drive` and `demo` the same way: each
reached its state, no fault, the 12 cars on `drive` at 12 places, all moving.

**Left to others.** Nothing in these files: item 3's first part (597098e) did the rest of MORECARS.md 3.1 around them
(init_cars's first distances and its reset of the slots after the race's, set_catchup, CarReportsGameOver's
locals), and relies on munge's clear reaching `PC_MODEL_SLOTS` (above) so that LoadCars, which reads every record's
drone type below the race's cars, doesn't load cars for records an earlier race of more cars left as drones. DoDrones
reads the circuit's per-slot ratings `gCircuitCars_skill[slot][38]` in mode 3 only: the circuit stays 6 cars (MORECARS.md
3.3). MORECARS.md 2.3 lists `lastbrake` and `lastthrottle` as 32 bytes, 8 by the gap: they are 24, with HAL_DrawText's
statics after them.

## 9. MusyX's per-voice storage

**What it is.** The sound library's storage for each of its voices ([AUDIO.md](AUDIO.md) 10.3): nine arrays of 32 in
the boot segment's BSS, each indexed by a voice's number (the low byte of its id). For `[audio] voices` above 32 they
are PC data of `PC_MX_VOICE_SLOTS` (96), and the library walks `pc_mx_voices()` of them (32 at original). With them,
the sound manager's message pool and its two command lists (AUDIO.md 10.4), 128 each in the game's BSS: PC data of
`PC_SND_MSG_SLOTS` (256), of which the game walks `pc_snd_messages()` (128 at original).

| N64 | What | N64 storage | PC |
|---|---|---|---|
| `sound_synth_voices` | the voices: macro pointers (32-bit window addresses, [MUSYX_DATA.md](MUSYX_DATA.md) 8.3), the voice's links in its chain, its kind, priority, volume, pitch, envelope, LFOs | 32 x 0x1A0 B | `pc_mx_voice`, `unsigned char[96][0x1A0]`, the N64 layout |
| `sound_handle_nodes` | the sequence nodes: a key for a voice chain, kept sorted; free and used lists (`sound_handles_free`, `sound_handles_used`) | 32 x 16 B; own storage before (24 B: two host pointers) | `pc_mx_nodes`, `struct pc_mx_node[96]` |
| `sound_voice_priority_links` | each voice's links in its priority's list (`sound_set_voice_priority`, `sound_remove_voice_priority`) | 32 x 4 B | `pc_mx_prio_links`, `struct pc_mx_link[96]` |
| `sound_voice_free_links` | each voice's links in the free list (`sound_init_free_voices`, `sound_free_voice`, `sound_alloc_voice`) | 32 x 4 B | `pc_mx_free_links` |
| `sound_voice_controllers` | an effect voice's 134 MIDI controllers: an effect is its own channel in set 255 (`sound_macro_start` gives it its number) | 32 x 134 B | `pc_mx_fx_ctrl`, `unsigned char[96][134]` |
| `sound_voice_last_keys`, `sound_voice_portamento` | its two per-channel bytes in set 255 (`sound_midi_set_last_key`, `sound_midi_set_portamento_state`) | 32 B each | `pc_mx_fx_chan_a`, `pc_mx_fx_chan_b` |
| `sound_voice_channel_owners` | its legato owner (`sound_synth_claim_channel`, `sound_synth_owns_channel`, `sound_synth_release_channel`) | 32 B (80 by the gap: macros.c's statics follow, host statics on the PC) | `pc_mx_fx_legato` |
| `sound_streams` | a stream's buffer record: a stream plays on the voice `sound_alloc_stream_voice` gives it (`sound_stream_start`) | 32 x 24 B; own storage before (40 B: the callback and buffer pointers) | `pc_mx_streams`, `struct pc_mx_stream[96]` |
| `audiomgr_gCmds` | the sound manager's messages: a command to its thread (a start, a change, a stop) | 128 x 24 B, game BSS; own storage before (32 B: the tag pointer) | `pc_snd_msgs`, `struct pc_snd_msg[256]` |
| `gStartCmds`, `gStopCmds` | the priority pass's starts and stops to send (`gStartCmds`, `gStopCmds`), a message each | 128 x 4 B each; own storage before (8 B pointers) | `pc_snd_start_cmds`, `pc_snd_stop_cmds`, `void *[256]` |

**Inventory.** The objects that referenced the nine names (nm over `game_objects.txt`, build host, 86bc6d0): the
voices `lib_1a660`, `lib_1f5b0`, `lib_207b0` and `lib_22300`; the nodes and both links `lib_1f5b0`; the controllers
and channel bytes `lib_21210`; the legato owners `lib_22300`; the streams `lib_1cf90`. No game file under `src/blob`
reaches them: the game talks to the library through its functions. The message pool: 13 objects, the allocator
(`GetCmd`: `slot18_alloc`'s, and the weak copies in `audio_options_menu`, `game_music`, `update_fx/getcmd.c`,
`entity_lookup`, `entity_helpers`, `transform` and the three `transform_b109.c`), the shutdown's wait for the pool to
empty (`AudioShutdown`, `heap_release`'s and `carcolor`'s copy) and AudioInit's clear, in five views of one 32-byte
layout (`Msg`, `Slot18`, `Slot24`, `ShutdownMsg`; `tAudioMsg` through the queue); the two lists: `audiomgr_thread.c`
and `updatefx.c`. Every reader walks the pool by index, 128, or indexes the lists by the pass's counts. Their views: the voices' about 90 (MUSYX_DATA.md
8.3), every one 0x1A0 bytes with its fields at N64 offsets (each unit's `VoiceState`, `SynthVoice`,
`SynthVoiceLegatoView`, `SynthVoiceAllocView`...); the nodes one (`SequenceNode`, packed, 24 bytes); the links two of
one layout (`ChannelLink`, `VoicePriorityLink`: `{u8 previous, next; u16}`); the controllers `u8[][134]`; the
bytes `u8[]`; the streams `SampleBuffer` (`sound_sample_record.h`). Every access is by a voice's number or
through a pointer to one entry; no file reaches a neighbour from them or them from a neighbour. The test
`assets_musyx` defined five of them; it defines the PC arrays now.

**What holds an address.** A voice's +0x18 holds its chain's node as the node's N64 address, 0x80050A50 + 16 times its
index ([MUSYX_DATA.md](MUSYX_DATA.md) 8.3), which `voices.c`'s `pc_voice_node` translates to `pc_mx_nodes`; that
encoding is kept, past the N64's 32 too (a number that nothing but `pc_voice_node` reads). The nodes' links and the two
list heads are host pointers into `pc_mx_nodes`; the library passes voice pointers (`&sound_synth_voices[i]`) only within a
call. A scan of the four images inflated from the retail cartridge (on the Pi) found no data word in the nine ranges,
and 74 code references into them (`lui` with an offset within eight instructions), all in the boot segment's code,
where the six units' C is; the references at the ends are the next symbols' own users (`sound_track_volumes`, `sound_priority_first_voice`,
`sound_voice_free_head`, `sound_handles_used`, `sound_midi_portamento`, `sound_midi_channel_owners`, macros.c's `macro_steps_this_update`), none of them a
loop's end.

**The size.** On the N64 the library walks `sound_voice_count` voices (at most 32: AudioInitSys caps them) and its set-up
loops walk 32. On the PC each array is `PC_MX_VOICE_SLOTS` (96) long, the caps are 96 (`PC_MX_CAP`), the set-up loops
walk the run's voices (`PC_MX_LOOP`, `pc_mx_voices()`, never below 32), and the rest walks `sound_voice_count`, as on the
N64. At original every loop walks what the N64's does.

**The change.** `pc_game_types.h`: the nine arrays, `struct pc_mx_node`, `struct pc_mx_link`, `struct pc_mx_stream`,
`PC_MX_VOICE_SLOTS` and `pc_mx_voices()`; the three of the manager, `struct pc_snd_msg`, `PC_SND_MSG_SLOTS` and
`pc_snd_messages()`, which the pool's loops and the queue's count walk (`PC_SND_MSG_LOOP(128)`). Each of the six units defines its N64 name on the PC as the array in its own
view, `#define sound_synth_voices (*(VoiceState (*)[PC_MX_VOICE_SLOTS])pc_mx_voice)`, and keeps its declarations for the N64
under `#ifndef TARGET_PC`, so the uses keep their N64 tokens. `decoupled.txt`: the nine rules (the legato owners'
with the C's 32 bytes, not the gap's 80, whose last 48 are statics).

**What the tables say.** `data_symbols.txt` loses the nine rows (two own storages among them); the synthetic cartridge
is the same file (build host); the loader's summary line differs only by its table's size; the message pool's and the
lists' own storages go too. gen_data's check: 38 decoupled (26 before).

**The test.** `assets_mxvoices` builds `voices.c` and `controllers.c` with 96 declared and every entry a sentinel:
96 voices, 80 of them effects (the effects' quota at 96): 80 effect voices each its own, 48 of them past the N64's 32;
the 81st takes one of its own kind (a steal, counted); a song's voice from the free ones; all freed and given again;
96 sequence keys and none for a 97th, and at 32 none for a 33rd with the nodes past 32 untouched; voice 90's
controllers (a 14-bit modulation written and read back, copied to voice 33) and channel bytes, nothing past 90 or
between 34 and 89, and a song's channel in the N64's table. `assets_musyx` runs the voices' macros on the PC arrays at
32.

**The views of the messages.** The 13 readers each cast `pc_snd_msgs` to their own record (`Msg`, `Slot18`, `Slot24`,
`ShutdownMsg`), so the symbol's declarations are `struct pc_snd_msg`'s alone and `views_sound_commands`, which compared
them, checks the three live readers' types now (`view_check.py --tag`, the three it compared before). Over all 13
(`--tag Msg,Slot18,Slot24,ShutdownMsg`): every view with live code is 32 bytes on the host with `id`, `type` and
`used` at 0, 2 and 3; four copies with no live code (the weak `GetCmd`s of `audio_options_menu`, `game_music`
and `update_fx/getcmd.c`, and carcolor's weak `AudioShutdown`) are 24, as before; entity_helpers' and entity_lookup's
`Msg`, whose payload union holds a pointer, have it at +8, the stop's tag, where `tAudioMsg` has it (the three
`transform_b109.c` copies, with no live code, put the change's floats there too).

| Check | Result |
|---|---|
| gen_data's check (build host, Pi, mingw-w64) | 38 decoupled (26 before), nothing else in the window: OK |
| the synthetic cartridge | the same file as the base's (build host, Pi) |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), build host | no open findings (endian_check's 2 open value conflicts in 1 storage, as before); layout_check takes `audiomgr_thread.c`'s `tAudioMsg` as `pc_snd_msgs`' reference (`SYMBOL_REFERENCE`: `struct pc_snd_msg` is no view the game writes through) |
| `bounds_check`, build host (GCC 14) | 4 new, none `clamped` or `two offsets`, each a `view` of a declaration at the storage's true size (`pc_mx_free_links`, `pc_mx_prio_links`, `pc_mx_voice` in voices.c and macros.c), added to `bounds_reviewed.txt` as `[gcc14]`; then none new |
| `assets_mxvoices`, `assets_musyx`, `assets_audiocmds`, `assets_sound`, `assets_audiomgr`, `assets_mgrstart`, `views_sound_commands` | pass (build host Debug and RelWithDebInfo, Pi ASan + UBSan: [AUDIO.md](AUDIO.md) 10.9) |
| the IDO check (`ido_check.sh`, IDO 5.3 and 7.1, every N64 object, cc3a517 against 09da2f0) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `n64_neutral.py --base 09da2f0` | passes |

## 10. The level-object nodes

**What it is.** The pool of the track's targets (MAT3 `tTarget`, `gTargetArray`: cones, cacti, fences, windows,
coins, mines, weapons, objects moving on paths; [POINTERS.md](POINTERS.md) 10.2): 130 nodes of 0x70 bytes at
0x8014D280-0x80150B60 in the game's BSS, which `initTargetStructs` makes the StatLL pool `gTargetList` of. Until
2026-11-09 the nodes kept their N64 layout there: a node's link and its kind's record were 32-bit window addresses,
the pool linked its nodes by them (its header's `n64` flag, `pc_statll_link`), and a visual held its target as one in
its 32-bit data word. That is the step of [NATIVE.md](NATIVE.md) (step 3) that takes them out of the window.

**Inventory.** One object references `gTargetArray`: `initTargetStructs.c` (the pool's set-up); the tests
`assets_targets`, `assets_battle` and `assets_weapons` placed it at its N64 address. The N64 range had one other
reference, the model records' collision walk to their end, which is `PC_MODELS_END` since section 3. What holds a
node: the pool's links, an environment visual's data word (`struct pc_visual`), the target groups' list
(`gTargetGroupList`), the path targets with a collision object (`CollisionTargetPtrs`), the mines' slots, the
always-colliding list and the power-ups; the last four were pointer-sized already. What a node holds: its link, and
its kind's record (a heap block, a static per-kind table in the game's data, or a flag's world object). Nineteen files
read a node through views of their own at the N64 offsets.

**The change.**

- `pc_game_types.h`: `struct tTarget`, the host layout (0x78 bytes: `next` and `data` pointers, MAT3's fields in
  MAT3's order), `PC_TARGET_NODES` (130) and `pc_target_nodes`; include/game/types.h names it `tTarget`, and every
  file that reads a node reads it so (the per-file `Target`, `Model` and `CollisionObject` views are gone).
  `decoupled.txt`: `pc_target_nodes gTargetArray 0x38E0`; `extents.txt` loses `gTargetArray`; the state hash
  (`statehash.c`) takes `pc_target_nodes` with the other decoupled items.
- The StatLL functions link every list by host pointers (include/game's `tStatList`, `tStatLLE`, `tDLLE`); the `n64`
  flag and `pc_statll_link`/`pc_statll_set_link` (pc_prelude.h) are gone, and `gTargetList` is a `tStatList`.
- `struct pc_visual`'s data word is pointer-sized (`uintptr_t`): a visual is 0x28 bytes and a car record 0x510
  ([CARS.md](CARS.md) 2, 3); `CAR_OFS` and `VIS_OFS` follow.
- The target groups' list is a native side table of keys and targets (`struct pc_target_group`, `pc_world_chunk`;
  [ASSETS.md](ASSETS.md) 4 #14).
- What stays N64-shaped, because the game's heap accounts for it (NATIVE.md 4.3): the kinds' records keep their N64
  sizes; a path target's record keeps its PTHD record as a window address (step 7's); a bounding box model
  (`tMiniModelData`) keeps its box as its row of `BBoxCollInfo` (`bboxRow`; MAT3 the row's address).

**What the tables say.** `data_symbols.txt` loses `gTargetArray` (gen_data retires it and the three names the image
tables have inside its range, `D_80150000`, `D_80150004` and `gEffectsPoolBase`, which no C names); `data.c` loses its
`.set` and gains `pc_target_nodes` (`struct tTarget[130]`, 15,600 bytes, `.bss`). Four own storages change type, all
BSS: `gTargetList` (`tStatList`, 40 bytes as before), `gEnvirons` (`EnvironNode[100]`, 4,000 bytes, its data word a
pointer-sized long), `CollisionTargetPtrs` (`tTarget *[30]`) and `gTargetGroupList` (`struct pc_target_group *`). The
synthetic cartridge is the same file. The game arena's 0x8014D280-0x80150B60 has no symbol now. The proof and the
crossings are NATIVE.md step 3's (section 10).

**The tests.** `assets_targets` builds the pool and its functions, the target set-up with the target groups' side
table, the start functions, the visuals, the collision pass, a path target and a bounding box model's collision on
`pc_target_nodes` (the host type's offsets and size checked, every link a host pointer, records past the first);
`assets_battle` walks the list in the power-ups' selection and updates and in both `PauseTargets`; `assets_weapons`
the weapon icons; `assets_world` the side table `pc_world_chunk` builds; `assets_zoid` and `assets_endian` the flag's
and the coin's node; `assets_carrecs` the visual's and the car's new offsets.
