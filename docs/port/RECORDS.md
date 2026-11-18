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

**`tModelData`** (MAT3's name, its fields and their order), one per car slot: **`pc_models[PC_MODEL_SLOTS]`**, both
declared in [`include/game/modeldata.h`](../../include/game/modeldata.h), which physics.h includes. It is a car's
simulation: the model thread steps it (syminit, then sym and regular: forces, torques, accelerations, velocities,
positions, the road, the collisions), the game thread sets its controls (`mainin`) and draws the car where its dead
reckoning (`reckon`) puts it, and the cameras, effects, sounds and HUD read the rest. 0x808 bytes on the N64, 0x830 on
the PC (its five pointers, `car`, `tune`, `lasttp[4]`, `torquecurve` and `movingSurface`, 8 bytes wide: CARS.md 4).
The record has its own header because the port's state log (port/main.c) and state hash (port/ultra/statehash.c)
read the records and don't include the game's other headers; the record needs only the base types.

Its parts:

- `initin` (where the car starts: its grid place or respawn point), `fastin` (the model clock) and `mainin` (the
  controls) are declared in place, anonymous, as in MAT3. `mainin` is not `volatile` (MAT3's is): the game's threads
  run one at a time, and most of the files read it plain.
- `tires[4]` (the tire state), `resurrect` (the respawn's flight) and `reckon` (the dead reckoning) are A2's records
  (section 2.2), declared in place with MAT3's field names and marked `wave 4` until theirs exist.
- The vectors and matrices are float arrays (`f32 rwr[3]`, `f32 uvs[3][3]`), as MAT3 declares them: B2's types
  (section 3.2) go in at the merge where the record marks them.
- `color[3]` at MAT3 0x7CD is the N64 game's three paint colours, where MAT3 has `PaintJob` and an unused short.

**Its other tags.** 103 declarations in 102 files: `tModelData` (75, and sparks' anonymous struct its typedef names so),
`Model` (8: CheckCameraView, FindMaxpathIntervals, animDot, update_camera, PassedCP, CheckCPs, shots, StartWeaponIcon),
`MODELDAT` (4: animRadarBlip, StopCarSounds, UpdateCarSounds, drones), `CarState` (2: model_controls_input,
skid_marks), `CCar` (2: stree_collide's group and CollideCarBodyAgainstSurface), and one each of `ModelData`
(StartCone), `CollisionModel` (TargetCollBBox), `Car` (TargetCollTrigger), `Vehicle2056` (StartCarSounds),
`AudioModel2056` (UpdateBumps), `ModelView` (UpdateCarSounds), `VCar` (CollideCarBody), `EUCar` (TireSurf), `HudRec`
(stunt), `ModelRecord` (update_model) and `Model2056` (AnimateWing); and `struct pc_model`
(port/include/pc_game_types.h) with its 50 accessors (`PC_MODEL_SLOT(i)` ..., `PC_MODEL(i)`, `PC_MODELS`,
`PC_MODELS_END`), 370 uses in 98 files. The prototypes that named those tags (audio.h, input.h, race.h, system.h,
track.h) take a `struct tModelData *`.

**How the copies went.** Each file's view was mapped to the record from the build's DWARF and clang's syntax tree:
every member access through a view, with its subscripts and sub-members, has a host offset and a size, which pick
the record's field there; the access was rewritten to that field (`m->reckon_rwr[0]` → `m->reckon.rwr[0]`,
`m->vectors[0][2]` → `m->f[2]`, `m->bodyR[i].x` → `m->bodyR[i][0]`), the view removed, and the view's name replaced
by `tModelData`. The accessors became the fields (`PC_MODEL_SLOT(i)` → `pc_models[i].slot`, `PC_MODEL(i)` →
`&pc_models[i]`, `PC_MODELS_END` → `&pc_models[PC_RACE_CARS]`). The names the files had that aren't MAT3's are gone
with them: `struct pc_model`'s `speed`, `lap`, `active`, `flags`, `steer`, `gas`, `viscode`, `suspension`
(`magvel`, `laps`, `in_game`, `appearance`, `mainin.wheel`, `mainin.throttle`, `sviscode`, `reckon.suscomp`); the
views' flattened parts (`resurrect_pos`, `reckon_rwr`, `fastin_dt`, `mainin_gear`, `initpos` ...: the part's field);
and the views' own guesses: animDot's `hit_target`, `crash`, `hide`, `mode` and its `collidable` at 0x7EB
(`resurrect.moving_state`, `crashflag`, `hide_car`, `drone_type`, `solid`), skid_marks' `surface`, `terrain`,
`airborne[0]` and `wheelHeight` (`tiresli`, `sviscode`, `resurrect.moving_state`, `reckon.suscomp`), UpdateBumps'
`collision_kind`, `last_collision_kind`, `collision_direction` (`thumpflag`, `mthumpflag`, `thump_side`),
AnimateWing's `type`, `state`, `sound` and `player` (`body_type`, `wing`, `wingSnd`, `slot`), TargetCollBBox's
`corners`, `radius`, `position`, `basis` (`bodyR`, `colrad`, `reckon.rwr`, `reckon.uvs`).

**Where the copies disagreed**, and the type taken:

| Field | The copies | Taken |
|---|---|---|
| `appearance` | `s32` in 6 files, `u32` in 6 and `struct pc_model` | `u32` (MAT3) |
| `body_type`, `color[3]` | `u8` in 16 files and `struct pc_model`; `s8` in init_cars, which read it through `(u8)` casts | `u8` |
| `crashflag`, `topscrape`, `thumpflag` | `s8`; `u8` in initialize and TireSurf | `s8` (MAT3) |
| `sviscode`, `roadcode`, `roadboost`, `obResPt` | MAT3's signedness; init_road and TireSurf the other | MAT3's |
| `slot`, `laps`; `magicdiff`, `nothrusttorque`; `mainin.gear`; `resurrect.waterAbort` | signed; dead_reckon_all, carsetup, init_mdrive, TireSurf unsigned | signed (MAT3) |
| `mainin` | `volatile` in syminit and model_controls_input; plain elsewhere and in `struct pc_model` | plain |
| `uvs`, `reckon.uvs`, `resurrect.uvs` | `f32[9]` in some views, `Basis` in others, `f32[3][3]` | `f32[3][3]` (`uvs[4]` is `uvs[1][1]`) |
| vectors | `Vec3` arrays, `struct {x, y, z}` views, `f32[3]` | `f32[3]` (`.x` is `[0]`) |
| `tires` | `u8[4][92]` (forces1), views starting 8 bytes in (AnimateTire) or at `sideforce` (init_mdrive) | the tire state, MAT3's fields (forces1 passes `&m->tires[i]`) |
| initialize's `vectors[6][3]` | f, m, a, aa, v, w as one array | the six fields |

Every one of these gives the same Debug code in the files that had the other type (objsame), but for the units below.
**Reads across fields**, kept and commented: StartSmoke reads `sviscode[type]` for its types 4 to 7 too, on into
`roadboost` (the wreck's smoke is skipped when `roadboost[0]` is 8, as on the N64): `((u16 *)m->sviscode)[type]`.

**Units that aren't `same`** (objsame against wave 4's reference, d224beab's builds). Debug, 7 of 961, each the same
loads and stores of the same addresses:

| Unit | What changed |
|---|---|
| AnimateShadow, skid_marks | `m->reckon.airdist[i]`, `m->reckon.suscomp[i]`: GCC splits a nested member array's offset between the index and the displacement (`0x30(%rdx,%rcx,4)` after `add $0x1d8`, where the flat view had `0x10` after `add $0x1e0`): the same address |
| maxpath | `m->reckon.uvs`'s address in two steps (`add $0x7a0; lea 0x24`) where the flat view's was one (`lea 0x7c4`) |
| sparks | the speed and the roof scrape read as `pc_models[slot].magvel`: the address computed and then loaded, where `PC_MODEL`'s `void *` cast had it folded into the load |
| init_cars | stores of the roster's car and colours (u8) into `body_type` and `color`: the byte goes straight to the store, where the view's `s8` field took it through another register |
| model_controls_input | `if (--car->mainin.gear <= 0)`: the gear is read back after the store, where the view's `volatile` field reused the value it stored (the same value: nothing else runs between) |
| StartSmoke | `((u16 *)m->sviscode)[type]`: the address of `sviscode` and then the index, where the view's `u16[8]` field had them folded |

RelWithDebInfo, 37 of 961: those 7, and 30 whose Debug objects are the same, so the C means the same; GCC's
optimizer works from another form of it: a record reached by its own type and `&pc_models[i]` (not through
`PC_MODEL`'s `void *` into another struct type) is indexed where it was a base pointer (`imul $0x830` at each use, or
a base and an index register), registers are allocated and instructions scheduled otherwise; syminit merges the four
plain `mainin` stores into one 16-byte store, and model_controls_input reloads less, where the views had `volatile`.
The synthetic gate runs the optimized build against the reference's optimized build ([6](#6-the-proof)).

**VIEW lines.** None: `pc_models` had no VIEW line (its readers had their accessors or their own pointer types).

**The numbers** (src/ and nonmatching/, by `struct {` and `union {` declarations): 913 local declarations before, 810
after (the 103 of the model record); tags in more than one file 92 to 87 (`tModelData`, `Model`, `MODELDAT`,
`CarState`, `CCar`), their declarations 506 to 414; `u8` pad arrays 1,426 to 715 (the views' 711); `struct pc_model`
and its 50 accessors gone.

**Also.**

- collision.c's comparison of distinct pointer types (mingw's warning, `other < PC_MODELS_END`) is gone: the walk's
  end is `&pc_models[PC_RACE_CARS]`. dotireforce.c has a new one until the merge: `tire == &m->tires[2]` compares its
  own `Tire *` with the record's tire state, which is anonymous until A2's record is swapped in.
- `gGhosts[m->slot]` (NATIVE.md 18.4) stays inside its four entries: PlayGame reads it only for a car whose
  `drone_type` is `DRONE_TYPE_HUMAN`, and UpdateModel only for a car with an input record (`player`); init_cars and
  munge_gLink_data make a car human only from roster owner 6 (LINK_OWNER_HUMAN) on a slot in the race, and the roster
  (CalcPolePositions, GhostStart; fake_init's are drones) gives owner 6 only to slots below `gNumPlayers`, at most 4.
  CheckCameraView's switch times are indexed by the car record's camera (B1's record), not by the model record.
- The GIMPLE reader of the layout tools (`endian_check.read_gimple`) read a local of an anonymous struct type
  (GCC prints the type in full, over several lines) as the function's end: functions with a pointer to the record's
  tire state lost their accesses (forces1 and the tire code). It reads such a declaration as one now.
- The tests that defined `struct pc_model pc_models[]` define `tModelData pc_models[]`; `assets_models` checks the
  record's fields at MAT3's offsets (`MODEL_OFS`).

### 2.2 The car's descriptor, its tune, its tires and the paths

(Agent A2: the physics descriptor, the tune, the tires, the reckoning, resurrection, the drones' paths.)

## 3. The game car, the scene and drawing

### 3.1 The game car

(Agent B1: the game car, MAT3's CAR_DATA.)

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
