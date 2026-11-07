# Online play: racing a friend over the internet (plan)

Written 2026-11-05 against c8692ce. Line numbers are from that commit.

**Status (2026-11-06):** the user took the recommendations of section 7 and asked for the work to start, with one
emphasis: "robust, put up with lower and higher latencies, and stay in sync" (section 11). Phase 0 is in progress.
**The network library is written and tested on its own** ([NETWORK.md](NETWORK.md): `port/net`, its wire format, the
delay controller, time sync, `RUSH_SIM_NET` and the proofs against a toy simulation); the game links it since phase 1
(section 12).
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
  `paced_stop`, `paced_gate`, `paced_latency`, `ultra_selftest_paced`; the results in ULTRA.md 10). The pads gate's
  waits follow port/net's rule (over half a retrace late: re-based to the pads' arrival, never catching up), and
  port/net's clock contract maps onto `pc_pace_set_start` and `pc_pace_adjust_ns` (`pc_port.h`). In paced mode
  `audio.latency` and the audio device don't reach the game: the state hash is `--fast`'s with the null output.

**Phase 1 and the network half of phase 2 (2026-11-06, branch `net-session`, section 12):** the game is attached to
`port/net`. `--net-host` and `--net-join` run a session between two processes or two PCs: the handshake before the
game's memory (the build, the ROM, the mods, the host's settings adopted by the guest, each player's pak), the pads
source over the library at every retrace, both state hashes exchanged and compared, the session's rules (nothing holds
the game, the overlay's locks, the paks in memory only), the waiting notice, the ends and their reports, and each
session's input log. Two processes over loopback give one game, the same at every retrace, and with a fixed delay the
same as one process playing both players' presses: on every two-player synthetic path, under every network model and
in a fuzzer's random play. A session runs on the paced clock (phase 0.1) with the audio tap (0.2), `--fast` for the
unpaced tests (12.6).

**Online play from the desktop (2026-11-06, branch `net-ui`, section 12.10):** before a test between two real PCs
(Windows, over Tailscale): the host opens its window at once and shows a waiting screen (the port, this PC's
addresses, Tailscale's 100.x one and its MagicDNS name first; Esc cancels) instead of waiting with no window; the
guest shows who it joined as soon as its window opens; every way a session can end before the race says why in a
message box; launchers in the release folders (`Host online race.bat`, `Join online race.bat`, `host-online.sh`,
`join-online.sh`); F1's main page ends with two Online rows in a session (the other PC, the round trip, the input
delay, stalls, the last hash check).

**The F1 Online page and replays (2026-11-07, branch `net-online`, section 13):** phase 6's comforts. F1 > Online hosts
(a port, a join code), joins (an address typed in, or one of the last 8 hosts joined, `net.recent`) and watches a
replay (each session's input log), lists this PC's addresses as the waiting screen does and copies one to the
clipboard; in a session it has the status and Leave the session. Lockstep starts at boot, so these restart the
program: exec on Linux, CreateProcess on Windows, with this run's options. A session's input log now replays offline
as it is (`--replay-inputs` alone takes the session's settings, rules and paks), to the session's hashes.
**Own view (2026-11-07, branch `net-view`, section 14, phase 4):** `net.view = own` (F1 > Video > Online view) shows
each PC its own player's view alone, filling the window, with that player's HUD at the screen's edges; render-only, so
each player chooses their own and the two games stay one, hash for hash. The game's culling leaves no holes at the top
or bottom of the taller view (the survey, 14.2).

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
  (Built: a rejoin window of 300 s once the game runs, section 15.)

**Rejoin.**

- There are no savestates, so there is no rejoin into a running session. The cheap alternative: replay the
  session's input log from boot on the free-running virtual clock, then switch to paced lockstep.
- On the synthetic cartridge that is about 70 times faster than real time on rocky (ULTRA.md 1) and 27 times on the Pi
  (E1). On retail it is unmeasured.
  The peer that stayed must wait while the other catches up.
- Later phase, if ever. (Built on branch `net-rejoin`, for both players and for a desync: section 15.)

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

**Done (2026-11-07):** section 14.

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

- An F1 "Online" page: host, join, code, status. **Done (section 13).**
- A quick-race macro.
- Saving replays (the input log already is one). **Done (section 13): watched from the page, kept in replays/.**
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
  on. Without savestates there is no rollback to a good state. (Since branch `net-rejoin` the first recovery below is
  built, `net.recover`, on by default: section 15; a desync that repeats in the rebuilt game still stops with the
  report.) Two recoveries are possible later and are research:
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

## 12. Status: the session (phase 1, and phase 2's network half)

Written 2026-11-06 on branch `net-session` (from 543e5b0). The game attached to `port/net` ([NETWORK.md](NETWORK.md))
over the pads gate and the state hash of [LOCKSTEP.md](LOCKSTEP.md). The players' side is the README's "Playing
online"; this is how it works and what was proved.

| Where | What |
|---|---|
| `port/app/netplay.c`, `netplay.h` | the session: connecting, the host's settings and the paks, the pads source, the hashes, the waiting notice, the ends, the input log |
| `port/app/netwait.c` | the host's waiting screen (12.10), and the window's events before the game |
| `port/main.c` | the options, the handshake's place in the boot, the settings text and this PC's pak, the session's paks in the SI, the paced clock and the paced audio in a session |
| `port/config/config.c` | `pc_must_match`: the one list of the settings the game reads (a replay's header, the guest's adoption, the overlay's locks: `pc_settings_session_rule`); `[net]` port, delay, code |
| `port/input` | `pc_input_set_session`: nothing holds the game; the overlay's locks; `pc_menu_notice`, a notice over the frame |
| `port/ultra/si.c` | `pc_si_set_pak_image`: a pak in memory with no file or folder behind it |
| `port/net/net.c` | `net_end_text` (the reason to the peer), `net_linger` and the drain after a BYE (the peer reaches the retrace the other left at) |
| `tools/port/net_session_check.py` | the two-process tests (12.7) |

### 12.1 Options and the flow

`--net-host [PORT]` (0: any free port, in the log), `--net-join HOST[:PORT]` (names, MagicDNS, 100.x, `[v6]:port`),
`--net-delay auto|N`, `--net-code CODE`, `--net-timeout S`, `--net-bind ADDR`; `[net] port, delay, code` in the
settings file. Hosting or joining is never a setting. Without a `--net-*` option nothing of this runs: the main loop's
path, the paks, the audio and the clock are as before (12.7, `synth_compare.py --same`).

The flow, in the boot's order (`netplay.h`):

1. **Connect**, right after the ROM and `mods_prepare`, before `game.memory` maps the window. Each side sends a
   settings text and its controller 1's pak. The guest waits here for the host's (it adopts them before its memory is
   mapped); the host only opens its port, sets up as usual, opens its window and waits with the waiting screen up
   (`pc_net_wait_guest`, 12.10), then checks the guest's pak. The text: `build=` (the commit, `-dirty` when it had changes),
   `rom=` (SHA-1), `mods=` (a hash of each wave's file the mods give, or `none`), then the adopted settings of
   `pc_must_match` as `section.key=value`, and `#` lines (the mods' summary, the version) that are only said. The
   library compares the build first (a guest of another build is told so; the host turns it away and waits on).
2. **The guest checks and adopts.** The ROM and the mods must be the host's ("the host's ROM is X, this PC's Y"; "the
   host's mods are A, this PC's B"), else `NET_END_REJECTED` with a short reason for the host. The settings are
   taken into this run's settings only (never the file), each change logged (`game.cars 7 -> 12 (the host's, for
   this session)`); when `game.memory`, `expansion_pak` or `cars` changed, the mods' memory plan is made again.
3. **The paks.** This PC's pak is its controller 1's as its saves setting gives it (`game.saves = files`: built from
   the folder; `pak`: the `.mpk`, a new or N64-order one a blank pak), or none (an empty file) without a memory pak.
   The other's is checked with `pc_pak_image_check`; a damaged one ends the session.
4. **The setup goes on** with the adopted settings: the memory (with the mods' raise), the Expansion Pak flag and
   every `pc_*_set`. The paks go into the SI: port 1 the host's, port 2 the guest's, in memory only
   (`pc_si_set_pak_image`); ports 3 and 4 none; no rumble pak. Two paks with one ID (both made by a controller 1) are
   one pak moved to the game, so port 2's copy takes controller 2's ID, on both PCs alike. Nothing is written back.
5. **Begin**, just before the boot: `must_match_text` with the session's paks in (its `game.pak*` lines say what the
   session has, `session.rules=on`); its compared lines go to the log and their xxHash64 is kept (12.3). The input
   log starts (12.5); hashing goes on; the pads source is set.
6. **Start.** `net_accept` only now, so the host's START, and its start time, come when both have set up. The start
   time is the paced clock's N64 time 0 on each PC (12.6), which holds the boot until it (with `--fast`, a sleep).
7. **The end** (12.4).

### 12.2 The lockstep

The pads source (`pc_lockstep.h`, `net_start_thread`'s service thread doing the network):

- `local(n)`: this PC's controller 1 (`local[0]`, after all shaping, idle while the overlay is open) goes to
  `net_submit` for the port this PC owns. The library assigns it to retrace n + D; `--input-delay` isn't used.
- `pads(n)`: `net_pads(n, ..., 4 ms)`, woken at once by the service thread's condition when input arrives; then
  `pc_pads_signal`, so the gate asks again at once and the window's events are pumped every 4 ms of waiting. Ports 1
  and 2 are connected and 3 and 4 not, whatever this PC's pads are (one unplugged changes nothing); `rumble` is 0 (no
  accessory follows it). `RUSH_SIM_NET`'s pause (`net_frozen`) holds it too, and the sample of a retrace that came
  during the pause is submitted when it ends, as a stopped process's would be (else the peer takes the pause for its
  transit, and its delay goes to the cap).
- `hash(n)`: the curated state hash folded to 32 bits, every retrace (`net_hash`), and the full hash (window, own
  storage, decoupled data) when it was taken (`net_window_hash`): every 60 retraces (`--log-hash N,W`'s W when
  given). 240 retraces of curated bytes are kept for a report.

### 12.3 The final check at retrace 0

Retrace 0's window hash is the full hash XORed with the must-match text's hash. Anything the handshake doesn't see
that makes the two setups differ (the paks as installed, the window's size, a test model in the environment, a
guest that skipped its checks) ends both at retrace 0 with "the two PCs' setups differ"; each PC's log has its
must-match text to compare.

### 12.4 Rules, waiting and ending

- **Nothing holds the game**: the pause key and focus loss (`pause_on_focus_loss`) say "an online session isn't held";
  F1 opens over the running game, "online" in its title, this player's pads idle while it is open (that idle input is
  what is sent). Its items of the settings the game reads say "locked in an online session" (a preset's and
  Defaults' share of them too); the Saves page's import, pin and delete are locked (the session's paks are copies,
  and no saves folder is behind them). `pc_gfx_hud_dx` gives the game 0 (`--session-rules`).
- **Waiting**: after 250 ms without the peer's input, "Waiting for the host..." (or guest) with the seconds and the
  quit key, over the last frame (`pc_menu_notice`, drawn by the overlay's UI hook, `pc_gfx_redraw`). After a second
  of silence it says what the wait is and how long it may last: the peer silent or its game closed ("Waiting for the
  guest to come back... Nothing from it for 0:12; waiting 4:48 more."), a desync it rebuilds from, the log on its way,
  its replay's percentage (section 15). The quit key (Escape) leaves; closing the window is the game going, not the
  player leaving (section 15).
- **Ending**: this side leaving at a retrace of its own (a limit, the quit key, the overlay's Quit, Ctrl-C) waits up
  to 0.5 s until the peer has its last inputs (`net_linger`), then BYE; the peer runs on to the retrace the other
  ended at (its inputs are there), or at most the delay past it if it was ahead when the BYE came, and ends, "the
  host left" (or guest). A test whose two runs have one limit ends both at it. The window closed, or the system
  ending the program (SIGTERM, SIGHUP), is BYE `gone`: the peer waits for the game to come back (section 15). A
  timeout (the rejoin window once the game runs, `net.rejoin`, 300 s; 15 s before it; `--net-timeout` sets both), a
  desync that isn't recovered from (section 15), a mismatch or a protocol error ends both; the reason is logged and
  shown (a message box with a window), with `net_stats_line`. Exit 0 when a player left, 3 otherwise.

### 12.5 The input log and a desync's report

Every session writes `netplay/DATE-ROLE.pads` in the data folder (pc_padfile, the must-match text as its header) and
the session's paks beside it (`.p1.mpk`, `.p2.mpk`). On a desync each side writes `DATE-ROLE.desync.txt`
(`pc_hash_report` of the retrace, from the kept bytes). The log ends with the command that replays the session in one
process: `--replay-inputs` and the log, which takes the session's settings and paks itself (13.3; before, the command
gave them all as `--set`, and the paks as files the replay wrote to); replayed to the retrace on each OS with
`--hash-report`, `hash_compare.py --reports` names the words that differ.

### 12.6 The clock and the audio

A session runs on the paced clock ([ULTRA.md](ULTRA.md) 10): `--fast`'s virtual clock, each move at its wall time.
`--fast` with a session keeps it unpaced (as fast as both PCs and the network allow: the tests). Attached as port/net's
clock contract says (`net.h`): `pc_pace_set_start(net_start_time)` (`pc_net_start`: N64 time 0 at the agreed start, on
`pc_real_ns`'s clock, the session's own), so both PCs' retrace N comes at start + N retraces; each retrace, once its
pads are known, `pc_pace_adjust_ns(net_pace_adjust_ns(s))` (`src_pads`); and the loop's own re-base when a retrace's
pads came more than half a retrace late (the pads gate's rule, ULTRA.md 10). `RUSH_PACE_SPEED` (tests) shortens the
session's retrace for the library's time sync and delay too.

The audio: a session's AI keeps the null output's queue on the N64 clock whatever the device (`pc_audio_set_paced`,
[AUDIO.md](AUDIO.md) 11), and the player's own device plays from the tap. So `audio.latency` is each PC's own in a
session (the tap's FIFO): `pc_must_match` marks it so (`adopt` -1). It isn't adopted or locked, and the must-match
text says the default for it whenever the audio is paced (a session, or `--paced`), since that is the queue the game
then has (a `--fast` replay with the default takes the same course).

### 12.7 Proofs

`tools/port/net_session_check.py`, ctest `net_session_*` (rocky, x86-64, Debug, unless said):

| Test | Result |
|---|---|
| `net_session_<path>`: race2p, battle2p, battlequit, memhigh2p, race2p12; pause, raceend, menu with the guest idle | host = guest = one process with `--input-delay 2`, line for line: 2,689-6,975 trace lines, 1,325-3,425 retraces hashed each (every retrace, the full hash every 30) |
| `net_session_sim_<model>` (lan20, wan80, bad150, shift, pause, burst; to the race and 2 s into it, D 3-14) | the clean single process's trace under each: 1,685-1,697 retraces; up to 973 stalls, 11.5 s of waiting, all of it changing when retraces happen, nothing of what they do |
| `net_session_auto` (wan80, the adaptive delay 6-7) | host = guest, and the host's input log replays in one process to the same trace |
| `net_session_paced` (race2p to the race and 2 s into it, the session's own clock: no `--fast`) | at 4 times the pace (`RUSH_PACE_SPEED`) with delay 2: one `--fast` process's trace, 3,428 lines (paced takes `--fast`'s course); at the real pace under wan80 with the adaptive delay (6-7): host = guest, 60.000 and 59.998 retraces a second of wall time, the gate never late (the Pi 5, RelWithDebInfo) |
| the same at the real pace under each model (the Pi 5) | host = guest under each; retraces a second, without the time re-based at a stall: clean 60.000 (delay 1), lan20 60.000 (2), bad150 59.95 (12-14; 1 stall), shift 59.92 (1-13; 2 stalls, 0.55 s, at its jump to 200 ms), pause 59.97 (2; the 2.98 s pause re-based), burst 60.00 (3-7; 2 stalls) |
| `net_session_fuzz` (3 seeds, 120 s of game time each) | the same hash on both at every retrace: seeds 1 and 2 all 7,201 retraces, 12-13 game states, the overlay opened 14-15 times over the running game; seed 3 to retrace 4038, where the game hangs alike on both (12.8) |
| the soak: `fuzz --seeds 20 --seconds 300` (the Pi 5, RelWithDebInfo) | 278,315 retraces, every hash the same on both; 11 seeds ran their 5 minutes, 9 reached the game's hang (12.8), on both alike |
| `net_session_desync` (the guest's random seed changed at retrace 1000) | both end "desync" at retrace 1000, exit 3; each report written; `hash_compare.py --reports` names `rng+0x0` |
| `net_session_mismatch` | another ROM, build or mods refused before the game, saying what differs; a test model on the guest only ends both at retrace 0, "the two PCs' setups differ" |
| `net_session_adopt` (race2p12) | the guest takes game.cars 7 -> 12, audio.voices 64 -> original, video.draw_distance far -> original, logged; both new paks in (port 2's re-IDed), neither file written; a race of 12 cars, the same trace |
| `net_session_ends` | the guest killed: the host ends "timeout" 3.0 s later (`--net-timeout 3`); the guest leaving at its limit: the host ends "peer-left" at the guest's last retrace or within its delay past it, exit 0; joining nobody: "no-answer" after 2.2 s |
| `frontend.menu.session` | the overlay holds nothing and its pads are idle; a locked setting, a free one, Defaults; the notice drawn and gone |
| the whole suite, rocky, with main's paced clock merged in | Debug: 561 of 562, `net_session_ends` failing on a check too strict (the peer one retrace past the guest's last, within its delay: e7c4e28 corrected the check), then passing; RelWithDebInfo: 562 of 562 |
| the default off: `synth_compare.py --same`, 543e5b0's program against this branch's (main merged in), every path | Debug 62 of 62 the same, RelWithDebInfo 62 of 62 |
| the Windows cross build (mingw-w64 on the Pi) | everything links; `rush2049pc.exe` imports `WS2_32.dll`, `pe_check.py --dlls` passes |

### 12.8 Found on the way

- **A hang in the game, reached by the fuzzer.** Random two-player input in a race's pause menu hangs the game: the
  same on both PCs at the same retrace each time (the virtual clock's "no retrace for 30 s"), so the session stays
  one game. On the Pi's soak (20 seeds of 5 minutes) 9 seeds reached it, always in the pause menu's controls or audio
  screen (pause state 4 or 3); on rocky, seed 3 of the ctest's at retrace 4038. The session's input log replayed in
  one process hangs at the same retrace, with 543e5b0's program too (no session code): it is the game's, or the
  port's, on the synthetic cartridge at least. Where (gdb, seed 11's replay): the controls screen's step through a
  control's values, `do { controls[slot]--; } while (!func_800D8078(p))` (`src/blob/groups/controlscr/caller.c`
  251-268, the same for ++), which never ends when no value passes the predicate; the predicate reads row
  `D_8013C068[D_8014A118[car].row]` while the loop changes `D_8013C068[input_rec0[p].controller]`, and its table
  `D_80114060` (`gControlValid`) is cartridge data. Not looked into further here (game C). The fuzzer says such a
  hang (the same on both) and goes on; `net_session_check.py fuzz --first 3 --seeds 1` reaches it on rocky's
  cartridge.

### 12.9 Not done

- Phase 3: two real PCs (Windows against Linux) over Tailscale.
- ~~Hosting and joining from inside the game (an F1 "Online" page, phase 6)~~: done in section 13; the launchers and
  the waiting screen (12.10) stay.
- A pak imported from another player's ghost keeps its own name on the session's pak (the game's note names are read
  once the game's data is loaded, after the handshake).
- Rumble: no rumble accessory in a session; the local player's motor to its own controller can come later.
- Rejoining and recovering from a desync: done on branch `net-rejoin` (section 15); what isn't is in 15.7.

### 12.10 Online play from the desktop: the host's window, the ends, the launchers, F1's rows

Written 2026-11-06 on branch `net-ui` (from 4b63166), before the first session between two real PCs: the user on
Windows and a friend, over Tailscale. On Windows `rush2049pc.exe` is a GUI program with no console, so a host that
waited for its guest before its window (up to 10 minutes) showed nothing at all and looked hung or crashed.

**The host's window first.** The host adopts nothing of its guest's, so its setup needs nothing from the guest.
`pc_net_connect` only opens its port (a port that can't be opened is said at once: "Couldn't host an online race on
UDP port 24049: bind: error 10048 (the port is in use). Is another copy of the program hosting already?"); main maps
the memory, loads the game's data and opens the window as without a session; then `pc_net_wait_guest` waits, before
`pc_input_init`. A guest that comes meanwhile is answered by the network thread as before. Then the guest's pak is
checked (`pc_net_paks`) and the setup goes on (12.1, 4-6). The guest is unchanged: it adopts the host's settings
before its memory is mapped, so its window can't open before the host has answered (within a second when the host is
there; when not, "no answer" after the timeout, 15 s, in a box).

**The waiting screen** (`port/app/netwait.c`) is drawn with the overlay's font over a cleared frame (`pc_gfx_redraw`
before the game has presented one), four times a second and at once after a window event. Its events come through
the renderer's event hook: port/input isn't set up yet, so no hotkey and no F1 overlay, which could change a setting
the guest has been sent. It says:

- "Hosting an online race on port N." and "Waiting for a player to join... m:ss"; once one is joining, "A player
  joined from ADDRESS: getting their settings...";
- "Your friend joins with one of this PC's addresses:", best first: Tailscale's 100.x and under it its MagicDNS name,
  Tailscale's IPv6 address, the local network's (192.168, 10, then 172.16-31, which containers and WSL often take),
  public ones; at most six, the rest in the log (every one, with its interface on Linux), containers' and virtual
  machines' bridges (docker0, br-..., virbr...) only there; "(no Tailscale address here: is Tailscale running?)"
  when none is Tailscale's;
- "They start "Join online race" and type one of them in."; the port to add when it isn't 24049; that a join code is
  set; on Windows "If Windows asks, allow the program on private networks.";
- "Esc cancels." Esc or closing the window cancels (exit 0, no box); Ctrl-C in a terminal stops it as before; after
  10 minutes the host gives up, with a box saying so.

The addresses are `net_local_addrs` ([NETWORK.md](NETWORK.md) 2): getifaddrs on Linux; on Windows getaddrinfo on the
computer's own name, which gives its permanent addresses, Tailscale's among them, with nothing beyond ws2_32. The
MagicDNS name is asked of Tailscale's own resolver (100.100.100.100, answered by tailscaled on the PC itself) as the
PTR of this PC's 100.x address, then of the system's resolver (getnameinfo), then taken from the computer's canonical
name; a name under ts.net counts. On a thread, so the screen shows at once and the name joins it. Tailscale's own
program isn't used. On the Pi 5 and on rocky Tailscale's resolver gave `nvmepi3.tailfe03a6.ts.net` and
`rocky.tailfe03a6.ts.net` at once; getnameinfo gave them too there, but on Windows a PC's own address may come back
from the system's resolver as the bare computer name, which is why Tailscale's resolver is asked first. The window's
title says "hosting an online race" until port/input sets its own, and the window is raised so that Esc reaches it,
not the console a launcher started it from.

**The start notice.** Once its window is open (the guest), or once a guest has joined (the host), `pc_net_starting`
puts up the overlay's notice, "Joined the online race at my-pc (100.101.102.103:24049)." or "A player joined from
100.101.102.104:41641.", over "Starting when both PCs are ready... Esc leaves.", drawn at once and four times a second
while `pc_net_start` waits for the other PC, until the first retrace's pads. There the window's events are only looked
at (`SDL_PeepEvents`): Esc or closing the window leaves (exit 0), and the rest stay queued for the game's loop.

**Every end before the race says why**, in a message box when the program has a window (on Windows always; before
its window is open the box needs none), and in the log:

| What ended it | What the player reads |
|---|---|
| the address isn't one | "The address to join, 'x', isn't one: it is a name or an address (my-pc, 100.101.102.103), with :PORT after it when the host's port isn't 24049" (spaces around it are trimmed first: the launcher's prompt) |
| the host's name isn't found | "Couldn't join the online race at my-pc: my-pc: Name or service not known." |
| the host's port can't be opened | "Couldn't host an online race on UDP port 24049: bind: error N (the port is in use). Is another copy of the program hosting already?" |
| nobody answers the guest | "Couldn't join the online race at 100.x: no answer from 100.x:24049 in 15 s: check the address, the port, the join code and the host's firewall." |
| another build | the guest: "Couldn't join the online race at X: the host runs build A; this is B: both players need the same build."; the host turns it away, logs it and waits on |
| another ROM, other mods, settings the guest can't use | the guest: "Can't join the online race at X: the host's ROM is ... both players need the same ROM."; the host: "The online race ended before it started (rejected): the peer ended the session: the guest's ROM isn't the host's (its SHA-1 differs)." |
| a damaged pak | "The online race can't start: the guest's controller pak can't be used: ..." |
| nobody joins in 10 minutes | "Nobody joined the online race in 10 minutes, so this PC stopped hosting. Host again when your friend is ready to join." (exit 3) |
| the other player leaves before the start | "The online race ended before it started (peer-left): the other player left the session (local)." (exit 0) |
| the network, before the start (a timeout, a mismatch) | "The online race ended before it started (timeout): nothing from the other player for 15 s: the connection was lost." |
| Esc, closing the window, Ctrl-C | nothing more: the player's own choice (exit 0; Ctrl-C 130) |
| during the race | as before (12.4) |

Anything else that stops the program with an error (game.memory that can't be mapped, no OpenGL) has its own box, or
port/app's last-lines box for a program nobody sees the console of (log.c's `error_box`); started from a launcher, its
console shows the lines and waits for a key.

**The launchers** are in `dist/`, installed by `cmake --install` with `RUSH_INSTALL_PORTABLE` (so in the release zip
and tarballs, next to the program; release.yml checks they are there, and gives the .bat files CRLF line ends, as
`.gitattributes` keeps them):

- `Host online race.bat`: `"%~dp0rush2049pc.exe" --net-host %*`, then `if errorlevel 1 pause`. cmd waits for a GUI
  program a batch file runs (`start`'s documentation), and the program attaches to the batch's console and prints
  there (port/app/startup.c): the console behind the game shows the log, and after an error stays until a key.
- `Join online race.bat`: `setlocal`, then `set /p` asks "The host's address (their Tailscale name or 100.x
  address): "; nothing typed ends it; then `"%~dp0rush2049pc.exe" --net-join "%HOST%" %*` and the same `pause`.
- `host-online.sh`, `join-online.sh`: the same in POSIX sh (`exec`, `read -r`).

Options given to a launcher are passed on: `--net-code`, a port after the host's.

**F1's Online rows.** In a session two rows, which can't be selected, read the session's numbers each time the
overlay is drawn (`pc_menu_set_online`, `pc_net_status` over `net_stats`): "Online: the guest, 100.101.102.104:41641"
and "RTT 23 ms, delay 2, stalls 1, hash ok at 1234": the round trip, this side's input delay, the stalls counted so
far, and the last retrace whose state hashes both PCs compared alike (`hash_checks` counts them from retrace 0, in
order). They ended the main page here; since section 13 they head F1's Online page, and the main page's Online item
shows the round trip and the delay (its footer the other PC), so the main page keeps to 14 items and doesn't scroll.

Proofs (rocky, x86-64, unless said):

| Test | Result |
|---|---|
| `net_session_wait` (new) | headless at 1280 x 720 the screen is drawn and saved (`RUSH2049_TEST_NET_WAIT_SHOT`: the panel, 16,506 pixels of white text, rocky's 100.x and IPv6 Tailscale addresses, its MagicDNS name `rocky.tailfe03a6.ts.net`, two local network addresses, "(and 6 more: the log lists them)": its Docker bridges), then cancelled as Esc would: exit 0, no box, nothing of the game run, 0.5 s; nobody joining (`RUSH2049_TEST_NET_HOST_WAIT=2`, `--window` with no display, the boxes printed): "Nobody joined the online race in 2 seconds, so this PC stopped hosting...", exit 3 after 2.3 s |
| `net_session_ends` | as before, and the no-answer guest's box: "Couldn't join the online race at 127.0.0.1: no answer from 127.0.0.1:56410 in 2 s: check the address, the port, the join code and the host's firewall." |
| `frontend.menu.session` | as before, and the Online rows: read from the status when drawn, gone with it, stepped over by a page down, down and up |
| `net_unit` | as before, and the address kinds, this PC's addresses, the PTR question and its answers |
| every other `net_session_*` | pass unchanged: the host now sets up and opens its renderer before its guest joins, and the traces are the same |
| a guest joining 3 s into the host's wait (by hand, `--fast`, 5 s) | both reach the game and agree (the same hashes), host exit 0, guest "peer-left" exit 0 |
| a paced host at 24049 with a join code (by hand) | the screen as above, without the port line, with "A join code is set: your friend gives the same code." |
| the whole suite | Debug 563 of 563 (`assets_rom` skipped: the retail ROM), 40 min at -j3; RelWithDebInfo 563 of 563 |
| the default off: `synth_compare.py --same --runs 2`, main's program (4b63166) against this branch's, every path | Debug 62 of 62 the same, RelWithDebInfo 62 of 62 (`pcsetup` left out by the tool: the host's clock only) |
| the Windows cross build (mingw-w64 on the Pi, `RUSH_FETCH_SDL2`, RelWithDebInfo) | `rush2049pc.exe` and `net_test.exe` link; the exe imports only Windows' own DLLs and WS2_32 (`pe_check.py --dlls` passes); netwait.c's SDL part and net_sock.c's getaddrinfo, getnameinfo, gethostname and inet_pton are in it |
| `cmake --install` with `RUSH_INSTALL_PORTABLE` | Windows: both .bat files, CRLF; Linux: both scripts, executable; `host-online.sh --version` passes its option on |

Not tried: a real window (rocky and the Pi's tests are headless; the Esc key, the window's close, the title and the
raise are read, not run), Windows itself (CI builds and runs the tests; the .bat files were checked by reading), and
two PCs over Tailscale (the user's test is the first).

## 13. Status: the F1 Online page, the restart, replays (phase 6's comforts)

Written 2026-11-07 on branch `net-online` (from 41b4aee). Players hosted and joined from the desktop's launchers
(12.10); now they can from inside the game too, pick a host they joined before, and watch a session again. The players'
side is the README's "Playing online"; [FRONTEND.md](FRONTEND.md) has the page among the overlay's.

| Where | What |
|---|---|
| `port/input/menu.c` | the Online page and its pages (Host a race, Join a race, Replays, a replay's own), text typed into an item, the session's rows |
| `port/input/input_sdl.c` | SDL's text input, on only while an item is typed in; Backspace, Enter, Escape, Ctrl+V and Shift+Insert there |
| `port/config/online.c`, `port/include/pc_online.h` | without SDL or the game: the address and port checks, `net.recent`, the restart's command line, Windows' quoting |
| `port/app/netmenu.c`, `netmenu.h` | main's side of the page: this PC's addresses, the replays, Keep, the restart (exec, CreateProcess), `--net-return`, a session's log replayed offline |
| `port/app/netaddr.c`, `netaddr.h` | this PC's addresses best first and the MagicDNS name, out of `netwait.c` (the waiting screen and the page share them) |
| `port/app/netplay.c` | `pc_net_end_local` (whose choice the end was); the replay hint shortened (13.3) |
| `port/main.c` | `--net-return`, the page's side installed, a guest's host kept in `net.recent`, a session's log replayed, the restart at the end |
| `port/config/config.c` | `[net] recent` |
| `tools/port/net_menu_check.py` | the restarts and the replays with real processes (13.5) |

### 13.1 The page

F1 > Online, after Saves on the main page. Out of a session:

- **Host a race**: Port (`net.port`'s, typed: digits, 1-65535), Join code (`net.code`'s, optional), Host now.
- **Join a race**: Address (typed), Join code, Join now, then the hosts joined before (`net.recent`, the last first):
  choosing one puts it in Address and selects Join now.
- **Watch a replay**: Watch the last session's (the newest log of `netplay/`, when this build can play it), then
  every input log of `netplay/` and `replays/`, the newest first, each with its length; a log's own page says when it
  was played, its length, whose log it is or why it can't be watched (another build or ROM, its paks missing), its
  file, and has Watch and Keep this replay.
- **This PC's addresses**, as the host's waiting screen lists them (`netaddr.c`): Tailscale's 100.x first and under it
  its MagicDNS name (looked up on a thread; the page fills in when it comes), then the local network's and public
  ones, bridges left out. Enter on one copies it to the clipboard; **Copy my address** copies the first, with
  `:PORT` when the Host page's port isn't 24049.

In a replay being watched, **Stop watching** comes first. In a session the page starts with the two status rows of
12.10 and **Leave the session**, chosen twice ("Choose Leave again: the other player is told"), then the addresses;
Host, Join and Watch aren't there (one session at a time). The main page's Online item then shows "RTT 23 ms, delay 2",
its footer the other PC.

**Typing.** Enter, A or a click on a text item starts typing in it: `pc_input_live_text(1)` turns SDL's text input
on (it is off otherwise, so no IME pops up over the game), and SDL_TEXTINPUT's text goes to `pc_menu_text`: printable
ASCII only (the overlay's font), digits only for the port, at most 127 characters for an address and 63 for a code;
a paste (Ctrl+V, Shift+Insert) takes the clipboard's first line. Backspace (it repeats) and Delete take the last
character off, Enter is done (an address or a port without the spaces around it), Escape puts back what was there,
Up and Down end it and move, a click elsewhere ends it, F1 closes the overlay. The value shows the end of the text
with a cursor while it is typed, and the footer the whole of it. A controller can't type; the recent hosts are its
way to join.

**Nothing changes the game before a choice.** The fields are the page's own (not settings, not saved); only Host now,
Join now, Watch, Stop watching and Leave act on the game, each by restarting the program (Copy writes the clipboard,
Keep a file). The defaults are a normal start: `net.recent` empty, and the page itself does nothing until a choice.

### 13.2 The restart

Lockstep starts at boot: the handshake comes before the game's memory is mapped, and the guest adopts the host's
settings before that (12.1). So a choice on the page doesn't start a session in this run: main's `act`
(`netmenu.c`) takes the choice down, the overlay stops the main loop as its Quit does, main ends the run as always (a
session's BYE, the saves written, the window, the audio, the log), says the restart's command line in the log, and
runs the program again:

- **Linux: `execve` of `/proc/self/exe`.** The same process: a shell, a launcher script or a test that started it
  waits on and sees its output, as before. The other threads go with the exec.
- **Windows: `CreateProcessA` of the program's own path** (`GetModuleFileNameA`), the command line quoted as the C
  runtime parses it back (`pc_online_quote_windows`: an argument with a space, a tab or a quote, or an empty one, in
  quotes; backslashes doubled before a quote and before the closing one; a quote after a backslash). When this run had
  a console or its output went to a file or a pipe (a launcher's `.bat`, a script, a test, CI), the new program gets
  the same standard handles and this one waits for it and exits with its exit code: the launcher's `if errorlevel 1
  pause`, a console that stays until the game ends, and a test's pipe keep working, as with exec. Started from Explorer
  (nobody sees the output) this one exits at once. A waiting run keeps its memory (the RDRAM window: 8 MiB, more with
  `game.memory`) while the new one plays; each restart from the page nests one more.

**The command line** (`pc_online_args`, `port/config/online.c`): this run's own, without what a restart replaces,
wherever it is: a session's options (`--net-host [PORT]`, `--net-join`, `--net-code`), a replay's and a record's
(`--replay-inputs`, `--replay-force`, `--record-inputs`, `--input-delay`, `--hash-report`) and the press scripts'
(`--press`, `--press-file`, `--press-via`: a script that chose Host would choose it again), each with its value (a
table of main's options that take one, so a value is never taken for an option); then `--rom` and the ROM loaded when
the command line named none (one chosen in the file dialog without a settings file to keep it in); then the overlay's
changes not saved, as `--set SECTION.KEY=VALUE` (`pc_menu_unsaved_list`: what the player set before choosing Host
plays in the session), and fullscreen when F11 left it otherwise than the settings say; then `--net-return` and the
restart's own: `--net-host PORT --net-code CODE`, `--net-join ADDRESS --net-code CODE` (an empty code given, so a
`net.code` in the file isn't used when the page's field was emptied), or `--replay-inputs FILE` with `--paced` (but
with `--fast` or `--paced` already there: the tests). **`--net-return` and every option after it are that restart's
own**, so the next restart (leaving the session, its end) leaves them out and the run is the one before it again;
the `--set` changes come before it and stay. The settings file, the mods folder, the window and every other option
stay as given; the working directory is the same.

**The environment.** main sets `RUSH_GFX_SCALE`, `RUSH_GFX_DRAW_DISTANCE` and the others from the settings in effect
(port/gfx reads them), and a new run takes those as the environment's layer over the settings file: a guest back in
the game after a session kept the host's draw distance (`net_menu_restart` caught it). `pc_net_menu_args` keeps the
environment as the program started (`environ`, or Windows' block from `GetEnvironmentStringsA`) before main sets
anything, and the restart gets that one (`execve`'s, `CreateProcessA`'s `lpEnvironment`).

**`--net-return`: back to the game.** A run the page started (Host now, Join now, Watch) has `--net-return`. When its
session ends or doesn't start, it restarts without the session (`pc_net_menu_session_end`), unless the player stopped
the program: the window closed (`pc_net_window_closed`, from the waiting screen's and the start's event peeks, not
Esc), Ctrl-C, or in the game an end of this side's own (`pc_net_end_local`: the quit key, F1's Quit, the window, a
limit). So the other player leaving, a timeout, a desync, no answer, the host refusing the guest, the waiting screen's
Esc and nobody joining in 10 minutes all come back to the game, after their message box. Leave the session and Stop
watching restart without the session in any run (a launcher's too: back to the game). A launcher's session is as
before: it ends, and the program with it.

**The log.** Each run opens the log afresh (12.10's `rush2049pc.log`, the one before kept as `rush2049pc.old.log`), so
after a session started from the page and the way back, the session's log is `rush2049pc.old.log`. The restarting
run's last lines say `rush2049pc: online page: Restarting to host on port 24049...` and `rush2049pc: restarting: ...`
with the command line as a shell would take it.

**The hosts joined before.** A guest whose host answered (its settings arrived: the handshake) puts the address as
typed first in `net.recent` in the settings file it read (`pc_net_menu_remember`; not with `--no-config`): from the
page or a launcher alike. The list holds 8; the same host again (letters' case, a trailing dot and `:24049` being the
usual port aside) moves to the front instead of being said twice; an entry that isn't an address is dropped.

### 13.3 Replays

**Before this branch a session's input log didn't replay offline as it is.** On rocky with main's program (41b4aee), a
session of two processes with each player's pak in, then `rush2049pc --replay-inputs HOST.pads` with nothing else:
refused, "recorded by another build, cartridge or settings", five lines differing: `game.pak2` (memory in the file,
here auto), `game.pak3` and `game.pak4` (none, here auto), `pak2.image` (the guest's pak, here none) and
`session.rules` (on, here off); a player's own settings file would add each adopted setting where it isn't the host's.
It replayed with the command the session's log ended with (12.5: `--session-rules --saves pak`, every adopted setting
as `--set`, and the `.p1.mpk` and `.p2.mpk` as `game.pak_file`, which the replay then wrote to, so a second replay
needed fresh copies).

**Now it does** (`pc_replay_session`, `netmenu.c`): `--replay-inputs` of a log whose header says `session.rules=on`
takes, for the run only, the session's settings the game reads (`pc_must_match`'s adopted ones and the accessories;
each change logged: `replay: game.cars original -> 12 (the session's)`), its rules (`--session-rules`), its audio
queue (the paced audio's, `audio.latency` at its default, as in the session), and its paks: `NAME.p1.mpk` and
`NAME.p2.mpk` beside the log, each checked against the header's `pak1.image` and `pak2.image` hashes ("isn't the pak
the session had", "isn't beside its input log"), put in the SI in memory as the session had them, so nothing is
written to them. Then the replay's own check (LOCKSTEP.md 3) compares the rest as before: the build, the ROM, the mods,
the memory, the test models. A recording made with `--record-inputs` is replayed as before.

**Watch a replay** restarts into `--replay-inputs FILE --paced`: the paced clock, real time as it was played and
`--fast`'s course to the bit (12.6). The list checks each log first (`log_header`, the header alone): this build's,
this ROM's, a session's, its paks there; the others show "(can't)" and why. **Keep this replay** copies the log and
its paks into the data folder's `replays/` under the same name (`2026-11-07_210412-host.pads`: the date, the time and
whose log); the list shows the kept ones too. The track isn't in the name: the log doesn't hold it, and a session goes
through menus and races, so it would take reading the game's memory during the session.

### 13.4 Small robustness

- **An address that isn't one is said at once** (`pc_online_address`, before the restart; port/net's own parse and the
  name's lookup still come in the session): nothing typed ("Type the host's address first: their Tailscale name or
  100.x address"), a space inside it, `http://`, a path, a name and `@`, a character that can't be in one, an IPv4
  address with three numbers or one over 255, a port after `:` that isn't 1-65535 or is missing, an IPv6 address with
  a port outside brackets, brackets around something else or without their `]`, a name with two dots together. Each
  is a sentence in the footer that quotes what was typed.
- The port: "The port is a number from 1 to 65535, not '70000'".
- `net.recent`: 8 at most, one entry a host.
- Leave the session asks twice; Host now, Join now and Watch don't (they say "Restarting...").
- Defaults: `net.recent` empty; the page acts only on a choice.

### 13.5 Proofs

Rocky, x86-64, RelWithDebInfo, unless said.

| Test | Result |
|---|---|
| `frontend.menu.online` (new) | 76 checks: the page's items in order (Host, Join, Watch, a heading, the addresses with their tags, Copy), the Tailscale name joining the page when its lookup ends; Copy with no clipboard (headless) saying the address to give; the port typed (the Space that chose the item not typed, digits only, five at most, Backspace, Enter done, Escape undone, Down ends it and moves), a port that isn't one refused with its sentence and no act; Host now calling main's act with the port and the code, then the main loop stopping; Join's twelve wrong addresses each refused with its sentence, then one typed with spaces around it trimmed and joined; three recent hosts listed in order, one chosen into Address and Join now selected; Watch the last session's, a log of another build without Watch, Keep; Stop watching in a replay; in a session Leave after the status rows, asked twice, forgotten when moving away; the overlay's changes not saved listed for the restart |
| `frontend.menu.relaunch` (new) | 32 checks: the host's command line from one with a press file, a session's options, a record and a code (all out), a ROM path with spaces, `--set` changes and fullscreen, `--net-return` and the restart's own; that line's back-to-the-game restart (everything from `--net-return` out); a replay's `--paced` only without `--fast`; `--rom` added only when none is given; an empty join code given; too small for out or buf: -1; Windows' quoting of 14 arguments (a program path with spaces, `C:\Users\Ann Lee\ROMs\rush 2049.z64`, a folder ending in a backslash, quotes, an empty one, double backslashes, a tab) parsed back by the C runtime's rules (a parser in the test, from Microsoft's description) to the same 14; the log's shell quoting; `net.recent`: the same host once and first (case, `:24049`, IPv6 brackets), another port another entry, 8 kept, one there moving to the front, a list that doesn't fit unchanged, bad entries skipped; good addresses kept as typed; ports |
| `frontend.sdl.text` (new) | 14 checks through the real window (SDL's offscreen driver): no text input over the game; F1, Online, Join, Address; text before Enter goes nowhere; Enter turns SDL's text input on; SDL_TEXTINPUT events typed; Backspace held and tapped; a key's own event types nothing; non-ASCII refused with its message; Ctrl+V pasting the clipboard's first line; Enter done and text input off; Escape undoing; F1 closing the overlay and ending the typing |
| `frontend.menu.session`, `nav`, `save`, `pages`, `mods`, `presets`, `frontend.sdl.menu`, `mouse` | as before, a step more where they pass Saves on the main page; the session's rows on the Online page and the main page's Online item showing "RTT 23 ms, delay 2" |
| `net_menu_restart` (new, realtime) | **host**: a process in real time opens F1 > Online > Host a race > Host now (its port `net.port`'s, a free one) and execs into `--net-return --net-host PORT --net-code ''` (the press file out, `--seconds`, `--net-bind`, `--set` kept); a guest joins with race2p's port 2 presses; both paced at 4 times the pace (`RUSH_PACE_SPEED`): 961 retraces hashed by both, every hash the same; the host ends at its limit and doesn't come back (its own end). **join**: a host plays race2p's port 1 presses (the menus to the player screen), its draw distance far; a process in real time opens F1 > Online > Join a race, picks the first recent host (`--set net.recent=127.0.0.1:PORT, other-pc`), Join now, and execs into `--net-return --net-join 127.0.0.1:PORT --net-code ''`: 841 retraces hashed by both, every hash the same; the guest adopted the host's draw distance; the host leaves at its limit ("peer-left"), and the guest, started from the page, restarts without the session (no `--net-join`, `--net-code`, `--net-return` or press file) and runs offline to its limit, exit 0, without the host's draw distance. **leave**: a launcher's host in a session (its press script opens F1 over the running game) chooses Online > Leave the session twice: the session ends at retrace 560, 559 retraces hashed by both, every hash the same; the guest ends "peer-left", exit 0; the host restarts without the session (its `--net-host` and press file out) and runs offline to its limit. 53.9 s |
| `net_menu_replay` (new, realtime) | a session of two processes (`--fast`, race2p to the race and 300 frames into it, each player's pak in): 2,165 retraces hashed alike on both; the host's log replayed offline as it is (`--replay-inputs` alone, `--fast`): the session's settings, rules and paks taken, 2,165 retraces, every hash the session's; the two `.mpk` files beside the log byte for byte as before; the log alone in another folder refused before the game, exit 2 ("controller 1's pak of the session isn't beside its input log"), and with a pak of zeros as its `.p2.mpk` ("isn't the pak the session had"); then a process in real time in the host's data folder opens F1 > Online > Watch a replay > Watch the last session's and execs into `--net-return --replay-inputs FILE --paced` (at 4 times the pace): 2,165 retraces, every hash the session's. 18.2 s |
| mutation: the restart with the environment as it is now (`environ`) | `net_menu_restart` fails: the guest back in the game has the host's draw distance (`RUSH_GFX_DRAW_DISTANCE`) |
| mutation: four bugs in `port/config/online.c`, one at a time (the Pi, the ASan build) | each caught: backslashes before the closing quote not doubled (`relaunch`, 2 checks), the same host not collapsed (4), the press scripts kept on a restart (2), no IPv4 range check (`online`, 10) |
| the whole suite | RelWithDebInfo: 569 of 569 (518 at `-j4` without the label realtime, `assets_rom` skipped: the retail ROM; then the 51 realtime ones alone, under the shared lock) |
| the default off: `synth_compare.py --same --runs 2`, main's program (41b4aee) against this branch's, every path | RelWithDebInfo: 62 of 62 the same (`pcsetup` left out by the tool: the host's clock only) |
| the Windows cross build (mingw-w64 on the Pi, `RUSH_FETCH_SDL2`, RelWithDebInfo) | `rush2049pc.exe`, `frontend_menu_test.exe`, `frontend_sdl_test.exe` and `net_test.exe` link; the exe imports only Windows' own DLLs (`pe_check.py --dlls` passes): the new calls (`CreateProcessA`, `GetEnvironmentStringsA`, `SetHandleInformation`, `WaitForSingleObject`, `GetExitCodeProcess`) are KERNEL32's, SDL's text input IMM32's, which SDL imported already |
| AddressSanitizer and UBSan (the Pi, aarch64) | `frontend_menu_test` online, relaunch, session, nav: no report |
| Debug (rocky) | the frontend tests, `net_unit`, `net_session_race2p`, `adopt`, `mismatch`: 47 of 47 |
| `n64_neutral.py --base 09da2f0` | no game C changed (`src/`, `include/`): nothing to check |

Not tried: a real window on a real display (the SDL tests use the offscreen driver; an IME, a real clipboard manager,
fullscreen across the restart); Windows itself (CI's `windows` job builds and runs every test, the new ones with the
CreateProcess path included; the `.bat` launchers with a restart were checked by reading); two PCs.

### 13.6 Not done, and for the user to decide

- **A rejoin path** (the `net-rejoin` branch) can join the page's plumbing as one more `PC_ONLINE_*` action and
  `PC_RELAUNCH_*` kind with its option; `pc_online_args` leaves a session's options out by its table.
- **The track in a kept replay's name**: not known without the game's memory (13.3).
- **Pruning `netplay/`**: each session adds its log (about 72 KB a minute) and two 32 KB paks; nothing removes them.
  Keep this replay exists for when something does.
- **A controller can't type an address**: the recent hosts are its way to join; an on-screen keyboard would be more.
- **Windows' nested waiting runs**: each restart from the page in a run with a console keeps the run before waiting
  (13.2); a page used for many sessions in one launcher's console nests that many. Exiting at once there instead would
  break the launcher's console (it closes, and a program attached to it is closed with it).
## 14. Status: own view, full screen (phase 4)

Written 2026-11-07 on branch `net-view` (from 41b4aee). Each player sees only their own car's view, filling the
window, while both PCs go on running the game's two-player race with both views (1.4: the game's logic is per window,
so both PCs must run both). The players' side is the README's "Playing online"; the renderer's is GFX.md "One view of
the split screen".

| Where | What |
|---|---|
| `port/gfx/gfx_view.c` | the setting and the player; per graphics task, the race's views; the view filter, the zoom and the taller frustum, fills and 2D sorted by view, the models at HUD points |
| `port/gfx/gfx_rsp.c` | where a view lands (`view_place`): another view's 3D not drawn, the view shown zoomed and centred; outcodes and the guard band's clipping with clip y's widening (ys) as with x's (xs) |
| `port/gfx/gfx_rdp.c`, `gfx_hud.c`, `gfx_interp.c` | the 3D's scissor and the fills; the 2D through the HUD's queue, moved across and down or not drawn; frames between with the recorded context |
| `port/main.c` | the views' rectangles from the game's window records (`race_views`); the session's player (the host 1, the guest 2) |
| `port/config`, `port/input/menu.c` | `net.view`; F1 > Video > Online view |

### 14.1 The setting

`net.view = split|own`, `split` by default. With `own`, in an online session, the host's PC draws player 1's view
alone and the guest's player 2's, across the window, with that player's HUD at the screen's edges. It is only what a PC
draws: it isn't in `pc_must_match`, so a session neither compares it, nor gives the host's to the guest, nor locks it in
F1, and each player chooses their own (14.5: the two games stay one, hash for hash). F1's Video page has it below
"Race HUD" ("Online view": split screen, own (full screen)); it applies from the next frame. The main page already
holds fifteen rows in a session (12.10), so the Video page, where the other drawing choices are, takes it.

**A game on one PC stays split screen.** Two players at one PC share its screen, so "player 1's view only" would hide
player 2's car from player 2; a setting left at `own` after an online race would do that to the next couch race. The
setting is an online one, then, and the renderer's own variable, `RUSH_GFX_VIEW=1-4`, shows one view in any game for
tests and tools (the `race2pown` path uses it).

### 14.2 The survey (the game C)

**Vertical culling: none.** The game's CPU code leaves nothing out by the vertical field of view, the window's height or
its rectangle, so a taller view gets no holes and no pop-in from the game's side. Fifteen functions address the window
records (`D_8017A510`), found in the compiled code as well as by name: the setup (`exhaust_smoke_effect`, which derives
fovy from fovx and the window's shape, `arb_rate_set`, `ZOID_ResetWindows`, `wheel_render_full`, `car_render_full`,
`viewport_scale`), the fog (`func_800AB18C`, `func_800A7428`, `func_800A7480`), a setter and a getter of the records'
pointers (`func_800A5518`, `func_800A6078`, the getter inlined in `ZOID_ProjectWindow`), the view's own display list (`func_8009F058`: the viewport, the scissor and the projection from the record), and three
that use it to place things: the battle off-screen arrow (`entity_render_setup`, by fovx and fovy), `ZOID_WindowTo3D`
(`car_lights_render`, the models at HUD points) and `ZOID_ProjectWindow` (`brake_light_update`, called only by menus).
tan(fovx/2) and tan(fovy/2) are never read. The scene walk's tests are the hidden flag, the per-view mask, the
matrices' range, the fog's radius in x and z, the levels of detail and the nodes' boxes, which go to G_CULLDL and so
to the renderer (DETAIL.md 1.2); the poly list's, the hidden flag, the view bit and the range; shadows, skids and the
emitters test no view. The visibility tables go by the eye's place alone (DETAIL.md 1.1). So the holes a taller view
could show are the renderer's and the frame's (14.4). (The horizontal survey, GFX.md "The game's own culling", said the
battle arrow reads fovx: it places the arrow by fovy too.)

**Where the two-player HUD draws.** All 2D goes into one display list of its own (`pc_text_dl`), called after every
view's 3D (`ctx_game_loop.c:112-117`, `particle_knot/group.c:1590-1773`), under a full-frame scissor: no 2D is
scissored to a view, the game clips sprites in software. Each piece's place comes from a table of the cartridge's,
indexed by the number of windows and the window, so which view a piece belongs to can only be told by its box:

- per window, each drawn once per view: the speed (digits and box), odometer, tach, gear, place, lap counter, lap
  split, radar frame and blips, wrong way, coin counts, the countdown's message pane, the results' panes, battle's
  health, points and lives, stunt's panes;
- shared, drawn once a frame: the race clock, the time left (timed modes), the map with its dots and the flag, the
  pause screen, the results' header;
- on the retail cartridge (the Pi, the frames of 14.5, in words): every per-window piece lies inside its view's
  rectangle, the speedometer and radar at its top left, the place, laps and coin counts at its top right; the side
  panel holds the shared ones, over its background of nine texture rectangles across its whole width.

**Which 2D tracks 3D: none.** The markers over the other player's car are quads in the view's own 3D (the poly list,
`music_control.c:106-142`, rebuilt as billboards by AnimateArrow), the countdown's digits camera-space quads in the
view's 3D, and nothing in a race projects a 3D point to place 2D (`ZOID_ProjectWindow` is the menus'). The other way
round, 3D tracks 2D: the coin icons beside their counts (and battle's and stunt's status models) are models placed at a
HUD point, unprojected 2 ft in front of the camera through the view's record (`ZOID_WindowTo3D`, `coins.c:141`).

### 14.3 The renderer

In the race's views (main.c's hook reads them from the game's window records, `D_80149B00` and `D_80149B20`: x 2-242,
rows 0-109 and 111-220), with own view on, per graphics task:

- **3D:** a perspective draw is the view whose rectangle holds its viewport's centre. Another view's isn't drawn. The
  view shown is zoomed by 4/3, its width to the frame's, so its field of view across stays the game's 90 degrees, and
  centred; its picture then reaches the frame's top and bottom, tan(fovy/2) 0.6875 where the view had 0.458, the
  one-player view's own frustum. Clip y goes through the outcodes, G_CULLDL and the guard band divided by 1.5 (ys), as
  widescreen divides clip x (xs). Widescreen then widens it as any view across the frame, with `video.fill` too.
- **Fills:** the clears across the frame as before; the dividers between the views, the side panel's fill and the
  left border, drawn before the 3D in fill mode, not drawn; one in the view shown, mapped with its picture.
- **2D:** through the HUD's queue (GFX.md "The HUD at the screen's edges"), grouped within one view: the view shown's
  groups keep their distance to the side of the view they are nearest, now the frame's (across by thirds, down the
  bottom third with the frame's bottom, a centred message to the middle, the rest with the top), at their own size,
  on the N64 pixel grid; another view's not drawn; in the side panel, its background (a piece across its whole width)
  not drawn and the map and clock at the picture's right side; anything else of the frame where the game puts it, and
  so the pause screen: its darkening across the whole frame and all drawn after it (its title and menu, whose lines
  each fall in one view's rectangle). With `video.hud = edges` in widescreen the sides are the screen's, with
  `centered` the 4:3 frame's.
- **Models at HUD points:** a transform in the view shown whose origin is 2 ft (or 5.75) in front of the eye and on
  screen is drawn at the view's own scale, moved as its point's 2D is, so the coin icon stays beside its count.
- Frames between (`video.frame_rate`) record the zoomed context and the moved 2D, and show the same.

### 14.4 Holes at the top and bottom: what was chosen, and why

The game's culling leaves none (14.2). What would have left holes is the renderer's and the frame's, and each is
handled where it is:

- the view's scissor, its rows only: mapped with its picture to the frame's;
- G_CULLDL and the trivial reject against the view's own frustum: the outcodes take the taller frustum (ys), so a node
  above or below the view's old field is drawn, not cut;
- the frame begin's black fills between the views and in the side panel, drawn before the 3D: not drawn, so where
  nothing is drawn the clear colour shows, as in a one-player race.

**The zoom: to the width.** Zoomed to the frame's width, the view keeps the game's 90 degrees across, which its own
culling, levels of detail, markers and battle arrows are made for, and gains height: its frustum is then exactly the
one-player race's (90 degrees across, 320 x 220), which the game's world, its visibility tables and its sky are made to
be seen with, so it shows nothing a one-player race doesn't. Zoomed to the height instead, the sides would be cut (the
view's 90 degrees down to about 67), and a stretched picture would distort the game. On the retail frames looked at
(14.5) the sky fills the top and the road the bottom.

What stays on the view's old lines: the battle off-screen arrow, which the game places by the view's fovy at the old
frustum's bottom (about 37 rows above the frame's bottom in own view); battle only, and not a hole.

### 14.5 Proofs

Rocky (x86-64, RelWithDebInfo) unless said.

| Test | Result |
|---|---|
| `gfx.view` (new, and in every `gfx.all.*` variant: scales 1.25-6.75, native 2880 x 1620, 16:9, 21:9, GLES, desktop GL, llvmpipe) | split, own with no player and own outside a race of several views: the split screen byte for byte; view 1 and view 2 alone: the ground to every edge and where the dividers, border and panel were, the other view's ground and HUD nowhere (every other pixel scanned), the picture zoomed 4/3 and taller, each HUD piece at its corner of the frame, the panel's background gone and its map at the right, the HUD-point model beside its digit; at 16:9 with edges the HUD at the target's edges, with centered in the 4:3 frame |
| `gfx.clip` | 80,000 triangles, the earlier 40,000 and the same made 1.5 times taller, alone and widened: every vertex GL gets within both guard bands |
| `frontend.menu` | the Video page's Online view applies at once; in a session it isn't locked |
| `synth_boot_race2pown` (new path: race2p with view 1 alone, `RUSH_GFX_VIEW=1`) | race2p's own end, exact floats; `synth_shot.py race2pown.png own`: the screen 96.5% drawn, the divider's rows, the side panel's place and the left border 100% (the split screen's: 0, 0, 0) |
| `net_session_view` (new; race2p, delay 2) | own view on the host and split on the guest, then the other way: each time host = guest = one process, 5,606 trace lines, 2,765 retraces hashed (every retrace, the full hash every 30, and retrace 0's setup check); the PC with own view says "player 1's view" (host) or "player 2's view" (guest) and its screenshot is one view across the frame (96.5%, 100%, 100%, 100%), the other's two views (72.6% each, the border 0%) |
| `gfx.interp.own` (new, and in the `gfx.interp.all.*` variants) | two views moving their own ways, each alone: a frame between at 0.5 is that view alone as drawn directly at 0.5 (within 80 pixels), and as far from the split screen's picture |
| frames between (by hand: race2pown's path at `RUSH_GFX_FRAME_RATE=120`, 12 frames shown in the race) | the nine drawn between two are one view across the frame, as the game's three are (the screen 96.4-96.5%, the divider's rows, the side panel's place and the left border 100%) |
| the default changes nothing: `synth_compare.py --same --runs 2`, main's program (41b4aee) against this branch's, every synthetic path | RelWithDebInfo: 63 of 63 paths the same, race2pown among them (`pcsetup` left out by the tool: the host's clock only), with the branch's first and its last code |
| the whole suite (the branch's last code) | RelWithDebInfo: 570 of 570, 521 at `-j4` and the 49 `realtime` alone under the shared lock (`assets_rom` skipped: the retail ROM); an earlier run on the first commit, 569 of 569 |
| the Windows cross build (mingw-w64 on the Pi, `RUSH_FETCH_SDL2`, RelWithDebInfo) | `rush2049pc.exe` and `gfx_test.exe` link; `pe_check.py --dlls` passes (Windows' own DLLs and WS2_32 only) |
| `n64_neutral.py --base 09da2f0` | no game C changed |
| the retail cartridge, the Pi (six frames looked at, 2026-11-07, not kept) | the pause screen with view 2 alone: at first its title and the menu's first four lines were missing (each line falls in view 1's rectangle), so the pause screen's pieces became the frame's (14.3); then the whole menu and title over view 2, its HUD at the edges. A two-player race on track 0 with view 1 alone and with view 2 alone at 4:3, and view 1 at 16:9 with `video.fill`: one view across the frame, sky to the top and road to the bottom, no band or hole; the speedometer and radar at the top left, the place, laps and coin counts at the top right (view 2's moved up from its view's rows, in its own colour), the map and race clock at the right over the 3D, the panel's background, the other view and the dividers gone; at 16:9 the HUD at the screen's edges. The countdown with view 2 alone: its 3D digit zoomed in the middle, view 2's own message across the top. The coin icons were found at their HUD points (ten draws a frame, at the coin counts' place) but are not visible on these frames, nor in the split screen at that moment |

### 14.6 Not done

- Three and four views: the renderer takes up to four, but main's hook gives the two-player layout only (sessions are
  two players); `RUSH_GFX_VIEW` shows one of two.
- The sound stays the split screen's mix (both listeners), as 1.4 planned: per player it would change the audio
  manager, which is game state.
- The two-player detail cuts (DETAIL.md 1.8: models' two-player sections, level objects, the tires' level, the nearer
  fog) are the game's for two players: own view shows the split screen's world, larger. `video.detail = high` restores
  the models and objects, for both PCs (it must match).
- The battle off-screen arrow (14.4); battle, stunt, the pause screen's other pages (options, controls) and the results
  with own view were not looked at on the retail cartridge. The side panel's background is told by its width, and the
  pause screen by its darkening across the frame, as seen on track 0's race; a piece across the whole frame in a race
  that isn't the pause screen's would keep what is drawn after it in the frame's place too.
- Two real PCs: not tried.
## 15. Status: rejoining, and recovering from a desync by replay

Written 2026-11-07 on branch `net-rejoin` (from 41b4aee). The user's priority is robustness: sessions that survive
lower and higher latencies, and stay in sync. Before this a peer silent for 15 s ended the session, and a desync
stopped it. The network's side is [NETWORK.md](NETWORK.md) 12; this is the game's, and the proofs.

| Where | What |
|---|---|
| `port/net` | the session's id and log, the rejoin window, rejoins of either side, BYE `gone` and `recover`, the replay's API, the outage rule for the delay (NETWORK.md 5, 12) |
| `port/app/netplay.c` | the rejoin ticket, the rejoin's flows (the guest's and the host's), the replay in the pads source, the notices on both screens, a desync's report and its restart |
| `port/app/startup.c` | `pc_app_reexec`: the program again, same arguments (POSIX exec; on Windows a child with the same handles, waited for) |
| `port/ultra/pace.c`, `loop.c`, `rcp.c` | `pc_catchup_set`: the clock free-running, graphics tasks not drawn, nothing presented; then the mode before it, paced from the N64 time then |
| `port/audio/audio_out.c` | `pc_audio_tap_hold`: the sound card gets nothing during the replay |
| `port/gfx/gfx_sdl.c`, `port/ultra/loop.c`, `win32.c` | `pc_gfx_window_closed`, `pc_quit_signal`: the window's close, SIGTERM and Windows' end of a console, a logoff or a shutdown told from the quit key |
| `port/config` | `net.rejoin` (300 s; off), `net.recover` (on) |
| `port/main.c` | a rejoining host adopts the session's settings before its memory is mapped, as a guest does; the restart at the end |
| `tools/port/net_session_check.py` | `rejoin`, `outage`, `recover`, `speed` (15.5) |

### 15.1 The design

**Surviving a drop.** A running session waits `net.rejoin` seconds (300) for a peer that went silent, its game held as
the lockstep holds it, the notice saying what the wait is and how long it may last ("Waiting for the guest to come
back... Nothing from it for 0:12; waiting 4:48 more. Escape ends the session."). A network that comes back in that
time goes on with nothing lost: the inputs and hashes not acknowledged are resent, and both sides re-base their
pacing (ULTRA.md 10). The quit key ends it for both, as before. 15 s stays the limit before the game runs (the
handshake, a guest no host answers). `--net-timeout S` sets both, as the tests' kill-and-timeout checks want.

**The rejoin window: 300 s**, a setting (30-3600, or off). What it has to cover, from the moment a player's game
goes:

- a PC that restarts: one to three minutes to the desktop on Windows 11 and most Linux desktops (more when an update
  installs, which no window can promise);
- starting the game again: the ROM, the mods and the handshake, seconds; the log's transfer, an hour's log is 2.6 MB,
  sent at up to about 3 MB a second (32 chunks of 1 KiB a pump of the network thread), so seconds on a home
  uplink;
- the replay: retail's speed on the Pi 5 is in 15.5; the keeper's window doesn't run during it (the rejoiner's
  progress keeps the session alive), so a long session doesn't shorten it;
- a router or a modem that restarts: one to three minutes; Wi-Fi or a phone's hotspot reconnecting, Tailscale finding
  a new path: seconds.

Five minutes covers a restart with room to spare. Past it the player who stayed has most likely given up, and a
session kept waiting longer only keeps their game frozen; they can end the wait any time with Escape. 15 s, as
before, is `net.rejoin = off`.

**The session's id and the ticket.** The session's id is the first session ID of its handshake (NETWORK.md 12), on
both PCs. Each side writes `netplay/rejoin-host.txt` or `rejoin-guest.txt` in its data folder when the game starts:
the id, the role, the build, the time (refreshed every 20 s while it runs), the window, and a desync being recovered
from. It is deleted when the session ends: a player leaving, the other player leaving, a timeout, a desync that isn't
recovered from. So it remains only when the game went without ending its session: a crash, a kill, a power cut; and
by design a closed window and the system ending the program (below).

**Rejoining.** A player whose game is gone starts it the same way as before. With a fresh ticket of its role and build:

- the guest's HELLO asks to rejoin the session by its id. The host that kept it answers with its offer once its own
  game waits at the first retrace it lacks the guest's input for (`live`) and the old game is gone or silent 1.5 s; a
  host without that session (a new one, the old one ended) answers as to any guest, and the game joins a new
  session, saying so and dropping the ticket;
- the host, before its window opens, gives its guest 3 s to knock: the guest that kept the session knocks at the
  host's address every 0.25 s once the host is silent (at once after a BYE `gone`). Without a knock it forgets the
  ticket and hosts a new session as usual.

The rejoiner then does what a guest does at a join, whichever side it is: it checks the session's build, ROM and mods
lines against its own and adopts the session's settings for this run, before its memory is mapped (`pc_net_adopt`;
for a host, its own settings at the session's start, which its ini may no longer have), and installs both paks as the
session had them at its start (sent by the keeper: neither PC's files are read for them). Retrace 0's window hash,
with the must-match text's hash in it (12.3), is the first one the replay checks, so a setup that differs anywhere
stops it there.

**What the keeper sends** (NETWORK.md 12): the host's settings blob, both paks as at the start, and the log: both ports'
pads of every retrace before `live` and its own hash of each, and every window hash (every 60 retraces). The keeper
keeps all of that from the start, in memory: 12 bytes a retrace, 2.6 MB an hour, and the paks' 64 KiB.

**The replay.** At the rejoin's START the rejoiner turns on `pc_catchup_set`: the N64 clock free-running (`--fast`'s
course, ULTRA.md 10), graphics tasks not drawn, no frame presented, the sound card fed nothing (`pc_audio_tap_hold`).
Then the game boots, and the pads source gives each retrace before `live` its pads from the log; each retrace's state
hash, and the window hash every 60, is checked against the keeper's. A notice four times a second says how far it is
("Catching up with the race: 45%. Retrace 32400 of 72000, about 0:20 left."), drawn over a cleared frame; the keeper's
says the same of the rejoiner ("The guest is catching up: 45%"), from its PROGRESS. At `live` the source turns the
mode back (the paced clock from the N64 time then, due then), submits this PC's first input for retrace `live`, and
the session goes on: the keeper's game was waiting there. The input log of the rejoined run is whole from retrace 0
(its header says it was rebuilt).

**What the replay skips, without changing the game's course.** Drawing: the renderer only reads the game's memory
(the display lists, textures, vertices) and writes GL, so a graphics task not drawn still sends its SP and DP events
at the same moment of N64 time (`rcp.c`); the one value the renderer gives the game, the HUD models' offset, is 0 in
a session (LOCKSTEP.md 6). Presenting: a swap that waits for vsync would hold the replay to the display's rate. The
sound card: in a session the AI's queue is the null output's on the N64 clock whatever the device (AUDIO.md 11), and
the device only gets a copy (the tap), so holding the tap changes nothing the game sees; the game's own sound is
still made, as it is game state. The proof is the replay's own check: every retrace's hash, and the trace of every
test below, the keeper's line for line.

**The window closed is the game going, not the player leaving.** The user listed a closed window with crashes and
restarts. On Windows a restart closes windows the way a click does, and on Linux a shutdown sends SIGTERM; with them
counted as leaving, a restart would end the session for both. So the quit key, F1's Quit, Ctrl-C and a run's limits
leave as before (BYE, the other side ends "the guest left"); closing the window, SIGTERM and SIGHUP (on Windows the
console closing, a logoff, a shutdown) end this run with BYE `gone`: the other side waits for it at once ("Its game closed 0:05 ago; waiting 4:55 more"), this side keeps
its ticket, and starting the game again rejoins. The player who stayed ends the wait with Escape.

### 15.2 Recovering from a desync

With `net.recover = on` (the default; only `--net-*` sessions read it, so the N64 game is unchanged), the host is
authoritative. At the first retrace r whose hashes differ the host tells the guest (BYE `recover`), writes its desync
report as before, and waits at the first retrace it lacks the guest's input for. The guest writes its report, adds r
to its ticket and starts its program again with the same arguments (`pc_app_reexec`: on Linux the same process,
exec'd; on Windows a child the old process waits for). The new run's ticket rejoins, and its replay checks the host's
hashes at every retrace, r and those after it: if they match, the session goes on, and both logs say "recovered from
the desync at retrace r". If the replay differs again at r, the difference is deterministic (the same inputs give
the same wrong state): the rebuilt game stops the session with a desync at r, "deterministic: the rebuilt game went
out of sync again at retrace r, where the desync was", the host says the same, and both write today's report.

At most three a session (the ticket counts them). A desync at retrace 0 is never recovered from: it means the two
setups differ (12.3), which a rebuild with the same setup can't fix. `net.recover = off` (either side) stops at the
first desync with the report, as before. `RUSH_SIM_DESYNC_ONCE=1` (tests) drops `RUSH_SIM_DESYNC` from the restarted
run, so a forced desync is a one-off.

### 15.3 The host rejoining

Done, and no harder than the guest's: the library's keeper and rejoiner are roles of their own, either side; the
guest knocks because only the host listens. The one difference in the game: a host normally sets up with its own
settings and opens its window before its guest joins (12.10), so a rejoining host waits for its guest's knock (3 s,
before its memory is mapped) and adopts the session's settings as a guest does. The host's port must be the one the
session used: the default 24049, or the same `--net-host PORT`.

### 15.4 Proofs

`tools/port/net_session_check.py` (ctest `net_session_*`), two `rush2049pc` processes over UDP on 127.0.0.1, rocky,
x86-64, RelWithDebInfo, the synthetic cartridge, each run's trace replay_check.py's (every state change, a status line
a second, every retrace's physics as exact floats, the pads, the state hash every retrace and the full one every 30).
A process "killed" is killed by its own PID; its replacement is the same command in the same data folder, started 2 s
later.

| Test | Result |
|---|---|
| `net_session_rejoin`: race2p, the guest killed at retrace 1700, the same command 2 s later | it rejoined and replayed 1,713 retraces in 0.59 s (2,913 a second, 48.6 times real time, the per-retrace trace on and the suite running beside it), every hash the host's; then host = new guest, 5,606 trace lines, line for line, from retrace 0; both exit 0; the host waited 2.9 s |
| `net_session_rejoin_host`: the host ended by SIGTERM at retrace 1700 (BYE `gone`), the same command 2 s later on the same port | the guest waited for it at once and knocked; the new host replayed 1,704 retraces in 0.53 s (3,223 a second), every hash the guest's; guest = new host, 5,606 lines; both exit 0; the guest waited 3.0 s |
| `net_session_rejoin_paced`: the session's own clock at 4 times the pace, the guest killed at 1700 | the replay unpaced (1,704 retraces in 0.38 s), then paced again from the N64 time then; host = new guest, 5,606 lines; the host waited 2.6 s |
| `rejoin --paks` (by hand): a new controller pak each (one ID), the guest killed at 1700 | the new guest installed the session's two paks from the host's record, port 2's re-IDed as at the start ("the two paks have one ID"), its own file not read; retrace 0's hash (the paks' images in it) the host's; host = new guest, 5,605 lines; 1,711 retraces replayed in 0.60 s |
| `rejoin --sim bad150` and `rejoin --host-side --sim bad150` (by hand): 150 ms ± 40 each way, 10% loss, duplicates and reordering on both sides and on the restarted one, delay 14 | the offer through the loss, then 1,717 and 1,716 retraces replayed in 0.50 s, every hash the other's; then the two the same, 5,630 lines; the side that stayed waited 4.0 and 3.9 s |
| `net_session_outage`: race2p to the race and 2 s into it, delay 3: the link down 20 s (every packet lost both ways); then the guest stopped 20 s (`RUSH_SIM_NET`'s pause) | each time host = guest = one process (3,428 lines, 1,685 retraces hashed), the host waiting 20.0 s and 21.2 s, past the old 15 s timeout; the delay stayed 3 |
| `net_session_recover`: `RUSH_SIM_DESYNC` changes the guest's random seed once, at retrace 1000 | the host had the guest rebuild; its program started again (the same process ID: exec), replayed 1,004 retraces with the host's hash at every one, 1000 and past it, and played on: host = rebuilt guest, 5,606 lines, both exit 0, both logs "recovered from the desync at retrace 1000" |
| `recover --paced` (by hand): the same on the session's own clock at 4 times the pace | the first game paced (240.000 retraces a second of wall time), the rebuilt one's replay unpaced (1,004 retraces in 0.22 s), then paced again at 240.000 a second, none re-based; host = rebuilt guest, 5,606 lines |
| `net_session_recover_repeat`: the change again in the rebuilt game | both end with the desync at 1000, exit 3: "deterministic: the rebuilt game went out of sync again at retrace 1000, where the desync was ..."; both reports written |
| `net_session_desync` (`net.recover = off` now), `net_session_mismatch` | as before: both stop at 1000 with reports naming `rng+0x0`; a setup that differs stops both at retrace 0 (never recovered from) |
| the short outages already tested (`net_session_sim_pause` and `_burst`, `net_sigstop`: 3 s at most) | inside the old 15 s already; unchanged, the delay as before |

The library's own (the toy simulation, NETWORK.md 12): on the virtual clock, both sides' games gone and rebuilt, at
the same address and another, a stranger told `busy`, a desync recovered from and one that repeats, a 20 s outage;
over UDP, `net_rejoin`, `net_rejoin_host`, `net_outage` (and a 20 s SIGSTOP), `net_recover`, `net_recover_repeat`.

| Check | Result |
|---|---|
| the whole suite, rocky, RelWithDebInfo (`-LE realtime` at `-j4`, then `-L realtime` alone under the lock) | 575 of 575: 519 at `-j4`, then the 56 realtime ones alone, the new ones among them (`net_rejoin`, `net_rejoin_host`, `net_outage`, `net_recover`, `net_recover_repeat`, `net_session_rejoin_paced`, `net_session_outage`); `assets_rom` skipped (the retail ROM) |
| the default off: `synth_compare.py --same --runs 2`, main's program (41b4aee) against this branch's, every path | Debug 62 of 62 the same, RelWithDebInfo 62 of 62 (`pcsetup` left out by the tool: the host's clock only) |
| `port/net` alone under ASan and UBSan (the Pi 5, Debug) | `net_test unit` (the rejoin cases), `fuzz` (100,000), `sim` clean, bad150, pause, and `net_check.py` rejoin, rejoin-host, recover with the sanitized `net_peer`: no report, no leak |
| the Windows build | the changed files with Windows code (`startup.c`'s `CreateProcessW`, `netplay.c`, `net.c`, `loop.c`, `main.c`...) compile with mingw-w64 (a syntax check on the Pi); run by CI's `windows` job |
| game C | untouched: `n64_neutral.py --base 09da2f0` passes |

### 15.5 The catch-up's speed

`net_session_check.py speed NAME`: a session on the paced clock at the real pace without the per-retrace trace, the
guest killed 15 s into the race and started again 1 s later; the rejoin's own line ("caught up: replayed N retraces
in T s"), from START to the live retrace. Then, for comparison, one `--fast` process replaying the host's input log as
far, which draws every frame headless (its start, the ROM and the data, measured apart and taken off).

| Path (rocky, RelWithDebInfo) | The rejoin's replay, nothing drawn | One `--fast` process, every frame drawn | The catch-up |
|---|---|---|---|
| race2p, killed 15 s into the race | 2,469 retraces (41.1 s of the session) in 0.40 s: 6,134 a second, 102 times real time | 2,595 a second | 2.4 times as fast |
| race12 (12 cars, the guest idle) | 2,767 retraces (46.1 s) in 0.44 s: 6,336 a second, 106 times real time | 2,302 a second | 2.8 times as fast |

With the per-retrace trace on and the suite running beside it (15.4) the same replays ran at 2,900-4,500 a second, 49
to 77 times real time.

On the retail cartridge (the Pi 5, two processes over loopback, by hand; its numbers stay off the repository): a rejoin
late in a two-player race had the host's hash at every retrace and played on; its replay ran many times faster than
real time; and drawing was most of a replay's cost there, a `--fast` replay drawing every frame being several times
slower than the catch-up. Skipping the drawing is what makes a retail rejoin quick on a modest PC.

What the speed means for the wait: the player who stayed waits for the restart, the transfer and the replay. At S
times real time an hour of session is 60/S minutes of catching up; the rejoin window doesn't count that time (the
replay's progress keeps the session alive), so it only lengthens the wait, and both screens show how far it is.

### 15.6 Found on the way

- **An outage taken as latency.** Inputs resent after a 20 s outage arrived with a 20 s transit, and the delay
  controller went to its cap: half a second of input lag that shrank a retrace every 2 s, a minute to recover
  (measured on the virtual clock with the rule taken out). An input slower than the cap is now an outage, not
  latency (NETWORK.md 5); the delay stays where it was (2 in every outage test).
- **A rejoining game at its old address.** A guest that starts again often gets the same UDP port. Its packets in the
  offered session came from the peer's own address and were dropped as the wrong session before the offer was
  confirmed; a host now confirms a pending session from the peer's address too (`net_unit` runs both addresses).
- **63 characters.** The keeper hears why a rebuilt game stopped from the BYE's text, its first 63 characters: the
  word "deterministic" was past them. It comes first now.
- **A desync at retrace 0** is two setups that differ (12.3): rebuilding with the same setup can only fail again, so
  it isn't tried (`net_session_mismatch` checks the stop at 0, as before).
- **A FILE_ACK that couldn't grow.** Version 1's FILE_ACK carried the whole blob's bitmap in one packet: a log past
  about 9 MB would not have fit. Version 2's carries a window from the first chunk not received.

### 15.7 Not done, and for the user to decide

- **Tried headless only.** The window's close is read, not run (the tests use SIGTERM for "the game went"); a real
  window on Windows, and the Windows restart for a desync (a child process with the parent's handles, waited for), are
  compiled (a mingw-w64 syntax check of the files here) and will run in CI's Windows job's `net_session_*` tests.
- **The window's close counts as the game going**, so the other player waits up to 5 minutes, and a player who meant
  to leave that way leaves the other waiting until they press Escape. The user may prefer the close to leave (one
  line in `pc_net_finish`); then a Windows restart ends the session for both.
- **The window's length**, 300 s: a setting (`net.rejoin`, 30-3600, off).
- **No rejoin without the ticket.** A player on another PC (theirs died) can't rejoin: the session's id is in the
  first PC's data folder. A host could show it, and a guest type it, later.
- **Both games gone at once** (a power cut on both): nobody keeps the session; both start new ones.
- **The host's port must be the session's** (24049 by default) for its guest's knock to reach it; `--net-host 0`
  can't be rejoined.
- **Recovery trusts the host.** When the host's own game is the one that went wrong, the guest's rebuild differs at
  the desync again and the session stops, said deterministic: two PCs can't tell which one is wrong.
- **A NAT rebinding** (the same game at a new address) still ends by timeout, now the rejoin window's.
- **Not in F1.** `net.rejoin` and `net.recover` are settings in `rush2049pc.ini` only, so the shared overlay is
  untouched; the F1 Online page (branch `net-online`) could show them.
