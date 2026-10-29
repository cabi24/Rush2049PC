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

**Item 1d, the effects (done: [DECOUPLE.md](DECOUPLE.md) 6).** The seven effects arrays of 2.3 are PC data with
`PC_MAX_LINKS` entries; none needed a window address, and their two loops of 3.1 (`physics_collision_test.c:81`,
InitBlasts `camera_smooth_lerp.c:3832`) are resets that clear every slot on the PC. `D_80111299` needs nothing: it
is rows 1-4 of `D_8011128C` (`s8[5][13]`), a setting per player and car type that image A's car screen reads as
`[player + 1][car]`, and only human slots (0-3) reach its three readers (the wings' set-up returns for a drone, and
the trails' colour comes from the wing's animator with one player), with any number of cars. InitBlasts takes its
50 matrices whatever the cars (StartBlast's ring of 50, not per car).

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
| `func_800EB90C.c:262` | items 1a, 1c, 1e (done: `PC_RACE_CARS`, DECOUPLE.md 7) |
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

### 4.1 Done: the grid for 12 places (item 4, 2026-10-29)

| Piece | Where |
|---|---|
| The rows | `pc_pole_row(place)`, `pc_pole_offsets[PC_MAX_LINKS][3]`: `src/blob/groups/resurrect/resurrect.c` (PC only, above Resurrect) |
| Where a place from 6 on starts | `pc_grid_start` (its point, orientation and offset; the same file), read by Resurrect's start branch through `PC_START_POS`, `PC_START_UVS`, `PC_POLE_ROW` |
| The respawn coding | `PC_RESPAWN_BASE` (`port/include/pc_game_types.h`: `PC_MAX_LINKS`, 12); `init_mdrive/group.c` (multiinit), `resurrect.c` (Resurrect's start branch) |
| The test | `assets_grid` (`port/assets/tests/test_grid.c`) |

**The rows.** `pc_pole_row(place)`: places 0-5 are the loaded table's rows (`D_801210E8`, read each time a car is
placed, so the build holds none of its values); each place from 6 on continues the places of its column parity two
grid rows before it by the table's own two-row step:

    row[p] = row[p - 4] + (row[4 + p % 2] - row[p % 2])        p = 6 .. PC_MAX_LINKS - 1

The table's grid, as the retail cartridge has it: three rows of two cars side by side, the middle row shifted sideways
by half the cars' spacing. The rule keeps that alternation and the row spacing: places 6-7 sit as places 2-3 do, two
rows further back, places 8-9 as 4-5, places 10-11 as 6-7; the twelfth place is five row spacings behind the pole
where the sixth is two. It is derived from the loaded rows only (a table of another shape gives its own continuation:
`assets_grid` checks two). Places 0-5 are copied bit for bit.

**Where they stand.** Resurrect's start branch puts a car at its start point (SetResurrectionData's: the point of the
resurrection path of the start checkpoint, facing along the path), offset by its row. For places 0-5 that is the
N64's, unchanged. A place from 6 on (`pc_grid_start`) takes, in this order, the first candidates with road under them
(RoadPosition, `camera_trigger_check`, at the candidate's world position):

1. the rows continued back, places 6 to 11 in turn: each from the point of the resurrection path its row's distance
   behind the start point, walked back point by point (and across the loop point to the path's last), facing along the
   path there (`camera_follow_path`, as the start point is faced), with its row's sideways offset; where the path has
   no point behind the start, the rest of the distance straight back;
2. a column outside the table's grid on each side, beside each of its three rows (left, then right; front row first),
   at the row's own sideways spacing, from the start point;

and its own row of the first kind if too few have road. On a straight road the first kind is the straight row; where
the road behind the start bends or dips, the rows follow it. Resurrect asks seven times for one car's start (three
coordinates of the point, the orientation, three of the offset): the answer is kept for the record, its place and its
start point.

**On the road** (the retail cartridge, Pi, local runs with the spike's caps, one and two players, 12 cars; logs and
screenshots kept under `build/`): RoadPosition found road under every car the grid placed, on all six tracks, and the player's car at place 11 kept its height through the countdown and
drove off (the human cars are the ones to watch: in the local test branch's 12-car races cars 6-11 sink through the
road from the first frame whatever their place, a per-car state other items still own, so their positions were
checked at placement, not by watching them). Track by track: on tracks 0-3 the path behind the start is straight and
the continued rows stand where straight rows would; on track 4 the road behind the start slopes down, and the rows
follow it (straight rows stood up to 7 ft above the road at place 11 and dropped onto it); on track 5 the
resurrection path begins at the start (the lap comes back to a later point of it) and the road ends about 75 ft behind
the pole, so places 6 and 7 take the continued row, 60 ft back, and places 8-11 the places beside the grid's first two
rows, left and right of each, which have road: the screenshot shows the player inside the start tunnel with cars beside
the front rows. With straight rows only, places 8 and 9 on track 5 had no road under them by RoadPosition (a car put
there held, on something else), and places 10 and 11 none at all: both fell through the world at the start and came
back as respawns about 5 s later; with the rows following the path alone, places 8-11 had no road under them.

**The respawn coding.** multiinit_n64 (`init_mdrive/group.c`) codes a respawn as `index + PC_RESPAWN_BASE`
(`PC_RESPAWN_BASE` in a ghost race), where the N64 has `index + 6` (6); Resurrect's start branch gives no offset to a
drone with `polepos >= PC_RESPAWN_BASE`, and a human's respawn reads the table's row `polepos - PC_RESPAWN_BASE + 6`,
the humans' rows 6-9 (row 6 in a ghost race), as the N64's `index + 6` did. The N64 side of each file defines
`PC_RESPAWN_BASE` as 6, `PC_START_POS(m)` as `m->resurrect.save_pos`, `PC_START_UVS(m)` as `m->resurrect.uvs` and
`PC_POLE_ROW(p)` as `pole_pos_offset[p]`: the original tokens. Resurrect's respawn branch (`pole_pos_offset[m->slot +
6]`, `[6]` in a ghost race) reads the table directly and is unchanged. Nothing else reads or writes `polepos` (the
model record's +0x6D0): `layout_check.py --storage pc_models` has multiinit_n64's four writes, Resurrect's reads and
compare, and pc_grid_start's reads. With 6 cars every start place is 0-5 and every respawn a row of 6-9, as on the
N64.

**With item 3** (3.1, merged): CalcPolePositions fills the race's slots, the drones first, then the humans, so with
one player and 11 drones the player starts at place 11, at the back; init_cars gives the humans the first distances
behind the drones', `-(max(6, cars) - slot)`.

**The test.** `assets_grid` builds Init_MDrive (`func_800E543C`) and Resurrect (`stunt_combo_display`) with tables and
paths of its own: places 0-5 at the start point with the table's rows bit for bit; places 6-11's rows by the rule (two
tables: columns staggered forward; rows shifted sideways every other row); on a straight path the continued rows where
the straight rows are; on a bent one (a right angle 30 ft behind the start) each at its distance along the path,
facing its segment there; on a path that runs out, the rest straight back; across the loop point; where the test's
road ends 50 ft back, the side places in order and the last place its own row; 80 ft back, place 6's row and then the
side places; too narrow for the side places, each its own row; twelve cars placed by Init_MDrive (one human at the
back, eleven drones) no two closer than the table's closest pair (a car's length, 14 ft, beside the grid); the attract
demo's straight rows; a human's respawn on the table's row index + 6, a drone's none, a ghost race's row 6; battle no
offset. Undoing a part fails it: the rule changed (each place from the one two before it, half the two-row step), 12
checks; the respawn coded `index + 6`, 6; the continued rows straight from the start point, not along the path, 21;
the road check off, 10.

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

### 7.1 Done: the HUD and the setting (items 6 and 7, 2026-10-29)

On main 5e108ff (the car records `pc_cars[12]`, `PC_MAX_LINKS` 12; items 1a, 1c-1e, 2, 4 and item 3's first
part). The race's count is `pc_race_cars()`, latched at a race's start by `pc_race_cars_begin()` (CalcPolePositions
and the attract demo's func_800EC190 call it, item 3); the setting and its latch are in [FRONTEND.md](FRONTEND.md)
"The cars a race holds". Each change below gives the N64's behaviour while the count is 6, which `original`, the
default, keeps it at.

| Element | Code | Up to the N64's count | Past it |
|---|---|---|---|
| the place, "P/N" | `animPlace`'s PC version, `credits_scroll_grp/gr3_7af4.c:93-180` (the N64's at `:182-244`) | up to 9 cars the N64's calls, in its order, at its places | each number its digits (`place_digits_7AF4`): a two-digit place starts one big digit further left, so the '/' and the count's first digit stay where the N64 draws them, both digits in the player's colour; the count's second digit follows by a digit of its own font, measured only then |
| the results' finishing place | `drawRaceStandings`, `results_table/results_table.c:332`, through `PC_PLACE_TEXT` (`pc_place_text`, `:217-243`; the N64's expression on the N64) | places 1-6: the bank's own ordinals | the number, then what the bank's sixth ordinal has after its own number, read at run time in the language in effect |
| the minimap's dots, the radar's blips | ShowHUD, `showhud/showhud.c:184` (`PC_HUD_RACE_SET`, `PC_HUD_RACE_N`; `pc_hud_race_build`, `:112-131`) | up to 8 cars: the ROM's race set itself, 89 entries | a copy of the set made at run time (`pc_hud_race[]`): after the 7 of each run of dots (`func_80109A60`) and of each viewer's blips (`func_80108F40`), copies of that entry for 8 to the race's cars less one, the viewer bits kept: 20 entries more at 12 cars |
| the HUD's car order | `func_80105B74.c:92-110` | `order[]` has `PC_MAX_LINKS` places; the clear covers the N64's 6 entries | the clear covers `D_80142DB4`'s 12, so a dot past the race's cars hides. (In the N64's races entries 6 and 7 are never written and stay 0, so its dots 6 and 7 draw slot 0's again; kept at 6 cars) |
| the Drones option | `runtime_a_c93c_closure/group.c:564-565` (`PC_DRONES_MAX`, `:70-77`) | 0-5, wrapping, as the N64 | 0 to `pc_cars_setting() - 1`, wrapping; reset value and default 5. The circuit's track select changes only option 20 (`:434`), so its drones stay the N64's |

`func_80109A60` and `func_80108F40` themselves need nothing: the dot reads `D_80142DB4[AnimID & 0xF]` and the blip its
slot from `AnimID & 0xF`, both up to 15, and the model and car records have 12.

**What the retail race set holds** (read with gdb on the retail cartridge, Pi): the dots are its last 8 entries, AnimID
0-7 in order; the radar has, for each of the 4 viewers, its frame (`func_80108DA8`) followed by 8 blips, AnimID
`viewer << 8 | slot`, slots 0-7 in order; the place is one callback blit (`animPlace`). So a 12-car set has 4 dots
and 16 blips more, 109 entries. The HUD set `D_801158DC` (8 entries) has no per-car entry; its first is the HUD
order's callback (`func_80105B74`).

**The text bank's place strings.** The bank has six language sections (waves 0-5: English, Spanish, Italian, German,
Dutch, French, chosen on the options screen); in each the results' `cars` index (the same in all six) starts the six
ordinals, first to sixth. What follows them differs: in English, Dutch and French each ordinal's suffix alone, six
entries; in Italian the six ordinals again; in Spanish and German six empty strings (their ordinals are the number,
alone or with a full stop). The sixth ordinal's text after its number is the suffix every language gives the seventh
to twelfth too, so a place past the sixth is composed from it; the suffix entries would be wrong in Italian (a second
number), German (no full stop) and Spanish. Read with gdb, each language switched in turn; nothing from the bank is
in the build.

**On the screen** (retail cartridge, Pi, track 0, local screenshots). The Drones option, with `game.cars=12`: six
presses of RIGHT from the default show 11 in its box, two digits as the other options' numbers are (the options print
their values as plain decimal numbers); with `original` one press shows 0. With 12 cars (the spike's tree with this
work merged in, `RUSH_SPIKE_CARS=12`; the spike alone showed no place in one player, a bare '/' in two, and the drones'
dots but not the player's):
- one player: the place reads "3/12", then "4/12": the big digit where "6/6" has it, the count's two small digits
  after the '/', on the screen; the player's dot on the minimap and the blips on the radar, as with 6 cars. While the
  car is being put back on the road (not solid, `func_800CF604`) the place is not drawn, as the N64 does at 6;
- two players: "11/12" and "12/12", each view's place in two big digits in the player's colour, the '/' and the
  count where "6/6" puts them, each view's radar and the minimap with the players' dots;
- one player without gas, the drones past: "12/12" in two big digits, with the player's dot on the minimap; the
  same over the pause screen and its quit question;
- the game over screen's standings: after retiring 3 s into the race the player's place is fifth, the bank's own
  ordinal; after retiring 30 s in without gas, the drones past, twelfth: the number 12 and the sixth ordinal's
  suffix, in the place column's font, in its cell.
With 6 cars the same moments look as before: "4/6", and sixth in the standings.

**Tests.** `assets_carshud` runs animPlace, func_80105B74, ShowHUD (`skid_mark_render`), func_80109A60, func_80108F40
and drawRaceStandings on the game's code with made-up data: the N64's glyphs and calls for 6 and 9 cars; "9/10",
"12/12" and, in split screen, "1/12" and "11/12", at their places, fonts and colours; the HUD order for 12, 8 and 6
cars; the race set at 6, 8, 10 and 12 cars and in battle; the dots of entries 8-11 (the human's last) and slot 11's
blip, and a blip of a slot not in the race hidden; places 7-12 with a suffix, a full stop and nothing after the
number. Mutation check: 13 deliberate bugs, one at a time, all caught (one character a number; the count's second
digit a big digit on; one place digit coloured; the place not moved left; the HUD order cleared over 6 always, and
over 12 always; no copies; the copies at the set's end; the copies keeping slot 7; the copy at 8 cars too; places
past 6 read from the bank; the suffix taken from the entry after the six; the sixth's number kept). The setting's
tests: [FRONTEND.md](FRONTEND.md) "Tests" (`frontend.cars`, `frontend.precedence`, `frontend.menu.live`).

**Proof** (`game.cars` at `original`; "base" is a build of main 5e108ff):

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, base against the branch, Debug, build host (x86-64, GCC 14), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same, Pi 5 (aarch64, GCC 12) | 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 248 of 248 each (`assets_rom` skipped: no ROM there). An earlier run of each (on 597098e, while other jobs loaded the host) had one real-time pacing test fail, `hfr_paced` and `hfr_paced_queue`; alone, and in every run since, they pass |
| `data.c`, `data_symbols.txt`, the synthetic cartridge | the same files as the base's (build host and Pi; Debug and RelWithDebInfo the same) |
| `n64_neutral.py --base 09da2f0` | passes (1,014 files: 92 code-exact, 922 shifted); the 5 game files changed here each "shifted" |
| the N64 IDO check (`tools/port/ido_check.sh`, build host) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, base and branch; offset_check 59,451 accesses (12 more), layout_check 48,196 (16 more), endian_check's 2 open value conflicts in 1 storage the same, 3,525 puns (1 more), 5 more live functions (`place_digits_7AF4`, `pc_place_text`, the three of ShowHUD's copy); check-loader 96 places, 0 wrong |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`, base and branch alike. GCC 13 and mingw-w64 12 not run on it |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds, `test_assets_carshud.exe` with it; gen_data's check of the PE: OK, 18 decoupled, 1 reserved in the window |

**Not done here.** A synthetic path's screenshot check of a 12-car race (10.3's `drive12`) waits for the drones'
arrays (item 1b) and their schedule (item 5), without which a race of more than 6 cars faults: with this work on
5e108ff, `game.cars=12` and the Drones option at 11 got the retail cartridge's race through PrePlay and faulted as the
countdown began, in the model thread, `update_model` from `update_drone_models` (on 597098e, before items 1a and 2,
at PrePlay in SetResurrectionData: section 9's first crash). The check needs data the synthetic cartridge doesn't
have yet too: its race set is zero (SYNTH.md 1: only the HUD set has entries), so the place's entry, the eight dots
with a texture, the map's size (`D_801161C4`) and its places (`D_801160A8`); and a test in `synth_shot.py` of the
place's and the player's dot's pixels. The circuit's "n of 6" and points stay 6 (3.3).

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

### 8.1 Done: the pools sized and checked (item 2, 2026-10-29)

| Piece | Where |
|---|---|
| Sizes | `port/include/pc_game_types.h` ("The pools a race fills"): `PC_N64_MATRICES`, `PC_MATRIX_SPILL`, `PC_SCENE_NODES`, `PC_FRAME_LOOKATS`, `PC_FRAME_MATRICES`, `PC_GLIST_LEN`, `PC_GLIST_OFFSET`, `PC_FRAME_BYTES` |
| The checks and the high-water marks | `port/ultra/pools.c` (`pc_pool_overflow`, `pc_frame_pools`, `pc_pools_report`) |
| GetMatrix's spill in the window | `pc_matrix_spill`, `port/data/reserved.txt` (gen_data: `load_reserved`, `reserved_place`, `reserved_lines`, `check_reserved`) |
| The frame's buffer | `pc_frame_dl` in the three copies of _InitGfx (`initgraphics/initgfx.c`, `carcolor/ctx_initsystem.c`, `heap_release/initsystem.c`), `gfx_frame_begin.c`, the `Dynamic` view and the check in `world_collision_response.c` |
| The test | `assets_pools` (`port/assets/tests/test_pools.c`) |

**The pools now.**

| Pool | N64 | PC | Past its end |
|---|---|---|---|
| persistent matrices (GetMatrix, `func_800A7D6C.c`) | 150 at `D_8015F740` (0x8015F740-0x80161360), index 0 unused; no limit | the same 150 at their N64 addresses, then 128 more in `pc_matrix_spill` (0x8014A250-0x8014BA50, 6,144 B, in the window: 2.7): indexes 1-277 | said; the last matrix handed out again |
| scene nodes (`func_8008E26C`, `D_8012E700`) | 600; no limit (a 601st is written past the table) | 600 (`PC_SCENE_NODES`) | said; the last node handed out again, the count kept at 600 |
| the frame's lookats (`GfxBuf.la[32]` at +0x280 of the frame's buffer, `D_80124F84`) | 32; no limit (past it: the identity, then the frame's matrices) | 32 (`PC_FRAME_LOOKATS`) | said, at the frame's end |
| the frame's matrices (`D_8015B260`, from +1,728) | 600 (to +0x9CC0) | 1,200 (`PC_FRAME_MATRICES`, to `PC_GLIST_OFFSET` = +78,528) | said, at the frame's end |
| the frame's display list (`D_801497C8`) | 4,000 commands of 8 B from +0x9CC0 | 8,000 of 16 B from `PC_GLIST_OFFSET` (`PC_GLIST_LEN`); a buffer is 206,528 B, two of them | said, at the frame's end |
| environment visuals (`func_80090284`, `D_80138880`) | 100 | the same | the game's own: NULL, the caller goes without |
| sprites (`func_800A79F4`, `D_80140BF0`) | 200 | the same | the game's own: -1 |
| tire puffs (pool `D_8013F1E0`, `D_8013C378[100]`) | 100 | the same | the game's own: NULL, no puff |
| skid segments (pool `D_80155220`, `D_80155B30[100]`) | 100 | the same | the game's own: the farthest is taken |

`pc_pool_overflow` logs `rush2049pc: <pool> ran past its end: <used> of <capacity>` once a pool; a Debug build then
prints a backtrace and stops, a Release build (`NDEBUG`) goes on, so a player sees a wrong picture and the log says
why. `pc_frame_pools` runs every frame where the SDK's check was an empty `if` (`world_collision_response.c`:
`PC_FRAME_OVER`) and keeps the frame's high-water marks; at a run's end (`port/main.c`) one line gives every pool's:

    rush2049pc: pools: persistent matrices 202 of 277, scene nodes 441 of 600, environment visuals 47 of 100, sprites
    102 of 200, frame lookats 11 of 32, frame matrices 232 of 1200, display list 1800 of 8000 commands (high-water marks)

(the game's own high-water marks for GetMatrix `D_80156BD0`, the scene nodes `D_801569A8`, the environment visuals
`D_8012E678` and the sprites `D_8013C234`, never reset; the port's for the frame's three). It is not part of a trace
(`synth_compare.py` reads the state and status lines and the loader's summary).

**The window range.** GetMatrix's matrices must stay in the window (a window's camera matrix is kept as a u32,
`D_80124FD0`: 2.7), so the spill is a named reservation, not a bare address: `port/data/reserved.txt` (`PC_NAME
ADDRESS`) names `pc_matrix_spill 0x8014A250`; gen_data takes its size from its one declaration
(`float[PC_MATRIX_SPILL][12]`, pc_game_types.h), and refuses a range outside the game's BSS or outside a range a
decoupled item freed (here the model records' 0x8014A250-0x8014D280), over a placed symbol or another reservation, or
a type laid out otherwise on the N64; it defines the symbol absolute in data.c (zeroed with the game arena's BSS, as the bytes were), lists it in
`data_layout.txt` ("Reserved"), and `--check` fails if the executable doesn't have it there (`gen_data check: ...
1 reserved in the window`). `data_symbols.txt`, the loader's tables and the synthetic cartridge don't have it, so they
are the same files. A `RUSH_CHECK_CASTS` build ran the race, the demo, two players, drive and pause with 12 cars on
the synthetic cartridge with the spill in use (indexes to 207) and stopped nowhere. 0x8014BA50-0x8014D280 (6,192 B)
is left for others.

**The frame's buffer.** Only the list's start and the two lengths moved: the GfxBuf (viewports, vertices,
projections, lookats, identity: particle_knot's and particle_context's view, 1,728 B) and the matrices' start
(+1,728) are as on the N64. The list's start is `PC_GLIST_OFFSET` where the three _InitGfx copies and gfx_frame_begin
set `D_801497C8`, and in the `Dynamic` view; nothing else names it (no other file reaches `glist`, the buffer's size
or +0x9CC0). The renderer reads the list from the task's `data_ptr` to its end command and keeps no address of the
buffer (`port/gfx`: no reference to `pc_frame_dl` or the layout); the high frame rate's pairing never pairs the
frame's matrix or list addresses, which alternate with the two buffers anyway (GFX.md "High frame rate"). The two
buffers still alternate through `D_80156BE0[0..1].dl_base`.

**The test.** `assets_pools` builds GetMatrix, world_collision_response and the scene nodes' allocator with the pools
shrunk (`PC_MATRIX_SPILL` 4, `PC_FRAME_MATRICES` 8, `PC_GLIST_LEN` 32) and links `port/ultra/pools.c`, with a handler
of its own for a pool past its end (`test_pool_overflow`; the asset tests' `pc_pool_overflow` fails a test otherwise):
GetMatrix's 149 matrices at their N64 places, each cleared and set to the identity, then the spill's 4, then the
overflow said and the last handed out again, and the frame's reset starting over at index 1; a frame's list closed in
the layout (the text list, the full sync and the end after the frame's commands, the task sent); nothing said with the
list, the matrices or the lookats exactly full, and one command, matrix or lookat past each end said, with the count;
the scene nodes handed out to 600, the 601st said and the last handed out again with the count kept, then a freed node
handed out again.

**Measured** (the high-water marks of the pools line; a local test branch: this branch with the spike's caps,
`RUSH_SPIKE_CARS`, never merged):

Synthetic cartridge (build host, Debug; the same with `RUSH_CHECK_CASTS`):

| Path | Cars | Persistent matrices | Scene nodes | Env. visuals | Sprites | Lookats | Frame matrices | Display list |
|---|---|---|---|---|---|---|---|---|
| race, drive, pak, pause, raceend | 6 | 127 | 222 | 18 | 97-124 | 6 | 150 | 780-836 |
| | 12 | 202 | 338 | 36 | 97-124 | 12 | 197 | 1,081-1,181 |
| race2p, rumble2p | 6 | 132 | 229 | 18 | 98 | 12 | 300 | 1,659 |
| | 12 | 207 | 345 | 36 | 98 | 24 | 395 | 2,336 |
| demo | 6, 12 | 123, 198 | 216, 332 | 18, 36 | 3 | 6, 12 | 150, 197 | 836, 1,181 |
| circuit | 6, 12 | 130, 202 | 225, 338 | 18, 36 | 97 | 6, 12 | 150, 197 | 846, 1,169 |
| race3p (12: the spike gives drones to three players) | 3, 12 | 101, 212 | 163, 352 | 9, 36 | 98 | 9, **36** | 330, 593 | 1,616, 3,407 |
| race4p, pak4p (the same with four) | 4, 12 | 118, 217 | 189, 335 | 8, 24 | 97 | 16, **48** | 472, 695 | 2,311, 4,292 |

Retail cartridge (Pi, Debug, a single race on each track, its countdown and 14 s of racing; one and two players, 6
and 12 cars):

| Track | Cars | Persistent matrices | Scene nodes | Env. visuals | Sprites | Lookats | Frame matrices | Display list |
|---|---|---|---|---|---|---|---|---|
| 0 | 6 | 130, 138 | 366, 376 | 38 | 102, 108 | 6, 12 | 202, 410 | 1,767, 3,485 |
| | 12 | 202, 210 | 441, 450 | 47, 44 | 102, 108 | 11, **23** | 232, 470 | 1,800, 3,928 |
| 1 | 6 | 130, 138 | 444, 454 | 49 | 102, 108 | 6, 12 | 241, 492 | 1,884, 3,719 |
| | 12 | 202, 210 | 516, **526** | 49 | 102, 108 | 10, 21 | 269, **546** | 1,772, **4,131** |
| 2 | 6 | 130, 138 | 357, 375 | 40, 44 | 102, 108 | 6, 12 | 184, 374 | 1,659, 3,269 |
| | 12 | 202, 210 | 429, 447 | 40, 44 | 102, 108 | 11, **23** | 213, 434 | 1,619, 3,669 |
| 3 | 6 | 130, 138 | 426, 449 | 48, 56 | 102, 108 | 6, 12 | 215, 438 | 1,812, 3,575 |
| | 12 | 202, 210 | 498, 522 | 48, 54 | 102, 108 | 10, 21 | 247, 510 | 1,719, 4,063 |
| 4 | 6 | 130, 138 | 349, 373 | 52, 55 | 102, 108 | 6, 12 | 195, 396 | 1,706, 3,363 |
| | 12 | 202, 210 | 428, 438 | 52 | 102, 108 | 11, **23** | 225, 449 | 1,598, 3,811 |
| 5 | 6 | 130, 138 | 411, 421 | 75 | 102, 108 | 6, 12 | 153, 316 | 1,298, 2,995 |
| | 12 | 202, 210 | 483, 493 | 75 | 102, 108 | 6, 11 | 182, 374 | 1,575, 3,521 |

(one player, two players; one number where they agree. The 12-car rows are the grid of 4.1; the 6-car rows' display
lists on track 0 are the spike's numbers above, 1,767 and 3,485. On track 5 the cars start in a tunnel, where fewer
lookats are drawn. An earlier grid, which left two cars of track 4's back row to drop onto the road, ran track 4 to a
fault in the sound manager at 12 cars: 8.3.)

**What runs out, and what doesn't.** With 12 cars in one or two views every pool keeps a margin: the persistent
matrices reach 210 of 277, the scene nodes 526 of 600 (track 1, two views), the frame's matrices 546 of 1,200, its
list 4,139 of 8,000 (past the N64's 4,000, as the spike found), its lookats 23 of 32. On the retail tracks a car takes
12 persistent matrices, 12-13 scene nodes (19 on the synthetic cartridge), a few environment visuals, a lookat in each
view that draws it, about 5 of each view's frame matrices and up to about 40 commands of each view's list (in one view
less, and with the player at the back of a longer grid the view holds other things: track 1's one-view list is shorter
with 12 cars). The sprites don't depend on the cars. The tire puffs and the skid segments are full with 6 cars
already on the retail tracks (100 of 100 each at every frame's end once the race runs; the spike's sampling, local):
their allocators give no puff or take the farthest segment, as on the N64, so with 12 cars each car's smoke and skids
last less; a matter for the effects (item 1d), not a fault. The scene nodes are the tightest: 74 left in the worst
case; growing them means decoupling `D_8012E700` (own storage, about 30 files), not needed for 12.

Three or four views with 12 cars would pass the frame's 32 lookats (36, 48: one a car a view), which a Debug build
stops on. The N64's mode screen offers three and four players stunt and battle only, and `SetOptions` gives no drones
with three or more players (1.2), so a race of 12 cars has at most two views; if the setting (item 7) ever allows
drones with three or four players, the GfxBuf's `la[32]` needs to grow first (a PC view of the GfxBuf in particle_knot
and particle_context, and the matrices' start at +1,728 in the _InitGfx copies and gfx_frame_begin).

**The default is the N64's.** With 6 cars nothing reaches the spill (indexes to 132 on the synthetic paths, 138 on
retail), the scene nodes' 600, or the old 600 and 4,000 of the frame; the checks only read. The proof table is
8.2.

### 8.2 Proof (items 2 and 4)

The count at 6 (`pc_race_cars()`); "base" is a build of c1ab1d7, the branch's merge base, "branch" of c21afbd (the
commits after it are documents and `bounds_reviewed.txt`).

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, base against the branch, Debug, build host (x86-64, GCC 14), every synthetic path | the same trace, every run: 19 of 19 paths |
| the same, Pi 5 (aarch64, GCC 12) | 19 of 19 paths |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (`--script`, one game at a time) | the same trace, every run: 6 of 6 |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 246 of 246 each (`assets_rom` skipped: no ROM there); `assets_pools` and `assets_grid` among them |
| `data_symbols.txt`, the synthetic cartridge | the same files as the base's (build host and Pi; Debug and RelWithDebInfo the same) |
| `data.c` | the base's, and 6 lines more: the reservation's block, `pc_matrix_spill` absolute at 0x8014A250, 6,144 B (Debug and RelWithDebInfo the same file) |
| `n64_neutral.py --base 09da2f0` | passes (1,014 files: 92 code-exact, 922 shifted); 9 changed game files under `src/` |
| the N64 IDO check (`tools/port/ido_check.sh`, build host) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, base and branch; endian_check's 2 open value conflicts in 1 storage the same; the branch's 2,550 live functions (6 more: the grid's and the pools' PC functions), 48,180 accesses (78 more), 3,524 puns (6 more); offset_check 59,439 accesses placed (base 59,461), 430 untyped raw ones in 47 functions (332 in 46: the path's `s16 (*)[3]` points in pc_grid_candidate); check-loader 96 places, 0 wrong; `layout_check.py --storage pc_models`: `polepos` (+0x6D0) has multiinit_n64's 4 writes, Resurrect's reads and compare and pc_grid_pose's and pc_grid_start's reads, nothing else |
| `bounds_check`, build host (GCC 14) | 2 new *tmr*, on `pc_grid_point` and `pc_grid_offset` (the PC's own `float[3]`, declared at their true size, written by coordinate loops `k < 3`), added to `bounds_reviewed.txt` as `[gcc14]`; then none new, 0 `two offsets`, 0 `clamped`; 9 reviewed keys GCC 14 doesn't find, as the base. GCC 13 and mingw-w64 12 not run on it |
| a `RUSH_CHECK_CASTS` build (build host): race, demo, two players, drive | no stop with 6 cars; and with the test branch's 12 cars (race, demo, race2p, drive, pause), GetMatrix's spill in use to index 207, no stop |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; gen_data's check of the PE (`--pe`): OK, 18 decoupled, 1 reserved in the window |
| `assets_pools`, `assets_grid` | pass, Debug and RelWithDebInfo (the latter found a write past the test's own frame buffer, fixed in the test); undoing a part of the grid fails `assets_grid` (4.1) |

### 8.3 Found on the way

- **The sound manager's voice records** (`D_80110244`, as many as `hud_setup` makes: `D_80110264`, twice its first
  count up to 64; free list `D_80143FF8`): `func_80092278` takes the free list's head with no empty check (an empty
  `if`, `camera_target_track/group.c:62-64`), so a sound started with none free faults at NULL + 0x18. It happened once
  in the local test branch, with 12 cars on track 4 (one and two players) and the earlier grid that dropped two cars
  onto the road at the start: `entity_flags_apply` ← `func_800B61A8` ← `entity_update` ← `func_800CF06C` ← the drones'
  model iteration. With the grid of 4.1 the same races ran to their end. Not fixed here (the sound manager is item
  1a's); a PC check like `PC_OBJ_OR_UNLOCK`'s, before the message is taken, would make a start with no voice free
  return -1 as the emitter request does.
- **Cars 6-11 sink through the road** in the local test branch's 12-car retail races (this branch, the spike's caps
  and per-car arrays, before items 1 and 3 were merged): from the first frame, on every track, whatever their place
  (place 5's car sinks too when it is slot 6), while the slots below 6 sit on the road. A per-car state some other item
  still owns; it is why the grid was checked at placement (4.1).
- **The 1d note** (InitBlasts takes 50 of GetMatrix's matrices whatever the count; a blast 5 scene nodes and a ring 1 a
  car): included in the counts above, which the test branch made with every car's blast set up. The skid segments in
  use (the pool's list, `D_80155220`) were 100 of 100 at every frame's end once a retail race ran, with 6 cars and
  with 12; the allocator takes the farthest, so it never runs out.

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
| 1 | The per-car arrays of 2.3 to PC data, `PC_MAX_LINKS` long, by subsystem (each a DECOUPLE.md item, each with its inventory's window-address check): **1a** sound (`D_80140420`, `D_80140640`, `D_8010FFC4`, `D_8010FFCC`): done ([DECOUPLE.md](DECOUPLE.md) 7) | `mode_select/d5e64.c`, `mode_select/mode.c`, `func_800D5524.c` | the default; a test running StartCarSounds/StopCarSounds and the engine update on 12 records (`assets_percar`) |
| | **1b** drones (`D_801527D8`, `D_80152808`, `D_80153F28`, `D_80153F48`, `D_80153F68`, `D_80153F88`, `D_80154138`; and CarSetup's `D_801439F8`, 3.1; DoDrones' locals, 2.5): done ([DECOUPLE.md](DECOUPLE.md) 8) | `func_800EC914.c`, `maxpath_controls/group.c`, `func_800E56F8/group.c`, `display_list_flush.c`, `audio_effect_remove.c` | the default; `assets_roster` with 12 slots |
| | **1c** checkpoints (`D_80152218`, `D_801527E8`): done (DECOUPLE.md 7; CalcPlaces' locals: item 3, 3.1) | `car_checkpoints_func_800F8EC8.c` (and its `demo_pair` copy) | the default |
| | **1d** effects (`D_801392D8`, `D_8013A020`, `D_8013C098`, `D_80154660`, `D_80154FD8`, `D_80155290`, `D_80161450`) | `entity_spawn_init.c`, `func_8008E408.c`, `hud_render.c`, `physics_collision_test.c`, `func_80092484.c`, `func_800CB718.c`, `blast.c`, `entity_update_callback.c`, `entity_collision_detect.c`, `camera_smooth_lerp.c`, `skid_marks/group.c`, `func_800B0580.c` (two), `track_select_handler.c` | the default; the effects with 12 cars in `assets_cars`-style tests |
| | **1e** cameras (`D_80110680`, `D_80110668`: ROM data, filled from the image, `init=image`; the drone cameras' `j < 6`): done (DECOUPLE.md 7) | `render_post_process.c`, `func_800E9E2C.c`, `race_countdown_display.c`, `func_800EB90C.c` | the default |
| 2 | The pools: GetMatrix's pool to 150 + 12 a car in the window (the model records' freed range, a gen_data reservation); `pc_frame_dl`'s glist and the frame's matrix region sized for 12 cars and two views, with a PC check that stops (in Debug) or logs (in Release) when either runs past its end | `func_800A7D6C.c`, `initgraphics/initgfx.c`, `world_collision_response.c`, the `Dynamic` views | the default; high-water marks of 12-car runs under a test; the checks themselves fail a test that shrinks the pools |
| 3 | The count: CalcPolePositions, `func_800EC190`, LoadCars and every loop of 3.1 follow `pc_race_cars()`; the locals of 2.5; `PC_MODELS_END` → the race's end; `world_physics_tick`'s `6 - i`; `func_8010C2E4`'s per-car bits: done (3.1; the fx tags and the engine speed, 10.4) | the files of 3.1 (many overlap with 1: do 3 after 1) | the default (the count is 6); `assets_roster`/`assets_models` at 12; the synthetic paths with `cars=12` |
| 4 | The start grid: `pc_pole_offsets` (rows 0-5 the ROM's, read at startup), the respawn coding (`index + 12`) | `init_mdrive/group.c`, `resurrect/resurrect.c` | the default; a test placing 12 cars (positions on the computed rows, no two within a car's length) |
| 5 | The drone schedule for 7-11 drones (PC rows: with one human, the N64's pattern of at most two groups; with two, the same): done (DECOUPLE.md 8: the N64's rows from the game's data, past them as many iterations a round as the N64's last row of the kind) | `func_800E56F8/group.c`, `world_physics_tick.c` | the default; a test stepping `update_drone_models` for 7-11 drones (each drone updated once per round) |
| 6 | The HUD: two-digit place, ordinals 7-12 (number + the bank's suffix), 4 more minimap dots and radar icons per viewer | `credits_scroll_grp/gr3_7af4.c`, `results_table/results_table.c`, `showhud/showhud.c` and a PC table of the added blits | the default; screenshots of 12-car races in the synthetic paths' checks (`synth_shot.py`: the place and the dots drawn) |
| 7 | The setting: `[game] cars`, the F1 menu, `pc_race_cars()`, the Drones option's range | `port/config/config.c`, the F1 menu, `runtime_a_c93c_closure/group.c` | config tests; the menu's wrap at the setting |
| 8 | Tests (10.3) and the doc's proof table: done (10.4) | `tools/port/synth/*.press`, ctest | |

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

### 10.4 Done: races of 12 cars, tested (item 8 and item 3's second part, 2026-10-29)

On main 448047e, where every item of 10.2 is in. **A race of 12 cars works from the menu to the finish**: on the
retail cartridge (Pi 5, the Debug build and the ASan + UBSan one), one or two players and 10 or 11 drones race on each
of the six race tracks with every car at a place of its own and every car driving; races on tracks 0 and 5 run to
their finish line, through the results and back to the main menu; and nothing faults.

**How to use it.** Set the cars a race holds: F1 > Game > Cars in a race (7-12), or `--set game.cars=12`, or
`cars = 12` under `[game]` in rush2049pc.ini. Then raise the drones: in the track select's options list, the Drones
option now goes up to the cars less one (11 at 12). Its default stays 5, so until it is raised every race holds 6 cars.
A single race takes them; the circuit stays 6 cars (3.3), and practice, ghost, stunt, obstacle and battle have no
drones. The attract demo races the setting's cars, all drones. A change applies from the next race.

**What a race of 12 does.** The grid: the drones in two staggered columns ahead of the player, who starts at the back
(behind every drone, `-(max(6, cars) - slot)`, 3.1), all on the road (screenshots of the countdown on tracks 0, 2
and 4). The HUD: the place in two digits ("12/12"), the lap and the timer, the minimap and radar with every car (7.1).
The finish: each car's place locked as it crosses the line (car_setup_confirm, 3.1), the record screens (a lap record
asks for a name), and the game over screen's standings with places past the sixth ("12TH"). Sound: each car's engine,
within the sound manager's 80 fx tags (16 without the Expansion Pak): a start that finds none is not started (below).

**The synthetic paths.** Six press scripts, each with `--set game.cars=12` and, for a single race, the track select's
Drones option raised as a player does (DOWN to it in the options list, then RIGHT), never through a debugger:

| Path | What it does | The check |
|---|---|---|
| `race12` | one player and 11 drones on track 0, the player driving for 15 s | `cars 12`; cars_check: 12 cars, the count 12, places 0-11 once each, every car moved |
| `race2p12` | two players and 10 drones, split screen | the same, and both views drawn (synth_shot.py) |
| `demo12` | the attract demo: 12 drones | the same |
| `pause12` | the pause screen's four screens, then a retirement: the results, the game over screen | `cars 12` at the game over screen; the results visited |
| `raceend12` | 20 s of driving, the retirement, the results, the game over screen, Quit, the main menu | the results, the game over screen and state 4 visited, the main menu reached |
| `circuit12` | the circuit with the setting at 12 | 6 cars, the count 12, places 0-5 once each, every car moved |

`--log-cars` (port/main.c) prints at the run's end one line with each car of the race: in it or not, its place, lap,
distance, position, how far it is from where it stood as the race began, and the race's latched count
(`pc_race_cars()`); `tools/port/cars_check.py` checks it. Off by default, so every trace is as before. ctest runs each
path (`synth_boot_race12`...), so every CI job that runs ctest does: Linux and Windows, the UBSan job (x86-64) and the
ASan + UBSan job (aarch64). Their timeouts need no change: the slowest, raceend12, takes 3 s in a Debug build on the
build host and 4.6 s under UBSan, and the six take 6.5 to 11.2 s each under ASan + UBSan through ctest on the Pi
(race2p, for comparison, 12.2 s), against SYNTH_PATH_TIMEOUT's 1,500 s. CI's Debug-against-RelWithDebInfo comparison
runs them too (the same trace, 6 of 6, on the build host). The synthetic cartridge's laps don't count (its cars' distances never
grow, with 6 cars as with 12), so no synthetic race finishes; the races to the finish are the retail cartridge's.
A screenshot check of the place and the player's minimap dot (10.3's `drive12`) is not made: the synthetic
cartridge's race HUD set is zero (7.1 lists what it would need), and the retail screenshots show both.

**On the retail cartridge** (Pi 5, `--fast`, one game at a time; the press scripts, logs and screenshots in the ignored
`build/scratch/`; tracks 4 and 5 are locked on a fresh profile, so for them gdb set the player's two unlock bytes
(`D_80150DD8`) as the track select opened, as a profile that had won them would have them; everything else is the menu):

| Run | Result |
|---|---|
| each race track, 0-5, one player and 11 drones, then two players and 10 drones: 60 s of racing with gas (12 runs) | each reaches its state with 12 cars, the count 12, places 0-11 once each, every car moved; no fault |
| track 0, one lap, 11 drones, the player holding the gas | the race ends 98 s in with every car on lap 1 at places 0-11, the player last; the name entry (a lap record), the game over screen's standings, "12TH", 1:36.234; Quit; the main menu |
| track 5, the same | the race ends after 370 s of the gas alone (resurrections carry the car round), every car on lap 1 at places 0-11; "you just unlocked", the name entry, the best laps, the game over screen, "12TH", 6:10.675; the main menu |
| 12 cars, then 6: track 0 with 11 drones, retired from the pause screen, the results and back to the menu, then track 1 with the Drones option back to 5 (six LEFT) | the second race holds 6 cars at places 0-5, each moving, and the count stays 12: a race of fewer cars after one of more |
| the setting changed in the overlay in the middle of a race of 12 (F1, Game, Cars in a race from 12 to 11; in real time, since the overlay holds the virtual clock too), then the race goes on, is retired, and the next one starts | the race keeps its count, 12: 12 cars at places 0-11, each moving after the change ("settings closed; 1 change not saved"); the next race holds 11 cars, the count 11, places 0-10, the Drones option's 11 capped at 10 drones. 9,599 retraces in 165 s, none dropped |
| without the Expansion Pak (`game.expansion_pak=off`): track 0 and track 4, 60 s, 12 cars | each reaches its state with 12 cars at 12 places, every car moving; the same with the base build (448047e) |
| ASan + UBSan (the whole game): the attract demo of 12, one player and two players on track 0, and without the Expansion Pak | no fault, no ASan report; one UBSan report in each, `route_cross/group.c:164`'s `s16 [17]` at 17, which 6 cars give too (9); the races end with the cars where the Debug build's do |

**The leads the other items left.**
- (a) `func_80092278` takes the sound manager's fx tag list's head unchecked (`camera_target_track/group.c:62-64`), and
  the engines of 12 cars can use up its tags: 80 with the Expansion Pak, 16 without (`hud_setup` makes the larger of
  twice its voices, at most 64, and its voices and emitters). On the PC its three callers now check the list as they
  check the command pool: a start that finds no tag gives its command back and
  returns -1, camera_target_track its emitter too (`PC_TAG_OR_UNLOCK`); a race of 6 never gets there. `assets_sound`
  runs both with the list empty. None of the runs above, with the base build either, ran out: the fault (at NULL +
  0x18, on track 4) was under the old grid, with cars piled up.
- (b) Cars 6-11 sinking through the road: not seen. At the countdown on tracks 0, 2 and 4 every car stands on the
  road, and in the races every car moves and keeps its place; on tracks 0 and 5 all 12 finished.
- (c) The NaN-to-int casts in `func_8009D99C`: none, in the 25 synthetic paths under UBSan (build host) and the four
  retail runs under ASan + UBSan. They came from positions already NaN, with the old grid; not changed.
- (d) The engine speed, `(S16)` of a float (`sym_controls/group.c:211`, and `func_800D11BC.c:52` at a resurrect):
  undefined in C past an s16; not seen in these runs. Through s32 now, whose low half the N64's trunc.w.s and sh
  keep (particle_knot's `V16`), so the PC gives what the N64 gives.
- (e) Nothing else: the race's end with 12 cars (the record screens, the standings), the results' retirement, the
  pause screen, two players, the Expansion Pak off.

**Proof** (`game.cars` at `original`; "base" is a build of main 448047e):

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, base against the branch, Debug, build host (x86-64, GCC 14), the 19 synthetic paths | the same trace, every run: 19 of 19 |
| the same, Pi 5 (aarch64, GCC 12) | the same trace, every run: 19 of 19 |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` | the same trace, every run, each reaching its state: 6 of 6 |
| the six 12-car paths, Debug against RelWithDebInfo (CI's `optimized` job), build host | the same trace, every run: 6 of 6 |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 255 of 255 each (`assets_rom` skipped: no ROM there) |
| every synthetic path under UBSan (build host, halt_on_error=0): the 19 and the six | all 25 reach their states; no runtime error |
| `data.c`, `data_symbols.txt`, the synthetic cartridge | the same files as the base's (build host and Pi; Debug and RelWithDebInfo the same) |
| `n64_neutral.py --base 09da2f0` | passes (1,015 files: 92 code-exact, 923 shifted) |
| the N64 IDO check (`tools/port/ido_check.sh`, build host, 7efb7e6) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), over the game, build host | no open findings, base and branch; offset_check 59,458 accesses, layout_check 48,203 (7 more each); endian_check's 2 open value conflicts in 1 storage and 3,525 puns the same; check-loader 96 places, 0 wrong |
| `view_audit.py` on `pc_cars`, `pc_models` | 223 views, 0 disagree; 165, 0 |
| `bounds_check`, build host (GCC 14) | none new, 0 `two offsets`, 0 `clamped`; the same counts as the base's |
| Windows cross build (Pi, mingw-w64, fresh configure) | builds; gen_data's check of the PE: OK, 26 decoupled, 1 reserved in the window |

## 11. What was not checked

As of 10.4 (2026-10-29); the races, tracks and modes checked are there.
- A race of 12 to its finish on tracks 1-4, and with two players (both were checked racing, 60 s each, not to the
  finish); laps above one; the circuit past its first race; ghosts; the obstacle mode.
- Without the Expansion Pak, more than 60 s of a race, and whether LoadCars fell back to sharing a loaded car type
  (not logged); a race that uses up the fx tags (none of the runs did, 10.4 (a)).
- Sound through a device: which engines are heard, and whether a start the tags refuse is missed.
- The window-address question for each array of 2.3 beyond what the races exercised.
- The Windows program and the RelWithDebInfo build with 12 cars on the retail cartridge: the synthetic 12-car paths
  run in CI's Windows jobs and its Debug-against-RelWithDebInfo comparison (6 of 6 the same on the build host).
- `bounds_check.py` with GCC 13 (CI's runner) and mingw-w64 12 on the new code.
