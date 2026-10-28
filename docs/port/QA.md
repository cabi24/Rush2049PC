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
| track select | every option row, wrap-around | as the code says (TRACK, LAPS, BACKWARD, MIRROR, FOG, WIND, DRONES, DIFFICULTY, DEATHS) |
| main menu screens | records (with and without a pak: totals pages), options (every HUD toggle row, wrap-around) | as the code says |
| cheats | the main menu's code, the cheat screen, two cheats' codes, the unlock cheat, then a race on a track it unlocks | works after fixes 1 and 2 |
| cheats in a race | cone mines, car mines, super speed, frame scale and others switched on, then a race with gas and steering | runs; no fault, no odd frames but a wall beside the camera at speed |
| controller pak across restarts | a fresh `--pak` image: create a player (name entry), select it, race one lap to the finish (the record takes the player's name), quit; restart with the same image: the records screen lists the player with the race's totals, the player screen lists the player | as the code says; the image is written when the player is created and again by the race |
| name entry after a race | without a player: the grid, letters, END | the cursor starts on END; a scripted gas press still held when the entry opens gives no new A edge (a script's mistake, not the game's) |
| pause | the pause menu and each of its screens: options (the HUD toggles), controls, audio, video (the test pattern), rumble pak (none: its prompt), back with B, then continue | as the code says |
| ghost race (gRace 2) | track 0, three laps, gas only: the time limit ends it (DNF), the results, the game over screen, RACE AGAIN | the race restarts with its time limit; no ghost was saved (an unfinished race) |

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

- **Ghost races**: a ghost race reaches its results and RACE AGAIN, but a script driving with the gas alone can't
  finish its three laps within the time limit, even with the super speed cheat, so no ghost was saved, compressed
  (zlib) or raced against. That needs a run with real steering (a recorded input script, or a person).
- **The Expansion Pak is off by default** (`game.expansion_pak`): without it the sixth race track can't be chosen
  (OptionValid, and the unlock code), and the per-track animated objects, the front end's song and the extra race
  WADs are skipped. With `--expansion-pak` the demo, the races tried and the sixth track run. Whether the PC should
  default to it is a product decision.
- **Unlockable content** beyond the cheat: tracks 4-5, stunt arenas 15-17, battle arenas 10-13 and mode 5 (track
  18) unlock through circuit and stunt results; only track 4 was raced (through the unlock cheat).
- **Not reached on the retail cartridge**: the audio, video and controls screens' every row; the records screen's
  other pages; circuit mode to its end; a controller pak on players 2-4; the rumble pak; a window (`--window`) and
  sound through a device.
- **Scripted runs and the user's pak**: without `--pak` or `--no-pak` a run uses, and on first use formats, the
  user's own `rush2049pc.mpk`. Tests and scripts should pass one of the two.
