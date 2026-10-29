# More cars a race: up to 12

The N64 game races at most 6 cars: the human players plus drones, capped at 6 when the race's roster is filled. This
page is the map for a PC setting that lets a race hold up to 12: what counts the cars in each mode, every storage
and every bound a race of N > 6 cars reaches, the start grid, loading and memory, audio, the front end and the HUD,
and the per-frame buffers; then a work plan. A throwaway spike raised the caps and ran 8- and 12-car races on the
synthetic and the retail cartridge under UBSan and AddressSanitizer; what broke, in order, is section 9. Written
2026-10-29 against main 25f43c8 (the race roster `pc_links` and the model records `pc_models` decoupled,
[DECOUPLE.md](DECOUPLE.md) 2-3); the car records `player_array` (to `pc_cars[12]`) and `PC_MAX_LINKS` 12 were in
progress elsewhere at the time.

| Piece | Where |
|---|---|
| This page | `docs/port/MORECARS.md` |
| The spike (never merged) | local branch `morecars-spike` (3 commits on 25f43c8), worktree `build/spike-src`; the same as patches in `build/morecars-spike-patches/`, with the spike's tools (`declsize.py`, `ubdiff.py`, `ubsrc.py`) and the UBSan site lists of section 9 |
| The spike's bounds-declared tree | local branch `morecars-spike-ub`, worktree `build/spike-ub` (the spike plus `declsize.py`'s 1,904 declarations in 778 files, uncommitted) |
| Retail runs, logs, screenshots | `build/scratch/retail/` (local only, never committed) |

Names: N is the number of cars a race holds. MAT3 names (`gNumCars`, `gLink`, `model[]`) come from the GameCube
decomp the game C cites; `slot` is a car's index into the roster, the model records and the car records (0 to N-1,
humans first).

## 1. How the game counts its cars

### 1.1 The counts

| Variable | MAT3 | Type | Set by | Meaning |
|---|---|---|---|---|
| `D_8014A108` (`active_player_count`) | `gNumPlayers` | s16 | the main menu (`src/ovl_a/groups/start_screen/group.c:177-201`), `func_800DE45C` (vsync/func_800DE45C.c:133, 1), GhostStart (`menu_save_options/props.c:194, 263`), `ghost_race_setup.c:123`, `state/state.c:153` | the human players, 1-4 |
| `D_80142724` | `gNumDrones` | s16 | `SetOptions` (`large_init_state/group.c:100-219`, per mode), `func_800DE45C.c:156` (boot: option 22) | the drones asked for |
| `D_801543CA` | `gNumCars` | volatile s16 | `CalcPolePositions` (`update_game/init_state_continue.c:90`), `func_800EC190.c:51` (the attract demo: 6), GhostStart (`props.c:193, 261`) | the roster slots in the race |
| `D_80152744` | `gNumActiveCars` | s8 | `func_800EC914.c:103-111` (munge), `world_physics_tick.c:253, 286` (init_cars) | the slots with `LINK_ACTIVE`, counted; `pc_models[i].slot` lists them |
| `D_80153FD2`, `D_80152768`, `D_8015274C` | `num_humans`, `num_drones`, `num_our_drones` | s16 | `func_800EC914.c:123-140`, `display_list_flush.c:300-317` | the three lists below, counted |
| `D_80143A40`, `D_801527D8`, `D_80152808` | `humans[]`, `drones[]`, `our_drones[]` | s16 lists | the same | slots by kind (section 2.3) |

The race's cars are `D_801543CA` roster slots; each active one gets a model record and a car record of the same
index. `func_800EC914` (MAT3 `munge_gLink_data`) and `world_physics_tick` (`init_cars`) walk `i < D_801543CA` and
build `D_80152744` and the lists; everything after that walks `D_80152744` (or the lists), or a literal 6
(section 3).

### 1.2 Per mode

`gameplay_mode` (`D_8014A110`, MAT3 `gRace`): 0 single race, 1 practice, 2 ghost, 3 circuit, 4 stunt, 5 obstacle,
6 battle (the `k*` constants of `resurrect/resurrect.c:223-228`).

| Mode | Drones (`SetOptions`, `large_init_state/group.c`) | Cars |
|---|---|---|
| attract demo (`state_word_a & 8`) | 6 (line 100); the demo's roster is filled by `func_800EC190` instead (6 slots, all drones, `D_801543CA = 6`) | 6 drones; the camera follows car `D_801163F4`, a counter that wraps at 6 (`func_800EC190.c:64-69`) |
| 0 single race | option 22 (line 114), 0 with 3 or more players (line 115) | humans + min(drones, 6 - humans) (`CalcPolePositions`, `init_state_continue.c:83-90`) |
| 1 practice | 0 (line 131) | the humans |
| 2 ghost | 0 (line 147) | humans and ghosts, 2 slots without the Expansion Pak, 4 with it (`props.c:181`); a ghost is an owner-6 slot with its own input record (`props.c:236-263`) |
| 3 circuit | 5 (line 163) | humans + min(5, 6 - humans); places from the circuit's standings after its first race (`init_state_continue.c:92-99`), random places in the first (`:105-116`) |
| 4 stunt, 5 obstacle, 6 battle | 0 (lines 180, 202, 219), and `CalcPolePositions` forces 0 in these modes (`init_state_continue.c:87-89`) | the humans |

The retail mode screen offers 3 and 4 players only stunt and battle ([BRINGUP.md](BRINGUP.md) "Where it stands"), so
a race with drones has at most 2 views. The synthetic cartridge's mode table allows every mode for every count, which
is why `race3p` and `race4p` exist there.

### 1.3 The Drones option

| What | Where |
|---|---|
| storage | `D_80146108[22]` (`gOptions`, an s8 array of 41); options 21-40 are saved per player profile and per track (`SavePlayerOption` = `audio_bus_mix`, the getter `reverb_setup`, `reverb_setup/reverb.c:3-6`: pages 0-5 and 19-24 are the race tracks) |
| default | 5 (`func_800CCB40.c:20`, case 22) |
| the menu | the track select's options list, item 17 (`runtime_a_c93c_closure/group.c:553-558`, in `func_8038E5DC`): LEFT/RIGHT adds -1/+1 and wraps 5 → 0 and -1 → 5; the reset button sets 5. After any change `init_state_begin()` (= `SetOptions`, line 581) copies it to `D_80142724` |
| shown | `settings_values.c:184-186` (case 17): `sprintf(text, D_803B925C, D_80142724)`, the copy, not the option |
| offered | `func_8038ACB0` (OptionValid, `runtime_a_c93c_closure/group.c:776-795`): the per-mode table `D_803B77D4[mode][opt]` (image A data), and in mode 0 only with fewer than 3 players (line 780) |

So the option's range (0-5) is code, not data: two constants in the menu's case, the default in two places (`5` at
line 558 and `func_800CCB40`). A profile written by the PC with a value above 5 is read back as is; an N64 reading it
would cap the race at 6 in `CalcPolePositions` and wrap the value on the next press.

### 1.4 Owner codes are not car counts

A roster record's `owner` byte (+7) is 0-5 for a drone (the arcade's linked node that drives it; always 0 on the N64),
6 for a human, 7 for an empty slot ([DECOUPLE.md](DECOUPLE.md) 2). The 6 and 7 are codes: `func_800EC914.c:114`
(`owner < MAX_LINKS ? DRONE : HUMAN`, with `MAX_LINKS` 6 in that file), `init_mdrive/group.c:88`,
`race_countdown_display.c:113, 125`, `music_tempo_set.c:81`, `music_control.c:149`, `menu_controller_remap.c:176`,
`update_game_data.c:417`, `func_800DE860.c:101`, `car_checkpoints_func_800F8EC8.c:140`, `race_tail.c:174`,
`camera_play_script.c:240`, `display_list_flush.c` (`-= 6`). They stay as they are for any N; a `MAX_LINKS`
raised in `func_800EC914.c` would make every human a drone.

## 2. Per-car storage

### 2.1 How it was found, and what the searches covered

- **Static.** Every live game file (`game_objects.txt`, 1,308 files: 1,292 under `src/`, 16 under `nonmatching/`),
  weak context copies marked by `build/pc/weak/*.h` and dropped: (a) every loop whose bound is a car count (6, 12,
  13, `D_801543CA`, `D_80152744`, `MAX_LINKS`, the list counts) or that walks `player_array`/`PC_MODELS`, with the
  data symbols its body indexes by the loop variable: 138 loops; (b) every indexing of a data symbol
  (`NAME[expr]`, `(&NAME)[expr]`, `PC_MODEL_*(expr)`, `PC_LINK_*(expr)`): 9,484 sites on 761 symbols, ranked by
  index expressions that name a car (`slot`, `node`, `drone`, `place`, `->index`, the lists...): 152 symbols, sorted
  by hand into per car type (the `[13]` tables), per player or window, per track, and per car slot, the last group's
  users and neighbours listed symbol by symbol; (c) `memset`/`memcpy` with a byte count over the candidates;
  (d) comparisons with 6, `% 6`, `6 -`, `+ 6`: 181 lines. Each candidate's count, neighbour and users come from
  `data_symbols.txt` (main, build host) and the code. Text searches miss a car index that reaches an array through a
  pointer the search doesn't follow (an m2c byte offset off a record pointer); the checks below cover the rest.
- **Dynamic.** In the spike's bounds-declared tree every open-ended `extern` of an arena symbol got its extent as an
  array bound (`declsize.py`: 1,904 declarators in 778 files; `data_symbols.txt` and the synthetic cartridge are the
  same files after it), so `-fsanitize=bounds` names each index past an array's N64 extent. All 19 synthetic paths
  ran with 6, 8 and 12 cars (UBSan, build host); `ubdiff.py` keeps the sites that 8 or 12 cars reach and 6 don't.
  The retail cartridge ran the same press scripts under ASan + UBSan on the Pi (section 9).
- **Through pointers.** `layout_check.py --storage` over the 23 storages of 2.3 and the matrix pool (main, build host:
  529 accesses) finds the users the table lists, plus `func_800FBE60` (`D_80152218`) and `mode_flags_clear`
  (`D_80110680`), which the table now has, and the callees that receive one record's address and stay inside it
  (`player_conditional_call`, `player_conditional_check`, `PitchUV`, `func_80090E9C`, the skid functions
  `func_800AF844`, `func_8008C074`): none reaches a record by a car index of its own.

### 2.2 The records

| Data | N64 | Storage | N | Status |
|---|---|---|---|---|
| the roster `pc_links` (`D_80153E88`) | 13 × 8 B | decoupled | 13 | enough: `PC_LINK_SLOTS` is the larger of 13 and `PC_MAX_LINKS` |
| the model records `pc_models` (`D_8014A250`) | 6 × 0x808 | decoupled | `PC_MODEL_SLOTS` | 8 at 25f43c8 (the larger of 6 and `PC_MAX_LINKS`); 12 with `PC_MAX_LINKS` 12 |
| the car records `player_array` (`D_80152818`) | 6 × 0x3B8 (0x80152818-0x80153E68) | own, 0x468 a car | 6 | in progress (`pc_cars[12]`). `D_80153E68` (an `OSMesgQueue`) follows on the N64 |
| the input records `D_8014A118` | 4 × 0x4C (304 B; DECOUPLE.md 4's "6 × 76" is 4) | own, 80 B | 4 | per human: `world_physics_tick.c:291` gives `&D_8014A118[i]` to human slots, which are 0-3. Stays 4 |
| the car-part records `gCarParts` `D_80139320` | 52 × 0x40 (3,328 B) | arena BSS | 13 in a race | per roster slot in a race (`sfx_position_3d.c:77-87`: cleared, then `sfx_volume_set` for 13 slots; `startcar.c:146`, `rocket.c:319-554`, `music_seq_load.c:119-124`); 4 players × 13 parts at the car select (`sfx_stop.c`). Enough for 12 |
| the car palette descriptors `D_8013FEE0` | 12 × 24 B (0x8013FEE0-0x80140000) | arena BSS | 12 | per roster slot (`func_800BB02C.c:51-70` from LoadCars, `carcolor.c:168`, `startcar.c:115`, `func_8008BEA4.c:40`, `heap_release/group.c:150`). The tables list 24 B for it and 264 B for `D_8013FEF4` (+0x14, a name inside); the range holds 12. Enough for 12 |

A car's own visuals (wheels, shadow, sparks, wings, brake lights, the arrow: 21 a car, [CARS.md](CARS.md) 2) live in
its car record, and the drone cameras are its scene node's (`func_800EB90C.c:262-270`); both follow the records.

### 2.3 Per-car arrays

Every array here is indexed by a car slot for every car in the race; N64 count is the code's, the neighbour is the
next symbol in `data_symbols.txt`, and "hit" is what a 7th or later car writes over (on the PC, the window bytes under
an own-storage symbol are unused, so an overrun into them does no harm until it runs past them). All are BSS unless
marked ROM. Fix, for each: PC data with `PC_MAX_LINKS` entries (the [DECOUPLE.md](DECOUPLE.md) recipe), unless
noted.

| Array | N64 | Count | Users | Neighbour; what an overrun hits | Found by |
|---|---|---|---|---|---|
| **Sound** | | | | | |
| `D_80140420` (engine sound set: the engine table pointer and four handle records; `SndSet`/`Player84`) | 0x80140420, 504 B | 6 × 84 (own, 88 host) | StartCarSounds `mode_select/d5e64.c:92-110`, StopCarSounds `func_800D5524.c:87-92`, the engine update `mode_select/mode.c:233` | own `D_801406B8`, then own `D_801407F0` (`gResPath`): a 7th car's record overwrote the resurrection path's header (the spike's first crash) | static, the first crash (gdb), UBSan |
| `D_80140640` (screech handle per car, `State20`) | 0x80140640, 120 B | 6 × 20 | `d5e64.c:106`, `func_800D5524.c:94`, `mode.c:343-366` (`func_800E05F0`, and `func_800E0048` through a pointer) | own `D_801406B8` (dead bytes), then `D_801406C0` (the humans' sound records) | static, UBSan |
| `D_8010FFC4` (car sounds started) | 0x8010FFC4, 8 B by the gap, ROM (zero) | 6 | `d5e64.c:91, 116`, `func_800D5524.c:82-83`, `mode.c:329` | `D_8010FFCC` | static, UBSan |
| `D_8010FFCC` (engine update phase: each car's engine is updated every other frame) | 0x8010FFCC, 8 B, ROM (an alternating pattern) | 6 | `mode.c:325-330`, `func_800D5524.c:84` (`(slot & 1) == 0`) | `D_8010FFD4` (ROM `s32[]`) | static, UBSan |
| **Drones** | | | | | |
| `D_801527D8` (`drones[]`) | 0x801527D8, 12 B | 6 | written `func_800EC914.c:135`, `display_list_flush.c:306, 315`; read `maxpath_controls/group.c:298, 303` | own `D_801527E4` (4 dead bytes), then `D_801527E8` at +0x10: a 9th drone overwrites the checkpoint distances | static, UBSan |
| `D_80152808` (`our_drones[]`) | 0x80152808, 16 B | 6 (8 by the gap) | written `func_800EC914.c:138`, `display_list_flush.c:307, 316`; read `update_drone_models` (`func_800E56F8/group.c:4040`), `:3924` | `player_array` (own): dead window bytes on the PC, the car records on the N64 | static, UBSan |
| `D_80153F28` (drone steer) | 0x80153F28, 24 B | 6 | `maxpath_controls/group.c:750-751`; cleared for 6 by `audio_effect_remove.c:3808-3812` | `D_80153F40` (`update_drone_models`' cursor) | static, UBSan |
| `D_80153F48`, `D_80153F68` (drone brake, gas) | 0x80153F48, 0x80153F68, 32 B each | 6 (8 by the gap) | `maxpath_controls/group.c:697-733`; cleared for 6 by `audio_effect_remove.c` | each other, then `D_80153F88` | static, UBSan |
| `D_80153F88` (drone heading, f32[3]) | 0x80153F88, 72 B | 6 | `maxpath_controls/group.c:444-563` (`func_800E398C`) | `D_80153FD2` (`num_humans`): a 7th drone set the human count to a float's high half, and the model thread walked 16,076 input records (the spike's second crash) | static, UBSan, crash |
| `D_80154138` (drone saved heading, f32[3]) | 0x80154138, 72 B | 6 | `maxpath_controls/group.c:549-563` | `D_80154182` (the drone the camera selected) | static, UBSan |
| **Checkpoints, places, results** | | | | | |
| `D_80152218` (last position, f32[3]) | 0x80152218, 72 B | 6 | `car_checkpoints_func_800F8EC8.c:132` (CheckCPs); set at the race's start by `func_800FBE60.c:3822-3824` (a 12-byte m2c stride, for `D_80152744` cars) | `D_80152260` (`uch[512]`) | static, UBSan |
| `D_801527E8` (distance to the checkpoint plane) | 0x801527E8, 24 B | 6 | `car_checkpoints_func_800F8EC8.c:155, 160` | `D_80152800` | static, UBSan |
| `D_80142DB4` (the HUD's car order: humans last) | 0x80142DB4, 12 B | 12 by the gap; cleared for 6 (`func_80105B74.c:91-93`), filled for `D_80152744` (`:95-117`) | read by the minimap dots (`func_80109A60.c:135`, `id & 0xf`) | `D_80142DC0`. Enough for 12; the clear loop wants N | static, ASan (its function's local) |
| **Effects** | | | | | |
| `D_801392D8` (tire-effect latches) | 0x801392D8, 24 B | 6 | `entity_spawn_init.c:216-309`, `func_8008E408.c:118-121`, `hud_render.c:204-213`, `func_80092484.c:3715-3719`, `func_800CB718.c:3707`; cleared for 6 by `physics_collision_test.c:81-82` | own `D_801392F0` (16 dead bytes), then `D_80139300` | static, UBSan |
| `D_8013A020` (tire smoke shade) | 0x8013A020, 8 B | 6 | `entity_spawn_init.c:306, 433-438` | `D_8013A028` (the four profiles' save blocks, `SaveData[4]`): a 9th car's smoke shade writes the first save block | static, UBSan |
| `D_8013C098` (two trail points, f32[6]) | 0x8013C098, 144 B | 6 | `entity_spawn_init.c:402-407` | own `D_8013C128` (dead bytes) | static |
| `D_80154660` (blast, `tBlastStruct`) | 0x80154660, 2,400 B | 6 × 400 | `blast.c:183-221`, `entity_update_callback.c:130-209`; set up for 6 by `camera_smooth_lerp.c:3831-3838` (InitBlasts, a 0x190 stride) | `D_80154FC0` (per player) | static, UBSan, crash (demo) |
| `D_80154FD8` (blast ring, `tBlastRingStruct`) | 0x80154FD8, 360 B to `D_80155140` (the tables list one 60-byte record) | 6 × 60 | `blast.c:241-244`, `entity_collision_detect.c:84-123`; set up for 6 by `camera_smooth_lerp.c` (a 0x3C stride) | `D_80155140` (the circuit's) | static, UBSan |
| `D_80155290` (skid state per tire, `WheelSlot[4]`) | 0x80155290, 2,208 B | 6 × 4 × 92 (own, 2,304 host) | `skid_marks/group.c:302`; cleared by byte count `func_800B0580.c:78` (`SKID_TABLE_BYTES`), `slot18_alloc/func_800B0580.c:54` (2208) | own `D_80155B30` (the skid segment pool): the retail spike faulted in the skids at 8 cars | retail crash |
| `D_80161450` (the resurrect's midpoint, f32[3]) | 0x80161450, 72 B | 6 | `track_select_handler.c:105-141` (the arcade's `update_resurrecting_car`) | `D_80161498` (a vertex buffer) | static, UBSan |
| **Cameras** | | | | | |
| `D_80110680`, `D_80110668` (the finish's circling camera: state, start time) | 0x80110680 6 B, 0x80110668 24 B, ROM (zero) | 6 | set for humans at the finish (`race_countdown_display.c:130-131`), read by car (`func_800E9E2C.c:39-44`) and by player (`func_800EB90C.c:222`); `render_post_process.c:35-37` defines it and clears 6; `mode_flags_clear.c:12` clears its first two bytes | `D_80110688`, `D_80110680` | static, UBSan |

Not per car, though they look it (each checked): `D_80150F88` (`WeightRecord[12]`, listed in [DECOUPLE.md](DECOUPLE.md)
4 as maxpath weights) is the best-times records per track, 6 tracks × 2 directions (`assign_drones.c:133`,
`draw_text.c:79`, `audio_channel_priority.c:39`); `D_80151578` (`StatsC[8]`), `D_80151410`, `D_801515F8`,
`D_80151618` are statistics (`draw_minimap.c:3705-3709` clears them, `func_800F769C.c`); `D_80151510`,
`D_801515D8` (f32, 6 each, set to 1 by `func_800FAEE4.c:7-10` at PrePlay) have no other reader; `D_80110270` (6
`Chan`) is the sound manager's timed events (`hud_setup.c:178-187`, indexed by handle); `D_80142B08` (s16[13]) and
`D_8013FE90` (13 used) follow the roster's 13; the maxpath per-car control (`mpctl`, car +0x314) and the drone fields
of the model record (`drone_target`, `drone_scale`, `time_boost`, `can_cheat`) live inside the records of 2.2.

### 2.4 Per player or per window: they stay 4

Indexed by a human's slot (0-3, guarded by `slot < D_8014A108`, `drone_type == 2`, `owner == 6` or a window loop),
by a window (`gNumWindows`, the camera's `slot`), or by a player: none needs more entries for more cars.

| Kind | Arrays |
|---|---|
| sound, humans only (`if (player < 4)` in `d5e64.c:107`, `MODE(model) != 2` returns at `mode.c:372`, `unk7CC == 2` in `func_800D5524.c:95`) | `D_801406C0` (`SndRec[4][3]`), `D_801407C0`, `D_801407E0`, `D_80140AE0` (s32[4]), `D_80140808` (impacts), `D_80140A08`, `D_80140B10`, `D_80140BE0`, `D_80142518`, `D_80110020` |
| cameras, per window | `gCamera` (`D_80150B70`), `D_801526A8`, `D_801526E0`, `D_801526F8`, `D_80152708`, `D_80152720`, `D_801106A4`, `D_801106B4`, `D_801525F8`, `D_80152680`, `D_80155178`, `D_801551A8`, `D_801551D8`, `D_80155210`, `D_80138668`, `D_80138878`, `D_801391E8`, `D_801392B8`, `D_80139308` |
| HUD, per window | `D_80142740`, `D_80142770`, `D_80142AC8`, `D_801543B8`, `D_80143F54` (`gRankedPlayer`, `FindWinner.c:51-53` fills `gNumWindows`), `D_801439D8` (4 time strings, `func_80105B74.c:88-90`) |
| race records, per player | `D_80149A78` lap splits and `D_80144018` (`func_800D2054.c:33`: `player < D_8014A108`), `D_80144DA8`, `D_80146180` (`world_physics_tick.c:292`) |
| battle and stunt (no drones) | `D_80399118`, `D_80399120`, `D_80399550` (`[4]`, image B), `gCarTeam` `D_8012E67C`, `D_80152038` (stunt), `D_8015256C` |

[DECOUPLE.md](DECOUPLE.md) 4 counts `D_801526E0`, `D_801526F8` and `D_801526A8` among the race's per-car words: they
are indexed by the camera's player (`pl`, `slot`: `steady_camera/group.c:3886-3983`, `func_800E92C8/group.c`,
`func_800EB90C.c:201-233`, `func_800EB028.c:129-134`), so they are per window; `D_80152218`, the fourth, is per car
(2.3). `D_8011B558` (`tRGBA[5]`) is indexed by `gCarTeam[slot]` in battle and by `vehicle->player` for the arrows, which only
humans have (`music_control.c:149`).

### 2.5 Per-car locals

| Function | Local | Fix |
|---|---|---|
| `func_800D1AB0` (MAT3 CalcPlaces, `car_checkpoints_func_800D1AB0.c`) | `s16 place[6]`, `s32 dist_tab[6]`, indexed by slot | `PC_MAX_LINKS`. Overrun: the spike saw indexes -11, -10 and 65544 read back from the stack |
| `render_large_objects` (MAT3 DoDrones, `render_large_objects/group.c:159-165`) | `humans`, `drones`, `car_in_conflict`, `cars_in_order`, `place` `[MAX_LINKS]`, `car_distance[MAX_LINKS][MAX_LINKS]`, `MAX_LINKS` 6 (line 53) | that file's `MAX_LINKS` to `PC_MAX_LINKS` (it has no owner-code use) |
| `func_80105B74` (the HUD's car order, a blit callback) | `s16 order[8]` | `PC_MAX_LINKS`. ASan: stack-buffer-overflow at 12 cars (retail) |

### 2.6 ROM tables a 12-car race reads past

| Table | N64 | What it is | Users | Past row 6 |
|---|---|---|---|---|
| `D_801210E8` (`pole_pos_offset`) | `f32[12][3]` | rows 0-5 the start grid, 6-9 the humans' respawn offsets (`slot + 6`) | `resurrect.c:470-477, 628-636` | section 4 |
| `D_80120E74` (`drone_sched`) | `Row3[12]`, 3 s16 a row | how many drones `update_drone_models` updates per model iteration, until -1: rows 0-6 for 0-6 drones with one human, rows 7-11 for 0-4 drones with two or more (`world_physics_tick.c:371-373`: `D_8015274C`, + 7) | `func_800E56F8/group.c:4031-4035` | 7-11 drones with one human read the two-human rows; with two humans, rows 14-18 are past the table (the spike's UBSan: rows 12 and 17). Needs PC rows, or a rule |
| `D_8011176C` (circuit points by place) | `u32[6]` | the circuit's points table | `net_session_update.c:154`, `stat_race_start.c:84-85` | places 7-12 have no points (section 7) |
| `D_80111299` | s8, 55 B by the gap, zero in the ROM | the smoke colour choice, `[slot * 13 + car type]` | `entity_spawn_init.c:426-432` | 13 bytes a slot: slots 4-11 read past the gap |
| the HUD's MultiBlit tables (`D_80114C58`, `D_801158DC`) | the HUD's blits | 8 minimap dots and 8 radar icons per viewer | `showhud/showhud.c:126-128`; `func_80109A60.c`, `func_80108F40.c` | section 7 |

### 2.7 What must keep a window address

A per-car array can't simply move to PC storage when something stores its address as a 32-bit N64 address. The
spike moved the matrix pool `D_8015F740` out of the window to grow it, and every path faulted in its first race frame
in `particles_spawn_emitter` (`particles_spawn_emitter.c:3804-3822`: `D_80124FD0` holds each window's camera matrix as
a u32; `camera_smooth_lerp.c`'s 50 blast matrices from the same pool are s32 on the N64 and already pointers on the
PC, `camera_smooth_lerp.c:3813-3817`). The other arrays of 2.3 moved out of the window in the spike without a fault on
any path, which is not proof; the decoupling recipe's inventory step ([DECOUPLE.md](DECOUPLE.md) 1: `PC_N64W` stores,
u32 fields, a `RUSH_CHECK_CASTS` run) decides it for each.

## 3. The bounds

### 3.1 Per car: must follow the race's count

| Site | Bound | What | Fix |
|---|---|---|---|
| `init_state_continue.c:84-85, 91` (CalcPolePositions) | `6 - D_8014A108`, `slot < 6` | the drone cap and the roster fill | the setting's N |
| `func_800EC190.c:51-52, 65` | `D_801543CA = 6`, `i < 6`, `>= 6` | the attract demo's roster; the followed car's counter | N |
| `world_physics_tick.c:268` | `6 - i` | a human's starting `distance`, `-(6 - i)`, so humans start behind the 5 drones' `-1..-5` | N. With 11 drones a human's -6 sits among the drones' -1..-11 |
| `world_physics_tick.c:360` | `i < 6` | clears the slots after `D_801543CA` | `PC_MODEL_SLOTS` |
| `func_800EC914.c:117` | `i < MAX_LINKS` (6) | the same in munge | `PC_MODEL_SLOTS`, keeping line 114's owner test at 6 |
| `func_800E762C.c:69` | `i < 6` (IDO made it an end pointer) | the model clock of every record | N |
| `menu_audio_settings.c:35` | `other < PC_MODELS_END` (`&pc_models[6]`) | the car-to-car collision's walk | `&pc_models[N]` |
| `func_800E56F8/group.c:4065` (model_iteration) | `v != 6` | every car's speed | N |
| `players_race_update.c:3720` | `i != 6` (a 0x808 pointer walk) | per car | N |
| `update_game_data.c:353` | `i < 6` | copy the model's clock to the car | N |
| `func_800CFCA8.c:3831` | `var_v0 < 6` | per car | N |
| `func_800DE860.c:100, 107, 120` (set_catchup) | `j < 6`, `i < 6` | the drones' catch-up scales | N |
| `func_800EB90C.c:262` | `j < 6` | the drone cameras | N |
| `battle_mode_setup.c:62` | `i < 6` | dead reckoning of every car | N |
| `music_control.c:140, 146` | `i < 6` | StartEnvironment: each car's sounds, shadow, arrow | N |
| `menu_controller_remap.c:164` | `i < 6` | resets each car's stuck time | N |
| `audio_effect_remove.c:3808` | `i < 6` | clears the drone controls of 2.3 | N |
| `camera_smooth_lerp.c:3832` | `i < 6`, strides 0x3C and 0x190 | InitBlasts | N |
| `physics_collision_test.c:81` | `i < 6` | clears `D_801392D8` | N |
| `func_80105B74.c:91` | `i < 6` | clears `D_80142DB4` | N |
| `loadcars/root.c:68, 97` | `i < 6` | which roster slots load a car of their own; 6-12 copy slot 0's (section 5) | N |
| `func_8010C2E4.c:116, 118` | `1 << (idx + 24)` | a scene trigger's per-car bits, 24-31: 8 cars (UBSan: shift by 32 at 12) | a wider mask, or a PC per-car array |
| `func_800D1AB0`, `render_large_objects`, `func_80105B74` | locals | 2.5 | |
| `init_mdrive/group.c:109`, `resurrect.c:470` | `index + 6`, `polepos >= 6` | a respawn is coded as a pole position of 6 or more | section 4 |

Already following the count: the loops over `D_801543CA` (`world_physics_tick.c:254`, `func_800EC914.c:104`,
`car_checkpoints_func_800F8EC8.c:130`, the battle and shot loops in image B, `func_80109A60.c:166`) and over
`D_80152744` (26 loops: `CarSetup.c:3718`, `func_800D60AC.c:3810`, `func_800EC914.c:127`, `func_800FBE60.c:3820`,
`car_setup_confirm.c:88`, `func_800D1AB0.c:57`, `race_tail.c:137`, `demo.c:168, 175`, `maxpath_controls/group.c:479`,
`render_large_objects/group.c` (8), `slot18_alloc/group.c:72`, `countdown.c:329`, `menu_controller_remap.c:172`,
`players_frame_update.c:3813`, `world_gravity_apply.c:157`, `display_list_flush.c:309, 327, 367`; and the results'
rows `results_table.c:308, 391`, the place tests `func_800F56E0.c:172-176`), and the 13-slot walks
(`loadcars/root.c:67`, `func_8039156C.c:17`, `sfx_position_3d.c:84`). Their bodies are what 2.3 lists.

**Done** (item 3, the first part, 2026-10-29: da9d3a4 and ee4bdac on 8ff5be1). The sites of the table above that are
item 3's, the locals of 2.5 outside other items' files, car_setup_confirm's locals and the scene trigger follow the
race's cars. `PC_RACE_CARS` is `pc_race_cars()` (pc_game_types.h); each game file defines its N64 side as 6
(`#ifndef TARGET_PC`), so the N64 tokens are the original ones. With the count at 6 the game is the N64's, to the
trace (the proof below).

| Site | On the PC |
|---|---|
| CalcPolePositions, `update_game/init_state_continue.c:99-107` and its copy `input_aux/ctx_init_state_continue.c` | latches the setting first (`pc_race_cars_begin()`), except on the results screen (state 0x2000000: display_list_flush calls it again there for the cars that race on behind the results, which keep the race's count); the drone cap is `PC_RACE_CARS - humans`, the circuit's the N64's `6 - humans` (3.3: with 2 humans 4 drones); the fill walks `PC_MAX_LINKS` slots, so every slot after the race's cars is empty (owner 7, flags 0), whatever an earlier race of more cars left in it |
| func_800EC190, `func_800EC190.c:63-78` | latches; `D_801543CA` and the demo's drones are `PC_RACE_CARS`, and the camera's car (the counter) wraps at it; the loop walks `PC_MAX_LINKS` slots, so the slots after the demo's cars take the loop's own arm for slots past the count (flags 0, owner slot + 6), which the N64's 6 never reaches |
| init_cars, `world_physics_tick.c:284` | a human's first distance is minus the larger of 6 and `D_801543CA`, plus its slot: the N64's `-(6 - i)` in any race of 6 cars or fewer, whatever the setting; with 11 drones the human at -12, behind their -1 to -11. A choice: `PC_RACE_CARS - i` would put the one human of a 6-car race at -12 with the setting at 12, which the N64 doesn't |
| the same, `:376` | resets the records after `D_801543CA` up to `PC_MODEL_SLOTS` (the N64's to 6) |
| `func_800E762C.c:73`, `players_race_update.c:3729`, `update_game_data.c:362`, `func_800CFCA8.c:3840`, `func_800DE860.c:110, 117, 130`, `battle_mode_setup.c:71`, `music_control.c:149, 155`, `menu_controller_remap.c:175`, `loadcars/root.c:72, 101` | `PC_RACE_CARS`; LoadCars' slots from the race's cars to the 13 copy slot 0's |
| `PC_MODELS_END` (`menu_audio_settings.c:36`, the car-to-car collision) | `&pc_models[PC_RACE_CARS]`, the race's end; the name kept (pc_game_types.h) |
| `func_8010C2E4.c:154-159` | cars 0-7 keep bits 24-31 of the scene's flags, as on the N64; car 8 and up a PC field of its car record, `pc_car.scenes` (PC +0x464, in the record's alignment padding, so the record stays 0x468): bit n while it is in a scene whose trigger is collision index n (0-31, what the test can match). Both count when the trigger asks whether a car is in the scene already (the 0x200 it sets for the first); the records of the race are zeroed at its start (init_cars), as the scenes' bits are when their track loads |
| CalcPlaces, `car_checkpoints_func_800D1AB0.c:66-67` (and the demo_pair copy) | `place[]`, `dist_tab[]`: `PC_MAX_LINKS` |
| car_setup_confirm, `car_checkpoints_car_setup_confirm.c:99-100` (and the demo_pair copy) | `place[7]` and `scores[6]` are the finished cars in score order, by slot, and their scores, indexed by how many have finished: up to the race's cars, whatever the circuit holds. `PC_MAX_LINKS + 1` (the N64's 7 is its frame's) and `PC_MAX_LINKS` |

Left as they are, and why:

| Site | Why |
|---|---|
| track_texture_load's `i < 6` (`billboard_render/track_texture_load.c:187`, the high-score screen's ranks) | inside `gameplay_mode == 3`: the circuit's 6 (3.3) |
| SetOptions' `D_80142724 = 6` for the demo (`large_init_state/group.c:100`) | read only by CalcPolePositions and the track select's Drones line (`settings_values.c:185`); the demo runs neither |
| `func_800EC914.c:117`, model_iteration's `v != 6`, render_large_objects' locals, `display_list_flush.c`, `audio_effect_remove.c` | item 1b and 5 |
| `func_800EB90C.c:262` | items 1a, 1c, 1e (done: `PC_RACE_CARS`, DECOUPLE.md 6) |
| InitBlasts, `physics_collision_test.c:81` | item 1d |
| `init_mdrive/group.c:109`, `resurrect.c:470` | items 2 and 4 |
| `func_80105B74.c:91` and its local | items 6 and 7 |
| `ub612_src.tsv`'s 71 rows outside the other items' files | 62 are the car record views' `[6]` and `[8]` (battle_mode_setup, car_select_handler, cpak_read, update_game_data, world_physics_tick, menu_controller_remap, music_seq_load, music_tempo_set, func_800D1004, func_800D1AB0, the visuals' starters...): those views are each file's N64 side now, and the PC reads `pc_cars[12]` (DECOUPLE.md 4); 2 (`func_8008BEA4.c:40`, `startcar.c:115`, `D_8013FEF4[11]`) are declsize's extent, and the 12 palette descriptors fit (2.2); 6 (`func_8009D99C.c:57-84`, NaN to int) are a symptom, for the 12-car runs; 1 (`func_8008B640/group.c:3824`, `D_80161368[194]`) is the matrix pool run past its end (item 2) |

**Found on the way: `D_801439F8`** (MAT3 `carsetup_flag`, arena BSS, 4 bytes declared, 8 by the gap): one byte a car,
that CarSetup sets once it has set up a car's models (`music_tempo_set`: its root, body and wheel objects) during the
countdown (`CarSetup.c:3720`, `(u8 *)&D_801439F8 + slot`: m2c byte
arithmetic, which neither the text searches of 2.1 nor `-fsanitize=bounds` see), cleared for 6 at PrePlay
(`display_list_flush.c:374`). With 12 cars, slots 6 and 7 are the gap and are never cleared, and slots 8-11 write the
first word of `D_80143A00` (the pads' buttons pressed this frame, which the controller poll rewrites each frame): car 8
sets bit 0 of pad 0's on a little-endian host. It needs PC data of `PC_MAX_LINKS` bytes and the clear loop at the race's
cars; `display_list_flush.c` is item 1b's, so it waits for the merge (the second part). The same search over every game
file's byte arithmetic on a data symbol's address (27 sites) found no other per-car array.

**Proof** (the count at 6; "base" is a build of 8ff5be1):

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, base against the branch, Debug, build host (x86-64, GCC 14), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same, Pi 5 (aarch64, GCC 12) | 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 242 of 242 each (`assets_rom` skipped: no ROM there) |
| `data.c`, `data_symbols.txt`, the synthetic cartridge | the same files as the base's (build host and Pi; Debug and RelWithDebInfo the same) |
| `n64_neutral.py --base 09da2f0` | passes (1,010 files: 92 code-exact, 918 shifted); 19 changed game files under `src/` |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, ee4bdac) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, base and branch; offset_check 59,461 accesses, layout_check 48,102 (13 more each); endian_check's 2 open value conflicts in 1 storage the same, 3,486 puns, 2 more live functions (func_8010C2E4's two); check-loader 96 places, 0 wrong |
| `layout_check.py --storage pc_cars` | 1,599 accesses (4 more, the scene field), none off the reference (`struct pc_car[12]`, which the tool now gives an N64 size of 0x3BC: the field sits past the N64 record's 0x3B8, in the PC record's padding) |
| `view_audit.py` on `pc_cars`, `pc_models` | 223 views (func_8010C2E4's one more), 0 disagree; 165, 0 |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`; two reviewed keys GCC 14 found on the base it doesn't now (`pc_links` *tmr* in update_game_data.c and music_control.c: their loops' bound is a call now, so the loop optimizer doesn't index the array), kept for the other compilers: 9 reviewed keys GCC 14 doesn't find, not 7. GCC 13 and mingw-w64 12 not run on it |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; gen_data's check of the PE: OK, 3 decoupled |
| `assets_roster`, `assets_models`, `assets_carrecs`, `assets_targets` (test_race_cars 12) | pass. With each change undone, checks fail: the drone cap at 6, 190; the circuit at the setting, 6; the roster filled to the race's cars only, 20; the demo at 6, 93; its camera wrapping at 6, 2; a human's distance at -(6 - i), 4, at -(PC_RACE_CARS - i), 2; init_cars' reset to 6, 46; LoadCars' drones to 6, 33; the model clock to 6, 12; the collision's end at 6, 3; the stuck times to 6, 12; set_catchup's first loop and its leader's to 6, 6 and 1; the scene's cars 0-7 only, 1; car 8 at the shift's low 5 bits, 5. The locals at 6 fail none: an overrun of a local isn't seen without ASan (next row) |
| the same tests and `assets_cars` under ASan + UBSan (Pi 5, GCC 12) | no report. With car_setup_confirm's `scores[6]` or `place[7]`, or CalcPlaces' locals at 6, ASan stops at a stack-buffer-overflow in the function (`car_checkpoints_car_setup_confirm.c:110`, `:109`; `car_checkpoints_func_800D1AB0.c:76`, `place`) |

**The tests.** `assets_roster` now also builds init_cars (world_physics_tick) and LoadCars: the demo's 6 and 12 cars and
its camera's car round them; CalcPolePositions for 1 or 2 humans and 10 or 11 drones, the option above the cap, the
default 5 drones at the setting 12 (6 cars), battle and stunt, the circuit's first race (6 cars, places drawn) and a
later one (the standings'), and 6 cars after 12; munge counting 12; the first distances for 6, 8 and 12 cars and the
records reset after them; LoadCars giving 11 drones cars of their own. `assets_models`: the clock of 12 records and the
collision's walk to the 12th. `assets_carrecs`: the stuck times of 12, 12 cars finishing one by one (car_setup_confirm,
then CalcPlaces for the rest), CalcPlaces alone over 12, set_catchup over 12. `assets_targets`: the scene trigger with
cars 7, 8 and 11 of 12, and a race of 6 with a car 8 left from one of 12.

**For the other items.** munge's reset (`func_800EC914.c:117`) must reach `PC_MODEL_SLOTS`: LoadCars reads the drone
type of every record below the race's cars, and a 6-car race after a 12-car one with the setting at 12 would otherwise
load cars for records 6-11 that an earlier race left as drones (only heap; the race doesn't walk them). The results
screen's CalcPolePositions doesn't latch (above), which item 7's `pc_race_cars_begin()` can rely on. Item 4's grid: the
humans' first distance is `-(max(6, cars) - slot)`. `D_801439F8` above.

### 3.2 Values that look like a car count but are not

The owner codes 6 and 7 (1.4); the six race tracks (`D_8014978C < 6`, `func_800CD8EC`, `reverb_setup`, `SavePlayerOption`,
the save file's six track records `menu_save_options/group.c:251`); `% 6` on clocks (`func_800E5D64.c:88`, the timers'
minutes digits); `render_post_process.c:35` (clears `D_80110680`'s 6: the spike's `SPIKE_N` there was wrong, UBSan
named it); the name entry's 6 characters (`menu_save_options/group.c:408`); `hud_setup.c:182` (6 sound events); the
unlocks per player (`highscore.c:127, 181`, `D_80150F40[p][6]`); `func_800B78F0.c:24`, `func_800F7A98.c:16` (modes);
the battle and stunt `type >= 6` of `entity_spawn_init.c`; `StartTire`'s `tire + 6` (a visual index); `demo.c:192`
(the demo's track).

### 3.3 The circuit's sixes

The circuit is built for 6 cars throughout: `net_session_update.c:85` (`cnt = 6 - active_player_count`, the drones),
`:143` (`m = 6 - (players == 1 ? 0 : 2)`), `:163-178` (6 standings), `func_800F45F8.c:62, 84`, `stat_race_start.c:72-96`
(six values, ranks, the points table), the standings records `D_80154450` (6 × 76 B, 0x80154450-0x80154618: the
tables split it at the names inside, `D_80154451` and `D_80154484`), the per-profile circuit records `D_80155148`
(`u16[][6]`), `D_80155140`, `D_80154FD0`, and the points table `D_8011176C[6]` (2.6). A 12-car circuit is a project
of its own (section 10).

## 4. The start grid

**How a car gets its place.** `CalcPolePositions` gives each roster slot a place: drones 0, 1, 2... and humans after
them (`init_state_continue.c:100-103`: a human's place is the drone count plus its index), random places in a circuit's
first race (`:105-116`), the circuit's standings after it (`:92-99`). `world_physics_tick` copies it to the car record
(+0xEE). At PrePlay `func_800E543C` (`Init_MDrive`) calls `multiinit_n64` (`init_mdrive/group.c:100-110`): the start
sets `initin.polepos` to the roster's place (0 in a ghost race), a respawn to `index + 6`.
`SetResurrectionData` (`func_800C4F68`, `resurrect/camera_pair.c:160-210`) puts every car on one point: the
resurrection path's point of the checkpoint the car is at (`gResPath.points[checkpoint.pathPts[0]]`, checkpoint 0 at
the start), facing the path (in battle and stunt each player's own start record instead). `Resurrect`
(`stunt_combo_display`, `resurrect.c:465-490`) then offsets it by `pole_pos_offset[polepos]` unless the mode is
battle, stunt or obstacle, or the car is a drone with `polepos >= 6`. The grid is the table, not the track: one start
point per track, six offsets from it.

**The table.** `D_801210E8`, `f32[12][3]`, game data. Rows 0-5 are the grid: in the retail cartridge, two columns
staggered over three rows, a few car lengths deep. Rows 6-9 are where the four humans respawn
(`resurrect.c:628-636`, `pole_pos_offset[slot + 6]`, row 6 in a ghost race): offsets along one line across the road,
none behind. Rows 10 and 11 are read by nothing. (The GameCube's copy of the table has the same twelve rows; its
comment says rows 6-11 are never read, which the N64's respawn makes untrue.)

**Places 6-11.** A drone at place 6 or later gets no offset at all: it starts on the pole car's spot. A human at place
6-11 (a human's place is drones + index, so with 11 drones the one human is place 11) gets a respawn row's offset, on
the start line beside the front row. The retail spike shows exactly that: at the start of a 12-car race the player's
car sits at the front of the grid between two drones, the five drones of places 6-10 stacked on the pole car and pushed
apart by the collision; in the race UBSan reports a car's engine speed of about -33,600 rpm, past an s16
(`sym_controls/group.c:197`), which no 6-car run gives.

**What places 6-11 need.**
- Six more grid rows: two columns staggered as rows 0-5 are, continuing backwards (rows 3-5 of the grid in the same
  spacing). A PC table, `pc_pole_offsets[PC_MAX_LINKS][3]`, whose first six rows are the ROM's (read from
  `D_801210E8` at startup, so nothing is copied into the build) and whose rows 6-11 are computed from rows 4 and 5
  (the last row's columns, one row spacing further back each two places).
- A respawn coding that isn't a place: `multiinit_n64`'s `index + 6` and `Resurrect`'s `polepos >= 6` become
  `index + PC_RESPAWN_BASE` and `polepos >= PC_RESPAWN_BASE` with the base 12 (or a flag), and the respawn rows stay
  rows 6-9 of the ROM's table.
- A check that the grid fits the road: rows further back from the start point may leave a track's road where the
  start is in a curve. Untested beyond track 0 (section 11).
- `world_physics_tick.c:268`'s `6 - i` (section 3.1), which orders the cars' first `distance`, and therefore the first
  frame's places, before the cars cross a checkpoint.

## 5. Car loading and memory

**LoadCars** (`loadcars/root.c:57-116`) walks the 13 roster slots. Slots below 6 that are drones (or every slot in the
demo) get a random car type (0-12) that no earlier slot has (`again:` loop, lines 69-73), the type's three colours,
and load the type's car WAD (wave `88 + type`, `audio_frame_sync` = LoadWave) into the heap if the heap has its size
plus 512 bytes free (`audio_output_setup(0)`, the heap's free bytes); otherwise they look for a type whose WAD is already
loaded, then any type that fits, then reuse a loaded car of an earlier slot (lines 78-91). Human slots below 6 load
their chosen type, or fall back to any type that fits, or slot 0's (lines 97-111). Slots 6-12 copy slot 0's car and
wave (lines 112-114). `D_80142B08[13]` keeps each slot's wave handle; `func_800BB02C` sets up the slot's palette
descriptor (`D_8013FEE0`, 512 bytes of heap for the paint, `func_800BB02C.c:70`). Two slots of the same type share
the one WAD. With 13 types, 12 cars can all differ.

**What each car loads.** Its car WAD (models, textures, palettes, from the file table's waves 88-100) once per type,
512 bytes of paint per slot, and at track load per-car scene nodes and matrices (section 8). Sounds are the shared
banks; the car's engine sounds are table entries (`D_8010FD80[type]`).

**Measured** (the spike's `LoadCars` note, retail cartridge, Pi, Expansion Pak, track 0, one player): with 12 cars every
slot loaded a type of its own (12 waves), and the heap's free space after LoadCars and at the race's start fell by
less than a tenth from 6 to 12 cars, leaving megabytes free; two players, the same. So the 8 MB RDRAM holds 12 cars with room
to spare; without the Expansion Pak the game's own fallback (sharing a loaded type) would apply, untested.

**For N cars**: `i < 6` at lines 68 and 97 becomes `i < N`; slots N-12 keep copying slot 0's. Nothing else in loading
depends on 6.

## 6. Audio

**Per car.** StartCarSounds (`func_800D5E64`, `d5e64.c:85-118`) starts two engine layers (`D_8010FD80[type]`'s two
items) per car: for a human through `high_scores_display` (non-positional), for a drone through `camera_target_track`
(a positional emitter at the car's position); each frame `func_800E0050` (`mode.c:207-292`) sets their pitch and volume and
sometimes starts a third, random sound (`rec[3]`); `func_800E05F0` (`mode.c:314-392`) adds the screech
(`D_80140640`) and, for humans, gear and horn sounds (`D_801407E0`, `D_801407C0`). Each car's engine is updated
every other frame, alternating by `D_8010FFCC`. `sfx_position_3d`'s 13 slots are the car parts, not sounds
(section 2.2); `D_80142B08[13]` is each slot's car WAD.

**Limits.** The sound manager is set up by `hud_setup(32, 16, 16, 5, 64, ...)` (`initsystem.c:188`,
`state/state.c:149`): 64 positional emitters (`hud_speed_display.c:84-100`, the free list `D_801461B0`), 128 messages
(`D_80142DD8`), and without the Expansion Pak `hud_setup(8, 8, 0, 0, 0, ...)` (`state/state.c:210, 273`): no
positional emitters at all, so the drones' engines are silent there. MusyX has 32 voices (`D_8004BEB8[32]`, [MUSYX_DATA.md](MUSYX_DATA.md) 8.3) and
allocates them by priority and per-sound voice limits (the sfx table's "max voices"). 12 cars ask for about 24 engine
voices plus screeches and the random sounds, against 32 with the music and the effects.

**More engines than voices.** Nothing breaks: an emitter request with none free returns -1 and every caller tests for
it (`camera_target_track/group.c:137-145`; the PC's `PC_OBJ_OR_UNLOCK_CAM` gives the emitter back when the message pool
is empty), and MusyX takes a voice from a lower priority sound or doesn't start the new one. The cost is which engines
are heard: likely the nearest and loudest, by MusyX's priority, not tested by ear (the spike's runs used `--audio
null`; the MusyX code ran, no fault, no sanitizer report). A PC-side choice is possible later: start the drones'
engines only for the N nearest cars to each listener.

## 7. Front end and HUD

| Element | Code | 6 cars | N > 6 |
|---|---|---|---|
| the place, "P/N" at the top right | `animPlace` (`credits_scroll_grp/gr3_7af4.c:87-140`): `str[0] = place + '1'`, `str[2] = D_801543CA + '0'`, one character each | "6/6" | 10-12 are the characters `:`, `;`, `<`: the font has no glyph for them. Seen: with 12 cars the place disappears in one player and shows a bare "/" in two. Fix: draw two digits (a PC version of the function's string, the rest as it is) |
| the finishing place on the results | `drawRaceStandings` (`results_table/results_table.c:287`): `strings[indices->cars + place]` | the text bank's six ordinals, first to sixth | the entries after the sixth hold an ordinal suffix alone (read on the retail cartridge with gdb): place 7 would show a suffix with no number. Fix: compose the number and the bank's suffix entry (read in the bank's first section; the others not looked at), or PC strings |
| the race standings' rows | `drawRaceStandings`: `rows = active_player_count + 1` (line 213) | the players only | the same: unaffected |
| stunt and battle standings | `func_80101904`, `func_80101D84`: `rows = D_80152744 + 1` (lines 308, 391) | | no drones in those modes |
| the minimap's dots | `func_80109A60.c`: a dot blit per entry of the HUD order `D_80142DB4` (`id & 0xf`), the order from `func_80105B74` (humans last, at the top entries) | the HUD has 8 dots (blits with AnimID 0-7, counted on the retail cartridge with gdb) | entries 8-11 have no dot; since the humans take the last entries, a 12-car race shows the drones' dots and not the player's (seen). Fix: 4 more dot blits. The HUD's blits come from the MultiBlit tables ShowHUD creates (`showhud/showhud.c:126-128`: `D_80114C58`, 89 entries, and `D_801158DC`, 8), ROM data: the PC adds its entries after them |
| the radar | `func_80108F40.c` (AnimateTraffic): a blit per viewer and car slot (AnimID bits 8-11 viewer, 0-3 slot) | 8 per viewer in the HUD set (counted the same way) | slots 8-11 have none. Same fix |
| the wrong-way, lap and timer texts | `gr3_*.c` | per window | unaffected |
| the camera's car cycling | the attract demo follows car `D_801163F4`, wrapping at 6 (`func_800EC190.c:64-69`); the ghost mode's "bump target" wraps at `D_801543CA` (`func_800EB90C.c:172-183`) | | the demo wraps at N |
| the Drones option | 1.3 | 0-5 | 0 to N-1 |
| the circuit's points | `D_8011176C[6]`, `net_session_update.c:154` (`b28[j] = points[place]`) | 6 values | the circuit stays 6 cars (3.3) |
| "n of 6" | `animPlace` above; `func_800F45F8.c:62` and `net_session_update.c:143` (the circuit) | | the circuit only |

## 8. Rendering and per-frame buffers

The car draw loops walk the active cars (`D_80152744`) or the scene's node list; nothing in drawing has a car count of
its own. The pools do: each was measured with the spike's high-water marks at "state reached" (retail cartridge, track
0, the `drive` and `race2p` scripts; synthetic cartridge, the same scripts).

| Pool | Size | 6 cars | 12 cars | Overflow detected? |
|---|---|---|---|---|
| the persistent matrices (`func_800A7D6C` = GetMatrix: scene nodes and blasts take one each, `D_8015F740`, 48 B) | 0x8015F740-0x80161360: 150 (index 0 unused). The tables give `D_8015F740` 2,240 B because the image tables name addresses inside it (`gAudioChannelState` 0x80160000, `gTorqueCurveStandard` 0x80160800 ...) that no C uses; a 6-car race's matrices run over them | 130 (retail), 127 (synthetic) | 202 (8 cars: 154) | no: GetMatrix has no limit (`func_800A7D6C.c:3812-3825`); past 149 it writes the options and the sprite count after `D_80161360`, which crashed the synthetic 12-car race. About 12 matrices a car. Must stay in the window (2.7) |
| the frame's matrices (`D_8015B260`, from +1,728 of the frame buffer to its display list at +0x9CC0, 64 B) | 600 | 206 (one view), 418 (two) | 240, 486 | no |
| the frame's display list (`glist`, `Gfx[4000]`) | 4,000 commands; on the PC 16 B each in `pc_frame_dl` (`initgraphics/initgfx.c:53`) | 1,767 (one view), 3,485 (two) | 1,915, **4,093** | no: the SDK's `glistp > &dynamicp->glist[GLIST_LEN]` check is compiled out (`world_collision_response.c:128`, an empty `if`). A two-player 12-car race on track 0 writes past the frame buffer; `pc_frame_dl` is game C data, which ASan doesn't guard (SANITIZE.md 1.3) |
| sprites (`D_80140BF0[200]`, `func_800A79F4`) | 200 | 102 | 102 | yes: the allocator returns -1 at 200 |
| scene nodes (`D_8012E700`, 600), environment visuals (`D_80138880[100]`), skid segments (`D_80155B30[100]`, steals the farthest) | | not measured | not measured | pools with free lists; a car adds its visuals at track load |

The PC owns the frame buffer (`pc_frame_dl`), so its display list can grow with no N64 layout to keep: the glist
offset (+0x9CC0) and `GLIST_LEN` are in the `Dynamic` views (`world_collision_response.c:94`, `particle_context.c:540`)
and the frame's matrix region ends where the glist starts. The persistent matrix pool needs more room in the window:
the 12 KB the decoupled model records left free at 0x8014A250-0x8014D280 ([DECOUPLE.md](DECOUPLE.md) 3) holds 257
more matrices, which is what the spike used.

## 9. The spike

**What it is.** `RUSH_SPIKE_CARS` (1-12, default 6) read by a port function `pc_spike_cars()`; `PC_MAX_LINKS` 12;
CalcPolePositions and `func_800EC190` fill N slots (drones = N - humans whenever the mode asks for drones);
`gen_data.py` grows the arrays listed in `port/data/spike_grow.txt` (23 of them) by a factor, moving them into own
storage; the per-car loops of 3.1 follow N; `update_drone_models` updates every drone each iteration above 6 cars; the
locals of 2.5 take `PC_MAX_LINKS`; GetMatrix spills past the N64 pool into the model records' freed window range;
diagnostics print high-water marks and LoadCars' result. Debug, `-fsanitize=bounds` and the rest of
`RUSH_SANITIZE_GAME_UB`: UBSan on the build host (x86-64, GCC 14) for the synthetic paths, ASan + UBSan on the Pi
(aarch64, GCC 12) for the retail cartridge.

**What broke, in order** (each round patched the first breakage crudely and ran again):

| Round | Change | 8 cars | 12 cars | Found by |
|---|---|---|---|---|
| 1 | the caps only (CalcPolePositions, the demo's roster, `PC_MAX_LINKS`, `player_array` 12) | segfault at PrePlay in `SetResurrectionData` (`camera_pair.c:203`): `gResPath`'s point pointer was a float. A hardware watchpoint on it: StartCarSounds for car 6, the 7th (`d5e64.c:97`), wrote its `D_80140420` record over the own storage after the array, `D_801406B8` and `gResPath` | | gdb |
| 2 | grow the engine sound arrays | the race runs 600 frames; UBSan: CalcPlaces' locals and DoDrones' locals at index 6 | segfault in the model thread (`camera_clip_planes` ← the engine update of model 255): `D_80153F88[6]` (the 7th drone's heading) overwrote `num_humans`, and the human loop read input record 255's car | UBSan, gdb |
| 3 | the per-car loops of 3.1; the lists, maxpath and checkpoint arrays grown; the drone schedule bypassed | | segfault drawing sprite 201: GetMatrix had run past its pool into the sprite count `D_801613AC` (a hardware watchpoint: `func_8008B640` ← a car part's matrix); the demo faulted in the blasts (`D_80154660` index 11) | UBSan, gdb |
| 4 | the matrix pool and the blasts, tire effects grown | | every path faulted in its first race frame in `particles_spawn_emitter`: the matrix pool had left the window (2.7) | the backtrace |
| 5 | GetMatrix spills into 0x8014A250 | all 19 synthetic paths reach their states with 12 cars (the circuit, the demo, pause and its retirement, the results and back to the menu, 2-4 players) | the same | synth_paths |
| 6 (retail) | | | ASan: stack-buffer-overflow in `func_80105B74` (`order[8]`, called as a blit callback from the 2D draw) | ASan |
| 7 (retail) | `order` sized, blast rings and the resurrect midpoint grown | segfault in the skid marks (`func_8008C074` ← `func_800AF8C0` ← `save_validate` ← `cpak_init`): `D_80155290` (6 cars × 4 tires) ran into the skid segment pool | the same | ASan (SEGV) |
| 8 (retail) | skids grown | a race with gas and steering for 20 s: no fault, no new sanitizer report | the same; also the demo, two players, the circuit, pause and retirement through the results and the game over screen, and the race's end back to the main menu (each reaches its state) | |

After round 8 the retail runs' UBSan reports at 12 cars that 6 cars don't give are three: `func_8010C2E4`'s shift
by 32 in the demo (3.1), the engine speed cast (section 4), and the spike's own wrong loop in `render_post_process.c`
(3.2); the rest of what they print is the same at 6 (`route_cross/group.c:164`, `animControls.c:154`). In the synthetic
runs of the bounds-declared tree, every per-car access past the N64's extents is named, since that tree declares the
N64 extents of the arrays the spike grew: the sites 12 cars reach and 6 don't are in `build/morecars-spike-patches/ub612_src.tsv`,
8 against 6 in `ub68_src.tsv` there (251 and 194 rows; most are the car record views' own `[6]` and `[8]` declarations, which
makes them a list of every function that touches a car above 5).

**What the screen shows** (retail, one player, track 0, local screenshots):
- the grid: the player's car at the front between two drones (section 4), instead of at the back of the field;
- the place: missing at the top right (6 cars show "6/6"); a bare "/" in each view with two players;
- the minimap: the drones' dots, not the player's (section 7);
- the rest of the HUD, the track and the cars drawn as with 6.

**Not tried in the spike:** a race to its finish line with 12 cars (the results' place text, 7), the circuit beyond its
first race, ghosts, without the Expansion Pak, tracks other than 0, sound through a device, Windows.

## 10. The plan

### 10.1 The setting

- `[game] cars`: `original` (default: the N64's 6, exactly) or 6-12. `--set game.cars=12` and the F1 menu's Game page
  ("Cars in a race: original, 7 ... 12"), applied at the next race's start (`CalcPolePositions`), never mid-race.
  The game reads it through one port function (`pc_race_cars()`, like the spike's), so the game C gets no ini code.
- **The Drones option** stays the player's choice: its range becomes 0 to cars - 1 (the menu's wrap at 5 and the
  N64's cap `6 - humans` become cars - 1 and cars - humans); its reset value stays 5 and its default stays 5, so a
  player who never touches it races 6 cars whatever the setting. A second preset could set the default to the
  maximum; not needed for the first version.
- **Per mode.** Single race: humans + drones, up to `cars`; with 3-4 players the N64 gives no drones and the retail
  mode screen offers no race anyway: unchanged. Circuit: stays 6 (3.3); a 12-car circuit is a later project (its
  standings records, points table, per-profile records and save format). Practice, ghost, stunt, obstacle, battle: no
  drones, unchanged. The attract demo: `cars` (it is the cheapest test of N cars and shows the setting).
- **Saves.** The option byte is per profile and per track already; values above 5 only appear when the player chose
  them.

### 10.2 The work items

In order; each with its proof. "The default" means `cars = original`: every synthetic path's trace must be the same
(`synth_compare.py --same`) as main's, the retail cartridge's six scripted paths too (as [DECOUPLE.md](DECOUPLE.md)'s
proofs), plus the five checks and the IDO check of every changed game file.

| # | Item | Files (overlap) | Proof |
|---|---|---|---|
| 0 | `pc_cars[12]` and `PC_MAX_LINKS` 12 (in progress) | the car record's ~134 files | DECOUPLE.md's |
| 1 | The per-car arrays of 2.3 to PC data, `PC_MAX_LINKS` long, by subsystem (each a DECOUPLE.md item, each with its inventory's window-address check): **1a** sound (`D_80140420`, `D_80140640`, `D_8010FFC4`, `D_8010FFCC`): done ([DECOUPLE.md](DECOUPLE.md) 6) | `mode_select/d5e64.c`, `mode_select/mode.c`, `func_800D5524.c` | the default; a test running StartCarSounds/StopCarSounds and the engine update on 12 records (`assets_percar`) |
| | **1b** drones (`D_801527D8`, `D_80152808`, `D_80153F28`, `D_80153F48`, `D_80153F68`, `D_80153F88`, `D_80154138`) | `func_800EC914.c`, `maxpath_controls/group.c`, `func_800E56F8/group.c`, `display_list_flush.c`, `audio_effect_remove.c` | the default; `assets_roster` with 12 slots |
| | **1c** checkpoints (`D_80152218`, `D_801527E8`): done (DECOUPLE.md 6; CalcPlaces' locals: item 3, 3.1) | `car_checkpoints_func_800F8EC8.c` (and its `demo_pair` copy) | the default |
| | **1d** effects (`D_801392D8`, `D_8013A020`, `D_8013C098`, `D_80154660`, `D_80154FD8`, `D_80155290`, `D_80161450`) | `entity_spawn_init.c`, `func_8008E408.c`, `hud_render.c`, `physics_collision_test.c`, `func_80092484.c`, `func_800CB718.c`, `blast.c`, `entity_update_callback.c`, `entity_collision_detect.c`, `camera_smooth_lerp.c`, `skid_marks/group.c`, `func_800B0580.c` (two), `track_select_handler.c` | the default; the effects with 12 cars in `assets_cars`-style tests |
| | **1e** cameras (`D_80110680`, `D_80110668`: ROM data, filled from the image, `init=image`; the drone cameras' `j < 6`): done (DECOUPLE.md 6) | `render_post_process.c`, `func_800E9E2C.c`, `race_countdown_display.c`, `func_800EB90C.c` | the default |
| 2 | The pools: GetMatrix's pool to 150 + 12 a car in the window (the model records' freed range, a gen_data reservation); `pc_frame_dl`'s glist and the frame's matrix region sized for 12 cars and two views, with a PC check that stops (in Debug) or logs (in Release) when either runs past its end | `func_800A7D6C.c`, `initgraphics/initgfx.c`, `world_collision_response.c`, the `Dynamic` views | the default; high-water marks of 12-car runs under a test; the checks themselves fail a test that shrinks the pools |
| 3 | The count: CalcPolePositions, `func_800EC190`, LoadCars and every loop of 3.1 follow `pc_race_cars()`; the locals of 2.5; `PC_MODELS_END` → the race's end; `world_physics_tick`'s `6 - i`; `func_8010C2E4`'s per-car bits | the files of 3.1 (many overlap with 1: do 3 after 1) | the default (the count is 6); `assets_roster`/`assets_models` at 12; the synthetic paths with `cars=12` |
| 4 | The start grid: `pc_pole_offsets` (rows 0-5 the ROM's, read at startup), the respawn coding (`index + 12`) | `init_mdrive/group.c`, `resurrect/resurrect.c` | the default; a test placing 12 cars (positions on the computed rows, no two within a car's length) |
| 5 | The drone schedule for 7-11 drones (PC rows: with one human, the N64's pattern of at most two groups; with two, the same) | `func_800E56F8/group.c`, `world_physics_tick.c` | the default; a test stepping `update_drone_models` for 7-11 drones (each drone updated once per round) |
| 6 | The HUD: two-digit place, ordinals 7-12 (number + the bank's suffix), 4 more minimap dots and radar icons per viewer | `credits_scroll_grp/gr3_7af4.c`, `results_table/results_table.c`, `showhud/showhud.c` and a PC table of the added blits | the default; screenshots of 12-car races in the synthetic paths' checks (`synth_shot.py`: the place and the dots drawn) |
| 7 | The setting: `[game] cars`, the F1 menu, `pc_race_cars()`, the Drones option's range | `port/config/config.c`, the F1 menu, `runtime_a_c93c_closure/group.c` | config tests; the menu's wrap at the setting |
| 8 | Tests (10.3) and the doc's proof table | `tools/port/synth/*.press`, ctest | |

**What can run in parallel.** 1a-1e touch disjoint files (1c's `car_checkpoints` group and 1b's are different files)
and can run at once after item 0; 2 and 4 and 5 are disjoint from each other and from 1 (5 shares
`func_800E56F8/group.c` with 1b: put 5 after 1b, or in the same hands). 3 touches most of the files of 1 and 2 (the
loops sit next to the arrays): after them. 6 and 7 are disjoint from everything else and can start at once. Every
item but 7 keeps the default exactly, so each merges on its own.

### 10.3 Testing N > 6

- **Synthetic paths with N cars:** `race12`, `demo12`, `race2p12`, `circuit` (stays 6), `pause12`, `raceend12`, each
  a copy of its path with `# args: --set game.cars=12` and `# expect: ... cars 12`; `drive12` with a screenshot check
  of the place and the dots. They run in every CI job that runs ctest.
- **The sanitizers:** the CI's UBSan job (x86-64) runs the 12-car paths too; on aarch64 an ASan + UBSan build runs
  them locally (ASan can't run on x86-64, SANITIZE.md 1.1). The spike's `declsize.py` becomes a check: with the
  per-car arrays decoupled, their PC declarations have the true count, so `-fsanitize=bounds` names an index past
  `PC_MAX_LINKS`; for what stays in the arena, `declsize.py` run in the sanitizer job gives every open-ended arena
  declaration its extent, and a 12-car run must report nothing a 6-car run doesn't.
- **The pools:** the high-water checks of item 2 as test assertions over the 12-car paths (matrix pool, frame
  matrices, glist below their ends).
- **The default stays the N64's:** `synth_compare.py --same` against main on every path at `cars = original`, on both
  hosts, and the retail scripts on the Pi; the IDO check for the changed game files.

## 11. What was not checked

- Tracks other than track 0 with 12 cars (the grid's rows on a curved start, the frame matrices and the glist on
  denser tracks); a race to its finish with 12 cars; the circuit past its first race; ghosts; the obstacle mode.
- Without the Expansion Pak (no positional emitters; LoadCars' sharing).
- Sound through a device: which engines are heard.
- The window-address question for each array of 2.3 beyond what the spike's runs exercised.
- Windows and the optimized build with N > 6 (OPTIMIZED.md 2.7's class applies to every new declaration).
- The text bank's other language sections (the ordinal and suffix strings were read in the first).
- `bounds_check.py` and `view_audit.py`: they check declarations and views, which the fixes of section 10 add; run
  them on each item, as [DECOUPLE.md](DECOUPLE.md)'s proofs do.
