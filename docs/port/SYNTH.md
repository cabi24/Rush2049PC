# The synthetic test cartridge (tools/port/synth_rom.py)

Written 2026-10-27. Without the ROM, `rush2049pc --no-rom` boots a blank cartridge, which ends in AudioSfxInit:
most of the game's code had never run on the PC, and the 64-bit bugs left were in that code. The synthetic cartridge
is a ROM image of original, minimal data, built from the decompilation's knowledge of where the game reads its data
and in what format. On it, the game boots through the front end's screens and along each of its paths: races for one
to four players in split screen, the circuit, stunt and battle modes, the attract demo (section 2). Getting there
found port bugs in eight places (section 3): a function the PC had linked to the wrong runtime image, the audio
manager's records, IDO varargs, a busy-wait, four readers of the car model record, a walk bounded by the next
symbol's address, and tables indexed past their declarations. Each is fixed with a regression test.

| | |
|---|---|
| Generator | `tools/port/synth_rom.py` (Python 3, stdlib) |
| Output | `build/synth/rush2049_synth.z64` (12 MiB) and `rush2049_synth.txt` (every value and file, and why) |
| Needs | the PC build's data report, `build/pc/pc-data/data_symbols.txt`: build the port first |
| Run | `build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 --no-rom-check --headless --no-config --audio null --no-pak` |
| Paths | `tools/port/synth/*.press`, run by `tools/port/synth_paths.py` (section 2) |
| Checks | ctest `synth_rom` and a `synth_boot_<path>` per path; CI step "Boot the synthetic cartridge" (both Linux jobs) |

**Provenance note (2026-10-27).** While the paths were widened, a research helper of the agent doing it decoded
the retail control and button-map tables from a local ROM, against the project's rules. Nothing from it was used:
the button map was already in the generator before that (from the libultra button masks and ControllerUpdate's
loop), and the control table is the formula `1 << k` for k < 19, else k, from how the car's readers use it. Those
commits were reviewed before the merge; the ROM was not copied or committed.

**No ROM bytes.** The generator reads no ROM and copies nothing from one. Its inputs are the image table
(`tools/port/n64syms.py`), the symbol files and `layout/*.json` (function addresses), the build's data report
(addresses and element kinds, from the build's DWARF), the decompiled C (names it looks up, record layouts) and the
format documents ([ASSETS.md](ASSETS.md), [LOADERS.md](LOADERS.md), [MUSYX_DATA.md](MUSYX_DATA.md)). Constants with a
mathematical meaning (the VI registers, the elementary functions' approximants) are computed from that meaning. The
images are zero except about 920 values, each listed with its reason; the files are as small as their formats allow,
with solid-colour textures, untextured boxes and tiles for models, and no sounds. The output is under `build/`, which version control ignores: never commit it. Its SHA-1 is
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

**The values** (`make()`, in six parts):

| Part | Symbols | Why |
|---|---|---|
| boot | `D_8002C5D0` (sample cache entries per voice: 2), `D_8002D8B8`..`D_8002D8C8` (MusyX buffer scales: 1.0) | zero caches corrupt the heap in AudioSfxInit (MUSYX_DATA.md 8.6) |
| | `D_8011EAAC` | the MusyX service table: the eight functions (MUSYX_DATA.md 8.1) |
| | `D_8011F0A0` | the sfx groups pushed at boot (12-15), which the project has |
| | `D_8002E4A0`, `D_8002E528` | the two static display lists: G_ENDDL (a list of zeros runs off its end) |
| | `state_word_b` (gNewGameState) | 1: the first frame runs initGameState; 0 is a game with no state |
| | `gSkipToGameState` | 1, no screen skipped: a screen whose state isn't this one passes straight on (0 skipped the start prompt and the main menu) |
| math | `gSinCoeffs`, `gCosCoeffs` and their reductions (boot, f64) | libultra's sinf and cosf: reduction by pi (1/pi, pi in a 21-bit and a rest part), a sine polynomial P1-P4 fitted by least squares here (2e-8), 0 past 2^28. Zero gave sin x = x, cos x = -abs(x) |
| | `gOrthoScale` | guPerspectiveF's degrees to radians, pi/180 |
| | `D_80123B98`..`D_80123BC4` | tanf (func_800A557C, Cody and Waite's structure): ymax, 2/pi, pi/2 as 201/128 and the rest, 2^-12, and the [7/8] Pade approximant from Lambert's continued fraction (5e-16). Zero gave tan x = 0: the projection's 0.5 / tan divided by 0 |
| | `D_801238C0`..`D_80123900` | atan (func_8008C5E0, C680, C720) and atan2 (C768): the [4/5] Pade approximant of atan t / t in t^2 with a monic denominator (one constant term for both, as the code has it; 1e-14), tan(pi/8), tan(3pi/8), pi/4, +-pi/2, +-pi |
| | `D_8011F010[4]` | asin and acos (func_8009C3F8): {0, pi/4, pi/2, pi/4}, the offsets its two ranges need |
| | `D_80123888`, `D_8012394C`, `D_80123F80`, `D_80123E04/08` | vector helpers' floors (1e-10, 1e-6), a gimbal epsilon (1e-5), the tire slip ratio's slope past 400 (1/400, continuous) |
| display | `D_8002AFC0`, `D_8002AFC4` | the screen, 320 x 220: a framebuffer block is 0x22620 bytes, 32 more than that at 16 bits. Zero was a colour image 4096 wide (its width field is width - 1) |
| | `gViModeTableBase[0]` | VI mode 0 (NTSC, get_tv_offset), an LPN1 mode computed from what the VI registers mean (`vi_mode_ntsc_lpn1`): display_update divided 0 by 0. Its yScale is what display_update makes of 220 lines (`(220 x 1024/240 + 16.5) & ~15`): display_mode_tick shows the picture (osViBlack(0)) only when they agree |
| | `gViewportFloatA`..`D` | xScale per pixel (1024/640), yScale per line (1024/240), a PAL and an NTSC retrace in seconds (1/50, 1/60: gScaleSecondsPerTick, the game's clock) |
| front end | `D_8011FAD4` | the button map (game bit j = N64 buttons): none means no button ever works. Made up from the code: bits 0-2 Start, A, B (what the prompts take), 10-13 the d-pad (what the stick makes) |
| | `D_801118E4[26]` | gControlTable: SetControls makes each control option byte k the mask table[k], which the car's readers test against the game buttons; 0x13-0x19 they take as analog sources. So k < 19 is game button k and 19-25 are those codes (gas A, brake B, steering the stick with the code-set default controls) |
| | `D_803B65E4[10]` | the main menu's valid options: 0-4 (race, then four screens); none loops forever in updateStartScreen |
| | `D_803B4679` rows 1-4 | the modes valid per player count: all; none loops forever in updateModeScreen |
| | `D_80116D14[14]` | the options screen's valid items: all; none loops forever in options_initScreen (from the main menu or the pause screen) |
| | `D_80110030[12]` | gSongNames: "SONG 1".. (the audio screen copies each) |
| | `D_803B7738[20]` | the track select's map objects: a name each (it swaps their LODs and puts them back) |
| | `D_8011B3A0`, `D_8011B400`, `D_8011B404`, `D_8011B438` | the car select's texture, model and sheen names: "" (a NULL name faults) |
| race | `D_8011A924[19]` | gWorldNames: "WORLD" |
| | `D_801202F8[9]` | the gravity options' factors (gravity = 32.2 x): 1 each. Zero is a weightless car whose mass is 0, and the first physics step divides 0 by 0 (func_800E1540) |
| | `D_80110DD8[13]`, `D_80111274`, `D_80124150`, `D_801116D0[3]` | a car's mass (func_800D0B14): 93 slugs per type (3000 lb), no mass-option difference, the three tunes' factors 1 |
| | `D_80110E0C[14]` | a car type's weight in pounds, the model's +0x5C4, which the car-car collision shares its push by: 3000 |
| | `D_801240FC`, `D_80124100`, `D_80124104` | the push apart of two overlapping cars (menu_load_options): 1e-4, 2000, 2000; the six cars start on one spot here |
| | `D_80117530[122]` (+0) | the targets' names: TARGET001.. (NULL faults; "" would match every world node) |
| | `D_80117530[0]` | the weapon icon, WEAPON: StartWeaponIcon, flags 0x4028 (made without the Expansion Pak, a category record, the start function when made), shape 350, category 5. The battle arenas have four |
| | `D_80117510[5]` | category 5's record size, 136 (Slot136): battle mode writes 16 of them; 0 is a zero-size heap block and the writes go over the next blocks' headers |
| | `D_80121D60[8]`, `D_8011F844[13]` | the weapons' ammunition (5) and the bodies' collision boxes (4.5, 4.5, 2.5, 3: the tires are 4 ahead and behind, 2.5 aside) |
| | `D_80117480[6]` | the arenas' puff textures: "" (physics_collision_test looks them up from track 6 on) |
| | `D_80111754[6]` | a gRace 2 race's time limit on tracks 0-5 (func_800FBED8): 15 s. Zero ended the race in its first frame |
| | `D_80120E74[12]` | the drones' update schedule (update_drone_models): row r is {drones, -1}, the race using row n for n drones and n + 7 with two humans or more. With zeros the walk never met -1 and went on through the tables after it, updating cars those values named; a row asking for more drones than there are updates players' cars with a time step of 0: NaN |
| | `D_80110DA4[13]` | a car type's brake factor: 1 (zero is a car without brakes) |
| | `D_801173D8[12]`, `D_80117408[12]` | the demo paths' start points: one, point 0 |
| | `D_80110D08[13]`, `D_801108C8` | the cars' physics descriptor (0xB4 words at 0x801108D0, where CARS.md puts them): mass, inertia, tire positions, four tire curves (the records `D_8011F814`, below) |
| | `D_80111560[3]` | carSuspensions: one `_tSusp` (nine f32), in the same f32 table |
| | `D_8011AF90[19]` | gTrackColors: sixteen greys |
| | `D_801158DC[0, 1, 4..7]`, `D_80115A68` | the HUD set (ShowHUD): a BUTTON panel, a sprite whose draw function is the digital speedometer, animSpeedText (gShowSpeed is option 5, 1 by default), with its digits' place per view (speedPos), and the four players' crossbones (animBones, which only deaths mode shows). With zeros every HUD blit had no texture or size |
| battle | `D_80394330[5]`, `D_80394344[5]` (image B) | the weapons' texture names: "" (func_803908D0 looks each up); their model names stay NULL (AAANULLOBJ) |

**From the retail cartridge's bring-up** ([BRINGUP.md](BRINGUP.md)): data at the places the retail cartridge showed
to matter, each the regression test of a fix there, synthetic as the rest. The places (addresses, sizes, which tables
strings follow) came from looking at the retail layout; the values, names and strings are the cartridge's own, or
the game C's (BUTTON) and zlib's (extra_blbits) constants. The cartridge's own texture names are `SYN_FOG` ("SYF",
which fits `D_803B9260`'s 4 bytes) and `SYN_BAR`.

| Values | Why (BRINGUP.md section) |
|---|---|
| NewMultiBlit tables declared without their layout: the demo's blit (a NULL name), the game over title, the controls screens' first buttons, the menu backgrounds (createBG), the video and audio screens' second blits (BUTTON blits) | the generator's kind check needs the PC layout, and a second blit inside its table (1, 9) |
| the car types' names (`SYNC00`..) after `D_8011F914`, the songs' after `D_8011F0A0`, a car select blit's name ("") after `D_803B83A4` (`strings_after`) | strings where the images keep them, which a table sized by the gap would cover: the generator refuses (2) |
| the tire curves in `D_8011F814[2][6]` (the descriptors' four `tire_info` words point there) | the records only those u32 fields reach, declared in car_resistance (3) |
| `state_word_a` (gGameState) -1, the game's own "no state" (func_800B7FF8 tests for it) | a start value `--until-state` must not take for a state (6) |
| option 14 valid in a single race, `D_803B9260` the name of `SYN_FOG`, an IA8 texture | the track select's fog blit and its palette word (7) |
| track 0's palette animation and object animation (two frames), in bytes no symbol covers | tables only pointers reach, converted by `port/data/reached.txt` (8) |
| battle HUD: an IA blit (`SYN_FOG`) and player 1's health bar (`SYN_BAR`, I8, animHealth) | the health bar's palette (10) |
| `D_80110EBC`, every gear ratio row: 1 | the eight-row gear table (11) |
| `D_803B470C`, every player's model slots: name 1, id -1 | the four-player slot table (12) |
| the HUD set's four crossbones blits (animBones, BUTTON) and `D_801160C8`, their places in deaths mode for every view | the crossbones' place table, declared whole (15) |
| the idle overlay's blit `D_801146C4` (no name, callback_init) and its colour `D_801146FC` as bytes; the `idle` path | the colour callback at the Blit's host offsets, and the colour declared as bytes (15) |

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
| 120-138 | world WADs | WHDR naming one world, one WOBJ node (TRACK), one empty GDAT leaf, no GTLD keys; the battle arenas' (tracks 6-13) four more nodes, WEAPON00-03, siblings of TRACK at the corners of a 1000-unit square, which become weapon icons |
| 139-157 | road | flat pavement at y = 0, x and z from -1000 to 3000 feet: one tree node whose four quarters list one surface (type 0, an identity orientation, four vertices; `road_file` says what each reader takes) |
| 158-182 | paths | a 2000-unit square: four checkpoints, an eight-point resurrection path, four drone paths |

**The models** (`model_objs`): each has a display list and vertices of its own (SetupWaveModel relocates every
model's lists in place, so two models sharing one list would have its addresses relocated twice). A list sets the
state it draws with (one-cycle, the shade combiner, an opaque z-buffered surface, no texture, smooth shading without
fog or lighting, back faces culled), then loads its vertices eight quads at a time and draws them with G_TRI2. The
faces wind clockwise seen from outside in the world's axes: the scene's modelview negates x, which makes them front
faces on the screen (the ground is seen from above, a box not from inside). Vertices are in
sixteenths of a foot (the scene's matrices and the projection's near and far are x16). The world node's model TRACK
is the ground: 16 x 16 tiles of 250 feet, a checker of greens with the tiles under the square loop grey, centred on
the node, which the world puts at the loop's middle. Every other model (AAANULLOBJ, which the game takes for a NULL or
empty name: the cars, the buttons, the weapon icons; MINEG1; the map objects) is a car-sized box, 9 x 3 x 5 feet with a
colour per face (the front red, the top yellow).

## 2. How far it boots: the paths

```
python3 tools/port/synth_rom.py
python3 tools/port/synth_paths.py [NAME ...]          # every tools/port/synth/*.press, or those named
build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 --no-rom-check --headless --no-config --audio null \
    --no-pak --fast --press-file tools/port/synth/race.press --until-state 0x400000:300 --seconds 75
```

The host loop logs each change of the game's state word (`state_word_a`, gGameState), with the players, windows,
cars, mode (gRace) and track, and the first car's position and speed. The front end, on x86-64 (Rocky) and aarch64
(the Pi 5) alike:

| State | Screen | Reached | Left by |
|---|---|---|---|
| 0x1 | initGameState: InitWaves, overlay "Start" (image A), SetOptions | frame 1 | itself |
| 0x2 | the start prompt (two blits, 3 s) | vi 4 | its timer |
| 0x40000000 | the controller pak warning (updateWarningScreen) | vi ~210 (3.5 s) | A |
| 0x4 | "press start" | 5 s | START; after 5 s the attract demo (0x8) |
| 0x10 | the main menu (updateStartScreen): the race, four screens; the player count (LEFT/RIGHT, up to the controllers connected) | 8 s | A |
| 0x20 | the player screen: each player readies | | A on each port |
| 0x40 | the mode screen (updateModeScreen): gRace 0-6, mode 5 locked | | DOWN, A |
| 0x4000 | the circuit screen (gRace 3) | | A |
| 0x80 | the track select (its six maps built from the paths) | | A |
| 0x100 | the car select (LoadCars, the car paint, each player's car) | | A on each port |
| 0x40000 | race start: thirteen fonts; gRace 4-6 load image B ("Extra") | | itself |
| 0x80000 | PrePlay: LoadTrack (track, world, road, path, race WADs, the targets), the cars' physics, InitVisuals | | itself |
| 0x200000 | the countdown (battle) | | itself |
| 0x400000 | the race | 23.6 s | |

**The paths.** Each `tools/port/synth/NAME.press` is a `--press-file` script whose leading comments say what the run
must reach (`# until:`, `# seconds:`, `# expect:` the state line's text, `# visits:` states on the way, `# realtime`,
`# args:`, `# check:`);
`tools/port/synth_paths.py` runs them (on the virtual clock, `--fast`, unless `# realtime`), checks the exit code and
the last state line (and that the first car's position isn't NaN), and keeps each log in `build/synth/paths`. ctest has one `synth_boot_NAME` per script, and the
CI step runs them all on both Linux jobs and keeps the logs. On Windows the native job's ctest runs them, and
`windows-run` generates the cartridge from the cross build's data report and runs every path with the cross-built
program ([WINDOWS.md](WINDOWS.md) 9).

| Path | What it does | Reaches | Time (Rocky) |
|---|---|---|---|
| `menu` | the front end to the mode screen | 0x40, 240 frames | 0.2 s |
| `race` | one player, a single race (gRace 0), track 0, real time | 0x400000, 300 frames: 1 player, 6 cars (5 drones) | 34 s |
| `race2p`, `race3p`, `race4p` | 2, 3, 4 players, split screen | 2 players 6 cars; 3 and 4 players, as many cars | 0.4-0.7 s |
| `circuit` | gRace 3, through the circuit screen | 6 cars | 0.4 s |
| `stunt` | gRace 4 in the first stunt arena (track 14), image B | 1 car | 0.4 s |
| `battle2p` | gRace 6, two players in the first battle arena (track 6), image B: weapons, the four weapon icons as power-ups | 2 cars, 4 power-up slots | 0.4 s |
| `drive` | the one-player race with gas, steering and brake for 30 s; a screenshot (below) | | 0.5 s |
| `pause` | the race paused (START), the four screens the pause menu opens (options, controls, audio, video), then quit, confirmed: the race is retired, its results and the game over screen | 0x800000 | 0.5 s |
| `screens` | the main menu's other four options, each screen entered with A and left with B (`# visits:` checks the states 0x2000, 0x200, 0x800, 0x1000) | back on the main menu | 0.3 s |
| `pak` | the one-player race with a fresh controller pak (`--pak`): the warning finds it, and the save code runs on it at the race's start | 0x400000, 300 frames | 0.4 s |
| `raceend` | the race retired from the pause screen, then _EndGame, the results, the game over screen, its Quit and back to the main menu (`# visits:` the results, the game over screen and state 4). gRace 2's time limit would end a race by itself once the car has stopped, which the cartridge's car physics doesn't do | 0x10 after the race | 0.6 s |
| `demo` | no START at the prompt: the attract demo (0x8), a 30 s race of the drones, then the prompt again | 0x8, 900 frames: 6 cars | 0.3 s |
| `idle` | the front end to the mode screen, then no button for 320 s of the virtual clock: game_loop's idle overlay over the screen, which callback_init gives its colour ([BRINGUP.md](BRINGUP.md) 15) | 0x40, 9600 frames | 1.9 s |

**Screenshots.** `--screenshot-at N` of a race now shows the game's own display lists drawn: its matrices and
viewports put the models where the cars are, in one view or split in two (top and bottom) or four (quarters).
`drive`, `race2p` and `race4p` take one and check it (`# check:` runs `tools/port/synth_shot.py SHOT VIEWS`): the
picture is the game's 320 x 220, each view the split screen should have shows drawing (a share of non-black pixels),
and the borders between them stay dark. The HUD's speedometer draws its three digits in each view. The menus are
still black: their buttons and backgrounds come from
MultiBlit tables and button records that are zero.

The races run every thread (game, model, audio manager, MusyX, scheduler) with no fault. Earlier, nothing was drawn:
the textures are solid and the models empty. The physics turn the cars' positions into NaN (section 4).

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
| 26 | the main menu never shows | gSkipToGameState 0: every screen whose state isn't it passes on, so the start prompt and the main menu went straight to the player screen | data | 1 |
| 27 | the main menu spins | no valid option (`while (!optionValid(i)) i++`) | data | `D_803B65E4` |
| 28 | stunt and battle: LoadTrack (physics_collision_test) copies a NULL name | the arenas' puff textures | data | `D_80117480` |
| 29 | stunt and battle: `effect_system_init` faults at 0x80800000 | **the walk over the player records stopped at the address of D_80156BA8, which has storage of its own on the PC: it cleared the arena to the window's end** | **port bug** | the records' end under TARGET_PC |
| 30 | battle: `audio_helper` faults at 0x100000007 in LoadTrack | engine_sound_update writes battle's 16 power-up slots (136 bytes each) into a block of D_80117510[5] x 16 bytes, 0: over the next heap blocks' headers | data | `D_80117510[5]` |
| 31 | battle: func_803908D0 copies a NULL name | the weapons' texture names (image B) | data | `D_80394330`, `D_80394344` |
| 32 | the car select: sfx_stop copies a NULL name | the cars' sheens; reached once the retrace clock ran (the VI floats) | data | `D_8011B438` |
| 33 | battle: the race hangs (the game thread computes for ever) | UpdateBattleHealth picks a power-up with `while (slot not free) i++`: the arena had none | data | the weapon icon target and four icons per arena |
| 34 | (with `RUSH_TRAP_FP`) display_update divides 0 by 0 | the VI mode table is zero | data | an NTSC mode, the screen size, the VI floats |

| 35 | screenshots black, 320 x 220 | the picture is never shown: display_mode_tick unblacks the VI only when its computed yScale matches the mode's | data | the mode's yScale from the game's formula |
| 36 | (with `RUSH_TRAP_FP`, the projection) 0.5 / tan(fov / 2) divides by 0 | tanf's constants; then sinf and cosf (guPerspective), atan | data | the math constants, computed |
| 37 | every car's position is NaN from the first physics step | gravity 0: mass 0, and func_800E1540 divides 0 by 0 | data | the gravity factors, the cars' masses and tunes |
| 38 | the car-car collision (menu_options_screen) and the push (menu_load_options) divide 0 by 0 | no weights; the push's constants | data | weights, push constants |
| 39 | (looking for 37) | **the cars' masses D_80110DD8, the tunes D_801116D0 and D_801117B4 are indexed past their one-element declarations, so the loader converted only the first element and left the others in the ROM's byte order** | **port bug** | declarations of their size, an extent; `index_check` |
| | the cars drive without NaN (they fall: the road has no surfaces) | | | |
| | the cars fall for ever | no road surface under the tires: every lookup misses | data | the flat pavement |
| 40 | (with models) SetupWaveModel faults in the vertex chain walk at 0x6 | **the generator's**: the models shared one display list, which each model's relocation moved again | data (the generator's) | lists of their own |
| 41 | the pause screen's options screen spins | no valid option (`while (!valid[i]) i++`) | data | `D_80116D14` |
| 42 | the pause screen's audio screen copies a NULL name | gSongNames; and its declaration ran by the gap to 131 pointers (the loader would have made the 119 words after the 12 names pointer slots), so `extents.txt` gives it 12 | data; extent | `D_80110030` |
| 43 | gRace 2's car, then the 3- and 4-player races' and battle's first car, go NaN | update_drone_models walked off its zero schedule, then (with a schedule) row n + 7 for two humans or more asked for drones that weren't there and updated the players' cars with a time step of 0 | data | `D_80120E74` |

Stops 26-34 came with the paths of section 2 (`--fast`, the virtual clock, makes each a second's run), 35-39 with
looking for what makes the picture and the physics: the state log's car position, `RUSH_TRAP_FP` (which also stops at
harmless divisions the N64 does too, such as guLookAtF on the zeroed cameras of the start prompt), and gdb
watchpoints on the model record's fields with a NaN condition.

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
7. **A walk bounded by the next symbol** (stop 29). `effect_system_init` clears the player records D_801569B8 four
   0x7C-byte records at a time `while (p != &D_80156BA8)`, the symbol after them on the N64. With pointers in it,
   D_80156BA8 has storage of its own on the PC, so the walk never met it. Under TARGET_PC its name means the records'
   end. Test: `assets_effects` runs the function on records with a guard after them. The same kind of walk in
   `ambient_sounds_clear` (D_80114628 to D_80114638) ended only because both have own storage, in address order and
   adjacent (gen_data's order), which an optimized build's compiler didn't keep: it now ends at the handles' end
   under TARGET_PC too ([OPTIMIZED.md](OPTIMIZED.md) 2.2); `sound_handles_clear` has a PC version.
8. **Tables indexed past their declaration** (stop 39). m2c declared the car type's mass as one `f32` and indexes it,
   `(&D_80110DD8)[type]`, and the same for the three car tunes (`((F2C *)&D_801116D4)[tune]`, D_801116D0 declared as
   one Tune44) and D_801117B4 (`(&D_801117B4)[arg0]`, s32). The loader converts each symbol by its declaration over
   its extent and copies the bytes no symbol covers as they are, so on the ROM 12 of the 13 masses, two of the three
   tunes and three of the four words would have read back byte-swapped. Under TARGET_PC the two scalars are declared
   as the tables they are (`extern f32 D_80110DD8[13]; #define D_80110DD8 D_80110DD8[0]`, so the code reads as
   before), and `extents.txt` gives D_801116D0 its three records. Test: ctest `index_check`
   (`tools/port/index_check.py`) lists every initialized symbol the C indexes past a scalar whose extent in the data
   report is one element with bytes no symbol covers after it: there were these three, and it fails on a new one.

Also from this work: `--until-state MASK[:N]` and the state log (port/ultra/loop.c, ULTRA.md 4); the data loader
counts arena pointers whose target no declaration describes (`undescribed` in its report, section 4).

## 4. What the real ROM will still need

- **Data reached only through pointers.** The loader swaps what declarations describe. A table that initialized data
  points at, but no C declares, keeps the ROM's byte order: right for strings and byte tables, wrong for anything
  wider. The cartridge has some (the demo start points, s16, kept 0; the colours, bytes; the strings), and the race
  needed more that real data has: the car descriptors (CARS.md puts them inside `D_801108C8`, an f32 array, which
  would make them right), their tire curves and the suspension records (where in the ROM is unknown). The loader's
  report lists every such pointer as `undescribed` (`PC_DATA_REPORT`, default `build/pc/data_report.txt`): with the
  ROM, read that list for tables of 16- or 32-bit values and give each a declaration or an `extents.txt` size.
- **Tables indexed past a scalar** are covered by `index_check` (section 3, port bug 8). gSongNames' 12 entries in
  `extents.txt` come from the code; check what the 488 bytes after them, which no declaration describes, hold.
- **The values the cartridge made up** are the first to compare: the button map and the control table, the mode,
  main menu and options tables, the state words' start values (gSkipToGameState 1), the MusyX cache counts and
  scales, gWorldNames, the target names and the weapon icon's record, the VI mode, the screen size (320 x 220), the
  math constants (computed approximants: the ROM's may differ in the last bits), the gravity factors, car masses,
  weights and tunes, the drone schedule, the HUD entries.
- **Car physics.** The cars no longer go NaN and sit on the road, but they don't really drive: the torque curve,
  gear ratios, tire curves (all 1 here), suspension, drag and the start grid are zero or made up, so gas moves
  nothing much, the six cars start on one spot and push each other apart, and gRace 2's car never stops (its race
  then never ends by its time limit). The physics code ran with values on every path; with the ROM, watch the state
  log's car position for NaN first (`synth_paths.py` fails on it).
- **The views of the model and car records.** The races here ran with one flat surface and a few targets, so the
  collision with walls, most targets, the cameras' special cases and the HUD's lookups ran little. CARS.md's audits
  missed two views found here (`camera_victory`, `func_800E2D18`) and two m2c readers; the flow-based layout check
  ([LAYOUT.md](LAYOUT.md)) finds each of them when its fix is reverted, and found one more (the brake lights'
  car slot, LAYOUT.md 4.4); what it can't follow is in its section 8.
- **Sound.** The cartridge has no sounds and no songs: the sequencer, the voices and sample DMA did not run (MUSYX_DATA.md
  7 and 8.7 stand).
- **Graphics.** The models are untextured boxes and a checker of tiles with their own render state, and the menus'
  blit tables and button records are zero (black menus): texture loads, the combiners the real models set, fog,
  lighting, the front end's 3D buttons and 2D panes, and mirroring have not been drawn on the synthetic cartridge.
- **Windows.** The paths run in both Windows CI jobs (section 2). Their first run there faulted at once, on an
  argument the Microsoft x64 convention passes in a stack slot ([WINDOWS.md](WINDOWS.md) 4). Now all 14 pass there,
  to the same states, in times within the Linux runners' range.

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
