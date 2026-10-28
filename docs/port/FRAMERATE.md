# A high frame rate (video.frame_rate)

Phase 8 of [PORT_PLAN.md](../PORT_PLAN.md), written 2026-10-27. The game draws at most 30 frames a second; with
`video.frame_rate` the port shows 60, 120, the display's refresh or any rate from 30 to 1000, while the game's timing
stays the N64's. The frames between the game's are drawn by the renderer from the game's last two
([GFX.md](GFX.md) "High frame rate"); no game C changed. This file says how the game paces itself, why that leads to
interpolation in the renderer, what the setting does, how it is tested and what it costs.

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
  float operations. And whole retraces cap it at 60: no step is smaller than a retrace.
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
- **As late as it makes its vblank.** A tick starts when its drawing (the running mean of its cost, without the
  swap's wait) ends 1 ms before the vblank, plus how late the loop's timed waits return (a decaying maximum). Its
  swap then waits about a millisecond, if at all. A frame between the shown one and the game's next, which isn't
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
| `rush2049pc.ini` | `[video] frame_rate = original` (default), `display`, or a number from 30 to 1000 (60, 120, 144) |
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
| `hfr_paced` (`tools/port/realtime_check.py --pacing display,60,120`) | the synthetic race in real time under a 59.94 Hz vsync swap (`RUSH_SIM_VSYNC_HZ`), the loop's waits precise: paced by the vblanks, at most 10% of them lost to skipped ticks (1.7-5.6% on rocky), at least 250 frames drawn between in 30 s, the retraces at 59 a second or more and the audio's bounds (`audio_realtime`'s) |
| `hfr_paced_queue` (`--vsync-queue never --display-hz 59 --display-timing rate,vblank --max-doubled 5`) | the same at `display` with a driver whose swap never blocks (`RUSH_SIM_VSYNC_QUEUE=never`) and SDL saying 59, once with the platform giving the exact rate (as XRandR) and once its vblanks too (as Windows' compositor): as `hfr_paced`, and at most 5 vblanks in 30 s showing a frame twice with nothing decided for them (1 and 1 on rocky; at SDL's 59, about 22) |
| `hfr_paced_60` (`--vsync-hz 60 --vsync-phase 0.05 --display-timing vblank`) | an exact 60.000 Hz display, its vblanks 0.05 of a refresh after the retraces (the worst phase): as `hfr_paced` (2 ticks lost, 666 drawn between on rocky; 1114 of 1200 lost before the retraces moved) |
| `gfx.display.*` (`port/gfx/tests/display_test.c`) | the refresh arithmetic on made-up values: XRandR modes (1080p60, 59.94, 1440p "144", interlaced, doublescan), Windows' ratios, a refresh SDL's whole hertz could be the rounding of, the compositor's counter as an age, and which period and vblanks are taken from a monitor's, a compositor's (the same monitor, another one, stale, none) and SDL's |
| `hfr_realtime` (`realtime_check.py --frame-rate 120 --present-ms 40`) | the same race with each swap held 40 ms, then until the vsync (a host always behind): at least 5 frames shown a second, the game's own |
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
600-666 in 30 s. At 60.000 Hz the blocking model shows 16-17 frames held twice in 30 s at any phase, moved or not,
all after the first 10 s: a tick that waits for the game's next frame and then overruns its vblank (section 6).

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
- With a swap that blocks at an exact 60.000 Hz, 16-17 vblanks in 30 s show a frame twice with no decision for them
  (at any phase): a tick waiting for the game's next frame (`pc_gfx_tick_waits`) starts as late as its mean cost
  allows and overruns its vblank when its drawing takes longer. At 59.94 the same race shows 0-1. A margin from the
  cost's spread, not its mean, would cover it.
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
