# The car, visual and model records on the PC

The game keeps two per-car arrays, and the C views them through many struct declarations: the car record `pc_cars`
(N64 `gGameCar` = `gGameCar`, MAT3 `gGameCar`, `tCarData`) and the model record `pc_models` (N64 `D_8014A250`,
MAT3 `model[]`, `tModelData`). A third record type lives inside the car, the visual (`tVisual`), which also makes up the
environment visual pool. Natively the views disagreed: gen_data laid out 1,120 bytes a car while about 40 views
indexed cars by 952, the link warned that `gGameCar`'s size changed, and the model record's pointer fields moved
everything after them. This page gives each record one PC layout, says why, and lists what was changed and what is
left. 2026-10-27. Related: [POINTERS.md](POINTERS.md) (the N64-neutral conventions), [DATA.md](DATA.md) (where data
symbols live), [PROTOTYPES.md](PROTOTYPES.md).

## 1. Summary

| Record | N64 | PC | Storage on the PC | Why |
|---|---:|---:|---|---|
| car, `pc_cars[12]` (N64 `gGameCar[6]`) | 0x3B8 | **0x468** | own storage, no N64 address (`struct pc_car`, [DECOUPLE.md](DECOUPLE.md) 4) | its visuals hold function pointers, and +0x380 points at own storage: those must be host pointers |
| visual, `tVisual` (21 per car at +0x110; the environment pool `gEnvirons[100]`) | 0x18 | **0x20** | inside the car; the pool is C-defined | `next` and `func` are host pointers; `data` holds flags or a window address |
| model, `pc_models[12]` (N64 `D_8014A250[6]`) | 0x808 | **0x830** | own storage, no N64 address (`struct pc_model`, [DECOUPLE.md](DECOUPLE.md) 3) | its five pointer fields (MAT3's `car`, `tune`, `lasttp[4]`, `torquecurve`, `movingSurface`) are host pointers (since 2026-11-09, section 4) |
| car physics descriptor, `car_descriptors[6]` (MAT3 `_tCar`; `model->car`, `gCarModels[body]`) | 0xB4 | **0xC8** | own storage (data.c), filled from the image | its four tire pointers (`tires[4]`, to `frontTire`) are host pointers (since 2026-11-09, section 4) |

Results: `view_audit.py` finds 0 disagreeing views of the car (134 views; 222 with `struct pc_car` since DECOUPLE.md 4)
and of the model (120; 143 with the pointers of section 4), and one of the visual (30, `AnimateFlag`'s, left to the
front-end work); the link no longer warns about `gGameCar`; gen_data's pointer conflicts for these symbols are down from 154 to 1 (a raw byte view);
`test_cars` runs the game's own car and model code on synthetic records.

## 2. The car record

Since 2026-10-29 it is PC data with no N64 address: `pc_cars[PC_CAR_SLOTS]` (12 records) of `struct pc_car` in
`port/include/pc_game_types.h`, which has the layout below and names the 36 fields the game reaches by index; the
game reaches them through `PC_CAR_<FIELD>(i)`, a record through `PC_CAR(i)` in its own view, and the names that were
inside the array (`D_80152907` and 10 others) are the accessors of the fields they named ([DECOUPLE.md](DECOUPLE.md)
4). It was own storage before (host storage, so its own address was a host one already).

### 2.1 Pointer fields and their pointees

| N64 offset | Field | Pointee | PC |
|---|---|---|---|
| +0x110 + 0x18k (k < 21) | `visuals[k].next` | another visual (unused for car visuals; the car array is host storage) | `void *`, 8 |
| +0x124 + 0x18k | `visuals[k].func` | the visual's animator, a host function (`AnimateTire`, `AnimateShadow`, ...) | function pointer, 8 |
| +0x11C + 0x18k | `visuals[k].data` | flags (`VIS_BIT` ...) or a float (the tires' angle) | u32 |
| +0x380 | `input` | the car's input record `&gPlayers[i]` (own storage), NULL for drones | `void *`, 8 |

So the car can't keep its N64 size, and the PC layout is the N64 field order with those widened:

| N64 | PC | |
|---|---|---|
| 0x000–0x110 | 0x000–0x110 | unchanged |
| 0x110 + 0x18k + f | 0x110 + 0x20k + f' | f' = f for `next` (f < 4), f + 4 after it (`index` +8, `objnum` +0xA, `slot` +0xC, `data` +0x10, `timeStamp` +0x14, `func` +0x18) |
| 0x308–0x380 | 0x3B0–0x428 | +0xA8 |
| 0x380 (`input`) | 0x428 | 8 bytes |
| 0x384–0x3B8 | 0x430–0x464 | +0xAC, then 4 bytes to 8-byte alignment: `struct pc_car` names them `scenes`, a PC field with no N64 one (a scene trigger's cars from 8 on, [MORECARS.md](MORECARS.md) 3.1); every view leaves them as its alignment padding |
| stride 0x3B8 | **0x468** | |

`port/include/pc_prelude.h` has it as `PC_CAR_SIZE` and `CAR_OFS(n)` (the PC offset of N64 offset n, which may run
into later cars: `CAR_OFS(0x3B8)` is the stride) and `VIS_OFS(x)` (inside a visual, `VIS_OFS(0x18)` = 0x20);
`struct pc_visual` is in `port/include/pc_game_types.h` (which `pc_prelude.h` includes), with the record itself,
`struct pc_car`, since the record is PC data ([DECOUPLE.md](DECOUPLE.md) 4). Game files that use them define the N64
side, `#define CAR_OFS(n) n`, so the N64 tokens are the original ones.

### 2.2 The views

Every view declares the visuals and the input pointer as pointer-sized members, with N64-sized padding between
them, so the compiler gives each one the layout above and gen_data computes the right N64 offsets from the PC
DWARF:

- **Padding over the pointers** was split under `TARGET_PC` by `tools/port/view_pads.py` (`u8 pad0F0[0x35B - 0xF0]`
  became `u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x35B - 0x308];`), the original
  line kept as the `#else` branch for the N64 build (both gone since 2026-11-08), in about 140 files (overlay B's and the nonmatching drafts among them); and in
  every copy of `include/game_types.h`'s `GameCar` context block (426 files, most of which never use it).
- **Views that name fields there**, by hand: StartSparks' `CarSlot` (visuals 1–5 seen from +4, its last word is the
  next visual's `next`), the wing visuals as `Side24` (`AnimateWingFlame`), the `SndSlot` sound slots of the
  `audio_frame` groups, `startcar.c`'s `tVisual`, `GC2.w380`, rocket's `pad380`/`b381`, `setMiniFBCollisionForce`'s car view
  (its `CENTERFORCE` belongs to `MODELDAT`), and `StartEnvironment`, whose view counted visuals from +0xF8, one before
  the first: on the PC it starts at the first and its indices go through `VISUAL(n)` (n - 1).
- **Partial views through pointers**, found by DWARF (structs whose padding spans the visuals and that name car
  fields after them): `InitCarPath`'s `CarB`, the `Cr` views of `steady_camera` and `UpdateRearCamera` (with `Q *q` at
  +0x380), `init_camera_on_track`'s, and the `Object` views of `point_at_car`, `maxpath_cam`, `circle_camera_around_car`,
  `update_camera`.
- **Names inside the array** (`D_801528F0` +0xD8, `D_80152900` +0xE8, `D_80152906`/7/8 +0xEE..0xF0, `D_80152BBB`
  +0x3A3) are declared as arrays of a car seen from that offset. Most start at an odd offset, where a struct with
  pointers can't sit, so on the PC they keep only their stride (`u8 rest[PC_CAR_SIZE - 1]`); gen_data placed those
  names at the mapped offset (`D_80152B70 = gGameCar + 0x400`) whatever they declared. Since DECOUPLE.md 4 they
  are retired with the array, and each use is the accessor of the field it named (`D_80152907[i].flag` is
  `PC_CAR_PLACE_LOCKED(i)`).

### 2.3 Code at raw offsets

m2c code that reaches cars with byte arithmetic goes through `CAR_OFS` (on existing lines; the N64 expansion is the
original token):

| Site | What |
|---|---|
| `gameover_validOption` (+ weak copy), `Update_MDrive`, `AreWeSolid`, `StartCoin`, `animBones` | `* 0x3B8` strides on names inside the array |
| `InitCPTime` (race start) | `* 952` strides on `gGameCar` (+0x108, the position at +8) |
| `StartWingFlame`, `StartWing` (StartWingFlame, StartWing; both group copies) | the wing visuals at +0x2C0 and +0x290 |
| `StartFrame` | visual 14's func (stored pointer-sized now: it was `(s32)AnimateFrame`), objnum, slot, the scale at +0x374 |
| `SyncMaxpathToLastCheckpoint`, `StartMine`, `init_view3`, `fix_camera_in_space` (`motion_norm`), `BlinkResurrectingCars`, `UpdateModel` | fields past the visuals through car pointers (+0x33A, +0x384, +0x35C–E, +0x30C, +0x380, +0x388) |
| `dead_reckon_all`, `SetNewPathIndex`, `AbortCar` (3 copies), `CheckCrash`, `BlinkResurrectingCars`, `UpdateModel` | `GameCar`'s `pad0EC[x]` used as a byte window from +0xEC: `PAD0EC(p->pad0EC, x)` is `CAR_OFS(0xEC + x)` on the PC |
| `RemoveCar` (the car visuals' stop loop) | `VIS_OFS` for the func, objnum and stride |
| `InitCarVisuals` (visual init) | a PC version of the function over `struct pc_visual` |

`init_cars` clears cars with `sizeof(Player)`, which is the PC size. Code that builds a car pointer and
passes it on was followed to its callees (`steady_move_cam`, `UpdateCarObj`, `BattleDamage`, the maxpath functions):
they use views, not raw offsets.

## 3. The visual record

On the PC a visual is `{void *next; s16 index, objnum, slot; u32 data; f32 timeStamp; void (*func)(); }`, 0x20
bytes, in the cars and in the environment visual pool `gEnvirons` (100 nodes, defined in `InitEnvs.c`;
`GrabEnvEntry` `GrabEnvEntry`, the list `gEnvList`, `ReleaseEnviron` to free one).

`data` stays a u32. For car visuals it holds flags or a float. For environment visuals it holds what the visual
animates, and every such pointee is in the window: the targets, which are the level-object nodes, in the arena
block `gTargetArray` (`AnimateMine`, `StartWindow`, `StartKnockdown`, `StartCactus`, `AnimateVelRotGeneric`,
`Animation24`, and the cameras of `CamNode`; their state records are heap blocks of their category,
POINTERS.md 10.2). Those views store it through `N64W()` and read
it through `N64P()`. `func` is pointer-sized in every view: the pool's `Node` views had it as `u32` (and `data` as a
pointer), `StartCone`, `BNode`, `CamNode`, rocket's `Entity` and `ModelObj` as an integer (now `intptr_t`), and the
callbacks' own views (`AnimateShadow`, `AnimateNewBlast`, `AnimateWing`, the tire views) had padding
standing for `next`.

## 4. The model record

`pc_models[PC_MODEL_SLOTS]` (12 records) of `struct pc_model` in `port/include/pc_game_types.h`: PC data with no N64
address since 2026-10-28 ([DECOUPLE.md](DECOUPLE.md) 3), which names the fields the game reaches by index (through
`PC_MODEL_<FIELD>(i)`; a record through `PC_MODEL(i)`, in the reader's own view) and MAT3's five pointers. Since
2026-11-09 (wave 3 step 2, [NATIVE.md](NATIVE.md) 5) those five fields hold host pointers, no longer the N64's 32-bit
window addresses:

| MAT3 offset | PC offset | Field | Points at | Set by |
|---|---|---|---|---|
| +0x000 | +0x000 | `car` (`_tCar *`) | the car type's physics descriptor, `car_descriptors[]` (below) | carsetup (`gCarModels[body]`), init_cars (the first) |
| +0x004 | +0x008 | `tune` (`Tune44 *`) | `&gTune[handling]` (game data) | carsetup |
| +0x3A0 | +0x3A8 | `lasttp[4]` (`streetyp *`) | each tire's last node of the road's surface tree (`stree_nodes`, the road file) | init_collision (the tree's root), TireSurf |
| +0x40C | +0x428 | `torquecurve` (`const s16 *`) | `stdtorquecurve` (game data) | carsetup |
| +0x648 | +0x668 | `movingSurface` (`surftyp *`) | the moving surface under the car (`stree_surf`, the road file), or NULL | road (NULL each step), TireSurf |

The layout is MAT3's field order with each pointer 8 bytes wide: a field after one lies further on than on the N64,
by 8 after `tune`, 0x18 after `lasttp`, 0x20 after `torquecurve` (which the compiler aligns to 8, leaving 4 bytes
after `engangvel`) and 0x24 after `movingSurface`, and the record is 0x830 bytes (0x82C rounded up to the pointers'
alignment). The offsets in the game's comments and padding stay MAT3's: every view of the record (the 143 of its
whole size and the physics stages' partial ones, in 106 game files) declares the five pointers by MAT3's names and types
and pads in MAT3's bytes between them (`u8 pad8[0x3A0 - 0x8]; streetyp *lasttp[4];`), so the compiler gives each view
the host layout, as the car record's views have its visuals (section 2.2). The tests that name fields by MAT3's
offsets map them with `MODEL_OFS(n)` (`port/assets/tests/support.h`).

What changed with the pointers:

- The readers that converted read and store the pointers themselves: `N64P()` at each dereference of `car`, `tune`
  and `movingSurface` (forces1, forces2, antispin, autoshift, dotireforce, enginetorque, torques, positions,
  init_road, update_game_data, whatslips, MovingSurfaceForce, TireSurf, CollideCarBody), the casts of `PC_N64P(m->car)`
  (calcaa, steer_constants, transmission, find_best_gear), engine's `CURVE()` of the torque curve (enginetorque takes
  a `const s16 *`), carsetup's and init_cars's `PC_N64W` stores, init_collision's `PC_N64A(stree_nodes)`. The link
  enums (`enum n64p_tCar` ...) and the files' `N64P` macros went with them.
- The 0x808 strides: dead_reckon_all, modelgo, modelstop, AbortCar and update_model reach record i as `PC_MODEL(i)`;
  init_cars clears `sizeof(tModelData)`, the host size of its view. road clears `movingSurface` as a pointer (it was
  an `s32` store there, which would have left half of it). UpdateCarSounds read the position through
  `(u8 *)model + 0x22C`, a raw MAT3 offset (in a branch no car takes: a drone's screech in a human's block), now its
  view's `rwr`.
- The names follow MAT3: TireSurf's and CollideCarBody's `node[4]` is `lasttp[4]`.

**The physics descriptors** are `car_descriptors[6]`, declared in `include/game/cars.h` with MAT3's `_tCar`
(`include/game/types.h`): MAT3's six (basic, forward, backward, shortie, stretch and truck, in MAT3's order) at N64
0x801108D0..0x80110D08, after the steady camera's `steady_leg_duration` (MAT3 `legDuration[2]`, whose `f32[272]`
declaration had run over them; the loader named them `steady_leg_duration+0x8`). `gCarModels[13]` points at them
by car type. Their `tires[4]` are host pointers to `frontTire`'s two `TireDesc` records (MAT3 `frontTire` and
`backTire`), so the table is own storage: 0xC8 bytes a descriptor, the floats after the pointers 0x10 further on than
on the N64. The data loader fills it from the image and resolves the tire pointers (into the game arena) and
gCarModels' (into it) as it does for any own storage ([DATA.md](DATA.md) 4). Only pointers reach the descriptors and
the tire records, and the loader places only data some file names, so carsetup's `pc_car_setup_data` names both
(it named `frontTire` alone as `pc_tire_records`). Every view of a descriptor (the physics files' `tCar`s, the car
select's `ModelDescription`) declares `tires` as pointers: `view_check.py --tag tCar,_tCar,ModelDescription
--n64-size 0xB4` finds 157 views, none disagreeing.

Before wave 3 the record and the descriptor kept their N64 layouts: every pointee of their pointer fields was in the
RDRAM window, so they held u32 window addresses (`enum n64p_X` fields, `N64P()` at each dereference, `N64W()` at
each store), and code stepping by 0x808 landed on the fields. The crossing counter ([NATIVE.md](NATIVE.md) 9) measured
those translations as the bulk of the game C's crossings into the arenas.

## 5. Tools and checks

- `python3 tools/port/view_audit.py --build build/pc --symbols pc_cars --size 952 --ref StartWeapon.c` (the car;
  the reference is `struct pc_car` now: 222 views, 0 disagreeing), `--symbols pc_models --size 2056 --ref
  init_cars.c` (the model; the reference is `struct pc_model`, with its host pointers: 143 views, 0 disagreeing),
  `--symbols gEnvirons,gEnvList,gFreeVis --size 24 --tags tVisual,Visual,Node,... --ref StartWeapon.c` (the
  visuals): every view's named fields at the host offsets the reference's field program gives, and its pointers
  where the reference has them (intptr_t counts as a pointer).
- `python3 tools/port/view_pads.py ... --name "0x110=visuals:struct pc_visual" --name 0x380=input [--pad REGEX]
  [--apply]`: the padding split of section 2.2. It lists what it leaves for a hand edit.
- `test_cars` (`port/assets/tests/test_cars.c`, ctest `assets_cars`) runs the game's own code on synthetic records:
  init_cars (`init_cars` with `InitCarVisuals` and `InitCarPath`), StartEnvironment and
  StartCar with every starter (StartFrame, StartSparks, StartWing, StartWingFlame, StartWeapon, brake lights,
  tires, StartShadow, StartArrow) in a normal and a battle race, the stop loop `RemoveCar`, the environment pool,
  the raw-offset functions of section 2.3, and the model set-up `carsetup` with `steer_constants` and
  `find_best_gear`. It checks `CAR_OFS` against the layout's `offsetof`s and every field the code writes at its PC
  offset (the model's through `MODEL_OFS`). The car array, the model array and the descriptors are host storage, as
  in the game; the checkpoint block, `gCarParts`, the tune table and the torque curve are absolute symbols at their
  N64 addresses in a window the test maps, with their GOT slots put back by `port/ultra/absfix.c` as `rush2049pc`
  does. Reverting a `CAR_OFS` or a model pointer to a u32 makes it fail. `assets_models` checks `struct pc_model`'s
  offsets (each `MODEL_OFS` of MAT3's) and its size, `assets_model` the m2c readers through the pointers.
- `n64_neutral.py --base 09da2f0`: every changed game file is code-exact or shifted (0 FAIL).

| | Before | After |
|---|---:|---:|
| `gGameCar` storage | 6,720 B (1,120 a car), link warning | 6,768 B (0x468 a car), no warning; since DECOUPLE.md 4, `pc_cars`: 13,536 B (12 of 0x468) |
| `D_8014A250` storage | own, 12,384 B (0x810 a model) | arena at 0x8014A250, 12,336 B; since DECOUPLE.md 3, `pc_models`: own, 24,672 B (12 of 0x808) |
| gen_data pointer conflicts, all symbols / these | 394 / 154 | 241 / 1 |
| view_audit, car views disagreeing | 137 of 140 | 0 of 134 |
| model views with pointer fields | 15 | 0 (120 views agree) |
| view_audit, visual views disagreeing (before the visual edits) | 17 of 25 | 1 of 30 |
| census pointer warnings | 371 | 370 |
| proto_check harmful | 27 views of 9 functions | 26 views of 8 functions |

gen_data's width conflicts for these symbols went up (they were listed as pointer conflicts before): partial views
that name different fields at different places. They are not width disagreements at one offset.

## 6. What is left

- **The target info table `targetInfo`**, **the targets** and **the input records `gPlayers`**: done in
  [POINTERS.md](POINTERS.md) 10. The table is 0x40 bytes a record on the PC (its start and visual functions are
  host functions); the targets are the level-object nodes, which keep their N64 layout in the arena with 32-bit
  links and state (section 3's reasoning holds: everything a target holds is a window address); an input record is
  80 bytes in every view, so `init_cars` gives every car its own record. `AnimateFlag`'s `Obj` (a target
  visual, padded over `next`) has the visual's PC layout there too.
- The heuristics that found partial views and raw offsets (DWARF: padding spanning the visuals and car-like fields
  after it; text: car pointers in m2c byte arithmetic, `pad0EC[x]`, `M2C_FIELD` at car offsets, with
  `-Wint-conversion` for implicit conversions) can miss a view that reaches cars some other way.

**Needs the ROM to confirm:** that `gCarModels`'s pointers all point at `car_descriptors` (the data report's summary
counts them as `own`; MAT3's table gives descriptors 0, 0, 0, 0, 0, 1, 0, 0, 3, 4, 5, 2, 0) and that the descriptors'
`tires` point at `frontTire`'s two records (`arena`); that no other pointer in the ROM's data points into the
descriptors' range at an offset that isn't a float's (the synthetic cartridge's `carSuspensions` does, at a second
descriptor's tire positions, which the loader maps through the descriptor's runs); and the visual starters and
animators in a real race (`--screenshot-at` in a race).
