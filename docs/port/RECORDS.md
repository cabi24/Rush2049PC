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

The car type's physics and what the model record holds of it are in `include/game/carphys.h`; a car's respawn, the
track's world parts, a wall contact and a cone's flight in `include/game/track.h`. Each was declared again, in part,
by every file that reads it.

| Record | Declared in | What it holds | Its other declarations (files) |
|---|---|---|---|
| `_tCar` (MAT3) | carphys.h | a car type's physics descriptor: inertia, springs, damping, steering, drag, its tires' constants (pointers to `frontTire`), the tires' places, the drivetrain's constants. `car_descriptors[6]`, `gCarModels[13]` (both declared there now, from cars.h) | `tCar` (find_best_gear, transmission, autoshift, calcaa, steer_constants, torques, whatslips/group, forces1, carsetup/group, visuals/AnimateTire; unnamed in init_road, positions), `ModelDescription` (selectcarload) |
| `tTune` | carphys.h | a handling tune, the factors the model applies for the handling chosen. `gTune[3]` | `Tune44` (include/game's), `tTune` (antispin, autoshift, find_best_gear, forces2, transmission, update_game_data, whatslips/group; unnamed in dotireforce, enginetorque) |
| `TireDesc` | carphys.h | a tire's six constants. `frontTire[2]` | (types.h's, moved) |
| `Tire` | carphys.h | a tire of the model record: its constants, the brush model's coefficients for its load, its spin and forces from step to step | `Tire` (copy_tire_info, tire_constants, dotireforce, drivetrain, frictioncircle, carsetup/group; unnamed in sparks/group), `tTire` (resurrect), `tModelTire` (dead_reckon_posuv, FinishResurrecting), `tTireState` (update_game_data), `tiredes` (sym/group), `TireForces` (init_mdrive/group), `TireView` (visuals/AnimateTire), forces1's `u8 tires[4][92]` |
| `tReckon` | carphys.h | the model record's dead reckoning: what the game thread sees of a car between model steps | `tReckon` (dead_reckon_posuv, resurrect) |
| `tResurrect` | track.h | a car's respawn: the point, its orientation, the flight there | `tResurrect` (UpdateResurrectingCar, resurrect) |
| `tPathPoint` (MAT3) | port/include/pc_game_types.h, beside the car record that holds it (`nav`, MAT3's `mpctl`) | a car's place on its drone path | `struct pc_car_nav` (pc_game_types.h's, renamed; maxpath, SetNewPathIndex, SetDroneResPathIndex, track.h), `tPathPoint` (resurrect, SetResurrectionData) |
| `DronePath`, `DronePathPt` | pc_game_types.h (the path file's converter's too) | a drone path and its points | `tDronePath` (resurrect, SetResurrectionData), `tDronePt` (resurrect), `SearchPoint` (SetResurrectionData) |
| `tModelPart` (MAT3) | track.h | a part of the track's world, a WOBJ record of the world file, read where the file lies (its offsets are the file's) | `tModelPart` (FindWorldGroup, targets/NewWorldObject, targets/group, nonmatching WorldCullObjects), `struct Node104` (scene.h's `cur_world_obj`) |
| `CCol` | track.h | a car's contact with a wall of the road tree | `CCol` (stree_collide's CollideCarBody, CollideCarBodyAgainstSurface, group) |
| `tTargetStructVelRot` (MAT3) | track.h | a knocked-over cone's flight, its target's data | `VelRot` (StartCone, AnimateVelRotGeneric) |

Where the copies disagreed:

- **The descriptor's tire places**: `f32 tireR[4][3]` (MAT3's) where forces1 and carsetup had a struct of x, y, z and
  steer_constants a lone `tireR2z`. carsetup reads `tireR[i][2]`; forces1 passes each row to `calctireuv`, whose `r`
  is a vector struct, through a commented cast.
- **The tires**: carsetup's `slipflag_word` (a float over the byte and its padding, never read) is MAT3's `s8
  slipflag`. AnimateTire's view began 8 bytes into the tire (`angvel` at +0x40) and init_mdrive's at `sideforce`
  (+0x50); both model views now hold the whole tire at +0x430, their padding moved to match. forces1 kept the tires
  as bytes and passed `m->tires[i]`; it passes `&m->tires[i]`.
- **The reckoning's orientation**: `f32 uvs[9]` (dead_reckon_posuv) or `[3][3]` (resurrect, MAT3): `[3][3]`.
- **The path point**: SetResurrectionData's stopped at `path` (0x2A bytes); its car view's padding after it is two
  bytes shorter. Its drone path's points were `s16[4]` rows; they are `DronePathPt`'s `pos`. Its `DRONE_PATH(p)`
  stays (`((DronePath *) gDronePaths)[p]`): through a pointer to the first element the unoptimised code addresses the
  path in the original's order.
- **The world part's matrix** (MAT3 `tUVMatrix`, B2's): `f32[12]` (NewWorldObject), `uvs[4][3]` (WorldCullObjects),
  `UVMat` (targets/group), a lone `pos` (FindWorldGroup). It is `UVMatrixRows` (`uvs[4][3]`: rows 0-2 the orientation,
  row 3 the position) until the merge gives it `tUVMatrix`: WorldCullObjects indexes the position's row by a
  variable, and `uvs[3][k]` keeps its code. NewWorldObject passes `&ptr->mat` and the part's `name` where it passed
  `ptr->mat` and `ptr`.
- **The wall contact's frame** stays `f32 uvs[9]` (MAT3 `[3][3]`): every reader takes rows 3-5 as the normal by
  index, and the vector functions take a `f32 *`.

Until the merge carphys.h keeps `typedef tTune Tune44`: the model record's views in the physics files (A1's,
section 2.1) still name the tune that way, and the alias goes with them.

Units not `same` under objsame: none in Debug. In RelWithDebInfo, `resurrect.c`: in `Resurrect`'s drone branch
(MAT3's FindDroneResPt, written inline) `path = &gDronePaths[car->mpctl.path]` was a `tDronePath *`, a type of the
file's own; with `DronePath *`, the array's own type, GCC forwards the address into the two reads (`path->numPts`,
`path->pts`) and computes it at each instead of keeping it in a register. The same two loads from the same address,
nothing stored between: the behaviour is the same (the gate's paths, RelWithDebInfo against the reference).

Left, and whose:

- `Cr` (rear_camera, steady_camera): the cameras' view of the car record (B1's, section 3.1), not the physics'.
- resurrect.c's `tInitIn` and `tFastIn` (MAT3's `initin`, `fastin`): the model record's (A1's).
- Fields of these records that model and car views name one by one (`reckon_rwr`, `resurrect_save_pos`,
  `slipflag0`, InitCarPath's `xrel` ... `basePath`): they become `reckon.rwr`, `tires[0].slipflag`, `mpctl.idx` with
  the model and car records (A1's, B1's).
- `tPathPoint`, `DronePath`, `DronePathPt`, `AltPath` and `ResurrectPath` stay in pc_game_types.h: the car record that
  holds a `tPathPoint` is declared there, and the path file's converter (port/assets/path.c) builds the paths.

Numbers in this area (src/ and nonmatching/), before and after: local declarations of these records 54 and 0; tags
declared in more than one file 11 (`tCar`, `tTune`, `Tire`, `tModelPart`, `CCol`, `tResurrect`, `tReckon`,
`tPathPoint`, `tModelTire`, `tDronePath`, `VelRot`) and 0; pad arrays in those declarations 53 and 0; `VIEW()` lines
none before.

The checks, against a build of the base (7d065d8e): `view_check.py --tag` over `_tCar`, `Tire` and `tTune`, no view
disagreeing; `layout_check.py` and `offset_check.py --strict` the base's findings, finding for finding (layout_check's
six open ones are the heap's, the sample DMA list's and the sequencer's, none of these records); `alias_check.py` and
`bounds_check.py` nothing new; `census.py` and `n64_audit.py` the base's counts (only line numbers moved); gen_data's
data.c and symbol lists the reference's but the type names of `gTune` and `cur_world_obj`.

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
