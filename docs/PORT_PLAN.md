# Rush 2049 PC: native port plan

Draft, 2026-10-26. A native PC build of *San Francisco Rush 2049* (N64, USA), compiled from the
[decompilation](https://github.com/cabi24/SFRush2049-n64-decomp)'s C, the way the SM64 PC port was built from the
SM64 decompilation. No machine code is translated: the game's C is compiled for the PC, and a PC layer replaces
the N64 hardware and libultra. The player brings their own ROM, which supplies the game's data and assets.

## Where we start

Measured on 2026-10-26 by compiling the published decompilation (`09da2f0`) natively with gcc 12 on aarch64
(`-std=gnu89 -funsigned-char -fno-strict-aliasing -fwrapv -fcommon`):

| | Result |
|---|---|
| C files that compile | **1,342 of 1,346** (game 1,168/1,168; overlay A 39/40; overlay B 30/31; boot 105/107) |
| Why the 4 fail | 2 need a struct type from another file (`MultiBlit`, `Egg`, `tBlitDef`); 2 are libultra/libc files with IDO-only syntax (`__unaligned`, varargs), which the PC layer replaces anyway |
| 64-bit pointer warnings | **948 in 174 files** (661 pointer-to-int casts, 287 int-to-pointer). This is the main list of places that assume 4-byte pointers. |
| Symbols the C defines | 2,328, with no clashes, including between the two overlays that share 0x8038A400 |
| Symbols still undefined after linking everything | **2,393** (below) |

| Undefined | Count | What fills it |
|---|---:|---|
| Zero-initialized data (BSS), boot and game | 895 | Generated definitions with the right size: no ROM bytes needed |
| Initialized data, boot and game | 746 | Generated from the ROM by type (byte order fixed, pointers made into symbol references) |
| Overlay data and BSS | 540 | Same, per overlay |
| Functions with no C yet, game and overlays | 71 | The 19 + 8 near-miss drafts in `nonmatching/`, the 24 eight-byte stubs, the rest written fresh |
| Boot and libultra functions | 78 | The PC layer (threads, messages, video, DMA, controllers, pak, timers), zlib for inflate, libgsm for the GSM kernels |
| Names missing from the symbol file | 48 | Aliases to resolve (`gCamera`, `gControllerData`, ...) |

Other portability facts:

- Code mostly uses struct fields (about 13,600 `->field` accesses; about 45 raw `*(T*)(p + 0x..)` reads), so the
  compiler can redo layouts when pointers grow to 8 bytes. About 1,000 fields are still `unkNN`-style.
- `long` appears 159 times in function bodies. It is 32-bit on the N64 and 64-bit on Linux and macOS: it becomes
  `s32`.
- The game defines its own `main`, `memcpy`, `memset`, `memchr`, `strlen`, `strchr`, `sinf` and `cosf`. They
  are renamed or dropped in favor of the host's.
- Many game function names are "historical labels". Read each file's header comment before trusting a name.

## Architecture

```
Rush2049PC/
  src/  include/  asm/  Makefile   the decompilation, unchanged layout; the N64 matching build still works
  port/                            the PC layer (new)
    main.c                         startup, ROM loading, main loop
    ultra/                         libultra replacement: threads, message queues, VI timing, PI DMA, SI, timers
    gfx/                           display-list interpreter (F3DEX2) + OpenGL backend, SDL window
    audio/                         audio command interpreter (aspMain ABI) + SDL output
    input/  pak/                   SDL controllers and keyboard; .mpk controller pak (from v5's pak.cpp)
    assets/                        loaders that convert the ROM's big-endian files to native structs
  build/pc-data/                   data generated from the user's ROM (git-ignored)
  tools/port/                      census, data generator, import and parity tools
  CMakeLists.txt                   the PC build
```

- **One source tree, two targets** (as sm64-port did): game C stays buildable for the N64, and PC-only changes go
  under `#ifdef TARGET_PC`. The matching N64 build then proves every shared change (shared headers, data turned
  into C, renames) is byte-identical. It needs IDO, so it runs on the x86-64 Rocky host. The PC build runs
  anywhere.
- **Threads first, frame loop later.** The game runs idle, game, audio and render threads plus the libultra
  scheduler, and it busy-waits on variables other threads change (v5 found 14 such loops). Bring-up keeps the
  threads: each N64 thread is a host thread, only one runs at a time, and switches happen in the blocking libultra
  calls, as N64ModernRuntime does. The 14 loops are edited to yield. Folding everything into one frame loop, as
  sm64-port did, can come after the game runs.
- **Display lists with 64-bit pointers.** Under `TARGET_PC`, `Gfx` and the audio command type carry a
  `uintptr_t` second word, so the `gSP*`/`gDP*` macros store host pointers (872 macro uses, 225 raw word writes to
  review). Display lists loaded from ROM files are big-endian with 32-bit segment addresses. Either convert them on
  load, or have the interpreter read them in N64 form and resolve segments to host pointers; the decision belongs
  in Phase 3. The game's own display-list walkers (relocation at 0x80096734 and its neighbors) change with it.
- **Renderer**: a Fast3D-style interpreter (what sm64-port used), not RT64. RT64 needs the game's memory in N64
  layout, which a native build doesn't have. Candidate: libultraship's Fast3D (MIT; used by Ship of Harkinian and
  Starship; F3DEX2, OpenGL/D3D11/Metal, widescreen, interpolation). Check whether it can be used without the rest
  of libultraship before deciding. OpenGL also runs on this Pi 5.
- **Overlays**: both runtime images are linked in. An overlay "load" becomes resetting that overlay's data to its
  initial values and zeroing its BSS.
- **Licenses**: sm64-port and sm64ex have no license, so they are a model, not a source. libultraship is MIT,
  Starship is CC0. v5's `pak.cpp` and `sdl_frontend.cpp` are ours to reuse. Rush 2 Recompiled's code stays out
  (no license); its 2049 research docs describe the asset formats.

## Phases

| Phase | Goal | Done when | Status (2026-10-27) |
|---|---|---|---|
| **0. Seed and census** | Repo seeded, PC build compiles everything, census tool committed | `cmake --build` compiles 1,346 files into a library; `tools/port/census.py` reproduces the tables above | Done |
| **1. It links** | Every undefined symbol has a definition: BSS generated, data generated from the ROM, missing functions stubbed (abort with the name) | `rush2049pc` links and runs until the first stub | Done: 0 function stubs left |
| **2. It boots** | PC libultra: threads, message queues, VI timer events, DMA from the ROM file, controllers. Busy-waits yield. Missing functions get C. | Game code reaches the first graphics task (`osSpTaskStartGo`) without a fault | On the synthetic test cartridge (`tools/port/synth_rom.py`, [port/SYNTH.md](port/SYNTH.md)) the game runs through the whole front end into a race on an empty track, with graphics and audio tasks every frame; CI boots it on every push. Real data **needs the ROM** |
| **3. First picture** | Interpreter + OpenGL + SDL window. 64-bit `Gfx`. Pointer warnings on the boot path fixed. | Midway logo and title screen | Renderer done and tested with synthetic lists; `Gfx` widened. Needs the ROM |
| **4. Assets** | List every ROM file the game loads and its format; a converting loader per format (models, textures, track geometry, collision, AI paths, car tables, fonts) | Front-end menus, then one race | A converter for every format, run from `_ReadWave` ([port/LOADERS.md](port/LOADERS.md)): WAD, text bank, fonts, world, road, paths, MusyX. Tested with the game's own readers on synthetic files; needs the ROM to confirm |
| **5. Sound** | Audio command interpreter in C (aspMain ABI); the MusyX code is already game C; SDL output | Title music; race sound | Interpreter done (MusyX v1, not aspMain). MusyX files converted and the library's structures fixed for 64-bit; songs and voices run through the game's own sequencer in tests ([port/MUSYX_DATA.md](port/MUSYX_DATA.md)) |
| **6. Saves and input** | `.mpk` controller pak (from v5), SDL controllers and keyboard, rumble | Create Player and records survive a restart | Controller pak at the SI block level done; keyboard, up to 4 controllers, rumble and `rush2049pc.ini` ([port/FRONTEND.md](port/FRONTEND.md)). The game's file records, profiles and ghosts have their PC layouts, and its own code creates, reloads and deletes them on a test pak (POINTERS.md 9.10) |
| **7. Parity with v5** | v5 is the reference: same `--press` input scripts on both; compare state probes (game state, car positions and speeds per frame), screenshots and audio | Every mode v5's scripted runs cover | `--press` scripts work on the PC build |
| **8. Features** | Widescreen, high frame rate, settings menu, remapping, texture packs: the V6 list, now plain C edits plus our renderer | Per feature, behind a setting | Internal resolution scale (`video.scale`, 1-8 or `auto`: the window's whole multiple). Widescreen (`video.widescreen`: 16:9, 21:9, auto...) in the renderer: perspective views widen, 2D stays 4:3, full-width fills span ([port/GFX.md](port/GFX.md) "Widescreen"); tested with synthetic lists, needs the ROM to judge the menus. Settings overlay (F1, or both stick clicks): holds the game; widescreen, scale, fullscreen, vsync, window size and volume live (the graphics API and audio backend after a restart); remapping the keyboard, the controllers and the hotkeys by capturing the next key or button; Save into rush2049pc.ini in place, keeping its comments and order; drawn by port/gfx in window pixels with the port's own font ([port/FRONTEND.md](port/FRONTEND.md) "The settings overlay"); tested headless and on SDL's offscreen driver, not yet over the game's frames. High frame rate (`video.frame_rate`: 60, 120, the display's...): the game keeps its 30 fps and N64 timing, the renderer draws frames between its last two by blending their matrices, paired by what they drew; cuts, teleports, split screen, 2D, appearing objects handled; `--fast` runs made repeatable to check the timing is unchanged ([port/FRAMERATE.md](port/FRAMERATE.md)) |

## Phase 0 results (2026-10-26)

Done: the repository is seeded with the decompilation's history, and `cmake --build build/pc` compiles every
game file natively (about 40 s on the Pi 5) and links a test executable. Live numbers are in
[port/STATUS.md](port/STATUS.md) (`python3 tools/port/census.py --build build/pc`). At the end of the session:
1,346 of 1,346 files compile, 836 pointer warnings in 172 files, **0** symbols with two strong definitions, and
**1,515** symbols still undefined.

What was learned, and how the build handles it:

- **gnu89 is required.** With C99 rules, an `__inline` function without `extern` gets no out-of-line copy; IDO
  (and gnu89) emit one, and the ROM uses it (`audio_effect_process` in `heap_release`). CMake adds no `-std` flag
  when the compiler's default is newer, so the build passes `-std=gnu89` itself.
- **Non-owned copies are made weak.** Group files compile copies of other functions as context for IDO's
  interprocedural optimizer, and some files carry stand-ins; natively, 317 names were defined two to nine times.
  The manifests name each function's owner, so `tools/port/gen_weak.py` writes a per-file header of
  `#pragma weak` lines for the copies (788), and the build force-includes it. The linker then binds every call to
  the owner's definition, and the compiler won't inline a weak copy. The source is untouched. 32 names (mostly
  `__standin_*`, which nothing references) have no manifest owner, and the first copy is kept
  (`build/pc/weak/report.txt`).
- **BSS keeps the N64 layout.** `tools/port/gen_bss.py` defines all 895 boot and game BSS symbols at their N64
  offsets inside one zeroed array per region (assembler `.set` aliases), so code that indexes from one symbol into
  its neighbor still works. The 106 symbols declared with a pointer type get their own storage, since an 8-byte
  pointer would overwrite the next N64 slot. 82 of those are also declared as integers somewhere; they are the
  first places to check in Phase 2. Struct types that contain pointers are not detected yet.
- **The game's libc names are renamed.** `main`, `memcpy`, `memset`, `memchr`, `strlen`, `strchr`, `sinf` and
  `cosf` compile as `n64_*`, so the game keeps its N64 versions and the host's (SDL, GPU drivers) are untouched.
- **Link test.** `tools/port/gen_stubs.py` turns what's still missing into aborting function stubs (159) and zeroed
  data placeholders (1,351), and `rush2049pc_linktest` links and runs. It proves the scheme links; it is not the
  game.
- Two files needed struct definitions IDO didn't (arrays of an incomplete type), added under `#ifdef TARGET_PC`:
  `src/ovl_a/groups/start_screen/group.c` and `src/ovl_b/groups/hud_show/hudextra.c`. Check them with the N64
  matching build on the Rocky host.

## Second session results (2026-10-26)

Six parts were built in parallel against one interface header, [`port/include/pc_port.h`](../port/include/pc_port.h).
Each part's design, tests and open items are in its own document:

| Part | Where | Document | What it does |
|---|---|---|---|
| PC libultra and main loop | `port/ultra/`, `port/main.c`, `port/input/` | [port/ULTRA.md](port/ULTRA.md) | Each N64 thread is a host thread, and one runs at a time with N64 priorities. Message queues, events, timers, VI at 60 Hz, PI DMA from the ROM file, SI with the game's own controller pak code on a `.mpk` file, AI and RSP tasks. 54 libultra units are replaced; 12 busy-waits yield. `rush2049pc --selftest` |
| Game data | `tools/port/gen_data.py`, `port/data/` | [port/DATA.md](port/DATA.md) | Storage for every boot, game and runtime-image data symbol, laid out from the build's DWARF types. At startup the loader inflates the images from your ROM and fills each symbol by type: byte order fixed, N64 pointers made host pointers. The executable holds no ROM bytes. An overlay load resets that image's data |
| Graphics | `port/gfx/` | [port/GFX.md](port/GFX.md) | F3DEX2 (NoN fifo 2.08) on the CPU, with 4 KB TMEM, all nine texture formats the game uses, combiners and blender compiled to GLSL. GLES 3 / GL 3.1+, headless EGL, optional SDL2 window, internal resolution scale |
| Audio | `port/audio/` | [port/AUDIO.md](port/AUDIO.md) | The audio microcode is Factor 5's MusyX v1 (subframe descriptors, not an Acmd list). Interpreter, ring buffer, null, WAV, SDL2 and ALSA output |
| 64-bit types | `include/`, `src/`, `tools/port/ptr64_*.py`, `tools/port/n64_neutral.py` | [port/POINTERS.md](port/POINTERS.md) | `Gfx.w1` is pointer-sized on PC in all 444 local typedefs. Pointer warnings dropped from 836 to 392, and most of what is left is deliberate: 32-bit N64 addresses in heap headers and ROM records, in the identity-mapped RDRAM window |
| Asset survey | | [port/ASSETS.md](port/ASSETS.md) | One loader (`LoadWave`, 183 files) into the game heap, with 11 formats. The game relocates loaded records with 32-bit arithmetic |
| Speech | `port/ultra/gsm.c` | [port/ULTRA.md](port/ULTRA.md) | The three hand-written GSM decoder kernels in C, checked against their MIPS code run in an interpreter |

Key design decisions that came out of it:

- **RDRAM window.** An 8 MB window is mapped at host address 0x80000000 (`MAP_FIXED_NOREPLACE`). The game heap,
  frame buffers and the overlay region sit at their N64 addresses inside it. This is load-bearing: the game relocates
  the files it loads with 32-bit arithmetic (`+= base`, `| 0x80000000`, bit-31 tests), so those records keep 32-bit
  address fields and stay exact.
- **Host data above 4 GB.** The executable is position-independent, so any value below 4 GB is an N64 address (K0,
  physical or segmented) and anything above is a host pointer. The renderer and audio resolve addresses with that
  rule. The game's pointer-free data fits it too: it sits at its N64 addresses in the window (docs/port/DATA.md),
  so its N64 address is its host address.
- **Two display-list forms.** Lists the game builds are native (16 bytes per command, host pointers). Lists loaded
  from the ROM stay in N64 form (8 bytes, big-endian) inside registered big-endian ranges.
- **Data from the ROM at run time.** Nothing derived from the ROM is generated at build time, so the build and its
  tests run without a ROM.
- **Ceiling drafts.** The 8 standalone C drafts in `nonmatching/game/` that nothing else defines are compiled into
  the PC build. Historical labels are linked to the function the layout places at their address.

Numbers ([port/STATUS.md](port/STATUS.md)): 1,329 of 1,329 files compile, 392 pointer warnings in 93 files, **0**
function stubs, 1 undefined name (`ShowCull`, a debug flag in a draft), 34 tests passing.

How far it runs: `rush2049pc --no-rom` (a blank cartridge) runs the game's own `main`, the idle thread, `game_init`,
the scheduler, audio and render threads, heap setup in the window and `game_late_init` into AudioInitSys. It stops
in `LoadWave(6)`, where every file size read from the blank cartridge is zero. Going further needs the ROM.

## Third session results (2026-10-27)

The second wave of parallel work, all merged into `main` and checked by CI:

| Part | Where | Document | What it does |
|---|---|---|---|
| Asset converters | `port/assets/` | [port/LOADERS.md](port/LOADERS.md) | `_ReadWave` calls `pc_wave_loaded` once per file, which converts it by format: WAD containers (TOC, OBHD, TXHD, PLHD, PTHD/PATH swapped, display lists, vertices and texels left big-endian), text bank (native side tables), fonts, world (WHDR, WOBJ, GDAT, GTLD), road, paths. Each converter checks the file fits before it swaps anything |
| MusyX | `port/assets/musyx.c`, `src/rom/lib_1*.c` | [port/MUSYX_DATA.md](port/MUSYX_DATA.md) | The project, pool, sample directory and songs swapped at the width the library reads each field. The library's service callbacks, sequencer slots, voice state, DMA request list and stream state now have one layout each on 64-bit |
| Data in the window | `tools/port/gen_data.py`, `port/data/` | [port/DATA.md](port/DATA.md) | The game's pointer-free data and BSS sit at their N64 addresses inside the RDRAM window, so an N64 address of game data is its host address, and 32-bit fields can hold it. Pointer-holding symbols keep their own storage above 4 GB. Arrays that overlapped their neighbours are bounded (`player_array` is 6 cars) |
| Front-end records | about 120 game files | [port/POINTERS.md](port/POINTERS.md) §9 | One PC layout for the scene-node table, sprite list, blits and panes, the StatLL pools and the controller pak records; `tools/port/view_check.py` compares every view of a record from DWARF |
| Prototypes | `tools/port/proto_check.py` | [port/PROTOTYPES.md](port/PROTOTYPES.md) | Cross-file prototype disagreements ranked by what they cost on a 64-bit ABI: 62 harmful at the start, 2 now |
| N64 neutrality | `include/types.h`, `port/include/pc_prelude.h` | [port/POINTERS.md](port/POINTERS.md) | Every shared file passes `n64_neutral.py`: PC-only statements sit on lines the function already has, through `PC_SPIN`, `PC_ONLY` and `N64_ONLY`, which are empty or the original tokens on the N64 |
| CI | `.github/workflows/pc.yml` | | x86-64 and aarch64 Linux: build, all tests, the ROM-less boot, the census; the N64 neutrality check. A failing test is run again under gdb |

Bugs found on the way that would have broken the game on a 64-bit or little-endian host:

- **bzero, bcopy, bcmp**: the host versions take a `size_t`, but the game's prototypes declare an `int`, so through about
  900 calls the length's upper half was undefined. The game now calls 32-bit versions (`port/ultra/n64mem.c`).
- **The scheduler read each task's done queue at the wrong field**, so it would have blocked after the first
  graphics task.
- **x86-64**: game code compiled as PIE reached the absolute N64-address symbols PC-relatively; everything is PIC on
  x86-64 now, without GOT relaxation. `listener_position_set` read a variable in one call argument that another
  argument assigns, which only IDO's left-to-right evaluation defines; `-Wsequence-point` is on for game C and finds
  no other case.
- **`(char *)-1` sentinels** in ROM tables came out as 0x00000000FFFFFFFF; the loader now sign-extends small
  negatives.

Numbers: 1,341 of 1,341 files compile, 332 pointer warnings, 1 undefined name, 53 tests here (62 in CI, where SDL2
and ALSA are installed), 0 neutrality failures in 817 changed files.

How far it runs: `rush2049pc --no-rom` boots on both architectures through the game's init, the text bank, the MusyX
project, pool and sample directory, sndInit and sndPushGroup, and stops in AudioSfxInit, where the blank cartridge's
zero sample-cache count corrupts the heap (the N64 would do the same with those bytes).

## Fourth session results (2026-10-27)

A third wave of parallel work, with builds on a 20-thread x86-64 host (`tools/port/remote.sh`), all merged into
`main` and green in CI, now with Windows:

| Part | Document | What it does |
|---|---|---|
| Car, visual and model records | [port/CARS.md](port/CARS.md) | A car is 0x468 bytes on the PC (its 21 visuals and its input pointer are pointer-sized); the model records keep their N64 layout with u32 window addresses, in the arena. `tools/port/view_audit.py` checks every view against the reference layout by DWARF |
| Race-time records | [port/POINTERS.md](port/POINTERS.md) §10 | The target info table, the level-object nodes (N64 layout, 32-bit links), the input records (80 bytes), the ZOID windows and the frame records |
| Save records | POINTERS.md §9.10 | The pak file records, profiles and ghosts; heap handle slots are pointer-sized. The game's own code saves, reloads and deletes on a test pak |
| Byte order | [port/ENDIAN.md](port/ENDIAN.md) | `tools/port/endian_check.py` follows each access of the live game code through GCC's GIMPLE and DWARF; node handles read as their N64 low half, colour words, and declarations fixed; the loader copies colour unions as bytes |
| Pointer casts | [port/CASTS.md](port/CASTS.md) | Every remaining cast warning classified; 8 truncations fixed; `-DRUSH_CHECK_CASTS=ON` aborts where a host pointer is narrowed |
| Review | | A second read of the hand-written views found 5 more: the pause screen's and battle mode's node walks, the dead-reckoning view, four car strides at race start |
| Widescreen | [port/GFX.md](port/GFX.md) | `video.widescreen` (16:9, 21:9, `auto`): 3D views widen per viewport, 2D stays 4:3; `video.scale = auto` |
| Windows | [port/WINDOWS.md](port/WINDOWS.md) | mingw-w64, cross or MSYS2; the window is a file mapping at 0x80000000; CI builds it, runs the tests natively and runs the cross-built exe |
| Synthetic cartridge | [port/SYNTH.md](port/SYNTH.md) | `tools/port/synth_rom.py` generates a cartridge of original, minimal data from the layout alone (it reads no ROM). The game boots it through every front-end state into a race and runs it; on the way it found 6 port bugs (image A's warning screen bound to image B's function, the audio manager's records, IDO varargs, a busy-wait, the sprite comparator, four model record readers). CI boots it to the menu and into the race |
| Layout check | [port/LAYOUT.md](port/LAYOUT.md) | `tools/port/layout_check.py` follows every access through pointers, parameters, returns, list and tree walks and function-pointer slots to the storage it reaches, and flags views off the storage's reference layout, raw offsets inside fields, N64 strides and pointer-width mismatches. It re-finds every bug the review and the synthetic boot found; over the whole game it found 81 more, all fixed (0 open, 60 reviewed) |
| Settings overlay | [port/FRONTEND.md](port/FRONTEND.md) | F1 (or both stick clicks) pauses the game and opens video, audio and control settings, with remapping and an in-place ini save; its own 5×9 font |
| Last functions | [port/DATA.md](port/DATA.md) | C for the last 23 game and overlay functions (2,000 of 2,134 by address; the rest are boot code the PC layer replaces); each overlay's C data goes back to its initial values on every load, as on the N64 |
| N64 check | POINTERS.md "The neutrality check" | `tools/port/ido_check.sh` builds every N64 object with IDO at `09da2f0` and at HEAD: all 1,011 agree in code and data |

Also: executables are forced PIE (Red Hat's gcc links non-PIE, which put host data below 4 GB, where the port
reads N64 addresses); the self-test's timing bounds hold on a loaded host.

Numbers: 105 tests on x86-64 Linux with SDL2, 0 harmful prototype mismatches, 325 pointer warnings (all
classified), 0 neutrality failures in 987 changed files, 0 objects that differ under IDO. The synthetic cartridge
reaches a race; `--no-rom` (all zeros) still stops in AudioSfxInit, as it should.

## Fifth session results (2026-10-27)

The synthetic cartridge widened ([port/SYNTH.md](port/SYNTH.md)):

| Part | What it does |
|---|---|
| The virtual clock | `--fast`: the N64 clock moves on only while every thread waits, so a run is as fast as the host allows (the race's 28 s in about 0.4 s) and the same on any host to within a graphics task ([port/ULTRA.md](port/ULTRA.md) 1). The self-test runs on it too |
| Paths | press scripts in `tools/port/synth` say what to reach; `tools/port/synth_paths.py` runs and checks them (the state, players, windows, cars, mode and track, the states on the way, a non-NaN car, screenshots). 14 paths: the front end and its screens, races for 1-4 players in split screen, circuit, stunt, battle (two players, weapon icons as power-ups), the attract demo, driving, the pause screen and its four screens, a retirement through results and the game over screen back to the main menu, a controller pak. About 45 s in all on the build host, 36 of them the one real-time race |
| Pictures | a VI mode that shows, the screen size, the elementary functions' constants (computed), models that draw (a 4000-foot ground, car-sized boxes), flat pavement under it, the HUD's speedometer: `--screenshot-at` of a race shows the game's display lists, matrices and viewports; `synth_shot.py` checks one, two and four views |
| Tools | the state log names the players, windows, cars, mode, track, pause state and the first car's position; `RUSH_TRAP_FP` traps the first invalid floating-point operation; `RUSH_GFX_TRACE=N` prints a graphics task's commands; `index_check.py` finds initialized tables indexed past a scalar declaration |
| Windows | the synthetic paths in the native job's ctest and, with the cross-built program, in `windows-run`: all 14 pass ([port/WINDOWS.md](port/WINDOWS.md) 4) |

Port bugs found and fixed, each with a test: `effect_system_init` walked the player records up to the address of the
next symbol, which has its own storage on the PC (`assets_effects`); the car masses, the car tunes and `D_801117B4`
were indexed past their one-element declarations, so the loader would have left all but the first element in the
ROM's byte order (`index_check`). Also: gSongNames' extent (131 pointers by the gap, 12 by the code). The rest of the
40-odd stops were data the cartridge lacked, now synthesized and documented (SYNTH.md 3).

## Next

1. **The ROM.** Put `baserom.us.z64` in the repository root (git-ignored). Then:
   - `build/pc/rush2049pc_datacheck` checks the data loader on real data.
   - `RUSH_TRACE_WAVES=1 build/pc/rush2049pc --headless --stop-at-gfx`, then `--screenshot-at N`, is the bring-up
     loop: run, read the backtrace, fix. The converters log any file that doesn't parse (LOADERS.md and
     MUSYX_DATA.md list what to check first).
2. **N64 check.** `tools/port/ido_check.sh` builds every N64 object with IDO at `09da2f0` and at HEAD and compares
   code, data, relocations, symbols and section sizes. On 1575cc9 all 1,011 objects agree (234 differ only in
   `.mdebug` line numbers), and the build's other inputs (Makefile, linker scripts, asm, manifests) are unchanged,
   so the ROM still matches. A full `make` with the ROM remains the final word. Run it after each merge of game
   C; `n64_neutral.py` stays the fast check, in CI.
3. **Records on 64-bit.** The car, visual and model records, the target table and level-object nodes, the input
   records, the ZOID windows and the frame records have one PC layout each ([port/CARS.md](port/CARS.md),
   POINTERS.md §10), and so do the save records (§9.10). `tools/port/layout_check.py` ([port/LAYOUT.md](port/LAYOUT.md))
   finds no open layout disagreement in game C. Its blind spots are function tables in ROM data and call contexts:
   the synthetic cartridge's widening coverage (SYNTH.md) is the runtime check, the ROM's `undescribed` report the
   next one.
4. **Byte order** where widths disagree: `tools/port/endian_check.py` finds no open conflict in game C or in the
   loader ([port/ENDIAN.md](port/ENDIAN.md)); two reads of bytes inside a word of the car record are left there.
5. **Other platforms.**
   - **Windows x86-64 builds with mingw-w64**, cross from Linux (`cmake/mingw-w64.cmake`) or in MSYS2, and CI runs
     its tests, self-test and ROM-less boot on windows-latest ([port/WINDOWS.md](port/WINDOWS.md)).
   - What makes it work: the window is a file mapping viewed at 0x80000000 and 0xA0000000. The image stays above
     4 GiB with high-entropy ASLR. Code reaches the absolute symbols through `.refptr` slots, which get no base
     relocations.
   - Still to do on Windows: a real GPU, display, audio device and controllers.
   - macOS on arm64 reserves the low 4 GB (`__PAGEZERO`), so it would need address translation instead.

## Decisions

1. **Seed**: done, from the decompilation's history.
2. **Keep the N64 matching build working** in this repo: yes. Shared changes are checked with
   `tools/port/n64_neutral.py` here and in CI, and with `tools/port/ido_check.sh` (IDO objects, no ROM) on a host
   with IDO.
3. **Renderer**: our own Fast3D-style interpreter (`port/gfx`), written for this game.
4. **License** for `port/`: open. Nothing in it requires GPL. `port/audio` follows public reverse-engineering
   notes on MusyX v1 whose main source is GPL code (mupen64plus); no code was copied. If you want strict clean-room
   provenance, review those parts.
5. **Rush2049Recompiled**: v5 is the reference for parity testing. Its `V6_PLAN.md` is superseded by this plan.
