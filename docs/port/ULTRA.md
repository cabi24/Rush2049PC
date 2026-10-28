# The PC libultra: threads, devices, memory and the main loop (port/ultra)

Phase 2 of [PORT_PLAN.md](../PORT_PLAN.md). Written 2026-10-26. No ROM was present: everything below was checked
with `rush2049pc --selftest` and with the game's own boot running on a blank cartridge (`--no-rom`).

`port/ultra` replaces libultra's dispatch and device layer, loads and checks the ROM, maps the RDRAM window, and
runs the host main loop that stands in for the RCP and the interrupts. `port/input` feeds the pads. `port/main.c`
owns `main()`. About 3,300 lines of C11.

| File | What |
|---|---|
| `sched.c` | N64 threads on host threads (the baton), message queues, events, busy-wait and idle yields |
| `timer.c` | `osGetTime`/`osGetCount` at 46.875 MHz, `osSetTimer`/`osStopTimer` |
| `vi.c` | the video interface: mode, blackout, framebuffer swaps, the retrace event |
| `rcp.c` | RSP tasks (graphics on the main thread, audio at once in real time), SP/DP done, the AI |
| `pi.c` | ROM loading and SHA-1, PI DMA, `osPiReadIo`, inflate (zlib) and the boot LZSS |
| `si.c` | the PIF: controller status and buttons, the controller pak (`.mpk`), the rumble pak |
| `memory.c` | the RDRAM window, `pc_n64_ptr`/`pc_n64_addr`, big-endian ranges, the absolute-symbol fix-up |
| `misc.c` | `osInitialize` and the low-memory globals, cache/TLB/interrupt no-ops, address conversion, faults, stubs |
| `loop.c` | the host main loop |
| `defaults.c` | weak no-op versions of the other parts' interfaces |
| `gsm.c` | the boot segment's GSM 06.10 speech kernels (RPE decoding, long-term and short-term synthesis), from `kernels.s` |
| `selftest.c` | `--selftest` |

## 1. Threads

**One N64 thread runs at a time.** Each `OSThread` the game creates gets a host thread (2 MiB stack). They pass a
baton: the holder runs game code; the others wait on their own condition variable. The baton moves only inside
libultra calls, with libultra's rules:

- The highest-priority runnable thread runs. Equal priorities run in the order they became runnable.
- A call that wakes a strictly higher-priority thread hands it the baton at once (`osSendMesg`, `osRecvMesg`,
  `osStartThread`, `osSetThreadPri`), as libultra's `__osEnqueueAndYield` does.
- Blocking `osRecvMesg`/`osSendMesg`/`osJamMesg`, `osYieldThread` (back of its priority level), `osStopThread`
  (self or another), `osDestroyThread`, and a returned entry function give the baton up.
- The boot context (`n64_main`) runs on its own thread; its first `osStartThread` never returns, as on the N64.

**Events come from the host main thread** (`pc_os_post`): VI retraces, timers, SP/DP/AI/SI completions. They make
threads runnable and hand the baton over when nobody holds it or when the holder waits for events. They never take
it from a thread that is computing, which an N64 interrupt would. That is the one deliberate difference from the
hardware. A woken high-priority thread therefore runs at the holder's next libultra call. The game blocks often,
but its busy-waits must yield (section 6).

**An event never finds its queue full and is lost** (`pc_os_event`), except a retrace, which is dropped as the
N64's VI interrupt would drop it. Others are held and go in as the receiver empties the queue. The scheduler's queue
(8 messages) fills when a thread computes long enough without a libultra call; an audio task it starts then posts its
SP event from the scheduler's own thread (section 3), and until 2026-10-27 that event could be lost. The scheduler
then kept the ended task as its current RSP task, and `dma_wait_complete`, which runs the audio callback after each
retrace once that task is done, waited for ever: the game hung at the start prompt or the main menu, now and then in
real-time runs on the Pi 5, more often when the main loop sends late retraces in a burst. The log names the first
event of each kind held (`event N held`).

**Side tables, not the game's structs.** The game's `OSThread`, `OSMesgQueue` and `OSTimer` are N64-sized in the BSS
arenas (port/data), and the native structs are larger, so nothing is written into them. Threads, queues and timers
are kept in tables keyed by the game's object address. Message buffers are host-side too: the game's `OSMesg`
arrays have 4-byte slots. A queue used before `osCreateMesgQueue` acts as a zero-capacity queue, as a zeroed one
does, and is logged.

**Idle and busy-waits.** `pc_os_idle` (the game's idle loop, `lib_2cf0.c`) waits until any thread can run.
`pc_os_spin_yield(site)` hands the baton to a runnable thread of equal or higher priority. Otherwise it waits up to
1 ms for an event. After 200 ms with nothing to run at its priority or above, it lets a lower-priority thread run
once and logs the site. That escape exists for writers the N64 would have run from an interrupt; the self-test
exercises both paths.

**The virtual clock (`--fast`).** By default the N64 clock is the host's. With `--fast` it is virtual
(`timer.c`): it stands still while a thread computes, and when the system is idle the main thread moves it straight
to its next deadline (a retrace, a timer, the AI) instead of sleeping. Idle means no thread holds the baton, or the
holder waits for events and nothing that would take the baton over is runnable (a lower-priority thread doesn't
count: it runs only through the busy-wait escape). Each change to idle wakes the main thread (`idle_changed`). A
busy-wait's 1 ms nap is 1 ms of the virtual clock, so its 200 ms escape keeps its length. Timers, `osGetTime`, the
AI, scripted presses, `--seconds` and the null audio output's drain all run on the virtual clock; the renderer and
the RSP tasks take no N64 time. A run is then as fast as the host allows (the synthetic race's 28 s in about
0.4 s) and the same every run, on any host. The main loop's work (RSP tasks, timers, the AI, retraces) happens only
while every N64 thread waits: a thread that submits a task or wakes the main thread goes on until it waits, and
only then does the main loop run (`pc_os_wait_main` returns 0 meanwhile, after 100 ms of real time, for the hang
check). Before 2026-10-27 the main loop ran a task as soon as it was submitted, while the thread that submitted it
computed on, and which of that thread's next libultra calls saw the task's events depended on the host: two runs
of one script could start a race two retraces apart ([FRAMERATE.md](FRAMERATE.md) 2). A run whose clock stops for 30 s of real time (a thread
computing without a libultra call) is stopped as hung, with the threads' states. The self-test runs on it too
(`--selftest --fast`, CTest `ultra_selftest_fast`).

So two runs, two builds (Debug and RelWithDebInfo) or two architectures give the same states at the same retraces
and the first car at the same place, to the bit: `tools/port/synth_compare.py` checks it ([OPTIMIZED.md](OPTIMIZED.md) 5).

`--trace-os` logs every switch and blocking call. Ctrl-\ (SIGQUIT) prints every thread's state and what it waits on.
With `RUSH_TRAP_FP` set (glibc), every host thread traps the first invalid floating-point operation or division by
zero (SIGFPE, reported with a backtrace like any fault): where a NaN or an infinity starts.

## 2. Memory

**The RDRAM window is load-bearing.** The game addresses RDRAM outside its symbols with 32-bit arithmetic. It does
this in the heap (`align32(0x8017A640)` to `osMemSize | K0BASE`, where every loaded file is relocated in place,
[ASSETS.md](ASSETS.md)), the low-memory globals at 0x80000300 and the runtime images' region at 0x8038A400. The
game's pointer-free data and BSS are in the window too, at their N64 addresses (port/data's arenas, [DATA.md](DATA.md)
§3: boot 0x800277D0–0x80086A50, game 0x8010FD7C–0x8017A640, the runtime images' inside their heap block), so a
32-bit N64 address of game data is a host address. An 8 MiB memfd is mapped at host 0x80000000 (KSEG0) and again
at 0xA0000000 (KSEG1), with `MAP_FIXED_NOREPLACE`, before the data is filled. On Linux the mapping is required and
the program stops if it fails, and on Windows too, where a file mapping is viewed at both addresses
([WINDOWS.md](WINDOWS.md)). Elsewhere it falls back to a translated window (`pc_n64_ptr`), the game's literal
addresses fault, and `pc_data_init` refuses to fill data that isn't mapped.

**Host addresses are above 4 GiB**, except the window's. The executable is PIE, and its heap and the N64 threads'
stacks are mapped high. So the renderer tells host pointers (≥ 4 GiB) from N64 addresses (K0/K1, physical,
segmented), and game data in the window is simply RDRAM to it. `osVirtualToPhysical` maps window addresses to
physical; it truncates a host address, with one warning, since the game keeps the result in a u32.

**Data at absolute N64 addresses.** Some names the game uses are addresses, not storage: the low-memory globals
(`osTvType`, `D_80000300`, `D_80000318` for osMemSize), the heap start `D_8017A640`, the images' base
`D_8038A400`, `gDecompressedDataStart`, and the ROM offset `D_00039B40`. gen_stubs makes them absolute symbols
(`aliases.ld`): window addresses, and ROM offsets as cartridge addresses (`0xB0000000 | offset`, which the game's PI
and MusyX address callbacks accept). port/data's arena names are absolute symbols too (`data.c`, about 1,800). The
game reaches all of them through the GOT. binutils 2.40 on aarch64 gives the GOT slot of an absolute symbol in a
PIE a RELATIVE relocation, so at run time the slot holds load address + value. `pc_mem_fix_absolute`, called in
`main` before anything else runs, puts the values back (1,802 slots). It only touches RELATIVE relocations whose
addend is at 0x80000000 or above, where nothing the linker places can be (`gen_data.py --check` verifies that),
and whose slot holds exactly base + addend. Initialized pointers to them are resolved at link time.

**osMemSize is 8 MiB** (the Expansion Pak) by default, as N64ModernRuntime and v5 report: the game sets its
Expansion Pak flag (`D_80156994`) and loads overlay B and the extra waves in races, which give it its sixth race
track, the tracks' animated objects and the front end's song (docs/port/QA.md). `game.expansion_pak = off` reports
4 MiB, a console without one, where the game skips them. The window is 8 MiB either way.

**Big-endian ranges** (`pc_mem_register_be`): sorted, disjoint. A range that overlaps existing ones merges with
them, so a DMA into a registered buffer keeps it one range. Adjacent ranges stay apart. `pc_mem_unregister_be(p)`
drops the range that contains p. PI DMA, inflate and LZSS register their destinations. The MusyX driver
registers its two DMA caches when it allocates them and drops them when it frees them.

## 3. Devices

**VI.** No manager thread. The main loop runs a 60 Hz retrace (NTSC). It sends the `osViSetEvent` message every
`retraceCount` retraces. A framebuffer passed to `osViSwapBuffer` becomes current at the next retrace and is shown
(`pc_gfx_present`), unless the VI is blacked out. Mode, special features and scale are recorded.

**RSP and RDP.** `osSpTaskStartGo` copies a graphics task for the main thread, which owns the GL context:
- `M_GFXTASK` goes to `pc_gfx_run_dl(t.data_ptr)`, then the SP and DP events.
- `M_AUDTASK`, in real time, runs at once on the thread that starts it (the game's scheduler):
  `pc_audio_run_task(t.data_ptr, t.data_size)`, then SP, which the scheduler finds in its queue after
  `osSpTaskStartGo` returns. `data_size` is the SFD count, passed through ([AUDIO.md](AUDIO.md)). On the main thread,
  behind graphics tasks and vsync swaps, the tasks ran late enough that the AI pushed ring buffers they hadn't
  rendered yet (AUDIO.md §7). With `--fast` the main loop runs them, with the graphics tasks, while every N64 thread
  waits, as it sends the AI events. Run at once there, the task count in the status lines raced the main loop (two
  runs of a path differed by one, `hfr_timing`).
- Any other task gets only the SP event.

Tasks run to completion, so `osSpTaskYielded` is 0. `osDpSetNextBuffer` gets a DP event. `--stop-at-gfx` reports the
first graphics task and stops: the Phase 2 goal.

**AI.**
- `osAiSetFrequency` rounds as libultra does and returns the real rate (22047 for 22050). It tells port/audio
  the same rate.
- `osAiSetNextBuffer` pushes the buffer to port/audio's queue.
- AI events go out while the queue and the events not yet answered come to less than port/audio's target:
  384 frames (two of the game's 192-frame buffers) with the null output, two device pulls, a retrace and a buffer
  with a device, more after underruns (AUDIO.md §7). Up to 3 are outstanding (the game's queue holds 4); an
  event not answered in 50 ms counts as dropped. While pushes leave the queue empty (no output), events are half
  a buffer apart, so the audio thread can't run flat out.
- The main loop checks each pass. In real time the device's thread checks too, after each pull (port/audio's
  pull hook), so a main thread blocked in a swap doesn't starve the device; with `--fast` only the main loop sends
  them. While the game is held (`hold()`), port/audio's target is 0.
- `osAiGetLength` reports the queued frames × 4.

**PI and the ROM.**
- The ROM comes from `--rom` (default `baserom.us.z64`). It is byte-swapped from .v64 or .n64 and checked against
  the US SHA-1; `--no-rom-check` runs another dump anyway.
- `osPiStartDma` has no manager thread: it copies at once, registers the destination as big-endian and sends the
  completion. The game passes ROM offsets as `devAddr` (libultra adds `osRomBase`); cartridge bus addresses
  (0x10000000 and up, or KSEG1 0xB0000000) are accepted too.
- `osPiReadIo` reads big-endian words: the game's main reads 16 at 0xFFB000, past the ROM, so they are 0.

**Decompressors** (hand-written assembly on the N64):
- `inflate_entry(src, dst, n)`:
  - the game image (ROM 0xB0CB10, `gRomCompressedData`) does nothing: it is linked in and port/data filled its data;
  - the runtime images (0xB5C534, 0xB6FEC4) become `pc_data_overlay_load` (port/data's overlay loaders call that
    themselves under `TARGET_PC`);
  - any other stream is inflated with zlib from the ROM.
- `inflate_entry_alt(src, size, dst)` inflates from memory (ghosts).
- `lzss_decode(src, dst)` is the boot LZSS: flag bytes LSB first; a copy is two bytes, length `(b0 & 15) + 2`,
  distance `(b0 & 0xF0) << 4 | b1`; distance and length nibble 0 end the stream.

**SI.** The game's own libultra C runs unchanged: `osContInit`, `osContStartReadData`, `__osContRamRead`/`Write`,
the whole `osPfs*` file system and `osMotorInit`. `__osSiRawStartDma` writes fill a 64-byte PIF RAM. A read runs
its commands, as the PIF does, so re-reading gives fresh buttons. The commands are 0x00/0xFF status, 0x01 buttons,
and 0x02/0x03 accessory read/write of 32-byte blocks with the address CRC and a data CRC from the game's own
`__osContDataCrc`. Each DMA sends the SI event. The block layout is the hardware's, since the game fills it through
u8 fields. The one u16 field, the buttons, is read natively, so it is written in host order. There is no EEPROM:
the game saves only to the controller pak.

**Controller pak.**
- `--pak PATH`: default `rush2049pc.mpk`, created formatted when missing and written within a second of each change
  (`pakfile.c`'s `pc_file_write_atomic`: a temp file flushed to the disk, then renamed over it; a failed write is
  tried again at the next flush). Only a missing file is new: one that can't be read, or is damaged
  (`pc_pak_image_check`: not 32 KiB, no good ID block copy, no good inode table), plugs in no pak and is never
  written over. port/app's saves.c sets such a file aside before the game starts, and keeps three backups
  ([RELEASE.md](RELEASE.md) "Saves").
- `--no-pak` plugs in nothing, and `--rumble` a rumble pak (0x8000 probe, 0xC000 motor).
- The image keeps the `.mpk` page layout, but its multi-byte fields are in the native Pfs C's order: the ID
  checksums are sums of host-order u16, and `PFS_EOF`/`PFS_PAGE_NOT_USED` are host-order u16. Page links keep
  their `{bank, page}` bytes, which is how the rest of the Pfs C reads them.
- So an N64-order image (v5's, an emulator's) can't be read. It is detected by its ID checksum and left untouched,
  and a blank pak is used in memory. A converter needs the game's save formats too.

**Cache, TLB, interrupts, FPU.** No-ops: `osInvalDCache`, `osWritebackDCache`, the TLB calls, `__osDisableInt`
(returns 1), `__osSetFpcCsr`/`__setfpcsr` (the game's FPU traps stay masked). `osSetIntMask` returns the previous
mask.

**Low memory.** `osInitialize` (the game's `__osInitialize_common`) writes NTSC, ROM base 0xB0000000, a cold reset,
CIC 6102 and `osMemSize` into the window at 0x80000300 and clears the NMI buffer.

## 4. The main loop and options

`main` resolves the settings (`rush2049pc.ini`, the environment and the options: [FRONTEND.md](FRONTEND.md)),
fixes the absolute symbols, maps the window, loads and checks the ROM and runs `osInitialize`. It then calls
`pc_data_init(rom, size)`, `pc_gfx_init(headless, 640, 480)` (the window size is a setting; the game draws
320 × 220), `pc_input_init`, `pc_si_init` and `pc_audio_init`. It starts `n64_main` on an N64 thread and runs the
loop on the host main thread.
- Each pass runs due timers, queued graphics tasks and the AI.
- At each retrace it calls `pc_gfx_poll_events` (window events, which also reach port/input), polls
  `pc_input_poll`, sends the VI event and presents a swapped framebuffer.
- A retrace that comes a whole period late (a slow graphics task, a swap waiting for vsync) is caught up: the
  ones due come at once, so the game keeps its 60 a second: it counts its time in retraces, and its audio renders
  the buffers the AI has passed at each one, skipping some when it finds too many ([AUDIO.md](AUDIO.md) §7). A stall
  of 6 periods or more (a debugger, a long hitch) resynchronizes and drops them.
  At exit the loop writes `N retraces in T s; C came a whole retrace late and were caught up, D dropped`.
- Between deadlines it sleeps until a thread submits work.
- While `pc_input_paused` (the pause key, or focus lost with `pause_on_focus_loss`), it only handles window
  events. Afterwards it sets the N64 clock back by the time held.
- The state log, the status lines and `--until-state` read the game's values when a retrace is due, before the
  loop does anything else: with `--fast` every thread waits then. Read after the retrace's event, they raced the
  threads it wakes (the model thread steps the cars at each retrace).
- With a high frame rate (`video.frame_rate`, [FRAMERATE.md](FRAMERATE.md)) the loop calls `pc_gfx_present_tick`
  that many times a second, in step with the retraces (the first tick at a retrace), with the moment in retraces;
  port/gfx shows nothing at the retraces then. On the host's clock the ticks are deadlines; with `--fast` they are
  not (they run when the loop wakes for something else), so the virtual clock stops at the same moments with the
  setting on or off. The ticks post nothing to the game. On the host's clock they give way to it: a tick waits while
  an RSP task is queued (`pc_rcp_pending`), is skipped when it would run into the next retrace (by how long the last
  ones took), and draws no frame between two while the host is behind (two retraces late within a second). At
  exit the loop says how many it skipped.

| Option | |
|---|---|
| `--rom PATH`, `--no-rom-check`, `--no-rom` | the ROM; run another dump; bring-up on a blank cartridge (data stays zero) |
| `--headless`, `--window` | EGL surfaceless (default without `DISPLAY`/`WAYLAND_DISPLAY`); an SDL2 window |
| `--seconds N`, `--frames N`, `--stop-at-gfx` | stop after N s, N graphics tasks, or at the first one |
| `--fast` | the virtual clock (section 1): as fast as the host can, the same run on any host; `--seconds` is game time |
| `--until-state MASK[:N]` | stop once the game's state word (gGameState) has a bit of MASK and N more graphics tasks have run; with `--seconds`, not getting there is exit code 1. Every change of the state word is logged, with the players, windows, cars, mode (gRace) and track, and the first car's position, speed and steering input ([SYNTH.md](SYNTH.md)); in whole feet, or as exact hex floats with `RUSH_STATE_EXACT` set |
| `--screenshot-at N`, `--screenshot PATH` | a PNG once N graphics tasks have run (default `rush2049pc-N.png`) |
| `--press T:BUTTON[@PORT][:DUR],...`, `--press-file PATH` | scripted input in v5's format |
| `--pak PATH`, `--no-pak`, `--rumble`, `--expansion-pak` | accessories; 8 MiB of RDRAM |
| `--audio BACKEND`, `--dump-audio PATH` | port/audio's output (`RUSH2049_AUDIO`, `RUSH2049_DUMP_AUDIO` too) |
| `-v`, `--trace-os`, `--status N`, `--selftest` | logging; switches; a status line every N s; the self-test |
| `--fullscreen`, `--size WxH`, `--scale N`, `--log-input` | the window and live input ([FRONTEND.md](FRONTEND.md)) |
| `--frame-rate R` | frames shown a second: `original`, `display` or 30-1000 ([FRAMERATE.md](FRAMERATE.md)) |
| `--config PATH`, `--no-config`, `--set S.KEY=VALUE`, `--print-config` | the settings file ([FRONTEND.md](FRONTEND.md)) |

`--frames` and `--screenshot-at` act two retraces after their graphics task, once the scheduler has swapped the
frame in and it has been shown.

## 5. The libultra units (`src/rom`)

**Replaced: 54 units, excluded in `GAME_REPLACED` (CMakeLists.txt).**

| Units | Functions | Replaced by |
|---|---|---|
| lib_7b30, lib_7c80, lib_8e10, lib_10d10, lib_107a0, lib_c990, lib_cc50, lib_d580 | `osCreateThread`, `osStartThread`, `osRecvMesg`, `osSetThreadPri`, `osYieldThread`, `osDestroyThread`, `osStopThread`, `osGetThreadId`, `osGetThreadPri`, the run-queue and timer internals | sched.c, timer.c |
| lib_7600, lib_81e0, lib_8040_msg, lib_7a10 | `osCreateMesgQueue`, `osSendMesg`, `osJamMesg`, `osSetEventMesg` | sched.c |
| lib_7fb0, lib_efd0 | `osGetTime`, `osSetTimer` | timer.c |
| lib_7630, lib_7940, lib_79a0, lib_7ad0, lib_8040, lib_8330, lib_83d0, lib_8920, lib_d140, lib_d270 | the VI manager and calls | vi.c |
| lib_8440, lib_8700, lib_a5b0, lib_e240, lib_e250, lib_e280, lib_e310, lib_e340, lib_10760, lib_ca70 | SP tasks, `osDpSetNextBuffer`, raw SP/DP, the AI | rcp.c |
| lib_8e10_pi, lib_9230, lib_8dd0, lib_e7c0, lib_e8d0, lib_e9a0, lib_10a00, lib_10b60 | the PI manager, `osPiStartDma`, raw PI | pi.c |
| lib_f0b0, lib_e390, lib_e3f0, lib_10970 | raw SI | si.c |
| lib_8a80, lib_109a0, lib_10cc0, lib_a800, lib_e1c0, lib_d260 | `osInitialize`, interrupt masks, address conversion, `osGetActiveQueue` | misc.c |
| lib_5610 | inflate (with the hand-written `inflate_entry`, `lzss_decode`) | pi.c |
| lib_34a0 | the IDO libc (`sprintf`, `modff`, the formatter core `fcvt`) | the host's; libc.c |

All of their data (`__osTimerList`, `osViClock`, the VI manager's state...) had users only inside this set.

**Compiled natively:** everything else. That covers `gu*`, libm, `memset`/`strlen`/`memcpy` (as `n64_*`),
`osContInit` and the controller packing, the whole `osPfs*` file system, `osMotorInit`, `__osSiGetAccess`, the
SDK scheduler (`lib_1050_sdk.c`), the VI timing helpers, the boot logic (`main`, `idle_thread_entry`, `game_init`),
the MusyX v1 driver (`lib_11640.c`), the sequencer and libgsm C.

**`TARGET_PC` edits in `src/`**, each preprocessing to the same tokens as before without `TARGET_PC` (checked with
gcc's preprocessor against main; the IDO build itself was not run):

| File | Edit |
|---|---|
| `lib_2cf0.c` | the idle loop calls `pc_os_idle`; `game_init`'s game-BSS `bzero` is skipped (zero at startup; the BSS with pointers lives outside that range) |
| `lib_1050_sdk.c` | the scheduler's layout and its N64 names (below) |
| `lib_11640.c` | `__unaligned`; the native task records (`unknown44` a pointer, the real types in `func_800139D4`, `sizeof` for the SFD and delay sizes); the DMA caches registered big-endian; host pointers count as RAM (`func_800146B4`, `func_80014CAC`); two busy-waits; the renderer's wrapping range goes on where the cap stopped it (`func_80013DEC`, [AUDIO.md](AUDIO.md) §7) |
| `lib_15a10.c`, `lib_17dc0.c` | `func_8001558C`/`func_800156E8` take pointer-sized integers (a stack address passed through a u32) |
| `lib_f700.c` | `__osIdCheckSum` adds to a pointer, not a truncated one |
| `lib_b300.c` | `__osPfsReleasePages` builds its first link through the byte fields, like every other link |
| `rom_tu.h` | declarations of `pc_mem_register_be`/`unregister_be` |

**The scheduler at the game's offsets.** port/data gave the game's N64 names for the scheduler's storage their own
storage, apart from `gScheduler`. The game sent its graphics tasks to `D_8002E960`, stopped `D_8002E998`, and
polled `D_8002EB70`; all three missed the scheduler. Under `TARGET_PC`, `lib_1050_sdk.c` lays `gScheduler` out with
the offsets the game reads it at:
- `retraceCount` at 0x27C: the game's frame clock, read as `frameCount`, `tick`, `[159]`, `gViTickCounter`;
- storage-only queues and thread at 0x40, 0x78 and 0xB0;
- host-sized pointers after them.

It defines those names as aliases into its own storage: `D_8002E8E8`, `D_8002E928`, `D_8002E960`, `D_8002E998`,
`gViTickCounter`, `D_8002EB64`, `D_8002EB5C` (`curRSPTask`), `D_8002EB70` (`__osScTaskCount`), and
`D_8002EB90`/`94`/`98`.

The scheduler reads tasks through `include/m2c_types.h`'s `OSScTask`. Its PC version has the OSTask in place with
host-sized pointers, as the game builds its tasks (`world_collision_response.c`'s `Frame`, `func_8010FBE0`'s
audio task). Before that, the scheduler read a task's done queue at its `output_buff` and blocked after the first
graphics task ([MUSYX_DATA.md](MUSYX_DATA.md) §8.4).

**The names missing from the symbol file.** By `rom_symbol_aliases.ld`, `osEPiRawReadIo`, `osEPiRawStartDma`,
`osEPiRawWriteIo`, `__osPiDeviceBusy`, `__osPiGetCmdQueue` and `__osTLBLookup` are the decomp's names for other
functions at those addresses. All their callers are in replaced units, so they are defined in pi.c and misc.c with
that meaning (no-ops, or the cartridge). `__osPfsDataChecksum` is `__osContDataCrc`, which `__osContRamRead`
calls.

## 6. Busy-waits

The game polls memory that another thread or an interrupt changes. v5 found 14 such loops in the N64 code; in the
C they are 12 loops, since overlay A inlined `func_8038B388` into two callers. Each calls `pc_os_spin_yield`
under `TARGET_PC`:

| Function | File | Waits for |
|---|---|---|
| `game_loop` | `src/blob/groups/input_aux/game_loop.c` | `D_8002EB70` (pending graphics tasks) = 0 |
| `func_80096130` (`dma_wait`) | `src/blob/groups/heap_release/func_80096130.c` | the same |
| `ReplaceWave` (v5: `suspension_setup`) | `src/blob/groups/audio_frame_sync/replacewave.c` | the same |
| `InitWaves` (v5: `tire_compound_set`) | `src/blob/InitWaves.c` | the same |
| `func_8038B388` (v5: also in `func_8038B3EC`, `func_8038B940`) | `src/ovl_a/groups/runtime_a_c93c_closure/group.c` | the same |
| `func_8038B930` | the same file | the same |
| `dma_wait_complete` | `src/blob/dma_wait_complete.c` | the audio task stops being the scheduler's current RSP task |
| `wheel_params_set` (two) | `src/blob/groups/heap_release/wheel_params_set.c` | `D_8011028C` = 0, then `D_80146160.head` = 0 |
| `func_800E7038` | `src/blob/func_800E7038.c` | `D_80111950` (controller bit pattern) ≠ 0x80 |
| `func_80011848` | `src/rom/lib_11640.c` | `D_800382CC` = 0 (MusyX) |
| `func_80014140` | `src/rom/lib_11640.c` | `D_80038362` ≤ 0 (MusyX) |

Weak copies of these functions in other group files are never linked in, so they are left alone.

## 7. gen_stubs and the executable

`rush2049pc` links the game, port/data's generated data and loader (`pcdata_rt`), `pcultra`, and `pcaudio` and
`pcgfx` whole (`$<LINK_LIBRARY:WHOLE_ARCHIVE>`), so their strong definitions beat `defaults.c`'s weak ones. Further
parts go in `PC_PORT_OBJECT_LIBS` or `PC_PORT_ARCHIVES`. gen_stubs counts the PC layer's definitions; its
references (libc, zlib, pthreads) are left to the linker. It also writes:

- `aliases.ld`, an implicit linker script:
  - `PROVIDE` aliases for names differing only in the case of their address digits (`func_8001fe58`);
  - aliases for address names of a defined symbol (`func_8008B964` = `physics_velocity_integrate_b`;
    `D_8008BEA4` = `func_8008BEA4`, a callback's address);
  - the absolute symbols of section 2.
- `stubs.c`: whatever is still missing calls `pc_stub_hit`. It prints the name, the N64 thread and a backtrace
  symbolized with addr2line (PIE offsets), then aborts. Faults print the same report.
- `stubs_standalone.c` (`--standalone`) for programs that link the game without the PC layer
  (`rush2049pc_datacheck`).

Now: 0 function stubs, 1 data placeholder (`ShowCull`), 75 aliases, 6 absolute symbols. The last function stubs were
the libgsm kernels, now `gsm.c` (§8).

## 8. Verification

- `rush2049pc --selftest` (also CTest `ultra_selftest`) passes 27 of 27 in about 1.5 s; `--selftest --fast`
  (`ultra_selftest_fast`) runs the same on the virtual clock, with the clock's own check in place of the
  device-paced AI's (real time only), in under 0.1 s. It covers:
  - threads: preemption on start, priorities, FIFO among equals and yield, `osSetThreadPri` both ways,
    `osStopThread` on a waiting thread;
  - queues: FIFO, jam, full and empty, a blocking send;
  - events: `osSetEventMesg`; the VI retrace at 60 Hz (12 in about 200 ms);
  - time: one-shot and periodic timers and `osStopTimer`; `osGetTime`'s rate;
  - busy-waits: both yield paths;
  - memory: the window and its KSEG1 alias, the absolute symbols, game data at its N64 addresses, host addresses
    above 4 GiB, big-endian ranges;
  - RSP tasks and DP buffers, and the AI rate and event;
  - in real time, the AI paced by a device: 30 pulls of 512 frames from a host thread, 10 ms apart, answered by
    88 buffers, only the first pull short;
  - the SI through the game's own C: `osContInit`, `osContGetReadData`, `osPfsInitPak`, `osPfsFreeBlocks`, and a
    three-page note allocated, written, read back after a new `osPfsInitPak`, and deleted.
- CTest `ultra_gsm` (`port/ultra/tests/test_gsm.c`, 2 s) checks `gsm.c`'s three GSM kernels against their own MIPS
  code, interpreted from the words in `asm/us/boot_data/kernels.s`, and against a GSM 06.10 reference decoder
  written from the spec. Kernel by kernel on random and edge inputs, and over 6,160 frames through the game's own
  frame decoder (`func_80026640`). The kernels keep the assembly's arithmetic: 32-bit products, and sums that wrap
  where the spec saturates (the filter state and `msr` stay 32-bit, `drp` wraps to 16 bits). So they match the
  MIPS code always, and the spec until the spec saturates something.
- All 32 CTests of the other parts still pass with the PC layer linked in.
- **Boot on a blank cartridge** (`rush2049pc --no-rom --no-pak --audio null`). The game's own boot runs on the PC
  layer: `main` starts the idle thread, the idle thread starts `game_init`, and `game_init` creates the queues and
  starts the scheduler (thread 4, priority 12), the audio thread (8) and the render thread (5). It goes into
  `game_late_init`, and the heap initializes in the window (`D_8017A640`). The first fault is in game code:
  `func_800CCCCC` (`src/blob/groups/menu_save_options/group.c:108`, from `menu_save_options`, from
  `game_late_init`) dereferences a handle, and handle tables are `u32` slots. That is agent D's pointer work. It
  happens before the first graphics task, so the Phase 2 goal (`--stop-at-gfx`) is not reached yet.
- With the ROM: not run (no ROM here).

## 9. What's left

- **Game code (agent D)** on the boot path:
  - `func_800CCCCC`'s handles (above), then whatever follows.
  - `dma_wait_complete`'s scheduler view reads `cur` at 0x274 as a native pointer. The scheduler's `curRSPTask` is
    at native offset 0x2B0 (`D_8002EB5C`); the view needs `TARGET_PC`.
  - The scheduler's `OSScTask`/`Frame` records in the arenas (`D_80156BE0`) are larger natively than their N64
    slots.
  - `fcvt_wrapper` passes a pointer into its own frame as an IDO va_list. Natively it needs a real `va_list`;
    until then `fcvt` copies the format unformatted and logs once.
- **Preemption.** Events never take the baton from a computing thread. A busy-wait not in the list above would hang.
  `--trace-os` and Ctrl-\ show it, and the fix is one more `pc_os_spin_yield`.
- **Controller pak interop** with N64-order images (v5, emulators) needs a converter that knows the save formats.
- **census.py** counts the names gen_stubs satisfies through `aliases.ld` (aliases and absolute symbols) as
  undefined. It also sees port/audio's test programs in `compile_commands.json`, which shows up as duplicate
  `main`/`ref_*`.
- **Untested:**
  - anything with the ROM, speech included (the GSM kernels have only decoded test frames);
  - the graphics task path end to end (`pc_gfx_run_dl` from a game task);
  - a live window and live input (only scripted input);
  - x86-64 and macOS. On macOS a 64-bit process's 4 GiB `__PAGEZERO` covers 0x80000000, so the window needs
    `-pagezero_size` there.
