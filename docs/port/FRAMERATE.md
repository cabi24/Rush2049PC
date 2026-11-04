# A high frame rate (video.frame_rate, game.frame_rate)

Phase 8 of [PORT_PLAN.md](../PORT_PLAN.md), written 2026-10-27. The game draws at most 30 frames a second; with
`video.frame_rate` the port shows 60, 120, the display's refresh or any rate from 30 to 1000, while the game's timing
stays the N64's. The frames between the game's are drawn by the renderer from the game's last two
([GFX.md](GFX.md) "High frame rate"); no game C changed. This file says how the game paces itself, why that leads to
interpolation in the renderer, what the setting does, how it is tested and what it costs. Since 2026-11-03 the game
itself can also draw a frame every retrace, 60 a second, with its per-frame steps taken to the shorter frame
(`game.frame_rate`, section 7); `video.frame_rate` works on top of it.

## 1. How the game paces and times itself

**Retraces.** The game creates its scheduler with one field per message: `osCreateScheduler(&gScheduler, ..., 12,
tvType, 1)` (`src/rom/lib_2cf0.c:88`), so `osViSetEvent(&scheduler->cmdQueue, 666, numFields)`
(`src/rom/lib_1050_sdk.c:122`) gets a message at every 60 Hz retrace. `__scHandleRetrace` counts it
(`retraceCount`, at 0x27C: the game reads it as `gViTickCounter`, `frameCount`, `tick`) and sends every client a
message.

**At most one frame every two retraces.** The scheduler swaps a finished frame only if two retraces have passed since
the last swap: `__scExecTask` swaps at once when `retraceCount - __osScSwapCount >= 2U`, else leaves it pending, and
`__scHandleRetrace` swaps the pending one when that becomes true; `__scTaskReady` holds the next graphics task until
the swapped buffer is on screen and the two retraces have passed. The game's own main-thread step
(`world_trigger_activate`, MAT3's main loop, `src/blob/groups/mainloop/mainloop.c`) starts a frame only at a retrace
message, when the next framebuffer is the current one, no frame is in flight (`D_80035470/71/72`) and fewer than two
are pending (`D_80114730`): it calls `gfx_frame_begin` and wakes the game thread (`D_8002ECC0`). The game thread's
`game_loop` (`src/blob/groups/input_aux/game_loop.c`) ends each frame with `UpdateFrame` (`src/blob/UpdateFrame.c`),
which sends message 2750 (the main thread's `MainEndFrame`, `world_collision_response`: the graphics task) and waits
for the next start.

So the game draws at 60/n frames a second, n = 2, 3, ...: 30 when a frame fits in two retraces. On the PC the host is
fast enough for every frame: in the synthetic cartridge's races, 150 graphics tasks per 300 retraces, a frame
shown every two retraces.

**What steps the world.** Two clocks:

- **The car physics, a fixed tick per retrace.** The model thread (`render_thread_entry`, MAT3's ModelThread,
  `src/blob/groups/modelthread/modelthread.c`) is a scheduler client at priority 7 and runs `model_iteration`
  (`src/blob/groups/func_800E56F8/group.c:4024`) once per retrace message: the model tick `D_80143FF4` counts up,
  the model time `D_801543CC` is ticks x 1/60, each human car steps by `dt = (ticks since its last step) x 1/60`, the
  drones by the schedule `update_drone_models` walks (`D_80120E74`) with the ticks since their last update, then
  `players_race_update`. This does not depend on how often frames are drawn.
- **Everything else, per frame, by whole retraces.** `UpdateFrame` calls `viUpdateTime` (`src/rom/lib_1050.c`):
  `gViFrameDelta` = retraces since the last frame (`D_8002EB98`), and the frame time `__osScDeltaTime` (`D_8002EB94`,
  MAT3's gFrameSeconds) = that, at most 5, x `gScaleSecondsPerTick` (1/60). Some 290 places step by it: particles
  (`hud_render.c`'s smoke puffs), animations (`race_setup_1.c`'s palette and object animations, which also multiply
  by `D_8002EB98`), menus (`ctx_optionscr_buttons.c`), timers. Some logic is per frame with constants tuned to the
  frame delta: the follow camera `func_800E8D50` (UpdateCarObj) moves `position = position x e + target x (1 - e)`
  once a frame, its elasticity `e` 0.6 x the slot's, scaled by 0.75, 0.6 or 0.5 when a frame took 3, 4 or 5
  retraces; `game_loop` counts frames (`frame_counter`, gFrames).

**The choice that follows.**

- (a) Running the game loop faster is not equivalent. The 2-retrace rule is in the game's scheduler, so it would be
  a game C change; with it the game would step by one retrace a frame, which no N64 frame does. The car physics would
  be the same (per retrace), but the camera's per-frame filter would follow twice as tightly (no constant is tuned
  for a delta of 1), per-frame counts would run twice as fast, and the frame-time code would take a different path of
  float operations. And whole retraces cap it at 60: no step is smaller than a retrace. (2026-11-03: `game.frame_rate
  = 60` does this as a setting of its own, the per-frame steps taken to a frame of one retrace: section 7.)
- (b) Interpolating in the renderer leaves the game alone: it runs at its own 30 fps, its world exactly as on the
  N64, and the renderer draws extra frames between two of the game's. That is what the port does.
- A hybrid has nothing to add: the part that could run faster (the physics) already runs every retrace, and what
  is shown is drawn by the per-frame part.

## 2. The design

**The renderer interpolates** (port/gfx, [GFX.md](GFX.md) "High frame rate" has the mechanics). While the setting is
on, each graphics task is recorded as the interpreter runs it, its live drawing unchanged: each vertex load in model
space with the transform it was loaded under (modelview, projection, their product), each triangle with what turning
it into GL vertices takes, rectangles as GL vertices, clears. When a frame B is on screen and the next one C is drawn
(the scheduler swaps C in two retraces after B, so on the PC C is drawn well before it is due), a frame at fraction a
of the way is drawn again, into a target of its own, from the recording of the nearer one (B up to halfway, then C),
with its 3D transforms moved toward the other's:

- Transforms are paired by what they drew: the view, and the vertex sources loaded under them (the same model data
  from frame to frame; the game's per-frame buffers alternate, so matrix and list addresses don't pair anything).
  Instances of one model pair by their order under a paired parent (a car's wheels), else as mutual nearest
  neighbours by place, allowing for the camera's move.
- A paired transform's modelview and the view's projection (where the game puts the camera's turn) are blended a of
  the way. A transform turned too far to blend (a wheel at speed) keeps the nearer frame's turn at the blended place.
- The camera's move is measured from the pieces of the world, which only it moves: the median change of the
  modelviews that didn't turn. A transform only one frame has, or one that jumped (a teleport), stays where that
  frame has it in the world, seen through the camera at that moment; so do the game's eye-relative native vertices
  (skid marks) under the identity modelview.
- 2D (rectangles, orthographic triangles) is the nearer frame's, as it drew it: the HUD doesn't move between frames.
- A view whose camera turned or moved too far, or most of whose transforms jumped, is a cut: no frame is drawn
  between the two, the screen holds B until C is due. A frame with no 3D (menus) is held the same way.

**The game's timing is untouched.** The main loop (port/ultra/loop.c) calls `pc_gfx_present_tick` at the chosen rate,
in step with the retraces; the VI's swaps still go through `pc_gfx_present`, which only marks the frame swapped in
and copies it to the presented image (what screenshots and F12 read). Nothing is posted to the game's threads. With
`--fast` the ticks are not deadlines of the virtual clock either: they run when the loop wakes for something else, so
the clock stops at the same moments with the setting on or off.

**Never at the game's expense.** On the host's clock the game's retraces and frames come first, since a late
retrace slows the game in real time:

- a tick waits while an RSP task is queued (the task runs first);
- a tick is skipped (the screen keeps what it shows) when it would run into the next retrace, by how long the last
  ones took;
- a frame between is drawn only when the GPU has finished what it was given (a fence after each graphics task and
  each frame drawn between, waited on for at most 2 ms);
- while the host is behind (two retraces late within a second), no frame is drawn between: only the game's are
  shown;
- a tick that would show what is already on screen (a held frame) shows nothing.

So on a host with headroom every frame between is drawn; on one without, fewer are, down to none, and the game runs
as it would with the setting off. The game's own frames are never lost to skipped ticks: a frame the VI swapped in
that no tick has shown by the next retrace, or every frame when no tick fits between two retraces, is shown at that
retrace's present, with its swap, as the setting off shows each (`of the game's frames shown at their retrace`). At
exit the loop and the renderer say how many frames were shown, drawn, skipped and not drawn, and `RUSH_GFX_TIMES`
adds what they cost.

**Paced by the display, with vsync** (2026-10-28). A swap that waits for vsync (the window's with `video.vsync`, its
default; headless, `RUSH_SIM_VSYNC_HZ`) changes the screen only at the display's vblanks, and blocks the loop until
one. The ticks were deadlines of the retrace grid, and their running cost included that wait (up to a refresh), so
nearly every tick looked too slow and was skipped, and a swap that blocked past a retrace made it late. On main, the
synthetic race under the model (rocky, 36 s, a 60 Hz vsync and coarse waits): 1310 of 2400 ticks skipped and 46
frames drawn between at 60, 1459 and 16 at `display`, 1911 and 104 at 120. So with vsync on, the loop now paces the
ticks by the display (`vb` in port/ultra/loop.c):

- **The vblanks.** A grid at the display's own refresh where the platform gives it (`pc_gfx_display_timing`,
  port/gfx/gfx_display.c, asked every 100 ms), else at SDL's (`pc_gfx_display_hz`: whole hertz, 59 or 60 for 59.94):
  - Windows: the window's monitor's mode from `QueryDisplayConfig` (user32: the target's `vSyncFreq`, a ratio such as
    60000/1001), and the compositor's last vblank (`qpcVBlank`) from `DwmGetCompositionTimingInfo`, used as the
    grid's phase when the compositor's refresh (`rateRefresh`) is the monitor's within 0.05% (a desktop whose
    monitors differ composes at one of them). dwmapi.dll is loaded at run time, so the program imports no new DLL
    (`pe_check.py --dlls` unchanged);
  - X11: the CRTC under the window's centre, its mode's dot clock / (htotal x vtotal) from XRandR (libXrandr loaded
    at run time, as SDL loads it; the headers at build time). No vblank times: GLX_OML_sync_control would give
    them, but the default GL here is GLES through EGL;
  - Wayland and headless: nothing (wp_presentation feedback is not done), SDL's whole hertz as before.
  An exact refresh is taken only when SDL's whole hertz could be its rounding (within 1 Hz). Each swap that blocked
  (it returned at a vblank) corrects the phase, unless the display gives its vblanks, and without the display's own
  period the period too (an eighth of the way toward the time between two, within 2% of SDL's). A driver that queues
  frames (WGL on NVIDIA and Intel: a swap blocks only once the queue is full) blocks rarely while one frame a vblank
  is submitted, so without the display's period the grid runs at SDL's whole hertz until a swap blocks again: at 59 on
  a 59.94 Hz display a frame is held twice about once a second. With it, one tick a vblank: under the model, 15
  frames held twice in 20 s at SDL's 59, 1 with the exact rate or the vblank times (section 5).
- **Retraces halfway between vblanks, on a display that keeps its phase.** Where the display's own period makes its
  refresh a whole multiple of 60 Hz, drifting less than a quarter refresh a minute against the retraces (60.000,
  120.000 Hz; not 59.94, whose vblanks pass through every phase against the retraces in 17 s), the phase between
  the two stays for good, and one phase is bad: a vblank just after a retrace leaves no time to draw its tick after
  the retrace, and one drawn before it lacks the game's next frame. Under the model at 60.000 Hz, a vblank 0.8 ms
  after the retrace lost 1114 of 1200 ticks and drew none between; 1.7 ms after, 70 lost and 100 drawn; on the
  retrace, 89 drawn, half of them the next frame shown early; 0.25 to 0.95 of a refresh after it, 173-178 drawn and
  none lost. So the loop moves the retraces, once, to sit halfway between two vblanks (`vb_lock`): one retrace
  interval is up to half a refresh longer or shorter (as a pause or a slow frame makes one), and the retraces stay
  a 60 Hz grid of their own, `vi_ns` apart on the host's clock, never slaved to the display's. They move again only
  if the phase drifts out of the middle half, at most every 10 s. The game's timing is in retraces, so it doesn't
  see the move; with vsync off, `--fast` or `original` nothing moves. With the move every phase drew 177-178.
- **One tick a vblank**, its moment that vblank (where the frame appears), not when the tick began. At a rate at or
  above the refresh (`display`, 120 on a 60 Hz display, 60 on 60) every vblank gets one; below it (60 on 144 Hz),
  the vblank at or after each tick of the rate. A rate that isn't a divisor of the refresh still holds frames for
  uneven counts of vblanks, but each shows its own moment.
- **As late as it makes its vblank.** A tick starts when its drawing ends 1 ms before the vblank, plus how late the
  loop's timed waits return (a decaying maximum). Its drawing is planned at the mean of its cost (without the swap's
  wait) plus twice the mean deviation, both a quarter of the way per tick (`cost_note`, `vb_cost`): ticks that draw
  a frame between two cost several times those that show the game's own or nothing new, so after a cheap one the
  mean alone was far below the next, and that tick drew past its vblank (2-4.7 ms against a mean of 0.2-1.9 ms on
  the synthetic race; a blocking swap then waits for the vblank after, a frame held twice). Its swap then waits
  about a millisecond, if at all. A frame between the shown one and the game's next, which isn't
  drawn yet, waits for its graphics task (which wakes the loop) as long as the tick still makes the vblank.
- **Never across a retrace.** A tick whose drawing would run into the next retrace is drawn after it, if that still
  makes the vblank, else before it; a tick drawn before a retrace due before its vblank leaves its swap until the
  retrace has gone out (`swapped after a retrace`), and drops it if the vblank went by meanwhile (`missed it`). A
  tick that fits none of these is skipped: the vblank shows what the last one did. A swap that takes longer than a
  vblank and one more (a slow GPU, `RUSH_SIM_PRESENT_MS`) counts in the cost, so such a host skips ticks as before.
- **Coarse timers.** Where the loop's timed waits return more than 2 ms late (`RUSH_SIM_TIMER_STEP_US`'s model, or a
  Windows without the high-resolution waitable timer that `pc_os_wait_main` uses), sleeping until a tick's start
  would miss its vblank: the swap is the clock. Each tick is drawn once the vblank before its own has come and its
  swap waits for the vblank, across a retrace if one comes, as the setting off waits after each of the game's frames.

With vsync off, or `--fast`, the ticks are scheduled as before. The exit line `paced by the display's vblanks (H Hz,
the display's | from SDL's whole hertz; N swaps returned at one, M vblank times from the display): ...; the retraces
moved K times ...` gives the refresh paced by, where it came from and the counts above.

**Determinism.** Checking that the setting changes nothing needs runs that repeat. `--fast` runs didn't quite: the
main loop ran RSP tasks and posted their events while a game thread was still computing, and which of its next
libultra calls saw them depended on the host (two runs of the same script started a race two retraces apart). The
loop now does its work only while every N64 thread waits (`pc_os_wait_main`, port/ultra/sched.c), and the state log
reads the game's values then too, not after the retrace's event, which raced the model thread ([ULTRA.md](ULTRA.md)
1, 4). `--fast` runs are now the same every time, to the line.

**Limits.**

- The frame between two is a blend of matrices, not a re-run of the game: per-vertex effects keep the nearer frame's
  values (lighting, texture generation; fog is recomputed), and so do the game's LOD and culling choices, so
  something the nearer frame culled at the edge of the view is missing for that frame.
- Blending matrices elementwise shrinks a turn slightly (0.25% at 8 degrees between frames); larger turns are cuts
  or keep the nearer frame's turn.
- A frame between shows things up to half a frame early or late: 2D, what appears or goes, a teleport.
- Pairing is heuristic. A wrong pair between unrelated transforms would show for one frame between two; the cut
  and jump tests bound how far it can move. The thresholds are in `gfx_interp.c` (in the game's units).
- Instances of one model with no paired parent, spaced closer than they move in a frame (smoke puffs from a fast
  car), can pair with a neighbour: a puff then slides by one place for the frames between.
- No latency is added: the frames between are drawn from frames the game has already drawn, ahead of their swap.
  When the game is late (a frame takes 3 retraces), the next frame is shown as drawn when it is due by the last
  period, a retrace before the VI swaps it in.
- Frames that draw into two color images, or set MP directly (G_MV_MATRIX), are shown as drawn (held). The game does
  neither in the races and menus seen so far.

## 3. The setting

| Where | |
|---|---|
| `rush2049pc.ini` | `[video] frame_rate = original` (default), `display`, `auto` (section 7.9), or a number from 30 to 1000 (60, 120, 144) |
| command line | `--frame-rate R`, or `--set video.frame_rate=R` |
| environment | `RUSH_GFX_FRAME_RATE` |
| F1 overlay | Video, Frame rate: original, 60, 120, 144, display; applies at once |

- `original` shows each frame when the VI swaps it in, as before: nothing is recorded, nothing drawn between, the
  output is what it was.
- `display` is the display's refresh rate (SDL's mode for the window's display, taken again at each event pump), or
  60 without one. Use it with vsync on: a frame at each vblank, for that vblank's moment (section 2).
- A number shows that many frames a second, in step with the retraces: 60 and 120 put a frame on every (half)
  retrace, 60 shows the game's frame and one halfway. With vsync on the ticks follow the display's vblanks: a rate
  at or above the refresh is the same as `display`; a rate below it that doesn't divide it (60 on 144 Hz) holds
  frames for uneven counts of vblanks, so prefer `display`.
- Screenshots (`--screenshot-at`, F12) are the game's frames, at any rate.
- `RUSH_GFX_VERBOSE=1` prints a line per frame pair (transforms blended, moved with the camera, as drawn; views
  cut), 2 the camera's move per view, 3 every transform. `RUSH_GFX_HFR_DUMP=DIR[:FIRST[:COUNT]]` writes the frames
  shown as PNGs (a tool for looking at them; with `--fast` the same every run).

## 4. Tests

| Test | Checks |
|---|---|
| `gfx.interp.*` (`port/gfx/tests/interp_test.c`, 14 cases, and `gfx.interp.all.*` at 2x, 16:9, desktop GL, llvmpipe) | pairs of synthetic frames built the game's way, the frame shown between against a frame drawn directly with the scene as it should look then: [GFX.md](GFX.md) "High frame rate" lists them |
| `hfr_timing` (`tools/port/hfr_check.py`) | three synthetic paths (race2p, drive, demo) on the virtual clock with the setting off and at 120: every state, status and result line the same, frames drawn between in the second run |
| `hfr_paced` (`tools/port/realtime_check.py --pacing display,60,120`) | the synthetic race in real time under a 59.94 Hz vsync swap (`RUSH_SIM_VSYNC_HZ`), the loop's waits precise: paced by the vblanks, at most 10% of them lost to skipped ticks (1.7-5.6% on rocky), frames drawn between at least half of what the same host draws in the same race with vsync off (a run of its own first: how many a host draws is its GPU's, llvmpipe 80-140 in 30 s, the Pi 5 150, rocky 570-670; main before the pacing drew 16-104 where vsync off drew 264), the retraces at 59 a second or more and the audio's bounds (`audio_realtime`'s). Skipped on Windows (the vsync model is Linux's) and under the sanitizers |
| `hfr_paced_queue` (`--vsync-queue never --display-hz 59 --display-timing rate,vblank --max-doubled 5`) | the same at `display` with a driver whose swap never blocks (`RUSH_SIM_VSYNC_QUEUE=never`) and SDL saying 59, once with the platform giving the exact rate (as XRandR) and once its vblanks too (as Windows' compositor): as `hfr_paced`, and at most 5 vblanks in 30 s showing a frame twice with nothing decided for them (1 and 1 on rocky; at SDL's 59, about 22) |
| `hfr_paced_60` (`--vsync-hz 60 --vsync-phase 0.05 --display-timing vblank`) | an exact 60.000 Hz display, its vblanks 0.05 of a refresh after the retraces (the worst phase): as `hfr_paced` (2 ticks lost, 666 drawn between on rocky; 1114 of 1200 lost before the retraces moved) |
| `gfx.display.*` (`port/gfx/tests/display_test.c`) | the refresh arithmetic on made-up values: XRandR modes (1080p60, 59.94, 1440p "144", interlaced, doublescan), Windows' ratios, a refresh SDL's whole hertz could be the rounding of, the compositor's counter as an age, and which period and vblanks are taken from a monitor's, a compositor's (the same monitor, another one, stale, none) and SDL's |
| `hfr_realtime` (`realtime_check.py --frame-rate 120 --present-ms 40`) | the same race with each swap held 40 ms, then until the vsync (a host always behind): at least 5 frames shown a second, the game's own |
| `hfr_timing_game60`, `hfr_paced_game60` | `hfr_timing` and `hfr_paced` with the game at 60 (`--game-fps 60`, section 7.6) |
| all other tests | unchanged: the setting is off by default |

A mutation check of `gfx_interp.c` (14 deliberate bugs: no blend, always one frame's list, no camera move for new
transforms, no eye-fixed vertices, no cut, no teleport, nearest by order, no parent order, no hold on a cut, a fixed
period, 2D blended, no camera median, a spinning transform blended) is caught by the cases each one concerns.

**On a retail cartridge**, locally: the same `hfr_check.py` runs (`--rom <path> --rom-check`, the synthetic paths'
scripts and a few more) give the same lines off and at 120, and the frames shown were inspected and scanned frame by
frame for a frame between two that doesn't fit between its neighbours, which found the eye-plane clipping, the
camera's move from culling boxes and the wheels in [GFX.md](GFX.md) "High frame rate". Nothing measured on it is
kept in the repository.

## 5. Results and cost

Measured 2026-10-27 on the synthetic cartridge, whose scenes (boxes, a checkered ground) are far lighter than the
game's: the costs below are for the renderer's machinery, and real scenes cost more in proportion to their
triangles. "Graphics task" is one of the game's frames drawn live (with the recording when the setting is on);
"between" is one frame drawn between two. Averages over whole runs, the front end included.

**The game's timing, against N64 time** (`hfr_check.py`, `--fast`, original against 120): every state, status and
result line the same, on rocky (ctest `hfr_timing`) and on the Pi.

| Paths | Lines compared | Frames drawn between at 120 |
|---|---|---|
| race2p, drive, demo, race3p, race4p, battle2p, stunt, pause | 46-73 each | 1,684-2,875 each (battle2p's arena draws no 3D: none) |

**Cost, GPU and CPU** (`--fast`, `RUSH_GFX_TIMES=2`: glFinish around each part, so the GPU's time is in it):

| Host | Run | Graphics task: off / 60 / 120 | Between: 60 / 120 (most) |
|---|---|---|---|
| Pi 5 (V3D 7.1, GLES 3.1, Mesa 24.2) | drive, 1 view | 1.86 / 2.04 / 1.94 ms | 3.3 / 2.8 ms (8.5 / 12.2) |
| | race4p, 4 views | 2.66 / 3.37 / 3.26 ms | 4.1 / 3.7 ms (11.6 / 12.4) |
| rocky (Intel UHD 770, GLES 3.2, Mesa 25.2) | drive, 1 view | 1.08 / 1.11 / 1.09 ms | 1.7 / 1.5 ms (3.7 / 4.1) |
| | race4p, 4 views | 1.09 / 1.23 / 1.44 ms | 1.5 / 1.5 ms (5.0 / 6.3) |

At the window's own pixels (1920 × 1080 and 2880 × 1620, upscaled textures or not), the CPU's and the GPU's parts
apart (`RUSH_GFX_TIMES=3`), before and after the renderer's work of 2026-10-28: [GFX.md](GFX.md) "What it costs".
Recording adds up to a few tenths of a millisecond to a graphics task. A frame between redoes the same triangles,
including those the frame rejected (another moment may show them), but not the display-list walk, the texture
lookups or the game's matrices; with the game's scenes it costs about half a graphics task.

**In real time** (headless, the host's clock; retraces a second and the game's frames a second, 10 s of race):

| Host | Run | off | 60 | 120 | 240 |
|---|---|---|---|---|---|
| rocky | race4p | 60.0 retraces/s, 30 fps | | 60.0, 30 fps; 2,010 drawn between, 10 not drawn | 60.0, 30 fps; 4,658 drawn, 55 not |
| Pi 5 | race4p | 60.0, 30 fps | 60.0, 30 fps; 480 drawn, 15 skipped | 60.0, 30 fps; 1,796 drawn, 104 skipped or not drawn | |

**With vsync** (the synthetic race in real time, 30 s, a 59.94 Hz vsync swap, rocky, one run at a time; the
display's vblanks 1798; on main the same race lost most ticks, section 2):

| Waits | Rate | Vblanks lost to skipped ticks | Frames drawn between | Retraces, caught up |
|---|---|---|---|---|
| precise | display | 31-101 | 516-606 | 60.00 a second, 0 |
| precise | 60 | 36 | 533 | 60.00, 0 |
| precise | 120 | 36 | 617 | 60.00, 0 |
| coarse (15.6 ms steps) | display | 0 | 229 (855 of the game's frames at their vblank) | 60.00, 0 |
| none (vsync off, the old scheduling) | display | 3 | 264 | 60.00, 0 |

**A driver that queues frames, and an exact 60 Hz** (2026-10-28, the same race under the model, rocky, `display`,
precise waits; "held twice": vblanks the model's display reached with no tick's decision queued for them, a frame
shown twice that the pacing meant to replace; ticks that show nothing new or are skipped decide to keep the frame):

| Display | Swap | The platform gives | Run | Held twice | Vblanks lost to skipped ticks | Frames drawn between |
|---|---|---|---|---|---|---|
| 59.94 Hz, SDL says 59 | never blocks | nothing (Wayland's case) | 20 s | 15 | 26 | 165 |
| | | the exact rate (X11) | 20 s | 1 | 17 | 178 |
| | | the rate and vblanks (Windows) | 20 s | 1 | 49 | 178 |
| | | the exact rate, `hfr_paced_queue` | 30 s | 1 | 20 | 666 |
| | | the rate and vblanks, `hfr_paced_queue` | 30 s | 1 | 44 | 602 |
| 59.94 Hz, SDL says 60 | blocks | nothing / the vblanks | 20 s | 1 / 0 | | 178 / 178 |
| 60.000 Hz, vblank 0.05 after the retrace | blocks | the vblanks, retraces not moved | 20 s | | 1114 | 0 |
| | | the vblanks, retraces moved 7.5 ms | 20 s | | 2 | 177 |
| 60.000 Hz, at 0.0, 0.1, 0.95 | blocks | the vblanks, retraces moved | 20 s | | 0-1 | 177-178 |
| 60.000 Hz, at 0.5 | blocks | the vblanks, none moved | 30 s | 16 | | |

The races' frames between are the last 10 s of a 20 s run (the front end draws no 3D), hence 165-178 there and
600-666 in 30 s. The rows above planned a tick by the mean of its cost. At 60.000 Hz the blocking model then showed
16-24 frames held twice in 30 s at any phase, moved or not, and the 30 s runs of `hfr_paced` at 59.94 too: ticks
drawing a frame between that started by the mean and drew past their vblank (section 2, "As late as it makes its
vblank"). Planned by the mean and twice the deviation (the ctest runs, rocky, 30 s, before and after):

| Test | Held twice | Vblanks lost to skipped ticks | Frames drawn between |
|---|---|---|---|
| `hfr_paced_60` (60.000 Hz, the worst phase, moved) | 24 -> 9 | 6 -> 2 | 644 -> 669 |
| `hfr_paced` (59.94 Hz, blocking): display, 60, 120 | 55, 24, 35 -> 4, 4, 5 | 39, 45, 40 -> 37, 36, 36 | 571, 641, 573 -> 669, 672, 640 |
| `hfr_paced_queue` (never blocks): rate, vblank | 1, 1 -> 1, 1 | 21, 45 -> 43, 44 | 660, 592 -> 637, 660 |

`audio_realtime` was the same (0 underruns, 2 buffers as silence). Three and four deviations were tried on the
60.000 Hz and 59.94 Hz races: 1-6 held twice, but 18-122 ticks lost in a run. What remains at two is the first frame
between after a run of cheap ticks (the race starting) and a frame that costs more than its neighbours.

With precise waits (Windows' high-resolution timer) each vblank gets its tick; the frames drawn between are more
than the retrace grid gave without vsync, since a tick waits for the game's next frame while it can. With coarse
waits the swap is the clock: every vblank still gets a frame, but drawn a refresh ahead, more of them are the
game's own. The audio's bounds held in every run (one underrun at the start, a few buffers as silence of 3400, none
skipped).

Hosts with headroom draw nearly every frame between and keep the game's retraces exactly. A host without it draws
fewer: the ticks that would come too close to a retrace, or find the GPU busy, are skipped, and while the host drops
retraces none is drawn, so the game runs as it would with the setting off. The Pi 5 is shared with other work here;
its numbers vary from run to run.

## 6. What's left

- A real display: vsync pacing was checked only headless, under the model (a swap that blocks until the next vblank,
  or a driver that queues frames). The display's own refresh and vblanks (section 2) were run on no real Windows or
  X11 desktop yet: `paced by the display's vblanks (... the display's; ..., M vblank times from the display)` in the
  log says they were found. A display the queries don't cover (Wayland: wp_presentation feedback, not done; a
  queueing driver there) still runs at SDL's whole hertz between blocking swaps, and holds a frame twice about once
  a second where that is off.
- Windows' compositor's timing is the composition clock's (`DwmGetCompositionTimingInfo(NULL)`): with monitors of
  different refreshes, a window on one the compositor doesn't follow gets its monitor's exact period but no vblank
  times (the phase from blocking swaps). In exclusive fullscreen the compositor may not present at all; its vblank
  still comes from the display's clock.
- With a swap that blocks, 4-9 vblanks in 30 s still show a frame twice with no decision for them (section 5): the
  first frame between after a run of cheap ticks, and a frame that costs more than its neighbours. Planning by what
  the tick will draw (a frame between or the game's own: the renderer knows) would cover the first kind.
- On the Pi 5 the synthetic race draws about 150 frames between in 30 s, against 650 on rocky, with the vsync
  pacing and without it alike (151 and 151 at 60, vsync off and on; 0 not drawn, the rest held), and the race's
  states come at the same retraces on both: which frames are interpolated differs on that host, not when the ticks
  run. `hfr_paced`'s frames-between bound is relative to the same host's with vsync off for that reason. With the coarse-wait model (15.6 ms steps for every timed wait, the
  main loop's included) the Pi's race at 60 or 120 has 806 of 1800 retraces late and none drawn between; on Windows
  the main loop waits on a high-resolution waitable timer (and asks for a 1 ms period), so that model is the case
  where Windows refuses both.
- The retraces are moved against the vblanks only on displays that keep their phase (section 2). At 59.94 the bad
  phase comes for about 2.5 s in every 17 (the vblank just after a retrace); moving the retraces to follow it would
  make them the display's grid, not their own 60 Hz, so it isn't done.
- Adaptive vsync (swap interval -1) and a present thread of its own were weighed and not done: the first still
  blocks when on time, the second moves the GL context off the thread that runs the graphics tasks.
- Per-vertex effects of the moment (lighting, texture generation) and LOD and culling choices are the nearer frame's.
- Instances of one model with no paired parent, closer together than they move in a frame, can pair with a neighbour
  (a smoke trail behind a fast car).
- A host with little headroom (the Pi 5 in busy scenes) draws few frames between; the renderer itself would have
  to get faster there.

## 7. The game at 60 (game.frame_rate)

Written 2026-11-03. `game.frame_rate = 60` makes the game itself draw a frame every retrace: its menus, HUD, cameras,
effects and timers step 60 times a second at the N64's speed, and every frame shown is one the game drew. The car
physics was per retrace already (section 1) and stays as it is. `original`, the default, is the N64's game exactly.

### 7.1 The setting

| Where | |
|---|---|
| `rush2049pc.ini` | `[game] frame_rate = original` (default) or `60` |
| command line | `--set game.frame_rate=60` |
| F1 overlay | Game, Game frame rate: original (30), 60; from the next screen |

- **The name.** `video.frame_rate` is how many frames are shown, `game.frame_rate` how many the game computes and
  draws: one word for one quantity, the section saying whose. They combine: `game.frame_rate = 60` with
  `video.frame_rate = 120` draws one frame between each two of the game's (7.6).
- **Only 60.** The game's clocks count whole retraces (section 1): a frame of one retrace is the shortest there is,
  so a rate between 30 and 60 would be frames of one and two retraces mixed, and above 60 there is nothing to step.
- **From the next screen.** The game reads a copy, `pc_fps60` (port/config/game_fps.c), which it takes where its state
  changes: game_loop's branch for a state change (`src/blob/groups/input_aux/game_loop.c:87`, pc_game_fps_begin).
  That is a new screen of the front end, the race's start (PrePlay, the countdown, the race), the results, the attract
  demo. A change in the overlay applies there, never in the middle of a screen, so a screen's per-frame steps and the
  scheduler's swaps agree for all of it, and nothing that one frame rate counted is read by the other. Until the
  first state change the copy is original. The setting is not an environment variable.

### 7.2 How the game runs at 60

- **The swap.** The scheduler swaps a finished frame after `PC_SWAP_RETRACES` retraces since the last swap: 2 at
  original, 1 at 60 (`src/rom/lib_1050_sdk.c:254, 331, 353`, the three tests of section 1). The main loop's start
  test (`src/blob/groups/mainloop/mainloop.c:97`) needs no change: EndGraphics stores the retrace count in
  `D_8002EB98` (`src/blob/world_collision_response.c:179`), and the next retrace differs from it, so at 60 a frame
  starts at the retrace after the one it was swapped in. The game's two frame records and its two framebuffers
  alternate as before.
- **The frame's task.** EndGraphics puts the frame's task in the scheduler's task queue, which the scheduler drains
  only at a message on its command queue (`__scMain`). In real time on the PC the RSP finishes the retrace's audio task
  at once, before the frame is done, so that message was the next retrace, and every frame was shown two retraces
  after it began (rocky, the synthetic race in real time: 1,210 to 1,487 of the game's frames in 1,800 retraces,
  however it was presented). At 60 EndGraphics also sends the scheduler 670, as the audio task's sender does
  (`world_collision_response.c:178`): 1,738 to 1,772 of 1,800 in the runs of 7.6, the rest the front end's loads. Original keeps the N64's
  messages; on the virtual clock the main loop runs the audio task after the frame anyway, so `--fast` runs drew a
  frame every retrace before.
- **The frame's length.** viUpdateTime (`src/rom/lib_1050.c:74`) gives each frame's retraces, at most 5, to
  `pc_game_frame_note`: `pc_frame_scale` is the frame's length in frames of two retraces (0.5 at 60),
  `pc_frame_steps` the whole ones it completed (0 and 1 in turn at 60). The frame time
  `D_8002EB94` and the frame delta `D_8002EB98` are the game's own, 1/60 s and 1 at 60. On a host that can't keep up,
  a frame of two retraces at 60 has a scale of 1 and a step of 1: the N64's frame.
- **The steps.** What steps by the frame time or the retraces keeps its speed by itself. What steps by a constant
  tuned to a frame of two retraces goes through `port/include/pc_game_fps.h`'s macros, each file defining the N64 side
  at file scope (the original tokens: [DECOUPLE.md](DECOUPLE.md) 1's conventions, `n64_neutral.py`):

| Macro | For | At 60 | At original |
|---|---|---|---|
| `PC_FSCALE(x)` | a float step a frame (a slide, a fade, a spin, a rate limit) | x × the frame's length | x |
| `PC_STEPS(n)` | an integer step a frame where a step of 0 is harmless | n × the whole 30 Hz frames | n |
| `PC_EACH30(stmt)` | a count, or a test a step of 0 would repeat (`if (--t == 0)`), an integer filter, a random draw a frame | the statement on the frames that complete a 30 Hz frame | the statement |
| `PC_FILTER(e)` | a filter or decay a frame, `x = x·e + y·(1 − e)`, `v *= e` | e to the power of the frame's length | e |

  Fourteen files have a macro of their own for a form these don't cover (the follow camera's √e, a filter's other weight,
  an ease over the frames left, a gap in model time, a timer counted in 30 Hz steps); each is defined beside its use
  and listed in 7.3. Every macro gives exactly the N64's value at original, with no float operation added, and
  `-ffp-contract=off` (CMakeLists.txt) keeps the compiler from fusing differently.
- **The follow camera.** `func_800E8D50` (UpdateCarObj) moves `pos = pos·e + target·(1 − e)` once a frame, e being
  0.6 × the slot's elasticity for a frame of two retraces; the N64 multiplies e by 0.75, 0.6 and 0.5 for frames of 3,
  4 and 5, which for e = 0.6 is 0.45, 0.36 and 0.30 against e^1.5, e^2, e^2.5 = 0.465, 0.36, 0.279: close to
  e^(frames), the same time constant. A frame of one retrace now takes √e (`func_800E8D50.c:40-46, 56`): toward a car that
  stands still, two frames at 60 leave what one at 30 does, to rounding. Toward a moving car the lag at 60 is longer, by
  the finer sampling: at 60 feet a second and e = 0.6, 3.000 feet at 30 and 3.436 at 60 (`assets_fps`), 0.05 s
  against 0.057 s; a continuous filter of that time constant would lag 3.92 feet. Frames of 2-5 retraces at 60 are
  the N64's.

### 7.3 The per-frame audit

**How the sites were found.** A build instrumented with `-finstrument-functions` (the game C only; a scratch hook,
not in the repository) recorded for each game function the N64 thread that ran it and in how many game frames, over
106 runs: the 25 synthetic paths at original and at 60 on rocky, and on the Pi on the retail cartridge the same 25
scripts plus a race to the results, the attract demo and the front end's screens, at both settings. 1,985 functions
ran: 1,595 on the game thread or the main thread (per frame), 231 on the model thread (per retrace), 109 of them on
both (the math, the sound starts, the random numbers). In those 1,595 functions' bodies, lines that step state were
picked out: the frame time (`D_8002EB94`, `gFrameSeconds`), the frame delta (`D_8002EB98`), the frame counter
(`frame_counter`), the retrace counter, `++`/`--`, `+=`/`-=`/`*=` a literal, `x = x op literal`: 1,594 lines in 224
files over the first 78 runs, which were read in six parts, each with the rest of its files' per-frame functions and
those the coverage didn't run; then 31 lines in 8 more files the last 28 runs reached (the name entry, the records),
39 that step by a named constant (`p->a0C += D_80123F30`), and a last sweep for self-updates of any form, which found
two through accessor macros (`PC_MODEL_SCALE(i) = PC_MODEL_SCALE(i) * 0.97f + ...`). Every use of the frame time in the
C was read, run or not. The 775 per-frame functions with none of those lines step nothing a frame (they draw, look up or decide from
state set elsewhere).

**Verdicts.** The table below has a row for each site that steps per frame (a function's several uses of the frame
time are one row; a row of two verdicts counts in both), by area: of its 98 rows, 45 are changed (a constant tuned to
two retraces, taken to the frame), 25 step by the frame time already, 6 by the retraces (the retrace counter, the
deadlines, the frame delta), and 26 are unchanged for the reason given (an edge, an event, a count within a frame, a
value made again each frame, init). In lines: 83 lines inside functions changed, in 31 files, 6 of them the mechanism
of 7.2 and 77 per-frame steps in 27 files (2026-11-04: PrePlay's load, changed, and the stuck detector, by model
time, were added: 7.4, 7.5); of the 307 lines that use the frame time (`D_8002EB94`, `gFrameSeconds`),
one changed (the reload, `shots.c:1069`); of the frame delta's 11 uses, two changed (the texture scrolls, `race_setup_1.c:154,
160`), and the follow camera reads it once more (√e for a frame of one retrace).

The table has every site that steps per frame, file:line in the tree as committed. Loop indices, pool counts,
display-list building, text layout and init code are counted in the last row of each area, not listed one by one.

| file:line | function | what it steps | verdict | change |
|---|---|---|---|---|
| **The frame and the scheduler** | | | | |
| src/rom/lib_1050_sdk.c:254, 331, 353 | __scHandleRetrace, __scTaskReady, __scExecTask | the swap after two retraces | changed | `PC_SWAP_RETRACES`: 1 at 60 |
| src/rom/lib_1050.c:74 | viUpdateTime | the frame's retraces and time | delta | notes the frame's length (`pc_game_frame_note`) |
| src/blob/groups/input_aux/game_loop.c:87 | game_loop | a state change | changed | latches the setting, and tells the race watch the state it changes to (`pc_game_fps_state`, 7.7) |
| src/blob/groups/input_aux/game_loop.c:92 | game_loop | the idle overlay after 300 s, retraces × seconds a retrace | delta | |
| src/blob/groups/input_aux/game_loop.c:125 | game_loop | `frame_counter++` (gFrames) | unchanged | nothing reads it but game_loop's reset, by any of its names |
| src/blob/groups/mainloop/mainloop.c:97, 101-104, 111 | world_trigger_activate, MainEndFrame | the frame's start test, the tasks pending, `D_80151AE0` (retraces since the frame began) | unchanged | scheduling, not time; right at one retrace (7.2) |
| src/blob/world_collision_response.c:178, 179 | world_collision_response (EndGraphics) | the frame's task; `D_8002EB98 = frameCount` | changed / unchanged | wakes the scheduler at 60 (7.2) |
| **Cameras** | | | | |
| src/blob/func_800E8D50.c:56-59 | func_800E8D50 (UpdateCarObj) | the follow camera's elasticity a frame | changed | `PC_CAM_ELASTIC`: √e for a frame of one retrace (7.2) |
| src/blob/func_800EB028.c:138 | func_800EB028 (setcamview, the driver's eye) | `cur = cur·e + in·(1 − e)` on the car's peak forces | changed | `PC_FILTER` |
| src/blob/func_800EB028.c:140 | func_800EB028 | the eye's bob, `y = y·e − spring` | changed | `PC_FILTER`, and `PC_DECAY_IN`: the spring × (1 − e^f)/(1 − e), so it settles where the N64's does |
| src/blob/groups/func_800E92C8/group.c:3963 | func_800EA108 | the chase camera's gain of .13 a frame toward its target | changed | `PC_GAIN`: 1 − (1 − g)^f |
| src/blob/groups/func_800EA3F4/group.c:202, 204 | func_800EA3F4 | the share of the last frame's camera directions kept a frame | changed | `PC_FILTER` |
| src/blob/groups/func_800EA3F4/group.c:208-227 | func_800EA3F4 | the camera blends' ramps | time | |
| src/blob/func_8008B3C8.c:201-220 | (a weak copy of func_800EA3F4's blends) | the same ramps | time | |
| src/blob/groups/visual_tire_module/anim_tire_donor.c:224 | anim_state_update | the camera spring (the in-car view's bob, `func_800EB028.c:140`), `·0.75 + random` a frame | changed | `PC_SPRING_KEEP`, `PC_SPRING_NOISE`: every frame, 0.75^f and the noise × `pc_fps_noise` (the bob's spread kept); it was `PC_EACH30`, a bob stepping 30 times a second at 60 (2026-11-04, 7.6) |
| src/blob/groups/camera_aspect_ratio/group.c:4434, 4525 | camera_update | the scripted cameras' key time; a texture cycle's timer | time | |
| src/blob/groups/camera_aspect_ratio/group.c:4533 | camera_update | the texture index, once the timer runs out | unchanged | an event of a timed timer |
| src/blob/groups/camera_aspect_ratio/group.c:3927-3990 | camera_free_look, PlayMoveSound | a key's rotation from t/dur; a volume from speeds | unchanged | absolute time, rates |
| src/blob/groups/steady_camera/group.c:3946 | func_800E95DC | the camera's transition, elapsed += dt | time | |
| src/blob/groups/steady_camera/group.c:3945; func_800E92C8/group.c:3943 | func_800E95DC | `v84[1] += 4` | unchanged | after func_800E8D50 sets the place, elasticity 0: an offset |
| src/blob/func_800EB90C.c:209, 241, 268, 285 | func_800EB90C (the attract demo's cameras) | the switch by model time and a deadline; the view button; averages | delta / unchanged | |
| src/blob/groups/particle_knot/group.c:1238-1500 | func_8009F058 and the view set-up | the projection, lookat, fog | unchanged | made again each frame |
| src/blob/groups/particle_knot/group.c:1761-1810 | func_800BFD8C (InterpQuats) | a quaternion blend | unchanged | its caller's frac is absolute time |
| src/blob/entity_iterate.c:36-38 | entity_iterate (CameraCollision) | the eye moved a quarter unit on a hit | unchanged | a correction of a place made again each frame |
| **The cars, the drones and the race** | | | | |
| src/blob/groups/render_large_objects/group.c:431, 435, 445, 449 | render_large_objects (DoDrones) | the drones' brake scale and time boost, ±.01 a frame toward their targets (the drones' physics reads both) | changed | `PC_FSCALE` |
| src/blob/groups/render_large_objects/group.c:171-458 | render_large_objects | the targets, from places and distances | unchanged | decided once a frame, as on the N64: at 60 every retrace (7.4) |
| src/blob/func_800DE860.c:151 | func_800DE860 (set_catchup, from DoDrones) | each human car's catch-up scale (the model's +0x400, which its physics reads), 0.03 of the way to its target a frame | changed | `PC_FILTER(0.97f)` and `PC_FILTER_REST(0.97f, 0.03f)`, its other weight 1 − e^f |
| src/blob/func_800CFCA8.c:3855 | func_800CFCA8 | a car's blink (its appearance bit 8), counted in frames | changed | `PC_EACH30` |
| src/blob/groups/update_game_data/update_game_data.c:340 | func_800E7FA0 | the skids' intensity, −0.125 a frame | changed | `PC_FSCALE` |
| src/blob/groups/update_game_data/update_game_data.c:408, 419 | update_game_data | the body's pitch (0.6/0.4) and roll (0.7/0.3) filters | changed | `PC_FILTER`, `PC_FILTER_REST` (1 − e^f) |
| src/blob/groups/update_game_data/update_game_data.c:371-376 | update_game_data | the model's stunt rotation handed over and zeroed a frame | unchanged | its reader sums it: the same total at any rate |
| src/blob/groups/visual_tire_module/anim_tire_donor.c:228, 229 | anim_state_update | the fast tire's spin, +3.7 a frame; the slow one by model time | changed / time | `PC_FSCALE` |
| src/blob/groups/visual_tire_module/buffer_swap.c:122, 183, 198 | tire_extend, buffer_swap | the wings out and in by sixths, one at a frame whose model time passed a 0.015 s gap | changed | `PC_GAP30`: 0.03 s at 60, 30 sixths a second |
| src/blob/groups/visual_tire_module/rocket.c:345, 351 | AnimateFrame | the car's fade, alpha ±0x19 a frame | changed | `PC_EACH30` |
| src/blob/groups/visual_tire_module/rocket.c:510, 516, 522 | AnimateRocketFlame | the flame's shrink, −(Random(.025) + .01) a frame | changed | `PC_FSCALE` |
| visual_tire_module (rocket.c, anim_tire_donor.c:251, buffer_swap.c) | AnimateFrame, anim_state_update, buffer_swap | translucency by a flag toggled each frame | unchanged | the alternation is the effect: 30 Hz at 60, 15 at 30 |
| src/blob/groups/car_checkpoints/car_checkpoints_func_800F8EC8.c:150, 170 | func_800F8EC8 (the checkpoints) | the distance, a sum of moves; a crossing's time within the frame | unchanged / time | finer at 60 (7.4) |
| src/blob/groups/car_checkpoints/car_checkpoints_func_800D1AB0.c:74-96 | func_800D1AB0 | the places | unchanged | made again each frame |
| src/blob/groups/camera_scene_manager/group.c:4276, 4287, 4083-4241 | camera_scene_manager, the trick scores | the stunt idle and combo timers; points by model time | time | |
| src/blob/groups/camera_scene_manager/group.c:3943-4048 | func_800C2430, 26C4, 2944 | the trick detectors' sums of the car's rotation; a full turn off on a trick | unchanged | rate-independent sums; events |
| src/blob/func_800E7DD0.c:56, 63 | func_800E7DD0 | the race end's settle timer | time | |
| src/blob/groups/update_game/countdown.c:171-325 | countdown | the race's state machine | time | deadlines in retraces, model time |
| src/blob/groups/countdown/countdown.c:112, 115 | animCountdownTimer | the beeps; the blink by the retrace count | delta | |
| nonmatching/game/display_list_flush.c:379 | display_list_flush (PrePlay) | the track's load, a wave a call (LoadTrack waits on four waves), a call a frame | changed | `PC_LOAD_ON30`: at 60 called on the frames that complete a 30 Hz frame (the countdown began 5 or 6 retraces sooner) |
| src/blob/menu_controller_remap.c:197-226 | menu_controller_remap (detect_drone_abort, from render_viewport_init) | a car under 25 ft/s for 6 s of model time aborted and resurrected | time | its test at a frame: at 60 the abort can come a retrace sooner (7.4) |
| src/blob/groups/credits_scroll_grp/gr3_84d4.c:120; gr3_9f54.c:185-188 | animLapTimeText, animFlag | the lap time's display, the flag's frames by dt × 1000 | time | |
| src/blob/race_countdown_display.c:116 | race_countdown_display | `laps++` | unchanged | a checkpoint event |
| src/blob/groups/car_checkpoints/race_tail.c:148 | render_viewport_init | `numCrashes++` | unchanged | once a crash |
| **Effects** | | | | |
| src/blob/hud_render.c:278, 284, 286 | hud_render | the car glow's pulse, ±0.04 and ±0.03 a frame | changed | `PC_FSCALE` |
| src/blob/hud_render.c:288, 291 | hud_render | the glow's 15-frame cycle | changed | `PC_ON30`, `PC_EACH30` |
| src/blob/hud_render.c:322 | hud_render | a puff's fade, alpha −9 a frame (u8) | changed | `PC_EACH30` |
| src/blob/hud_render.c:326-328 | hud_render | a type-4 puff's jitter, three random draws a frame | changed | `PC_EACH30` |
| src/blob/hud_render.c:329, 350, 352 | hud_render | the puffs' growth, +0.05 and +0.03 a frame | changed | `PC_FSCALE` |
| src/blob/hud_render.c:262, 312, 331, 354-356 | hud_render | the puffs' timers, life and motion | time | |
| src/blob/groups/entity_spawn_init/entity_spawn_init.c:210 | entity_spawn_init (StartSmoke) | the continuous spawns each frame: tire smoke, dust (its every second or third call, its fading colour), the exhaust trail; their random draws | changed | at 60 on the frames that complete a 30 Hz frame (`PC_ONLY`); a blast's burst (an event) and the latched wheel effect at any frame |
| src/blob/groups/func_8008B640/group.c:3824, 3825, 3831 | physics_velocity_integrate_a | the backfire flame's frame, +1 a frame, a random one at 3, −1 to die out | changed | `PC_STEPS`, `PC_IF30` |
| src/blob/groups/blast/blast.c:291 | entity_physics_update (AnimateBlast) | a blast on no car steps every frame, its 1/30 s timer bypassed | changed | `PC_EACH30_IF` |
| src/blob/groups/blast/blast.c:290-296 | entity_physics_update | a blast on a car, a step every 0.0333 s | time | |
| src/blob/entity_collision_detect.c:111-135 | func_80090B68 (the blast ring) | its phases every 0.0333 s | time | |
| src/blob/entity_update_callback.c:141, 171-208 | entity_update_callback (AnimateNewBlast) | life and steps behind a 0.0333 s gate | time | |
| src/blob/audio_channel_setup.c:109-114; func_800947F0.c:19; func_800B7FF8.c:19; func_8010E0FC.c:213-215; func_8010E694.c:821-823; func_8010C974.c:206-254; func_8010E4E4.c:135-139 | the animated objects, coins, icons | timers, phases, spins by rate × dt | time | |
| src/blob/race_setup_1.c:89, 174 | race_setup_1 | the palette and object animations' delays | time | |
| src/blob/race_setup_1.c:154, 160 | race_setup_1 | a texture scroll that runs every few 30 Hz frames adds step × the frame's retraces | changed | `PC_SCROLL_RETRACES`: the N64's 2 at 60; every frame (ticks 0) it was by the retraces already |
| src/blob/groups/particle_knot/group.c:849-1000 | particle_system | LOD and distance fade | unchanged | made again each frame |
| **Front end and HUD** | | | | |
| src/ovl_a/groups/cleanup_colortrace/group.c:644, 917 | func_803A1EAC (the car select) | the frames since every player was ready, the screen leaving at 10 | changed | `PC_EACH30` (at 60 the race began 11 retraces early) |
| src/ovl_a/groups/cleanup_colortrace/group.c:1608 | func_8039EF08 | the cars turned to the front, angle −= angle/(frames left − 1) a frame | changed | `PC_EASE_DIV`: a·(1 − (1 − 1/n)^f) |
| src/ovl_a/groups/runtime_a_c93c_closure/group.c:820-822 | func_8038A634 (SetFogIntensity) | the fog colour, an integer tenth of the way a frame | changed | `PC_EACH30` |
| src/ovl_a/groups/runtime_a_c93c_closure/group.c:739-767, 966-976, 1340, 1461-1508 | the track select | the map's angle, the carousel, the fog height, the buttons | time | the carousel `pos += frames·(target − pos)/10`, frames = dt·30: at 60 0.9025 of the way a 30 Hz frame against 0.9 |
| src/blob/groups/input_aux/countdown_handler.c:98 | countdown_handler | the "controller removed" scroller, a pixel a frame | changed | `PC_SCROLL_DEC`: `PC_STEPS(1)` |
| src/blob/groups/controller_poll/group.c:3796 | controller_poll | the frames of input skipped | changed | `PC_EACH30` |
| src/blob/groups/controller_poll/group.c:3834 | controller_poll | a held button's repeat, every 0.2 s of retraces | delta | |
| controller_poll, func_800E7134 (gPrevPressed) | controller_poll | a press: the model thread ORs each retrace's in, the frame takes them once | unchanged | a press registers once at any rate |
| the menus' screens: optionscr_buttons/buttons.c:169-232, physics_sym/group.c:135-197, controlscr/target.c:107-157, audio_options_menu/func_800D7634.c:103-162, runtime_a_callers_deep/group.c:730-964, seven_option_group/eligibility.c:103-142, seven_option_init/group.c:85-125, text_helpers/group.c:450, 602-713, start_screen/group.c:378-452, availability_refresh/group.c:503-597, cleanup_colortrace/group.c:1211-1256, 1595-1622 | each screen's updateButtons | the buttons' flips (4π·dt), slides (180·dt), the cursor's spin (2π·dt) and slide (its speed from the distance left, × dt) | time | |
| the same screens' control functions | (each screen's control) | selection, values, the cheat code's steps | unchanged | edges and the input's repeat edges |
| src/blob/func_80109A60.c:172 | func_80109A60 | a blink from the retrace count | delta | |
| src/blob/func_800FC9F8.c:81-82 | func_800FC9F8 | the HUD's point list scroll by dx·dt | time | |
| src/blob/groups/menu_save_options/group.c:764 | SaveServer | a two-frame wait before the first pak scan | unchanged | unseen: 2 retraces at 60 |
| src/blob/groups/demo_pair/demo.c:119, 190; showhud.c:196-200 | the attract demo, its credits | its 30 s deadline; the credits' page switch | time | |
| **Battle and stunt (runtime image B)** | | | | |
| src/ovl_b/groups/shots/shots.c:325-331 | ElevateGun | the guns' yaw and pitch, ±0.01 a frame | changed | `PC_FSCALE` |
| src/ovl_b/groups/shots/shots.c:530 | _UpdateShotObject | a bounced mine's spin, 0.18 a frame | changed | `PC_FSCALE` |
| src/ovl_b/groups/shots/shots.c:556 | UpdateDefense | the shield's spin, −0.005 a frame | changed | `PC_FSCALE` |
| src/ovl_b/groups/shots/shots.c:738-739 | UpdateShots | a mine's drag, vel ·= 0.9 a frame | changed | `PC_FILTER` |
| src/ovl_b/groups/shots/shots.c:972-978 | UpdateShots | a new shot's first frames, flags 0x10 → 0x20 → 0x40, one a frame | changed | `PC_EACH30` |
| src/ovl_b/groups/shots/shots.c:1069 | ShotUpdate | the reload, set at each shot, −= dt | changed | `PC_DT30`: counted in 30 Hz frames, so it ends where the N64's frames do (0.0666667 s: 6 retraces, not 5; a machine gun's 10 shots a second, not 12) |
| src/ovl_b/groups/shots/shots.c:540, 557, 720-903, 1014-1023 | UpdateDefense, UpdateShots, AnimateCase | lifetimes, motion by vel·dt, gravity, homing, the sonic shot's scale per 0.0333 s, casings | time | |
| src/ovl_b/groups/runtime_b_nearest_steering/group.c:93-96 | func_8038DDDC (the homing missile) | its turns, ±0.04 and ±0.03 a frame | changed | `PC_FSCALE` (not run in the coverage) |
| src/ovl_b/groups/runtime_b_resource_callbacks/group.c:113, 125, 128 | UpdateInvs | the invisibility fade, alpha ∓8 a frame | changed | `PC_STEPS(8)` |
| src/ovl_b/groups/runtime_b_resource_callbacks/group.c:138 | UpdateInvs | `countdown--`, the shot's flash | changed | `PC_EACH30` |
| src/ovl_b/groups/runtime_b_resource_callbacks/group.c:118, 130, 158, 190, 222 | UpdateInvs, the power-ups | fade timers, phase timers | time | |
| src/ovl_b/groups/runtime_b_trail/group.c:84 | func_8038CB20 | a fade whose interval is 0 steps every frame | changed | `PC_FRAME_INTERVAL`: 1/30 s at 60 |
| src/ovl_b/groups/runtime_b_trail/group.c:74, 82, 102 | func_8038CB20 | fades and trail lifetimes | time | |
| src/ovl_b/func_80392894.c:157 | animStuntPoints | the bonus text, 11 frames a count | changed | `PC_EACH30` |
| src/ovl_b/func_803936A8.c:130 | the deaths mode's point events | `age++`, a slide over 40 frames | changed | `PC_EACH30` (not run in the coverage) |
| src/ovl_b/groups/weapons/weapons.c:356-429; runtime_b_status_scene/group.c:287 | AnimateWeapon, func_80391B00 | the muzzle flash, rip interval; the stunt coin slide | time | |
| src/ovl_b/func_80392FE4.c:131, worlddam/worlddam.c:76-142, the rest of src/ovl_b | animHealth, the decals, set-up, drawing | their positions and counts | unchanged | set each frame, events, init |
| **Everything else** | | | | |
| src/blob/groups/billboard_render/func_800F207C.c:54-127; func_800D2054.c:41; highscore.c:278-290; func_800D18D8.c:18; car_setup_confirm.c:109-113 | the name entry, the lap times, the records' names, a serial, the places locked at the finish | the cursor and the name on repeat edges and presses; a lap's time; layout; once an event | unchanged | events and drawing |
| src/rom (libultra, MusyX, the controller pak, gu) | (the drivers) | loop indices, locks, audio-time fades, retries | unchanged | nothing steps per game frame |
| the remaining per-frame functions (775 with none of the lines searched) | (all) | pool allocators, display lists, text, lookups, the HUD's values made from the cars each frame | unchanged | nothing steps a frame |

Shared with the model thread and left alone (no per-frame step in them, or the step is the physics'):
`menu_options_screen` (the car-car push), `race_countdown_display`, `stunt_combo_display`, `camera_follow_path` and
`func_800C4F68` (the resurrection's place), `func_800ADCE0`, the math helpers.

### 7.4 What the game thread feeds the physics

The car physics runs per retrace on the model thread, and the per-frame logic changes it in a few places. At 60 those
decisions come every retrace, at retraces a run at original doesn't decide at:

- **DoDrones** (`render_large_objects`, once a frame): each drone's time boost and brake scale, which the drones'
  physics reads, and through set_catchup (`func_800DE860`) each human car's catch-up scale in a race of more than one
  player (in a retail two-player battle the scale of car 0 differs from tick 1). Its rate limits are scaled (7.3); its targets come from the places, which are made each frame
  (`func_800D1AB0`), and in the countdown from the places' order alone. In every synthetic race with drones the
  physics first differs at model tick -59 or -58, in the countdown, by a drone's time boost (0x7F0 in the model
  record: found by dumping the model records each retrace at both rates).
- **The checkpoints** (`func_800F8EC8`, once a frame): a crossing is a change of side of the next checkpoint's plane
  between two frames, within its radius at the second; the last checkpoint passed is where a resurrection puts the
  car (`func_800C4F68`, SetResurrectionData). In the synthetic attract demo, car 0's last checkpoint is 1 at 60 from
  tick 2 and 0 at original, and at tick 214 the drone that resurrects is put at another corner of the square loop.
- **The stuck detector** (`menu_controller_remap`, the arcade's detect_drone_abort,
  `src/blob/menu_controller_remap.c:197-226`, once a frame from render_viewport_init,
  `car_checkpoints/race_tail.c:184`): a car under 25 ft/s starts a timer at
  its model time, and one still under it 6 s of model time later is aborted: SetResurrectionData (`func_800C4F68`),
  the car record's dead state (`+0x359`, which stops the model thread's step of it, `func_800E56F8/group.c:4156`) and
  the resurrection. The timer is model time, but its test runs at a frame: at 60 every retrace, at original every
  other one, so the abort comes at the first retrace past the 6 s at 60 and at the first frame past it at original,
  the same tick or one later. In the retail race of one car alone to its end (`r_solo3`, 7.5) the first three
  resurrections come at the same tick at both rates and the fourth a tick sooner at 60, at the same place.
- **The random numbers.** `rand` (seed `D_8011735C`, the static copies in `entity_update_callback.c`,
  `func_8008B2E4.c` and others) is drawn by both threads: 5,059 times by the model thread (a battle resurrection's
  offset, `resurrect.c:505`, the only draw in a model step that feeds a car's place; sounds) against 24.5 million by
  the game thread (effects) in the 106 coverage runs. At 60 the game thread draws at other moments (the spawns and
  jitters above draw at the N64's rate, the flames and flickers each frame), so the seed differs from original's
  through a race, and a battle resurrection's offset with it. A race's resurrections (FindHumanResPt, from the
  checkpoints) draw none, and nothing else the model thread draws reaches the physics: the seed is left shared.
- **Battle:** a sonic shot pushes the car it hits from the game thread (`func_8038D498`), at the frame it finds the
  hit.
- **The car records** (`pc_cars`): the game thread makes them again once a frame from the model records
  (update_game_data), and the model thread reads parts of them: the region a resurrection takes its place from, and
  the dead state above.

None of these is a fault at 60: each is the N64's logic deciding once a frame, and at 60 a frame is a retrace. The
player's controls are read by the model thread each retrace (`ControllerUpdate`, into `D_8013FED0`, which
`func_800E6460` and `func_800E627C` turn into the model record's gas, brake and steering), the race's clock is the
model thread's (`D_801543CC`, ticks × 1/60), modelrun and the countdown's deadlines are in retraces, and a model step
never lets the game thread run inside it: it gives the baton only to the sound system's thread (id 10, priority 10,
`menu_save_options/hud_setup.c:148`) when its sounds take the sound lock `D_80142728` (`pc_os_trace` over
`r_solo2`'s steps 3716-3720: 0, 6, 0, 8 and 0 hand-overs to it and back, the same at both rates): a race
the game thread decides nothing in (no drones, no shots, no resurrection, or each at the same tick) has the same
physics to the bit (7.5). The ghosts record and replay the controls in the model thread too (`func_800E5D64`: per
model step, with the recording's retrace time in the header), so a ghost is the same data at either rate (7.5).

### 7.5 Proof

The runs below were made on the branch's final game (ab77470's tree; the commit after it changes the docs alone):
the base-against-branch runs on rocky and the Pi, the 30-against-60 runs, synthetic and retail, and the twelve
retail races at 60 were made on it again after the first round, with the same results but for `battle2p`'s first
tick, which 2405546's fix of set_catchup moved (its row). The screenshots, the ghost run and the Pi 5's real-time
runs of 7.7 are of 4a08f2e's game, before 2edc8fe and 2405546; those two change nothing at original.

**The default is the N64's game.** With the setting at `original` every macro gives the N64's value and the
scheduler its 2U, so the game must run as before, to the bit:

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, a build of 0d7d016 against the branch, Debug, rocky (x86-64, GCC 14), every synthetic path (the 25 and `solo`) | the same trace, every run: 26 of 26 |
| the same, Pi 5 (aarch64, GCC 12), under the lock | the same trace, every run: 26 of 26 |
| the same on the retail cartridge, Pi 5: `race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause` (MORECARS.md 10.4's six), `--runs 2` | the same trace, every run, each reaching its state: 6 of 6 |
| the N64 IDO check (`tools/port/ido_check.sh`, rocky, 2405546) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `n64_neutral.py --base 09da2f0` | passes: 1,017 files, 92 code-exact, 925 shifted, none failing |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), `bounds_check`, over the game, rocky | no open findings, base and branch alike: offset_check 59,618 accesses on 3,489 storages (base 59,458 on 3,486: the macros' reads of `pc_fps60`, `pc_frame_scale`, `pc_frame_steps`), layout_check 48,363 (48,203); endian_check's 2 open value conflicts in 1 storage and 3,525 puns the same, check-loader 96 places, 0 wrong; alias_check none open; bounds_check none new, the same counts as the base's (93, 7, 54, 41; 0 two offsets, 0 clamped); `index_check` 0 not reviewed, `weak_copies_check` 0 whose copies differ |
| ctest, rocky: Debug `build/pc`, RelWithDebInfo `build/opt`, at 2405546 | 262 of 262 each (`assets_rom` skipped: no ROM there), 255 before and 7 new: `frontend.fps`, `assets_fps`, `fps_physics`, `hfr_timing_game60`, `hfr_paced_game60`, `audio_realtime_game60`, `synth_boot_solo` |
| Windows cross build (Pi, mingw-w64, a fresh configure, the fetched SDL2) | builds, every test program too (3,678 steps); gen_data's check of the PE: OK, 26 decoupled, 1 reserved in the window. Its first run found the test stand-ins' weak data unresolved on PE (`tests/support.c`, fixed) |

Again on the branch (2026-11-04: PrePlay's load in 30 Hz frames, the slow-host watch, `RUSH_SIM_GFX_MS`; 9c05294, the checks marked 4539a72 on the pacing of 7.6 too), against
86bc6d0 (the merge of the above):

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, 86bc6d0 against the branch, Debug, every synthetic path | rocky: 37 of 37 the same (at 9c05294 and again at 4539a72); Pi 5, under the lock: 37 of 37 (9c05294) |
| the same on the retail cartridge, Pi 5: MORECARS.md 10.4's six, `--runs 2` (9c05294) | the same trace, every run, each reaching its state: 6 of 6 |
| the N64 IDO check (rocky, 4539a72) | 1,011 objects: 755 identical, 256 differ only in debug sections, 0 differ, 0 missing |
| `n64_neutral.py --base 09da2f0` | passes: 1,020 files, 92 code-exact, 928 shifted |
| `offset_check --strict`, `alias_check`, `layout_check`, `endian_check` (and `--check-loader`), `bounds_check`, `index_check`, `weak_copies_check`, rocky | no open findings, base and branch alike: offset_check 59,644 accesses on 3,493 storages (base 59,641: PrePlay's reads of `pc_fps60` and `pc_frame_steps`, game_loop's of `state_word_b`), layout_check 48,389 (48,386); endian_check 2 open value conflicts in 1 storage and 3,525 puns, both; check-loader 96 places, 0 wrong; bounds_check none new (93, 7, 54, 41 both); index_check 0 not reviewed; weak_copies_check 0 whose copies differ |
| ctest, rocky: Debug and RelWithDebInfo, 4539a72 | 302 of 302 each (`assets_rom` skipped: no ROM there); new: `fps_slow_host` (7.7), `realtime_game60_v60` and `fps_even` (7.6); `fps_physics` with `solo` at 180 s |
| Windows cross build (Pi, mingw-w64, a fresh configure, 4539a72) | builds, every test program too (3,452 steps); gen_data's check of the PE: OK, 26 decoupled, 1 reserved in the window |

**The game at 60 against 30: the physics.** `fps_compare.py` runs a press script at original and at 60 on the
virtual clock with `--log-physics 1`, and compares each car's position and velocity, lap and place model tick by
model tick (the model's own clock, which counts the race's retraces from its start), the states in order and the
retraces they come at. ctest's `fps_physics` runs eight paths (`solo`, `stunt`, `battle2p`, `race`, `demo`, `menu`,
`screens`, `raceend`), the physics required to agree on the first three.

Along the 37 synthetic paths (rocky, 9c05294's build, 2026-11-04): 435 state changes, in the same order on every
path; 111 at the same retrace, 293 a retrace earlier at 60 (a frame comes every retrace, so a press or a timer is
seen at the first), and 31 two earlier: the race, whose countdown begins a retrace sooner and whose deadline (in
retraces) is seen at the first retrace past it. Before PrePlay's load counted 30 Hz frames (`PC_LOAD_ON30`, 7.3)
the countdown came 5 retraces sooner and the race 6: 262 at -1, 31 at -5 and 31 at -6, the rest the same.

| Paths | Physics |
|---|---|
| `solo` (one player, the gas and full steering held, 180 s), `stunt`, `battle2p`, `battlequit` | the same to the bit at every model tick both reach: 9,180, 3,990, 3,990 and 392 ticks, with 1, 1, 2 and 2 cars. `solo`'s car is resurrected twice (ticks 217 and 430), at the same tick and place at both rates; with the host slowed (`fps_slow_host`, 7.7) the first comes at another place, which the game thread's frames decide (7.4) |
| the 27 with drones: `race`, `race12`, `drive`, `race2p`, `race2p12`, `race3p`, `race4p`, `circuit`, `circuit12`, `pak`, `pak4p`, `pause`, `pause12`, `raceend`, `raceend12`, `rumble2p`, `mem64`, `membig`, `membig8`, `memhigh`, `memhigh2p`, `detail`, `detail_far`, `detail_fog`, `detail_fogfar`, `detail_high`, `detail_max` | the same up to model tick -59 or -58, in the countdown (151 or 152 ticks): a drone's time boost from DoDrones' place order (7.4) |
| `demo`, `demo12` (the attract demo, drones only) | the same up to tick 213; at 214 a drone resurrects at the checkpoint the game thread last saw it pass, another corner of the loop at 60 (7.4) |
| `menu`, `screens`, `idle`, `cheats` | no race |

On the retail cartridge (Pi 5, `--fast`): the front end's states come at the same retrace at 60 as at original (the
scripts' presses fall on retraces both see), PrePlay 1 earlier and the countdown and the race 5 (on 9c05294's game,
PrePlay's load in 30 Hz frames, all three 1 earlier: `r_solo2`, `r_solo3`):

| Script | States | Physics |
|---|---|---|
| `stunt` (track 14) | 13, the same order | the same to the bit at every one of 3,991 model ticks, 1 car |
| `r_menus`: the main menu's four screens, the player, mode, track and car selects and back | 22, the same order, every one at the same retrace | no race |
| `battle2p` | 13, the same order | the same to tick 0; from tick 1 car 0's velocity differs, by 3 parts in 10,000 at first: its catch-up scale, which set_catchup filters once a frame in a race of two players, 0.015 of the way to its target each retrace at 60 against 0.03 every other retrace at original (7.4) |
| `r_solo`: one player alone, the gas and full steering pressed in the countdown (32 s) and held | 13, the same order | the same to tick -155: the press is at a fixed retrace, and the countdown began 5 retraces sooner at 60 (PrePlay's load), so the gas reaches the car (which revs in the countdown on this cartridge) 5 ticks later. Not a difference of the physics: of the script |
| `r_solo2`: the same, the gas alone, held from the car select's A on, before the cars exist | 13, the same order | the same to the bit at every one of 3,931 model ticks (-210 to 3721: 65 s of driving); then the step to tick 3722 differs. Not the game's: the script holds the gas 66 s from 30.6 s, so it is released at 96.6 s, retrace 5796, and tick 3722 is at retrace 5797 at original and 5792 at 60 (the race began 5 retraces sooner, before PrePlay's fix): at original that step's car has no gas, at 60 it still has. Found by tracing the basic blocks the model thread runs in the step at both rates (a `-fsanitize-coverage=trace-pc` build of the tree, a scratch hook keeping the blocks while gdb sets it, at model_iteration's entry with tick 3721): 7,200 blocks at original and 6,463 at 60, the same up to block 58, the gas in `func_800E6460` (`func_800E681C/group.c:182-185`), where `D_8013FED0[pad] & map[0]` (the pad's buttons, from ControllerUpdate on the model thread) is the gas at 60 and not at original. Ruled out on the way: the shared random seed (a write watchpoint on `D_8011735C` over the step: no draw at either rate), the game thread (the step gives the baton only to the sound system's thread, 7.4), the car record (the 52 words in which the two runs' car records differ at 3721, put into the original run there by gdb, changed nothing). `fps_compare` now names such a change of the script's input: on 9c05294's game (the race 1 retrace sooner at 60) "the same at every one of 3931 model ticks ... up to the script's input change A released at 96.6 s: tick 3722 is at retrace 5797 at original and 5796 at 60" |
| `r_solo3` (2026-11-04): `r_solo2` with the gas held past the race's end, three laps (86bc6d0's game) | 14, the same order; the race's end (the high scores, 0x2000000) 232 retraces sooner at 60: 274.2 s into the race against 277.9 s | the same to the bit at every one of 7,722 model ticks (-210 to 7511: 125 s), through 3 resurrections (ticks 817, 2273, 6421) at the same tick and place at both rates. At 7512 the car, against a wall at about 2 ft/s, is still a tick sooner at 60, and both are put back at the same place, at 60 at tick 7514 and at original at 7515: the stuck detector's abort (7.4), seen at the first retrace past its 6 s at 60 and at the next frame at original. From there the two cars' timelines are a tick apart and part: laps at 95.9, 190.4 and 276.0 s and 8 resurrections at original, 95.9, 187.6 and 272.3 s and 7 at 60. On 9c05294's game the same (7,722 ticks, then the stuck detector's tick), the race now 1 retrace sooner at 60 and its end 228 |
| `rt0_1p`: one player and 11 drones (12 cars), the gas held | 13, the same order | the same to tick -203: the drones' time boost |
| `r_demo`: the attract demo, three times | 12, the same order, at the same retraces | the same to tick 3: the drones |
| `r_finish`: one player holding the gas (no steering: it rides the walls and resurrects), 5 drones, 3 laps, to the results and the game over screen | 15, the same order; the results 984 retraces later at 60 (16.4 s): the player's own car | the same to tick -195, then differs (the drones). The drones' laps keep their pace: the first lap's five in 63.03, 64.97, 66.53, 73.97, 84.33 s at original and 63.25, 65.05, 66.55, 73.90, 84.62 s at 60; the leader's third at 186.10 and 186.75 s. The player's car, bouncing off the walls and put back on the road, finishes its laps at 98.6, 180.2, 274.3 s and 96.2, 188.0, 291.0 s: last of 6 in both |

So nothing but the per-frame logic's own decisions differs. Before the fixes of 7.3 the car select left after 10
frames, and the race began 11 retraces earlier at 60 (15 for the countdown, 16 for the race); now every state up to
PrePlay comes within a retrace of original's, and the two after it 4 retraces more, PrePlay's load counting frames.
Without the fixes, DoDrones' rate limits moved the drones' boost twice as fast in real time.

**The game at 60 against 30: real time.**

- The race clock is the model's time, ticks × 1/60, and the model ticks once a retrace: it keeps the wall clock as
  the retraces do, 60.00 a second in every real-time run here, with the game at 60 as at original
  (`audio_realtime_game60`, `hfr_paced_game60`).
- The front end's timings: every screen change comes within a retrace of original's (the table above): the start
  prompt's 3 s, the attract demo's 30 s and its 5 s wait, the idle overlay's 300 s, the menus' slides and blinks
  (by the frame time, the retrace clock or 7.3's fixes).
- The camera: two frames of one retrace follow a car standing still as one of two does, to rounding; behind a car at
  60 feet a second the lag is 3.436 feet at 60 against 3.000 at 30 (e = 0.6; `assets_fps`).

**Ghosts.** A ghost is the model thread's: `func_800E5D64` (called from model_iteration each model step) records the
car's gear, steering, gas and brake once a model step, a byte each, with the retrace time in the ghost's header
(`frameTime`), and plays them back once a model step into the car's controls; nothing of it is per frame. The model's
steps are the same to the bit at both rates (the races above that the game thread doesn't steer), so a ghost recorded
at 60 is the same data as one recorded at original, and either plays back the same at either rate. On the retail
cartridge (2026-11-04, Pi 5, `--fast`, the scripts in the ignored `build/scratch/ghost/`): a profile is made on a
fresh pak by a press script (the player screen's two DOWNs to CREATE PLAYER, A, A for the letter under the cursor,
START for END, START: player A on the pak, which the player screen then lists, and mode select follows). A ghost race
saves the car's ghost only when the car has finished (GhostEnd, `nonmatching/game/ghost_race_setup.c:117`; the high
scores save `D_80140800`, `billboard_render.c:117`), its three laps (ghost mode's laps are 3 whatever the track
select's LAPS row shows: SetOptions, `large_init_state/group.c`) within the ghost race's time limit, one per track
(`func_800FBED8`). The open-loop scripts tried (the gas alone; the gas and the stick swung every 15 s) drove two laps
of track 0 in that time, with 7 to 11 resurrections; marking the car finished at GhostEnd with gdb saved no ghost
either (the pak changed in 19 to 22 bytes), and the mode screen keeps the mode last chosen, so the replays' DOWNs
reached stunt mode. Not done: a ghost across the rates needs a script that drives three laps in the limit.

**Retail, the game at 60** (Pi 5, `--fast`, the press scripts and logs in the ignored `build/scratch/`; tracks 4 and 5
unlocked by gdb as the track select opens, as MORECARS.md 10.4 did):

| Run | Result |
|---|---|
| each race track, 0-5, one player and 11 drones, then two players and 10 drones (`game.cars = 12`): 60 s with the gas held (12 runs) | each reaches its race with 12 cars on its track; 5,970 of the game's frames in 6,001 retraces; `cars_check`: 12 cars at places 0-11 once each, every car moved; no fault. The pools' high-water marks over the 12: scene nodes 533 of 600, frame matrices 550 of 1,200, display list 4,327 of 8,000 commands, persistent matrices 210 of 277, environment visuals 79 of 100, sprites 128 of 200, frame lookats 24 of 32 |
| the front end, the attract demo and a race to the results at 60 (`r_menus`, `r_demo`, `r_finish` above), and screenshots (`--scale 2`) of each, looked at | the front end through the main menu's four screens and the selects, every screen change at original's retrace; the audio screen with its four rows and values and the cursor on the first, as the N64 draws it; the attract demo three times over 120 s, each 30 s, a drone's camera in a tunnel with PRESS START over it; the race to its third lap and the results (the last lap at 4:40.46 on the race clock as the race ends), then the game over screen; a race of 12 at 10.5 s: the car on the grass with its boost flame and dust, the HUD's 99 mph, 12/12, LAP 1/3, the map and the clock, as the same moment at original shows them |


### 7.6 With a high frame rate

`video.frame_rate` draws its frames between the game's two last (section 2); with the game at 60 the two are a
retrace apart. `gfx_interp.c` takes the period from the frames' retraces (1 to 4, else 2:
`pcgfx_hfr_frame`, `pcgfx_hfr_wants_next`), so at 60 a frame shown between two at 120 is halfway in one retrace; its
cut and jump thresholds are per retrace times the period (`CUT_MOVE_PER_RETRACE`, `JUMP_MOVE_PER_RETRACE`), and the
turn and zoom ones per frame, which a shorter frame meets more easily. With `video.frame_rate = 60` and the game at 60
every tick shows the game's own frame.

| Test (rocky) | Result |
|---|---|
| `hfr_timing_game60`: race2p, drive and demo with the game at 60, `video.frame_rate` original against 120, on the virtual clock | every state, status and result line the same; 1,117, 1,361 and 836 frames drawn between two at 120 |
| `hfr_paced_game60`: the synthetic race in real time under the 59.94 Hz vsync model, the waits precise, at display, 60 and 120 | paced by the vblanks; 36, 77 and 41 of 1,798 vblanks lost to skipped ticks; 674, 603 and 672 frames drawn between (a 59.94 Hz display's vblanks drift through the 60 Hz retraces, so at any rate most fall between two of the game's frames); the game's own frames 1,772, 1,770 and 1,769 in 1,800 retraces; the retraces 60.00 a second, none caught up; the audio's bounds |
| the same race, `video.frame_rate` original, the same vsync swap after each of the game's frames, precise waits (`audio_realtime_game60`) | 1,738 of the game's frames in 1,800 retraces, the retraces 60.00 a second, the audio's bounds |
| the same, the model's 15.6 ms waits (a Windows without the high-resolution timer) | 1,451 of 1,800 at original, 1,084 at display: the waits overshoot by up to a retrace, and the frame's work no longer fits in one; the retraces and the audio held |

So 60 shown with the game at 60 is right: each vblank shows the game's frame for its moment or one between. The
period logic, the thresholds and the pacing needed no change. `original` was right only off vsync, or on a display
whose phase against the retraces is a good one (below).

**The game at 60 under vsync, `video.frame_rate = original`** (2026-11-04). On Windows at 60 the camera looked as if
it moved at a lower rate, through the corners' pans and at other times. Two things were looked for:

- *A phase alias between the model thread and the game thread*, a frame seeing no model tick and the next two:
  none. `--log-frames` logs at each of the game's frames its retraces, the model tick, and the camera's eye and
  view and car 0 as the last frame drew them; `tools/port/fps_even.py` (ctest `fps_even`) checks one model tick a
  frame at 60 and two at original and the steps' evenness. On the virtual clock (rocky, the synthetic `drive`):
  3,781 race frames at 60, each one retrace and one tick, against 1,889 of two at original; the eye's step a tick
  uneven (under a third or over three times its neighbours') in 0.9% of the frames at 60 and 1.7% at original, the
  view's turn in 36 of 1,170 and 26 of 575: the same motion, the same events. On the retail cartridge (Pi 5,
  `--fast`, `r_solo3`'s 100 s, the gas held round track 0's corners): 3,926 race frames at 60, each one retrace and
  one tick, against 1,962 of two; the eye's step uneven in 1 of 3,823 frames at 60 and 3 of 1,905 at original, the
  view's turn (the pans) in 16 of 1,931 and 26 of 915. In real time (rocky, the same drive,
  40 s, after the fix below): 782 race frames of one retrace and one tick off vsync and under a 60.000 Hz vsync,
  776 and 3 of two retraces (and two ticks) under 59.94 Hz. The game thread reads car 0 from the car record it
  makes from the model record once a frame (update_game_data), after the model's step of that retrace: the model
  thread has the higher priority and the baton (port/ultra/sched.c), and a step never lets the game thread run
  inside it (7.4).
- *A camera state stepping once a 30 Hz frame*: the camera spring of the in-car view (`anim_tire_donor.c:224`,
  `spring_save`, which `func_800EB028.c:140` bobs the eye by) was stored with `PC_EACH30`. It is now taken every
  frame, 0.75^f with the noise scaled to keep its spread (`pc_fps_noise`; 7.3). No other `PC_EACH30` or `PC_STEPS`
  is on a camera's path.

The cause was the presentation. With `video.frame_rate = original` each of the game's frames is presented at its
retrace and its swap waits for the vblank, holding the main loop up to a refresh. At original a frame has two
retraces and survives that; at 60 the loop, held past the next retrace, runs the next frame's graphics task too late
for it, and that frame takes two retraces. Under the vsync model (rocky, real time, the synthetic race, the retraces
60.00 a second):

| Display (model) | 86bc6d0: the game's frames a retrace | vblanks showing no new frame |
|---|---|---|
| vsync off | 841 of 841 in the race | - |
| 60.000 Hz, the vblanks 0.02 of a refresh after the retraces, 20 s | 0.732 | |
| the same, 0.1 / 0.3 / 0.5 / 0.7 / 0.9 | 0.911 / 0.977 / 0.977 / 0.976 / 0.976 | |
| 60.000 Hz at the phase a run started at (the drive, 40 s) | 420 of 839 in the race: 30 a second | 1,210 of 2,397 |
| 59.94 Hz (its phase passes through every value in 17 s) | 804 of 841 | 86 of 2,395 |
| 144 Hz | 841 of 841 (each frame shown for 2 or 3 vblanks) | |

So on a 60 Hz display at a phase where the vblank comes just after the retrace (the swap misses it and waits a whole
refresh) the game ran at 30 for good, and on a 59.94 Hz one for a few seconds every 17 s: a camera moving at a lower
rate, plainly seen in a pan. Now at 60 with a vsync swap on a display under 100 Hz the loop paces the game's own
frames by the vblanks, as `video.frame_rate = 60` does but with none drawn between (`pc_gfx_set_own_paced`,
port/gfx/gfx_api.c; the loop's `PcRunOptions.game_rate`): each tick shows the game's newest frame, as late as it
makes its vblank, never across a retrace, and on a display that gives its timing (Windows' compositor, X11's XRandR)
the retraces move once to sit between two vblanks (`vb_lock`). At 100 Hz and up a swap waits at most 10 ms and
nothing changes. After (the same runs):

| Display (model) | the game's frames a retrace | vblanks showing no new frame |
|---|---|---|
| 60.000 Hz at the run's phase (the drive, 40 s) | 841 of 841 in the race | 29 of 2,400 in the whole run (40 s, the front end too) |
| 60.000 Hz, 0.02 after the retraces, the compositor's timing (ctest `realtime_game60_v60`, 30 s) | 0.97 (0.736 on 86bc6d0) | 1% (26.4%) |
| the same, the timing given as a rate (XRandR) or the vblanks' times, 0.02 / 0.05 / 0.1 | 0.977 | 27-38 of 1,200, the retraces moved once |
| the same with no timing (Wayland, headless), 0.05 | 0.96 | 506 of 1,200: without the display's period the retraces don't move |
| 59.94 Hz | 839 of 841 | 85 of 2,397 |

On a host that can't keep 60 the frames are uneven another way: the model thread's step of a retrace waits for the
game thread to give up the baton (the PC's threads never take it from one computing, where the N64's retrace
interrupt preempts it), so a frame of two retraces sometimes sees one model tick and the next frame the other. On the
Pi 5 in real time (retail, the solo race at 60 under the 60.000 Hz vsync model, the compositor's timing, 70 s): 1,062
race frames of one retrace and one tick, 438 of two and two, but 67 of two retraces with one tick and 15 longer ones
short of a tick, 82 of 1,582; the race kept 0.75 of its frames (0.64 on 86bc6d0) and the line of 7.7 said so. There
`original` is evener.

What the player should use: `game.frame_rate = 60` with vsync on now shows the game's 60 frames evenly on a 60 Hz
display at any `video.frame_rate`. On a display at another rate the game's 60 frames can't land evenly on its
vblanks: on 144 Hz each is shown for 2 or 3 of them (a judder in a pan that is the display's, not the game's), so
there `video.frame_rate = display` (frames drawn between the game's, one a vblank) is the smooth one, as are 120 Hz
(2 vblanks each) and 60 Hz. A host that can't draw a frame a retrace is told so (7.7).

### 7.7 What it costs

At 60 the game thread runs its frame's logic and builds its display lists 60 times a second, and the renderer draws
each: twice the work of original a second, the model thread's the same. On rocky (the synthetic race in real time)
the game keeps a frame a retrace (7.6). On the Pi 5 with the retail cartridge, in real time, headless, 90 s from the
boot, one run at a time under the lock (the host is shared, so its numbers vary run to run), 12 cars (`game.cars = 12`,
the track select's Drones raised), the gas held; the game's frames per retrace in each state, the retraces, the audio
(the null output, drained in real time: this build has no SDL2) and the pools' high-water marks:

| Run | Front end | Countdown | Race | Retraces in 90 s | Audio | Pools (most used) |
|---|---|---|---|---|---|---|
| one player, 11 drones, at 60 | 0.98-1.00 | 0.66 | 0.79 (47 frames a second) | 5,400; 36 caught up, none dropped | 123 underruns, 486 ms of silence | scene nodes 445 of 600, display list 1,979 of 8,000 |
| the same at original | 0.50 | 0.50 | 0.50 | 5,391; 8 caught up, 9 dropped | 97 underruns, 417 ms | the same within 20 |
| four players, 8 drones, at 60 (two runs) | 0.99-1.00 | 0.57, 0.57 | 0.58, 0.62 (35, 37 frames a second) | 5,400; 152, 193 caught up, none dropped | 580, 525 underruns; 2,974, 2,829 ms | scene nodes 212, frame matrices 448 of 1,200, display list 2,857 |
| the same at original (two runs) | 0.50 | 0.49, 0.46 | 0.50, 0.50 | 5,400; 54, 122 caught up, none dropped | 420, 584 underruns; 2,576, 3,281 ms | the same within 30 |

So the Pi 5 keeps 60 in the front end, but not in a race of 12 cars: one player gets 47 frames a second and four
players 35 to 37, the rest of the frames two retraces long. The game keeps its speed either way (a frame of two
retraces at 60 is the N64's frame: its scale 1, its step 1), and the retraces keep their 60 a second. The audio's
underruns, which original already has on this shared host, vary from run to run as much as between the rates (four
players: 580 and 525 at 60, 420 and 584 at original). A slower host is better left at `original` there, or at
`video.frame_rate = 60`, whose frames between cost about half a graphics task (section 5).
The pools hold at either rate: they are made again each frame, and a frame of one retrace fills them no more than
one of two (the 12 races of 7.5: at most 533 scene nodes of 600, 550 frame matrices of 1,200, 4,327 commands of
8,000, 24 frame lookats of 32; no pool ran past its end).

**Telling the player** (2026-11-04). In a race at 60 the game thread counts its frames and the retraces they take
(`pc_game_frame_note`, `port/config/game_fps.c`), the race told by game_loop's state change (`pc_game_fps_state`,
given the state it changes to, `state_word_b`: in that branch `state_word_a` is still the one it leaves, 0x100000
for the race, which the countdown's end sets and leaves within the frame). When over 4 s (240 retraces) of the race
it keeps fewer than 0.9 frames a retrace, one line says so in the log, once a race:

    rush2049pc: game.frame_rate = 60: the game drew 0.79 of its frames in 4 s of the race (47 a second): this host
    can't keep 60 here. The game keeps its speed: a frame it can't draw in a retrace takes two, as the N64's 30 Hz
    frames do; game.frame_rate = original keeps an even 30

(one line in the log), and F1 > About's Game frame rate row gives the last race's share: `60; the last race at 60
kept 0.79 of its frames (47 a second): this host can't keep 60 there` (`pc_game_fps_describe`; `original (30 a
second)` before any race at 60). There is no automatic fallback: the frames such a host takes two retraces for are
the N64's frames already (their scale 1, their step 1: 7.2) and the ones it draws in one are frames original
doesn't have, so falling back would show 30 a second where the host shows 47, only more evenly; which the player
prefers is theirs to choose, and the line names the setting. Tested on the virtual clock by `fps_slow_host`
(`tools/port/fps_slow.py`, rocky): with `RUSH_SIM_GFX_MS=20` (each graphics task's DP event held 20 ms of the N64
clock, `port/ultra/rcp.c`) every frame at 60 takes two retraces; along the synthetic solo race (50 s) the line comes
once, at 0.50 (30 a second); unslowed at 60 the race keeps 1.00 of its frames and nothing is said; at original
slowed nothing is said; and the game at 60 on the slowed host is original's on it: the car's physics the same at
every one of 1,379 model ticks, the race starting a retrace later. On the Pi 5 in real time (the retail cartridge,
one player and 11 drones, 90 s from the boot, as in the table above; 9c05294's game, under the lock): at 60 the race
kept 0.85 of its frames (2,796 in 3,286 retraces), and the line came once, for its first 4 s (0.69, 42 a second); at
original (0.50) nothing was said.

### 7.8 What's left

- **Physics with drones, resurrections or shots** is not the same to the bit at 60 as at original, and can't be: the
  game thread decides them once a frame, from places, checkpoints, the stuck detector's timer and random numbers it
  shares with the model thread (7.4). The drones' catch-up keeps its rate (7.3); in the one retail race with drones
  run to its finish at both rates the drones' laps came within 0.3 s, and in the one alone the first three
  resurrections came at the same tick (7.5), not measured over many races. A resurrection's moment could be made the
  N64's only by deciding at 60 on the retraces original's frames fall on, which depend on where the race began
  against the frames: not done.
- **The model thread's random numbers** share the game thread's seed (7.4). Nothing they give reaches a race's
  physics but a battle resurrection's offset, so the seed is left shared; a battle at 60 already differs by its
  shots.
- **First-order steps.** A step by the frame time toward a target, `pos += dt·k·(target − pos)` (the track select's
  carousel, some cursors), closes a little more of the way per 30 Hz frame in two steps: 0.9025 against 0.9 for the
  carousel. Not changed.
- **The coverage.** The audit read the lines that step state in every function the 106 runs ran per frame, with the
  rest of their files' per-frame functions, and every use of the frame time. Per-frame code that no run reached, in
  a file with no such line (parts of the obstacle mode, the records' screens past what the scripts visit, the
  credits), was not read line by line.
- **A real display** at 60 with vsync: checked only headless, under the vsync model (59.94 Hz: section 2's). Under
  the model the game at 60 kept 0.97 to 0.98 of its frames with precise waits, and 0.81 (original) or 0.60 (display)
  with the 15.6 ms waits of a Windows without the high-resolution timer: there a fifth to two fifths of its frames
  take two retraces.
- **Windows** with the retail cartridge at 60: the cross build builds; not run.
- **The Pi 5** doesn't keep 60 in a race of 12 cars (47 frames a second with one player, 35 with four: 7.7). Since
  2026-11-04 the host is told so (7.7); the setting doesn't fall back on its own. On such a host a frame of two
  retraces can see one model tick and the next the other (7.6): the model's step waits for the game thread to give
  up the baton. Letting the retrace preempt a computing game thread, as the N64's interrupt does, would even it out;
  the scheduler doesn't, for a run that takes the same course every time.
- **The display**: the vsync pacing of 7.6 was measured under the model (rocky, the Pi), not on a Windows desktop's
  60 Hz and 144 Hz displays themselves.
- **Ghosts** on the retail cartridge: a profile is made by a press script, but no ghost was saved: the car must finish
  its three laps within the ghost race's limit, which the open-loop scripts didn't (7.5). A script that drives a
  clean lap (or a track whose limit gas alone meets) is the next step; then record at one rate, replay at the other
  and compare the ghost car's positions (`build/scratch/ghost/ghostcmp.py`).

### 7.9 auto (video.frame_rate = auto)

Written 2026-11-04. `video.frame_rate = auto` picks the presentation from the display (`pc_gfx_auto_rate`,
port/gfx/gfx_api.c), asked at each pass of the main loop with the game's latched rate, vsync and the display's whole
hertz (port/ultra/loop.c, next to `pc_gfx_set_own_paced`); the display's refresh is taken again at each event pump
while auto is set, so a window moved to another monitor follows it. The log says the choice at the first pass and
at each change (`gfx: video.frame_rate auto: 144 Hz, the game at 60: frames between`), and F1 > Video's Frame rate
row shows it (`auto: 144 Hz, frames between`). The default stays `original`.

| Display (vsync on) | auto shows |
|---|---|
| vsync off, or the virtual clock | `original` |
| 59-61 Hz, 119-121, 179-181, 239-241 (a whole multiple of 60) | `original`: each of the game's frames held a whole number of vblanks (at 60 Hz and the game at 60, paced by the vblanks: 7.6) |
| any other (75, 90, 144, 165 Hz) | `display`: a frame each vblank, drawn between the game's |

**Measured** (`tools/port/vsync_even.py`, rocky, the synthetic race in real time under the vsync model, precise
waits, the platform giving the rate; counted over the race's last 10 s of a 20 s run). The model now says, besides
the vblanks with no new picture, how long each new picture was held and the spread of that, and how far the shown
moment (`pc_gfx_shown_moment`: a frame's retrace, or the moment a frame between was drawn for) steps off the vblank's
period, rms: the step a camera moving at a steady speed takes on screen, 0 when every vblank shows its own moment.
Four runs at a time, rocky's load average 7-12 (other agents' builds):

| Display | Game | video | No new picture | Held ms (spread) | Step off, rms ms | Frames between drawn |
|---|---|---|---|---|---|---|
| 60 | 30 | original | 0.500 | 33.33 (0.00) | 16.67 | 0 |
| 60 | 30 | display | 0.413 | 28.38 (8.22) | 15.61 | 77 |
| 60 | 60 | original | 0.037 | 17.30 (3.19) | 4.71 | 0 |
| 60 | 60 | display | 0.210 | 21.09 (7.44) | 10.84 | 74 |
| 75 | 30 | original / display | 0.606 / 0.602 | 33.85 (17.71) / 33.27 (16.33) | 16.97 / 17.69 | 0 / 46 |
| 75 | 60 | original / display | 0.491 / 0.459 | 26.11 (16.03) / 24.63 (14.60) | 15.46 / 14.30 | 0 / 28 |
| 120 | 30 | original / display | 0.763 / 0.829 | 35.23 (33.22) / 48.51 (39.35) | 15.54 / 20.03 | 0 / 0 |
| 120 | 60 | original / display | 0.716 / 0.804 | 29.39 (28.89) / 42.16 (36.45) | 15.11 / 18.24 | 0 / 0 |
| 144 | 30 | original / display | 0.792 / 0.778 | 33.31 (4.13) / 31.23 (8.45) | 13.54 / 13.43 | 0 / 77 |
| 144 | 60 | original / display | 0.583 / 0.611 | 16.66 (4.18) / 17.84 (6.11) | 8.22 / 9.20 | 0 / 36 |
| 165 | 30 | original / display | 0.818 / 0.773 | 33.32 (3.03) / 26.68 (10.02) | 12.86 / 12.05 | 0 / 103 |
| 165 | 60 | original / display | 0.636 / 0.620 | 16.67 (2.87) / 15.93 (4.90) | 8.02 / 8.02 | 0 / 107 |
| 240 | 30 | original / display | 0.875 / 0.839 | 33.33 (0.34) / 25.92 (8.43) | 11.02 / 10.11 | 0 / 174 |
| 240 | 60 | original / display | 0.750 / 0.751 | 16.67 (0.24) / 16.70 (2.25) | 7.22 / 7.27 | 0 / 169 |

Repeated one at a time (rocky, load 8.6-8.8, synth_compare's runs beside it), the game at 60:

| Display | video | No new picture | Held ms (spread) | Step off, rms ms | Frames between drawn |
|---|---|---|---|---|---|
| 144 | original / display / auto (then display) | 0.602 / 0.679 / 0.629 | 17.43 (7.52) / 21.66 (10.51) / 18.74 (7.91) | 8.80 / 11.02 / 9.73 | 0 / 1 / 15 |
| 120 | original / display | 0.500 / 0.686 | 16.67 (0.00) / 26.55 (14.02) | 8.33 / 13.99 | 0 / 16 |

What the numbers say:

- At 120 Hz the game at 60 under `original` holds each frame exactly two vblanks (16.67 ms, spread 0.00 in the serial
  run; 8.33 ms rms is the least an even hold of two can step); hence auto keeps `original` at whole multiples of 60.
  The first 120 Hz rows (four at a time) were far less even for every setting (held 29-48 ms, spread 29-39): a
  loaded host there, not repeated in the serial run.
- On this host `display` drew almost no frames between at any refresh (0-174 in 10 s of race against 600-1440
  vblanks; rocky draws 570-670 in 30 s at 59.94 Hz alone: section 5): the GPU was shared with the other runs and
  agents, and a frame between is drawn only on the GPU's spare time (section 2). So at 144 and 165 Hz `display`
  measured no smoother than `original` here (8.02-9.20 against 8.02-8.80 ms rms), and a little worse at 144. It is
  the right choice only where the frames between are drawn: auto picks it at 75, 144 and 165 Hz on the reasoning of
  7.6 (each of the game's 60 frames otherwise held 2 or 3 vblanks, spread 2.9-4.2 ms above), not on these numbers.
  That is the open question of this section: measured on an idle host, or the user's Windows GPU, `display` should
  show its frames between at every vblank.


Tests: `gfx.interp`'s parse case (`port/gfx/tests/interp_test.c`: `auto` read, and auto's choice at 59-61, 75, 90,
119-121, 144, 165 and 240 Hz, vsync off, no display; set and resolved at 144 and 60 Hz); `realtime_auto_even`
(`vsync_even.py --hz 60,120 --game 60 --rate auto`: the step off the vblank at most 7 and 14 ms rms: at 120 Hz 8.33 alone, 11.28 in a Debug build beside synth_compare's runs, held 20.13 ms with a spread of 12.70, so the even hold of two vblanks is not kept on a loaded host; real time,
`LABELS realtime`, RUN_SERIAL, skipped with the other real-time tests under the sanitizers and on arm). The defaults:
`synth_compare.py --same --runs 2 --shots` of d0b8b5d against this branch on rocky.
