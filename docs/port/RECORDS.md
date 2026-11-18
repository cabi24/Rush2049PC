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

(Agent B1: the game car, MAT3's CAR_DATA.)

### 3.2 The scene

The scene's records, each declared once. Most are in `include/game/scene.h`, the vector in `maths.h` and the
camera in `camera.h`. Four records live in `port/include/pc_game_types.h` instead: the matrix, the visual, the
blasts and a tire's skid. The PC's own data there (the car records `pc_cars`, the blasts `pc_blasts`, the skids
`pc_skids`) embeds them, and every game file sees that header through the prelude. They could move to scene.h
once the records that embed them move to include/game.

| Record | Where | What it holds | Its other tags before |
|---|---|---|---|
| `tVec3` | maths.h | a point or a direction: x, y, z (MAT3's) | `Vec3` (5 files), `V3` (2) |
| `tUVMatrix` | pc_game_types.h | a frame: the unit vectors of its axes by rows, then its position (MAT3's mat3 view) | 15 files' `tUVMatrix`, selectcarload's `Transform`, trackscr's `UVMatrix`, carscr's unused `ZMatrix`, `struct pc_uv_matrix` |
| `tCamera` | camera.h | a window's camera: its frame, the frame the scene draw looks by, the fixed camera's (moved from types.h) | (one already; its frames were `UVMat`) |
| `tWindowInfo` | scene.h | a window: viewport, scissor, projection, clip planes, fog; `ZoidWinInfo[4]` | 13 files' `tWindowInfo`, types.h's `ArbEntry` |
| `ObjectInstance` | scene.h | a scene node: matrix, object, tree links, lights, textures, tile, palette, colours (moved from types.h) | 2 files' `ObjInstance` |
| `tPoly`, `tPolyVertex` | scene.h | a flat polygon and its corners; `PolyInstanceList`, `backPolys` | 5 files' `tPoly`, 3 files' `Poly`, updateBG's `PointList`, two files' `Header`, types.h's `PolyInstance` |
| `tVisual` | pc_game_types.h (`struct pc_visual`) | something its animator moves each frame: 21 a car, the pool `gEnvirons` | 20 files' `tVisual`, 2 files' `Visual`, types.h's `EnvironNode` |
| `tSkid` | scene.h | a skid mark: pool links, its polygon, its middle, colour, whether it fades; `gSkidArray` | 3 files' `tSkid`, skid_marks' `EffectObj`, types.h's `PcEffectObj` |
| `tNewSkid` | pc_game_types.h | a tire's skid: its mark, start, the tire, direction, quad; `pc_skids` | skid_marks' `WheelSlot`, StopSkid's `tNewSkid`, `struct pc_skid` |
| `SmokePuff` | scene.h | a smoke or spray puff: its frame, scene handle, life, drift, car; `gSmokeObjs` | 3 files' `SmokePuff`, types.h's `PcPuff` |
| `tBlastObj`, `tBlastStruct`, `tBlastRingStruct` | pc_game_types.h | a car's fireball (four flame balls and a glow) and shock ring; `pc_blasts`, `pc_blast_rings` | 3 files' `tBlastObj`, 2 files' `tBlastRingStruct`, InitBlast's `tBlastStruct`, `struct pc_blast_obj`, `pc_blast`, `pc_blast_ring` |
| `tBlastMat` | scene.h | a blast sprite's matrix; `gBlastMats` (moved from types.h) | (one already) |
| `tPaletteAnim`, `tTextureSwap`, `tTextureBase` | scene.h | a track's palette animation, a texture flipbook and its frame; `PaletteLists`, `TextureSwapLists`, `texture_swap_shared_frames` | 2 files' each, types.h's `Item12`, the `Row20` and `Sequence20` tags |
| `CarTlut` | scene.h | a car's palette's colours: paint, tail lights, headlights | 2 files' `CarTlut` |

Where the files disagreed:

- **`tWindowInfo.fogmin`**: MAT3 and two writers have it signed; every reader takes it unsigned (`u16`).
- **`tPaletteAnim.state`**: one file read it `u8`, the other `s8` (MAT3). It is `s8`. TextureAnimation's switch reads
  it `(u8)` (the states are 0 to 10), so its code is the same.
- **The visual's data word** holds a pointer or a number; a tire's turned angle is a float there, so
  `struct pc_visual` has it in an anonymous union (`data`, `angle`). Its animator `func` takes the visual. The tire
  file's `union` view and AnimateWing's `object`, `flags` and `time` are `objnum`, `data` and `timeStamp`.
- **Scene handles**: a smoke puff's `mbIdx` and a ring's `ringIdx` are the s32 MBOX_NewObject returns. The files read
  the low half as the node through a union of their own, `Id`. The records have it as `mbNode` and `ringNode`, in an
  anonymous union with the handle (the low half on the PC, which is little-endian).
- **`tPaletteAnim.ptr`** is the palette's colours or a scroll's display list: a union (`palette`, `commands`), as
  TextureAnimation had it.
- **`tPolyVertex`** has MAT3's names (`ob`, `tc`, `cn`); `cn` is a `tRGBA`. The vertex views called them `pos`,
  `x y z`, `s t`, `color`, `col`.
- **`SmokePuff.tlifeTime`** (MAT3's, a float) is read by no code. It was a byte pad in types.h and an `s32` in one view.

**VIEW lines**, of the 13 in this part:

- Gone: ZOID_InitializeWindow's `ZoidWinInfo` (its true type), carscr's `gCarMatrices` (`tUVMatrix *[48]`),
  initgfx's `frame_dynamic` and `frame_dl`, and StartGraphics' `frame_dynamic`. Those read system.h's `struct Dynamic *`
  and `Gfx *` with the buffer's byte offsets cast where they are added. Also gone: EndGraphics' `frame_dl` and
  `last_frame_dl_end` (its own copy of include/game's `Gfx` went), animArrows' `trackscr_arrowPos` (`ScreenPos16`) and
  recordscr_draw's `recordscr_viewPane` (`tBlitSet *`, MAT3's `viewPane`).
- Left for the merge:
  - EndGraphics' `frame_records`: its frame record's task is libultra's `OSScTask` with `t`, and system.h's
    `FrameRecord` has `PcScTaskShape`, D2's.
  - zoid_draw's `frame_dynamic`: it reads the buffer's `vp`, `vtx`, `la` and `proj` through `GfxBuf`, and system.h's
    `struct Dynamic` is incomplete.
  - zoid_draw's `frame_matrix_cursor`: it steps the cursor as an `Mtx *`, and system.h declares it `u8 *`.
  - recordscr_draw's `gScreenRect`: it is a rectangle of four s16, but frontend.h types it as zlib's `config`, and no
    type for it exists yet (MAT3 `anon_x_y_c`, 12 bytes, C3's frontend).

**Code that isn't the same** (objsame against the wave 4 reference). Debug, 4 units:

- `WorldFogDensity`: the window is `&ZoidWinInfo[winidx]` in 64-bit index arithmetic. Before, it was a 32-bit byte
  offset times its own `WININFO_SIZE`. Windows 0 to 3 give the same address.
- `ZOID_SetFog`: the `s16` argument goes into the `u16` field. It is converted before the window's address is
  loaded instead of after, and the same two bytes are stored.
- `InitBlast`: the fireballs' and rings' scene objects by their fields (`pc_blasts[i].glowIdx = -1` ...). It was a
  byte pointer plus offsets through `BLAST`, `RING` and `BLAST_MAT` macros. The same stores.
- `AnimateNewBlastRing`: it indexes `pc_blast_rings` directly instead of through a cast of its address, so the
  registers differ.

RelWithDebInfo, 9 units: `WorldFogDensity` (as above), and eight where -O2 now sees a pointer it reads through
as the very type of the array it points into. It folds the field accesses into the array's addressing and so picks
other registers, other addressing modes and another schedule for the same loads and stores. The eight:
`ZOID_SetWindow`, zoid_draw's `zoid_draw_window`, animate_arrow's `AnimateArrow` (the window), `StartEnvironment`
and `StartWeapon` (a car's visual is the car record's own element type), `AnimateNewBlast` and blast.c (the
fireball), `ZOID_NewPoly` (the polygon). `ZOID_SetFog`, `InitBlast` and `AnimateNewBlastRing` are the same there.

**data.c** (gen_data): the same but for type names, and the runs of two own-BSS data, which the loader zeroes and
whose N64 and host offsets are unchanged. `ZoidWinInfo` (ptr, u8 → ptr, u32, f32, u16, u8: its fields, where it
was 64 bytes) and `gSmokeObjs` (`tlifeTime` f32 where it was 4 pad bytes). `PolyInstanceList` (arena BSS) and
`gSkidArray` join data_layout.txt's list of unions copied as bytes (their `tRGBA`).

**Numbers** (src/ and nonmatching/, this part's tags and the same records under other tags):

| | before | after |
|---|---:|---:|
| local declarations | 123 | 26 |
| tags declared in more than one file | 24 | 7 |
| pad arrays in them | 48 | 22 |
| VIEW lines of this part's data | 13 | 4 |

Left, with the agents whose records hold them:

- `Object` (8 files): six are views of the game car (B1: CheckCameraView, update_camera,
  circle_camera_around_car, init_camera_on_track, maxpath_cam, and camera.h's prototypes with `struct Object *`).
  UpdateCircuit's and SetCircuitCode's are a profile file's record (C2/C3). missile_lock's is a missile (C3).
- `Obj` (2): init_steady_move_cam's is the car (B1), ghost_files' a ghost handle (C2).
- `Vec3` and `Basis` in forces1, collision, carsetup and calctireuv: the model record's copies (A1) use them. The
  lead swaps `tVec3` in once those copies are tModelData. physics.h's `calctireuv` prototype names `struct Vec3` and
  `struct Basis`.
- `Vector` in MusyX's vector.c and streams_3d.c (src/rom, which doesn't include include/game), and types.h's
  `Vector`, which the sound records embed (C4). track.h's `zero_velocity` is a `Vector` too.
- types.h's `UVMat`, `MuzzleMat` and `UVMatrixRows` are the matrix under other names. The weapons' and HUD icons'
  records embed them (MuzzleObj, RingState, tCMuzzleObj, tHudIcon, tBattleCoin, tStuntCoin), so the lead swaps
  `tUVMatrix` in. `UVMatrixRows` reads the twelve as four rows, and no code reads row 3. animStuntPoints' own
  `tStuntCoin` copy uses `tUVMatrix` now.
- `tDLList` (audiomgr_thread, paksysdc), the same list header as InsertLLE's, RemoveLLE's and AudioInit's `List`,
  save.c's `ListHead` and types.h's `AudioList`. This is MAT3 ll.c's linked list, not a display list. It belongs with
  the lists (memory.h, D1), and its users are sound (C4), saves (C2) and the pak (D1).
- `LookAt` and `Light` (zoid_draw, lookat_reflect_fill) are gbi's (E).
- `struct pc_visual` keeps its tag: the car record's copies (B1) and frontend.h's, text.h's and track.h's
  prototypes name it. After the merge it can be renamed `tVisual` everywhere.
- `gMatrices` (`s32[560]`, 2,240 bytes, whose object runs 7,200 bytes to the next datum) and `PolyInstanceList`
  (`EXTENT(39)`, whose object runs to 200 polygons) keep their declared extents, so data_symbols.txt stays as it
  was. Their true extents are 150 matrices and 200 polygons.

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
