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
| stunt round to its end | one and two players, MINUTES set to 1 on the track select, gas and steering until the time is up | the round never ended (fix 4); now the scoreboard (both players ranked), the game over screen and Quit |
| track select | every option row, wrap-around | as the code says (TRACK, LAPS, BACKWARD, MIRROR, FOG, WIND, DRONES, DIFFICULTY, DEATHS) |
| main menu screens | records (with and without a pak: totals pages), options (every HUD toggle row, wrap-around) | as the code says |
| cheats | the main menu's code, the cheat screen, two cheats' codes, the unlock cheat, then a race on a track it unlocks | works after fixes 1 and 2 |
| cheats in a race | cone mines, car mines, super speed, frame scale and others switched on, then a race with gas and steering | runs; no fault, no odd frames but a wall beside the camera at speed |
| controller pak across restarts | a fresh `--pak` image: create a player (name entry), select it, race one lap to the finish (the record takes the player's name), quit; restart with the same image: the records screen lists the player with the race's totals, the player screen lists the player | as the code says; the image is written when the player is created and again by the race |
| name entry after a race | without a player: the grid, letters, END | the cursor starts on END; a scripted gas press still held when the entry opens gives no new A edge (a script's mistake, not the game's) |
| pause | the pause menu and each of its screens: options (the HUD toggles), controls, audio, video (the test pattern), rumble pak (none: its prompt), back with B, then continue | as the code says |
| ghost race (gRace 2) | track 0, three laps, gas only: the time limit ends it (DNF), the results, the game over screen, RACE AGAIN | the race restarts with its time limit; no ghost was saved (an unfinished race) |

### Round 2 (Expansion Pak on by default)

| Path | How | Result |
|---|---|---|
| all cars | the ALL CARS cheat, every car on the car select, a race with the last | each car's model loads and turns; the race runs. The top speed and acceleration bars are the same for every car (control and strength differ): unverified against the N64 (the bars multiply per-car tuning tables that are all zero in the attract demo) |
| audio, video, controls screens | every row of the main menu's audio and video screens, each value changed both ways, RESET TO DEFAULTS; the player screen's controls screen, every row | as the code says |
| two players with a pak | controller 1's pak with a player on it; player 1 picks it, player 2 walks its pak row | works for player 1; player 2's row offers controllers 2-4 only, each "no controller pak" or "not connected": the port gives only controller 1 an accessory (`game.pak`, port/ultra/si.c), so players 2-4 can't save |
| a two-player battle quit from the pause screen | battle, 15 s, pause, QUIT | faulted (fix 5); now the game over screen |
| a ghost saved and raced | the ghost race of one lap set first (ghost mode keeps three), car 0 marked finished with gdb mid-race (no script finishes three laps in the time limit, even with super speed), then RACE AGAIN | the ghost is compressed and saved to the pak at the race's end, and RACE AGAIN loads it: that second race's start faulted (fix 5); with the fix the race against the ghost runs (two cars, the place out of two, the ghost on the map) |
| circuit, race to race | the beginner circuit, car 0 marked finished by gdb 12 s into each race (laps can't be set for a circuit, and a gas-only car doesn't finish track 1) | after each race the circuit code (no pak), the game over screen's NEXT RACE, the circuit screen with the standings and races to go, the next track: tracks 0, 1, 2, 3 in a row (the whole circuit: see below) |
| a four-player battle quit from the pause screen | four players, battle, pause, QUIT | the game over screen; no fault (fix 5) |
| back to back | cycles of a race in a mode (single, practice, stunt), quit from the pause screen, the game over screen's Quit | the second race faulted (fix 5); with the fix seven races in a row, no fault |
| long session | the attract demo for an hour of game time, its memory sampled | 80 demo cycles, no fault; the resident memory stays flat |
| mode 5 (the obstacle course) to its end | the unlock cheat, mode 5, gas and steering until its time runs out | the time limit ends it (DNF), the game over screen |
| unlocked tracks | the unlock cheat (ALL TRACKS), then a race on the sixth race track, mode 5 (the obstacle course, track 18), stunt arenas 15-17 and, two players, battle arenas 10-13 | each starts and runs 20 s of driving; no fault; no spikes in the frame dumps but a lit wall beside the camera on track 5 |

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
the pause screen, and a ghost race's RACE AGAIN, faulted at address 0x1. Quitting a two-player battle from the pause
screen faulted at once (retail and synthetic cartridges alike).

Fix: object_activate's offsets go through `PKT(n)` (host `2 * sizeof(void *) + n - 8`) and main_menu_input's `Node`
has the packet's host layout, both under `TARGET_PC`. Either alone is not enough: with only the first, the packet's
bytes read correctly and the unlink follows the prev pointer the vectors overwrote, and single-race quits fault.
Tests: the synthetic `battlequit` path (a two-player battle quit from the pause screen) faults without the fixes;
`offset_check --strict` now finds both (score 5): its ROLES types object_activate's parameter and, a new form, what
func_800D63EC returns (LAYOUT.md 9).

## Not bugs (checked)

- Every weak copy of an owned function (gen_weak's headers) against its owner: none carries a `TARGET_PC` fix,
  `PC_N64W`, `CAR_OFS`, `INPUT_SIZE` or `PROFILE_OFS` its owner lacks (a PC fix in a copy would be dropped by the link).
- The HUD's place, "6/6", read as "6/5" in a scaled-down picture of the fog; at full size it is right, and the car
  count is written once at the race start (a watchpoint on it).
- The name entry after a race "ignored A": the script still held A (the gas) when the entry opened, so there was no
  new A edge. The cursor starts on END, as the code says.
- Multiplayer mode screens offer fewer modes (stunt and battle for three and four players; no ghost or circuit for
  two), so a script's DOWNs wrap: as the validity table says.

## Left

- **Ghost races**: saved, loaded and raced (round 2) only with car 0 marked finished by gdb: a script driving with
  the gas alone can't finish a ghost race's three laps within its time limit, even with super speed. A ghost from a
  real finish, and the ghost menu that picks a saved ghost on the car select, are untried.
- **A battle to its end**: the track select offers only a points limit for battle, and points come from kills, which
  a scripted pair of cars doesn't score; the battle's results run through the pause screen's quit (fix 5) instead.
- **Controller paks on players 2-4**: the port gives only controller 1 an accessory (`game.pak`, port/ultra/si.c),
  so players 2-4 can't load or save a player. A feature to add, not a game bug.
- **The car select's stat bars**: top speed and acceleration are the same for every car; unverified against the N64.
- **Not reached on the retail cartridge**: the records screen's pages beyond the totals; the rumble pak; a window
  (`--window`) and sound through a device.
- **Scripted runs and the user's pak**: without `--pak` or `--no-pak` a run uses, and on first use formats, the
  user's own `rush2049pc.mpk`. Tests and scripts should pass one of the two.
- The Expansion Pak (`game.expansion_pak`) is on by default since round 1's note (4ce46fa).
