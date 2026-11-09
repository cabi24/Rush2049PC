# The front end

Everything the player sees between races: the licence and pak warning at boot, the title and the attract mode,
the start menu, the player (profile) screen, the mode, track, car and circuit menus, the records, options,
controls and cheat screens, the controller-pak manager, and after a race the high-score and game-over screens.
This page is for a programmer new to it: what it does, where its files are, its data, how the screens follow one
another, and what is still shaped by the N64 (for wave 3). It uses the tree's names as they are after wave 2; the
original MAT3 names are given where they differ.

The area also holds two groups of functions that are not menus but that the N64 build placed among the screens'
callbacks and that were cleaned up with them: the **track objects'** start, animation and collision functions
(MAT3 `targets.c`, section 6) and the **MusyX glue** (section 7).

## 1. Where the code is

The front end lives in two places, because on the N64 most of it is a runtime image (overlay) that is loaded only
between races ([overlays.md](overlays.md)):

- **Image A** (`src/ovl_a/`): the screens that only exist between races. One MAT3 file per screen.
- **The game image** (`src/blob/`): the screens the pause menu also uses during a race (options, controls), the
  ones that run right after a race while the track is still loaded (high score, game over), the front-end
  dispatcher and the scrolling background. Their blit callbacks sit in the image's "animators" range for the same
  reason: they must exist when image A doesn't.

On the PC both images are linked in; image A's data is an arena refilled when the image is "loaded"
(docs/port/DATA.md 5).

| Screen (state) | Files | MAT3 |
|---|---|---|
| dispatcher | `src/blob/groups/front_end/group.c` (`updateFrontEnd`) | front.c |
| background | `src/blob/updateBG.c`, `src/blob/removeBG.c` (createBG is the scene area's) | front.c |
| licence (`GS_LICENSE`) | `src/ovl_a/updateLicenseScreen.c`, `licensescr_animText.c` | licensescr.c |
| pak warning (`GS_WARNING`) | `updateWarningScreen` (image A's first function), `src/ovl_a/warningscr_animText.c` | warningscr.c |
| title (`GS_TITLE`) | `src/ovl_a/updateTitleScreen.c`, `titlescr_animText.c` | titlescr.c |
| start menu (`GS_START`) | `src/ovl_a/groups/start_screen/group.c`, `groups/startscr_text/startscr_text.c` | startscr.c |
| secret codes | `src/ovl_a/CheckForEgg.c`, `groups/cheat_screen/eggs.c` (`InitEgg`), `startscr_InitEgg` | eggs.c |
| player / profiles (`GS_PLAYER`) | `src/ovl_a/groups/playerscr/`, `playerscr_draw/`, `playerscr_uninit/`, `playerBeingCreated.c`, `playerBeingDeleted.c` | playerscr.c |
| mode (`GS_MODE`) | `src/ovl_a/groups/modescr/`, `modescr_init/` | modescr.c |
| track (`GS_TRACK`) | `src/ovl_a/groups/trackscr/`, `trackscr_text/`, `trackscr_slider/`, `animArrows.c` | trackscr.c |
| car (`GS_CAR`) | `src/ovl_a/groups/carscr/`, `carscr_animNewSlider.c`, `animStatBars.c`, `animStatText.c`, `anim_car_color_marker.c` | carscr.c |
| car models for the car screen | `src/ovl_a/groups/selectcarload/`, `InitSelectCarLoad.c`, `LoadSelectCar.c`, `UnloadSelectCar.c`, `InitVisualsCarScr.c` | selectcarload.c |
| circuit (`GS_CIRCUIT`) | `src/ovl_a/groups/circuitscr/` | circuitscr.c |
| records (`GS_RECORDS`) | `src/ovl_a/groups/recordscr/`, `recordscr_draw/` | recordscr.c |
| cheats (`GS_CHEAT`) | `src/ovl_a/groups/cheat_screen/cheatscr.c` | cheatscr.c |
| pak manager (`GS_PAK`) | `src/ovl_a/groups/pakscr/`, `pakscr_draw/`, `pakscr_FindFile.c`, `CancelFunc.c` | pakscr.c |
| options (`GS_OPTIONS`, pause page 5) | `src/blob/groups/optionscr/` (`buttons.c`, `option_init.c`) | optionscr.c |
| controls (`GS_CONTROLS`, pause page 4) | `src/blob/groups/controlscr/` (`caller.c`, `dtc.c`, `rows.c`, `d9058.c`), `controlscr_text/controls.c`, `src/blob/animControls.c` | controlscr.c |
| pause menu's tear-down | `src/blob/groups/pausescr_cleanup/group.c` | pausescr.c |
| high score (`GS_HIGH_SCORE`) | `src/blob/groups/highscorescr/` (five of its files), `highscore_rank/`, `highscore_uninit/`, `highscore_unlocked/`, `highscorescr_draw/highscore.c`, `src/blob/censor.c` | highscorescr.c |
| game over (`GS_GAME_OVER`) | `src/blob/groups/gameover/gameover.c`, `gameoverscr_draw/results_table.c`, `src/blob/gameover_validOption.c` | gameoverscr.c |
| dimming blits | `src/blob/animControllerDim.c` (the idle warning), `animate_pause_dim.c`, `animate_blit_unchanged.c` | game.c, pausescr.c |

The audio and video screens, the pause menu itself and the HUD are the UI area's (docs/code/r5.md).

## 2. How the screens follow one another

The game's state `gGameState` has one bit set; its values are `GS_*` in `include/game/race.h` (MAT3's
`rush_flags.h` names). `setGameState(s)` asks for a state, and `updateGameState` switches at the top of the next
frame (`uninitGameState`, `initGameState`: fonts, waves, overlays). `GameLoop` calls `updateFrontEnd` in every state
of `GS_FRONTEND_MASK`; it runs the screen of the state, then `updateBG` and the profile saver `SaveServer`.

```
boot ─> GS_LICENSE ──(Start held at boot)──> GS_PAK ──> back to GS_LICENSE / GS_WARNING
            │
            ├─(first time)─> GS_WARNING ─> GS_TITLE
            └──────────────────────────────> GS_TITLE <──> GS_DEMO (5 s without Start)
                                                │ Start
                                                v
         GS_RECORDS, GS_OPTIONS, GS_AUDIO, <─ GS_START ─> GS_CHEAT (once the start menu's code is entered)
         GS_VIDEO                               │
                                                v
                                            GS_PLAYER ─> GS_MODE ─> GS_TRACK ─> GS_CAR ─> GS_GAME_INIT (the race)
                                                            └─> GS_CIRCUIT ──────────────^
race ... GS_HIGH_SCORE ─> GS_GAME_OVER ─> race again / GS_TITLE with gSkipToGameState
```

B on a screen goes back one step (car to track, track to mode, mode to player, player to start, start to title).
A **soft reset** (`gSoftReset`) sends every screen to `GS_TITLE`. A pak pulled from under a player's profile sends
the menus back to `GS_PLAYER`.

**After a race** the game-over menu doesn't open the next screen itself: it sets `gNewGameState = GS_TITLE` and
`gSkipToGameState` to the screen wanted (`GS_MODE`, `GS_TRACK`, `GS_CAR`, `GS_CIRCUIT` for the circuit's next race,
`GS_START` for quit, `GS_GAME_INIT` to race a ghost again). Each screen on the way checks `gSkipToGameState` first:
if it isn't `SKIP_NONE` and isn't the screen's own state, the screen passes straight on to the next one; the target
screen clears it. "Race again" without a ghost doesn't go through the front end at all (`gRestartGame`).

**The pause menu** (`gPauseState`: `kNoPause`, `kPauseMainScreen` ... `kPauseQuit`, MAT3's enum) runs the
options and controls screens as its pages 5 and 4 during a race; those screens test `gGameState & GS_FRONTEND_MASK`
to tell the two uses apart (the front end's has 3D buttons and the background; the pause menu's is a flat list in
panes, for the pausing player only), and go back with `setPauseState(kPauseMainScreen)`.

## 3. The shape of a screen

Each screen is one MAT3 file with the same parts (a static that several files have is prefixed with its file in the
tree: `modescr_initScreen`, `trackscr_buttonInfo`):

- `update<Name>Screen`, the state's frame: on its first frame (`inited`) `initScreen` sets the screen up; it reads
  the buttons, moves the selection and, when the player leaves, `uninitScreen` takes it down and `setGameState`
  asks for the next state.
- `initScreen` / `uninitScreen`: the 3D buttons (`initButtons`, `uninitButtons`), panes (`InitPane`,
  `RemovePanes`), the text blit (`NewMultiBlit` over the screen's `MultiBlit` table, `RemoveBlit`), the background
  (`createBG`, `removeBG`).
- `updateButtons`, each frame: lays the 3D buttons out. A button is a `MenuButton` (MAT3 `tMenuButton`, 64 bytes:
  its type, its scene object, its turn, its matrix and position, its alpha). The selected row's button makes a half
  turn (4 pi a second) to show its "selected" face (`BUTTON_SELECT` texture); the list scrolls by
  `gButtonScale`/`gButtonSpace` between `maxY` and `minY`, each button moving at most 180 a second towards its place;
  a cursor model spins beside the selection and eases towards it (`cursorSpeed`, 180 to 540 a second).
- `animText`, the text blit's callback: draws the screen's text every frame, over the 3D buttons once they exist
  (`animButtonText`: a row's text is skipped while its button faces away, half see-through while it turns), else as
  a flat list. Some screens add more callbacks: sliders (`animNewSlider`), arrows, stat bars.
- `validOption` / `optionValid`: which rows are offered (by the player count, the race type, unlocks, cheats).

The **buttons** are `gActiveEdges` (newly pressed, any pad), `gActiveAutoEdges` (newly pressed or auto-repeating),
`gActiveLevels` (held), and per player `gPlayers[p].edges` / `.repeated`: bits `BUTTON_START`, `BUTTON_A`,
`BUTTON_B`, `BUTTON_UP` ... (input.h). `PlayStartABSound`, `PlayUDSound` and `PlayLRSound` play the matching
menu sound. The secret codes (`gEggs`, `CheckForEgg`) are sequences of held and new buttons.

**Text** comes from `gText` (include/game's `TextTables`): `gText.single[n]` single strings,
`gText.taba[gText.tab[n] + i]` the i-th string of table n. Each file names the entries it uses (`TEXT_*`);
`TB_SetTextColor` and `TB_SetJust` take MAT3's `kTB_*` names (text.h; `kTB_FlashText` is the flashing colour of
the selection).

## 4. The main data

- `gPlayers[4]` (MAT3 `tPadState`, 76 bytes, 80 on the PC): a player's controller (`CONTROLLER_NONE` 5 without one),
  its button words, and at 0x48 a pointer to its profile handle. Files declare their own view of it.
- The **profile** (MAT3 `tPlayerProfile`, save.c's record; 48 bytes, 64 on the PC): links, its pak file handle
  (`fileh`, NULL for a guest), its id, `name[13]`, `controls[10]`, `rumble`, `datah`. On the PC its pointers are 8
  bytes, so its fields are later than on the N64 (`PROFILE_OFS` in pc_prelude.h); the files read it through a
  `PlayerProfile` view in its PC layout. `gPlayers_save` is the label of `gPlayers[0].profile`
  (the data loader makes it an alias into gPlayers' storage); the high-score files now write the field.
- The **pak file record** (MAT3 `tPS_File`; 76 bytes, 96 on the PC): links and two functions, then `pak`,
  `fileNo`, the note's name and suffix, its pages (`PSF_OFS`).
- Each screen's `buttonInfo[]` (`MenuButton` or a file view of it), its `MultiBlit` table (the texture name, place,
  z, alpha and the callback; the data loader turns the N64 callback address into the function) and its state
  variables (`gModeMenuSelect`, `gPlayerState[]`, `gHighScoreState`, `gGameOverState`, `gState` ...), with MAT3's
  enums as macros in frontend.h where several files share them (`kPlayerState*`, `kHighScore*`, `kGhostMenu*`,
  `kGameOver*`, `kState*`) and in the file otherwise (`kModeMenu*`, `kStartMenu*`, `kCircuitMenu*`, `kRecordState*`,
  `kViewMenu*`, `kTrackType*`; `kSelectMenu*` in the player and record screens' four files, each the same). The
  screens' own button indexes and options shared by a screen's files are frontend.h's too (`TRACK_OPT_*`,
  `CAR_OPT_*`, `PLAYER_BUTTON_*`, `RECORD_*`, `CONTROLS_*`, the name grid's `NAME_SEL_*`, a pak's `PAK_*`); the
  tracks (`TRACK_RACE_FIRST` ... `TRACK_OBSTACLE`, `NUM_TRACKS`) are track.h's, the menu sounds (`SND_MENU_*`)
  audio.h's, the shared text tables (`TEXT_YES_NO` ...) text.h's.
- The car screen's models: `gSelectCar[52]` (`SelectCarState`: created, hidden, ready, loading, its wave instance,
  the colours and rims and tyre scales it was last drawn with) and `gSelectCarWavePtrs[16]` (`SelectCarWaveSlot`:
  a model load: its wave, whether it started, the car slot).
- The race settings the menus write: `gRace` (the race type, `kOneRace` ... `kBattle`), `gTrackNum`,
  `gNumPlayers`, the per-player car settings (`gThisCar`, `gCarTrans` ... rows of 5: the defaults, then one per
  player), `gCheatState` and the cheat flags.

## 5. The screens in a sentence each

- **Licence**: starts the pak system; Start held at boot opens the pak manager; 3 s, then the pak warning (first
  time) or the title.
- **Pak warning** (N64): how many pages a profile needs; go on, or open the pak manager.
- **Title**: Start gives player 1 controller 1 and opens the start menu; 5 s without it start the attract race.
- **Start menu**: the player count (left/right on its row, at most the controllers plugged in), records, options,
  audio, video, the cheat menu once its code is entered, PC SETUP on the PC, exit.
- **Player screen**: one window per player: pick, create or delete a profile, or play as a guest; each window its
  own state machine (`gPlayerState`).
- **Mode**: race, practice, time trial (ghost), circuit, stunt, obstacle (once track 18 is unlocked), battle; the
  rows offered depend on the player count (`modescr_validOption`).
- **Track**: a carousel of tracks and the race options (laps, mirror, fog, wind, ghosts, drones ...), a rotating 3D
  map of the track.
- **Car**: per player, the car, its parts (transmission, engine, tyres, frame, wings, rims), its colours and, for a
  ghost race, the ghost; stat bars for one player.
- **Circuit**: continue, review, new circuit, enter a code; the trophy.
- **Records**: per-profile statistics and unlocks, page by page.
- **Cheats**: 21 cheats, each switched once its own code has been entered on this screen.
- **Pak manager**: pick a pak, list its notes, delete one (two buttons together, then "are you sure").
- **Options**: the language and thirteen on/off options; in a race, applied at once.
- **Controls**: per player, ten controls mapped to buttons and the rumble; conflicts must be resolved to leave.
- **High score**: what the race unlocked, the circuit's code, name entry for new records (a 9 x 6 letter grid,
  censored), the ghost save menu after a time trial, the tables.
- **Game over**: score, race again, new mode, track or car, the circuit's next race, quit; and the standings.

## 6. The track objects (targets.c)

Cones, pumps, cacti, fences, windows, signs, coins, flags, mines, weapon icons, windmills: a **target** is a
level-object node (MAT3 `tTarget`, 0x70 bytes) with its kind (`infoIdx`, an index into `targetInfo`), its scene
object, its matrix, flags and a per-kind data pointer. `CheckTargetCollision` (the track area's) tests each nearby
target with its kind's collision function (`gTargetCollFuncPtrs`: `TargetCollRadius` 0, `TargetCollCylinder` 1,
`TargetCollMineCylinder` 2, `TargetColl3Piece` 3, `TargetCollBBox` 4, `TargetCollTrigger` 5) and on a hit sets its
`TARGET_HIT` flag and calls its kind's start function (`StartCone`, `StartMine`, `StartCoin` ...), which takes an
environment visual (`GrabEnvEntry`) whose function animates it (`AnimateVelRotGeneric`, `AnimateKnockdown`,
`AnimateObjSwapGeneric` ...) until it releases itself. Flags: 1 started, 2 active (collides), 4 hit, 0x40 a gold
coin, 0x80 a coin every player has.

The files are in `src/blob/` (`Start*.c`, `Animate*.c`, `TargetColl*.c`); each declares only the fields of
`Target` and of the visual (`Visual`, or `struct pc_visual`) that it reads, with MAT3's field names.

## 7. The MusyX glue

`src/blob/musyx_*.c`: six of the eight services the MusyX sound library calls through its table (allocate, the
cartridge address of a sample, start a list of sample DMAs, set and clear the retrace callback, wait for the audio
task; `musyx_submit_audio_task` and `musyx_free` are the audio area's). N64 only in what they do; the PC keeps them
because the library is the same.

## 8. Still N64-shaped (for wave 3)

- **The targets keep their N64 layout.** A target, its per-kind data (`CoinData`, `MineData`, `VelRot`, a path's
  records) and the visual's `data` are 32-bit window addresses read through `PC_N64P`/`PC_N64W` (the `N64P(x)`
  `_Generic` macros dispatch on the enum type of the field). `struct Target` is defined by each file that reads it
  (fourteen files here, three in nonmatching/game), each with its own subset of fields: one shared definition in
  types.h, once every file agrees, is the next step. `TargInfo` (track area) still has guessed field names; MAT3's
  are `mboxname`, `startFunc`, `animFunc`, `collFuncIdx`, `Flags`, `objListIdx`, `structureID`, `structureIdx`,
  `collVal`, `startSnd`, `moveSnd`, `stopSnd`, `replace`, `fadeDist` (StartCone.c has a view with them).
- **Image A's data** is an arena at its N64 addresses, refilled on each "load" (docs/port/DATA.md 5); its
  `MultiBlit` tables hold N64 callback addresses the loader resolves.
- **Records with pointers** (the profile, the pak file record, a ghost) are in their PC layout, but the code still
  reaches some of them through `PROFILE_OFS`/`PSF_OFS`-style views per file; the shared definitions in types.h
  (`PlayerSaveSlot`, `ProfileRec`) disagree with each other and should become one `PlayerProfile`.
- **gPlayers** has no shared type; each file has a view (`PadState`, `InputRecord`, `Player76` ...).
- **The controller pak**: the pak manager, the warning screen, `gBlocksNeeded`, the licence screen's pak gate and
  the pak status strings are N64 only; on the PC the paks are files (docs/port/SAVES.md).
- **The buttons' meaning** comes from the cartridge's `n64_button_map`: bits 0-2 and 10-13 are known from the code
  (Start, A, B, the d-pad), the others only by position (the pak manager's delete combination is bits 7 and 8, the
  start menu's debug row bit 8).
- **The cartridge window**: `musyx_cart_address` turns a sample's offset into an address in the N64's 0xB0000000
  window, which the PC's DMA emulation reads.
- **`gNumModels[0]`**: the model count is an array of 4 bytes in include/game (the data loader's layout: the gap to the
  next datum), so the screens read `gNumModels[0]`; MAT3's `unsigned char gNumModels` is for wave 3.
- **The track maps** (trackscr.c): `MapLod.displayList` and `verts` are 32-bit fields given host pointers to the
  display lists `buildTrackMaps` makes; that holds only while the heap is in the fixed window.
- **Labels into objects**: `flashColors_at_1` is `flashColors[1]`, `gPlayers_save` is `gPlayers[0].profile` (no
  longer used here), the trophy sheen's scale is `ObjInstanceList[node].w` (until wave 2c circuitscr.c wrote it
  through `object_list_scale_view`, a label 0x10 before `ObjInstanceList` read as 0x44-byte records: the window's
  copy, so the drawn sheen kept w 1; the label is gone); `gFrontTireScale` and
  `gRearTireScale` are declared one row long and read rows 1-4, which are `gFrontTireScale_players` after them.
- **Stale names** the screens still call: `player_mode_set` is `SetControllerStickEdges` and
  `sound_handles_array_clear` is `RemovePane` (image A's symbol table keeps the old names).
- **The profile's statistics** (MAT3 `tSaveData`): recordscr_draw.c and circuitscr.c have the first typed views
  (race, stunt, battle, obstacle and circuit records, the circuit state at 0x6F4); save.h's `StatsB` ... `StatsE`
  and `WeightRecord` are the same tables under guessed names.
