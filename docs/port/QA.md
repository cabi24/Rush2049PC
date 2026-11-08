# QA sweep of the retail game (beta)

Started 2026-10-27. A sweep of the retail game on the PC for bugs the bring-up ([BRINGUP.md](BRINGUP.md)) didn't reach:
what was run, what was found, what was fixed, what is left. Runs are headless on the Pi 5 (aarch64), `--fast`, with
`--press-file` scripts. The scripts, logs, frame dumps and screenshots stay in the git-ignored `build/scratch`:
they come from the cartridge. Nothing here is measured from it (no values, times, names or counts).

```
build/pc/rush2049pc --rom <rom> --headless --no-config --audio null --fast --no-pak|--pak <scratch.mpk> \
    --press-file <script> --seconds N --status 5 [--until-state M:N] [--screenshot-at N --screenshot x.png]
RUSH_GFX_DUMP=DIR:FIRST:COUNT ...      then tools/port/frame_spikes.py DIR, and look at the frames
```

Where a script can't finish a race (a ghost race's three laps within its time limit, the circuit's tracks with the
gas alone), the run marks car 0 finished from outside: gdb attaches some seconds into the race and sets the car's
place-locked byte (`gGameCar` +0xEF), and the game's own race end, results, saves and next race follow. The
cheat codes the scripts enter are read from the cartridge and stay in `build/scratch` too.

A note on the default pak: without `--pak` or `--no-pak`, a run uses (and creates) the user's own
`~/.local/share/rush2049pc/rush2049pc.mpk`. Scripted runs should pass one of the two.

## Swept

| Path | How | Result |
|---|---|---|
| attract demo | no input for 900 s of game time | title and demo cycle through the race tracks the whole run; no fault |
| attract demo, Expansion Pak | `--expansion-pak`, 240 s | the demo also shows the sixth race track; no fault |
| single race to the finish | track 0, laps set to 1 on the track select, gas held | the finish, the results (place and time), the name entry, best laps, the game over screen and its lap times, Quit to the main menu |
| single races | tracks 0-3 with gas and steering; one lap on track 1 | run; the frame dumps' spikes are crashes, camera cuts, a resurrecting car's blinking and the stunt floor's fast grid |
| race options | backward + mirror + fog; laps 8 + wind + no drones + difficulty | the race starts with them; the HUD as the code says (no place with one car) |
| practice, Expansion Pak races | practice on track 2 (1 and 2 players); tracks 0 and 3 with `--expansion-pak` | run; the Expansion Pak's per-track objects load |
| 2, 3 and 4 players | 2: practice and battle; 3 and 4: the mode screen offers stunt and battle only, the stunt arena in three and four views | run |
| battle arenas | two players in the arenas 7-9 (6 in the bring-up; the four unlocked from the start) | run |
| stunt arena | one player, the first arena, 50 s of driving | runs |
| stunt round to its end | one and two players, the round's time option set to one minute on the track select, gas and steering until the time is up | the round never ended (fix 4); now the scoreboard (both players ranked), the game over screen and Quit |
| track select | every option row, wrap-around | as the code says (track, laps, backward, mirror, fog, wind, drones, difficulty, deaths) |
| main menu screens | records (with and without a pak: totals pages), options (every HUD toggle row, wrap-around) | as the code says |
| cheats | the main menu's code, the cheat screen, two cheats' codes, the unlock cheat, then a race on a track it unlocks | works after fixes 1 and 2 |
| cheats in a race | cone mines, car mines, super speed, frame scale and others switched on, then a race with gas and steering | runs; no fault, no odd frames but a wall beside the camera at speed |
| the PC's cheat settings (2026-11-05) | `game.cheats_menu` with and without `game.cheats_unlock`, and neither: the main menu, the cheat screen with no code entered, the invisible track switched, a race on the first track (FRONTEND.md "The cheat screen in the main menu") | as the code says: CHEATS only with the menu setting; every cheat switchable and the track hidden in the race only with the unlock |
| controller pak across restarts | a fresh `--pak` image: create a player (name entry), select it, race one lap to the finish (the record takes the player's name), quit; restart with the same image: the records screen lists the player with the race's totals, the player screen lists the player | as the code says; the image is written when the player is created and again by the race |
| name entry after a race | without a player: the grid, letters, END | the cursor starts on END; a scripted gas press still held when the entry opens gives no new A edge (a script's mistake, not the game's) |
| pause | the pause menu and each of its screens: options (the HUD toggles), controls, audio, video (the test pattern), rumble pak (none: its prompt), back with B, then continue | as the code says |
| ghost race (gRace 2) | track 0, three laps, gas only: the time limit ends it (DNF), the results, the game over screen, race again | the race restarts with its time limit; no ghost was saved (an unfinished race) |

### Round 2 (Expansion Pak on by default)

| Path | How | Result |
|---|---|---|
| all cars | the all-cars cheat, every car on the car select, a race with the last | each car's model loads and turns; the race runs. The top speed and acceleration bars are the same for every car (control and strength differ): unverified against the N64 (the bars multiply per-car tuning tables that are all zero in the attract demo) |
| audio, video, controls screens | every row of the main menu's audio and video screens, each value changed both ways, the reset row; the player screen's controls screen, every row | as the code says |
| two players with a pak | controller 1's pak with a player on it; player 1 picks it, player 2 walks its pak row | works for player 1; at the sweep player 2's row offered no pak, as the port gave only controller 1 an accessory (since added: `game.pak2`-`pak4`) |
| a two-player battle quit from the pause screen | battle, 15 s, pause, quit | faulted (fix 5); now the game over screen |
| a ghost kept and raced | the ghost race of one lap set first (ghost mode keeps three), car 0 marked finished with gdb mid-race (no script finishes three laps in the time limit, even with super speed), then race again | the ghost is compressed at the race's end and kept in memory ("limbo": `SaveGhost(..., 1)`; the game writes a ghost file only after its save prompt, which only a special profile name enables, `gSaveGhostFiles`), and race again loads it: that second race's start faulted (fix 5); with the fix the race against the ghost runs (two cars, the place out of two, the ghost on the map) |
| circuit, race to race | the first circuit, car 0 marked finished by gdb 12 s into each race (laps can't be set for a circuit, and a gas-only car doesn't finish track 1) | after each race the circuit code (no pak), the game over screen's next race, the circuit screen with the standings and races to go, the next track; the whole first circuit (tracks 0-3 twice) to its end, then a new circuit's first two races; no fault |
| a four-player battle quit from the pause screen | four players, battle, pause, quit | the game over screen; no fault (fix 5) |
| back to back | cycles of a race in a mode (single, practice, stunt), quit from the pause screen, the game over screen's Quit | the second race faulted (fix 5); with the fix seven races in a row, no fault |
| widescreen HUD | a one-player race at `--widescreen 16:9` with `video.hud=edges` (main 76a8344) | the speedometer and the left gauge at the left edge; place, laps, coins, the map and the timer at the right edge; nothing overlaps or is cut |
| long session | the attract demo for an hour of game time, its memory sampled | 80 demo cycles, no fault; the resident memory stays flat |
| mode 5 (the obstacle course) to its end | the unlock cheat, mode 5, gas and steering until its time runs out | the time limit ends it (DNF), the game over screen |
| unlocked tracks | the unlock cheat (all tracks), then a race on the sixth race track, mode 5 (the obstacle course, track 18), stunt arenas 15-17 and, two players, battle arenas 10-13 | each starts and runs 20 s of driving; no fault; no spikes in the frame dumps but a lit wall beside the camera on track 5 |

### Regression sweep, beta candidate (round 4)

Main 4f21620 (the merge 50b8377: the audio manager's guards, the HUD race fix, the RSP's yield, the renderer's
performance work), in the RelWithDebInfo build users run (`build/opt`, here with SDL2 2.30.9 built in as a release
has it), on the Pi, `--fast`: every script of rounds 1-3 again, each log compared with the earlier one's: the game
state lines (each state, in order), each race's setup (players, views, cars, mode, track), the exit status, and every
line of a fault, warning or error. A first pass at 6e98d65 gave the same.

| Path | Against the earlier runs |
|---|---|
| single race to the finish, practice, stunt, 3 and 4 players, the three 2-player battles, race options (both sets) | the same states and setups; no fault, no new warning. The race start's state 0x40000 that round 1's logs show no longer lasts a retrace, as in round 1's own Expansion Pak runs (the default since) |
| unlockables: the unlock cheat, then the sixth race track, mode 5, stunt arenas 15-17, battle arenas 10-13; mode 5 to its end | the same; mode 5's time limit ends it (DNF) and the game over screen follows, as in round 2 |
| the cheat screen; cheats in a race; all cars | the same; round 1's cheat run, which never reached the cheat screen, now reaches it (fix 2) |
| battle quit, 2 and 4 players | round 1's 2-player quit faulted; now the game over screen, as in round 2 (fix 5) |
| the stunt round to its end | the same: the scoreboard, the game over screen, Quit |
| back to back: seven races in a row, each quit from the pause screen | the same |
| the circuit (car 0 marked finished by gdb) | the whole first circuit and the next one's first race, the same tracks in the same order. One race's end came later (the gdb mark lands at another moment under load), so the script's fixed-time presses moved on one cycle later and the run's time held one race fewer |
| main menu rows (records, options) | the same |
| pak across restarts (a fresh image) | create a player, race, records, restart: the same states; the image holds the player as before |
| a pak on player 2 (`game.pak2=memory`, its own file) | player 2 creates a player on its pak, which reaches player 2's file alone; after a restart player 2's panel on the player screen lists it |
| the attract demo, 900 s | the same states as round 2's hour, cycle for cycle; no fault |
| widescreen: a race at 426x240, `--widescreen 16:9`, the edges HUD, frame dumps | frame_spikes finds two: a crash's camera swing and the camera passing through a tree beside the road; every frame both runs dumped is identical to round 2's, byte for byte, through the renderer's new state tracking and streamed vertices; the HUD at the edges, nothing cut or overlapping |
| rumble | below |

**Rumble.** The player screen's controls screen has a rumble row: with a rumble pak in, LEFT and RIGHT cycle its
three settings, off, weaker and full (else it says there is no rumble pak). The label is the text bank's entry
`rumbleNames` + the setting, read live: the three names in that order, so label and setting agree. The setting
is the profile's byte 0x2B, which UpdateModel reads: 0 no rumble, 1 the impact's level less four, 2 the level as
it is. The default is 1, not 0: the profile check (`CheckAndRepairPlayer`, a misnamed save.c function) gives a profile
without valid controls the default controls and 1, and the default player (no pak) has 1 too. With the setting
at full, a race with crashes turns the motor on, and through `--press-via controller` (input_check's virtual SDL
controller) the motor reaches the controller: the log has it rumbling at full and at a pulsed, weaker strength,
then stopping. At the default, the scripted crashes (into the side walls, at full speed too) left the motor off.
That was not the game's own: the impact's 36 thresholds (`D_801163F8`) were loaded in the ROM's byte order, so
nearly every impact stayed at the lowest levels. Main's loader sweep (ab7f592) loads them whole; round 5 below.

### Regression sweep, round 5

Main 80a2c7f: the loader's fixes (the level objects' bodies, the rumble thresholds, the wings, the respawn flags),
the UBSan declarations, the asset audit, the sanitizer fixes, the high frame rate paced by vsync and the display's
exact refresh, the RSP's yield, the fractional-scale rasterization. The RelWithDebInfo build (SDL2 built in) on the
Pi, one retail game at a time, `--fast`, each log compared with round 4's as there.

| Path | Against round 4 |
|---|---|
| every mode (single race to the finish, practice, stunt, 3 and 4 players, the three 2-player battles, both option sets); unlockables and mode 5 to its end; the cheat screen, cheats in a race, all cars; battle quits (2 and 4 players); seven races back to back; the stunt round's end; the main menu's rows | the same states and setups; no fault, no new warning |
| the pak across restarts; player 2's pak; again on 07f6a91 (the paks written on a thread of their own) | the same states; each image holds its player as before (player 2's on player 2's file alone); 07f6a91's images differ from 80a2c7f's only in the six bytes in which round 4's and 80a2c7f's differ too (each new player's ID among them) |
| the circuit (car 0 marked finished by gdb) | the same tracks in the same order and the same states as far as both runs went; as in round 4, one race's end moved the script's fixed-time presses on a cycle, at another race this time |
| the attract demo, 900 s | the same states, cycle for cycle; no fault |
| again on b318b50 (the audio pool's other callers guarded): a race to the finish, seven races back to back, battle quits, the stunt round's end, a battle, four players, cheats, mode 5 to its end, rumble, the attract demo | the same states as on 80a2c7f; no fault, no new warning |
| widescreen frame dumps (as round 4) | the same two spikes (the crash's camera swing, the tree); a third of the frames differ from round 4's by one or two pixels at the edge of the left gauge (rectangles' sides on internal pixel boundaries, the fractional-scale work), the rest byte for byte the same |
| rumble (the three rumble runs again, motor changes logged) | with the thresholds loaded whole, the scripted crashes turn the motor on at the default setting as well as at full, at strengths that follow the impact (the pulsed quarter, half and three quarters, and full), at full more often; it reaches the virtual controller. Round 4's "the default left the motor off" was the thresholds' bug |

**Real time at 120 frames a second.** The race (426x240, 16:9, the edges HUD) with `video.frame_rate=120` and vsync
on, through `soak.py` (SDL's disk audio; the Windows model: `RUSH_SIM_VSYNC_HZ=60` and coarse timed waits):

- With the game's frames and the frames shown dumped: no spikes in either; the world and the HUD drawn right in the
  frames shown (the world loss of the soak's fix 8 stays fixed). Writing a PNG per frame put the host behind in
  real time (the race ran slow, the audio short of buffers): the dumps' cost, not the game's.
- Without dumps: the game at full speed, the audio without dropouts. But at 120 a large share of the
  retraces (a quarter to most, from run to run) came a whole retrace late and were caught up, where the game's own
  rate has few, and few frames were drawn between the game's: the loop counts the host as behind and then draws
  none. The same with vsync off, and with the vsync model alone (no coarse waits). The synthetic
  `realtime_check.py --pacing 60,120` on the Pi: 150 frames drawn between in 30 s at each rate, under the check's
  250 (rocky passes it). Reported; there is no real-time run of round 4 to say whether it is new.
- Again on 30b5ab7 (a tick planned by its cost's spread, not its mean), the Pi busy with other work as before: with
  the vsync model alone the late retraces fell to a small share and many times more frames were drawn between; under
  the coarse-wait model too more were drawn, but a large share of the retraces still came late, the case
  FRAMERATE.md 6 describes (Windows refusing both its high-resolution timer and the 1 ms period).

## Found and fixed

### 1. No cheat could be entered (the cheat codes' table in the ROM's byte order)

The cheat screen (main menu, after its code) takes each cheat's button sequence (an "egg") before the cheat can be
switched. The eggs are `gEggs`, 21 records of a step count and two `u32[20]` button masks (CheckForEgg reads
them as words). Two struct declarations tie in gen_data's ranking: `Egg` (cheatscr.c, eggs.c) and an opaque
`Rec164` (`struct { u8 pad[164]; }`, func_8038FF90.c). The tie broke by the type's name, the bytes won, and the
masks stayed big-endian: no sequence could ever match. The main menu's own code (`start_screen_egg`, declared `Egg`
alone) was right, so the cheat screen opened, but every cheat stayed locked.

Fix: gen_data ranks a struct with no union and only bytes after a struct with wider elements of the same rank
(`score`, 7cfec9c). Among initialized data only `gEggs` changes; three BSS tables change kind, which the loader
zeroes anyway. Test: `datacheck`'s `known_widths` has the first and last cheat code's masks and the main menu's
code; with the old rule `rush2049pc_datacheck --synthetic` (ctest `pc_data_synthetic`) fails two of them.
Checked whether the same mistake hides elsewhere: ranking every opaque struct as plain bytes changes no initialized
symbol.

### 2. The cheat screen could not be opened (two InitEggs under one name)

The main menu opens the cheat screen (its option 6, state 0x8000000) once its own code is entered: it calls
InitEgg(23), its inlined copy of eggs.c's InitEgg, which on the N64 points at `start_screen_egg` (`&start_screen_egg[egg - 23]`).
eggs.c's InitEgg points at cheat `egg` of `gEggs`. Both are global functions named `InitEgg`, and gen_weak kept
eggs.c's ("defined in several files, no manifest owner, first kept"): the main menu compared the pads with bytes past
the table, a code of no steps, and never matched. With the fix, on the retail cartridge, the cheat screen opens, a
cheat's code is taken and the cheat switched, and a track the unlock cheat opens can be raced.

Fix: on the PC the start screen's copy has a name of its own (`startscr_InitEgg`, a `#define` under `TARGET_PC`;
05756a8). The other 46 helpers in gen_weak's list were compared body for body: the rest are copies, equivalent
spellings, or stand-in callers nothing calls. Test: the synthetic cartridge has made-up codes for the main menu and
cheat 0, and the `cheats` path opens the cheat screen, enters cheat 0's code, switches it and goes back
(`# visits: 0x8000000`); without the fix it fails, never in state 0x8000000.

### 3. A guard for the class: `weak_copies_check`

gen_weak keeps one copy of a function several files define when the manifest names no owner. The ctest
`weak_copies_check` (`tools/port/weak_copies_check.py`) reads gen_weak's report and fails when such a function's
bodies differ between its files (comments and spacing ignored), unless reviewed: one equivalent spelling
(`hs_font_set`) and four stand-in callers nothing calls. On a report with InitEgg's two copies it fails.

Since 2026-11-08 the class can't happen: every function has one definition in the C (the copies the linker never used
went with the N64 build, `tools/port/drop_copies.py`, [PCONLY.md](PCONLY.md) 4), and a second definition of a name is
a link error. gen_weak and the check went with them.

### 4. A stunt round never ended (gStuntsDone's low half)

When a stunt round's time is up and the cars have settled, CheckForStunt sets gStuntsDone, the s32
`gStuntsDone`, to 1. The race end asks are_all_cars_stopped whether it may end, and in stunt mode it answers with
`gStuntsDone_lo`, the halfword at +2: on the N64, big-endian, the word's low half. On a little-endian host the halfword
at +2 is the high one, always 0: on the retail cartridge the stunt timer ran down to 0:00, the car stopped and the
race went on for ever (only the pause menu's Quit got out).

Fix: under `TARGET_PC` are_all_cars_stopped reads the word's low half (`(s16)gStuntsDone`; 4129379). It is the only arena
symbol nested inside a wider one that code uses by both names (a scan of the data report and the C), so the class
has no other case of this form; values read back in parts through views are ENDIAN.md's. Test: `assets_stuntend`
runs are_all_cars_stopped with the two names overlapping as in the arena; without the fix it says the round may not end.

### 5. Quitting a two-player battle faulted (the players' packets at N64 offsets)

Each player gets a packet at the race start (AudAddListener: a Link, a type and a ready byte, four vectors), kept in
the player record as a window address. Two functions reached it at the N64's offsets, where on the PC the Link's
two host pointers make it 8 bytes longer: AudUpdateListener, which copies the listener's vectors into it every frame,
wrote them from +0x0C (over the link's prev pointer and the two bytes), and AudRemoveListener, which gives it back at
the race's end, read and wrote the type and ready bytes at +8 and +9 (the prev pointer again). A single race
corrupted the lists (the read found the head's NULL prev, so the packet stayed in both, and the write made its prev
1), and the next race's allocation unlinked through it: on the retail cartridge, a second race after a race quit from
the pause screen, and a ghost race's race again, faulted at address 0x1. Quitting a two-player battle from the pause
screen faulted at once (retail and synthetic cartridges alike).

Fix: AudRemoveListener's offsets go through `PKT(n)` (host `2 * sizeof(void *) + n - 8`) and AudUpdateListener's `Node`
has the packet's host layout, both under `TARGET_PC`. Either alone is not enough: with only the first, the packet's
bytes read correctly and the unlink follows the prev pointer the vectors overwrote, and single-race quits fault.
Tests: the synthetic `battlequit` path (a two-player battle quit from the pause screen) faults without the fixes;
`offset_check --strict` now finds both (score 5): its ROLES types AudRemoveListener's parameter and, a new form, what
AudAddListener returns (LAYOUT.md 9).

## Not bugs (checked)

- **The car select's stat bars (round 3).** Top speed and acceleration differ little between cars, which a
  scaled-down screenshot hides. The bars are calcStats's product of factor tables
  (`gTransStats`..`gFrameStats`, f32[][4]) indexed by each car's chosen parts (`gCarTrans`, `gCarHandling`,
  `gCarEngines`, `gCarTires`, `gCarFrame`: s8 rows, the player's from the profile's car settings at +1860).
  calcStats is byte-matched C and runs every update; the cartridge's defaults choose part 0 of every kind for
  every car but the frame, so with default parts the frame alone moves top speed and acceleration (a few percent)
  and control and strength more. On the screen the bars' widths change with the frames. The tables load as the game reads them: the factors are sane floats, the part
  tables bytes. Guard: datacheck's `known_widths` covers the factor tables and the six car physics descriptors.
- **The cars' physics differs by car (round 3).** Two car types with the same input on track 0
  accelerate differently. The six physics descriptors `gCarModels` points at load as floats and window addresses
  (every word checked after the loader); mass, spring, damping and brake factors are one value for every car on the
  cartridge, so the differences come from the descriptors, the gear and torque classes and the engine table
  (`gEngineSpecs`, fixed in main this night).

- Every weak copy of an owned function (gen_weak's headers) against its owner: none carries a `TARGET_PC` fix,
  `PC_N64W`, `CAR_OFS`, `INPUT_SIZE` or `PROFILE_OFS` its owner lacks (a PC fix in a copy would be dropped by the link).
- The HUD's place, "6/6", read as "6/5" in a scaled-down picture of the fog; at full size it is right, and the car
  count is written once at the race start (a watchpoint on it).
- The name entry after a race "ignored A": the script still held A (the gas) when the entry opened, so there was no
  new A edge. The cursor starts on END, as the code says.
- Multiplayer mode screens offer fewer modes (stunt and battle for three and four players; no ghost or circuit for
  two), so a script's DOWNs wrap: as the validity table says.

## Left

- **Ghost races**: kept, loaded and raced (round 2) only with car 0 marked finished by gdb: a script driving with
  the gas alone can't finish a ghost race's three laps within its time limit, even with super speed. A ghost from a
  real finish is untried; the car select's ghost slots on a fresh boot say there is no ghost, as they should
  with no ghost file on the pak.
- **A battle to its end**: the track select offers only a points limit for battle, and points come from kills, which
  a scripted pair of cars doesn't score; the battle's results run through the pause screen's quit (fix 5) instead.
- **Controller paks on players 2-4**: added after round 2 (`game.pak2`-`pak4`, each player's own file or a rumble
  pak; docs/port/ULTRA.md). Round 4 saved a player onto player 2's pak and read it back after a restart; players 3
  and 4's paks are untried on the retail cartridge.
- **Not reached on the retail cartridge**: the records screen's pages beyond the totals; a physical controller's
  rumble (round 4's went to a virtual one); a window (`--window`) and sound through a device.
- **Scripted runs and the user's pak**: without `--pak` or `--no-pak` a run uses, and on first use formats, the
  user's own `rush2049pc.mpk`. Tests and scripts should pass one of the two.
- The Expansion Pak (`game.expansion_pak`) is on by default since round 1's note (4ce46fa).

## Integration soak

2026-10-28, the morning before the beta. The features of the night before had each been tested alone: native and
fractional scales, the HUD at the screen's edges, upscaled textures and packs, the high frame rate, the audio output
with the device's rate, ring matching and retrace catch-up, the overlay's mouse and pages, first run and the saves'
backups. This soak ran them together, in the configuration the beta's first player has (a Windows laptop with a
2880 × 1620 panel, the release build): RelWithDebInfo, the defaults (`video.scale = native`, `widescreen = auto`,
`hud = edges`, the Expansion Pak on) plus `video.textures = upscaled` and `video.frame_rate = 120`. The runs were in
real time, not `--fast`. It started on main at fd86e03 and finished on 4f21620, after merging main.

```
python3 tools/port/soak.py --exe build/opt/rush2049pc --rom ROM --press-file SCRIPT --seconds N --out DIR [--name NAME]
python3 tools/port/soak.py --exe ... --rom build/opt/synth/rush2049_synth.z64 --no-rom-check --path NAME --out DIR
```

`soak.py` runs headless at `--size 2880x1620` (the window's pixels, which `native` and `auto` follow). Audio goes
through SDL's disk driver at 48 kHz (consumed in real time). Windows' timing is modelled: `RUSH_SIM_TIMER_STEP_US=15625`
(winpthreads' timed waits) and `RUSH_SIM_VSYNC_HZ=60` (a vsync swap). The settings, the data folder (the log, the
texture cache) and the pak are in the run's folder. It reads the resident memory every 10 s, then prints the exit
lines, the memory's growth over the run's second half and the log's most repeated lines. With `--path NAME` it runs
a synthetic path with that path's own targets and `synth_paths.py`'s checks. The overlay was driven by `--press`
scripts (`MENU`, then the Video page's rows), with the race going on between the changes. The retail scripts, logs
and frames stay in `build/scratch`.

### What ran

| Run | Where | Result |
|---|---|---|
| every synthetic path (19 on 4f21620: the races for 1-4 players, circuit, stunt, battle, battle quit, pause, race end, pak, 4-player paks, rumble, cheats, screens, menu, demo, idle) | rocky, real time | each reaches its state, no fault. On 4f21620: no retrace dropped (at most 62 caught up in a path); one underrun in each run, at its start, none after the first second; under 2% of the buffers played as silence; none skipped by the game's renderer |
| synthetic attract demo, 660 s (15 demo races) | rocky | no fault; resident memory 70-81 MB with no growth; one stall of the host (below) |
| synthetic races back to back, 700 s: a mode (single race, practice, stunt and others), a race, the pause screen's quit, the game over screen's Quit, again (11 races) | rocky | no fault, no retrace dropped; memory flat (+1 MB a minute over the second half, within the sampling's noise) |
| live changes mid-race, 225 s: textures upscaled → original → upscaled → pack → upscaled; frame rate 120 → original → 120 → display → 120; HUD edges → centered → edges; scale native → N64 → 4x → native; widescreen auto → off → auto | rocky (synthetic), the Pi (retail) | no fault; each change takes effect from the next frame; memory follows what is upscaled again for the new scale, within `video.textures`' cap |
| retail: the attract demo (660 s), races back to back (700 s), the live changes, a race with frame dumps | the Pi, real time | the audio manager fault (fix 6) before its fix, none after. With the high frame rate, the world was missing from the game's frames (8) before main's fix and is drawn after it, and the window hardly changed while the host was behind (9) before its fix. The demo's memory grows as it goes through the tracks, whose textures are upscaled 4x, toward the upscaled images' cap in memory (384 MB, `gfx_tex.c`) |
| retail frames at 2880 × 1620, 16:9: the game's frames (`RUSH_GFX_DUMP`) and the frames shown at 120 (`RUSH_GFX_HFR_DUMP`), on the virtual clock so that every frame between is drawn | the Pi, `--fast` | the HUD at the screen's edges in the game's frames and in those between, which keep the nearer frame's HUD; the 3D world upscaled 4x, the HUD and text original; `frame_spikes.py`: no spike in the game's frames or in those shown |
| the log | all | only the state and status lines a scripted run asks for, settings opened and closed, and the first underruns; nothing repeats per frame or per second, so an hour of play doesn't fill `rush2049pc.log` |

### Found and fixed

#### 6. A race faulted in the audio manager (its message pool ran out)

On the retail cartridge, in real time on a loaded host, a race faulted at address 0x2 in the audio manager's
thread: `AudChangeEffect` (transform/group.c) wrote through the NULL that `GetCmd` returns when all 128
messages in its pool `audiomgr_gCmds` are in use. The manager posts its commands to its own queue `audio_command_queue` (128
slots), the same queue the scheduler's retrace messages come in on. Each retrace message it takes queues one change
per sound source. When the manager falls behind (a burst of caught-up retraces, a host that doesn't give it the
CPU), the changes add up past 128. A command sent to a full queue (no block) also kept its message for good. The
N64's evenly spaced retraces never get there. It hit a plain race as well as one with the overlay used.

Fix (1a39ed2, under `TARGET_PC`, as macros above each function, so the N64 build's lines don't move):
- The per-retrace senders drop a command that finds no message, and give back the message of one the queue
  refuses: `AudChangeEffect`, the priority pass's starts and stops (`UpdateFx` in updatefx.c,
  `send_fx_command`), and the stop `AudStopEffect` (aud_stop_effect/group.c, the copy the game links). The next pass sends
  the sound's values again.
- The three sound starts (`AudPlayEffect`, `AudPlayEffectVP`, `AudEmitEffect`) return handle -1
  ("none" to every lookup) and give back the emitter.

Test: `assets_audiomgr` drains the pool and fills the queue. It faults without the fix.
Main's spaced catch-up (late retraces go out one at a time, once the game has taken the last) removes the burst
that made it likely.

**The other callers.** The rest of the pool's callers post on a screen's or an object's event, not per retrace,
but a starved host (a laptop on battery, background load) can still find the pool empty at a screen change, and
each of them wrote through the NULL too. The list is the linked program's, not the sources': every call to
`GetCmd` in `objdump -d rush2049pc`, mapped to the strong definition it sits in. That is 32 calls in 29
functions. The 33 calls in the group copies' bodies are dead: `gen_weak.py` makes those copies weak, and nothing
reaches their code. That covers the copies in credits_scroll_grp, mode_init and input_aux, the transform_b109.c
files and the camera_target_track_group.c files. Fix 6 had 7 of the 29. Nothing else takes a message from the pool.
Only `GetCmd` sets the in-use bytes. The manager clears them, `AudioInit` (AudioInit) clears the pool, and
the shutdown reads it. The other 22 are guarded the same way, under `TARGET_PC` with file-scope macros
(n64_neutral: shifted):

- **Sixteen posters** take the lock, fill one command and send it, and do nothing else:
  - `AudPlaySong` (play a song) and `AudSetMasterVol` (the volumes);
  - `AudKillSong`, `us_AudKillSong`, `di_post_type1`, `post_msg_a`/`_b`, `AudKillSong_TTL` and
    `AudKillSong`, which all stop the song;
  - `AudStopAllEffects` and `_alt` and `AudStopAllEffects` (stop every effect), and `AudPauseAllEffects`, which
    posts the same 7, as retail does;
  - `AudResumeAllEffects` and `AudResumeAllEffects` (resume), and `AudMono` (mono).

  Despite their names, the `object_type*_create` functions create nothing. With the pool empty, each gives the
  lock back and returns. With the queue full, it gives its message back (in-use byte +3). `AudSend`, the send in
  the nonmatching drafts, gives it back the same way.
- **The audio shutdown's two posts wait.** `AudioShutdown` posts a stop of the song and of every effect, busy-waits
  until the manager has freed every message, then frees the effects' blocks. Its `post_type1`/`post_type7` don't drop
  their stops ahead of those frees. With the pool empty, they give the lock back and yield (`PC_SPIN`) until the
  manager frees a message; with the queue full, they send with `OS_MESG_BLOCK` (the lock is free by then). This
  waits on nothing new: the shutdown waits on the same manager right after.
- **The way out of a race goes on after its stop.** The game over screen's choice (`controlMain`, three posts),
  `updateGameOverScreen` and AbortGame (`AbortGame`) stop the cars, release the environs and set the next state
  after posting. An early return would leave the exit half done. With no message, they write the command into a
  file-static scratch message, which is never sent; with the queue full, they give the message back.
- `__standin_a` (aud_stop_effect/group.c) takes two messages and keeps them, but nothing calls it
  (`weak_copies_check.py`).

None of the callers returns a result, so their own callers see nothing. The handle-returning starts are fix 6's.
The manager never answers a command. Only the shutdown waits on it, for the pool to drain. Before fix 6, one
message lost to a full queue would have held that wait forever; now none is lost.
A dropped command only changes what is heard. A dropped stop of the song leaves it playing until the next stop.
The manager starts a song only when none is playing, so the next screen's song is skipped too. A dropped stop or
pause of the effects leaves them sounding until the next stop.

Test: `assets_audiocmds` runs all 22 through the game's code with the pool empty, the queue full, and room.
With the pool empty, the lock must be balanced, nothing sent and no message taken. With the queue full, the
message must be given back; with room, the command must be sent. The shutdown must end, and the exits must still
set their next state. It faults without the guards. Retail: `RUSH2049_TEST_AUDIO_POOL_FAIL=N`, a PC-only test
hook in `GetCmd` that port/main.c reads, makes every Nth call find the pool empty. N must be 2 or more:
the shutdown waits for a message, and with 1 none would ever come.

On the Pi, the retail cartridge ran `--fast` under gdb with the hook at 1 in 2 and 1 in 3, through the synthetic
paths' scripts. A Python breakpoint on the hook counted each caller's calls and the ones that failed. 17 runs
covered the screens, single, 2- and 4-player races, circuit, stunt, a 2-player battle, battle quit, pause, race end,
the attract demo, cheats and idle (13 at 1 in 2; race, race end, pause and battle quit at 1 in 3). Each ran to its
state with no fault, and 6,097 of 15,013 `AudChangeEffect` changes were dropped. Of the event posters, these
found the pool empty and went on: `AudPlaySong` (45 times), `AudSetMasterVol` (79), `AudKillSong` (12),
`AudKillSong_TTL` (3), `AudPauseAllEffects` (6), `AudStopAllEffects` (4), `us_AudKillSong` (2), `AudMono` (4)
and the game over screen's `controlMain` (1). Fix 6's starts did too: `AudPlayEffect` (140),
`AudPlayEffectVP` (22) and `AudEmitEffect` (90). Seven more runs had gdb set the hook's phase on entry to
each poster, so that the poster's own message was the one refused: race end, battle quit, the screens, pause, the
menus, a race and the demo. Every call of the posters those paths make failed: `AudSetMasterVol` 74 of 74,
`AudPlaySong` 28, `AudMono` 7, `AudStopAllEffects` 7, `AudKillSong` 4, `AudKillSong_TTL` 3,
`AudPauseAllEffects` 3 and `us_AudKillSong` 2. Each run still reached its state. The scripted paths never reach the
rest: the shutdown (`AudioInit` on an inited audio system), AbortGame, the soft reset, unpausing, the audio screen's
init and update, `AudKillSong`/`AudStopAllEffects` (image A's `updateStartScreen`), and
`AudPauseAllEffects` and `AudResumeAllEffects`, which nothing calls directly (the data's function table names them).
`assets_audiocmds` covers those. Two last runs had gdb refuse the game over screen's own post at its line
(gameover.c's scratch message): battle quit and race end still went on to their next states, 0x80 and the main menu.

#### 7. The Windows timing model waited in the wrong place with a high frame rate

`RUSH_SIM_VSYNC_HZ` (and `RUSH_SIM_PRESENT_MS`) held the main loop after every retrace's present. With
`video.frame_rate` on, that present only keeps the frame for the ticks and swaps nothing, while the ticks swap and
never waited. A headless run of the player's configuration therefore slept where a real window doesn't, and never
where it does. The wait now follows the swap (471bb85): after a retrace's present at the game's own rate, after a
tick that showed a frame otherwise. Runs with the setting off are unchanged, `audio_realtime`'s among them.

#### 8. With a high frame rate, in real time, the game's frames lost most of the world

Found on fd86e03 with the retail cartridge in real time. With `video.frame_rate` on, at 640 × 480 and at
2880 × 1620, with original or upscaled textures, the game's own frames (dumped, and screenshots) lost the road and
the buildings. The ground showed as flat fog colour, with fragments of geometry and the cars. `--fast` runs were
right, and so were runs with the setting off. Cause: with the setting on, the present copies the frame into the
presented image with its own framebuffer binds, and `bind_fbo`'s cache wasn't told. In real time the next graphics
task runs before any tick (a tick waits while an RSP task is pending), so its depth clear went to the presented
image, and the frame kept the last one's depth. Already fixed on main by 538cd5a (renderer performance, test
`gfx.interp.depthclear`). The same retail frames on 4f21620 are right.

#### 9. With a high frame rate, a host that is behind hardly updated the window

With `video.frame_rate` on, only the main loop's ticks show anything; the retrace's present just keeps the game's
frame for them. The loop skips a tick that would run into the next retrace, by the running mean of how long ticks
take (`tick_ns`). That mean includes the swap, which with vsync waits for the display. On a host that is behind,
nearly every tick was skipped, the game's own new frames with them. On the retail cartridge in real time on the Pi
at 2880 × 1620, the window hardly changed for minutes of races while the game ran on. With the setting
off, each of the game's frames is shown at its swap whatever that costs. Fix (2747ce9): a tick skipped to keep the
retraces on time still shows a frame the game has swapped in and the window hasn't shown (`pc_gfx_frame_waiting`).
It shows it with no frame drawn between (mode 2), so the result is no worse than the setting off. Test:
`hfr_realtime` (`realtime_check.py --frame-rate 120 --present-ms 40`: the synthetic race with each swap held 40 ms,
then until the vsync) needs at least 5 frames shown a second. Without the fix it got 0.1. The cost: on a host far
behind (the Pi at that size), the forced show comes at a tick that is already late, and it holds the next retraces
more than the setting off does. The setting off shows each frame at its swap, a whole period ahead. Showing the
waiting frame at the retrace's present instead is left to the high frame rate's pacing work.

### Reported, not fixed here

- **Few frames between, with vsync or on an integrated GPU at 2880 × 1620.** Two limits, both on the synthetic race
  at 120 frames a second on rocky (UHD 770):
  - Without a vsync wait, at 2880 × 1620 about one tick in ten drew a frame between; the rest were "not drawn (the
    GPU was behind)", as the frame fence intends. This held on 4f21620 too, after main's renderer work. At
    640 × 480 it was about two in three.
  - With the vsync model (fix 7), most ticks are skipped: the running mean `tick_ns` includes the swap's wait for
    the display, so a tick looks as if it would run into the next retrace. At 640 × 480, rates 60, display and 120
    drew 46, 16 and 104 frames between in 36 s; without the vsync wait, 120 drew 1044.

  vsync is on by default, so on a 60 Hz panel the setting mostly shows the game's own frames. The tick
  scheduling with a blocking swap belongs to the high frame rate's pacing work (another agent's).
- **Stalls on rocky's shared GPU.** Runs on rocky that overlapped other GL work (two of these soaks at once, and the
  other agents' GL tests) stalled for 1 to 50 s, the main thread waiting in the kernel's i915 request wait. Over the
  same minutes the kernel log shows GPU hangs and "preemption time out" resets across many processes, this soak's
  among them. Runs alone had no such stall. The cause is the host, not the program; whether anything this program
  submits can trip the preemption timeout wasn't established.
- **The pak's write** (`pc_si_flush`: a temp file, fsync, rename, the folder's fsync) runs on the main loop's
  thread. It took under 20 ms here, but on a slow or busy disk it would hold the retraces. That is si.c's area.

### Not bugs

- After a live change the overlay counts "changes not saved" against the file (or the defaults with `--no-config`),
  not the command line: `--set video.textures=upscaled` changed to `original` is "no change". As FRONTEND.md says.
- SDL 2.30.9's disk audio driver (the Pi's fetched build) refuses `/dev/null` and falls back to the null output.
  `soak.py` gives it a file and deletes it afterwards.
- On the Pi 5 at 2880 × 1620 the retail game can't keep its 60 retraces a second. Graphics tasks take longer than a
  retrace, the host drops and catches up retraces, no frame between is drawn, and scripted presses land late.
  The Pi is not a target at that size.

### Left

- A real display: the vsync swap with a high frame rate and the overlay's mouse at 2880 × 1620 were modelled and
  tested headless only.
- GL errors: the renderer has no error check in the live path. Mesa's own log of the program's GL errors
  (`MESA_DEBUG=1`) printed nothing during the synthetic live changes on rocky; it wasn't run on the retail cartridge.
- An hour of the attract demo with upscaled textures, to see the memory settle at the cap. Round 2's hour (above)
  had the original textures.
- The per-player paks (`game.pak2`-`pak4`) came in after this soak's runs were planned and weren't part of the
  configuration.

## Player reports

Reports from play on the user's own machine, open until reproduced or explained.

### 1. The car exploded when steered left or right (Windows, 63640c8, open: not reproduced; Windows's game is Linux's)

- **Report** (2026-11-04): the Windows zip of 63640c8 (the Release workflow's artifact build), on the user's PC: "I
  explode when I steer right or left". The settings file was the defaults but for the window (`[video]` width 1920,
  height 1080, position 480,270): no `game.cars`, `game.frame_rate`, `draw_distance` or `game.memory` setting, so
  none of the PC features was on. Whether the keyboard or a controller steered was not said. After the user changed
  their settings they couldn't make it happen again, and no log was taken of the run that did.
- **Checked** (the Pi 5, the retail cartridge, `--fast`, `--no-config`): the Release workflow's linux-aarch64 build of
  the same commit and of the beta before it (95bfaf0), one race on track 0 with the gas held and full-lock steering
  left and right in turns (`build/scratch/steer/steer.press`: 1 s, 1 s, 1 s, 1 s, 0.5 s, 0.5 s, 2 s, 2 s). The two
  builds' traces are the same, second by second (the first car's position, speed and steering). In both the car
  crashed twice, each after a second of full lock at about 150 mph took it into the track's side, and the game blew it
  up and put it back on the road three seconds later, as the N64 game does after a hard crash. The keyboard and a
  bound button always steer at full lock (`input.stick_range`), which at that speed turns the car into the barrier
  within a second.
- **Compared** (2026-11-04, [WINDOWS.md](WINDOWS.md) 12): the Windows program against the Linux builds, on the
  synthetic cartridge in CI's runs of 63640c8 (41824398187) and ea4b82f (41853308343). The cross-built program, as
  the zip is built, gave the same trace as x86-64 and aarch64 Linux on all 59 synthetic paths (each state at the
  same retrace, the first car in whole feet, its steering), the same 26 screenshots byte for byte, and in the 10
  live-input runs, which print exact floats each second with the steering at full lock both ways, the same floats to
  the bit. Nothing in the game's arithmetic compiles differently there: no x87 and no fused multiply-add in the game
  code, and of the host's math library only `sqrtf` (correctly rounded on both) and `modff` (the window's field of
  view). CI's `windows-same` job now compares every synthetic path to the bit on each run. No Windows-only
  difference in the game's arithmetic was found, in the traces or in the program's code, so nothing points to one
  behind the explosion; the hard crash at full lock that the Pi runs show stays the likelier reading.
- **Not checked**: the x86-64 Windows build on the retail cartridge (the cartridge stays on the Pi, which has no
  x86-64 emulator). The synthetic cartridge's car doesn't follow the controls (its course is the same with the
  steering, gas and brake or without) and has no wall, so the turn into the barrier runs only on the retail one;
  and CI's Windows runs have Mesa's llvmpipe, not a GPU driver (whose effect on the threads' floating-point state
  was not looked at).
- **If it is seen again**: the log (`rush2049pc.log` next to the program), the settings file, keyboard or controller,
  the track, and whether a short tap of the steering on a straight, with nothing near, is enough.

## Integration QA of the PC features

2026-11-04, on main 1ec5f69. The features of the night before had each been proven alone: `game.memory`
([MEMORY.md](MEMORY.md)), `video.draw_distance`, `video.detail` and `video.fog` ([DETAIL.md](DETAIL.md)),
`game.frame_rate = 60` ([FRAMERATE.md](FRAMERATE.md) 7), the mods folder ([MODS.md](MODS.md)) and `game.saves =
files` ([SAVES.md](SAVES.md)), with `game.cars` 12, `video.frame_rate` 120, widescreen and texture packs from before.
This ran them together. "The heaviest settings" below are `game.cars = 12`, `game.memory = 64`, `video.draw_distance
= max`, `video.detail = high`, `video.fog = off`, `game.frame_rate = 60` and `video.frame_rate = 120`; "with a mod and
files" adds a mod of every car file recoloured (`rushmods.py extract`, then each palette and RGBA16 texel green, kept
under `build/`) and `--saves files`. The retail cartridge ran on the Pi 5 (Debug, `--fast` but for the long run, one
locked job at a time, its logs, screenshots, saves and mod in the ignored `build/qa/`); its results are described in
words, not numbers.

### What ran

| Run | Where | Result |
|---|---|---|
| every synthetic path (56) at the heaviest settings, Debug and RelWithDebInfo | rocky | the same trace in both builds, 56 of 56 (`synth_compare.trace`, `RUSH_STATE_EXACT`, status every second); no fault, no pool past its end, no heap short. The paths' own checks fail only where the settings change what they check (below, "Not bugs") |
| the same with `video.fog = far`, `video.widescreen = 16:9`, `video.textures = upscaled`, `video.hud = edges` and `--saves files`; at `game.memory = 16` and `256` | rocky | the same: every path to its state, the heap's line `short 0, none 0`; the mods' memory raised to 64 MiB for 12 cars of `mods_big`'s cars at 16 |
| `saves_check.py`'s five scenarios (import, migrate, game, race, swap) at the heaviest settings, with and without the game at 60 | rocky | 0 failed checks |
| `synth_paths`' real-time races of 12 cars at the heaviest settings (`soak.py --path race12`, `race2p12`, `demo12`, 1280 x 720) | rocky | each to its state; 0 retraces dropped, 0 underruns; resident memory flat |
| a single race of 12 cars on each race track (0-5), one player and two, to the results and the game over screen (the track select's Laps 1, the Drones 11 or 10, the gas held) | retail | every one of tracks 0-4 reached the results and the game over screen with 12 cars at places 0-11, each moved; track 5 (gdb unlocking it as MORECARS.md 10.4 did) in a job of its own, see below. No fault, no pool past its end (scene nodes, the tightest, kept a margin of over a tenth: track 1 with two views, which detail high gives the one-player level objects), no heap short, no retrace dropped |
| the same with a mod and files: tracks 0 and 3 with one player, 2 with two | retail | the same states at the same retraces as without them; the mod's 13 files replaced; the drones' and the player's cars green where their paint is |
| the attract demo, 12 drones, at the heaviest settings (and with a mod and files) | retail | four demos over 130 s, tracks 0-3, 12 cars each, every car moving |
| stunt (one player) at the heaviest settings | retail | the arena, the round's clock run out by itself and the game over screen |
| a two-player battle at the heaviest settings (and with a mod and files) | retail | the arena, two cars, 9 minutes of battle with no fault; it has no time limit and nobody scored, so it didn't end by itself (`r_battlequit` ends one from the pause screen) |
| the six retail scripts | retail | see "Found and fixed" 11 |
| track 5, the heaviest settings, one player (1 lap) and two players | retail, a job each | one player: 9 minutes of racing, the gas-only car going round by resurrections without finishing its lap (the job's 28 minutes ran out); two players: 3 minutes of racing (the run's limit). No fault, every car moving, the pools and the heap within bounds |
| the long run: 25 minutes in real time, the heaviest settings with a mod, files, 16:9 and the HUD at the edges, RelWithDebInfo (the release's type), the null audio drained in real time | retail | menus, a single race of 12 cars paused and resumed, two two-player races, their results and game over screens; no fault. Resident memory steps up when a new kind of screen first appears (the second view) and is flat otherwise (the second half's slope +0.02 MB a minute); the log holds only the state and status lines. The host is behind real time in races of 12 cars at max: retraces caught up and some dropped, about a tenth of the audio buffers played as silence (a Debug build loses more). The script's later steps (the other races, the battle) went astray: one press short at the pause screen's confirmation, found afterwards with `--fast` (below) |
| the ghost (SAVES.md 11) | retail, files | not written; see "Not bugs" and "Left" |

**The sanitizers** (ASan + UBSan, Debug, the Pi 5: SANITIZE.md's build, `halt_on_error=0` so that every report
prints, ctest's suppressions):

| Run | Result |
|---|---|
| ctest, the new features' 64 tests (`detail`, `mem`, `mods`, `profile`, `ghost`, `saves`, `fps`, `pak`, `game60` in their names, but the real-time ones) | 64 of 64; no report |
| ctest, the other 237 (but the real-time ones) | 237 of 237; reports only in `sanitize_fault_chain`, the blank cartridge's expected fault and its NaN casts |
| every synthetic path (56, the mods made first) at the heaviest settings | each to its state (the same checks as the Debug run fail, for the same reasons); no ASan, UBSan or LSan report |
| retail: a single race of 12 cars on track 0 to its finish at the heaviest settings with a mod and files; track 3 with two players, 30 s of racing; a two-player battle, 30 s; the attract demo, 70 s | each to its state, no ASan or LSan report; one UBSan report, `ControlName.c:98` at the results' name entry (12, suppressed with its reason) |

### Found and fixed

#### 10. `rushmods.py extract` refused MODS.md's own example

`python3 tools/mods/rushmods.py extract baserom.us.z64 out/ cars/car01 101-119 --text` (MODS.md 2) stopped at its
first file: `'cars/car01' is no wave number or name`. `names` lists each wave as `folder/name` and the document wrote
it so, but `parse_files` took a number, a range or a bare name only. It takes `folder/name` now when the folder is
the wave's (`tracks/car01` is still refused). Test: `rushmods.py selftest` (ctest `mods_tool`) finds every loaded
wave by its number, its name and its `folder/name`, and refuses another wave's folder. Found making this section's
recoloured-car mod.

#### 11. The retail battle scripts ran practice

The six retail scripts of every proof since MORECARS.md 10.4 (`race`, `race2p`, `battle2p`, `stunt`, `battlequit`,
`pause`, through `synth_compare.py --script`) were the synthetic cartridge's press scripts. On the retail cartridge
the mode screen of two players lists four modes (single race, practice, stunt, battle; screenshots of it, local),
not the synthetic cartridge's seven, and draws them in over about 2 s, so `battle2p.press`'s five DOWNs wrap round
to practice: `battle2p` and `battlequit` compared a two-player practice race on track 0 and its quit (mode 1, track
0 in their state lines), the same in every run, and no proof ran a battle on the retail cartridge. Found when a
battle at the heaviest settings came out as practice; the same with every setting at its default.

Fix: `tools/port/retail/battle2p.press` and `battlequit.press`, the retail mode screen (three DOWNs once its list is
in), each with an `# expect:` line of mode 6, track 6; checked on the retail cartridge at the defaults and at the
heaviest settings (the battle arena, two cars; the quit reaches the game over screen). And `synth_compare.py` now
checks a path's or a script's `# expect:` line as `synth_paths.py` does: a run that doesn't end so MISSES and fails,
however alike the runs are. Test: ctest `synth_compare_expect` (`tools/port/test_synth_compare.py`): its verdicts on
made-up traces, a practice race the same in every run MISSES a battle's expect, and the retail scripts expect a
battle. The six retail scripts are now MORECARS.md 10.4's last paragraph's. The synthetic `stunt.press` reaches the
stunt arena on the retail cartridge as it is (one player's list has stunt four down).

#### 12. UBSan: the name entry's read before an empty name (suppressed)

At the results of a race run to its finish (retail, ASan + UBSan): `highscorescr/ControlName.c:98: index -1 out
of bounds for type 'u8 [13]'`. A high score's name finished with nothing typed has its trailing spaces trimmed with
`name[len - 1]` at `len` 0, the byte before the name, which is 0 on the N64 and on the PC (the last of `humans`,
`humans`, for the first player; the previous name's terminator for the others), so the loop ends before it
writes. The retail function's own read: suppressed with its reason (`ubsan.supp`, SANITIZE.md 4). No synthetic path
finishes a race, so CI's sanitizer jobs never reach it.

### Not bugs (checked)

- **The synthetic paths' own checks at the heaviest settings.** `--until-state M:N` and `--screenshot-at N` count
  graphics tasks, which come every retrace with the game at 60: such a run stops at half its game time
  (`cheats`, `screens`, `raceend` before their last presses), and `race2p12`'s and `race4p`'s screenshots catch the
  views still black before the race (at twice the count every view is drawn, 97-98% of each); the `detail_*` paths'
  checks expect their own settings; `demo`, `race2p`, `rumble2p` and `memhigh2p` expect 6 cars (with `game.cars =
  12` the attract demo races 12, and two players with the Drones at 5 make 7); a 16:9 picture is 426 wide where
  `synth_shot.py` wants 320; with `--saves files` there is no `.mpk` for `pak_notes.py`. Every path reaches its
  state, and each check passes with its own settings.
- **"Past their last level of detail" at max** (the scene line, about one a car a view): the cars' sheens, which
  `PC_LOD_REACH` keeps at their reach by design (DETAIL.md 2.2).
- **A view without its place** in a split-screen screenshot: the place is drawn only while the car is solid
  (`AreWeSolid`: on its wheels, not being put back), as the N64 draws it.
- **The battle doesn't end** with the gas held: its rules end it on points by default or at a time limit (`countdown.c:276-297`), and nobody shot.
- **The Pi 5 doesn't run the heaviest settings in real time** (the long run, below): a graphics task takes the
  Debug build about 35 ms of CPU there; FRAMERATE.md 7.7 and DETAIL.md 4.1 measured the parts. That the program
  doesn't say so is FRAMERATE.md 7.8's (another agent's).
- **The retail game saves a ghost to the pak only for one name.** SAVES.md 11's open question: after a ghost race
  `updateHighScoreScreen` (`updateHighScoreScreen.c:117-128`) opens the save prompt only when `gSaveGhostFiles` is set, which only
  LoadPlayerData sets, for a profile named as the cartridge's developer name (`save/group.c:335-342`);
  for any other profile it calls `SaveGhost(..., 1)`, whose limbo is `SaveGhostInLimbo`: the ghost is copied into the
  in-memory records table `gLimboGhosts` (three a track and way, `save_limbo/SaveGhostInLimbo.c`), which the track
  select's ghost choice reads (`carscr/carscr.c:614-615`) and nothing writes to a pak. So with `game.saves
  = files` a player's own ghost races are kept for the session only, as on the N64, and the folder's ghosts are the
  imported ones. A ghost race also has three laps always (`SetOptions`, `options_set/group.c:146`) and ends only
  at its time limit (`countdown.c:309-316`); its track select has no Laps option, and GhostEnd
  (`nonmatching/game/GhostEnd.c:117`) keeps the ghost only of a car that finished its laps.
  Checked again 2026-11-05, as a possible porting bug: it is the cartridge's own behaviour. The compare is
  `strcmp` (`game_strcmp`) of the profile's name with `cheat_profile_name`, a 12-character code of digits, letters and a
  symbol (read in the running game on the Pi, not recorded), not a default name; the same branch unlocks the cheat
  screen (`cheat_menu_unlocked`) and three more flags (`gCheatState[14-16]`). The flag has no other writer: in the game's C
  only that write and updateHighScoreScreen's read use it (the other files declare it only); the retail ROM has two
  instructions with its address's low half (0x48E4) as an offset, the two of the C, and no data word holding it
  (no table of flags points at it). The pak write, `SaveGhost(..., 0)`, is only in the prompt's handler
  (`ControlGhost`, high score state 4), reached only from that test (`__standin_d`, which also sets state 4, is a
  matching stand-in nothing calls); the session's ghosts (`SaveGhostInLimbo`) are heap copies in the records table
  `gRealLimboGhosts`, which no code writes to a pak.

### Left

- **A ghost the retail game writes into a folder.** The game writes one only through the save prompt, which a
  profile of another name never gets ("Not bugs"), and only of a car that finished its laps, which a ghost race's
  three laps and its time limit rule out for a car driven by the gas alone. A gdb poke of the prompt's flag and of
  gNumLaps at the race start (`CalcPolePositions`) didn't hold: GhostEnd saw 3 laps again (SetOptions runs after it);
  the next try, gNumLaps set at every checkpoint (`PassedCP`), was not run. The synthetic cartridge's
  `saves_files` (the developer's name there) covers SaveGhost's write into a folder.
- **Making the game's own ghosts files** for every profile: `game.ghost_prompt` (SAVES.md 7.1, 2026-11-05) opens the
  cartridge's save prompt for every profile, whose save is then a file with `game.saves = files`. Writing the
  session's ghosts (`gLimboGhosts`) without a prompt is not done.
- **The long run's battle.** A long run's press script works on the host's clock, so every step after a race must
  wait at a screen that waits for a press; on this cartridge the pause screen's quit asks to confirm (START, UP, A,
  RIGHT, A), and the results need an A before the game over screen's UP and A. Both found with `--fast` after the
  runs; a corrected run was not made.
- **Track 5 to its finish** at the heaviest settings (the gas-only car doesn't finish its lap), and a battle to its
  end (nobody scores).
- **The six retail scripts** with the retail battle scripts, base against the branch: not run (nothing this section
  changed reaches the program: the fixes are tools, a suppression and documents).
- Windows (the cross build builds, fresh configure, RelWithDebInfo, every test program); a real display; an audio
  device (the Pi's build has no SDL2).
