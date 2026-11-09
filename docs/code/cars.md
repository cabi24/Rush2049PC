# The cars, their physics, the maths and the scene

The cars area of the game's C: the car model and its physics, collisions with the world and between cars, crashes
and respawning, the car's set-up and visuals, the effects (blasts, smoke, skids, sparks, streaks), the maths every
part of the game uses (orientation matrices, quaternions, vectors, the fast trig), and the scene: the scene-node
tree, the ZOID renderer front end that turns it into display lists, the sky, the world's parts and texture
animation. About 270 files, 15,000 lines; their headers are `include/game/cars.h`, `physics.h`, `maths.h` and
`scene.h`, with the cars', physics', maths' and scene's sections of `include/game/types.h`.

Written 2026-11-08 by wave 2b's cars agent ([PCONLY.md](../port/PCONLY.md) 4). It uses the tree's names as they are
now; the original names are MAT3's (the user's GameCube decomp of the same game, `SFRush2049-mat3-decomp`), whose
source file each function came from is said in the comment at the top of its file. The records' layouts are
[docs/port/CARS.md](../port/CARS.md)'s.

## 1. Two threads, three records

The cars live on two threads (README.md, "How the program runs"):

- **The model thread** (`ModelThread`, groups/modelthread) runs the simulation: once a retrace `model_iteration`
  turns the pads into each human car's controls, replays or records ghosts, and steps every car with
  `UpdateModel`, several times a frame at the model's fixed step. It reads and writes only the model records.
- **The game thread** draws: each frame `update_game_data` copies what the renderer needs from the model records
  into the car records (position, orientation, the tires, the appearance bits), and the visuals, effects and
  cameras read the car records.

The records:

| Record | Where | Layout | What |
|---|---|---|---|
| model, `tModelData` | `pc_models[12]` (port/include/pc_game_types.h `struct pc_model`) | the N64's, 0x808 bytes, which is MAT3's field for field | one car's physics state: forces, velocities, position, orientation (`uvs`), the four tires, the drivetrain, the controls (`mainin`), the respawn (`resurrect`), the race state |
| car, `tCarData` | `pc_cars[12]` (`struct pc_car`) | host: the N64's field order with the 21 visuals and the input pointer widened, 0x468 bytes | what the game thread shows: the dead-reckoned position (`dr_pos`, `dr_uvs`), `appearance`, the place, the scene node (`objnum`), the 21 visuals, the crash state, the battle block |
| visual, `tVisual` | in the car record (`visuals[21]`), and the environment pool `gEnvirons[100]` | host, 0x20 bytes | one animated thing: its scene node, a data word, a time and its animator `func(v, op)` |
| car descriptor, `_tCar` | `gCarModels[13]`, through the model's `car` | the N64's (arena data), 0xB4 bytes | a car's fixed physics: inertia, springs, damping, the tire descriptors (`tires[4]`, window addresses), the tire arms `tireR[4][3]`, the drivetrain constants |
| tune, `Tune44` | `gTune[3]`, through the model's `tune` | 11 floats | the handling's scales (MAT3: diffbl, weight_scale, torque_scale, trans_scale, clutchmaxtk, shiftk, fwdfk, sidefk, skidk, antispin, drift) |

Every file declares its own partial view of the model and car records, named `tModelData` and `tCarData` with
MAT3's field names; the car views write N64 offsets in their padding and put `struct pc_visual visuals[21]` at
0x110, so the compiler lays the later fields out at their PC offsets (CARS.md 2.2). The N64's car record has 21
visuals where MAT3's has 22, so after the visuals an N64 offset is MAT3's less 0x18, and the N64's battle block has
one more word (at 0x39C) than MAT3's.

The **appearance bits** (`APPEAR_*`, cars.h, MAT3 rush_flags.h's names) are how the physics talks to the visuals:
the model step sets them in the model record (a skid under a tire, sparks at a corner, the brake lights, tire smoke,
the wreck), `update_game_data` copies them into the car record, and the visuals, smoke and skids read them.

## 2. The physics step

One car's step is `sym(m)` (MAT3 drivsym.c), at `dt` = `model_step_dt` (MAT3: 0.0025 s, 400 steps a second). World
units are feet, speeds feet a second, angles radians; Y is up. A car's orientation `uvs` is a 3x3 matrix whose rows
are its right, up and forward axes in world coordinates; `WorldVector` takes a body vector to the world,
`BodyVector` back.

- `sym`: `ModelControls` (the controls `mainin`, written by the input code, to the steering angle, throttle, brake,
  clutch and gear, and each wheel's brake torque); then `drivetrain`, `regular`, `checkok`, the rpm, the engine's
  bog state, the peak forces of a human car, and clearing the force accumulators `centerForce`/`centerMoment`.
- `drivetrain` (MAT3 drivetra.c): `autoshift` (`find_best_gear`, `upshift`, `downshift`), `engine` (the torque off
  the car's torque curve: `enginetorque` over `stdtorquecurve`, 10 throttle rows by 12 rpm columns, interpolated by
  `interp`), `transmission` (the gear's ratio from `carTransGearRatio`), `whatslips` (the clutch slipping or locked,
  and the inertia the engine sees), then the differential.
- `regular`: `road` (the body against the world, `CollideCarBody`; each tire's surface, `TireSurf`; the suspension
  compression `suscomp` and the distance above the road `airdist`; then `collision`, car against car), `forces`
  (`forces1`: per tire the tire frame `calctireuv`, the tire force `dotireforce` and the friction circle
  `frictioncircle`; `MovingSurfaceForce`, wind and drag; `forces2`: the sum, gravity, the wings' lift),
  `antispin`, `torques` (the moments, with the wings' air control from the stick), `accelerations` (`calcaa`),
  `velocities`, `positions` (the new position and orientation, `RotateUV`, renormalised every 1,024 steps;
  `make_super_speed` for the cheat).
- `checkok`: the sanity and crash checks after the step.
- **Tires**: a brush model (Fiala's cubic, F = Fz (k1 a - k2 a^2 + k3 a^3) up to the slip angle afmax = 3 cfmax
  zforce / cstiff); the friction circle shares the grip `cfmax` between drive and side force; at low speed the
  contact patch is a spring (`patchy`); `slipflag` records which case applied. A tire's constants come from its
  descriptor (`copy_tire_info`, `tire_constants`).
- **Road codes**: each tire's road polygon gives `roadcode` (its flags & 15: `ROADCODE_DIRT`, `ROADCODE_WATER`,
  `ROADCODE_AIRBORNE`, and the lava and water codes that abort a car), `sviscode` (what the visuals and sounds show,
  `SURF_VIS_*`) and `roadboost`.

Set-up: `Init_MDrive` (the race start, `INIT_MODE_START`, or a respawn, `INIT_MODE_CRASH`) and `multiinit` call
`syminit`: `initialize` (`carsetup`: the mass `CalcMass`, `CalcDrift`, `CalcOffroadTires`, `CalcTireSideForce`, the
tires, `steer_constants`), then `init_road`, `init_collision`, `road` and `positions`. `End_MDrive` stops a car,
`SetCatchup` sets each drone's rubber band (`catchup`) every frame, and `Update_MDrive` (on the game thread) runs
`update_game_data` and the ghost cars' nodes.

## 3. Collisions, crashes and respawning

- **The frame** (groups/update_model, MAT3 mdrive.c): `ModelThread` (groups/modelthread) is woken by the scheduler
  each retrace, reads the controllers and, while `modelrun` is set, calls `model_iteration`: it advances
  `gModelIteration` and `gModelTime`, gives the cars another node drives their speed from their dead reckoning,
  turns the pads into controls (`UpdateModelControls`), steps each human car (its `dt`, the ghost in a ghost race,
  `SetClutch`, the abort button, `UpdateModel`; a dead car only collides), steps the local drones by the drone
  schedule (`update_drone_models`; on the PC `pc_drone_sched` continues the N64's table for cars past the sixth),
  then dead-reckons every car (`dead_reckon_all`) and updates the sound listeners.
- **One car** (`UpdateModel`, MAT3 _UpdateModel): the death-mode coast-out; `CheckCrash`, `FinishResurrecting`, and
  on `CRASH_RESPAWN` `End_MDrive` then `Init_MDrive(INIT_MODE_CRASH)`; the time scale `time_fudge` (the drone's
  catch-up times its time boost times the road's boost), which scales the step's `dt`; then the respawn flight
  (`UpdateResurrectingCar`) or the physics (`sym`, after `MaxPathControls` for a drone); the Rumble Pak's kick from
  the forces; `update_reckon_base`; the car's sounds; the crash and wreck-smoke bits; solid again after 5 s.
- **Car against car** (`collision`, MAT3 collision.c): bounding spheres (`colrad`) first, then each corner of one
  body (`bodyR`) in the other's frame (`PointInBody`), and the push `setFBCollisionForce` (against the relative
  velocity, growing with the depth squared, split by the two masses into `centerForce`); car mines, the battle ram
  (`BattleDamage`), a crash above half the crash threshold, the suicide modes. `ForceApart` and `MiniForceApart`
  separate bodies that overlap; `setMiniFBCollisionForce` is a car against a track object (`tMiniModelData`, its
  bounding box a window address); `init_collision` resets the forces and starts each tire's road search at the road
  tree's root.
- **Crashes**: `CheckCrash` goes `CRASH_NONE`, `CRASH_WRECKED` (the crash point kept by `SetResurrectionData`),
  `CRASH_RESPAWN` after 3.5 s (2.25 s in a battle). `AbortCar` forces a respawn; `DetectDroneAbort` aborts cars that
  fell out of the world (below -190 ft) or are stuck, and measures each human's `strayAngle` for the WRONG WAY sign.
  The car's `dead` state is `CAR_*`, the respawn flight's `RES_*`.
- **Respawning** (groups/resurrect, MAT3 resurrect.c): `Resurrect` places a car at the race start (on the PC the
  grid continues past the N64's six places: `pc_grid_start`, `pc_pole_row`, MORECARS.md 4), or after a crash finds
  its respawn point (`FindHumanResPt` on the resurrection path, a drone's on the drone path), moves it on by the
  car's speed over the time since the crash and the flight, and sets the flight's two orientations up as
  quaternions (`FindBestQuat`); `UpdateResurrectingCar` flies the arc, `FinishResurrecting` drops the car in
  (`syminit`, first or second gear, the wheels turning), `BlinkResurrectingCars` blinks it (`APPEAR_INVISIBLE`) until
  it is solid again. `GetResPt` keeps the obstacle course's furthest respawn point.
- **Dead reckoning**: the model thread steps at its own rate, the game thread draws at the frame's time:
  `dead_reckon_posuv` keeps a snapshot of the model (`reckon`) after its steps, `update_reckon_base` its base,
  and `game_reckon_all` moves every car to the frame's time.
- **The models to the cars** (`update_game_data`, game thread): with the model thread held, it copies each model
  into its car record (the reckoned position and orientation, the tires' positions from the descriptor's arms, the
  suspension's lean from `carSuspensions`), sets the appearance bits (`UpdateTireAppearance`: skids and smoke from
  the tire forces against `sSkidData`), then lets the model thread go on.

## 4. Making a car and its visuals

- **Creation** (MAT3 cars.c, visuals.c): `InitVisuals` (the object definitions in `gObjList`: per car slot its
  three parts, then the tire, the effects, weapons, wings and the track's objects, at the `OBJLIST_*` positions),
  `LoadCars`, `init_cars`, `CreateCar` (the car's scene nodes, `InitCarObj`, `InitCarDynamicObjs`; the track's
  objects `InitTrackDynamicObjs`), `SetupCar` and `PickCar` (which body), `SetupCarColor`, `MakeRamps`,
  `SetFrameColor`, `SetSheen`, `SetSheenColor` (the paint). `gCarParts[52]` holds each slot's object handles (13 cars
  for each of the car screen's four players; a race uses slots 0-5).
- **Paint**: the N64 paints through the palette: `car_palettes[slot]` points at a 512-byte palette (32-step ramps of
  the three colours at 32, 64 and 96, the tail lights at 160-167, the headlights at 168-175); the frame and hood
  draw with it, and the brake and head lights rewrite its entries. MAT3 swaps textures instead.
- **The visuals**: `StartEnvironment` (with `StartShadow`, `StartArrow`) and `StartCar` (`StartFrame`,
  `StartSparks`, `StartWing`, `StartWingFlame`, `StartWeapon`, `StartHeadLights`, `StartBrakeLights`, `StartTire`)
  fill the car's 21 visuals (`VIS_*` in cars.h: brake lights 0, sparks 1-5, tires 6-9, shadow 10, arrows 11-13, the
  body 14, wings 16-17 and their flames 18-19, the weapon 20; MAT3 has a headlight visual at 1, so its indices are one
  more from there). Each frame `UpdateVisuals` calls every visual's `func(v, 1)`; `func(v, 0)` stops it, and
  `RemoveCar` takes the car out. The animators: `AnimateFrame` (the body, the sheen, the hulk), `AnimateTire`,
  `AnimateShadow`, `AnimateWing`, `AnimateWingFlame`, `AnimateBrakeLights`, `AnimateRocketFlame` (the rocket car,
  groups/visuals/rocket.c); `CheckVisible` shows a part in the car's own window only, `CheckVisibleAll` in every one.
- **The environment visual pool**: `gEnvirons[100]` (`InitEnvs`), the free ones on `gFreeVis`, the live ones on
  `gEnvList` (`GrabEnvEntry`, `AddToEnvList`, `ReleaseEnviron`, `RemoveEnvirons`); `UpdateEnvirons` calls each one's
  `func` once a frame. The effects below and the track's animated objects are environment visuals.
- Differences from MAT3: only the first tire's node gets the rim texture (MAT3 sets all four); the shadow is one
  polygon (MAT3 builds it from strips); the arrows over a car are one polygon for each other window.

## 5. The effects

- **Blasts** (groups/blast, AnimateNewBlast.c, AnimateNewBlastRing.c, InitBlast.c): `StartBlast` (a crash in
  `PlayGame`, mines, cones, battle shots) runs a 30-frame sprite (`AnimateBlast`) on a matrix of `gBlastMats[50]`,
  round robin. For a car, frame 3 starts `StartNewBlast` (four flame balls, a glow and five `SMOKE_FIRE` puffs) and
  frame 4 scorches the ground (`newWorldDamage`); `StartNewBlastRing` adds the shock ring. Per-car state:
  `pc_blasts`, `pc_blast_rings`.
- **Smoke and water** (InitSmoke, StartSmoke, StartWater, UpdateSmoke, InitSmokeBlast, InitWingSmoke): a pool of
  100 puffs (`gSmokeObjs`, listed by `gSmokeList`). `StartSmoke(slot, type, water, offset)` (types `SMOKE_*`: a
  tire, the wreck, fire, a wing trail) is called by `UpdateVisuals`, `StartNewBlast` and `AnimateWing`;
  `StartWater` makes the two-puff water sprays; `UpdateSmoke` moves, grows, fades and animates every puff. Each
  car's smoke latches are in `pc_tire_fx` (`SMOKE_SPIN_*`, `SMOKE_WRECK_*`, `SMOKE_WATER_*`, `SMOKE_TRAIL_*`).
- **Skid marks** (InitSkids, AnimateSkids, StopSkid): a pool of 100 marks (`gSkidArray`, `gSkidList`);
  `UpdateSkids` (groups/skid_marks) lays each tire's marks, `StopSkid` ends one and `AnimateSkids` fades it.
- **Sparks** (StartSparks, groups/sparks): car visuals 1-5, driven by `APPEAR_SPARK_C0`..`C3` and `APPEAR_THUMP`;
  `HandleASpark` flickers textures on the spark object.
- **Light streaks** (overlay B: InitStreaks.c, groups/streaks): the ribbons behind battle shots (`gStreakArray[50]`,
  `gStreakPolyArray[150]`, flags `STREAK_*`).
- **The arrows** (groups/animate_arrow): one polygon over each human car in each other view; in a battle a car off
  screen gets an arrow at the screen's edge.

## 6. The maths

- **Orientation**: `uvs` matrices (rows right, up, forward). `YawUV`, `PitchUV`, `RollUV` turn about the object's
  own axes, `WYawUV`, `WPitchUV`, `WRollUV` about the world's; `fyaw`, `fpitch`, `froll` take a sine and cosine
  already known; `RotateUV` applies yaw, pitch, roll; `MulUVS` composes; `CopyUV`, `TransposeUV`, `ScaleUV`,
  `ScaleUVXYZ`; `CreateYPRMatrix` builds one from angles, `ExtractYaw` reads the heading. `WorldVector`,
  `BodyVector`, and `U64WorldVector` (with sign flips, for camera-facing quads).
- **Quaternions**: `MakeQuatFromUvs`/`MakeUvsFromQuat` (w first; the respawn flight) and
  `CreateQuatMat`/`InterpQuats` (w last; the camera paths).
- **Scalars**: the atan family is Unix V7 libm's (`fatan2`, `fatan` reduce to `fsatan`, which reduces to
  `fxatan`'s rational approximation; its constants `atan_*` are game data); `tan_cody_waite`; `invsqr`,
  `invmagnitude`, `NormalVector`, `Distance`, `Distance2D`, `vector_direction`, `vector_magnitude`; `Random(max)` on
  `game_rand`, the C library's linear congruential generator, whose seed `rand_seed` decides a replay.
- **Library helpers** under the game's names: `game_bsearch`, `game_strcmp`, `game_strncmp`, and the comparisons
  the scene's tables are sorted by (`objcmp`, `texcmp`, `world_name_compare`).
- **The scene's matrices**: `build_node_matrix` and `build_node_matrix_rows4` write the RSP's s15.16 `Mtx` words
  (the position at 16 units a foot, relative to the camera at depth 0; on the PC `pc_draw_range_scale` scales a far
  node's whole matrix down); `mtx_rotation_to_fixed` converts the view's lookat; `zoid_build_upright_matrix` gives a
  yaw-only node (`OBJ_UPRIGHT`); `lookat_reflect_fill` fills an environment-mapped node's LookAt.

## 7. The scene and the ZOID front end

- **The scene** is a tree of nodes in `ObjInstanceList[600]` (MAT3 `ObjInstance`; 0x44 bytes on the N64, 0x68 on
  the PC): flags (`OBJ_*`, scene.h), a culling word, a matrix relative to its parent (`tUVMatrix`: `uvs[3][3]`,
  `pos[3]`), the object handle `index` ((model << 10) | definition; 0xFFFF a free node), `child` and `next` (-1 none),
  its lights, a texture for each level of detail, a tile, a palette and two colours. `ObjHead` is the root list.
  `ZOID_AddObject` takes the first free node and links it (`ZOID_InsertObject`); `RemoveObjectTree` frees a
  subtree; `ZOID_ObjParent`, `ZOID_TrueObjParent` and `ZOID_ObjPrevObj` search the table, as nodes have no parent
  link; `ZOID_NewMatrix` hands out the persistent matrices (`gMatrices`, then on the PC `pc_matrix_spill`).
- **MBOX** (MAT3 mbox.c) is the layer the game talks to: `MBOX_NewObject`, `MBOX_NewObjectCam` (a node only one
  view shows), `MBOX_HideObject`, `MBOX_ShowObject` (`OBJ_HIDDEN`, or bits 8-11 for each view), `MBOX_GetChild`,
  `MBOX_GetNext`, `MBOX_GetPosition`, `MBOX_GetObjectRadius`; `MBOX_FindObject_WORK`, `MBOX_FindTexture_WORK`,
  `MBOX_FindPalette_WORK` find a definition by name, a binary search in each loaded model's sorted tables
  (`gRomObjDefPtrs`, `gTexDefPtrs`, `gPalDefPtrs`), and return its handle.
- **Windows** (MAT3 zoidwindow.c): `ZoidWinInfo[4]` (MAT3 `tWindowInfo`) with their viewports and scissors;
  `ZOID_ResetWindows`, `ZOID_SplitMenuScreen` and `ZOID_SplitScreen` lay them out from the screen's size;
  `ZOID_InitializeWindow`, `ZOID_SetWindow` the field of view (the near plane 2.5 ft, the far one 2,000);
  `ZOID_SetFog`, `ZOID_GetFog` (the fog, and the clear colour); `ZOID_ProjectWindow` and `ZOID_WindowTo3D` from the
  world to the screen and back (the PC's widescreen offset `HUD_X` applies in `ZOID_WindowTo3D`).
- **Flat polygons**: `PolyInstanceList[200]` (MAT3 `tPoly`: up to four vertices, `POLY_*` flags), filled by
  `ZOID_NewPoly` and `ZOID_UpdatePoly`: the shadows, the arrows, the skid marks. Their texture coordinates are in
  texels, from the texture's size and clamp flags (MAT3 uses 0.01 and 0.99).
- **The frame's draw** (groups/zoid_draw): `ZOID_EndFrame` sets the lights, then for each window
  `zoid_draw_window`: the culling (`WorldCullObjects`), viewport, scissor and fog, the perspective, the lookat
  (mirrored or upside-down tracks), the camera-facing and upright matrices; `Traverse` walks the tree (hidden, late
  and visible tests; the node's matrix; `OBJ_FAR` nodes through `build_far_node_matrix` and the PC's draw distance;
  the lights; then `zoid_draw_object`: the level of detail by distance, the reflection lookat, `zoid_load_texture`,
  `zoid_load_palette`, the colours, the model's display list; then the children). After the tree, the flat
  polygons, the two late passes and the 2D blits (`gBlitInstanceList`) through the blitter. The file also holds
  `farcsine`, `fasin`, `facos` (MAT3 asincos.c) and `InterpQuats` (MAT3 fmath.c).
- **The sky** (`SkyInit`, `SkyUpdate`, `SkyClear`): one dome node per window, which follows its camera's x and z.
- **The world** (MAT3 world.c): `NewWorld` finds the track's world by name (`FindWorld`, a binary search of
  `WorldTable`), adds its parts' trees to the scene (`NewWorldObject`, MAT3 `tModelPart`; the targets through
  `SetupTarget`) and builds the road tree (`InitSTree`); `FindWorldGroup` gives a point's top-level group,
  `downleaf` descends the road's quadtree (`streetyp`), `WorldFogDensity` sets each window's fog from the track's fog
  regions.
- **Texture animation** (`InitTextureAnimation`, `TextureAnimation`; MAT3 textureanim.c): `PaletteLists` (palette
  rotations, traffic lights, lamp chasers, texture scrolls through `displaylist_set_tile_size`; `PALANIM_*`) and
  `TextureSwapLists` (flipbooks: a new `texBase`, or a new image command through
  `displaylist_set_texture_image`); `relocate_texture_loads` rebases a wave's texture loads when it arrives,
  `flip_triangle_winding` reverses a model's triangles for the mirrored tracks.

## 8. Files

| Part | Files (src/blob unless said) |
|---|---|
| model thread and step | groups/modelthread, groups/update_model (`UpdateModel`, `model_iteration`, `update_model`), groups/sym, Update_MDrive.c, End_MDrive.c, groups/init_mdrive, syminit.c, initialize.c, groups/carsetup, Calc*.c, copy_tire_info.c, tire_constants.c, steer_constants.c, SetCatchup.c |
| drivetrain | drivetrain.c, autoshift.c, find_best_gear.c, upshift.c, downshift.c, engine.c, enginetorque.c, interp.c, transmission.c, groups/whatslips, SetClutch.c, ReadShifter.c |
| forces and motion | regular.c, road.c, init_road.c, forces.c, forces1.c, dotireforce.c, frictioncircle.c, calctireuv.c, calcalpha.c, MovingSurfaceForce.c, forces2.c, antispin.c, torques.c, accelerations.c, calcaa.c, velocities.c, positions.c, make_super_speed.c, groups/checkok, PointInBody.c, BodyVector.c |
| collisions, crashes, respawning | collision.c, init_collision.c, setFBCollisionForce.c, setMiniFBCollisionForce.c, ForceApart.c, MiniForceApart.c, CheckCrash.c, AbortCar.c, DetectDroneAbort.c, AreWeSolid.c, BlinkResurrectingCars.c, FinishResurrecting.c, UpdateResurrectingCar.c, groups/resurrect, groups/get_res_pt, InitDemoPathPoint.c, are_all_cars_stopped.c |
| model to game | groups/update_game_data, dead_reckon_all.c, dead_reckon_posuv.c, game_reckon_all.c, update_reckon_base.c, modelgo.c, modelstop.c, multigo.c, mcommunication.c, init_model_task.c |
| set-up and visuals | CreateCar.c, InitCarObj.c, InitCarDynamicObjs.c, InitTrackDynamicObjs.c, InitVisuals.c, groups/loadcars, init_cars.c, SetupCar.c, PickCar.c, groups/SetupCarColor, SetFrameColor.c, SetSheen.c, SetSheenColor.c, groups/StartCar, StartFrame.c, groups/StartWingFlame, StartEnvironment.c, UpdateVisuals.c, RemoveCar.c, AnimateBrakeLights.c, AnimateShadow.c, groups/visuals, InitEnvs.c, GrabEnvEntry.c, AddToEnvList.c, ReleaseEnviron.c, RemoveEnvirons.c, UpdateEnvirons.c, InitCarVisuals.c |
| effects | groups/blast, AnimateNewBlast.c, AnimateNewBlastRing.c, InitBlast.c, InitSmoke.c, InitSmokeBlast.c, InitWingSmoke.c, groups/start_smoke, groups/start_water, UpdateSmoke.c, InitSkids.c, AnimateSkids.c, StopSkid.c, ContinueSkid.c, StartSparks.c, groups/sparks, groups/animate_arrow, src/ovl_b InitStreaks.c and groups/streaks |
| maths | CopyUV.c, CreateQuatMat.c, CreateYPRMatrix.c, ExtractYaw.c, MakeQuatFromUvs.c, MakeUvsFromQuat.c, MulUVS.c, PitchUV.c, RollUV.c, YawUV.c, WPitchUV.c, WRollUV.c, WYawUV.c, RotateUV.c, ScaleUV.c, ScaleUVXYZ.c, TransposeUV.c, WorldVector.c, U64WorldVector.c, dot_uv_y_axis.c, fatan.c, fatan2.c, groups/fsatan, fxatan.c, fyaw.c, fpitch.c, froll.c, tan_cody_waite.c, invsqr.c, invmagnitude.c, groups/normal_vector, groups/distance, Distance2D.c, vector_direction.c, vector_magnitude.c, TB_Vector2Normalize.c, game_rand.c, Random.c, pick_random_masked_bit.c, game_bsearch.c, game_strcmp.c, game_strncmp.c, objcmp.c, texcmp.c, world_name_compare.c |
| scene | MBOX_*.c, groups/mbox_new_object, ZOID_*.c, zoid_*.c, groups/ZOID_InitializeWindow, groups/ZOID_ProjectWindow, groups/ZOID_SetFog, groups/zoid_update_poly, groups/zoid_draw, build_node_matrix.c, build_node_matrix_rows4.c, mtx_rotation_to_fixed.c, lookat_reflect_fill.c, RemoveObjectTree.c |
| sky, world, textures | SkyInit.c, SkyUpdate.c, SkyClear.c, NewWorld.c, groups/targets/NewWorldObject.c, FindWorldGroup.c, FindChunk.c, downleaf.c, WorldClear.c, WorldFogDensity.c, InitTextureAnimation.c, TextureAnimation.c, displaylist_*.c, relocate_texture_loads.c, flip_triangle_winding.c |

A few files the area holds belong elsewhere by what they do (the header placement put them here): the audio retrace
and sample DMA threads (audio_retrace_thread_entry.c, groups/sample_dma_*), the game music start
(groups/game_music), the controllers' set-up (init_controllers.c), the demo's roster (fake_init.c,
munge_gLink_data.c), a high-score animator (anim_show_on_high_score.c) and a camera helper (point_at_car.c).

## 9. Constants

The headers' constants carry MAT3 rush_flags.h's names where it has them (`APPEAR_*`, `CRASH_*`, `CAR_*` for the
car's `dead` state, `RES_*`, `INIT_MODE_*`, `SURF_VIS_*`, `DRONE_TYPE_*`, `MPH_TO_FTPS`), else names read off the
code (`ROADCODE_*`, `VIS_*`, `OBJLIST_*`, `SMOKE_*`, `PUFF_*`, `STREAK_*`, `OBJ_*`, `LOD_*`, `TEX_*`, `POLY_*`,
`PALANIM_*`). Other areas' values (the race kinds `gRace`, the game states, the roster's owners) are still numbers
with the MAT3 name in a comment (`gRace == 6 /* kBattle */`), for their headers to name.

## 10. What is still N64-shaped (for wave 3)

- **The model record** keeps the N64 layout in `pc_models`; its pointer fields (`car` +0, `tune` +4, `lasttp[4]`
  +0x3A0, `torquecurve` +0x40C, `movingSurface` +0x648) are 32-bit window addresses, read through `PC_N64P` (or a
  file's `N64P` over `enum n64p_*` fields) and written through `N64W` (carsetup). The car descriptors
  (`gCarModels`) and their tire pointers point into the arena; the road's nodes and polygons are in the road file.
  Every file declares its own partial view of the record; one shared definition is for when the layout is the
  PC's.
- **The car record** is host data, but the views' padding is written in N64 offsets around `struct pc_visual
  visuals[21]` (CARS.md 2), and a few fields are reached through accessors (`PC_CAR_*`, `PC_MODEL_*`) whose names
  predate MAT3's (`struct pc_car`'s `view` is MAT3's `camera`, `disabled` is `in_tunnel`, `heading` is
  `strayAngle`, `b358` is `crash_state`, `b3A0`/`b3A2` are `shoot`/`fadeState`; `struct pc_model`'s `scale` is
  `catchup`, `speed` is `magvel`, `lap` is `laps` ...).
- **The scene** emits N64 display lists (libultra's gbi.h macros, F3DEX2, s15.16 `Mtx`, viewports in quarter
  pixels) that port/gfx interprets; the wad records (TXHD, PLHD, OBHD) keep the N64 layout with window addresses
  (`texBase`, `palBase`, `displayList`); `gMatrices` and `pc_matrix_spill` are in the window; the texture loads and
  the models' display lists stay big-endian (`pc_be32_get`/`set`); `FindChunk` walks the N64-layout heap; the sky's
  matrices and the world table hold window addresses; `car_palettes`' data is a window address.
- **The pools** keep their N64 element order with pointer-sized links (the StatLL headers' 40-byte host layout; the
  puffs, skids and streaks are larger than on the N64); scene handles are `s32` whose low half is the node (read
  through an endian-aware union, ENDIAN.md).
- **The window record**: `ArbEntry` (MAT3 `tWindowInfo`) is still bytes after its pointers, so the window files read
  `ZoidWinInfo` through their own `tWindowInfo` (race.md 6). The scene nodes (`ObjectInstance`, MAT3's fields, typed
  pointers), the model files' tables (`ModelDefTable`, `TextureDefTable`, `PaletteDefTable`), the car parts
  (`CarParts`), the surface tree (`surftyp` ...) and the pools (`tStatList`, MAT3's fields) are one type each since
  2026-11-09. A scene colour word holds R, G, B, A in memory; `OBJ_COLOR` reads its bytes.

## 11. The original game's oddities

Kept as they are, each with an `/* Original game: ... */` comment where it happens: `forces1`'s poor-traction test
reads `roadcode[0]` where `roadcode[3]` is meant; `SetCatchup` reads `pc_cars[-1]` when no car counts (unused);
`StartSmoke` reads `sviscode[type]` for the types past the four tires (into `roadboost`); `SetupCarColor`'s grey
for `second == 0` is overwritten by `MakeRamps`, whose third ramp stops one entry short; `LoadCars`' shared-car
retry can never happen; `ReleaseEnviron` updates its high-water mark after the decrement; `MBOX_ShowObject`'s second
mode-0 branch never runs; `ZOID_InitializeWindow` writes the first viewport whichever the window's is;
`zoid_draw_object`'s eye distance from depth 2 on comes from stale stack values; `displaylist_get_texture_image`
returns nothing when a list has no image command. `DetectDroneAbort`'s `wait_time != 4` is always true (the N64 also has a stuck timer of 6 s, 12 with reverse held,
which MAT3 lost); `GetResPt` leaves an index unset on an empty path; `UpdateModel`'s `(gRace != 6) || (gRace != 5)`
is always true.

Two port bugs found here are fixed (2026-11-08): `SetResurrectionData`'s attract-mode branch read the drone path
point at the N64 offset 0x338 of the PC car record (visual 17's `index`) instead of `mpctl.idx`, so the demo's cars
started from another point of the drone path; and `StartHeadLights` took `car_palettes`' window address as the
palette's pointer without `PC_N64P` (the same pointer in the default builds, not under RUSH_NOFIXED).
