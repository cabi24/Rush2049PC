# The two runtime images (overlays)

The N64 has 4 MB of RAM, too little to keep the front end's screens and the special race modes resident with the
game. So two parts of the game are *runtime images*, overlays the game inflates from the cartridge into one shared
block of its heap when it needs them, one at a time:

| | Image A: the front end | Image B: the HUD extras |
|---|---|---|
| Sources | `src/ovl_a/`, `nonmatching/ovl_a/` | `src/ovl_b/`, `nonmatching/ovl_b/` |
| Loaded | when the game enters its menus (after boot, and after each race) | at the start of a battle, stunt or obstacle race, and of every race when the game found an Expansion Pak (`expansion_pak_present`) |
| Unloaded | when a race starts | when the front end is loaded again |
| Loaded flag | `start_overlay_loaded` | `extra_overlay_loaded` |
| Code | 0x8038A400 to 0x803AF978 | 0x8038A400 to 0x80393B34 |
| Data | 0x803AF978 to 0x803B9A50 | 0x80393B34 to 0x80394F70 |
| BSS | 0x803B9A50 to `start_overlay_bss_end` (0x803BAE20) | `gShotObjList` (0x80394F70) to `extra_overlay_bss_end` (0x8039B440) |
| Original modules (MAT3) | warningscr, trackscr, titlescr, licensescr, eggs, startscr, selectcarload, recordscr, playerscr, pakscr, modescr, circuitscr, cheatscr, carscr |
 worlddam, weapons, streaks, shots, powerups, hudextra |

Both images start at 0x8038A400 on the N64 (`overlay_base`): the game's overlay loader (`load_overlay`, `PrevMaxPath` in
the C) reserves the heap block from there to 0x803BB380 (`PurgeableMemory`), inflates the image into it, clears its BSS
and sets the flag; unloading frees the block (`DeallocateMemoryInternal(overlay_base)`) and clears the flag. On the PC
the block is an offset in the heap, the same place (`runtime_image_block`: `PC_RUNTIME_IMAGE_OFFSET` past the main
heap's header, [NATIVE.md](../port/NATIVE.md) step 6), and a load resets the linked-in image's data.
`LoadFrontEndOverlay(1)` (`display_enable` in the C) unloads image B if it is there and loads image A;
`LoadFrontEndOverlay(0)` unloads image A when the race state starts, and `load_extra_overlay` (`InitMaxPath` in the C)
loads image B there for the battle, stunt and obstacle modes, or for any race with an Expansion Pak. The game image
calls into whichever image is loaded by plain function calls and reads some of its data by name; nothing checks that the
right image is loaded, the state machine just never calls the other one's code.

**On the PC** both images' code is linked into the executable like the rest of the game, and their data and BSS are
arenas, blocks of host memory in the N64's layout (their data ranges don't overlap, only their code did). Loading
an image refills its arena from the cached inflated image, zeroes its BSS and puts back the data its C defines
(`pc_data_overlay_load`, docs/port/DATA.md section 5); the other image's data is left alone. The flags and the
heap reservation still work as on the N64, so the game's behaviour is unchanged. Wave 3 makes the images ordinary
PC data and the load a reset of that data.

Names: every function and datum named here comes from `tools/port/names/ovl.tsv`, with its evidence; MAT3 names
where the N64 function is shown to be MAT3's, else a name from what the code does. A file-static MAT3 name that
several files use (`initScreen`, `animText`, `blit`) carries its file's name as a prefix (`cheatscr_initScreen`,
`hudextra_blit`). The `_stub` functions and the stubs named after an inlined static are the 8-byte empty bodies IDO
leaves where it inlined a static function: nothing calls them, and the PC defines them only so that a pointer to
one would still resolve (nonmatching/ovl_*/stubs.c).

## Image A: the front end

Image A is every screen between races: the title, the licence and controller-pak warning screens, the start
menu, the mode, track, car, player (profile), records, circuit and cheat menus, and the controller-pak manager.
The game's front-end state machine calls one screen's update function each frame while image A is loaded.

### The shape of a screen

Each screen is one MAT3 file with the same parts (MAT3 names; a static that many files share carries its file's
prefix in the map, `playerscr_initScreen`, `trackscr_buttonInfo`):

- `update<Name>Screen` runs once a frame. On its first frame (`inited`, `screenInited`) it calls `initScreen`,
  which creates the screen's 3D buttons (`initButtons`), a pane (`InitPane`) and the text blit (`NewMultiBlit`
  over the screen's `MultiBlit` table). It reads the pads' new and auto-repeated buttons (`gActiveEdges`,
  `gActiveAutoEdges`), moves the selection, and when the player leaves it calls `uninitScreen` and `setGameState`
  with the next screen's state bit.
- `updateButtons` lays out the 3D buttons every frame: each option is a model (`tMenuButton`, 64 bytes:
  buttonType, object index, angle, matrix, alpha) in the screen's `buttonInfo[]`; the list scrolls by
  `gButtonScale`/`gButtonSpace` between `maxY` and `minY`, the selected button turns to face the camera, and a
  cursor model follows it with `cursorSpeed`/`scrollSpeed` easing. `buttonsInited` says the buttons exist.
- `animText` is the blit callback that draws the screen's text each frame (`animButtonText` on the buttons once
  they exist, else a flat list); some screens add `animNewSlider` (sliders), `animArrows`, `animStatBars`.
- The texts are `gText` entries; the textures and models are looked up by name (`MBOX_FindObject_WORK`,
  `MBOX_FindTexture_WORK`), and the name strings are image A's data (`*_title_texture_name`,
  `*_button_texture_name`, `*_select_texture_name`).

### The modules

In the image the update functions and their statics come first, one MAT3 file after another; the second half of
the code is the *callback region*, every file's blit callbacks with the statics inlined into them, in the
alphabetical order of the original file names (that order identified most of it).

| MAT3 file | Screen | Entry | What else is there |
|---|---|---|---|
| warningscr.c | the controller-pak warning at boot | `updateWarningScreen` (image A's function at 0x8038A400), `warningscr_animText` | `warningscr_blits`, `gPakScr` (the highlighted choice), `num_paks_found`, `gBlocksNeeded` (free pages a profile needs) |
| trackscr.c | track select: a carousel of track pictures, the race options (laps, mirror, fog, wind, ghosts, drones, death mode, difficulty, catch-up), a rotating 3D map of the track | `updateTrackScreen`, `trackscr_animText`, `trackscr_animNewSlider`, `animArrows` | `gTracks` (the carousel), `trackMap` (one 0x48-byte entry per map object), `buildTrackMaps` builds a display list per track from the path data (`buildRaceTrackMaps` and the stunt, battle, obstacle variants), keeping each map model's first LOD in `saved_map_lods` so `clearTrackMaps` can restore it; `n64FogPal` is the fog picture's palette (N64 only); the function-local statics of `showTrackSelect`, `trackscr_animTrackMap`, `animTrackMaps` and `BlowFog` |
| titlescr.c | the title | `updateTitleScreen`, `titlescr_animText` | a countdown to the attract mode; Start gives player 0 controller 0 |
| licensescr.c | the licence screen | `updateLicenseScreen`, `licensescr_animText` | on the N64 also the gate that opens the pak manager when a button is held at boot |
| eggs.c | secret pad sequences | `CheckForEgg`, `InitEgg` | `gEggs` (22 records: a length, 20 held-button masks, 20 new-button masks; MAT3 has 24), `gEgg`, `gNextTimeOut`, `gEggIdx`; the last record is the start menu's code (`start_screen_egg`) |
| startscr.c | the start (main) menu | `updateStartScreen`, `startscr_animText` | 10 options; option 6, the cheat menu, appears once the start-menu code sets `cheat_menu_unlocked`; counts the attached controllers into `gMaxPlayers` |
| selectcarload.c | the car models of the car select screens | `InitSelectCarLoad`, `LoadSelectCar`, `UnloadSelectCar`, `UpdateSelectCar`, `InitVisualsCarScr` | `gSelectCarWavePtrs` (16 wave loads), `gSelectCar` (per car: created, hidden, ready, loading, colours, rims, tyre scales); `UpdateSelectCar` takes r, g, b on the N64 instead of a paint job |
| recordscr.c | the records screen: per-profile statistics and unlocks | `updateRecordScreen`, `recordscr_animText` | `gRecordState` (select or view); `recordscr_drawSelect`, `recordscr_drawView` and one draw function per page (`drawRaceStats`, `drawScores`, `drawBattleStats`, `drawStuntStats`, `drawObstacleStats`, `drawCircuitStats`, `drawTotalStats`, `drawUnlockedTracks`, `drawUnlockedCars`, `drawUnlockedParts`); `gStatPlayer` 0 shows the machine's totals |
| playerscr.c | profile select, one window per player | `updatePlayerScreen`, `playerscr_animText` | a state machine per player (`gPlayerState`: select, wait, controls, create, delete, pak full, pak damaged) with one control and one draw function per state; name entry over `gPlayerNameChars`; `playerscr_gPlayerRects` places each player's text |
| pakscr.c | the controller-pak manager (delete files) | `updatePakScreen`, `AnimText` | `gState` picks the `_Control*` and `_Draw*` pair (pick a pak, file list, confirm, rumble pak, damaged pak); `CancelFunc` for the error box |
| modescr.c | the game-mode menu | `updateModeScreen`, `modescr_animText` | seven modes, valid by player count (a table it reads through `modescr_validOption_biased`) |
| circuitscr.c | the circuit menu: continue, review, new circuit, enter a code, the trophy | `updateCircuitScreen`, `circuitscr_animText` | `gCircuit` (the circuit record in the profile), `resetEnabled`, `controlCircuitCode` (a 20-letter code), `controlShowTrophy` spins `gTrophyMat`, `drawReview` lists the reviewed track's six cars (`gCircuitCars`) |
| cheatscr.c | the cheat menu | `updateCheatScreen`, `cheatscr_animText` | 21 cheats (MAT3 23): each row writes a game flag and mirrors it in `gCheatState`; `gCheatActive` says which ones the player has unlocked |
| carscr.c | car select: cars, parts, colours, ghosts, stat bars | `updateCarScreen`, `carscr_animText`, `carscr_animNewSlider`, `animStatBars`, `animStatText`, `anim_car_color_marker` | `carscr_cars` (the carousel), `gCarComponents` (the part models of the highlighted option), `gStats` (from the transmission, handling, engine, tyre and frame tables of the car's settings), `gCarLocked`; the per-car settings are game-image tables of 5 rows (defaults, then one per player); three colour settings (`car_color_choice_a`/`_b`/`_c` into `paint_colors_rgb`) are N64 only; `setGhosts`/`getNextGhost` choose ghosts from `gGhostFiles` |

The other front-end screens (options, audio, video, controls, pause, high score, game over) are in the game image
, which also keeps the state machine, `gText`, the profiles and the controller pak
code.

### Data structures

- `tMenuButton` (64 bytes) and its 0x3C variant without alpha (`ButtonInfo`, the start menu); the tree's file-local
  slot structs of those sizes in image A are views of these.
- `MultiBlit` (MAT3 `tBlitDef`, 0x24 bytes): texture name, position and size, z, alpha, callback, callback id; one
  table per screen. The callback slot is how the screen's `animText` is reached; on the PC the data loader resolves
  that N64 address to the function.
- `tCircuit` (the circuit record: active, type, track), MAT3's `tCircuitCar` (one car's circuit results), MAT3's
  doubly linked list (`gGhostFiles`, `gPlayerFiles`: 16 bytes,
  count, head at +8, tail at +12).
- Labels into the middle of an object, or shifted by an index IDO folded in, are named `<object>_<field>` and
  `<table>_biased` (`trackMap_selMat`, `gPlayers_save`, `modescr_validOption_biased`, `gPlayerRects_biased`). They
  work only while the data keeps its N64 layout at its N64 address: wave 2 should write the object's field or the
  table's `[n - 1]` instead.

### N64 only

The controller-pak code everywhere (pak status codes, `gBlocksNeeded`, the licence screen's pak-manager gate, the
whole of pakscr.c), the Expansion Pak test on the fifth track (`expansion_pak_present`), the track maps' display
lists (MAT3 builds PS2 packets), the fog palette, the car colour settings and their marker callback, and the 8-byte
stubs where IDO inlined a static. Six of those stubs could not be tied to their static and are named by position
(`carscr_slider_inline_stub`, `trackscr_slider_stub_first`, ...).

## Image B: the HUD-extras overlay (battle, stunt and obstacle races)

Image B holds what only the special race modes need: the battle weapons, shots, mines and shields, the battle
power-ups, the scorch marks shots leave on the road, the light streaks behind shots, and the extra HUD of battle and
stunt races (weapon and power-up icons, ammo counts, health bars, battle points and coins, stunt points and coins).
The game loads it at the start of a race in battle, stunt or obstacle mode (with an Expansion Pak, at the start of
every race), and the front end's load drops it again. Its code is 0x8038A400 to 0x80393B34, its data
0x80393B34 to 0x80394F70, its BSS 0x80394F70 to 0x8039B440.

It is six MAT3 files, in this order in the image. Every function has its MAT3 name; the evidence is in
`tools/port/names/ovl.tsv`.

| MAT3 file | What it does | Functions | Its data |
|---|---|---|---|
| worlddam.c | scorch marks: a ring of 25 four-vertex road decals; a new one snaps its corners onto the road and fades the four before it | `findDamageSlot`, `newWorldDamage`, `initWorldDamage` | `DamagePolys` (the ring: full flag, oldest index, 25 polygons), `damTexture`, `dummyVtx` (the quad), `damColor` (one colour, four fading alphas), `scorch_texture_name` |
| weapons.c | the weapon model on each battle car, its muzzle flashes and the cannon's rip smoke | `RemoveCannMFlash`, `RemoveGattMFlash`, `AnimateWeapon` (the weapon visual's callback), `StartWeapon`, `InitBattlePlayer` (resets a car's battle state, also after a resurrect) | `weaponOffsets` (weapon type x body type x xyz), `gCurWeaponType`, `gGattMuzzleObjs`, `gCannMuzzleObjs` |
| streaks.c | light streaks behind shots: fading polygon ribbons | `UpdateStreaks` (with `AnimateStreakPolys` and `NewStreakPoly` inlined), `NewStreak`, `InitStreaks` | `gStreakList`/`gStreakArray` (50 streaks), `gStreakPolyList`/`gStreakPolyArray` (150 polygons), `gStreakColors` |
| shots.c | shots, shell casings, mines' hits, shields, damage | `ShotInit`, `ShotUpdate` (each frame), `UpdateShots`, `InitNewShot`, `CreateShotObject`, `UpdateShotObject`, `ShotDone`, `ShotToCarCollision`, `SonicCarCollision`, `ProximityDamage`, `BattleDamage`, `MissileLock`, `ElevateGun`, `UpdateDefense`, `AnimateCase` | four pooled lists (`gShotList`/`gShots` 24, `gHitPolyList`/`gHitPolys` 24, `gShotObjList`/`gShotObjs` 24, `gShotCaseList`/`gShotCase` 30), the object and texture tables `ShotInit` looks up (`WprObjNames` into `gWprObjList`, `WfxObjNames` into `gWfxObjList`, `MineTexNames` into `gMineTextures`, `RipTexNames` into `gRipTextures`), `gCarShield`, `hitColor`, `wallTidx`; the C defines `gShieldSizes`, `sWeaponFx`, `CaseOffsets`, `ShotOffsets` |
| powerups.c | health, shield and invisibility power-ups that respawn on the level's targets | `PreInitBattlePowerUps`, `IncBattlePowerUps` (one per power-up target the level loads), `InitBattlePowerUps`, `UpdateBattlePowerUps` (each frame) with `UpdateBattleHealth`, `UpdateBattleShield`, `UpdateBattleInvs`, `UpdateInvs` | `gBattlePowerUps` (count, timers, the slot array it allocates) |
| hudextra.c | the battle and stunt HUD | `ShowHUDExtra`, `RemoveHUDExtra`, `UpdateHUDExtraPostCamera` (with `animStuntCoins` and `animBattleCoins` inlined), `animWeaponIcons`, `animPowerupIcons`, `AddBattlePointInternal`, and the blit callbacks `animStuntPoints`, `animHealth`, `animAmmoCounts`, `animBattlePoints` | the two HUD layouts (`StuntMultiBlit`, `BattleMultiBlit`), the screen positions by window count and player (`stuntPos`, `stuntCoinPos`, `coinStuntPos`, `fragPos`, `healthPos`, `ammoPos`, `powerupPos`), colours (`playerPaneColors`, `playerColor`, `healthColor`, `stunt_coin_color`, `weapon_icon_color`), names (`stuntCoinTexNames`, `powerupIconNames`, `battle_coin_object_names`), the printf formats, and the per-player records `weaponIcon`, `powerupIcon`, `battleCoin`, `stuntCoin`, `battlePoint`, `healthPal`, `bonuscnt`, `hudextra_blit` |

**Who calls in.** The game image calls image B by name: `ShotInit`, `InitBattlePlayer` and the power-up setup when a
race is prepared, `initWorldDamage` when the track loads, `ShotUpdate` and `UpdateBattlePowerUps` every frame,
`newWorldDamage` from the car code, `BattleDamage` and `ProximityDamage` from the collision and mine code,
`AddBattlePointInternal`, `ShowHUDExtra`, `RemoveHUDExtra` and `UpdateHUDExtraPostCamera` from the HUD. The four blit
callbacks are reached through the function pointers in `StuntMultiBlit` and `BattleMultiBlit`; `AnimateWeapon`
through the weapon visual. Game code also reads some image-B data directly: `coinStuntPos` (the HUD's coin counter),
`gMineTextures` and two entries of `gWprObjList` (the mines, which live in the game image).

**Data structures.**
- *Pooled lists* (MAT3 `tStatList`, called `StatLL` in the C): a header (`dbl`, element count, element size, the
  backing array, the active list's head, the free store) over a fixed array; `InitStatLL` threads the array,
  `GetStatLLE`/`FreeStatLLE` move one element between the lists. Every element starts with its link. Shots, hit
  polygons, shot objects, casings, streaks and streak polygons are such lists.
- *Muzzle objects* (`tMuzzleObj` for the gatling gun, `tCMuzzleObj` for the cannon): five flash models with their
  matrices, an on-timer, and for the cannon the rip-smoke model and its animation state.
- *HUD records*: an icon (`type`, object index, matrix), a stunt coin (adds alpha, screen position, start and
  destination, scroll time), a battle point in flight (`active`, frame count, from and to player, start and end
  position, points).
- *Position tables* are `[windows - 1][player][x, y]`: one row per split-screen layout.
- The car record's battle part (`gGameCar[i].battle`): weapon type (8 = none) and ammo, health, the damage this
  frame, the defense flags (1 invisible, 2 shield, 4 damage reduced), shot and fade state, points, the reload state.

**What is N64-only, and what the PC changed.**
- The image itself: inflated from the cartridge into the shared heap block at 0x8038A400, its BSS cleared, and the
  8-byte stubs IDO leaves where it inlined a static (`NewStreakPoly`, `AnimateStreakPolys`, `animStuntCoins_stub`,
  `animBattleCoins_stub`, `ClearBattlePoints`; nothing calls them). On the PC the code is linked into the executable,
  the stubs are empty functions (nonmatching/ovl_b/stubs.c), and a load refills image B's data arena and C data
  (docs/port/DATA.md section 5).
- Image B's first function and image A's first function share the address 0x8038A400. On the PC image B's is
  `findDamageSlot` and image A's is compiled under its own name (`updateWarningScreen`).
- The pooled lists' element sizes are the host sizes on the PC (pointers are 8 bytes); the power-up targets are
  level-object nodes that keep their N64 layout with 32-bit links (docs/port/POINTERS.md 9, 10).
- `weapons.c`'s three statics are `extern` on the PC so that `InitBattlePlayer`, a separate file, resets the same
  storage the N64 resets by address.
- `AnimateWeapon` reads `weaponOffsets[8]` when a car has no weapon, one row past the table; the N64 reads whatever
  follows, the PC reads zeros (`pc_no_weapon`). `UpdateInvs`' invisibility fade is clamped at 60 fps so the alpha
  can't wrap (`PC_FADE_STEP`).

## Things to watch in the overlay C

- **Reads of uninitialised locals the original code has.** `animPowerupIcons` (as MAT3) leaves the icon type unset
  for a car whose only defense flag is 4 (damage reduced); `anim_car_color_marker` leaves its palette and channel
  unset for a marker other than 7, 8 or 9; `NewStreak` leaves the half-width unset for a type above 3. The game
  never takes those paths at default settings; a cleanup should give them defined values without changing the
  paths it does take.
- **`AnimateWeapon` reads one row past `weaponOffsets`** for a car without a weapon (type 8); the PC reads zeros
  there (`pc_no_weapon`).
- **Labels into objects** (`gTargetList_head`, `gPlayers_save`, the `*_biased` tables, the image-B array entries
  the game reads by their own names: `gWprObjList_mine`, `gWprObjList_mine_glow`, `neutral_mine_texture`, the three
  power-up definitions in `gObjList`) only work while the data sits at its N64 address in its N64 layout: wave 2
  should write the object's field or element instead, before wave 3 moves the data.
- **`InitEgg` is defined twice** (its own file under src/ovl_a and src/ovl_a/groups/cheat_screen/eggs.c, the same
  body), and start_screen/group.c has a third, inlined copy that points at the start menu's record
  (`startscr_InitEgg` on the PC). One definition has to go when the maps are applied.
- **Same-named statics.** MAT3 gives many screens a static with the same name (`initScreen`, `animText`,
  `buttonInfo`, `blit`); the map prefixes them with the file (`trackscr_buttonInfo`). The two carscr.c statics the
  tree already calls `animButtonText` and `drawOptions` keep those names (they are `static` and only in
  nonmatching/ovl_a).
