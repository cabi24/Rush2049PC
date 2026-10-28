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
| attract demo | no input for 900 s of game time | cycles title and demo through the race tracks for the whole run; no fault |
| attract demo, Expansion Pak | `--expansion-pak`, 240 s | the demo also shows the sixth race track; no fault |
| single race to the finish | track 0, laps set to 1 on the track select, gas held | the finish, the results (place and time), the name entry, best laps, the game over screen and its lap times, Quit back to the main menu |
| single races | tracks 0-3 with gas and steering; one lap on track 1 | the race runs; no spikes in the frame dumps but crashes, camera cuts and a resurrecting car's blinking |
| race options | backward + mirror + fog; laps 8 + wind + no drones + difficulty | the race starts with the options; HUD as expected (no place with one car) |
| practice (gRace 1), Expansion Pak races | track 2; tracks 0 and 3 with `--expansion-pak` | run; the Expansion Pak's per-track objects load |
| 3 and 4 players | the mode screen offers stunt and battle only; the stunt arena in three and four views | run |
| track select | every option row, wrap-around | as the code says (TRACK, LAPS, BACKWARD, MIRROR, FOG, WIND, DRONES, DIFFICULTY, DEATHS) |
| main menu screens | records (no pak: its totals page), options (every HUD toggle row, wrap-around) | as the code says |
| controller pak | `--pak` on a fresh image: a race to the finish, the records screen | see "Left" |

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

## Left

(being filled in)
