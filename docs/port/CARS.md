# The car, visual and model records on the PC

The game keeps two per-car arrays, and the C views them through many struct declarations: the car record
`player_array` (`D_80152818`, MAT3 `gGameCar`, `tCarData`) and the model record `D_8014A250` (MAT3 `model[]`,
`tModelData`). A third record type lives inside the car, the visual (`tVisual`), which also makes up the
environment visual pool. Natively the views disagreed: gen_data laid out 1,120 bytes a car while about 40 views
indexed cars by 952, the link warned that `D_80152818`'s size changed, and the model record's pointer fields moved
everything after them. This page gives each record one PC layout, says why, and lists what was changed and what is
left. 2026-10-27. Related: [POINTERS.md](POINTERS.md) (the N64-neutral conventions), [DATA.md](DATA.md) (where data
symbols live), [PROTOTYPES.md](PROTOTYPES.md).

## 1. Summary

| Record | N64 | PC | Storage on the PC | Why |
|---|---:|---:|---|---|
| car, `player_array[6]` | 0x3B8 | **0x468** | own storage (host, above 4 GB) | its visuals hold function pointers, and +0x380 points at own storage: those must be host pointers |
| visual, `tVisual` (21 per car at +0x110; the environment pool `D_80138880[100]`) | 0x18 | **0x20** | inside the car; the pool is C-defined | `next` and `func` are host pointers; `data` holds flags or a window address |
| model, `D_8014A250[6]` | 0x808 | **0x808** | the game arena, at its N64 address | every pointee of its five pointer fields is in the RDRAM window, so they are u32 window addresses and the N64 layout stays |
| car physics descriptor (`model->car`, 0xB4) | 0xB4 | 0xB4 | the game arena (data, 0x801108D0..) | its four tire-curve pointers point at game data: u32 |

Results: `view_audit.py` finds 0 disagreeing views of the car (134 views) and of the model (120), and one of the
visual (30, `func_8010D85C`'s, left to the front-end work); the link no longer warns about `D_80152818`;
gen_data's pointer conflicts for these symbols are down from 154 to 1 (a raw byte view); `test_cars` runs the
game's own car and model code on synthetic records.

## 2. The car record

### 2.1 Pointer fields and their pointees

| N64 offset | Field | Pointee | PC |
|---|---|---|---|
| +0x110 + 0x18k (k < 21) | `visuals[k].next` | another visual (unused for car visuals; the car array is host storage) | `void *`, 8 |
| +0x124 + 0x18k | `visuals[k].func` | the visual's animator, a host function (`AnimateTire`, `AnimateShadow`, ...) | function pointer, 8 |
| +0x11C + 0x18k | `visuals[k].data` | flags (`VIS_BIT` ...) or a float (the tires' angle) | u32 |
| +0x380 | `input` | the car's input record `&D_8014A118[i]` (own storage), NULL for drones | `void *`, 8 |

So the car can't keep its N64 size, and the PC layout is the N64 field order with those widened:

| N64 | PC | |
|---|---|---|
| 0x000–0x110 | 0x000–0x110 | unchanged |
| 0x110 + 0x18k + f | 0x110 + 0x20k + f' | f' = f for `next` (f < 4), f + 4 after it (`index` +8, `objnum` +0xA, `slot` +0xC, `data` +0x10, `timeStamp` +0x14, `func` +0x18) |
| 0x308–0x380 | 0x3B0–0x428 | +0xA8 |
| 0x380 (`input`) | 0x428 | 8 bytes |
| 0x384–0x3B8 | 0x430–0x464 | +0xAC, then 4 bytes of padding to 8-byte alignment |
| stride 0x3B8 | **0x468** | |

`port/include/pc_prelude.h` has it as `struct pc_visual`, `PC_CAR_SIZE` and `CAR_OFS(n)` (the PC offset of N64
offset n, which may run into later cars: `CAR_OFS(0x3B8)` is the stride) and `VIS_OFS(x)` (inside a visual,
`VIS_OFS(0x18)` = 0x20). Game files that use them define the N64 side, `#define CAR_OFS(n) n`, so the N64 tokens are
the original ones.

### 2.2 The views

Every view declares the visuals and the input pointer as pointer-sized members, with N64-sized padding between
them, so the compiler gives each one the layout above and gen_data computes the right N64 offsets from the PC
DWARF:

- **Padding over the pointers** is split under `TARGET_PC` by `tools/port/view_pads.py` (`u8 pad0F0[0x35B - 0xF0]`
  becomes `u8 pad0F0[0x110 - 0xF0]; struct pc_visual visuals[21]; u8 pad0F0_308[0x35B - 0x308];`), the original
  line kept as the `#else` branch, in about 140 files (overlay B's and the nonmatching drafts among them); and in
  every copy of `include/game_types.h`'s `GameCar` context block (426 files, most of which never use it).
- **Views that name fields there**, by hand: StartSparks' `CarSlot` (visuals 1–5 seen from +4, its last word is the
  next visual's `next`), the wing visuals as `Side24` (`entity_anim_texture`), the `SndSlot` sound slots of the
  `audio_frame` groups, `startcar.c`'s `tVisual`, `GC2.w380`, rocket's `pad380`/`b381`, `random_float`'s car view
  (its `CENTERFORCE` belongs to `MODELDAT`), and `music_control`, whose view counted visuals from +0xF8, one before
  the first: on the PC it starts at the first and its indices go through `VISUAL(n)` (n - 1).
- **Partial views through pointers**, found by DWARF (structs whose padding spans the visuals and that name car
  fields after them): `func_800EC270`'s `CarB`, the `Cr` views of `steady_camera` and `func_800E92C8` (with `Q *q` at
  +0x380), `world_object_destroy`'s, and the `Object` views of `func_800E8D50`, `func_800E8F10`, `func_800E9E2C`,
  `func_800EB028`.
- **Names inside the array** (`D_801528F0` +0xD8, `D_80152900` +0xE8, `D_80152906`/7/8 +0xEE..0xF0, `D_80152BBB`
  +0x3A3) are declared as arrays of a car seen from that offset. Most start at an odd offset, where a struct with
  pointers can't sit, so on the PC they keep only their stride (`u8 rest[PC_CAR_SIZE - 1]`); gen_data places those
  names at the mapped offset (`D_80152B70 = D_80152818 + 0x400`) whatever they declare.

### 2.3 Code at raw offsets

m2c code that reaches cars with byte arithmetic goes through `CAR_OFS` (on existing lines; the N64 expansion is the
original token):

| Site | What |
|---|---|
| `gameover_validOption` (+ weak copy), `race_init_helper`, `func_800CF604`, `func_8010DF90`, `func_80105DA8` | `* 0x3B8` strides on names inside the array |
| `func_800B08FC`, `func_800B0A88` (StartWingFlame, StartWing; both group copies) | the wing visuals at +0x2C0 and +0x290 |
| `save_load_data` (StartFrame) | visual 14's func (stored pointer-sized now: it was `(s32)AnimateFrame`), objnum, slot, the scale at +0x374 |
| `func_800C4078`, `func_8010C7F4`, `func_800E9234`, `func_800E9C70` (`motion_norm`), `func_800CFCA8`, `func_800E56F8` | fields past the visuals through car pointers (+0x33A, +0x384, +0x35C–E, +0x30C, +0x380, +0x388) |
| `players_race_update`, `func_800C4C9C`, `func_800C54F0` (3 copies), `car_select_handler`, `func_800CFCA8`, `func_800E56F8` | `GameCar`'s `pad0EC[x]` used as a byte window from +0xEC: `PAD0EC(p->pad0EC, x)` is `CAR_OFS(0xEC + x)` on the PC |
| `func_800B1F30` (the car visuals' stop loop) | `VIS_OFS` for the func, objnum and stride |
| `func_800B27E4` (visual init) | a PC version of the function over `struct pc_visual` |

`world_physics_tick` clears cars with `sizeof(Player)`, which is the PC size. Code that builds a car pointer and
passes it on was followed to its callees (`func_800E95DC`, `func_800EA2DC`, `func_8038D3A4`, the maxpath functions):
they use views, not raw offsets.

## 3. The visual record

On the PC a visual is `{void *next; s16 index, objnum, slot; u32 data; f32 timeStamp; void (*func)(); }`, 0x20
bytes, in the cars and in the environment visual pool `D_80138880` (100 nodes, defined in `func_800B2BDC.c`;
`GrabEnvEntry` `func_80090284`, the list `D_801391F0`, `entity_transform_apply` to free one).

`data` stays a u32. For car visuals it holds flags or a float. For environment visuals it holds what the visual
animates, and every such pointee is in the window: targets and level-object nodes, which `level_objects` allocates
from the heap per category (`func_8010C974`, `func_8010DBB8`, `func_8010E72C`, `func_8010D9CC`, `func_8010E4E4`,
`Animation24`), and `gCamera` entries in the game arena (`CamNode`). Those views store it through `N64W()` and read
it through `N64P()`. `func` is pointer-sized in every view: the pool's `Node` views had it as `u32` (and `data` as a
pointer), `StartCone`, `BNode`, `CamNode`, rocket's `Entity` and `ModelObj` as an integer (now `intptr_t`), and the
callbacks' own views (`entity_process_main`, `entity_update_callback`, `buffer_swap`, the tire views) had padding
standing for `next`.

## 4. The model record

`D_8014A250` keeps its N64 layout (0x808 bytes): its pointer fields are u32 window addresses on the PC, so it has no
pointers, gen_data puts it in the game arena at 0x8014A250, the 120 views (most of them padding plus a few fields)
already agree with it, and the 0x808 strides of m2c code (`(u8 *)&D_8014A250 + i * 0x808`, `D_8014A964` and the
other names inside it) stay right.

| Offset | Field (names in the views) | Pointee | Where it is |
|---|---|---|---|
| +0x000 | `car`, `parameters`, `object` | the car's physics descriptor (`D_80110D08[type]`, 0xB4 bytes) | game data at 0x801108D0.. (inside `D_801108C8`, arena) |
| +0x004 | `tune`, `tuning`, `sub` | `&D_801116D0[tune_type]` (44-byte tune records) | arena |
| +0x3A0 | `node[4]` | each tire's road quadtree node (`entity_update`) | the road file, heap |
| +0x40C | `torque_curve` | `D_8011F724` | arena |
| +0x648 | `p1608`, `push`, `direction` | the road polygon a tire is on | the road file, heap |

The 15 views that declared them as pointers have them as `enum n64p_X` (`ptr64_u32fields.py`; `N64P()` at each
dereference, `N64W()` at each store): `MODELDAT` (`func_800D0424`, `menu_control_settings`), `State`
(`func_800E15A0`), `Model2056` (`func_800E1C30`, `func_800E31D4`, `physics_clutch`), `VehicleModel` (`func_800E23A4`),
`Vehicle2056` (`car_resistance`, `loadcars/root.c`), `tModelData` (`update_game_data`), `Car` (`world_physics_tick`,
which defines the array, `func_800E1AA0`, `func_800E1F80`), `Model` (`camera_follow_target`), `EUCar`
(`entity_update`). Two m2c functions read +0 and +4 at raw offsets as pointers and now read u32 (`MODEL_P` in
`func_800D0894`, `M2C_N64P` in `func_800E313C`), and `func_800E2F00` passes the torque curve to `func_800E2D18`
zero-extended instead of as an `s32` (a harmful `width` view in proto_check).

The physics descriptor keeps its N64 layout too, since it is arena data: its tire-curve pointers at +0x60 are u32 in
`car_resistance`'s `Car` and the `CarConfig` views (`anim_tire_donor`, `visibility_wrappers`), so `tire_positions`
(+0x78) are where the data has them.

## 5. Tools and checks

- `python3 tools/port/view_audit.py --build build/pc --symbols D_80152818,player_array --size 952 --ref
  func_8038C910.c` (the car), `--symbols D_8014A250 --size 2056 --ref world_physics_tick.c` (the model),
  `--symbols D_80138880,D_801391F0,D_801392C8 --size 24 --tags tVisual,Visual,Node,... --ref func_8038C910.c` (the
  visuals): every view's named fields at the host offsets the reference's field program gives, and its pointers
  where the reference has them (intptr_t counts as a pointer).
- `python3 tools/port/view_pads.py ... --name "0x110=visuals:struct pc_visual" --name 0x380=input [--pad REGEX]
  [--apply]`: the padding split of section 2.2. It lists what it leaves for a hand edit.
- `test_cars` (`port/assets/tests/test_cars.c`, ctest `assets_cars`) runs the game's own code on synthetic records:
  init_cars (`world_physics_tick` with `func_800B27E4` and `func_800EC270`), StartEnvironment (`music_control`) and
  StartCar with every starter (StartFrame, StartSparks, StartWing, StartWingFlame, StartWeapon, brake lights,
  tires, StartShadow, StartArrow) in a normal and a battle race, the stop loop `func_800B1F30`, the environment pool,
  the raw-offset functions of section 2.3, and the model set-up `track_info_display` with `func_800D0894` and
  `func_800E313C`. It checks `CAR_OFS` against the layout's `offsetof`s and every field the code writes at its PC
  offset. The car array is host storage, as in the game; the model array, the checkpoint block, `gCarParts`, the
  tune table and the torque curve are absolute symbols at their N64 addresses in a window the test maps, with
  their GOT slots put back by `port/ultra/absfix.c` as `rush2049pc` does. Reverting a `CAR_OFS` or a u32 model read
  makes it fail.
- `n64_neutral.py --base 09da2f0`: every changed game file is code-exact or shifted (0 FAIL).

| | Before | After |
|---|---:|---:|
| `player_array` storage | 6,720 B (1,120 a car), link warning | 6,768 B (0x468 a car), no warning |
| `D_8014A250` storage | own, 12,384 B (0x810 a model) | arena at 0x8014A250, 12,336 B |
| gen_data pointer conflicts, all symbols / these | 394 / 154 | 241 / 1 |
| view_audit, car views disagreeing | 137 of 140 | 0 of 134 |
| model views with pointer fields | 15 | 0 (120 views agree) |
| view_audit, visual views disagreeing (before the visual edits) | 17 of 25 | 1 of 30 |
| census pointer warnings | 371 | 370 |
| proto_check harmful | 27 views of 9 functions | 26 views of 8 functions |

gen_data's width conflicts for these symbols went up (they were listed as pointer conflicts before): partial views
that name different fields at different places. They are not width disagreements at one offset.

## 6. What is left

- **The target info table `D_80117530`** (48-byte N64 records, MAT3 `targetInfo`): `level_objects`' view has four
  pointers (name, file, callback, animation = the visual's func), others one or none, and the storage has the
  layout of `func_8010DBB8`'s `TargInfo` (56 bytes, one pointer). The environment visuals' func comes from it through
  32-bit views or raw N64 offsets (`StartCone`'s `vis->f20 = tInfo->f12`, `func_8010D9CC`'s `stat`, `CamNode.w14`
  from `CamTbl.w0C`, and `func_8010D9CC`'s `* 0x30` stride), so a hit target's visual gets a wrong function on the
  PC. It needs its own unification (about 11 views).
- **Targets** are heap records with pointer fields (`next`, `MineData *data`), allocated with N64 sizes by
  `level_objects`, while their views have the host layout.
- **The input records `D_8014A118`** (76 bytes on the N64; 80 on the PC, where `PlayerRow` has a pointer at +0x48):
  `world_physics_tick` sets `car->input = &D_8014A118[i]` with its 76-byte view, so only car 0's pointer is the start
  of its record. About 30 views index them by 76 (DATA.md).
- **`func_8010D85C`'s `Obj`**, a visual view with a pointer `data` (40 bytes), is in the front-end work's files.
- The heuristics that found partial views and raw offsets (DWARF: padding spanning the visuals and car-like fields
  after it; text: car pointers in m2c byte arithmetic, `pad0EC[x]`, `M2C_FIELD` at car offsets, with
  `-Wint-conversion` for implicit conversions) can miss a view that reaches cars some other way.

**Needs the ROM to confirm:** that `D_80110D08`'s six descriptors are at 0x801108D0.. inside the arena
(`data_report.txt` classes their pointers; the research notes say so) and that their fields are all words (the
arena swaps `D_801108C8` as `f32`); that the targets, level-object nodes and road nodes and polygons the u32 fields
hold are in the heap; and the visual starters and animators in a real race (`--screenshot-at` in a race).
