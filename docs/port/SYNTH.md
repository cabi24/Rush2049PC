# The synthetic test cartridge (tools/port/synth_rom.py)

Written 2026-10-27. Without the ROM, `rush2049pc --no-rom` boots a blank cartridge, which ends in AudioSfxInit:
most of the game's code had never run on the PC, and the 64-bit bugs left were in that code. The synthetic cartridge
is a ROM image of original, minimal data, built from the decompilation's knowledge of where the game reads its data
and in what format. On it, the game boots through the front end's screens and into a race on an empty track, and
runs it. Getting there found port bugs in six places (section 3): a function the PC had linked to the wrong runtime
image, the audio manager's records, IDO varargs, a busy-wait, and four readers of the car model record. Each is
fixed with a regression test.

| | |
|---|---|
| Generator | `tools/port/synth_rom.py` (Python 3, stdlib) |
| Output | `build/synth/rush2049_synth.z64` (12 MiB) and `rush2049_synth.txt` (every value and file, and why) |
| Needs | the PC build's data report, `build/pc/pc-data/data_symbols.txt`: build the port first |
| Run | `build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 --no-rom-check --headless --no-config --audio null --no-pak` |
| Checks | ctest `synth_rom`, `synth_boot_menu`, `synth_boot_race`; CI step "Boot the synthetic cartridge" (both Linux jobs) |

**No ROM bytes.** The generator reads no ROM and copies nothing from one. Its inputs are the image table
(`tools/port/n64syms.py`), the symbol files and `layout/*.json` (function addresses), the build's data report
(addresses and element kinds, from the build's DWARF), the decompiled C (names it looks up, record layouts) and the
format documents ([ASSETS.md](ASSETS.md), [LOADERS.md](LOADERS.md), [MUSYX_DATA.md](MUSYX_DATA.md)). The images are
zero except 495 values, each listed with its reason; the files are as small as their formats allow, with solid-colour
textures and no sounds. The output is under `build/`, which version control ignores: never commit it. Its SHA-1 is
not the retail one, so it runs with `--no-rom-check`.

## 1. What it makes

**The cartridge.** The PC reads four things from a ROM (DATA.md 3, ULTRA.md 3):

| ROM | What | Here |
|---|---|---|
| 0x0 | header | the byte-order mark and a name |
| 0x283D0, 0x7110 B | the boot segment's data (raw) | zero but the values below |
| 0xB0CB10, 0xB5C534, 0xB6FEC4 | the game image and runtime images A and B (raw DEFLATE, each within its retail slot) | zero but the values below |
| from 0x100000 | the 183 files of the file table (ASSETS.md 2.2) | below; each wave's offset, size, kind (0, raw) and model flag set in the table |

Values are written big-endian at their N64 addresses, as a ROM holds them, and the data loader swaps them by the
element kinds the build's DWARF gives. The generator checks each value against the kinds of the symbol it lands in,
so a value written at the wrong width stops it rather than reading back swapped. Strings and the two small tables
that pointers in the data reach go in game-image data bytes no symbol covers (`Cart.string`, `Cart.blob`): the loader
copies those as they are, so they are bytes, or zero.

**The values** (`make()`, in three parts):

| Part | Symbols | Why |
|---|---|---|
| boot | `D_8002C5D0` (sample cache entries per voice: 2), `D_8002D8B8`..`D_8002D8C8` (MusyX buffer scales: 1.0) | zero caches corrupt the heap in AudioSfxInit (MUSYX_DATA.md 8.6) |
| | `D_8011EAAC` | the MusyX service table: the eight functions (MUSYX_DATA.md 8.1) |
| | `D_8011F0A0` | the sfx groups pushed at boot (12-15), which the project has |
| | `D_8002E4A0`, `D_8002E528` | the two static display lists: G_ENDDL (a list of zeros runs off its end) |
| | `state_word_b` (gNewGameState) | 1: the first frame runs initGameState; 0 is a game with no state |
| front end | `D_8011FAD4` | the button map (game bit j = N64 buttons): none means no button ever works. Made up from the code: bits 0-2 Start, A, B (what the prompts take), 10-13 the d-pad (what the stick makes) |
| | `D_803B4679` rows 1-4 | the modes valid per player count: all; none loops forever in updateModeScreen |
| | `D_803B7738[20]` | the track select's map objects: a name each (it swaps their LODs and puts them back) |
| | `D_8011B3A0`, `D_8011B400`, `D_8011B404` | the car select's texture and model names: "" (a NULL name faults) |
| race | `D_8011A924[19]` | gWorldNames: "WORLD" |
| | `D_80117530[122]` (+0) | the targets' names: TARGET000.. (NULL faults; "" would match every world node) |
| | `D_801173D8[12]`, `D_80117408[12]` | the demo paths' start points: one, point 0 |
| | `D_80110D08[13]`, `D_801108C8` | the cars' physics descriptor (0xB4 words at 0x801108D0, where CARS.md puts them): mass, inertia, tire positions, four tire curves (one 23-word record) |
| | `D_80111560[3]` | carSuspensions: one `_tSusp` (nine f32), in the same f32 table |
| | `D_8011AF90[19]` | gTrackColors: sixteen greys |

**The files:**

| Waves | Format | Content |
|---|---|---|
| 0-5 | text bank | six sections: 1024 single strings, 256 in each other table, all short ("EN0.12") |
| 6, 7, 8 | MusyX project, pool, sample directory | the four sfx groups with empty lists; empty pool; no samples |
| 10-21 | songs | no tracks |
| 22-37, 38-53 | font WADs, font metrics | one solid I4 page; 95 letters, each the same 6 x 8 box |
| 54 | front-end WAD | a texture for every name the C looks up (BACKGROUND_SFX, BUTTON, CURSOR, ... and ""), the models it looks up (AAANULLOBJ, which MBOX_FindModel takes for a NULL or empty name, MINEG1, the 20 map objects), a palette named "" (the car paint's name comes from a format string, zero here) |
| 55-60 | front-end screen WADs | empty WADs (a TOC, no chunks) |
| 61-68, 76-100 | race, HUD, car WADs | like 54, without the map objects |
| 101-119 | track WADs | like those, plus the world node's model TRACK |
| 120-138 | world WADs | WHDR naming one world, one WOBJ node (TRACK), one empty GDAT leaf, no GTLD keys |
| 139-157 | road | one empty collision quadtree leaf, no surfaces |
| 158-182 | paths | a 2000-unit square: four checkpoints, an eight-point resurrection path, four drone paths |

Every model draws nothing (a display list that is just G_ENDDL), so nothing visible is drawn in the race.

## 2. How far it boots

```
python3 tools/port/synth_rom.py
build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 --no-rom-check --headless --no-config --audio null \
    --no-pak --press 5:A,8:A,11:A,14:A,17:A --until-state 0x400000:300 --seconds 75
```

The host loop logs each change of the game's state word (`state_word_a`, gGameState). On x86-64 (Rocky) and aarch64
(the Pi 5) alike:

| State | Screen | Reached | Left by |
|---|---|---|---|
| 0x1 | initGameState: InitWaves, overlay "Start" (image A), SetOptions | frame 1 | itself |
| 0x2 | the start prompt (two blits, 3 s) | vi 4 | its timer |
| 0x40000000 | the controller pak warning (updateWarningScreen) | vi ~210 (3.5 s) | A |
| 0x20 | the player screen | after A | A |
| 0x40 | the mode screen (updateModeScreen) | | A |
| 0x80 | the track select (its six maps built from the paths) | | A |
| 0x100 | the car select (LoadCars, the car paint) | | A |
| 0x40000 | race start: thirteen fonts | | itself |
| 0x80000 | PrePlay: LoadTrack (track, world, road, path, race WADs), the cars' physics, InitVisuals | | itself |
| 0x400000 | the race | vi ~935 | |

In the race, the game, model (physics), audio manager, MusyX and scheduler threads all run: 60 s of a run gives
1705 graphics tasks (30 a second, the game's rate) and 3595 audio tasks, with no fault. On the mode screen alone,
20 s give 587 graphics tasks and 1197 audio tasks. The front end runs the same way. Nothing visible is drawn: the
textures are solid, the models empty, and the VI mode table is zero data (a screenshot comes out 4096 x 240, black).

The ctests: `synth_rom` writes the cartridge into the build directory (a fixture); `synth_boot_menu` runs to the mode
screen with A at 5 and 8 s and 240 frames there (`--until-state 0x40:240`, about 15 s); `synth_boot_race` takes the
mode, track and car with A at 11, 14 and 17 s and runs 300 frames of the race (`--until-state 0x400000:300`, about
28 s). With
`--seconds`, not reaching the state is exit code 1; a fault is a signal. The CI step runs the race on both Linux jobs
and keeps the log and the generator's report. Windows: the generator and the run are not wired in yet (the ctests
are `if(NOT WIN32)`); the cross job would need the data report from its build.

## 3. The stops, in order

Each stop was missing data (synthesized; section 1) or a port bug (fixed in game C or port/, N64-neutrally, with a
test). Data stops are what an empty cartridge lacks; the N64 would stop at the same places on the same bytes.

| # | Where it stopped | Why | Kind | What was done |
|---|---|---|---|---|
| 1 | AudioSfxInit: `audio_helper` faults at 0xC | zero sample-cache counts: zero-entry caches overwrite the next heap block's header | data | `D_8002C5D0`, MusyX scales |
| 2 | `_DrawText` (countdown_handler's "controller removed") faults at 0x12 | a font with no texture pages | data | font WADs and metrics |
| 3 | the renderer reads a non-canonical matrix pointer | the static display lists are zeros: the walk runs past them into other data | data | G_ENDDL in both |
| 4 | nothing happens for ever | gGameState and gNewGameState both 0 | data | gNewGameState 1 |
| 5 | the pak warning never ends | **image A's func_8038A400 (updateWarningScreen) had no C on the PC; the name was image B's worlddam entry, which updateFrontEnd called in state 0x40000000, and port/data had no function for image A's address** | **port bug** | compiled as updateWarningScreen (nonmatching draft); gen_data maps it and fails on such a clash; datacheck checks it. Plus the button map (data) |
| 6 | AllocateHandleHeap: `audio_helper` faults at 0xC | **the audio manager's fx tags were allocated 80 bytes where they are 88: every effect start wrote the last tag's data pointer over the next heap block's header** | **port bug** | the audio manager's records (below) |
| 7 | the front end's background scroller faults at 0x10 | no texture BACKGROUND_SFX | data | the textures the C names |
| 8 | the mode screen spins for ever | no mode valid for one player | data | the mode table |
| 9 | ReadPathData reads past the window | 32-byte path files | data | path files |
| 10 | the track select indexes slot -1 | MBOX_FindModel finds no AAANULLOBJ | data | the models |
| 11 | MBOX_FindTexture copies from NULL | the car select's name tables are zero | data | "" for each |
| 12 | the renderer runs off the window | a freed display list (0x7FFF0BAD): all map objects were one model, so the track select saved its own list as the "original" | data (the generator's) | a model per map object |
| 13 | LoadTrack faults | no WHDR in the world file | data | world, road, track WADs; gWorldNames |
| 14 | `strlen(NULL)` matching world nodes to targets | the targets' names are zero | data | target names |
| 15 | InitDemoPathPoint faults | no demo start points | data | the start list |
| 16 | a smashed stack in `camera_trigger_check` | the generator's road leaf had children 0xFFFF, read as an offset into the surface lists: 255 surfaces into a 36-entry stack array (the N64 does the same) | data (the generator's) | children 0 |
| 17 | `track_info_display` faults at 0x78 | no car physics descriptor | data | descriptor, tire curves |
| 18 | `handbrake_apply` on a garbage node | **camera_victory read the tires' road nodes in the model record as 8-byte pointers; they are u32 window addresses** | **port bug** | VCar's u32 fields |
| 19 | `music_seq_load` faults | gTrackColors is zero | data | colours |
| 20 | SetupCarColor (`sound_bank_unload`) faults at 0x14 | no palette by its name | data | palettes |
| 21 | `update_game_data` faults | no suspension records | data | `_tSusp` |
| 22 | the audio manager faults in its tree insert | **the sorted-tag order (draw_sprites) read the tag and its priority at N64 offsets** | **port bug** | PC offsets |
| 23 | the model thread faults at 0x10008 | **func_800E2D18 read the model's tune pointer as a host pointer** | **port bug** | u32 field |
| 24 | the model thread faults | **func_800E2C70 (m2c) loaded the car and tune pointers with `*(s8 **)`** | **port bug** | PC version |
| 25 | the model thread faults | **func_800E1500 (m2c) loaded the car pointer with `*arg0`** | **port bug** | PC version |
| | the race runs | | | |

### The port bugs and their tests

All game-C changes are `TARGET_PC` types, macros or function versions at file scope (`n64_neutral.py --base
09da2f0`: 0 FAIL).

1. **Two functions at 0x8038A400** (stop 5). Both runtime images have one. Image A's (updateWarningScreen@warningscr.c)
   had only a draft in `nonmatching/ovl_a`, so on the PC the shared name was image B's, and the front end's warning
   state called image B's code. The draft is compiled as `updateWarningScreen` (its statics are the arena's
   `D_803B839C`/`D_803B83A0`, which drawWarningScreen reads); input_aux_handler calls it; gen_data maps image A's
   address to it (`IMAGE_RENAMED`) and now fails the build on such a clash instead of listing it. Test:
   `pc_data_synthetic` (datacheck) checks each image has a function of its own at 0x8038A400.
2. **The audio manager's records** (stop 6; layout agent's area, fixed here because it blocked the boot). On the PC
   an fx tag (`tFxSequence`) is 88 bytes, a command (`tAudioMsg`) 32, a sequence 72, an emitter 56, a sorted-tag node
   24. AudioInit allocated the fx tags at 80 and the tag nodes at 12; the command allocator and AudioShutdown stepped
   the command table at 24; nine views read the records at N64 offsets (`entity_lookup`, `entity_helpers`,
   `transform` (whose change command put its floats at +8), `engine_sound_sync`, `ap.c`, `record_score`,
   `results_time_display`, the sequences in `car_stats_display`, `game_timer_pause`, `func_800D18D8`,
   `entity_flag_check`, the emitter pop in `func_800AED20`). Tests: `assets_sound` runs AudioInit, two effect
   starts, the command table and a sequence on the game's code; `views_sound_{fxtag,sequence,emitter,commands}`
   compare every live view by DWARF (`view_check.py`).
3. **IDO varargs** (found with 2). `car_stats_display` (AudioPlaySequence) defined IDO's `va_list` (a walk over the o32
   stack after the last named argument); on a host ABI the arguments are in registers, so a sequence's ids were
   garbage. It uses the compiler's on the PC. Test: `assets_sound`.
4. **A busy-wait** (found with 2). AudioShutdown waits for the audio manager to take every command without yielding:
   on the PC's one-thread-at-a-time scheduler that would hang. `PC_SPIN`.
5. **The sorted-tag order** (stop 22): `draw_sprites`, m2c, at N64 offsets. Test: `assets_sound`.
6. **The model record's pointer fields** (stops 18, 23-25): `camera_victory`, `func_800E2D18`, `func_800E2C70`,
   `func_800E1500`. The record keeps its N64 layout (CARS.md 2.2). Tests: `assets_model` (the three physics functions
   on a record in the window), `views_model_road` (camera_victory and entity_update against tModelData).

Also from this work: `--until-state MASK[:N]` and the state log (port/ultra/loop.c, ULTRA.md 4); the data loader
counts arena pointers whose target no declaration describes (`undescribed` in its report, section 4).

## 4. What the real ROM will still need

- **Data reached only through pointers.** The loader swaps what declarations describe. A table that initialized data
  points at, but no C declares, keeps the ROM's byte order: right for strings and byte tables, wrong for anything
  wider. The cartridge has three (the demo start points, s16, kept 0; the colours, bytes; the strings), and the
  race needed more that real data has: the car descriptors (CARS.md puts them inside `D_801108C8`, an f32 array,
  which would make them right), their tire curves and the suspension records (where in the ROM is unknown). The
  loader's report now lists every such pointer as `undescribed` (`PC_DATA_REPORT`, default
  `build/pc/data_report.txt`): with the ROM, read that list for tables of 16- or 32-bit values and give each a
  declaration or an `extents.txt` size.
- **The values the cartridge made up** are the first to compare: the button map, the mode table, the state words'
  start values, the MusyX cache counts and scales, gWorldNames, the target names.
- **The views of the model and car records.** The race here ran with one empty node, no surfaces and no targets, so
  the collision response, the targets, the drones, the HUD's lookups and the cameras ran little. CARS.md's audits
  missed two views found here (`camera_victory`, `func_800E2D18`) and two m2c readers; the flow-based layout check
  ([LAYOUT.md](LAYOUT.md)) finds each of them when its fix is reverted, and found one more (the brake lights'
  car slot, LAYOUT.md 4.4); what it can't follow is in its section 8.
- **Sound.** The cartridge has no sounds and no songs: the sequencer, the voices and sample DMA did not run (MUSYX_DATA.md
  7 and 8.7 stand).
- **Graphics.** Nothing visible is drawn: the VI mode table and every model are empty. The renderer ran the game's
  lists every frame without a fault, but no textured geometry.
- **Windows.** The synthetic boot runs on x86-64 and aarch64 Linux only.

## 5. Changing it

- Each value goes through `Cart.set` (checked against the data report), `Cart.set_bytes` (bytes no symbol covers),
  `Cart.string`/`Cart.blob` (in free game-image bytes), or `Cart.func_ptr`. Give each a reason: it is the report.
- A file is `Cart.file(wave, bytes, what, model=...)`; `model=True` sets the file table's model flag (the WAD
  relocation in SetupWaveModel).
- The format builders follow the converters' formats (`port/assets/*.c` and their tests), big-endian.
- When the boot stops at a new place: `RUSH_TRACE_WAVES=1` shows each file's load and conversion; the fault report's
  backtrace names the function. Ask whether the N64 would stop there on the same bytes: if so it is data, and the
  value goes in the generator with its reason; if not, it is a port bug (a layout, a width, a host pointer where the
  N64 has 4 bytes, a host ABI difference): fix it in the game C under `TARGET_PC`, with a test.
