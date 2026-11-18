# One type per record (wave 4)

Every record the game shares between files has one declaration, in `include/game`, with every field the game uses
named and typed. A file includes `game/game.h` and uses that declaration. It doesn't declare its own copy that names
only the fields it touches and skips the rest with byte arrays.

The headers before this wave declared each datum once (PCONLY.md 2). Many records still had a different struct in each
file that reads them: the physics model, for example, had 73 declarations across the files that use it. A file that
read a datum with a different type said so with a `VIEW()` line ([view.h](../../include/game/view.h)). Those copies
had to agree on the layout, and `view_check.py`, `view_audit.py` and `layout_check.py`
([LAYOUT.md](LAYOUT.md)) checked that they did. With one declaration there is nothing to disagree.

## 1. The rules

- A record shared by two or more files is declared once, in the header of its subsystem. A record that only one file
  uses may stay in that file, declared whole.
- MAT3's names are preferred for records and fields. A field that no code reads keeps its place under a name that
  says so.
- Comments describe the record as the PC has it. Offsets are given only where they belong to a file format: a save,
  a ROM file the loader converts, or a network message.
- A file that reads a field as another type does so in its code, through a union member or a commented conversion,
  not through a second struct.

## 2. The car's physics

### 2.1 The model record

(Agent A1: the model record, MAT3's MODELDAT.)

### 2.2 The car's descriptor, its tune, its tires and the paths

(Agent A2: the physics descriptor, the tune, the tires, the reckoning, resurrection, the drones' paths.)

## 3. The game car, the scene and drawing

### 3.1 The game car

**`tCarData`** ([cars.h](../../include/game/cars.h)), MAT3's CAR_DATA: one record per car slot, `pc_cars[PC_CAR_SLOTS]`
(the N64's `gGameCar`; PC data with no N64 address, [DECOUPLE.md](DECOUPLE.md) 4). Once a frame `update_game_data`
copies into it what the game thread shows of the car's model record: the dead-reckoned position and orientations,
the tires' places, the speed, the appearance bits. It also holds the car's 21 visuals, its place and finish, its
respawn path points, its drone path state (`mpctl`), its window and camera view, its input record, and its battle
state. 0x510 bytes on the PC, the N64's field order (0x3B8 bytes there) with the visuals and the input pointer
pointer-sized; every field has MAT3's name. The N64 differs from MAT3 twice: 21 visuals where MAT3 has 22 (no
headlight visual), and one more float in the battle's defence, the shield's spin (`shldPitch`, MAT3's `defense` has
four). `scenes`, in the record's alignment padding, is the PC's own (MORECARS.md 3.1).

| Record | Where | What |
|---|---|---|
| `tCarData` | cars.h | the car as the race shows and follows it |
| `tCarBattle`, `tCarWeapon`, `tCarDefense` | cars.h | its battle state (MAT3's anonymous `battle`, `battle.weapon`, `battle.defense`): the weapon and its ammunition, health and the damage of the frame, invisibility and the shield, the fade, the points, the carried mine, the reload time and the gun's aim |
| `CarTlut` | cars.h | a car slot's palette (`car_palettes[slot].palBase`, 256 entries): the paint, the tail lights' 8 and the headlights' 8 |
| `tTargetStructCoin` | types.h, by `tTargetStructPath` | a coin target's record (MAT3's): its spinning object, its spin, who has yet to find it |

Embedded records that are other agents' stay as the copies declared them, marked for the merge: the visuals
`struct pc_visual visuals[21]` (B2's `tVisual`), `struct pc_car_nav mpctl` (A2's `tPathPoint`), the coin's `mat`
(B2's `tUVMatrix`); the colours `IRclr` and `prevIRclr` are `tRGBA` (B3's), as MAT3 and the two files that read their
bytes had them. The input record is `struct tPadState *player` (C1's), a pointer.

**The other tags it had**, each a file's own struct over the same storage, with only that file's fields and byte
pads between them: `tCarData` (35 files), `GameCar` (8), `Car` (6: image B's shots and weapons, the radar dot,
the target collision, the drones' intervals, StartWeaponIcon), `Object` (5 cameras), `Player` (4 in image B),
`CAR_DATA`, `CarData`, `Cr` (2 each), `Car952`, `PCar`, `Player952`, `ProximityPlayer`, `B952`, `CarBattleView`,
`CarDistance`, `CarRecord`, `CarTime`, `Obj` (1 each), the storage's own `struct pc_car` (port/include/pc_game_types.h,
which named 36 fields between N64-sized pads), and image B's `Battle` and `Defense` (3 each). The coin's `CoinData`
(2) and the palette's `CarTlut` (2) were declared twice. `struct Car` in TargetCollTrigger.c, `CCar`, `EUCar` and
`CarState` are the model record's (2.1). The forward declarations `Cr`, `Defense`, `CarA`, `CarB` and `CarE`
(types.h) named nothing once the views went.

**Fields that disagreed**, by name: a view's field kept its place, so each was renamed to the field at its offset.
animDot's `dead` and `active` were `crash_state` and `dead`; RecordRace's `distance` and RecordGhost's `dist` the
`mileage` (the record's `distance` is the one before it); the cameras' `player`, `mode` and `mode2` were `camera`,
`view` and `saveview`, CheckCameraView's `car` and `slot` the `slot` and `camera`, AnimateWing's `shift`, `quality`
and `disabled` the `camera`, `view` and `carMine`; image B's `flags`, `countdown`, `alpha`, `phase` and `timer` the
battle's `defense.active`, `shoot`, `fadeAlpha`, `fadeState` and `fadeTime`; `finished` and `state` the
`place_locked`, `time` the `score`, `RWR`, `position` and `pos` the `dr_pos`, PCar's `vx`, `vy`, `vz` the `stuntW`.
Flat matrices (`f32 m[9]`, `matrix[9]`, `dr_uvs0[3]`) are `dr_uvs`, indexed by row and column; the camera views'
`Vec3`/`Mat3` were arrays already.

By type: where a view read a byte unsigned (`camera`, `view`, `place`, `slot`, `carMine`, `Points`, `crash_state`,
`dead` in a few files) or a word signed (`appearance`, `sheenAlpha`, `defense.active`), MAT3's type is the record's;
the code is the same but in the units of 3.1.1. Two views read the scene object handle's low half as an s16 at the
handle's first bytes (`objnum_low`, `s_f6`: the little-endian host's low half); they pass the s32 `objnum` to the s16
parameter now, a conversion that keeps the low half on any host. AnimateCoin read its coin's handle the same way
(`SceneHandle`), and does the same now. Image B's `RGBA` colour copies take `IRclr.rgba`.

**How the game reaches it.** `pc_cars[i].field`, and `&pc_cars[i]` where it keeps a record pointer. The 44 accessor
macros of pc_game_types.h (`PC_CAR_PLACE_LOCKED(i)`, `PC_CAR(i)` as a `void *` for a file's own view, `PC_CARS`,
`PC_CARS_END(n)`, `PC_CAR_INPUT_AS(T, i)` ...) are gone: their 470 uses in 135 files (a few in comments) are the fields they named. Image B's
walks over the race's cars compare `tCarData` pointers, so the comparisons of distinct pointer types there
(missile_lock, proximity_damage, shots) are gone. Port code that reads the records or defines them (the run's
report in main.c, the state hash's size, the asset tests' stand-ins for data.c) includes cars.h with
`GAME_RECORDS_ONLY` defined: the records and `pc_cars`, not the data and functions the tests define themselves
with types of their own. Their include directories have `include` (CMakeLists.txt: the asset tests, pcultra,
test_overlay_data). cars.h reads types.h with a C library's `errno` macro set aside (`push_macro`): libultra's
`OSContStatus` has a field of that name, and mingw-w64's stdlib.h, which these files include first, defines the
macro (the 20 port files checked with mingw-w64 12's `-fsyntax-only`, clean).

No `VIEW()` line read the car record (it had none). Numbers in this area, before and after:

| | Before | After |
|---|---:|---:|
| declarations of the car record | 75 (74 files, and pc_game_types.h's) | 1 |
| of its battle sub-records, the coin's record, the palette | 6, 2, 2 | 1 each |
| byte-pad arrays in those declarations | 395 | 0 (the palette's `rest` and the coin's two unread floats keep their places by name) |
| accessor macros, and their uses | 44, 470 | 0 |
| warnings in the 140 game files changed (GCC 12, `-fsyntax-only`; none new) | 317, 311 of them incompatible pointer types, 3 comparisons of distinct pointer types | 280, 277, 0 |
| census (GCC 14): `-Wcompare-distinct-pointer-types` in the game | 7 | 4 (TextureAnimation, collision.c, showhud.c's two: not the car's) |

#### 3.1.1 Units that aren't the same

`objsame.py` against the wave's base (d224beab's builds), every other unit `same`:

| Build | Unit | What changed | Why the game is the same |
|---|---|---|---|
| Debug | `Update_MDrive`, `rear_camera/group.c` (DeathCam) | the scene object handle passed to `ZOID_UpdateObject` (an `s16` parameter) is loaded as the record's s32 `objnum`, not as an s16 from its first two bytes | the parameter takes the low half either way; the old read was the little-endian host's low half by address, the conversion is the low half on any host |
| Debug | `AnimateCoin` | the same for the coin's handle, passed to `RemoveObjectTree` | the same |
| Debug | `munge_gLink_data` | `car->place = PC_LINK_PLACE(i)` stores into the record's `s8` where the view had a `u8`: one more register move for the conversion | the same byte is stored |
| Debug | `targetpaths/group.c` | the obstacle course's path targets read the first car's `dr_pos[k]` as a field rather than as float 2 + k of the array: the offset is folded into the load | the same address, the same float, the same arithmetic |
| RelWithDebInfo | 55 units: 54 files that read the car through a view of their own (of the 74), and maxpath/group.c | the files reach a car through `&pc_cars[i]` now, a pointer of the array's own element type, where before a `void *` became their own struct's pointer: GCC folds the accesses into elements of the declared array, and allocates registers, schedules and sometimes keeps a loaded value in a register differently; maxpath's AvoidAreas reads other cars' `dr_pos` beside the drone's `mpctl`, fields GCC now tells apart where the old record had pads (the files that only used the accessors are otherwise the same) (`opcheck`: in 51 units the same float operations, in animDot, maxpath's AvoidAreas, StartSmoke and rocket's AnimateRocketFlame a common value kept rather than computed twice, a clamp done with `vmaxss`) | no float operation is reordered or regrouped (`-ffp-contract=off`, no fast-math); the optimized compare of section 3.1.2 runs every synthetic path the same, shots and sound |

#### 3.1.2 The proof

Rocky (x86-64, GCC 14), against the wave's base (`~/rush2049pc/w4-ref`, d224beab's builds), on the last commit:

| Check | Result |
|---|---|
| `pconly_gate.sh --base ~/rush2049pc/w4-ref --skip realtime --data-moved` | PASS: the build, ctest 572 of 572 (`assets_rom` skipped: no ROM there), 64 of 64 synthetic paths `same` |
| RelWithDebInfo against the base's, `synth_compare.py --same --shots --audio --data-moved -j10`, the synthetic cartridge | 63 of 64 `same` at `-j10`; the 64th, `mods_fuzz_short`, `varies` in its last audio subframe, as the base's own two runs did there (PCONLY.md 2: a loaded machine), and alone is `same` |
| the same on the audible cartridge (`rush2049_synth_snd.z64`) | 61 of 64 `same` at `-j10`; `mods_bad`, `mods_text` and `pause12` `varies` in the last audio subframe, as the base's own runs did, and alone are `same` |
| `ctest --test-dir build/opt -LE realtime` | 572 of 572 |
| the realtime tests (Debug, `ctest -L realtime`, under the realtime lock) | 57 of 61 while other agents' runs loaded the host; the four that failed on the host's clock (`paced_hold`, `paced_coarse`, `paced_stop`: frames re-based for the host's work; `pak_realtime`: retraces caught up) passed alone, each on its first try |
| ASan with UBSan (GCC 14's objects, Clang 21's runtime): CI's x86-64 set, its skip list | 399 of 399, no ASan, LSan or UBSan report (`sanitize_fault_chain`'s own) |
| the same build: the two paths that set leaves out (`synth_boot_race`, `synth_boot_pcsetup`) | pass |
| the same build: every synthetic path on the audible cartridge (`synth_paths.py`, each in a folder of its own) | 65 of 65 to their state, no report; the nine `mods_*` paths' checks need the fixture ctest makes (their mods folders, drive's screenshot), which a run alone lacks: CI's set ran them, under ASan, with it |
| `objsame.py`, Debug and RelWithDebInfo | 956 and 906 of 961 `same`, the units of 3.1.1 |
| `gen_data.py --check` (each build); data.c, `data_symbols.txt`, `data_layout.txt`, `data_layout.json` against the base's | OK; the same files but for `pc_cars`' type name, `tCarData[12]` where it was `struct pc_car[12]` (data.c's comment, data_layout.txt's line) |
| `view_audit.py --symbols pc_cars --size 952,956 --ref StartWeapon.c`, `view_check.py --sym pc_cars` | 141 views (183 before), 127, none disagreeing: every one `tCarData` |
| `layout_check.py`, and `--storage pc_cars` | the base's findings, no new one (6 open, none the car's); 1,667 accesses on `pc_cars`, every struct view `tCarData` (B2's visuals and A2's `pc_car_nav` apart) |
| `offset_check.py --strict`, `alias_check.py`, `bounds_check.py` (RelWithDebInfo) | no open finding; none open; none new (four GCC-14-only `pc_cars` entries of `bounds_reviewed.txt` found no more, removed; the four still found say which fields) |
| `census.py`, `n64_audit.py --baseline` | 1,063 of 1,063 compile, 0 pointer warnings, 0 undefined; no class grew, none fell |
| the 20 port files that include cars.h, mingw-w64 12 `-fsyntax-only` (the Pi) | clean |

### 3.2 The scene

(Agent B2: visuals, matrices and vectors, windows, scene objects, polygons, skids, blasts, smoke, texture swaps.)

### 3.3 Blits and colours

(Agent B3: the blits and the one colour type.)

## 4. Players, saves, menus and sound

### 4.1 Players and their input

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

(Agent C2: profiles, records, ghosts, the pak's files.)

### 4.3 Menus, battles and circuits

(Agent C3: the menus' buttons and items, battles, circuits.)

### 4.4 Sound

(Agent C4: voices, channels, speech, sequences, sound resources.)

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

## 6. The proof

(The lead's, after the merge.)
