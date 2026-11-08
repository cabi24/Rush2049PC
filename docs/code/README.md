# The game's code: a map

San Francisco Rush 2049's C, as this tree has it, is the N64 game decompiled: three images that the N64 loads at
fixed addresses, now compiled for the PC ([PCONLY.md](../port/PCONLY.md)). These pages map it for a programmer new to
it: what each part does, where the original source files are, the main data, how the functions call each other,
what only the N64 has. They use the names of `tools/port/names/*.tsv`, the maps that rename the tree in wave 2;
[NAMING.md](NAMING.md) has their conventions and how the six areas' names were made one set.

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
