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
place-locked byte (`player_array` +0xEF), and the game's own race end, results, saves and next race follow. The
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
| a ghost kept and raced | the ghost race of one lap set first (ghost mode keeps three), car 0 marked finished with gdb mid-race (no script finishes three laps in the time limit, even with super speed), then race again | the ghost is compressed at the race's end and kept in memory ("limbo": `SaveGhost(..., 1)`; the game writes a ghost file only after its save prompt, which only a special profile name enables, `D_801148E4`), and race again loads it: that second race's start faulted (fix 5); with the fix the race against the ghost runs (two cars, the place out of two, the ghost on the map) |
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
is the profile's byte 0x2B, which func_800E56F8 reads: 0 no rumble, 1 the impact's level less four, 2 the level as
it is. The default is 1, not 0: the profile check (`draw_speedometer`, a misnamed save.c function) gives a profile
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
| the pak across restarts; player 2's pak | the same states; each image holds its player as before (player 2's on player 2's file alone) |
| the circuit (car 0 marked finished by gdb) | the same tracks in the same order and the same states as far as both runs went; as in round 4, one race's end moved the script's fixed-time presses on a cycle, at another race this time |
| the attract demo, 900 s | the same states, cycle for cycle; no fault |
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

## Found and fixed

### 1. No cheat could be entered (the cheat codes' table in the ROM's byte order)

The cheat screen (main menu, after its code) takes each cheat's button sequence (an "egg") before the cheat can be
switched. The eggs are `D_803B3460`, 21 records of a step count and two `u32[20]` button masks (CheckForEgg reads
them as words). Two struct declarations tie in gen_data's ranking: `Egg` (cheatscr.c, eggs.c) and an opaque
`Rec164` (`struct { u8 pad[164]; }`, func_8038FF90.c). The tie broke by the type's name, the bytes won, and the
masks stayed big-endian: no sequence could ever match. The main menu's own code (`D_803B41D4`, declared `Egg`
alone) was right, so the cheat screen opened, but every cheat stayed locked.

Fix: gen_data ranks a struct with no union and only bytes after a struct with wider elements of the same rank
(`score`, 7cfec9c). Among initialized data only `D_803B3460` changes; three BSS tables change kind, which the loader
zeroes anyway. Test: `datacheck`'s `known_widths` has the first and last cheat code's masks and the main menu's
code; with the old rule `rush2049pc_datacheck --synthetic` (ctest `pc_data_synthetic`) fails two of them.
Checked whether the same mistake hides elsewhere: ranking every opaque struct as plain bytes changes no initialized
symbol.

### 2. The cheat screen could not be opened (two InitEggs under one name)

The main menu opens the cheat screen (its option 6, state 0x8000000) once its own code is entered: it calls
InitEgg(23), its inlined copy of eggs.c's InitEgg, which on the N64 points at `D_803B41D4` (`&D_803B41D4[egg - 23]`).
eggs.c's InitEgg points at cheat `egg` of `D_803B3460`. Both are global functions named `InitEgg`, and gen_weak kept
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

### 4. A stunt round never ended (gStuntsDone's low half)

When a stunt round's time is up and the cars have settled, camera_scene_manager sets gStuntsDone, the s32
`D_80152738`, to 1. The race end asks func_800E7DD0 whether it may end, and in stunt mode it answers with
`D_8015273A`, the halfword at +2: on the N64, big-endian, the word's low half. On a little-endian host the halfword
at +2 is the high one, always 0: on the retail cartridge the stunt timer ran down to 0:00, the car stopped and the
race went on for ever (only the pause menu's Quit got out).

Fix: under `TARGET_PC` func_800E7DD0 reads the word's low half (`(s16)D_80152738`; 4129379). It is the only arena
symbol nested inside a wider one that code uses by both names (a scan of the data report and the C), so the class
has no other case of this form; values read back in parts through views are ENDIAN.md's. Test: `assets_stuntend`
runs func_800E7DD0 with the two names overlapping as in the arena; without the fix it says the round may not end.

### 5. Quitting a two-player battle faulted (the players' packets at N64 offsets)

Each player gets a packet at the race start (func_800D63EC: a Link, a type and a ready byte, four vectors), kept in
the player record as a window address. Two functions reached it at the N64's offsets, where on the PC the Link's
two host pointers make it 8 bytes longer: main_menu_input, which copies the listener's vectors into it every frame,
wrote them from +0x0C (over the link's prev pointer and the two bytes), and object_activate, which gives it back at
the race's end, read and wrote the type and ready bytes at +8 and +9 (the prev pointer again). A single race
corrupted the lists (the read found the head's NULL prev, so the packet stayed in both, and the write made its prev
1), and the next race's allocation unlinked through it: on the retail cartridge, a second race after a race quit from
the pause screen, and a ghost race's race again, faulted at address 0x1. Quitting a two-player battle from the pause
screen faulted at once (retail and synthetic cartridges alike).

Fix: object_activate's offsets go through `PKT(n)` (host `2 * sizeof(void *) + n - 8`) and main_menu_input's `Node`
has the packet's host layout, both under `TARGET_PC`. Either alone is not enough: with only the first, the packet's
bytes read correctly and the unlink follows the prev pointer the vectors overwrote, and single-race quits fault.
Tests: the synthetic `battlequit` path (a two-player battle quit from the pause screen) faults without the fixes;
`offset_check --strict` now finds both (score 5): its ROLES types object_activate's parameter and, a new form, what
func_800D63EC returns (LAYOUT.md 9).

## Not bugs (checked)

- **The car select's stat bars (round 3).** Top speed and acceleration differ little between cars, which a
  scaled-down screenshot hides. The bars are func_8039D300's product of factor tables
  (`D_803B28C8`..`D_803B2A58`, f32[][4]) indexed by each car's chosen parts (`D_80110E78`, `D_8011103C`,
  `D_80111080`, `D_8011119C`, `D_80111230`: s8 rows, the player's from the profile's car settings at +1860).
  func_8039D300 is byte-matched C and runs every update; the cartridge's defaults choose part 0 of every kind for
  every car but the frame, so with default parts the frame alone moves top speed and acceleration (a few percent)
  and control and strength more. On the screen the bars' widths change with the frames. The tables load as the game reads them: the factors are sane floats, the part
  tables bytes. Guard: datacheck's `known_widths` covers the factor tables and the six car physics descriptors.
- **The cars' physics differs by car (round 3).** Two car types with the same input on track 0
  accelerate differently. The six physics descriptors `D_80110D08` points at load as floats and window addresses
  (every word checked after the loader); mass, spring, damping and brake factors are one value for every car on the
  cartridge, so the differences come from the descriptors, the gear and torque classes and the engine table
  (`D_8010FD80`, fixed in main this night).

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
thread: `entity_transform_calc` (transform/group.c) wrote through the NULL that `func_80091B00` returns when all 128
messages in its pool `D_80142DD8` are in use. The manager posts its commands to its own queue `D_801427A8` (128
slots), the same queue the scheduler's retrace messages come in on. Each retrace message it takes queues one change
per sound source. When the manager falls behind (a burst of caught-up retraces, a host that doesn't give it the
CPU), the changes add up past 128. A command sent to a full queue (no block) also kept its message for good. The
N64's evenly spaced retraces never get there. It hit a plain race as well as one with the overlay used.

Fix (1a39ed2, under `TARGET_PC`, as macros above each function, so the N64 build's lines don't move):
- The per-retrace senders drop a command that finds no message, and give back the message of one the queue
  refuses: `entity_transform_calc`, the priority pass's starts and stops (`sound_position_update` in updatefx.c,
  `_SendCmd`), and the stop `scheduler_recv` (entity_lookup/group.c, the copy the game links). The next pass sends
  the sound's values again.
- The three sound starts (`entity_flags_apply`, `high_scores_display`, `camera_target_track`) return handle -1
  ("none" to every lookup) and give back the emitter.

Test: `assets_audiomgr` drains the pool and fills the queue. It faults without the fix.
Main's spaced catch-up (late retraces go out one at a time, once the game has taken the last) removes the burst
that made it likely. The other callers of the pool fire on a screen's or an object's event, not per retrace, and
are left as they are.

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
