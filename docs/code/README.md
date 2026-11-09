# The game's code: a map

San Francisco Rush 2049's C, as this tree has it, is the N64 game decompiled: three images that the N64 loads at
fixed addresses, now compiled for the PC ([PCONLY.md](../port/PCONLY.md)). These pages map it for a programmer new to
it: what each part does, where the original source files are, the main data, how the functions call each other,
what only the N64 has. They use the names of `tools/port/names/*.tsv`, the maps that rename the tree in wave 2;
[NAMING.md](NAMING.md) has their conventions and how the six areas' names were made one set.

Start with the six subsystem pages (wave 2b, 2026-11-09): they use the tree's names and paths as they are now, and
each ends with what is still N64-shaped (for wave 3, the memory model).

| Page | Covers |
|---|---|
| [system.md](system.md) | boot, threads and the scheduler, the frame loop and the game states, the heap, asset and wave loading, the inflater, the controllers |
| [cars.md](cars.md) | the car model and the physics step, collisions, respawning, car set-up and visuals, weapons and blasts, the maths, the scene and the ZOID renderer front end |
| [race.md](race.md) | tracks, paths and checkpoints, targets and target paths, the surface tree, the race rules (laps, battle, stunt), the drones' driving, the cameras |
| [frontend.md](frontend.md) | the menus and screens (title, options, car and track select, players, records, high scores), the menu boxes, runtime image A |
| [ui.md](ui.md) | the Controller Pak saves (profiles, ghosts, records), the text engine and fonts, blits and panes, the in-race HUD |
| [audio.md](audio.md) | the sound manager and server, emitters and listeners, car sounds, songs, the MusyX sound library, speech |

The older pages below map the game image by address range. They (and NAMING.md) were written before the maps were
applied (2026-11-08, wave 2's first step) and are kept as written: where they give an old name or a file's old path beside a new name (`SaveRaceSetting` (voice_stop_2)),
it is the tree's name before `tools/port/rename.py`; `tools/port/names/applied.tsv` maps every one to the name the
tree has now.

| Page | Covers |
|---|---|
| [boot.md](boot.md) | the boot segment (`src/rom`): entry, threads, timer, scheduler, video mode, inflater, libultra, libc, the MusyX sound library, speech; the low image's data |
| [r1.md](r1.md) | game image 0x80086A50-0x8009DC50: the blitter, the serial bus and rumble, the scene objects and car visuals, the sound manager and server, the heap, the asset loader, the scene draw |
| [r2.md](r2.md) | 0x8009DC50-0x800B4720: the scene back end and windows, the overlay loader, the controller pak, deflate, the world and the road tree, car start-up, skids, blits, panes, fonts |
| [r3.md](r3.md) | 0x800B4720-0x800CD798: options and pause, the text engine, sky and cameras, paths and checkpoints, tyres, texture animation, stunts, road collision, the game state, profiles and ghosts |
| [r4.md](r4.md) | 0x800CD798-0x800E847C: the car simulation (collision, road, respawning, the physics step, the drivetrain, the drones' driving), car sounds, the option screens, pak errors, the model thread, the heap |
| [r5.md](r5.md) | 0x800E847C to the end of the image and its data: the main loop and race flow, the frame pipeline, the camera, the HUD, records and unlocks, the screens' animators, track objects, the MusyX glue |
| [overlays.md](overlays.md) | the two runtime images: A, the front end (every menu screen), and B, the battle/stunt HUD extras |
| [port-addresses.md](port-addresses.md) | every N64 address the port's own code (`port/`, `tools/`) still spells, and what replaces it |
| [NAMING.md](NAMING.md) | the naming conventions, every disagreement between the areas and its decision, aliases, the labels to drop, what wave 2 does by hand |
| [The headers](#the-headers-includegame) (below) | `include/game`: the declarations every game file shares, and what a file still declares itself |

## How the program runs

The boot code's `main` starts libultra and the main thread (`main_thread_entry`), which creates the scheduler
(`gScheduler`), inflates the game image from the cartridge, and starts the other threads: `model_thread` (the car
simulation, `ModelThread`, once a retrace), `bg_load_thread` (the background asset loader, `BGLoadWaveMain`) and
`game_thread` (`GameLoop` forever); the game image adds the audio threads (`audio_retrace_thread`,
`sample_dma_thread`, `audio_manager_thread`) and the Rumble Pak's (`rumble_thread`). The main thread then serves
the scheduler: each retrace it may start a frame (`StartGraphics`) and wake the game thread on `game_thread_queue`;
the game thread builds the frame's display lists and asks for the frame's end (`UpdateFrame`, `EndGraphics`).

`GameLoop` dispatches on the game state (`gGameState`): the front end (image A loaded, one screen's update function
a frame), or a race (`UpdateGame`: loading the track and cars, the countdown, `PlayGame` each frame, `EndGame`), with
the HUD drawn as blits whose animation functions run each frame. The cars move on the model thread (`UpdateModel` and
the physics step `sym`), and the game thread copies the results into the car records (`update_game_data`). Sound
goes through the game's sound manager (`AudPlayEffect` and the emitters and listeners) to the sound server and the
MusyX library in the boot segment. On the PC the threads are host threads, the RSP's display lists go to the
renderer in `port/gfx` and the audio microcode is interpreted by `port/audio`.

## The images, in address order

**The boot segment** (0x80000400, [boot.md](boot.md)): code to 0x800277D0 (entry and threads, frame timer,
scheduler, video mode, decompression, libultra, libc, MusyX: hardware layer, registries, synth, macros, sequencer,
streams and 3D; speech and the GSM decoder), then the microcode, initialized data and BSS to 0x80086A50.

**The game image** (0x80086A50). IDO compiled the whole game as one program, so its functions lie in runs of one
original file, often in reverse source order, with callees before their callers and an 8-byte stub where an inlined
static was; the runs, merged from the five area pages:

| Range | Original files (MAT3) | What | Page |
|---|---|---|---|
| 0x80086A50-0x8008AD04 | blit_pc.c (N64 side), controlsys.c's SI lock | the 2D blitter; the serial-bus lock, the Rumble Pak thread, two sound threads | r1 |
| 0x8008AD04-0x80094888 | mbox.c, zoid.c, zoidpoly.c, zoidtexture.c, fmath.c, atan.c, smoke.c, visuals.c, audiomgr.c | the scene-object tree, vector maths, every car visual (sparks, lights, tyres, wings, blast), smoke and spray, the effect calls | r1 |
| 0x800947F0-0x80099B30 | TB_text.c, targets.c, save.c, zoidblit.c, audiomgr.c, audiosfx.c, the N64 forms of the PS2 audio files, memsys.c, waves.c, mb_wad.c, bstree.c | small services; the sound manager and server; the heap; the asset (wave) loader | r1 |
| 0x80099B30-0x8009EBB8 | N64 scene draw, asincos.c, fmath.c | the per-node model draw, matrices, lookats; the scene walk | r1, r2 |
| 0x8009EBB8-0x800A13C4 | world.c, N64 back end, N64 overlay loader | windows and frame end; loading and unloading the runtime images | r2 |
| 0x800A13C4-0x800A80D0 | paksysdc.c, libc, blit_pc.c, waves.c, audiosysps2.c, text.c, memsys.c, blit.c, zoidwindow.c, zoid*.c | the controller pak system; start-up of blits, waves, audio and text; heap compaction; windows, split screen, fog; object setters | r2 |
| 0x800A80D0-0x800AB18C | zip.c (zlib deflate) | ghost compression | r2 |
| 0x800AB18C-0x800B2CB4 | world.c, stree.c, mbox.c, targets.c, waves.c, audiomgr.c, audio.c, visuals.c, skids.c | the world and the road quadtree; track objects; the background loader; emitters; car start-up (StartCar, CreateCar); skid marks | r2 |
| 0x800B2CB4-0x800B4720 | stree.c, targets.c, blit.c, pane.c, HAL_text.c, TB_text.c | collision objects; blits and panes; fonts | r2 |
| 0x800B4720-0x800B8C14 | save.c, pausescr.c, state.c, front.c, videoscr.c, audio.c, controlsys.c, countdown.c, HAL_text.c, TB_text.c, sky.c, camera.c | option saving, pause, backgrounds, the video screen, the text engine, sky, cameras | r3 |
| 0x800B90F8-0x800C1A00 | tracks.c, smoke.c, pathcommon.c, maxpath.c, checkpoint.c, carload.c, targets.c, tires.c, textureanim.c, targetpaths.c, audiomgr.c, stree.c | track and car loading, paths and checkpoints, tyre forces, texture animation, moving targets, emitters | r3 |
| 0x800C1B60-0x800C6AA0 | stunt.c, stree.c, maxpath.c, resurrect.c, hud.c | stunt detection; car-against-road collision (CollideCarBody, RoadPosition); respawn points | r3 |
| 0x800C70BC-0x800CD798 | state.c, save.c, gamerecords.c, bstree.c, options.c, memsys.c, controls.c | the game state machine (front end, overlays, waves), profiles and ghosts on the pak, options | r3 |
| 0x800CD798-0x800D50E4 | save.c, collision.c, road.c, resurrect.c, mdrive.c, drivsym.c, initiali.c, audiomgr.c, checkpoint.c, gamerecords.c, hud.c, maxpath.c, rekon.c | profile settings; car-to-car collision, road contact, respawning, car set-up; sequences; laps and places; dead reckoning | r4 |
| 0x800D52C4-0x800E0B20 | audiomgr.c, audioscr.c, controlscr.c, optionscr.c, pausescr.c, password.c, pane.c, pakerr.c, options.c, audio.c | listeners; the audio, controls, options and pause screens; passwords; pak error dialogs; car sounds | r4 |
| 0x800E0B20-0x800E847C | drivsym.c, drivetra.c, maxpath.c, mdrive.c, ghost.c, controls.c, model.c, controlsys.c, memsys.c | the physics step, the drivetrain, the drones' driving, `UpdateModel`, ghosts, pads to controls, the model thread, the heap | r4 |
| 0x800E847C-0x800EF5AF | mdrive.c, camera.c, maxpath.c, HAL_text.c, gfx.c, Init.c, hud.c | the model results to the cars; the car cameras; the frame pipeline (StartGraphics, EndGraphics, the main thread's loop); start-up | r5 |
| 0x800EF5B0-0x800F8D9B | hud.c, highscorescr.c, gamerecords.c, circuit.c, ghost.c, blit.c, gfx.c, gameoverscr.c | the HUD, high scores, unlocks, circuits, game over | r5 |
| 0x800F8D9C-0x800FECA3 | game.c with camera.c, checkpoint.c, drones.c, countdown.c, front.c, demo.c; fmath.c, controls.c, circuit.c, audio.c | `GameLoop` and the race flow; the N64 audio manager | r5 |
| 0x800FECA4-0x8010FD7C | the screens' animators (audioscr, controlscr, demo, gameoverscr, highscorescr, hud, optionscr, pausescr, videoscr), targets.c, the MusyX services | functions reached only through pointers: blit animators, track-object functions, the library's eight hooks | r5 |
| 0x8010FD7C-0x8017A640 | data and BSS of all of the above | grouped per original file, not in code order | r1-r5 |

**The runtime images** (0x8038A400, [overlays.md](overlays.md)): image A, the front end (title, licence and pak
warning, start, mode, track, car, player, records, circuit, cheat menus, the pak manager: MAT3 `*scr.c`, `eggs.c`,
`selectcarload.c`), and image B, the HUD extras of battle, stunt and obstacle races (`worlddam.c`, `weapons.c`,
`streaks.c`, `shots.c`, `powerups.c`, `hudextra.c`). One of them is loaded at a time into one heap block; on the PC
both are linked in and their data are arenas.

## The headers (include/game)

Every game file under `src/blob`, `src/ovl_a`, `src/ovl_b` and `nonmatching/` starts with `#include "game/game.h"`:
one declaration of each type, datum and function the files share (the boot segment's `src/rom` keeps its own,
`rom_tu.h` and `include/PR`). Until 2026-11-08 each file carried instead the context the decompiler (m2c) wrote,
thousands of lines of typedefs, structs and externs, most of them unused ([PCONLY.md](../port/PCONLY.md) 4,
"Wave 2: headers").

| Header | Holds |
|---|---|
| `game.h` | the umbrella: what follows, in order |
| `basic.h` | `s8` ... `u64`, `f32`, `f64`, their volatile forms; `EXTENT` |
| `types.h` | libultra's types first (`OSThread`, `OSMesgQueue`, `OSTask`, `Gfx` ...), then the game's structures, grouped by the subsystem that uses each first, every member with its offset; and the structures the headers only point to (`struct Car;`) |
| `system.h`, `memory.h`, `maths.h`, `audio.h`, `scene.h`, `blit.h`, `text.h`, `input.h`, `save.h`, `track.h`, `cars.h`, `physics.h`, `race.h`, `camera.h`, `hud.h`, `frontend.h` | each subsystem's data (in N64 address order) and its functions' prototypes (by name) |
| `m2c.h` | what the decompiler's code still uses: `M2C_FIELD` (a field by raw offset), `M2C_UNK`, `M2C_ERROR` |
| `view.h` | `VIEW`: a file's own view of a shared datum |

**Data.** One `extern` per datum, with the type the data loader lays it out by: the type `gen_data.py` chose from
the files' declarations until then (DATA.md 1), so the tables came out the same. An array's extent is
`EXTENT(n)`: the data loader's generator sees it (`port/data/game_decls.c`, the headers compiled alone), the game's
files see the array open-ended as they always did, since much of the C indexes a table past the end it declares.
A datum a game file defines (a tentative definition with a type of its own, 14 of them) is declared only in
`game_decls.c`, for the data loader.

**Functions.** Each function a game file defines, with its definition's prototype (the parameter names too). A
parameter that points to a structure only its own file defines is `struct Name *`, the structure declared but not
defined in `types.h`; the defining file's own definition completes it. Left to their files' own declarations, for
the per-area cleanup: a function that returns a pointer to such a structure (each caller reads the result through
its own view: 33), takes or returns a structure by value (4), whose callers pass another number of arguments or
whose file keeps its own type of a name in the prototype (15: `AddStunt`, `dotireforce`, `setFBCollisionForce`,
`MBOX_FindPalette_WORK`, `sound_set_volume`, `sound_key_off`, `AudioSongStop`, `CountdownTimeOut`,
`relocate_texture_loads`, `musyx_submit_audio_task`, `sound_seq_stop`,
`zoid_load_palette`, `zoid_load_texture`, `zoid_draw_object`, `Traverse`), where a caller passes a 32-bit integer
for a pointer, which the prototype would sign-extend where the file's own declaration passed it zero-extended (20:
`RemoveBlit`, `ZOID_SetTexture`, `AudEmitEffect`, `ZOID_AddObject`, `PS_WriteFile`, `point_at_car`,
`ChangePaneSize`, `ZOID_UpdatePoly`, `ZOID_RemovePoly`, `ShowPakError`, `SetupTarget`, `SavePlayerOption`,
`lzss_decompress`, `inflate_decompress`, `GetPlayerOption`, `GetPasswordString`, `fixword`, `enginetorque`,
`CompressGhost`, `AudUpdateEmitter`); the C library
the build renames `n64_*`.

**Types.** The structures the shared data and prototypes need, printed from the compiler's own record of them
(DWARF), so each lays out as it did. A structure keeps the name the files gave it when no file uses that name for
another type; else it has a new one (`tMenuButton`, `ScreenButton`, `UVMatrixRows`, `OSThreadBytes` ... the MAT3 name
where the layout is MAT3's, else what it is). Structures that lay out alike are one type.

**What a file keeps.** Its code, untouched; the types only it uses (a struct with just the fields it reads, the
decompiler's padded views); declarations of what the headers don't declare; and two kinds of lines after the
include:

- a **view**, where the file reads a shared datum through another type than the headers': `VIEW(Cam152[], gCamera);`
  then `#define gCamera gCamera_view`. VIEW declares `gCamera_view` with the file's type and the datum's own symbol
  (an assembler label), so the code compiles exactly as with the file's old declaration (1,764 views);
- a **rename**, where the file's own type has a name the headers give another type: `#define Gfx GfxView   /* this
  file's own Gfx: include/game's Gfx is another type */` (150 of them, mostly libultra's types the decompiler
  wrote as opaque words).

**Tools.** `tools/port/to_headers.py --build build/pc --apply` converts a file (re-runnable: a converted file has
nothing left to convert; its docstring has the rules); `tools/port/objsame.py BASE NEW` says which units compile to
other code than before (`docs/port/headers-units.txt` lists the conversion's, each a call through the definition's
prototype, and why each is harmless); `gen_data.py --decls` lays the data out by the canonical declarations. The
headers were written once by a generator from the base tree's DWARF and the MAT3 decomp's file map; they are kept
by hand now: a new shared datum or function goes into its subsystem's header (an array with `EXTENT`), a new type
into `types.h`. A subsystem was given by the MAT3 source file a function or datum comes from, else by its address
neighbours or the files that use it: some are in the wrong place (`sTrackSongs` in `physics.h`), to be moved as
the per-area cleanup reads them.
