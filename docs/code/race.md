# Track, race, cameras: a map

Wave 2 of the PC-only work ([PCONLY.md](../port/PCONLY.md)). This page maps the part of the game that runs a race:
the track's data (its paths, checkpoints, targets and collision surfaces), the race's rules (laps, places, circuits,
stunts, battle, ghosts, the attract mode's demo), the drones' driving, and the cameras. It is for a programmer new to
it: what each piece does, its files, its data, how the pieces call each other, and what is still the N64's shape for
wave 3. The names are the tree's; "MAT3 X" is the GameCube build's name for the same thing (the MAT3 decomp,
`decomp/include/rush/rush_types.h` for the structures), which the code follows wherever it is the same code.

The shared types are in [include/game/types.h](../../include/game/types.h) (the "track", "race" and "camera" sections,
and `tCheckpoint` in "system"); the data and prototypes in `track.h`, `race.h` and `camera.h`.

## 1. The race, frame by frame

The game thread runs `GameLoop` (`groups/front_end/GameLoop.c`) once a frame. A race is a run of game states
(`gGameState`, named in `race.h`):

| State | What happens (`UpdateGame`, `groups/update_game/UpdateGame.c`) |
|---|---|
| `GS_GAME_INIT` | the options, the roster and grid (`CalcPolePositions`), the catch-up setting |
| `GS_PREPLAY` | `PrePlay` loads the track (`LoadTrack`) and the cars, a step a frame, then sets the cars on the grid |
| `GS_COUNTDOWN` | the cars sit on the grid (`CarSetup`, `Update_MDrive`); `CountDown` counts 3.5 seconds |
| `GS_GO` | the race clock (`SetCountdownTimer`: by the race kind), the start call and the music |
| `GS_RACING` | `PlayGame` each frame; the race's end decided (below) |
| `GS_END_GAME` | `EndGame`: the models stop, the high score screen follows |

`PlayGame` (`groups/car_checkpoints/PlayGame.c`) is one frame of a race: the model thread's results into the car
records (`Update_MDrive`), the race kind's own rules (`CheckForStunt`; battle's `ShotUpdate` and
`UpdateBattlePowerUps`; else the drones' catch-up `DoDrones`, or the obstacle course's `GetResPt`, and the
checkpoints `CheckCPs`), the crashes and respawns (the crash view, blasts, sounds), each car's visuals and its hits on
the track's targets (`CheckTargetCollision`), the drones that want to be put back (`DetectDroneAbort`) and the HUD.
The cars themselves move on the model thread (`UpdateModel`, outside this area), and the drones' controls come from
`MaxPathControls` there (section 3).

The race kinds are `gRace`: `kOneRace`, `kPractice`, `kGhost`, `kCircuit`, `kStunt`, `kObstacle`, `kBattle` (MAT3's
enum, `race.h`). A lap race ends when every human has finished; practice never; stunt by time, points or crashes
(`gStuntMode`); battle by time or a team's points (`gBattleMode`); the others when the clock runs out.
`AbortGame` (`groups/abort_game`) is the pause screen's quit and restart.

## 2. The track's data

`LoadTrack` (`groups/loadtrack/loadtrack.c`) loads a track's four files a step at a time, each part's state in
`gTrackLoadInfo`, then sets the track up once (`initTargetStructs`, the world, `InitCheckpointData`,
`InitPathData` ...). `UnloadTrack` and `ReloadTrack` undo and redo it.

### Checkpoints

`gCheckpoints` (`tCheckpoints`, MAT3's) holds up to 10 gates across the road, read from the start of the path file
(`ReadCheckpointData`). A car passes its next checkpoint when it crosses the gate's plane within its radius
(`CheckCPs`, `PassedCP`); the finish line (`finishPt`) counts the laps once the car has passed the start checkpoint
(`startPt`); after the first lap the lap starts at `loopPt` (the start is a run-up). Each checkpoint also records
where every path crosses it (`pathPts`: `resPathPt`, `dronePathPt[4]`, `altPathPt[15]`, `SyncCheckpointsToPaths`),
the time the drones take to the next one (`time[2]`, `CalcCheckpointTimes`, which also sets the race's start time)
and the distance (`distToNext`, `FindPathDistances`). The places come from how far round the race each car has got
(`FindMaxpathIntervals` sets `distance` on the car record from the resurrection path; `PlaceUnlockedCars`,
`CarReportsGameOver`).

### Paths

`ReadPathData` (`groups/pathcommon_read`) reads, after the checkpoints:

- the **resurrection path** `gResPath` (`ResurrectPath`, MAT3 anon_numPts_pts_10): a line down the middle of the
  road, `numPts` points (`pts`, `s16[3]`, also `gResPathPtList`), with its **alternative paths** (`AltPath`,
  `gAltPaths`): shortcuts and second routes whose ends join the main path or each other (`SyncAltPaths` finds where,
  `FindClosestPt`; `TraceCheck`, `TraceToEnd`, `TraceToMain`, `TraceToCheckpoint` walk them). A crashed car is put
  back on it (resurrect.c, outside this area, with `FindResPt` mapping a branch's point onto the path it joins), and
  the race distance is measured along it (`GetPathDistance`, `NextResPathPt`, `NumPtsToCheckpoint`,
  `TracePathPtForward`, `TraceToIndex`). Its bounds (`CalcResPathBounds`) scale the HUD's track map.
- the four **drone paths** `gDronePaths[4]` (`DronePath`, `DronePathPt`: a position, the speed to drive there in mph
  and hints): loops the computer cars follow (`NextPathPt`, `PrevPathPt`, `FindClosestPathPt`).

The points are the path file's own. The headers are decoded beside the file when it loads, with host pointers to the
points (`struct pc_path_records`, `port/assets/path.c`), and `ReadPathData` copies them, the branches into an array of
its own, which `SyncAltPaths` joins up. These types are declared in `pc_game_types.h`, which the converter shares.

The world's names (`WorldTable`, `struct NameEntry`: each world tree's name and root node) and the targets' quadtree
and list (`gTargetGroups`, `gTargetGroupList`) are records the world file's converter builds the same way
(`struct pc_world_records`, `port/assets/world.c`), which `LoadTrack` points the globals at.

### Targets

The track's objects that take part in the race (cones, cacti, fences, windows, coins, mines, weapons, the trams and
doors that move along paths) are **targets** (`tTarget`, MAT3's): 130 nodes of `gTargetList` in `pc_target_nodes`
(PC data in the host layout, N64 `gTargetArray`).
`SetupTarget` (`groups/targets/group.c`) makes one from a world object or a PTHD path record whose name starts with
a kind's in `targetInfo` (`TargInfo`, 122 kinds: the model, the start and animation functions, the collision test,
the sounds); `SetTargetDefs` gives each its kind's record and starts it. `CheckTargetCollision` tests each car
against the targets of the quadtree leaf under it (`gTargetGroups`, `tTargetGroup`) and starts what it hits.
**Path targets** (`groups/targetpaths/group.c`) move along the track WAD's PTHD paths (`tBasicTargetPath`, its points
`TargetPathPt`, the target's place on it `tTargetStructPath`), animated each frame by an environment visual
(`AnimatePathTarget`), with their sounds; one with a collision object carries the road's surfaces with it
(`MoveCollisionObject`, `RotateCollisionObject`, `GetTargetVelocity` for the cars standing on it). Moving boxes
(`tMiniModelData`, `gBBoxModelStructs`) slow the drones (`TargetPathAvoid`).

### The surface tree

The collision WAD (`InitSTree`) holds the road's surfaces in a quadtree (`stree_nodes`, `streetyp`): each leaf a
packed list of surfaces (`DecodeSlist`), each surface a polygon of vertices (`DecodeVlist`, `sverttyp`: 1/32 foot)
with a frame and flags. `RoadPosition` and `TireSurf` (`groups/stree_collide`, `groups/stree_tiresurf`) find the
surface under a point or a tyre (`PointInSurf`, the road path's frames `fPathInterp` and `fHalfPipeInterp`);
`CollideCarBody` and `CollideCarBodyAgainstSurface` push a car out of the walls (`CarApplyCollision`);
`ShotToWorldCollision` and `CylinderInSurf` trace shots, and `CameraCollision` the cameras.

## 3. The drones

A drone follows a drone path (`groups/maxpath/group.c`, MAT3 maxpath.c), on the model thread: `MaxPathControls`
finds the stretch it is on (`FindInterval`, `IntervalPos`; now and then a drone moves to another of the four paths,
`IntervalPosChange`), the speed and the point 80 feet ahead it aims for (`TargetSpeed`, `TargetSteerPos`), steers
round the cars ahead (`AvoidAreas`, `TargetPathAvoid`) and sets its gas, brake and steer (`AdjustSpeed`,
`AdjustSteer`). Its state is the car record's `nav` (`struct pc_car_nav`, MAT3 tPathPoint). On the game thread
`DoDrones` (`groups/drones`) sets each drone's catch-up (`drone_scale`, `time_boost` on the model record) against
its rival human by the difficulty, its place and, in a circuit, its skill. `SetDroneResPathIndex`,
`SetNewPathIndex` and `SyncMaxpathToLastCheckpoint` put a respawned drone back on its path.

## 4. The race kinds' own code

- **Circuits** (`ContinueCircuit`, `NewCircuit`, `UpdateCircuit`, `GetCircuitCode`, `SetCircuitCode`): a run of
  races from a seed (`seeded_rand`), recorded in the player's profile (MAT3 tCircuit, in each file), the cars'
  standings in `gCircuitCars` (`tCircuitCar`) and the races in `gCircuitTracks`.
- **Ghosts** (`GhostStart`, `GhostUpdate`): a ghost race records the player's controls step by step and races
  against saved ghosts.
- **Stunts** (`groups/stunt`): trackers for flips, spins, rolls, two-wheel driving, air time and combinations add
  tricks to a run (`AddStunt`) that the landing banks.
- **Battle** (runtime image B, `src/ovl_b`): weapons on the cars (`weapons.c`), shots (`shots.c`), power-ups
  (`powerups.c`), damage (`BattleDamage`, `ProximityDamage`), scorch marks (`worlddam.c`).
- **The demo** (`groups/demo_pair`): the attract mode's 30-second drones' races, track by track.

## 5. The cameras

Each window has a camera (`gCamera[4]`; the files read it as `tCamera`, MAT3 anon_mat_n64_98: `mat`, `n64`, `fcam`,
`view2spring`, `slot`). `CheckCameraView` picks each window's view (the player's view button, the demo's random
cuts, the finish) and `update_camera` moves the camera for it: 0 the driver's eye, 1 the lagging chase view, 2 and 3
`View3Cam`'s, 4 the crash (`DeathCam`), 5 the finish's circling (`circle_camera_around_car`), 6 a camera fixed by
the track (`fix_camera_in_space`, `init_camera_on_track`), 7 the steady camera at the start (`steady_move_cam`), 8
and 10 the demo's car cameras (`UpdateCarObj`), 9 a camera over the drone path (`maxpath_cam`). The chase views
build on `UpdateRearCamera`; `CameraCollision` keeps a camera out of the walls. `UpdateCameras` makes the matrix
the N64 draws by (`n64`) and updates the sky.

## 6. What is still the N64's shape (for wave 3)

- **Targets** (`tTarget`) are PC data in the host layout since wave 3's step 3 (docs/port/NATIVE.md): their link
  and their kind's record are pointers, and so are the visuals' data words and the target groups' list that hold
  them. **Their records** keep the N64's layout (heap blocks the game's heap accounts): a path target's record
  (`tTargetStructPath`) holds its path, a PTHD record (`tBasicTargetPath`, its points `TargetPathPt`), as its index
  in `gTargetPaths.paths` (`pathIdx`, MAT3's `pathData`; `TARGET_PATH`); a bounding box model (`tMiniModelData`)
  keeps its box as its row of `BBoxCollInfo`.
- **The model record** (`tModelData`, 0x808 bytes on the N64) has host pointers on the PC (0x830 bytes,
  [CARS.md](../port/CARS.md) 4); each file reads it through a view of the fields it needs (MAT3's names), which
  declares the pointers too.
- **The car record** (`pc_car`, 0x510 bytes on the PC) is the N64's with host pointers in its visuals and input
  record: the files' views pad over the visuals (`struct pc_visual visuals[21]`), and `CAR_OFS` maps an N64 offset.
  Many fields the files read (MAT3's `restart`, `camera`, `view`, `saveview`, `collidable`, `crash_state`,
  `lastResPath` ...) are not in `pc_car` yet.
- **Data laid out by bytes**: `ZoidWinInfo`'s `ArbEntry` (MAT3 `tWindowInfo`) keeps a byte array where the files
  read fields: it is own BSS, whose field runs data.c carries, so its real members change data.c (runs the loader
  does not use for BSS); its last view (ZOID_InitializeWindow, which writes the clip planes and the fog) waits for that, wave 3.
  `gCamera` (`tCamera`), `gStuntInfo` (`StuntInfo`, MAT3 tStuntInfo) and `gCannMuzzleObjs` (`tCMuzzleObj`) have
  their real members since 2026-11-09: they are arena BSS, which data.c places by address and size only.
- **One type per datum** (2026-11-09): every file reads the race, track and camera data by include/game's types;
  the records some files still complete for themselves are those include/game leaves incomplete (`tBasicTargetPath`,
  `pathtyp`, `tModelPart`, `tVisual`), and `tCircuit` in the profile.
- **The profile's data** is read through `PROFILE_OFS` and `SLOT_DATA` offsets and pointer-sized reads (circuits,
  ghosts).
- **Walks to the next datum**: the arena keeps some arrays longer than include/game declares (`gCircuitCars[6]`,
  the circuit races), and the N64 code's loops ended at the next symbol; GetCircuitCode now stops by its count.
