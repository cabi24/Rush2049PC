# Bringing the port up on the retail cartridge

Written 2026-10-27. The PC build had run only the synthetic cartridge ([SYNTH.md](SYNTH.md)) and the start of the
retail one: the loader checks passed (`rush2049pc_datacheck <rom>`), and the game booted to the Midway logo, the title
and the attract demo, where it faulted. This is the log of the stops after that, in the order the game met them: what
stopped it, why, the fix, and the test that keeps it fixed. The retail cartridge is the user's own, used locally by
its path; nothing from it is in the repository (no values, tables, strings, pictures or logs). Tests use synthetic
data: the loader's unit tests, `port/assets/tests`, and the synthetic cartridge, which now has data at the places
the retail one showed to matter.

```
build/pc/rush2049pc --rom <rom> --headless --no-config --audio null --no-pak --fast --press-file <script> \
    --until-state 0x400000:300 --seconds 90 [--screenshot-at N --screenshot shot.png] [--status 4]
```

`PC_DATA_REPORT=<path>` writes the loader's report (section 2). On the Pi 5 (aarch64) `RUSH_TRAP_FP` does nothing:
its cores don't trap floating-point exceptions, so a NaN is found by watching memory (section 3).

## Where it stands

| Path | Reaches | Screens |
|---|---|---|
| boot, no input | the start prompt, the controller pak warning, the title (state 4) and the attract demo (8), cycling through tracks 0-3 for as long as it runs | the demo is a race of drones on the track, with the title's start prompt over it |
| the front end | the main menu (0x10), the player screen (0x20), the mode screen (0x40), the track select (0x80), the car select (0x100); the records (0x2000), options (0x200), audio (0x800) and video (0x1000) screens and back | as on the N64: the blue menus and their buttons, the track map and its fogged preview, the car turning on the car select with its bars; the video screen's test pattern has its grey ramp and red, green and blue bars in order |
| a single race, 1 player | race start, PrePlay, the countdown (0x200000), the race (0x400000); with gas and steering the car drives, up to about 150 mph, and off the road onto the grass | the track, the cars, the HUD (speed, lap, position, the map, the time) |
| pause | the pause screen, quitting: the results (0x2000000), the game over screen (0x800000), the title and the main menu again | |
| 2 players | a single race in split screen (top and bottom), both cars driving | each view with its HUD, the map in the side panel |
| circuit, practice, ghost race | a race each | |
| a controller pak (`--pak`, a fresh image) | the warning finds it; the race, with the save code at its start | |
| sound | `--dump-audio` from the title: silence for 4 s, then the title music (a WAV with the levels and peaks of music; not listened to) | |
| stunt, 1 player | the stunt arena's race; the car drives and jumps | the arena, the timer, the coin counts |
| battle, 2 players | the battle arena in split screen; the cars drive | the health bars, the scores, the side panel |
| 3 and 4 players | the player screen in quarters, the mode screen (stunt and battle only), a stunt arena in three or four views | four views, each with its HUD, the timer in the middle |

Not tried yet: a race to its finish line, saving a ghost, sound through a device (`--audio sdl`), a window
(`--window`).

## The stops

### 1. The attract demo's blit: NewMultiBlit's tables without their layout

The demo faulted in its first frame, in `strcpy` from InitBlit (`collision_sound_play`). NewMultiBlit
(`sound_control`) reads its descriptor table as `MultiBlit`, whose texture name and animation function are pointers;
the demo's table `D_80114128` was declared `s32` in every file, so the loader kept it as words in the N64 layout, and
the name, read as 8 bytes, took in the field after it.

Ten tables passed to NewMultiBlit had declarations without the layout (`s32`, `u8[]`, `char[]`, createBG's view
with an `s32` name). `pc_prelude.h` has the descriptor's PC layout, `struct pc_multiblit`, and each file declares its
table with it under `TARGET_PC`. Test: the synthetic cartridge writes those tables (the demo's blit with a NULL name),
so its generator's kind check fails if a declaration loses the pointer; the demo, pause, raceend and screens paths run
them. (Commit 270ba85.)

### 2. Tables sized by the gap ran over strings

An open-ended table takes its extent from the next known address. Where strings follow it, the loader swaps them with
the table's elements. The pointers to those strings resolve to described data, so the report didn't list them; it
does now: a pointer whose target lies in an element wider than a byte, in an arena or in own storage, and whose bytes
read as text, is a `text-in-words` line (`pc_data_test`). Undescribed targets that read as text are marked `(text)`:
those are fine as bytes, the others are tables (section 8).

| Table | Ran over | Symptom | Fix |
|---|---|---|---|
| `D_8011F914`, AnimateShadow's `f32[][4]` shadow scales | the car types' names | every car part lookup failed; a part object with no model id reused its parent's scene node, which became its own child, and the show/hide walk recursed until the stack ran out | `f32[14][4]` (body types 0-13) |
| `D_8011F0A0`, the sfx groups pushed at boot | the songs' names | the audio screen's names scrambled | `Instrument4[4]` |
| `D_803B83A4`, image A's warning screen blit (`MultiBlit[]`) | the car select blits' names | the car select faulted on a texture lookup that failed | one `MultiBlit` |

Test: the synthetic cartridge keeps strings right after these tables (`strings_after`), and refuses bytes a
declaration covers there, so a table sized by the gap again stops its generator. (91f98eb, c4db0a2.)

### 3. Cars at NaN: the torque curve over the tire records

In the demo every car's position became NaN. Watching car 0's record with a gdb Python breakpoint at each physics
step found the first NaN in a tire's traction: the tire records' floats had their halves exchanged. The torque curve
`D_8011F724` (`u16[]`) ran by the gap over the two tire records after it, which only the physics descriptors' u32
`tire_info` fields reach, so they were swapped as u16s. The curve is `u16[10 * 12]` (func_800E2D18's
`short[10][12]`), and the tire records are declared in `car_resistance` as `float[2][6]` (func_800D09E8 copies six)
and referenced there, so the loader knows them. Test: the synthetic cartridge's tire curves are those records, so its
kind check needs them to be floats. (91f98eb.)

### 4. The attract demo runs

With 1-3 the demo runs: the drones race on the track, the title's start prompt over them, and it cycles back to the
title and on through tracks 0-3. The picture is right: the road, the buildings and the bridge, the cars in their
colours, the checkered start, the sky.

### 5. Skid marks

At the end of the demo, and later in races, the skid code freed a display handle that was a float. StopSkid
(`func_800AF844`) writes a finished skid's midpoint and done flag through its own view of the effect record, which had
the N64 offsets; on the PC the effect's links and display handle are pointers, so the midpoint landed on the time and
the handle. Its view has the PC layout now. The reset (`func_800B0580`) also cleared the skid table's N64 size, 96
bytes short of the PC's. Test: `assets_pool` runs StopSkid and the reset. (acfd8e8.)

### 6. `--until-state` and the retail state word

The retail cartridge's gGameState starts as the game's own "no state", -1 (func_800B7FF8 tests for it), whose bits
match every `--until-state` mask, so each was met on the first retrace. The host loop takes no state from the value
the state word starts with, until the game first changes it; the synthetic cartridge starts it at -1 too. (f458ed4,
and the commit that made the rule general.)

### 7. The track select's fog

The track select faulted in its first frame, in the 2D draw's palette load, with a NULL colour table. The fog blit's
palette is `D_803BA1E8`, a WAD-style Palette whose colour table the draw reads at +0x14 as a u32 window address: the
word `D_803BA1FC`, which the C declared `u8 *` and so gave storage of its own outside the window. It is a u32 in the
arena now (`FOG_TABLE`, `PC_N64W`). Test: the synthetic cartridge makes the fog (an IA texture, its option valid);
without the fix its paths fault at the track select. (88be4c3.)

### 8. Tables only pointers reach

The stunt arena faulted in `camera_shake_start` at its race start. The track animations are per-track tables behind
the pointer tables `D_8011A31C` (object-animation sequences, each with frames) and `D_8011A840` (palette
animations), read with their host layout, but no symbol describes them: by the gap, the first sequence tables lay in
the shared frames table's storage and the palette animations in `D_8011A31C`'s, converted as the wrong types. The
same report showed the suspensions (`carSuspensions`) and the demo paths' start points in big-endian order.

`port/data/reached.txt` now names such tables by rule (DATA.md 7): the root table of pointers, the type, and how
the count is found (fixed, ended by a zero field, a field of the parent element, or a table of counts). At each load
the loader follows the pointers, converts a type with pointers to host storage of its own (once per table, children
included) and rewrites the pointer, and swaps a pointer-free type in place. Tests: `pc_reach_test` (hand-written
rules on a synthetic image); the synthetic cartridge's track 0 has a palette and an object animation, and its race
faults without the rules. (f27b457.)

### 9. The video and audio screens' second blit

The video screen called an animation function at -1. It makes two blits from `D_8011ACA0` (or `D_8011ACE8` when
paused), the audio screen two from `D_801105E4`, but each was declared one MultiBlit, so the second was read past
its storage. Each is declared with its two. Every other table passed to NewMultiBlit holds the count it is passed.
Test: the synthetic cartridge sets the second blits, which its generator refuses outside a symbol. (dc622ce.)

### 10. Battle: the health bars' palettes

Battle mode faulted in PrePlay in the same palette load as section 7: `animHealth` points each health bar's palette
(`healthPal`, image B) at the bar's colours, and the game C's palette type had a pointer for the colour table, 8
bytes at +0x18 instead of the u32 at +0x14 the draw reads. It has the u32 on the PC (`PAL_BASE`). Test: the synthetic
battle HUD has an IA blit and player 1's health bar; without the fix the battle2p path faults. (d6958b8.)

### 11. Battle: the gear ratios

Battle cars didn't move: full throttle, an absurd drive torque, wheels that never turned. The gear ratios are a
row of six per car class (the model's byte 9; battle's cars use a later row), read from `D_80110EC0` by byte
offsets; by the gap the table was one row, and the others kept the ROM's byte order. `extents.txt` gives `D_80110EBC` its eight
rows. `tools/port/index_check.py` now knows m2c's byte-offset indexing and finds this table, and only it; its ctest
fails without the extent. The synthetic cartridge sets every row. (5b30657.)

### 12. Four players: the model slots

A four-player game faulted on the player screen, on a model name index in the millions: the player screens' model
slots (`D_803B470C`, 22 per player) had an extent of three players (a known address inside the table), so the fourth
player's kept the ROM's byte order. `extents.txt` gives it four. Test: the synthetic cartridge sets all four
players' slots, which its generator refuses outside a three-player table. (762e8fe.)

### 13. zlib's extra_blbits

The report's last undescribed table that wasn't text, colours or short strings: zlib's bit-length extra bits
(`D_8011E974`, int[19]), which only `static_bl_desc` points to and no code names. It would have been wrong whenever a
ghost is compressed. `zlib_deflate` references it on the PC, so it is described. Test: the synthetic cartridge sets
it (zlib's own constants), which its generator refuses without the declaration. (00b6c8b.)

## How the stops were found

- The fault handler's backtrace names the function; `gdb -batch` with `bt -15` gives the outer frames of a deep
  recursion (section 2).
- The loader's report: `text-in-words` lines, `undescribed` lines without `(text)`, `reached` lines, and `addr-word`
  lines (words in non-pointer fields that look like addresses: mistyped declarations).
- For a wrong value, a hardware watchpoint on its address (`watch -l *(float *)ADDR if ...`) from `pc_data_init` on,
  or a gdb Python breakpoint that scans a record for NaN at every physics step and reports the first function after
  which one appears.
- Looking an address up in `build/pc/pc-data/data_symbols.txt`: which symbol's extent holds it, and as what
  elements; and `index_check.py`.

## What's next

- The paths not tried yet (the table above), and races to their end on the retail cartridge.
- The report's remaining `undescribed` targets that aren't text are colours (`gTrackColors`), short labels (the pak
  numbers, a few names), the per-kind byte tables `D_80118DDC`, and the tail of the shared animation frames'
  extent: right as bytes, or unused. Ghost saving and loading (zlib) haven't run on the retail cartridge.
- More tables sized by the gap may run over data no pointer reaches; `index_check.py` and the `text-in-words` lines
  find two kinds of them.
- Audio on the retail cartridge (`--audio sdl`), a window, and the renderer's look at full speed.
