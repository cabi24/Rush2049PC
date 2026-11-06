# Online play: racing a friend over the internet (plan)

Written 2026-11-05 against c8692ce. Line numbers are from that commit.

**Status (2026-11-06):** the user took the recommendations of section 7 and asked for the work to start, with one
emphasis: "robust, put up with lower and higher latencies, and stay in sync" (section 11). Phase 0 is in progress.
**The network library is written and tested on its own** ([NETWORK.md](NETWORK.md): `port/net`, its wire format, the
delay controller, time sync, `RUSH_SIM_NET` and the proofs against a toy simulation); the game doesn't link it yet.
Sections 1-10 are the plan as written; section 11 is what robustness adds to it.

**Phase 0.3 and 0.4 (2026-11-06, [LOCKSTEP.md](LOCKSTEP.md)):** the pads gate and its source API for `port/net`,
`--record-inputs`, `--replay-inputs` and `--input-delay` (with schedules), the state hash with host pointers
normalised and `--log-hash`, the desync report, the mutation check, `--session-rules` (`pc_gfx_hud_dx` 0), the
render-values check, `powf` at 60 replaced, and the settings matrix, which found `audio.latency` changing the game's
state (1.5's table has it as free). Two processes agree on every hash on all 62 synthetic paths; Windows against Linux
is wired in CI (`windows-replay`) and has not run yet.

- **Phase 0.1 and 0.2 done** (branch `net-clock`): paced mode (`--paced`, [ULTRA.md](ULTRA.md) 10), the virtual clock
  with each move at its wall time, `pc_time_virtual()` split into "deterministic" and `pc_time_free()` ("nothing is
  real time"), holds and lags re-based, the time-sync API (`pc_pace_set_start`, `pc_pace_adjust_ns`,
  `pc_pace_shift_ns`, `pc_pace_info`, `pc_port.h`); and the audio's AI on the N64 clock with the device fed from a
  tap ([AUDIO.md](AUDIO.md) 11). Every synthetic path takes `--fast`'s course to the bit in paced mode, under the
  timing models, a loaded host, a hold and a stopped process too; 60.000 retraces a second of wall time (CTest
  `paced_same_*`, `paced_model_*`, `paced_realtime`, `paced_audio`, `paced_slow_host`, `paced_load`, `paced_hold`,
  `paced_stop`, `ultra_selftest_paced`; the results in ULTRA.md 10). For the input gate and the time sync: a gate's
  wait at the retrace is a lag the pacing catches up (up to two retraces) or re-bases (`pc_pace_info` says which);
  `pc_pace_set_start`, `pc_pace_adjust_ns`, `pc_pace_shift_ns` (thread-safe) move the map.

The request: "Begin planning the online play feature. I want to race with a buddy." The user and a friend each sit at
their own PC (Windows or Linux, x86-64) and race each other over the internet. Ideally each of them sees their own car
full screen.

## 0. The short answer

**Recommendation: deterministic lockstep from boot, delay-based, over UDP, on the virtual clock that `--fast` already
uses, paced to real time.**

- Both PCs run the same two-player game: players 2, windows 2, the game's own split-screen race. They start from the
  same boot with the same ROM, settings and controller paks.
- At every retrace, each PC sends its own controller's state for a retrace a few retraces ahead (the input delay D).
  Neither PC starts a retrace until it has both players' input for it. With the same inputs, the same build and the
  virtual clock, both games take the same course to the bit. That is the property `synth_compare.py` and CI's
  `windows-same` already prove for scripted input.
- No game state is ever sent, only inputs (4 bytes a player a retrace) and, for desync checks, a hash.
- **First release:** both players see the game's split screen. **Next:** an "own view" mode in the renderer that
  draws only the local player's pane, full screen. The game itself doesn't change, so determinism doesn't change.
- **Connecting:** Tailscale first (the user already uses it). Then a host address with port forwarding. Later, UDP
  hole punching through a small rendezvous server, with a relay as the fallback.
- **Not rollback.** The port can't save and restore its state: every N64 thread is a host thread stopped in the middle
  of a call (section 1.6). Delay-based lockstep needs no savestates.
- **The first step:** phase 0 (section 5), with no network yet:
  - the paced virtual clock;
  - input record and replay files that CI can play on the Windows build;
  - a per-retrace state hash.
  Then the user's own retail replay compared across Windows and Linux.

| Question | Answer in one line | Section |
|---|---|---|
| 1. Deterministic? | Yes on the virtual clock, within a host and across Linux, Windows (msvcrt and UCRT) and aarch64, as proved on the synthetic cartridge; no in real time. Unproven on retail driving across OSes, and `game.frame_rate = 60` uses the host's `powf` | 1.1 |
| 2. Time | Logic per frame depends on the retraces a frame took, so in real time a loaded host steps differently. Lockstep needs the virtual clock (a frame is always 2 retraces, or 1 at 60), paced to the wall clock and gated on input | 1.2 |
| 3. Input | Latched once a retrace by the main loop (`pc_si_set_pads`) before the VI event, and read by the model thread's ControllerUpdate every retrace. Inject the merged pads there. The input for retrace N must be known before retrace N's event goes out | 1.3 |
| 4. Views | Per-window logic (cameras, effects, scene walk, listeners) draws from the shared RNG. Both PCs must run the same windows. Own view must be render-only: feasible but the least certain part | 1.4 |
| 5. Session | Lockstep from boot, with a pre-boot handshake: versions, ROM SHA-1, settings that the game reads, pak images, delay and start time. Menus are navigated together | 1.5 |
| 6. Sync | Delay-based lockstep. Rollback isn't possible without savestates. Hash curated state every retrace, and the whole window once a second | 1.6 |
| 7. Network | UDP, plain sockets plus a Winsock shim, about 10 message types, 4 bytes a player a retrace, inputs sent redundantly. Tailscale first | 1.7 |
| 8. Output | The audio's AI on the virtual clock, with the device fed from a tap. Rumble and overlay local only. The pause key and F1 must not hold the game | 1.8 |
| 9. Tests | Two processes over loopback, the same trace as a single-process replay with the same delay. Replay files that CI runs on Windows. A desync fuzzer, a network simulator (`RUSH_SIM_NET`), then two real PCs | 5, 8 |

## 1. What the code says

### 1.1 Determinism

**On the virtual clock it is deterministic, and that is proved.**

- One N64 thread runs at a time: they pass a baton inside libultra calls, and events never take it from a computing
  thread (ULTRA.md 1; `port/ultra/sched.c`).
- With `--fast` the main loop does its work (RSP tasks, timers, the AI, retraces) only while every N64 thread waits.
  `wait_main_virtual` (`port/ultra/sched.c:786-819`) waits for `system_idle()` and only then moves the clock
  (`pc_time_advance`, `port/ultra/timer.c:51-55`). "A run is then ... the same every run, on any host" (ULTRA.md 1).
- `--fast` also changes the devices:
  - Audio tasks run on the main loop, not at once on the scheduler's thread (`port/ultra/rcp.c:63-82`).
  - Graphics tasks never yield (`osSpTaskYield`, `rcp.c:85-95`).
  - AI events come from the main loop only (`ai_pulled`, `rcp.c:263-270`).
  - The null output drains on the N64 clock (`port/main.c:460-464, 1203-1206`).
- **Proof that exists:**
  - `synth_compare.py --same` runs every synthetic path with Debug and RelWithDebInfo, twice each, and compares the
    traces: states at their retraces, and the first car's position and speed as exact hex floats (OPTIMIZED.md 4-5).
    The result: "the same trace, every run, every path". x86-64 and aarch64 are the same, path for path. The retail
    cartridge is the same on the Pi across runs and builds.
  - `windows-same` (WINDOWS.md 12) compares 59 path logs between the Windows cross build (mingw-w64, msvcrt), Linux
    x86-64, Linux aarch64 and the native MSYS2 build (UCRT's math, Debug). All 59 are the same, to the bit, with each
    exact float read by its value (`exact_floats` in `tools/port/synth_compare.py`, since the UCRT's printf spells
    hex floats differently). The 10 live-input runs are the same line for line.
- **Why x86-64 Windows and Linux agree** (WINDOWS.md 12, OPTIMIZED.md 2.3, 3):
  - SSE arithmetic on both, and no x87 in the game code.
  - `-ffp-contract=off` for the game C (`CMakeLists.txt:165`): fused multiply-adds moved the car on x86-64-v3 and
    aarch64 before it.
  - `-fwrapv -fno-strict-aliasing -funsigned-char` (`CMakeLists.txt:161`).
  - The release targets plain x86-64, which has no FMA anyway.
  - The game uses its own `sinf` and `cosf` (`n64_sinf`, `n64_cosf`). It calls the host's `sqrtf` (correctly rounded
    on both) and `modff` (which differs only in the sign of a zero fraction of a negative whole number, and is used
    only for the field of view).

**In real time it is not deterministic.**

- "The real-time race (whose runs differ among themselves on any host)" (WINDOWS.md 12, first table). The causes are
  in the code:
  - Retraces come from the host's clock while a game thread may be computing.
  - Audio tasks run at once on the scheduler's thread (`rcp.c:66-76`).
  - AI events come from the audio device's thread after each pull (`rcp.c:263-270`).
  - Graphics tasks yield to audio tasks (`rcp.c:85-110`).
  - Late retraces are caught up, or dropped after a stall (`port/ultra/loop.c:688-716`).
- Experiment E7 (section 2) shows two real-time runs of one script diverging.
- **So online play must run on the virtual clock.** Section 1.2 says how it can still keep real time.

**The random numbers.**

- The game uses libc's LCG on one seed, `D_8011735C` (`x = x * 1103515245 + 12345`). It is inlined in many places:
  `src/blob/world_object_destroy.c:48`, `src/blob/hud_render.c:168`, `src/blob/func_800EB90C.c:156`,
  `src/blob/entity_update_callback.c:104`, `src/blob/groups/blast/blast.c:135`,
  `src/blob/groups/audio_options_menu/func_800D6160.c:33-34`.
- Nothing seeds it from time: there is no assignment other than the LCG step, and no `srand`. Its start value is ROM
  data.
- Both threads draw from it. The game thread draws for its effects (24.5 million draws in the 106 coverage runs). The
  model thread draws 5,059 times: a battle resurrection's offset (`resurrect.c:505`) and sounds (FRAMERATE.md 7.4).
- Other draws decide game state: the circuit's first grid is random (`update_game/init_state_continue.c:105-116`,
  MORECARS.md 1.2), and so is a race's song when the music option is random (`func_800D6160.c:26-43`, the table entry -1).
- The order of draws is fixed by the baton on the virtual clock, so the stream is the same on both PCs only if both
  run exactly the same game code with the same inputs. One extra draw on one PC (another window, another setting)
  shifts every later number. This is the constraint behind section 1.4.

**Other sources of time.**

- In the game C, `osGetTime` and `osGetCount` appear only in libultra:
  - the scheduler's profiling (`src/rom/lib_1050_sdk.c:189, 197, 421`);
  - `osContInit`'s wait until 0.5 s after boot (`src/rom/lib_a050.c:26`);
  - a repaired pak's ID (`src/rom/lib_f700.c:54`).
- On the virtual clock these read virtual time, so they are deterministic. A new pak's ID is a constant
  (`port/ultra/pakfile.c:69, 94-105`).

**Threads that aren't N64 threads.** The pak writer (`pakwrite.c`), the audio device's thread and SDL's threads touch
no game state, except for one thing: with a real device, the AI's queue level is the device's (section 1.8).

**Frame rates.**

- `game.frame_rate` is part of the simulation:
  - `PC_SWAP_RETRACES` (`src/rom/lib_1050_sdk.c:93, 263, 340, 362`);
  - 98 per-frame sites (FRAMERATE.md 7.3);
  - with drones, the physics differ between 30 and 60 (FRAMERATE.md 7.4-7.5).
- It must match on both PCs, and it does so through its latch (`pc_fps60`, FRAMERATE.md 7.1).
- `video.frame_rate` (frames drawn between the game's) is render-only. Every state line is the same with it off and
  at 120 (`hfr_timing`, FRAMERATE.md 4-5), and "the ticks post nothing to the game" (ULTRA.md 4). It can differ
  between the two PCs.
- **A risk at 60:** `pc_fps_filter` uses the host's `powf` (`port/config/game_fps.c:134-139`). glibc's, msvcrt's and
  the UCRT's `powf` are not guaranteed to round alike. At 60 on the virtual clock every frame is one retrace, so the
  scale is always 0.5, and `sqrtf(e)` would give the same value on every host. The follow camera already does this
  (`PC_CAM_ELASTIC`, `src/blob/func_800E8D50.c:43`, `__builtin_sqrtf`). `windows-same` runs the paths at original, so
  the 60 case has no cross-OS proof yet.

**The renderer can't feed back into the game.**

- It writes no RDRAM. "Offscreen color images get their own target. Nothing reads one back as a texture yet" (GFX.md
  "Backend, frame and settings").
- Culling with G_CULLDL is the renderer's own decision about what to draw. It doesn't change the game.
- **One exception goes the other way.** One render value flows into the game C. `car_lights_render` places the coin
  icons, and the battle and stunt HUD's models, at `HUD_X(x) = x + pc_gfx_hud_dx(x)` (`src/blob/car_lights_render.c:47-62`).
  That value comes from `video.widescreen`, `video.hud` and the frame width that the renderer publishes at each
  color image (GFX.md "The HUD at the screen's edges"). Those models are only drawn. But their places are in game
  memory, they differ between PCs whose screens differ, and they change when a window is resized. A session must
  give `pc_gfx_hud_dx` 0 to the game (and move them in the renderer instead), or leave them out of the hash.

**What is not proved.**

- The synthetic car "doesn't follow the controls". So steering into a wall, explosions, the real sound bank and other
  retail-only paths have never been compared across OSes: "That the Windows build takes the same course there rests
  on the reasons above" (WINDOWS.md 12, "What this can't show").
- A real GPU driver changing the floating-point state of the threads (denormals) is untested. The N64 threads aren't
  the GL thread, so the risk is small.

### 1.2 Time

**How the port drives the clock.**

- The main loop keeps a 60 Hz grid of retraces (`loop.c:543-` `pc_ultra_run`). At each retrace it does these steps, in
  order (`loop.c:633-647`):
  1. polls window events and input;
  2. sets the pads;
  3. sends the VI event;
  4. presents a swapped frame.
- The scheduler swaps a frame only two retraces after the last swap. `__scHandleRetrace`, `__scTaskReady` and
  `__scExecTask` test `retraceCount - __osScSwapCount >= PC_SWAP_RETRACES` (`lib_1050_sdk.c:263, 340, 362`).
  FRAMERATE.md 1 says how the main thread starts a frame only at a retrace message.
- The model thread runs once per retrace. ControllerUpdate, then model_iteration
  (`src/blob/groups/modelthread/modelthread.c:63`) steps the cars by ticks × 1/60.
- Everything else is per frame and scaled by whole retraces. `viUpdateTime` sets `gViFrameDelta` to the retraces since
  the last frame (`src/rom/lib_1050.c:74`).
- In real time, a late retrace is caught up and a stall drops retraces (`loop.c:688-716`). With vsync the retrace grid
  can be moved once (FRAMERATE.md 2). With `--fast` none of this happens: the clock jumps to the next deadline when
  the system is idle.

**Does the logic depend on how many retraces a frame took? Yes.**

- The per-frame logic steps by the frame's retraces, and some of it with constants tuned to a frame of two. The
  follow camera's elasticity is multiplied by 0.75, 0.6 or 0.5 for frames of 3, 4 or 5 retraces
  (`src/blob/func_800E8D50.c:56`, FRAMERATE.md 1).
- These decisions are made once a frame, so a longer frame makes them at other retraces (FRAMERATE.md 7.4):
  - DoDrones (the drones' boost and brake, each human's catch-up);
  - the checkpoints, and where a car is resurrected;
  - the stuck detector;
  - battle shots.
- So in real time a loaded host (a slow present, a hitch) takes a different course. FRAMERATE.md 7.6 shows the
  model and game threads falling out of phase on the Pi in real time. Experiment E4 (section 2) shows a slow host on
  the virtual clock taking a different course.
- On the virtual clock rendering and computing take no N64 time. A frame's length then no longer depends on the
  host: 2 retraces at original and 1 at 60. Only the game's own waits (timers, loads, the busy-waits' 1 ms naps) take
  N64 time, the same amount on every host.
  - FRAMERATE.md 1: "150 graphics tasks per 300 retraces".
  - E1: 1,369 graphics tasks in 2,763 retraces over the whole run, front end and loads included.
  - E5, at 60: 2,133 in 2,161.

**What lockstep needs: a paced virtual clock.**

- The N64 clock stays virtual (`pc_time_set_virtual(1)` with everything `--fast` does on the device side). It moves to
  its next deadline only when all three are true:
  - (a) every N64 thread waits (as now);
  - (b) the wall clock has reached that deadline, with the deadline mapped to the session's start;
  - (c) when the deadline is a retrace, both players' inputs for it are known.
- A host that falls behind then slows the game in wall time, and lockstep makes the other PC wait too. The game
  never takes a different number of retraces for a frame.
- This is about 30 lines in `wait_main_virtual` (sleep until the deadline's wall time, with the lock released and
  still woken by `pc_os_wake_main`), plus a gate at the retrace in `loop.c`.
- The bigger change is in `loop.c`. About 20 tests of `pc_time_virtual()` there mean two things that this mode must
  separate:
  - "the N64 side is deterministic" (keep);
  - "nothing is real time" (the presentation, vsync pacing, `sim_swap`, frames between).
  `pc_time_virtual()` needs to become two predicates.
- **The cost:** the main loop's work (graphics tasks, audio tasks) no longer overlaps the game threads' computing in
  the way real time lets it. Per frame, the work must fit in 33 ms at original (16.7 ms at 60) on the slower PC.
  - The synthetic race runs about 70 times faster than real time on rocky (ULTRA.md 1: 28 s in 0.4 s).
  - E1: 46 s of game time in 1.7 s on the Pi 5, about 27 times real time. That is the synthetic cartridge's light
    scenes, headless; retail scenes cost more (FRAMERATE.md 5).
  - Retail on a desktop PC should fit easily at original.
  - The Pi 5 can't keep `game.frame_rate = 60` with 12 cars even in real time (FRAMERATE.md 7.7).

### 1.3 Input

**The path a press takes.**

- At each retrace (`loop.c:633-641`) the main loop does three things:
  1. `pc_gfx_poll_events()`;
  2. `pc_input_poll(pads)` (`port/input/input.c:327-359`): live SDL input, then scripted presses taken at
     `pc_host_ns()` (virtual time), the `connected` flags, and idle pads while the overlay is open;
  3. `pc_si_set_pads(pads)` (`port/ultra/si.c:406-414`), which copies the pads under the SI lock.
  It then sends the VI event (`loop.c:647`).
- The game reads all four controllers in the model thread, once a retrace. ControllerUpdate
  (`src/blob/ControllerUpdate.c:73-127`, called from `modelthread.c:63`):
  - runs `osContStartReadData`, then `osContGetReadData`, which runs the PIF commands in `si.c`;
  - maps the buttons through `D_8011FAD4` into `gControllerData[i]`;
  - every 30th call queries status and accessories (`osContStartQuery`, lines 75-83), which is how the game sees a
    controller or pak come and go.
- The menus, the cars' controls (`D_8013FED0`, FRAMERATE.md 7.4) and the ghosts all take their input from that read.

**Where to inject remote input.** Replace `pads[]` between `pc_input_poll` and `pc_si_set_pads`
(`loop.c:640-641`), at the moment every N64 thread waits. The pads then hold the confirmed input of both players for
retrace N:

- the local player's own input, sampled D retraces earlier;
- the remote player's input, from the network.

**When it must be known.** Before retrace N's VI event, because the model thread reads the pads right after it. No
earlier: nothing reads the SI between retraces except through the same latched pads.

**What to send.** The final N64 pad per port: `buttons` (u16), `stick_x` and `stick_y` (s8) (`PcPad`,
`port/include/pc_port.h:250-256`), plus a connected bit: 4 bytes a player a retrace.

- Everything that shapes input happens before that point, on the sending PC: the dead zone, sensitivity, the
  octagonal gate, smooth keyboard steering (`port/input/stick.c`, FRONTEND.md "Controls"), and the triggers as
  buttons. So `[input]` settings can differ between the PCs.
- This also removes the one cross-OS input difference WINDOWS.md 12 names (`hypot` in `pc_stick_shape`).

**Fixed for a session.**

- **Connected ports.** Controller 1 is always connected; another port only while a pad is assigned to it
  (`input.c:334-335`). In a session, the ports of the session's players are connected for the whole session and the
  others never are.
- **Accessories.** `auto` makes a rumble pak appear and disappear with the local pad (`si.c` `update()`, ULTRA.md
  "Accessories"), so it would differ between the PCs. A session fixes each port's accessory explicitly.
- **Pak contents.** The game reads them at boot and at hot swaps, so they must be the same images on both PCs
  (section 1.5).

### 1.4 Views

**How the game sets them up.**

- PrePlay sets `gNumWindows = gameplay_mode == 2 ? 1 : active_player_count`
  (`nonmatching/game/display_list_flush.c:373`) and calls `wheel_render_full(gNumWindows)`.
- `wheel_render_full` (`src/blob/wheel_render_full.c:50-90`) gives two players two panes, each 3/4 of the width and
  half the height (240 × 110), at x = 2, top and bottom, with the unscaled field of view (`D_80154188`, 90). A 78-pixel
  panel is left at the right (GFX.md "Widescreen", the table of views).
- The synthetic paths' expect lines record this: `players 1, windows 1, cars 1` for `solo`, and
  `players 2, windows 2, cars 6` for `race2p` (`tools/port/synth/race2p.press:4`).
- The car counts (MORECARS.md 1) are separate: human players 1-4, drones up to 11 with `game.cars`, roster slots.

**The game's logic is per window, not only its drawing.**

- Per window (MORECARS.md 2.4):
  - cameras (`gCamera`, `D_801526A8`, `D_801526E0` ... with the follow camera's per-frame filter);
  - the HUD (`D_80142740` ..., `gRankedPlayer`);
  - the effects' emitters (`src/blob/Effects_UpdateEmitters.c:3819`, a loop over `gNumWindows`), and another such
    loop at `src/blob/func_800B90F8.c:3806`.
- Per view:
  - the scene walk (`func_8009F058(i, &gCamera[i])` for each window, `src/blob/groups/particle_knot/group.c:1613-1614`);
  - the per-view hide mask and the split-screen LOD cuts (DETAIL.md 1.2, 2: level objects with metadata 0x100 get no
    scene node with 2-4 players);
  - the radar and the battle arrows per viewer (`src/blob/func_80108F40.c:125, 160`;
    `animate_arrow/group.c:156,174`);
  - the sound listeners (`listener_position_set`).
- The effects draw from the shared RNG every frame (1.1). So a PC that runs one window where the other runs two
  draws a different number of random numbers, and its stream diverges. That changes:
  - battle resurrections (the model thread's draw);
  - the next circuit grid and a random song choice;
  - scene node pools;
  - the audio manager's state.
- **Therefore both PCs must run the same windows, and the per-player difference has to live in the renderer.**

**Render-only own view: feasible, with unknowns.**

- A frame draws each window's 3D in turn (`group.c:1613-1614`), then sets a full-screen scissor and draws every
  sprite, which is the HUD for all windows (`group.c:1616-1650`).
- The renderer would:
  1. **Tell the views apart.** By the viewport each 3D draw is under (MOVEMEM VIEWPORT). The frames-between code
     already pairs transforms per view (GFX.md "High frame rate").
  2. **Draw only the local view's 3D, across the whole target.** Re-aspect its projection: the widescreen code
     already scales clip x per view ("the projection's aspect grows by the view's widening", GFX.md "Widescreen").
     The same in y gives a taller view. The game's frustum culling is G_CULLDL, which the renderer evaluates itself
     ("done by the renderer against the widened frustum", DETAIL.md 1.2). The world's visibility tables depend only on
     the eye's position (DETAIL.md 1.1). So a taller frustum should mostly not show holes. **Unknown:** the
     horizontal survey found no game-side culling by field of view (GFX.md "Widescreen", "The game's own culling"),
     but no vertical survey has been done.
  3. **Sort the 2D by rectangle.** Sprites inside the local pane are moved and scaled, or anchored as
     `video.hud = edges` does for one view (GFX.md "The HUD at the screen's edges"). Sprites inside the other pane are
     dropped. The two-player side panel (minimap) goes to a side or is dropped. 2D that tracks 3D (markers over
     cars) needs the same transform as the 3D. GFX.md says per-view HUD rules for split screen are "not done".
- Cost: the game still builds both views' display lists. The renderer draws about half the 3D, so it is cheaper than
  split screen.
- **Audio stays the split-screen mix** (both cars' engines and both listeners, as on a couch). Making it per player
  would change the audio manager, which is game state.

**Rejected: one window per PC, with the other car remote-controlled.** The game binds human players to windows
(`gNumWindows = active_player_count`, and per-window arrays indexed by the player's slot). There are two ways to do
it, and both fail:

- A remote human without a window is a game change across the HUD, cameras and sound. The two PCs would run
  different window sets, and so different random streams, as above.
- Making the remote car a drone puts it on the drones' AI and update schedule (`update_drone_models`, `D_80120E74`,
  MORECARS.md 1), not a human's controls.

### 1.5 Session flow, and what must match

**Lockstep from boot is required, not just simpler.** The state can't be copied from one PC to the other (1.6). The
only way to have identical state is to build it identically, from the same boot with the same inputs. So:

1. **Before boot, a handshake.** Started from the command line or a launcher, later a page in the F1 overlay. The
   peers connect and compare:
   - the build (`rush2049pc --version`: `rush2049pc c8692ce`, the commit; the compiler may differ, see WINDOWS.md 12);
   - the ROM's SHA-1 (the US check is the default, `[game] rom_check`);
   - the settings the game reads (table below);
   - a hash of the mods the game will load.
   They then send the pak images, agree on D, and agree on a start time.
2. **Both boot.** Retrace 0 starts at the agreed wall time on both PCs. From then on every retrace is gated on
   input.
3. **The game's own menus, together.** Player 1 is the host, on port 1; player 2 is the guest, on port 2. The flow
   is `race2p.press`'s: the main menu's player count, A on each port at the player screen and the car select. Races,
   results and further races all stay in lockstep.
4. **Leaving.** Quit sends a goodbye. The session ends for both.

A "lobby that sets up the race directly" is not possible as state. It is possible as input: a PC-side menu could
generate a press macro (as the `.press` scripts do) that both PCs play into the game. That is a later convenience.
Its weakness: the menus remember cursor positions and profiles, so a macro can go wrong.

**What must match** (sent by the host, checked by the guest, and adopted for the session without writing the guest's
ini):

| Must match | Why (evidence) |
|---|---|
| The ROM's SHA-1 | data |
| The build's commit | game code. Different compilers are fine on synthetic paths (WINDOWS.md 12); require the same commit |
| `game.frame_rate` | the scheduler and 98 per-frame sites (1.1) |
| `game.cars` | the roster and the drones (MORECARS.md 7.1, latched at a race's start) |
| `game.memory`, `game.expansion_pak` | the heap's size and the Expansion Pak flag the game reads (`audio_heap/group.c:308-316`, MEMORY.md 1), and so the sixth track and extra waves |
| `game.mods`, `mods_dir` contents, `mods_order` | the files the game loads. Mods become a region of the cartridge at `PC_ROM_MODS_BASE` plus a changed file table (MODS.md 6, `pc_mods_install`), so hash the cartridge as the game reads it (the ROM, the patched table and the mods region) rather than the folder. Never send the files |
| `game.pc_setup`, `game.ghost_prompt` | game-side menu changes (FRONTEND.md "PC SETUP", SAVES.md 7.1) |
| `audio.voices`, `audio.engines` | the game's sound manager pools (AUDIO.md 10) |
| `video.draw_distance`, `video.detail`, `video.fog` | the game C reads them: `detail = high` makes scene nodes for level objects with 2-4 players (DETAIL.md 2, `level_objects/group.c:490`), and the draw distance changes the scene walk. They could be freed later if hashes show no effect |
| The accessory on each port, and each pak image | read by the game at boot and at hot swaps (ULTRA.md 3) |
| The connected ports | the main menu's player count, ControllerUpdate's status (1.3) |
| The input delay D, and the start time | the lockstep itself |

**Free per PC:** everything only the renderer, the output or input shaping uses:

- `video.*` except the three above: scale, widescreen, fill, `frame_rate` (frames between), hud, textures,
  `texture_lod`, vsync, fullscreen. Widescreen, hud and the window's size do reach the game C through
  `pc_gfx_hud_dx` (1.1), but only to place HUD models, so a session either gives the game 0 there or leaves those
  places out of the hash;
- `audio.backend`, `volume`, `latency`;
- all of `[input]`;
- texture packs (TEXTURES.md);
- the log.

**Saves.**

- Each player brings their own pak: player 1's image from the host, player 2's from the guest. They are 32 KiB each,
  checked with `pc_pak_image_check` before use. With `game.saves = files` the image is the one built from the folder
  (`pc_store_build`, SAVES.md 2).
- During a session nothing is written to disk. Afterwards, write-back of your own port's image could be offered as a
  choice; v1 doesn't.
- The F1 Saves page's hot swaps (`swap_pak`, SAVES.md 5.1) are disabled in a session.

**Pause.**

- The game's own pause (START in a race) is an input, so it is already in lockstep.
- The PC's pause key, and focus loss with `pause_on_focus_loss`, hold the main loop (`hold()`, `loop.c:528-540`,
  579-586). The F1 overlay holds the game too (`input.c:15-16`, `menu.c:4`).
- In a session none of them may hold. The overlay opens over a running game, with the local player's pads idle while
  it is open: that idle input is what gets sent, so it is still deterministic. Items that change the settings above
  are greyed out ("locked in an online session").

**Disconnects.**

- A missing input stalls both games at the next retrace. After about 0.25 s, show "Waiting for <name>..." over the
  last frame, keep pumping window events, and let Escape leave.
- After a timeout (15 s suggested), end the session. Optionally, carry on alone with the other port's input idle.

**Rejoin.**

- There are no savestates, so there is no rejoin into a running session. The cheap alternative: replay the
  session's input log from boot on the free-running virtual clock, then switch to paced lockstep.
- On the synthetic cartridge that is about 70 times faster than real time on rocky (ULTRA.md 1) and 27 times on the Pi
  (E1). On retail it is unmeasured.
  The peer that stayed must wait while the other catches up.
- Later phase, if ever.

### 1.6 Sync strategy: delay-based lockstep, not rollback

**Rollback needs the whole state saved every retrace and restored on a misprediction.** Here the state is:

| Part | Size, where |
|---|---|
| The RDRAM window | 8 MiB at `original`, 16-256 MiB with `game.memory` (MEMORY.md 1). Holds the game's data and BSS at their N64 addresses (boot 0x800277D0-0x80086A50, game 0x8010FD7C-0x8017A640) and the heap from 0x8017A640 (ULTRA.md 2) |
| Game data with host pointers | outside the window (port/data's arenas, DATA.md 3) |
| libultra's state | `sched.c`'s side tables of threads, queues (host-side message buffers) and timers, keyed by the game's objects (ULTRA.md 1); `timer.c`'s timers |
| Each N64 thread's execution | a host thread with a 2 MiB stack, stopped inside a libultra call. Its stack and registers are part of the game state |
| The SI | pads, paks, motors (`si.c`) |
| Audio | the output queue and held pushes (`audio_out.c`), the MusyX interpreter's state (`musyx.c`) |
| The renderer | not needed (render only) |

**The threads are the blocker.**

- A thread's host stack and registers can't be saved and restored portably while it waits in `pthread_cond_wait`.
- `fork()` snapshots work only on Linux.
- Running N64 threads as coroutines on fixed-address stacks with saved contexts would be a rewrite of `sched.c`. It
  would also need the side tables made copyable.
- So rollback is out of scope. GGPO (MIT) is a rollback library and doesn't apply.

**Delay-based lockstep.**

- Each PC samples its player's pads at retrace N and schedules them for retrace N+D. At retrace N both apply the
  inputs scheduled for N.
- The first D retraces have idle input on both.
- Both players feel the same delay on their own input, so it is fair.

**What delay to expect.** Notation:

- L is the one-way latency, J its jitter, and a retrace is 16.7 ms.
- With both PCs reaching retrace N at the same wall time (clock offset from pings), D must be at least
  ceil((L + J + about 2 ms) / 16.7).

| Link | RTT | D | Added delay |
|---|---|---|---|
| LAN | under 2 ms | 1 | 17 ms |
| Tailscale direct, same city or region | 10-30 ms | 1-2 | 17-33 ms |
| same country | 40-80 ms | 2-4 | 33-67 ms |
| across a continent or an ocean | 100-160 ms | 4-6 | 67-100 ms |
| Tailscale through a DERP relay | adds tens of ms | +1-3 | |

How it feels, as rules of thumb to measure in phase 3:

- The game already responds in steps of one 30 Hz frame: the model thread reads the controls every retrace, but
  what you see changes every other retrace.
- D = 2 is one game frame of extra delay. Hard to notice.
- D = 3-4 can be felt in quick corrections.
- D ≥ 6 (100 ms) feels laggy in steering, but an arcade racer stays playable.
- There is no prediction to hide it: a car that reacted early would need rollback.

**Desync detection.**

- Each PC hashes state at the retrace, while every thread waits: where the state log reads its values
  (`snapshot()`, `loop.c:612`).
- Every retrace, a curated set (a few KiB):
  - the model records `pc_models`, the car records `pc_cars`, the roster `pc_links`;
  - the seed `D_8011735C`;
  - `state_word_a` and `state_word_b`;
  - the model tick `D_80143FF4` and the race time `D_801543CC`;
  - `gControllerData`, `gCamera`, the places.
- Once a second, the whole window, without the words that hold host pointers. Experiment E6 found them: with ASLR
  on, two processes' 8 MiB windows differ only in 81 words. 80 are the audio manager's effect tags' `list` pointer
  to `D_80143FF8` (`audio_effect_setup_b132.c:170`). One is `D_80156BCC`, the low half of a display-list pointer
  (`world_collision_response.c:143`). These also differ between Windows and Linux, whose image layouts differ.
  Either leave out a list of such words, or replace each value that points into the executable by its offset there.
- Each PC sends its hashes. The first mismatch ends the session (or warns, in a debug mode). Each PC writes a desync
  report: the retrace, the hash of each region, a dump of the curated regions, and the input log so far. The session
  can then be replayed on each OS to find the first difference.

### 1.7 Networking

**Transport: UDP.** All traffic uses one socket and one port, so NAT traversal stays possible. The net code runs on
a host thread of its own, never an N64 thread. It hands inputs to the main loop through a small locked ring.

**Messages.** Little-endian, each with a magic, a protocol version and a session ID. Packets stay at 1200 bytes or
less (Tailscale's MTU is 1280).

| Message | When | Content |
|---|---|---|
| HELLO / HELLO_ACK | connect | the build's commit, the protocol version, the role, a nonce, the join-code check |
| SETTINGS | handshake (reliable) | the canonical list of the table in 1.5, and its hash |
| FILE, FILE_ACK | handshake (reliable) | pak images in 1 KiB chunks (64 chunks for two paks) |
| READY | handshake | the hash of everything agreed |
| START | host to guest | the input delay D; start at host time T (clock offset from PING) |
| INPUT | every retrace | the first retrace F, a count n, n × 4 bytes for each port the sender owns. Every input not yet acknowledged is resent (redundancy: one lost packet loses nothing). Also: the last retrace received from the peer (the ack), and the sender's current retrace (time sync) |
| HASH | every retrace, or every 30 | the retrace, the curated hash, and once a second the window's |
| PING / PONG | throughout | timestamps: RTT and clock offset (NTP-style) |
| BYE | leaving | the reason |

- Lobby messages are made reliable by resending every 100 ms until acknowledged.
- Bandwidth: about 60 packets a second each way, 40-100 bytes each, about 5 KB/s.
- **Time sync:** each side estimates how far ahead of the other it runs, from the peer's current retrace plus RTT/2.
  The side that is ahead stretches its pacing by up to 1 ms a retrace until they are even. Clock drift between two
  PCs is tens of ppm, a few ms a minute. This is the "frame advantage" idea, written from scratch.

**Connecting** (in order of effort):

1. **Tailscale** (recommended, and the user has it): the host's 100.x address or MagicDNS name. Tailscale does the NAT
   traversal and encrypts and authenticates with WireGuard. When it can't connect directly it relays through DERP,
   which adds latency; `tailscale ping <peer>` says which. The friend has to join the user's tailnet, or the user
   shares the node with them. ZeroTier works the same way.
2. **Port forwarding:** the host forwards one UDP port (a default plus a setting) on the router, and the guest
   connects to the public IP. IPv6 often needs no forwarding, only a firewall rule, so the code should use dual-stack
   sockets (`IPV6_V6ONLY` off).
3. **UDP hole punching:** a small rendezvous server (a few hundred lines; it could run on the user's own host) tells
   each peer the other's public endpoint, and both send at once. It fails on symmetric NAT and some CGNAT.
4. **A relay:** forwards packets for peers that can't connect. It needs a host, and adds a hop.

**Security basics.**

- Treat every packet as hostile:
  - fixed layouts, length checks;
  - retrace numbers within a window;
  - nothing from the network used as a path;
  - settings parsed by the existing config parser against a whitelist;
  - pak images checked by `pc_pak_image_check` before use.
- Never transfer the ROM, mods or code; compare only hashes.
- One peer per session; ignore other addresses after HELLO; rate-limit HELLOs.
- **Authentication:**
  - On Tailscale, WireGuard already authenticates and encrypts the link. Bind to the Tailscale address.
  - Over the open internet, use a join code shown by the host and an HMAC on every packet with a key derived from it.
    The implementation should be permissively licensed: Monocypher (CC0 or BSD-2) or a small public-domain BLAKE2s.
  - Inputs aren't secret, but spoofed packets must not be able to inject them.
- **Windows Defender Firewall** asks the first time the program listens. The host must allow it on the network
  profile in use. Say so in the log and the docs.

**Libraries.**

- **Plain sockets** (recommended): POSIX on Linux; Winsock 2 on Windows, linked to `ws2_32`:
  - `WSAStartup`/`WSACleanup`;
  - `SOCKET`, `INVALID_SOCKET`, `closesocket`;
  - `ioctlsocket(FIONBIO)`;
  - `WSAGetLastError`, `WSAEWOULDBLOCK`;
  - `getaddrinfo` from `ws2tcpip.h`;
  - `WSAIoctl(SIO_UDP_CONNRESET, FALSE)`. Without it, an ICMP "port unreachable" makes the next `recvfrom` fail with
    `WSAECONNRESET`, a classic UDP trap on Windows.
  `ws2_32.dll` is part of Windows but not on `pe_check.py`'s list (`tools/port/pe_check.py:23-24`): add it there and
  to RELEASE.md's list, or load it at run time as `dwmapi.dll` is (FRAMERATE.md 2).
- SDL_net (zlib licence) would add a second pinned dependency for little gain.
- ENet (MIT) gives reliable channels over UDP. It is an option if the handshake grows, but the needs here are small.
- **Avoid copying** from GPL code: Mupen64Plus' netplay, RetroArch, Dolphin, the emulators' netplay servers. Write
  from the protocol above and public documentation.

**Timing on Windows.** The main loop already asks for 1 ms timers and uses high-resolution waitable timers
(WINDOWS.md 5). Waiting for input at a retrace should wait on an event that the net thread sets, not spin.

### 1.8 Audio, rumble, the overlay: per-PC output kept out of the game

**Audio.**

- With a device, the AI's events follow the device's queue: `pc_audio_queued_frames` and the target come from its
  pulls (`audio_out.c`; `rcp.c` `ai_due`). That makes the game's audio thread run at moments the device decides,
  which isn't deterministic. Even with `--fast` only the null output keeps the AI on the N64 clock (`main.c:1203-1206`,
  `audio_out.c:406-425` `null_drain`).
- In a session, the AI's accounting must be the null output's on the virtual clock.
- The device gets a **tap** instead: each buffer the game pushes, after the existing hold-for-render matching, also
  goes into a FIFO the device drains. The FIFO keeps about 50 ms. Its resampling ratio (`resample.c`) is nudged by a
  fraction of a percent to keep its level, because the paced N64 clock and the sound card's crystal drift apart.
- A device underrun is then a click on one PC. It never touches the game.

**Rumble.**

- The game sets every player's motor (`pc_si_motor(channel)`, ULTRA.md 3). That is deterministic game state. Only
  the local port's motor goes to the local pad (`input.c` `rumble()`); the remote port's is ignored.

**The overlay, screenshots, the log, frames between, widescreen, textures.** All local, all render-only. The overlay
must not hold the game (1.5). A "Session" row could show the peer, RTT, D, stalls and the last hash agreement.

**The audio mix.** Both PCs play the game's two-player mix (1.4).

## 2. Experiments for this plan (synthetic cartridge, Pi 5)

Run 2026-11-05 on the Pi 5 (aarch64, GCC 12.2, RelWithDebInfo, headless, `--audio null --no-pak --no-config`), each
under the shared lock. The cartridge was generated by `synth_rom.py` from this build's data report. Everything ran on
`tools/port/synth/race2p.press`: two players, split screen, six cars, to 600 graphics tasks into the race. The traces
are `RUSH_STATE_EXACT=1 --status 1 --log-physics 1`: every retrace, every car's position and velocity as exact hex
floats, with its lap and place. The experiments' scripts and logs aren't in the repository.

| # | Question | Run | Result |
|---|---|---|---|
| E1 | Do two runs with the same scripted inputs give identical state? | `--fast`, twice | **Identical.** 2,763 physics lines (every retrace, 46.0 s of game time), not one differs; every state change at the same retrace (the race at vi 1563). Each run took 1.7 s of wall time: about 27 times real time on the Pi on this light cartridge |
| E4 | Does a slow host change the game's course? | `--fast` with `RUSH_SIM_GFX_MS=40` (each graphics task holds the RDP 40 ms of N64 time, so every frame takes 3 retraces or more) | **Yes.** The states come later (PrePlay at vi 1414 against 1403, the race at 1579 against 1563). Aligned by model tick, the cars first differ at tick -57, in the countdown: car 0's position in the last bits, then 486 of 1,353 ticks differ. The synthetic cars are put back on the same course every few seconds, so they meet again. This is FRAMERATE.md 7.4's per-frame decisions (DoDrones) taken at other retraces |
| E5 | What changes with `game.frame_rate = 60`? | `--fast --set game.frame_rate=60` | The front end runs a retrace or two sooner (the race at vi 1561). By model tick the cars first differ at tick -58 and 599 of 752 ticks differ, as FRAMERATE.md 7.4-7.5 found. So the frame rate is part of the simulation and must match |
| E6 | Can the whole RDRAM window be hashed across processes? | gdb stops each run at `pc_si_finish` after `--frames 1000` (vi 2027, in the race), dumps 0x80000000-0x80800000. Two pairs with ASLR off, one pair with ASLR on | ASLR off: **the two 8 MiB windows are identical, byte for byte** (both pairs). ASLR on: 243 bytes in 82 words differ. 80 are the same host pointer in heap records 0x58 apart from 0x801CBD10: the audio manager's effect tags, whose `list` points at `D_80143FF8` in host storage (`audio_effect_setup_b132.c:170`; the address resolved to `D_80143FF8` in the executable). One is `D_80156BCC`, the low half of a display-list pointer (`world_collision_response.c:143`). One, `D_80151AE0` (retraces since the frame began), 1 against 2, is the experiment's own race: the loop's last retrace event had already woken the game threads when gdb stopped the main thread. A session hashes before the retrace's event, as `snapshot()` reads. So **the window is deterministic but for host pointers**, which a window hash must leave out or normalise |
| E7 | Is real time deterministic, on a host that keeps up? | the same script without `--fast`, twice, 35 s each | **No.** Both runs kept 60.00 retraces a second (none late, none dropped), and every state change came at the same retrace. But the physics differ: by model tick, 161 of 689 differ, from tick 56 in the race (car 0's y position `0x1.86a1p+0` against `0x1.86a0fep+0`). The audio tasks were 2,094 against 2,096. The likely cause (not bisected): in real time the host's timing decides where the model thread's steps, the audio tasks and the AI events fall against the game thread's per-frame decisions (1.1) |

What these don't show:

- Anything about Windows. The cross-OS evidence is CI's (WINDOWS.md 12).
- Retail behaviour.
- A paced virtual clock, which doesn't exist yet.

E1 and E7 together are the case for building the session on the virtual clock. E4 is why it must be paced rather than
left to real time. E6 says the window hash is practical.

## 3. Recommended architecture

```
 PC A (host, port 1)                                   PC B (guest, port 2)
 ┌────────────────────────────────────────┐            ┌────────────────────────────────────────┐
 │ game C (unchanged)  N64 threads, baton │            │ game C (unchanged)                     │
 │   ControllerUpdate reads pads / retrace│            │                                        │
 │ port/ultra  virtual N64 clock          │            │ port/ultra  virtual N64 clock          │
 │   paced to wall clock, gated on input  │            │   paced, gated                         │
 │   retrace N: pads = {A[N], B[N]}       │            │   retrace N: pads = {A[N], B[N]}       │
 │ port/net (host thread)  UDP ───────────┼── inputs ──┼─► port/net                             │
 │   A's pads at N  → sent for N+D        │◄─ hashes ──┼── B's pads at N → sent for N+D         │
 │ port/audio  AI on the virtual clock,   │            │                                        │
 │   device fed by a tap                  │            │                                        │
 │ port/gfx  split screen, or own view    │            │ port/gfx  own view = pane 2            │
 └────────────────────────────────────────┘            └────────────────────────────────────────┘
```

| Piece | Where | What, and how big (rough) |
|---|---|---|
| The paced deterministic clock | `port/ultra/sched.c` (`wait_main_virtual`), `timer.c`, `loop.c` | wall-clock pacing of the virtual clock's advances; `pc_time_virtual()` split into "deterministic" and "free-running"; the input gate at the retrace. About 300 lines |
| The audio tap | `port/audio/audio_out.c`, `rcp.c` | AI accounting on the virtual clock with a device present; a device FIFO with drift correction. About 250 lines |
| Input frames | `port/input/input.c`, `port/ultra/loop.c` | local pads for N+D; merged pads at N; fixed connected ports and accessories; record and replay files (`--record-inputs`, `--replay-inputs`, `--input-delay`). About 300 lines |
| State hash | `port/main.c` (beside `game_info`), `loop.c` | curated and window hashes, `--log-hash`, a desync report. About 200 lines |
| Net | new `port/net/` (`net.c`, `net_sock.c` with POSIX/Winsock, `session.c`) | sockets, messages, reliable lobby messages, the input ring, time sync, `RUSH_SIM_NET`. About 1,500 lines |
| Session settings | `port/config/config.c`, `port/main.c` | the must-match list, adoption for the session, locks in `menu.c`, `pc_gfx_hud_dx` held at 0. About 200 lines |
| Own view | `port/gfx` (`gfx_rsp.c`, `gfx_rdp.c`, `gfx_hud.c`) | the view filter, re-aspect in y, 2D by pane. 600-1,000 lines, uncertain |
| Options | `port/main.c`, `[net]` in the ini | `--net-host [PORT]`, `--net-join HOST[:PORT]`, `--net-delay auto|N`, `--net-code CODE`, `net.view = split|own` |

No game C changes are needed for lockstep or own view. The one change on the simulation side, `powf` at 60, is in
`port/config/game_fps.c`, and only matters at `game.frame_rate = 60`.

## 4. The alternatives, and why not

| Alternative | Why not (or not yet) |
|---|---|
| Rollback (GGPO-style) | Needs state saved and restored every retrace; the N64 threads' host stacks make that impossible without rewriting the scheduler as coroutines (1.6) |
| Sending state (car positions) from a host, server-authoritative | The game has no seam for injecting state: the physics, drones, checkpoints and the RNG would all need hooks into game C, and the result would be a different game. Lockstep reuses the determinism the port already has |
| Lockstep in real time (no virtual clock) | Not deterministic: real-time runs differ on one host (WINDOWS.md 12, E7), and the per-frame logic depends on retraces per frame (1.2) |
| One window per PC, the other car remote or a drone | The windows' per-frame logic draws from the shared RNG; different window sets diverge (1.4). A drone is AI, not a human's controls |
| Different settings per PC with a "sim subset" | Possible later for settings shown not to matter (hash them in phase 0.4); until then, require a match |
| Streaming (Parsec, Steam Remote Play, Sunshine and Moonlight): the host runs two-player split screen and streams it; the friend's pad appears on the host | Works tonight with no code, and no determinism risk. But both see the split screen, the guest gets video-codec latency and quality, and these are third-party tools. A fair stopgap while this is built |
| Emulator netplay of the cartridge | Exists in other projects (GPL), split screen only, none of the port's features. Not this project's path |

## 5. Phases, each with its proof

Each phase lists what it must find out first. Phases 0-3 give "race a friend in split screen over Tailscale". Phases
4-6 make it nicer.

### Phase 0: the clock, the record and the hash (no network)

**0.1 The paced deterministic clock.** First find out whether anything on the N64 side behaves differently when the
virtual clock waits for the wall clock. By design it shouldn't: N64 time is still virtual. Then do it: pace
`wait_main_virtual`, and split `pc_time_virtual()` into its two meanings.

- Proof:
  - every synthetic path in paced mode gives the `--fast` trace to the bit (`synth_compare.py` with a `--set-b`-like
    switch for the mode);
  - the same under `RUSH_SIM_PRESENT_MS=40`, `RUSH_SIM_TIMER_STEP_US=15625` and a loaded host, where real-time mode's
    trace changes (E7);
  - 60.00 retraces a second in wall time on rocky;
  - with a slow host modelled, the game slows and the trace doesn't change.
- Also measure the retail game's wall time per frame in this mode on the Pi and on a desktop PC.

**0.2 Audio in paced mode.** AI accounting on the virtual clock with a device, and the tap.

- Proof:
  - with the device on and off, the trace is the same (the audio's own WAV is the same too, as `synth_compare.py
    --audio` checks);
  - an `audio_realtime`-style run: underruns, the FIFO's level, and drift over 10 minutes.

**0.3 Input record, replay and delay.** `--record-inputs FILE` writes each retrace's four pads; `--replay-inputs FILE`
plays them instead of live input; `--input-delay D` shifts the local pads by D retraces.

- Proof:
  - a recorded live or scripted run replays to the same trace;
  - with D, a press script's effects come exactly D retraces later;
  - **the same replay file run by the Windows cross build in CI** (`windows-run`) gives Linux's trace to the bit.
    That is a cross-OS proof of netplay's core without any network.

**0.4 The state hash.** `--log-hash N`, the curated set and the window hash.

- Proof:
  - two runs agree every retrace;
  - a deliberate one-word change (`RUSH_SIM_DESYNC=retrace:address`) is caught at that retrace (a mutation check,
    as FRAMERATE.md 4 does for `gfx_interp.c`);
  - E6 already found the words that differ between processes (host pointers, 1.6). Repeat it on every path and
    between a Windows and a Linux replay, so that the window hash's exclusions are complete;
  - **a settings matrix:** one replay run with each candidate setting changed, comparing hashes, sorts the settings
    of 1.5 by evidence.

**0.5 Retail, locally, across OSes.** This is the most important unknown, and only the user can run it with their ROM.

- Record a two-player retail race with crashes, an explosion and a battle. Replay it on the Pi (Linux aarch64), a
  Linux x86-64 PC and the Windows PC, and compare per-retrace hashes. Keep it out of the repository, as with the other
  retail runs.
- Do it at `game.frame_rate = 60` too, once `powf` is replaced.

### Phase 1: lockstep on one machine over loopback, unpaced

**Do:** `port/net` (sockets, the handshake, INPUT, HASH), `--net-host` and `--net-join`. Each process owns one port.
A press script is split by port (`race2p.press`'s `@2` lines go to the guest).

**Proof:**

- both processes' traces and hash logs are the same as each other;
- and the same as a single-process `--replay` of the merged script with the same D;
- paths: `race2p`, `battle2p`, `pause`, `raceend`, `cheats`, `menu`, and a circuit;
- **the desync fuzzer:** seeded random inputs on both ports (button changes and stick sweeps at random retraces, menu
  navigation included), 5 minutes of game time per seed, many seeds, hashes compared every retrace; plus the mutation
  check.

**Find out first:** whether the handshake's must-match list catches every setting that shows up in a hash
difference. The phase 0.4 matrix answers that.

### Phase 2: real time, and bad networks (loopback)

**Do:** pacing in a session; `RUSH_SIM_NET=delay=40,jitter=10,loss=5,dup=1,reorder=2` in the net code's receive path,
in the style of the other `RUSH_SIM_*` models; the waiting overlay, timeouts and BYE; the F1 and pause-key rules of
1.5.

**Proof:**

- for every network model, the trace is phase 1's (the network may only change when retraces happen, never what they
  do);
- stalls counted at each D;
- 60.00 retraces a second when D covers the model's latency;
- a disconnect ends the session cleanly on both;
- `realtime_check.py`-style exit lines.

### Phase 3: two PCs over Tailscale

**Do:** run it between the user's own two machines first, then with the friend: Windows against Linux, retail.

**Proof:**

- whole sessions, from boot through several races, with hashes agreeing every retrace;
- the log's RTT, chosen D, stalls and time-sync corrections;
- the feel at D = 1 to 5, played.

**Find out first:** whether Tailscale connects directly (`tailscale ping`) or through DERP, and the RTT.

### Phase 4: own view, full screen

**Find out first:** a survey of vertical culling in the game C (the horizontal one exists); where the two-player HUD
draws each element; which 2D tracks 3D.

**Do:** the renderer's view filter, the y re-aspect, and the 2D sorted by pane. `net.view = own` (render-only, so
free per PC).

**Proof:**

- the hashes are the same with own view on one PC and split on the other (render-only);
- `synth_shot.py`-style checks that the target shows one view across it;
- retail frames looked at for holes at the top and bottom and the HUD's placement.

### Phase 5: beyond Tailscale

**Do:** connect by address with port forwarding; join codes and the HMAC; IPv6; then the rendezvous server for hole
punching, and a relay.

**Proof:** a session between two networks without Tailscale; a spoofed INPUT is rejected (a test sends one).

### Phase 6: comforts

- An F1 "Online" page: host, join, code, status.
- A quick-race macro.
- Saving replays (the input log already is one).
- Rejoin by replay.
- Three or four players over two to four PCs: battle and stunt allow them (MORECARS.md 1.2). The design generalises
  to each PC owning a set of ports, including two local players on one PC.

## 6. Risks and unknowns, ranked

| # | Risk | Likelihood, impact | What finds it out, and when |
|---|---|---|---|
| 1 | **Retail driving diverges between Windows and Linux** (crashes, explosions, real sounds: never compared across OSes; WINDOWS.md 12 "What this can't show") | low to medium, fatal to a session | Phase 0.5 replays with per-retrace hashes; the first differing retrace names the region |
| 2 | **Real time leaks into the paced mode** (the AI following the device, presentation code mixing the two meanings of `pc_time_virtual()`) | medium if not separated, desyncs | Phase 0.1/0.2 trace equality under the timing models |
| 3 | **A host too slow for the serialised main loop**: the game slows for both | low on desktops at original; high on a Pi at 60 (FRAMERATE.md 7.7) | Phase 0.1 wall-time measurements, retail |
| 4 | **Input delay feels bad at the friend's RTT** | depends on distance: 2-6 retraces | Phase 3, played |
| 5 | **A desync ends the session**: no savestates, so no resync | rare if 1 and 2 hold | Phase 1 fuzzer, phase 3 logs; rejoin by replay later |
| 6 | `game.frame_rate = 60` uses the host's `powf` (`game_fps.c:138`) | low, but untested across OSes | Fix with `sqrtf` before allowing 60 online; phase 0.5 at 60 |
| 7 | **Own view**: vertical culling, HUD sorting, 2D that tracks 3D | medium, cosmetic | Phase 4's survey; split screen still works |
| 8 | **NAT and firewalls**: CGNAT, symmetric NAT, the Windows firewall prompt | medium off Tailscale | Phase 3 (Tailscale), phase 5 |
| 9 | Audio drift and underruns with the tap | low, cosmetic | Phase 0.2 long runs |
| 10 | Edge cases: an F1 setting changed mid-session, a saves hot swap, a pad unplugged (it must not change the connected port), Alt+Tab with `pause_on_focus_loss` | medium, desync or stall | Phase 1 tests for each |
| 11 | **Different builds**: the friend on an older zip | certain to happen once | The commit check at HELLO, with a clear message |
| 12 | **The release archives themselves are never compared.** `windows-same` compares the Windows cross build (debian:12 mingw-w64 GCC 12.2) with pc.yml's Linux jobs (ubuntu-24.04 runners). The Linux release tarball is built in `ubuntu:22.04` (RELEASE.md "Release archives"): another GCC | low (three compilers agree today) | Add a release job that runs `synth_compare.py --from-logs` on the archives' own programs |
| 13 | **Render values that reach the game C**: `pc_gfx_hud_dx` in `car_lights_render.c` (widescreen, HUD, window size); `pc_detail()` and the draw-distance macros in `particle_knot/group.c:850-867` | certain: they exist; the first is drawn only | Session rule (1.1); a grep for `pc_gfx_`, `pc_detail` and `pc_cfg` in `src/` and `nonmatching/` (today: those two files) as a CI check |
| 14 | **Game code that orders or hashes by host address** would differ between PCs (the window holds host pointers, E6) | low: traces agree across ASLR runs and OSes on every synthetic path | The phase 1 fuzzer with ASLR on; phase 0.5 across OSes |

## 7. Decisions for the user

Taken 2026-11-06: every recommendation below. The input delay is automatic, and section 11 makes it adapt
during a session.

| Decision | Options | Recommendation |
|---|---|---|
| How a session starts | (a) lockstep from boot with a short PC-side handshake, menus navigated together; (b) a PC lobby that picks track and cars, then a macro presses through the menus | **(a)**, with (b) as a later convenience. Setting the race up as state isn't possible (1.5) |
| What each player sees | (a) split screen on both first; (b) wait for own view before playing | **(a)**: race sooner; own view is phase 4 and render-only, so it can come later without touching the protocol |
| How to connect | Tailscale; port forwarding; hole punching with a rendezvous; a relay | **Tailscale** for the first sessions (both of you on the tailnet); port forwarding next; hole punching only if needed |
| The input delay | automatic from the measured RTT, or fixed | **Automatic, with a manual override** (`--net-delay`) |
| The game's frame rate online | original (30) or 60 | **Original** until the `powf` fix and phase 0.5 at 60; 60 only when both PCs keep it |
| Saves during a session | session-only copies of each player's pak; or write your own port's pak back afterwards | **Session-only** in v1 |
| Whose settings rule | the host's, adopted by the guest for the session (the guest's ini untouched); or both must already match | **The host's, adopted** |
| A peer drops | end the session after a timeout; or continue alone | **Wait 15 s, then end**; "continue alone" optional |
| Builds | the same commit required; or the same release only | **Same commit**, checked at connect |
| Platforms | x86-64 Windows and Linux only; or the Pi as a peer | **x86-64 first.** The Pi only after retail hashes agree across architectures, and only at original |
| Meanwhile | stream the split screen with Parsec or similar | Your call: it works today, at a cost in latency and picture |

## 8. Testing: the project's proof style, extended

| Test | Kind | Checks |
|---|---|---|
| `paced_same_<path>` | ctest, real time, RUN_SERIAL | a path in paced mode gives `--fast`'s trace (exact floats, `--status 1`), also under the timing models (`RUSH_SIM_PRESENT_MS`, `RUSH_SIM_TIMER_STEP_US`, `RUSH_SIM_VSYNC_HZ`) |
| `replay_same_<path>` | ctest | record, replay: the same trace; `--input-delay D` shifts by exactly D |
| `replay_windows` | CI (`windows-run`, `windows-same`) | replay files recorded on Linux give Linux's traces and per-retrace hashes on the Windows programs (cross and native) |
| `net_loopback_<path>` | ctest | two processes over 127.0.0.1, each driving its own port: both traces the same as each other, and as `replay` with the same D |
| `net_fuzz` | ctest (short), and a long soak tool | seeded random inputs on both ports; hashes per retrace; a mutation run (`RUSH_SIM_DESYNC`) must fail at the right retrace |
| `net_sim_<model>` | ctest | `RUSH_SIM_NET` models (delay, jitter, loss, duplicates, reordering): the same trace as the clean run; stalls within bounds; disconnect handling |
| `net_hash_settings` | tool | which settings change the hashes (phase 0.4) |
| `own_view_shot` | ctest | one pane across the target; hashes unchanged against split screen |
| Windows against Linux, live | by hand (phase 3) | a session between the user's Windows PC and a Linux PC; the logs' hashes; retail runs stay local |

What CI can't do: two machines over a real network. Replay files carry cross-OS proof without a network, and the
loopback tests carry the protocol. Only the users' own sessions test real routers and real latency.

## 9. What is not known yet (honestly)

- Whether retail play stays deterministic across Windows and Linux. Expected, from the synthetic proof and the
  reasons in WINDOWS.md 12, but not measured (risk 1).
- How fast the retail game runs on the serialised virtual clock on the users' PCs (risk 3).
- The friend's RTT and whether Tailscale connects directly (risk 4, 8).
- The two-player HUD's layout and the vertical culling, for own view (risk 7).
- Whether `video.draw_distance` and `fog` really change game state, or only what is drawn. They are treated as
  must-match until hashes say otherwise.

## 10. References

Docs: ULTRA.md 1-4 (threads, the virtual clock, the main loop, the SI); FRAMERATE.md 1, 2, 7.1-7.7 (pacing,
`game.frame_rate`, random numbers, slow hosts); OPTIMIZED.md 2.3, 3-5 (FMA, flags, `synth_compare`); WINDOWS.md 5, 12
(Windows' timing, the same game as Linux's); SYNTH.md 2 (paths, expect lines); MORECARS.md 1, 2.4 (counts,
per-window arrays); GFX.md "Widescreen", "The HUD at the screen's edges", "High frame rate" (views, culling); DETAIL.md
1-2 (the scene walk, settings the game reads); AUDIO.md 7 (real-time audio); SAVES.md 2, 5 (saves as files); MEMORY.md
1 (the window's size); FRONTEND.md "Controls", "Settings".

Code: `port/ultra/loop.c` 528-540, 543-, 579-586, 612, 633-647, 688-716; `port/ultra/sched.c` 224-230, 786-819,
830-870; `port/ultra/timer.c` 43-55; `port/ultra/rcp.c` 63-110, 263-270, 295-312; `port/ultra/si.c` 266-285,
406-414; `port/ultra/pakfile.c` 69-105; `port/input/input.c` 327-359; `port/include/pc_port.h` 250-256;
`port/main.c` 434-449, 460-464, 1203-1206; `port/config/game_fps.c` 128-139; `port/audio/audio_out.c` 406-425,
702-; `CMakeLists.txt` 161, 165; `tools/port/pe_check.py` 23-24; `src/blob/ControllerUpdate.c` 73-127;
`src/blob/groups/modelthread/modelthread.c` 63; `src/rom/lib_1050_sdk.c` 93, 189-197, 263, 340, 362, 421;
`src/rom/lib_1050.c` 74; `src/rom/lib_a050.c` 26; `src/rom/lib_f700.c` 54; `src/blob/func_800E8D50.c` 43-56;
`nonmatching/game/display_list_flush.c` 373; `src/blob/wheel_render_full.c` 50-90;
`src/blob/groups/particle_knot/group.c` 1613-1650; `src/blob/Effects_UpdateEmitters.c` 3819;
`src/blob/func_800B90F8.c` 3806; `src/blob/func_80108F40.c` 125, 160; the LCG at `src/blob/world_object_destroy.c:48`,
`hud_render.c:168`, `func_800EB90C.c:156`, `entity_update_callback.c:104`, `groups/blast/blast.c:135`,
`groups/audio_options_menu/func_800D6160.c:33-34`; `src/blob/car_lights_render.c` 47-62;
`src/blob/groups/engine_sound_sync/audio_effect_setup_b132.c` 170; `src/blob/world_collision_response.c` 143;
`tools/port/synth/race2p.press`. MODS.md 6 (how mods reach the game); RELEASE.md "Release archives".

## 11. Robustness: any latency, and no desyncs

The user's emphasis (2026-11-06): "robust", "put up with lower and higher latencies", "stay in sync", because
"lots of recomps have netplay issues where desync is a problem".

### 11.1 Why emulator and recomp netplay desyncs, and the answer here

| Classic cause | What happens there | The answer here |
|---|---|---|
| Timing decides the course | the emulated CPU, RSP and audio run at host-decided moments; an audio thread or a frame skip changes when the game sees what | the virtual clock: the N64 side moves only while every N64 thread waits, by N64 time, never by the host's (1.1, 1.2). The network may change *when* a retrace happens, never *what* it does. Every test below checks exactly that |
| Input taken at different moments | the input poll lands on another frame on each side | the pads for retrace N are fixed before N's event, on both sides, from one per-retrace record (1.3) |
| Different settings, plugins, cheats | one option changes the game on one side | the host's must-match settings adopted for the session; the build, ROM SHA-1, mods (as the cartridge the game reads) and paks hashed at connect (1.5) |
| Different saves | a profile or a ghost differs | the paks themselves sent from their owners and checked (1.5) |
| Time-seeded randomness, the RTC | the seed differs | none: the LCG's seed is data; osGetTime reads virtual time (1.1) |
| Host floating point | another libm or FMA | SSE only, `-ffp-contract=off`, the game's own sin and cos; `powf` at 60 replaced (1.1). CI already proves Windows = Linux on every synthetic path |
| Rollback restoring an imperfect savestate | a state not fully saved drifts | no rollback, no savestates: nothing is ever restored (1.6) |
| Render values leaking into the game | a window size changes a game value | `pc_gfx_hud_dx` held at 0 in a session; a CI grep for render values in game C (1.1, risk 13) |
| Local-only events | a pause, a menu, a pad unplugged, a hot swap | none may hold or change the game in a session; ports and accessories fixed (1.5) |

### 11.2 Any latency: the delay adapts, per side, with nothing to agree

The key property: **each player's input for retrace N is decided once, by that player's PC, and sent.** Which
retrace a press is assigned to is that PC's own business; the other PC only needs the value for N before N. So:

- **The input delay is each side's own, and it can change at any time without telling the other side.** It never
  affects sync, only feel. Growing it by one repeats the last assigned input for the slot skipped; shrinking it by
  one drops one sample (the press lands a retrace sooner from then on).
- **Each side sets its delay from what it measures:** one-way latency (RTT / 2 from pings), its jitter (a decaying
  maximum of the arrival lateness), plus a margin. It grows quickly when packets come late, and shrinks slowly (at
  most one retrace every couple of seconds) when the link has been better for a while. A setting can fix it instead
  (`--net-delay N`), and a cap stops it growing past a limit (say 30 retraces, half a second) where play would make no
  sense; past the cap the game stalls instead.
- **Range:** from a LAN (1 retrace) to across an ocean (6-10). A link that changes during a race (Wi-Fi, a DERP relay
  switching to direct) moves the delay with it.
- **Time sync** keeps both PCs at the same retrace at the same wall time, so neither waits for the other: each side
  knows the other's current retrace (in every INPUT packet) and the RTT; the side that is ahead stretches its pacing
  by up to 1 ms a retrace. A side that falls behind (a slow PC, a hitch) never makes the game skip: the game runs a
  little slower for both, then they re-even.
- **Stalls:** if the input for N isn't there by N, that PC waits for it (the game holds still for both, never
  diverges). After a stall, the pacing re-bases rather than racing to catch up. The delay controller treats a stall
  as a strong signal to grow.
- **Loss:** every INPUT packet carries every input the peer hasn't acknowledged yet (up to a window of 120
  retraces), so loss costs nothing until a whole RTT of packets is lost in a row. Duplicates and reordering are
  harmless (each input has its retrace number; the first copy wins; a different value for a retrace already known
  is a protocol error).
- **Packets go out at least every retrace and also when idle** (keepalive), so a quiet link is never mistaken for a
  dead one; the dead-peer timeout is 15 s, with "waiting for <name>" shown after 0.25 s.

### 11.3 Staying in sync: prevent, detect at once, explain

- **Prevent:** 11.1.
- **Detect within a round trip:** every INPUT packet carries the sender's hash of the curated state at each retrace
  it has run (4 bytes a retrace), and every 60 retraces the hash of the whole RDRAM window with its host pointers
  normalised (1.6). Each side compares as they arrive. The first mismatch names the retrace.
- **Explain:** on a mismatch, both sides write a desync report (the retrace, each region's hash, the curated regions'
  dump) and the session's input log. The log replays the whole session on either OS (`--replay-inputs`), so the
  first differing retrace and region can be found offline. This is the tool that turns "it desynced once" into a
  fix.
- **Prove before shipping:** the fuzzer (random inputs on both ports, many seeds, hashes every retrace), every network
  model (`RUSH_SIM_NET`: latency 0-300 ms, jitter, 10% loss, duplicates, reordering, spikes, a peer pausing for
  seconds), cross-OS replays in CI, and two retail processes on one machine over loopback through whole races.
- **What happens on a desync in v1:** the session stops with the report, rather than letting two different races go
  on. Without savestates there is no rollback to a good state. Two recoveries are possible later and are research:
  - **Replay rejoin:** the side that is wrong restarts and replays the input log from boot on the free-running
    virtual clock, then joins again. It fixes a transient fault, not a systematic one.
  - **State repair:** at a retrace every N64 thread waits at the same call on both sides; copying the host's RDRAM
    window (8 MiB, host pointers relocated) and the port's data arenas to the guest could repair a divergence that
    lives in game data. Unproven: thread stacks and libultra's side tables are outside it.

### 11.4 What changes in the phases

- Phase 0.3's `--input-delay` takes a schedule too (`--input-delay 2:0,4:600,1:1200`), so the replay tests prove that
  delay changes never change what retraces do.
- Phase 1's loopback tests run with the delay controller on, and under every `RUSH_SIM_NET` model of phase 2.
- The network library is written and tested on its own first, against a toy deterministic simulation that hashes its
  state every tick, so the protocol (redundancy, acks, time sync, the delay controller, stalls, timeouts) is proved
  before the game is attached.
