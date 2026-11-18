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

The 2D sprites of the HUD and the menus and the game's colours (agent B3). Each record below is declared once, in
`include/game/types.h`'s blit part (the blitter's own data in [blit.h](../../include/game/blit.h)), and every file
uses that declaration ([docs/code/ui.md](../code/ui.md)).

| Record | What it holds | Its other tags before |
|---|---|---|
| `tBlit` (MAT3 `_tBlit`) | one blit: texture name, palette or flat colour, texture, place, size, crop, alpha, flip, hide, the animation function, its argument (`animID`) and data (`animDTA`), the zoid entry it owns (`blidx`), the next blit of its multiblit (`child`) | `Blit` (13 files), `HostBlit` (3), animBones' `BonesBlit`, the pause pages' `MenuItem` (3); the headers' `struct Blit *`, `struct Voice *` (`HudBlit1`, `HudBlit2`, `gameover_blit`), `struct SoundState *` (`backBlit`) and `void *` (`multiPlayerBlit`, `hudextra_blit`) |
| `tBlitDef` (MAT3 `tBlitDef`) | one entry of a multiblit table: texture name, offset, size, crop, depth, alpha, animation function and its argument | `MultiBlit` (12 files: 11 never used it, ShowHUD's PC copy of the race set did), frontend.h's `HudBlitDef`, pc_prelude.h's `struct pc_multiblit` (13 tables in audio.h, frontend.h, race.h and save.h; now test_carshud.c's own), `SoundClearRecord` (`frontBack`, `trackBack`) |
| `tBlitSet` (MAT3 `tBlitSet`) | a pane: in use, permanent, the inside's and the border's colours, its five blits | `FiveParts` (6 files, frontend.h's `carscr_pane`, `playerscr_pane`, `circuitscr_pane`), `void *` (`recordscr_viewPane`, `video_gTitlePane`, `gTestPane`, `gConfirmPane`) |
| `tRGBA` (MAT3 `tRGBA`) | a colour: `c[0]` to `c[3]` red, green, blue, alpha in memory; `rgba` moves the four bytes at once | `union Color` (11 files, and cars.h's `GetCarsIRColor`, `SetSheenColor`), `Color4` (4), `RGBA` (3), `MapColor`, anim_car_color_marker's `Color`, TB_SetTextColor's `Color8`; include/game's `BlitColor`, `ArrowColor`, `HudColor`, `ButtonColor`, `ColorWord`, `Color8` (`gFontColors` is `tRGBA[22][2]`, MAT3's `gFontColors[22][2][4]`), and `PcText4` for `modescr_button_color` |
| `PaneColors` | a pane's two colours as `ChangePaneColor` takes them, the inside's and the border's (`hud_playerPaneColors`) | its fields were `u8 x[4], y[4]` |
| `BlitXluColors` (blit.h) | `blitPrimColor`: the primitive and the environment colour `BlitXLU` loads | two `BlitColor` |

**Fields that disagreed.** Every local view of the blit read `hide` as `s8`; `tBlit` and its users (UpdateBlit, the
radar, the tachometer, the credits) read it as `u8`, MAT3's `char` (unsigned on the original compiler). It stays
`u8`: where the code read it signed (`Hidden`, `animOdometer`, `animBones`) it reads `(s8)blt->hide`, with a comment,
so the code is the same; the flag is 0 or 1. Six views had `texIndex` as `s16` and five `init` as `s8`; no code
reads either through them. The views named fields by place: `data` (animControls) is `animID`, `half18` and `State`
are `z`, `Texture` (animArrows) and `resource` (trackscr) are `blidx`, `Image` is `palette`, `state` and `AnimDTA`
over the function's slot are `animFunc`. The callbacks that take their blit as `void *` (animBones,
animControllerDim, animate_pause_dim, anim_show_on_high_score) keep that parameter with a typed local, as their
code was.

The colour unions were the same four bytes under other names (`b`/`w`, `channel`/`word`, `c.r`..`c.a`,
`red`..`alpha`): each read is `c[i]`, each word copy `rgba`. Three locals the menus pass to `ZOID_SetColor` needed
four-byte alignment (`PC_WORD_ALIGNED`, [SANITIZE.md](SANITIZE.md) 3.2); a `tRGBA` has it, and the macro is gone.
playerscr's two white locals, `u8[4]` arrays, are `tRGBA` initialized by their word, `{.rgba = 0xFFFFFFFF}` (every
byte 255, the same in any byte order), which compiles as the array's initializer did.

**Data and views.** The 18 views of B3's data are gone, each file reading the datum by its declaration:
`gFlashColor1`, `gFlashColor2` (TB_SetTextColor, through `Color4`), `modescr_button_color`, `carscr_solidColor`,
`carscr_normalColor`, `circuitscr_button_color`, `circuitscr_init_button_color`, `loadTrackScreen_backColor`,
`race_map_vertex_color`, `trackscr_init_button_color`, `trackscr_update_button_color` (through the files' colour
unions), `recordscr_viewPane` (`FiveParts *`), and the six of `flashColors_at_1`, which no code read any more (the
screens read `flashColors[1]`): frontend.h's declaration of the label went with them. The colour data are `tRGBA`:
`hud_playerColor`, `hudextra_playerColor`, `stunt_coin_color`, `weapon_icon_color`, `arrow_base_color`,
`paint_colors_rgb`, `controller_dim_color`, `pause_dim_color`, `carscr_marker_colors`, the circuit and track
screens' button colours, `modescr_button_color`, `gFontColors`. gen_data lays them out as before: a struct of four
bytes and a union with a byte member that covers it are both bytes ([DATA.md](DATA.md) 2); data.c and the symbol list
are the reference's but for type names, and data_layout.txt lists 42 data under "Unions copied as bytes" where it
listed 33 (the nine that were byte structs). The blits' data have their records too: every multiblit table is a
`tBlitDef`, every blit pointer a `tBlit *` and every pane pointer a `tBlitSet *` (but the three below).

**Leftover bytes, kept.** Two environment colours nothing sets are read past their colour, as on the N64: BlitXLU's
(`blitPrimColor.env`, the first word of the Controller Pak names' table after it on the N64, loaded as before) and the
IA blits' (`blit_ia_palette`'s last four bytes, the first two of `blit_tile_lines`, one storage). blit.h and maths.h
say so where they are declared; [NATIVE.md](NATIVE.md) 18.4.

**Left.** weapons.c and shots.c keep their `union RGBA` for one member, the car view's `IRclr` (the typedef says
`wave 4: tRGBA`): the car record is B1's (3.1), whose header gives it its type. The blit animators of hud.h and
frontend.h whose parameter is an `s32` or a `void *` (`animPlace`, `hud_updateFrame`, `animLapCount` ...) keep it:
as a `tBlit *` an unused parameter spills eight bytes instead of four at -O0, which changes the Debug build's code.
Three blit and pane pointers stay `intptr_t` (`highscore_blit`, `audio_gTitlePane`, `options_gTitlePane`): gen_data
lays an `intptr_t` out as the N64's long and a pointer as a pointer, so their type would change their field program.
`trophySheenColor` (`s32[2]`, MAT3's `tRGBA`) and `animPause_color` (`u32`) are colours the game passes to
`ZOID_SetColor` and as a blit's flat colour, read as bytes, but declared as words: the loader swaps them as words,
so on the PC their bytes come out in the other order. Declaring them `tRGBA` would change what the loader does, which
this wave doesn't: the lead's, with the retail cartridge (the circuit screen's trophy sheen, the pause screen's
dimming). The track map's `animTrackMaps` builds a vertex colour from a word constant (`col.rgba = 255`), whose bytes
are the host's order.

**objsame** against the reference (d224beab): every unit `same`, Debug and RelWithDebInfo, after each stage.

| Count (src/, nonmatching/) | Before | After |
|---|---:|---:|
| local declarations of the blit and colour records | 54 | 2 |
| tags declared in more than one file (their declarations) | 7 (50) | 1 (2: `RGBA`, above) |
| VIEW lines on B3's data | 18 | 0 |
| pad arrays in those declarations (and members named pad, reserved or unknown) | 1 (19) | 0 |
| colour types in include/game | 8 (`tRGBA`, `BlitColor`, `ArrowColor`, `HudColor`, `ButtonColor`, `ColorWord`, `Color8`, `PaneColors`'s bytes) | 1 (`tRGBA`; `PaneColors` and `BlitXluColors` are pairs of it) |

The layout tools: layout_check's reference for the blits is `tBlit` (it named NewMultiBlit.c's `Blit`, which that
file no longer had), so 41 more storages have a reference layout, with no new finding; offset_check, alias_check,
bounds_check, census and n64_audit as before.

## 4. Players, saves, menus and sound

### 4.1 Players and their input

(Agent C1: the players, their pads and input records.)

### 4.2 Profiles, records and ghosts

The pak system's and the save code's records are declared in [save.h](../../include/game/save.h) (types.h keeps
only their names, `typedef struct tPak tPak` ..., for the headers before it). The records the game keeps in memory
have no offset comments; the two file formats, a profile's file and a ghost file's header, keep the file's offsets,
as the file's ([SAVES.md](SAVES.md)).

| Record | MAT3 | What it holds | The other tags it had |
|---|---|---|---|
| `tPak`, `tPakFile` | paksysdc.c's pak record | a controller's pak: its state flags, its libultra handle, the rumble's state, its 16 notes' directory entries | (types.h only) |
| `PakFileRecord`, reached through a `tPakHandle` | `tPS_File`, `tPS_File **` | an open pak file: its links on its pak's list (`gPS_Files`), the two functions profiles and ghosts set (`RemoveFile`, `MoveFile`), the pak, the note's number, name and extension, size, pages and data handle | `FileInfo` (carscr, trackscr, playerscr_draw, recordscr_draw), `PakFile` (playerscr, recordscr, pakscr_FindFile), `PSFile` (updateControlScreen), `Item` (circuitscr), `Resource` (modescr), `Record` and `Owner` (pakscr_draw), `Q` (FindGhosts), `Slot` (ghost_files), `tPS_File` (PS_ReadPartialFile); its handle as `PakHandle`, `PakFileHandle`, `Selection` |
| `PakErrorState`, `PakErrorFunc` | pakerr.c's dialog; paksysdc.c's `gErrorFunc` | a pak error dialog; the error callback's type | paksysdc's own `PakErrorFunc` |
| `tProfile` (tag `tPlayerProfile`) | `tPlayerProfile` | a profile in memory: its links on `gPlayerFiles` (handles), its pak file, id, name, controls, rumble and the handle of its file's data | `Profile` (9 files), `PlayerProfile` (3), `PSFile` (state), `HObject` (CheckCameraView), `Object` (SetCircuitCode, UpdateCircuit), `A` (ContinueCircuit), `PlayerState` (options_set), `HN` (rear_camera, steady_camera), `PlayerRec` (GhostStart), `OwnerBody` (circuitscr), `Owner` (modescr), `Player` (PrintScores), `OpenRecord` (pakscr_draw), types.h's `GhostPlayer`; its handle as `Handle` (4 files), `Owner` (playerscr), `MenuOwner` (circuitscr), `ListNode` (recordscr), `Player` (options_set) |
| `tSaveData` and its parts (`tRaceSettings` ... `tCarSettings`) | `tSaveData` | a profile's file on the pak (2064 bytes, the N64's revision 16): its name, options, controls, the race types' settings, the records and the circuit in progress | `SaveData` (circuitscr, recordscr_draw), `Sess` (ContinueCircuit), `Stats` (options_set), `CarCfg` and `TrackRec` (GhostStart), recordscr_draw's `RaceStats` ... `CircuitStats`; its handle as `Resource` (SetCircuitCode, UpdateCircuit, circuitscr) and `Profile` (options_set) |
| `tGhostHeader` | `tSaveHeader` | a ghost file's header (88 bytes), the compressed recording after it | `SaveHeader` (carscr) |
| `tGhostFile` | `tGhostFile` | a ghost in memory: its link on `gGhostFiles`, its pak file, track, direction, driver, time, and the handles of its recording and its file's data | `Ghost` (6 files), `Record` (save, carscr, carscr_animText), `Obj` (ghost_files), `SavedRecord` (pakscr_draw), types.h's `GhostFile`; its handle as `ReplaySlot` (GhostUpdate) |
| `tScoreOwner` | (a `tPlayerProfile **` with casts) | who set a time in the high score tables: a profile's handle, or the index of a name typed on the high score screen, or -1 | types.h's `RecordOwner`, InitRecords' `Table` |

Fields that disagreed, and how they were settled:

- **The ghost header's word at 0x30** was named `bestTime` ("the driver's best time on the track"); GhostStart writes
  the profile's `raceStats[track].totalGhosts` there, as MAT3 does into its `id`. It is `id` now.
- **The profile's id** was `u32 id0, id1` or `u8 id[8]`, a ghost's `s32 key0, key1` or `s32 id0, id1`: `u32 id[2]` and
  `u32 playerId[2]`, MAT3's. The code only compares them for equality, which is the same instruction either way.
- **The profile's name** was `name[24]` in five views (its name, controls and rumble byte as one array) and
  `name[1]` in two; the ghost's `name[14]` or `name[22]` (with its padding, or its id after it): `name[13]`.
- **The links.** A profile's `next` and `prev` are `tProfile **` (the list holds handles); a ghost's link was `ll` in
  types.h and `next` in the files: `next`, a `tGhostFile **` (the ghost list is singly linked).
- **The ghost's handles** were `void **h36` and `h40` (ghost_files), `void *datah` (carscr): `u8 **datah` (the
  decoded recording) and `tGhostHeader **header`.
- **The pak file record's fields** were `update`, `render`, `id`, `seat`, `catchup` (ghost_files), `suffix[15]`
  (pakscr_draw), `slot` (pakscr_draw's `Owner`): `RemoveFile`, `MoveFile`, `pak`, `fileNo`, `size`, `ext[11]`.
- **The save data's circuit bytes** were `option_a` and `option_b` at 1785 and 1787 (options_set) and the byte at 1785
  (trackscr's `CIRCUIT_DEATH_MODE_BYTE`): `circuit.death` and `circuit.difficulty`.
- **The profile tag** was `struct tProfile` in types.h's `PlayerTail` and `struct tPlayerProfile` in `tPadState.save`,
  two incomplete types for one record: the tag is MAT3's `tPlayerProfile` and `tProfile` its typedef, so both complete.

The data, each with its type in its header (VIEW lines removed: 43):

| Datum | Header | Was | Now | Its VIEW lines (files) and how they read it now |
|---|---|---|---|---|
| `gTotalNames` | save.h | `RecordOwner[12]` (`intptr_t names[2][5]`, `Profile **who[5]`) | `tScoreOwner[12][3][5]` | 7: `intptr_t[12][3][5]` (RemovePlayerScores, FindTempNameIdx, ControlName), `Table[12]` (InitRecords), `tProfile **[][15]` (RecordRace), `tProfile **[12][3][5]` (AddPlayerScores), `Player **[][3][5]` (PrintScores); RecordGhost used `.who[n]`. All index `[track][table][place]`, `.name` for an index, `.profile` for a handle |
| `gThisPlayer` | frontend.h | `intptr_t[4]` | `tScoreOwner[4]` | (no VIEW; RankScores and ControlName keep a handle or a name index in it) |
| `gGhosts` | save.h | `struct ReplaySlot *[4]` | `tGhostFile **[4]` | 7: `Ghost **[]` (GhostStart), `Handle[4]` (ghost_files), `intptr_t` (update_model: `(&gGhosts)[m->slot]`), `tGhostFile **[]` (PlayGame, UpdateGame), `Record **[4]` (carscr, carscr_animText); GhostUpdate read `gGhosts[i]->ghost` |
| `gNewTotalScore` | save.h | `s8[16]` | `s8[3][5]` | 4: `s8[3][5]` (ResetHighScores, RankScores, ControlName), `s8[][5]` (highscore); RecordRace and RecordGhost indexed it flat (`[shift + 5]`, `[n + 10]`): `[table][place]` |
| `gUnlockedCars`, `gUnlockedFrames` | save.h | `u8[16]`, `u8[56]` | `s8[4][13]`, `s8[4][6]` (MAT3's) | 3 each: `s8[][13]`, `s8[][6]` (CalcUnlockedStuff, IsCarUnlocked, IsFrameUnlocked, highscore); highscore_InitScreen took `&gUnlockedCars[p * 13]` and recordscr_draw `gUnlockedFrames[i]` (row 0) of the flat bytes: `gUnlockedCars[p]`, `gUnlockedFrames[0][i]` |
| `gUnlockedTracks`, `gUnlockedTrans` | save.h | `s8[1][19]`, `s8[1][8]` | `s8[4][19]`, `s8[4][8]` | (no VIEW: the data loader's extent was one row; the code reads four) |
| `gBattlePoints` | save.h | `s32` | `s8[4][4]` (MAT3's) | 3: `s8[][4]` (RecordBattlePoints, results_table), `s8[]` (RecordBattle: `[i * 4 + k]`): `[scorer][victim]` |
| `gTotal{Stunt,Battle,Obstacle,Circuit}Stats` | save.h | (their types) | (the same) | 4, recordscr_draw's own stats records: save.h's |
| `gSelectedPlayer`, `gStatPlayer` | frontend.h | `struct Owner *[4]`, `struct ListNode *` | `tProfile **[4]`, `tProfile **` | playerscr_draw, recordscr_draw: `(*h)->field` for `h->body->field` |
| `gPakFull` | frontend.h | `void *[4]` | `intptr_t[4]` (a flag, in the pointer-sized slot the data has) | playerscr's `intptr_t[]` |
| `gBlocksNeeded` | frontend.h | `char[5]` | `u8` | 3: `s8` (updateLicenseScreen), `u8` (pakscr_draw, updateWarningScreen); updateHighScoreScreen passes `&gBlocksNeeded` to CompressGhost (`u8 *blocks`, MAT3's) |
| `gErrorFunc` | save.h | `void (*)(...)` | `PakErrorFunc` | 2: the callback with an `s32` last parameter (paksysdc_flush); `intptr_t` (pak_set_error_func, which nothing calls: it converts its 32-bit argument, sign-extended as before) |
| `gEgg` | frontend.h | `union InputSequence *` (CheckForEgg's view of the egg as words) | `Egg *` | start_screen's `Egg *`; CheckForEgg's view of the egg as words (`union InputSequence`) is gone: it reads `gEgg->size`, and a step's buttons still as the egg's words, as a comment says why: its index can pass the 20 steps (the synthetic cartridge's codes are zeros, which an idle pad matches), where `levels[i]` and `edges[i]` would be out of bounds (UBSan's bounds check said so on the cheat paths) |
| `gPasswordAlphabet`, `gDefaultNames` | save.h, frontend.h | `char *`, `char *[55]` | (the same) | circuitscr's `u8 *` views |
| `obscenities` | frontend.h | `ObscenityList` | (the same) | censor's `List` |
| `gGhostPlayers`, `gRealGhostPlayers`, `gNewGhosts`, `gRealNewGhosts`, `gLimboGhosts`, `gRealLimboGhosts`, `limboGhosts`, `gPakGhosts`, `player_profile_handles`, `player_profile_records`, `gPlayerFiles` | race.h, save.h, frontend.h | `struct PlayerRec *`, `GhostPlayer`, `struct Ghost *`, `GhostFile`, `struct Record *` ..., `void **[4][16]`, `tProfile[4][16][1]`, `PlayerFileList` | `tProfile *`, `tProfile`, `tGhostFile *`, `tGhostFile`, `tGhostFile **` ..., `tProfile *[4][16]`, `tProfile[4][16]`, `AudioList` | (no VIEW) |

`gGhosts[m->slot]` (UpdateModel, [NATIVE.md](NATIVE.md) 18.4): it read `(&gGhosts)[m->slot]` through an `intptr_t`
view, which no bounds check sees. It is `gGhosts[m->slot]` of a `tGhostFile **[4]` now, so UBSan's bounds check
(`-fsanitize=bounds`, in the sanitizer builds) checks it on every path that runs a ghost race. The code bounds it
too: the test is made only in a ghost race (`gRace != kGhost ||` comes first), and a ghost race has `gNumCars =
gNumPlayers` of at most 4 (GhostStart: the players, then the ghosts in the slots after them, `n` = 2 or 4), so
`m->slot` is under 4.

The data loader lays every datum out as before: the same storage, size and alignment for every datum, the same
host offsets for every pointer-sized slot. data_symbols.txt and data.c differ in type names, the N64 sizes of the
reshaped tables (`gUnlockedCars` 16 to 52 bytes, `gUnlockedFrames` 56 to 24, `gUnlockedTracks` 19 to 76,
`gUnlockedTrans` 8 to 32, `gNewTotalScore` 16 to 15, `gBattlePoints` 4 to 16, `gBlocksNeeded` 5 to 1: each is
arena BSS, whose object runs to the next datum, so the objects are the same) and the run kinds of three own BSS
data: `gTotalNames` (its 15 slots a track were 10 `intptr_t` and 5 pointers, now 15 `intptr_t`, the union's first
member), `gPakFull` (pointers to `intptr_t`) and `gPlayerFiles` (its first word as `AudioList`'s bytes and count).
BSS is never filled from the ROM, the loader maps each N64 offset to the same host offset, and the state hash
takes pointer and `intptr_t` slots alike.

The save code reaches the records by their fields now: `groups/save/group.c` (the profiles' and ghosts' reading,
checking, creation, the save server), `ghost_files.c` and `NewGhost.c` (the ghosts), `update_model/group.c` (the
rumble request `gPaks[i].request`, the profile's rumble, the ghost header's flag), `trackscr.c`, `options_set`. The
helper macros that stood in for the fields are gone: `F()`, `SLOT_DATA` and `typedef void **Handle` (save/group.c),
`GHOST_HEADER_FLAG`, `PAK_RUMBLE_REQUEST`, `PROFILE_RUMBLE` (update_model), `CIRCUIT_DEATH_MODE_BYTE` (trackscr),
`PROFILE_SAVE`'s cast (recordscr_draw), and pc_prelude.h's `PSF_OFS`, `PROFILE_OFS`, `GHOST_OFS` (test_save.c, which
checks its mirrors of the records against the N64's layouts, keeps them for itself).

The checks: `offset_check.py --strict` no open finding (unconfirmed 8 to 5, untyped raw accesses 808 in 41 functions
to 801 in 40); `layout_check.py` the same 6 open findings as the reference build (none in these records);
`alias_check.py` none open; `bounds_check.py` none new (pairs reached through a variable 26 to 23; the three
reviewed keys whose VIEW lines went are gone from `bounds_reviewed.txt`); `census.py` 1,063 of 1,063, no pointer
warning; `n64_audit.py` no class grew; `view_check.py --sym` no live view disagrees on `gGhostFiles`, `gPS_Files`,
`pak_free_handles`, `gPlayerFiles`, `pak_file_records`, `gPaks`, `gTotalNames`, `gThisPlayer`, `gRealGhostPlayers`,
`gRealNewGhosts`, `player_profile_records` (`gGhosts`, `gSelectedPlayer`, `gStatPlayer`: no view left). offset_check's
two reviews of profile names made with `PROFILE_OFS(20)` (results_table, animBattlePoints) matched nothing any more
and are gone.

Units not `same` under objsame (each passes the synthetic gate; none changes what is loaded or stored):

| Unit | Debug | RelWithDebInfo | Why |
|---|---|---|---|
| groups/save/group.c | yes | yes | the profile's and ghost's fields for `F(p, T, PROFILE_OFS(n))` and byte offsets into the save data: the same loads and stores, with the field's offset in the instruction (`mov 0x10(%rax)`) where the raw form added it first (`add $0x10; mov (%rax)`); CreatePlayer's two list walks have a variable each (they shared one `Handle`) |
| update_model/group.c | yes | yes | the same: the rumble request `gPaks[i].request`, the profile's rumble and the ghost header's flag as fields |
| groups/save/ghost_files.c, trackscr.c | yes | no | the same: SaveGhost's file name and the circuit's death byte as fields |
| groups/save_circuit/group.c | no | yes | its checksums over the circuit's records as fields: -O2 allocates the registers otherwise |
| RecordRace.c, RecordGhost.c, RecordBattle.c, highscore_InitScreen.c, GhostStart.c | yes | yes | a table indexed by row and column (`gNewTotalScore[1][shift]`) where it was indexed flat (`[shift + 5]`), or a field where a byte offset was added (GhostStart's car settings and ghost count): the same address, computed in another order (and, where the flat index was an `int`, in 64 bits) |
| RankScores.c, ControlName.c, PrintScores.c | yes | yes | the union `tScoreOwner`: a store through `.profile` or a union copy loads its operands in another order than the `intptr_t` store with its cast; PrintScores' `.name` index takes one register move less |
| pak_set_error_func.c | yes | no | the conversion of its `s32` to the callback: `cltq` where the `intptr_t` view sign-extended with `movslq` (nothing calls it) |
| hud_text_anims/animResults.c | no | yes | -O2's instruction scheduling: the same instructions, in another order |

The runs (Rocky, against `~/rush2049pc/w4-ref`, d224beab's builds; the machine shared by thirteen agents):

- `synth_compare.py --same --shots --audio --data-moved`, Debug against the reference's Debug, the paths that reach
  this code: `profile`, `ghost`, `ghost_off`, `ghost_prompt`, `pak`, `pak4p`, `circuit`, `circuit12`, `battle2p`,
  `battlequit`, `race`, `raceend`, `stunt`, `screens`, `cheats_unlock`, `pause`, `rumble2p`, `menu`, `solo`, `drive`:
  20 of 20 `same`.
- The `.mpk` files the pak paths write ([SAVES.md](SAVES.md) 10.2's method): `pak`, `pak4p` (three paks) byte for byte
  the reference's; `profile` and `ghost` the reference's but for the directory entries' status and reserved bytes,
  which osPfsAllocateFile leaves as its stack had them and which differ between two runs of the reference too.
- The ASan+UBSan build (GCC 14's objects, Clang 21's runtime), 28 paths each in a data folder of its own (the 20 above,
  and `race2p`, `race3p`, `race4p`, `raceend12`, `cheats`, `cheats_hidden`, `cheats_menu`, `language`): no ASan,
  LSan or UBSan report, after CheckForEgg's step went back to
  the egg's words (UBSan's bounds check stopped `cheats_unlock` on `levels[20]` before). Its save tests
  (`assets_save`, `assets_input`, `assets_carshud`, `assets_casts`, `assets_pak`, `assets_carscr`, `assets_heap`,
  `ultra_pakstore`, `ultra_pakwrite`, `app_saves`, `ghost_tool`): 11 of 11.
- ctest, Debug: the tests of the data, the layouts and the saves (`assets_*`, `pc_data_*`, `pc_reach_test`,
  `ultra_selftest_fast`, `ultra_pakwrite`, `ultra_pakstore`, `app_saves`, `ghost_tool`, `layout_check`,
  `offset_check`, `index_check`, `endian_check`, `views_*`, `objsame_tool`, `names_check`, `one_spelling`,
  `window_census`, `n64_guard*`): 76 of 76. `gen_data.py --check` OK in Debug, RelWithDebInfo and the ASan build.
- Queued behind the other agents' runs under `~/rush2049pc/.w4-heavy.lock` when this was written, their logs to come in
  `~/rush2049pc/w4-c2/build/scratch/heavy/` (`heavy2.log` says where they are): the gate (`pconly_gate.sh --skip
  realtime --data-moved`), RelWithDebInfo against the reference on both cartridges, the ASan sweep of every path on
  both and CI's ASan ctest set; the realtime tests, waiting for `~/rush2049pc/.realtime.lock` (`light.log`).
- ctest, RelWithDebInfo (`-LE realtime`): 572 of 572 (`assets_rom` skipped: no ROM on Rocky).

Numbers (src/ and nonmatching/, the 73 files this changed): local struct and union declarations 176 to 90, tags
declared in more than one file 30 (109 declarations) to 17 (58), VIEW lines 62 to 19, pad arrays 149 to 129 (most
of those left are the players' and cars' records, C1's and B1's). Over the whole game: 891 local declarations to
805, 92 duplicated tags to 78, 100 VIEW lines to 57.

Left, as other agents' records: the players' input records (`gPlayers`: `Player`, `PadState`, `InputRecord`,
`Player76` ..., C1), whose profile field is typed `tProfile **` in every file this changed; the circuit in progress
(`tCircuit`, C3), which `tSaveData.circuit` embeds as types.h's `tCircuitRecord` (`/* wave 4: tCircuit */`) and
circuitscr, ContinueCircuit, SetCircuitCode and UpdateCircuit still read through their own `struct tCircuit`; the
list headers (`tDLList`: paksysdc's and save.c's `ListHead`, the lists' owner); libultra's `OSPfs` and `OSPfsState`
(`PakFileState`, D1); `Header` in ZOID_SetPolyFlags.c and ZOID_ClearPolyFlags.c, which is a flat polygon's start
(MAT3 `tPoly`, B2), not a pak file's; `gPrevUnlockedTires` (frontend.h, read flat by highscore_InitScreen).

### 4.3 Menus, battles and circuits

| Record | Declared in | What it holds | Its other tags |
|---|---|---|---|
| `MenuButton` (MAT3 `tMenuButton`) | frontend.h | One 3D button of a menu screen: the model it shows (`type`), its scene object (`handle`), how far it has turned (`angle`), the matrix and position its object draws by, and its text's opacity (`alpha`). Every screen's `buttonInfo` table but the start screen's: options, audio, video, controls, mode, track, car, circuit, player and record | include/game's `ScreenButton` (the audio and video screens' tables) and `ModeScreenButton` (the mode screen's); the files' `ButtonInfo` (audio.c, videoscr.c, options_anim.c), `Button` (audio_initScreen.c, updateAudioScreen.c, op_initButtons.c, optionscr/buttons.c, videoscr_update/group.c, video_init.c), `UButton` (audio_uninitScreen.c), `Item` (controls_updateButtons.c), `Slot64` (modescr.c, modescr_init.c), `Slot` (modescr_animText.c), `tMenuButton` (trackscr.c) |
| `StartScreenButton` (MAT3 startscr.c's `buttonInfo`, `anon_buttonType_objIdx_3c`) | frontend.h | The start menu's buttons: a `MenuButton` without the alpha, 4 bytes shorter | `ButtonInfo` (start_screen/group.c) |
| `Battle` (MAT3 `tCarData.battle`, `anon_weapon_health_30`) | types.h, the race section | A car's state in a battle: its weapon and ammunition, health, defence, gun's aim, and its fade while invisible. The game car embeds it by value, so it is declared before the cars' header | the copies in weapons.c, shots.c and StartWeaponIcon.c |
| `Defense` (MAT3 `anon_active_invsTime_10`) | types.h, the race section | The battle record's defence: the invisibility and the shield the power-ups give, and their countdowns | the same three files |
| `tCircuit` (MAT3 `tCircuit`) | types.h, in the profile's file (`tSaveData.circuit`) | A profile's circuit in progress: its type, races driven, seed, difficulty, time, and each car's place in each race. A file format: its comments keep the file's offsets | include/game's `tCircuitRecord`; the copies in UpdateCircuit.c, SetCircuitCode.c, ContinueCircuit.c and circuitscr.c |
| `tCircuitTrack` (MAT3 `tCircuitTrack`) | types.h | One race of a circuit, `gCircuitTracks[]`: the track, its direction, mirror, fog and wind | include/game's `CircuitTrackSlot` |
| `TextBox` | text.h | A box of text on the screen: `gScreenRect`, where the record screen draws its views (`TB_DrawTextScrollBox`) | zlib's `config` (gScreenRect's declaration), recordscr_draw.c's `TextBox` |

The pause screen's three text animators (`animPauseText`, `animSureText`, `animTitleText`) took a `MenuItem`, a copy of
a blit's first fields; they take the blit (`struct Blit *`) as every blit animator does, so `MenuItem` is gone. The
fields have MAT3's names, but where the screens' files already agreed on a clearer one: `MenuButton`'s `type` and
`handle` (MAT3 `buttonType`, `objIdx`), which some 300 reads in two dozen files use. Its matrix and position stay two
fields, marked `/* wave 4: tUVMatrix */`: they are MAT3's `mat` (`uvs`, `pos`), and the files read them as `matrix`
and `position` in some 900 places, so swapping in the matrix record means renaming those reads too.

Fields the copies disagreed on, and how they were settled:

- **The button's last word.** The mode screen's table had a word at +0x3C (`ModeScreenButton.unknown3C`), the audio and
  video screens' four bytes (`ScreenButton.pad3C`), the others the alpha byte. It is the alpha, as MAT3 has it; no code
  reads the mode, audio or video screens' alpha. So the data loader copies the mode screen's 13 alpha words as bytes
  now, where it swapped them as words (data.c's only change but type names); the audio and video tables' program is the
  same.
- **The start screen's matrix** was `f32 uvs[4][3]`, the position its fourth row: `matrix` and `position` now (`uvs[3]`
  is `position`, `uvs[0]` and `uvs` are `matrix`). Its first set-up still copies the identity matrix and the origin in
  one 48-byte copy from `unitmatrix`.
- **The battle record**: a pad before the defence (alignment, gone), and the defence's fifth word, which MAT3 doesn't
  have, `unknown10`: the shield's spin (`shldAngle`; UpdateDefense pitches the shield by it and turns it each frame).
- **The circuit's ranks**: `u8 ranks[54]` in two copies, `[6][9]` in the others and MAT3. `[6][9]`: UpdateCircuit
  indexes `ranks[car][bit >> 3]`. UpdateCircuit and SetCircuitCode reach the circuit as the save data's `circuit`,
  not at its byte 1780.
- **The record screen's box** `gScreenRect` was declared with zlib's `config` (four u16s). MAT3's has six s16s, the
  box and its centre, and the image has room for them; the data loader held four, the four the game reads, and still
  does: `TextBox` is those four (the field program is the same four 16-bit words).

**The VIEW lines** (16): `audio_buttonInfo` (4), `video_buttonInfo` (3), `options_buttonInfo` (3),
`controls_buttonInfo`, `startscr_buttonInfo`, `trackscr_buttonInfo`, `modescr_buttonInfo` (2) and `gScreenRect`. Each
file reads the datum by include/game's declaration. `audio_buttonInfo` and `video_buttonInfo` are declared in
frontend.h with the other tables (audio.h and system.h had them). Four files had a view with the table's extent, which
include/game leaves open (`EXTENT`): at -O2 GCC schedules their stores into the table against the loads of the
screen's other globals by it. So the controls, mode, start and track screens' own files complete the table's type
(`extern MenuButton trackscr_buttonInfo[TRACK_BUTTONS];` ...), and they compile as before; without those four lines
each unit differs only in its instructions' order.

**Not same** (objsame against the reference, Debug and RelWithDebInfo): one unit, UpdateCircuit.c. It indexes the
ranks as MAT3 does, `ranks[i][bit >> 3]`, where its copy had `ranks[i * 9 + (bit >> 3)]` over a flat `u8[54]`: the
same byte, whose address GCC now forms as the row (`i * 9`, 64-bit) plus the column instead of one 32-bit index
sign-extended (i under 6, bit under 72); at -O2 it also counts the row by `i` rather than in 9s and loads `gPlayers`'
address again after a call. The same loads and stores and the one float addition, in the same order. Every other unit
is `same`.

**Numbers** (src/ and nonmatching/): local declarations of these records 30 to 0 (their pad arrays 24 to 0), the
VIEW lines of these data 16 to 0; tags declared in more than one file 92 to 84 (`ButtonInfo`, `MenuItem`, `Slot64`,
`Battle`, `Defense`, `tCircuit`; `Item` and `Slot` are now one file's each, below).

**Left, and whose:**

- `Owner` (pakscr_draw.c, modescr.c, playerscr.c) is no battle record: a pak file record (MAT3 `tPS_File`), a profile
  and a profile handle. They are the save records' (4.2), as are circuitscr.c's `Item` and ghost_files.c's `Slot` (pak
  file records under those tags), and the views of a profile's save data that reach the circuit's fields: SetOptions'
  `Stats` (`option_a`, `option_b`: the circuit's `death` and `difficulty`), trackscr.c's `CIRCUIT_DEATH_MODE_BYTE`
  (the save data's byte 1785) and CheckAndRepairPlayer's checksum of the circuit (bytes 1780 and 1784). Each becomes
  `->circuit.death` and so on once the profile's views go.
- The car record's copies keep their own battle fields, flattened (`InitBattlePlayer.c`, `powerups.c`,
  `battle_damage/group.c`, `animate_arrow/group.c`, `update_model/group.c`, `rocket.c`, the car record's PC
  declaration `struct pc_car`): the game car's (3.1), which embeds `Battle`.

### 4.4 Sound

Agent C4: the sound manager's records (MAT3 audiomgr.c, audiosfx.c), the MusyX library's synth voice, and the records
the library and the game share.

**The sound manager**: [audiomgr.h](../../include/game/audiomgr.h), which the files that read the records include
after game.h.

| Record | What it holds | Its other tags (files) |
|---|---|---|
| `tFxSequence` | an fx tag: one sound the game asked for, on one of the three tag lists | `Voice` (save/AudioInit.c); `Effect` (audiomgr/ProcessEmitters.c, whose `kind`, `state`, `delay`, `persistent`, `voice` are the tag's `state`, `paused`, `loop`, `numCmds`, `sysId`); audiomgr_thread.c's own copy |
| `tAudioSequence` | a timed sequence of up to four effects (MAT3's unnamed anon_ll_id_3c) | `Chan` (AudioInit.c), `Event` (stop_sequence.c), `Sequence` (NewSequence.c), `SequenceHead` (free_sequence/group.c), `anon_ll_id_3c` (audiomgr_thread.c) |
| `tSortedFxTag` | a node of the priority pass's tree (MAT3's anon_te_tag_c) | `anon_te_tag_c` (audiomgr_thread.c); AudioInit sized its pool with a literal 24 |
| `tAudioMsg`, `tAudioMsgHeader` | a message to the audio manager thread | audiomgr_thread.c's copy, `Slot24` (AudioInit.c), `MessageNode` (abort_game/group.c), `ShutdownMsg` (heap_release/audio_shutdown_posts.c), `SceneMsg` (gameover/gameover.c) |
| `tSfxEmitter` | a 3D sound source | `Source` (ProcessEmitters.c, whose `radius`, `nearVolume`, `farVolume`, `parameter`, `handle` are `fadeDistance`, `maxVol`, `minVol`, `basePitch`, `sndTag`) |
| `tSfxListener` | a listener | `Listener` (ProcessEmitters.c, whose `right` and `forward` are MAT3's `look` and `up`) |
| `tSfxVoice` | an effect instance of the sound server | `SfxInstance` (are_sound_voices_idle.c, whose `active` is `used`) |

In [audio.h](../../include/game/audio.h): `SampleDmaList`, the sample DMA list in progress (`pc_sample_dma_list`, the
PC's own storage; was sample_dma_thread/group.c's `SampleDmaList`, sample_dma_next.c's and types.h's `DmaList` and
musyx_start_dma_list.c's `DmaListView`), and `SoundGroupEntry`, an entry of `song_groups` and `boot_sfx_groups` (a
group of the MusyX project to push; was AudioSongPlay.c's `SongGroup` and AudioInitSys.c's `Instrument4`, and the
data were declared as the library's `ResourceRange`, two u16s alike).

Settled: `tAudioMsgHeader.cmd` is MAT3's `char`, `u8` under the game's `-funsigned-char`. `tFxSequence.list` was a
`tDLList *` of audiomgr_thread.c's own (MAT3's char/int list header): it is `AudioList *`, and `gPlayingFxTags`,
declared `FxTagList` (the same header under MAT3's names), is an `AudioList` like the other two tag lists
(`FxTagList` is gone; the thread reads its `count`). Its field program now takes the two bytes after `doubly`, padding
in `FxTagList` and `pad[2]` in `AudioList`, as bytes too: BSS, which nothing reads there, and the state hash takes only
a datum's pointer runs. It is the one difference in the generated data beyond names. `tSfxEmitter.sndTag` stays u32 (ProcessEmitters read
an s32; every use passes it to an int). `tAudioSequence.id` is MAT3's unsigned (FreeSequence: `id &= gTagMask`). The
message pool `pc_snd_msgs` is port data whose declaration (`struct pc_snd_msg`) the game's types don't see: its readers
cast it to `tAudioMsg`, as AudioShutdown.c did. `gFreeQ` keeps its type `SfxQueue` (an `AudioList` whose first two words
are declared as words): as an `AudioList` its field program would change. The OS records some of these files declare
for themselves (`OSMesgQueue`, `OSThread`, `OSScClient`) are D2's and stay.

**The MusyX synth voice**: [src/rom/sound/musyx.h](../../src/rom/sound/musyx.h)'s `SynthVoice`, with its head
`SynthVoiceHead`, its controller inputs `SynthInputMix` and `SynthInputSource`, its LFOs `SynthLfo` and the
`SYNTH_INPUT_*` order (which macros.c and controllers.c each had). The library's five files read a voice as it
(`sound_synth_voices`, `pc_mx_voice` as voices). It replaces 56 views whose fields were named after their N64 offsets
(`flags24`, `id60`, `lfo17C` ...): `VoiceState` (api.c, controllers.c, macros.c, synth.c, voices.c), `VoicePrefix`
(controllers.c, synth.c), `MacroState` and 40 `MacroState*` handler views and `SynthVoice*View` (macros.c), synth.c's
`RampState`, `SynthVoiceLegatoView` and its own `SynthVoice`, voices.c's `SynthVoicePriorityView` and
`SynthVoiceAllocView`, controllers.c's `VoiceControls`; with them the inputs' and LFOs' two copies each
(`ControlInput`/`MacroInput`, `ControlSource`/`MacroInputSource`, `SynthLfo`/`MacroLfoControl`) and macros.c's 13
copies of the macro step (`MacroCommandAddAge` ... are `MacroCommand`). It is the library's header, not include/game:
only the library reads the voice (the game names it in prototypes only), and the library's files can't include the
game's headers (their libultra types are the boot code's).

- The names are macros.c's byte map, the most complete view (synth.c's `cFlags24`, `midi4A`, `midiSet4B`,
  `identifier60`, `note50`, `field68` are the same fields); the map is now the record's comments.
- `volume`, `panning` and their targets are u32 (16.16, as macros.c's 17 views read them); `panDelta`, `sweepOff` and
  `sweepAdd` s32. synth.c's update clamps a sweep that went below 0, which its view read signed: `(s32)sv->volume < 0`.
- Packed (`#pragma pack(1)`), as 56 of the 58 views were. Every field is at its natural alignment either way, but GCC
  makes other code for an aligned record's fields (sound_macro_loop's flag tests). For the same reason voices.c's
  allocator reads `blocked` and `allocId` through the voices as a pointer (`VOICE_AT`) and PLAY_MACRO links the chain
  through the voices' addresses, as their views did.

**Shared by the library and the game**: [soundlib.h](../../include/game/soundlib.h), which the library's files
include after rom_tu.h and audio.h after types.h (it needs the basic types and libultra's `OSMesgQueue`, `OSMesg` and
`OSTask`, which each side declares alike).

| Record | What it holds | Its other declarations |
|---|---|---|
| `SpeechStream` | a speech stream (`speech_streams[2]`): its GSM decoder state, input ring, PCM ring and voice | types.h (the data's layout), decode.c, streams.c; gsm.c's `GsmStream` view |
| `SoundServices` | the game's services to the library | types.h, src/rom/sound/sound_services.h (which now includes the header) |
| `SampleDmaRequest` | an entry of the sample DMA list (`sound_dma_list`, now typed by it) | hardware.c's `SoundDmaEntryCodebook` and `SoundDmaEntrySamples`, dma_queue.c's `SoundDmaEntry`, sample_dma_next.c's `DmaReq` |
| `TransferRequest` | a cartridge read a speech stream queued | types.h, dma_queue.c |

`SpeechStream`'s first 400 bytes are gsm.c's `GsmState`, which the data loader lays out as types.h had them (bytes, the
last lag's u16, bytes): gsm.c reads them as its `GsmState` through a commented cast. Its input ring is a union of the
bytes the reads fill and the words the decoder takes; the loader copies a union that bytes cover as bytes, so the
field program is the old `u8[4096]`'s. `SampleDmaRequest.length` is the library's int (the game's view had a u32).

**VIEW lines**: the eight are gone.

| Datum | Read as (file) | Now |
|---|---|---|
| `gFxTags` | `Voice *` (AudioInit.c) | its type, `tFxSequence *` |
| `gSortedFxTags` | `void *` (AudioInit.c) | `tSortedFxTag *` (audio.h named the struct `anon_te_tag_c`) |
| `gSequences` | `Event *` (stop_sequence.c) | `tAudioSequence *` (audio.h declared it `struct Chan *`) |
| `gSfxInstances` | `SfxInstance *` (are_sound_voices_idle.c) | its type, `tSfxVoice *` |
| `audio_manager_thread` | AudioInit.c's own 0x1B0-byte `OSThread` | its type, `OSThreadBytes` (the N64's 0x1B0 bytes, as the loader lays it out): AudioInit passes `(OSThread *)` to osCreateThread and osStartThread, which on the PC keep only the address, as the thread's key |
| `boot_sfx_groups` | `Instrument4[4]` (AudioInitSys.c) | `SoundGroupEntry[4]` |
| `song_groups` | `SongGroup[]` (AudioSongPlay.c) | `SoundGroupEntry[12]` |
| `musyx_retrace_callback` | `intptr_t` (musyx_set_retrace_callback.c) | its type, `void (*)()`, declared in audio.h (was frontend.h); `musyx_set_retrace_callback` takes `void (*)(void)` and `musyx_start_dma_list` its list and queue as pointers |

**Not `same` under objsame** (against wave 4's reference): RelWithDebInfo 959 of 961. `gameover.c`: its scratch message
for a full pool is a whole `tAudioMsg` (32 bytes, was 24, `.bss` only). `synth.c`: `sound_synth_update_voices`
allocates registers otherwise around the signed clamps of the volume and pan sweeps (the same values; the functions
after it in `.text` pad their loops otherwise). Debug 950 of 961: those two; `ProcessEmitters.c`, `audiomgr_thread.c`,
`free_sequence/group.c`, gsm.c, dma_queue.c and sample_dma_next.c convert between an s32 and a u32 of the same width
at -O0 (a register move, or the order of two loads); `sample_dma_thread/group.c` reads the done queue as a field (one
instruction where `M2C_FIELD`'s address took two); `macros.c` and synth.c's `sound_synth_init`: -O0 addresses a packed
record's field from its offset as GCC's layout splits it, which depends on the fields before it. The generated data
(`data.c`, the symbol and layout lists) differ in type names and in `gPlayingFxTags`' first run (above).

**Numbers** (the sound area: the audio manager's and sound server's files, the car sounds', src/rom/sound and
src/rom/speech, 120 files): local struct and union declarations 215 to 101, pad arrays 228 to 46, VIEW lines 8 to 0,
tags declared in more than one file 8 (19 declarations) to 5 (10): the five left are other agents' records
(`MODELDAT`, `OSMesgQueue`, `PlayerRec`, `Vec3`) and the library's own vector (`Vector` in streams_3d.c and vector.c,
with `SalVector` and `SoundVector` the same three floats). layout_check: open findings 6 to 5 (the DMA list's views,
which agreed in the end on one host layout). The views ctests take the records' names; `views_sound_commands` went
(the messages are a union, which audiomgr.h asserts at the pool's 32-byte element).

**Left**, and why:

- Not sound, though the brief's tags named them: the circuit code's `Resource` (SetCircuitCode.c, UpdateCircuit.c,
  circuitscr.c: the circuit's bytes; modescr.c's: a pak file), new_streak's and hudextra_icons' `Effect` (trails, HUD
  icons), AddBattlePointInternal.c's `Event` (a battle point's marker), gameover.c's `Voice` (a blit:
  `gameover_blit`, frontend.h's `struct Voice *`).
- The library's records that types.h copies for the data loader (`SequenceContext`, `SequenceNode`, `ChannelState`,
  `LargeNode`, `SmallNode`, `EmitGroup`, `ResourceRecord`, `LayerRecord`, `AudioConfiguration` ...) beside the
  library's own definitions (include/sequence_context.h, synth.c, streams_3d.c, registry.c, hardware.c): each needs
  the soundlib.h treatment, and some its readers' types made to agree first (synth.c reads the volume faders through
  three views, `ChannelState`, `SynthFader`, `MasterFader`, with signed and unsigned fields; sequencer.c's slot views,
  two of them layout_check's open findings).
- `gFreeQ` (`SfxQueue`, above), and the port's storage `pc_snd_msgs` and `pc_mx_voice` (port/include/pc_game_types.h
  declares them for the game without the game's types: the readers cast).

## 5. The OS records

(Agent D: the message queues, the pak file system, the scheduler's clients, zlib's state.)

### 5.1 zlib's state, the pak's file system, the PI's message (agent D1)

| Record | Declared in | What it holds | Other tags it had |
|---|---|---|---|
| `z_stream_s` (`z_stream`) | `include/game/zlib_deflate.h` | the deflater's stream: the input and output, their counts, the state; `zip` makes one on its stack for each ghost it compresses | copies in `zlib_deflate/group.c` and `heap_release/zip.c` |
| `tree_desc_s` (`tree_desc`) | the same | one Huffman tree being built: its nodes, its largest code, its static counterpart | the same two copies |
| `internal_state` (`deflate_state`) | the same | the compressor's state (zlib 1.0.4's, trimmed), allocated from the main heap for each `zip` | the same two copies; the int after `pending` is MAT3's `data_type` and `method` (two bytes, nothing reads them), where the copies had `unk10` |
| `OSPfs` | `include/game/pfs.h` | a controller pak's handle: what `osPfsInitPak` read of the pak (its id, label, tables' pages, banks), which every other call of the file system takes; `tPak.pfs` | include/game's `PcPfs`; copies in `paksysdc/group.c`, `paksysdc_flush/group.c`, `PS_ReadPartialFile.c`; `PR/os_pfs.h`'s for the boot segment's libultra |
| `OSPfsState` | the same | a file's entry in a pak's directory (size, game and company codes, name, extension); `tPakFile.state` | include/game's `PakFileState`; `PR/os_pfs.h`'s |
| `__OSInodeUnit`, `__OSInode` | the same | a page's place in a pak's inode table, and one bank's table | include/game/types.h's and `PR/os_pfs.h`'s |

The pak's records are declared once for both of the game's header worlds: `include/game/types.h` and the boot
segment's `PR/os_pfs.h` include `include/game/pfs.h`. The test programs' own host-type mirrors (`test_pak.c`,
`test_save.c`, the self-test's `SelfPfs`) stay theirs: they read the game's bytes, not its headers.

`OSIoMesg` and `OSIoMesgHdr`, the PI's DMA message (include/game/types.h, `PR/os_pi.h`, and a copy in `waves.c`), are
gone: the game reads the cartridge with `pc_cart_read` ([ULTRA.md](ULTRA.md) 11), and the datum that held one
(`sample_dma_io`) with them.

No field disagreed between the copies: each was the same layout under the same names but `unk10`. No unit is other
than `same` for the records (objsame, Debug and RelWithDebInfo; data.c, the symbol and layout lists the reference's).

| In src/ and nonmatching/ | Before | After |
|---|---:|---|
| local declarations of these records | 11 (zlib's 6, `OSPfs` 3, `OSIoMesg` and its header 2) | 0 |
| tags declared in more than one file | 4 (`z_stream_s`, `tree_desc_s`, `internal_state`, `OSPfs`) | 0 |
| `VIEW()` lines, pad arrays | 0 | 0 |

## 6. The proof

(The lead's, after the merge.)
