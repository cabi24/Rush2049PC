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
| `si.c` | the PIF: controller status and buttons, each controller's accessory: a controller pak (`.mpk`), a rumble pak, hot swaps |
| `pakfile.c`, `pakwrite.c` | the controller pak's file: a blank image, what an image is, the atomic write; the thread that writes the paks |
| `memory.c` | the RDRAM window (its size: `game.memory`), `pc_n64_ptr`/`pc_n64_addr`, the big-endian ranges and the models' vertices' ranges (model data), the absolute-symbol fix-up |
| `heap.c` | the game heap's high-water marks and fallbacks, the exit line ([MEMORY.md](MEMORY.md) 4) |
| `crossings.c` | only in the counting build (`cmake -DRUSH_COUNT_CROSSINGS=ON`): every crossing between an N64 address and a host pointer counted by site, and the arena guard (`RUSH2049_WINDOW_GUARD=arenas`) ([NATIVE.md](NATIVE.md) 9) |
| `misc.c` | `osInitialize` and the low-memory globals, cache/TLB/interrupt no-ops, faults, stubs |
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
then kept the ended task as its current RSP task, and `audio_retrace_thread_entry`, which runs the audio callback after each
retrace once that task is done, waited for ever: the game hung at the start prompt or the main menu, now and then in
real-time runs on the Pi 5, more often when the main loop sends late retraces in a burst. The log names the first
event of each kind held (`event N held`).

**Side tables, not the game's structs.** The game's `OSThread`, `OSMesgQueue` and `OSTimer` are N64-sized in the BSS
arenas (port/data), and the native structs are larger, so nothing is written into them. Threads, queues and timers
are kept in tables keyed by the game's object address. Message buffers are host-side too: the game's `OSMesg`
arrays have 4-byte slots. A queue used before `osCreateMesgQueue` acts as a zero-capacity queue, as a zeroed one
does, and is logged.

**Idle and busy-waits.** `pc_os_idle` (the game's idle loop, `boot.c`) waits until any thread can run.
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
game's pointer-free data and BSS are not in it: port/data's arenas are blocks of host memory in the executable, laid
out as on the N64 ([DATA.md](DATA.md) §3; they were at their N64 addresses in the window until [NATIVE.md](NATIVE.md)
step 5), so the window holds the heap and low memory only, and nothing is at the arenas' N64 ranges. An 8 MiB memfd
(with `game.memory`, 16-256 MiB: [MEMORY.md](MEMORY.md)) is mapped at host 0x80000000 (KSEG0) and again at
0xA0000000 (KSEG1), with `MAP_FIXED_NOREPLACE`, before the data is loaded. On Linux the mapping is required and the
program stops if it fails, and on Windows too, where a file mapping is viewed at both addresses
([WINDOWS.md](WINDOWS.md)). Elsewhere it falls back to a translated window (`pc_n64_ptr`), and the game's literal
addresses fault.

**Host addresses are above 4 GiB**, except the window's. The executable is PIE, and its heap and the N64 threads'
stacks are mapped high. So the renderer tells host pointers (≥ 4 GiB) from N64 addresses (K0/K1, physical,
segmented), and game data in the window is simply RDRAM to it. The game gives the renderer its colour and depth
images as the buffers' pointers; there is no `osVirtualToPhysical` (until [NATIVE.md](NATIVE.md) step 4 it made
their physical addresses). Pointers to the game's data in the arenas are host pointers like any other's (until step
5 the renderer saw `default_lights` and the cull vertices in native lists as window addresses). In a window of more
than 16 MiB the display lists' relocations keep the K0 address past 16 MiB, where a physical one would read to the
RSP as a segmented one ([MEMORY.md](MEMORY.md) 3).

**Data at absolute N64 addresses.** Some names the game uses are addresses, not storage: the low-memory globals
(`osTvType`, `osTvType`, `osMemSize` for osMemSize), `gDecompressedDataStart`, and the ROM offset `sample_bank_rom` (the
heap start `game_bss_end` and the images' base `overlay_base` were until [NATIVE.md](NATIVE.md) step 6: the heap's
memory is `pc_heap_memory`'s now, the images' block an offset in it). gen_stubs makes them absolute symbols
(`aliases.ld`): window addresses, and ROM offsets as cartridge addresses (`0xB0000000 | offset`, which the game's PI and
MusyX address callbacks accept). port/data's arena names were absolute symbols too until NATIVE.md step 5 (`data.c`,
about 1,800); they are relative to their arenas' blocks now, which the executable's own relocations place. The game
reaches all of them through the GOT. binutils 2.40 on aarch64 gives the GOT slot of an absolute symbol in a PIE a
RELATIVE relocation, so at run time the slot holds load address + value. `pc_mem_fix_absolute`, called in `main` before
anything else runs, puts the values back (1,802 slots with the arenas' names, a handful now: `aliases.ld`'s). It only
touches RELATIVE relocations whose addend is at 0x80000000 or above, where nothing the linker places can be
(`gen_data.py --check` verifies that), and whose slot holds exactly base + addend. Initialized pointers to them are
resolved at link time. On x86-64 with binutils 2.41 only the `aliases.ld` names' slots carry such a relocation (6
slots fixed; game_bss_end's and overlay_base's went with step 6); data.c's absolute names got slots that held the bare
value with no relocation, and its block-relative ones get RELATIVE relocations as any relocatable symbol's do. The
plan for a window at any address, and the guard mode that moves it now (`RUSH2049_NOFIXED=guard`), are in
[NOFIXED.md](NOFIXED.md).

**osMemSize is 8 MiB** (the Expansion Pak) by default, as N64ModernRuntime and v5 report: the game sets its
Expansion Pak flag (`expansion_pak_present`) and loads overlay B and the extra waves in races, which give it its sixth race
track, the tracks' animated objects and the front end's song (docs/port/QA.md). `game.expansion_pak = off` reports
4 MiB, a console without one, where the game skips them; the window is 8 MiB either way. `game.memory` (16-256 MiB)
sizes the window and osMemSize together, and so the game's heap ([MEMORY.md](MEMORY.md)); the heap's peak and least
free are the exit line's `heap:`.

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

**Yields.** When an audio task comes while a graphics task is on the RSP, the game's scheduler asks that task to
yield (`osSpTaskYield`), takes its SP event, and asks `osSpTaskYielded` whether it stopped. If it did, the scheduler
runs the audio task and then starts the graphics task again. In real time the port does this for a graphics task
the main loop hasn't taken yet: it leaves the queue with an SP event, `osSpTaskYielded` returns `OS_TASK_YIELDED`
once and marks the task, and `osSpTaskLoad` clears the mark when the task is started again. None of the task ran, so
it runs whole then: the interpreter never sees half a task. A task the main loop has taken runs to completion, and
its own SP event follows. Without yields, an audio task waited behind a swap or a slow present, past the next
retrace (AUDIO.md §7). With `--fast` nothing yields: the main loop runs every task in the order it came. The
exit line counts the yields. `osDpSetNextBuffer` gets a DP event. `--stop-at-gfx` reports the
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
- `unzipdata_from_rom(src, dst, n)`:
  - the game image (ROM 0xB0CB10, `gRomCompressedData`) does nothing: it is linked in and port/data filled its data;
  - the runtime images (0xB5C534, 0xB6FEC4) become `pc_data_overlay_load` (port/data's overlay loaders call that
    themselves under `TARGET_PC`);
  - any other stream is inflated with zlib from the ROM.
- `unzipdata(src, size, dst)` inflates from memory (ghosts).
- `lzss_decode(src, dst)` is the boot LZSS: flag bytes LSB first; a copy is two bytes, length `(b0 & 15) + 2`,
  distance `(b0 & 0xF0) << 4 | b1`; distance and length nibble 0 end the stream.

**SI.** The game's own libultra C runs unchanged: `osContInit`, `osContStartReadData`, `__osContRamRead`/`Write`,
the whole `osPfs*` file system and `osMotorInit`. `__osSiRawStartDma` writes fill a 64-byte PIF RAM. A read runs
its commands, as the PIF does, so re-reading gives fresh buttons. The commands are 0x00/0xFF status, 0x01 buttons,
and 0x02/0x03 accessory read/write of 32-byte blocks with the address CRC and a data CRC from the game's own
`__osContDataCrc`. Each DMA sends the SI event. The block layout is the hardware's, since the game fills it through
u8 fields. The one u16 field, the buttons, is read natively, so it is written in host order. There is no EEPROM:
the game saves only to the controller pak.

**Accessories: one per controller.** Each of the four ports holds a controller pak, a rumble pak or nothing
(`pc_si_set_pak`, from `game.pak`, `game.pak2`, `pak3`, `pak4`; [FRONTEND.md](FRONTEND.md) "Settings").
- Each port is its own: the status command reports its accessory (0x01, or 0x02 for none), the 32-byte reads and
  writes reach its pak, and a rumble pak's motor (0xC000) is that port's (`pc_si_motor(channel)`, which port/input
  sends to the player's controller).
- A controller pak has a file of its own per port (`rush2049pc.mpk`, `rush2049pc-p2.mpk`...), loaded, written and
  backed up the same way (below). A new one is formatted with an ID of the port's own: `pakfile.c`'s blank pak's
  random word, changed by the port number (controller 1's is the same as before). The game compares pak IDs
  across ports and takes two paks with one ID for one pak moved between them (PS_PakServer's "move"), so a file
  copied to another player's name is logged as such.
- **Auto** is a rumble pak while the port's controller can rumble (`PcPad.rumble`, from SDL), else nothing: a pad
  that leaves or comes takes its rumble pak with it. It is the default for players 2-4.
- **Hot swaps.** The game polls every port for accessories every 30 frames (ControllerUpdate's
  `osContStartQuery`, then `pak_poll_inserted` marks each pak record inserted or changed), and its pak server
  (PS_PakServer, from SaveServer) handles a pak that left (its files dropped, rumble stopped) or came (osPfsInitPak,
  or osMotorInit when that fails). So a change while the game runs (the overlay, a pad on auto plugged or
  unplugged) is made as a player makes one: the old accessory comes out at once, and the new one goes in only after
  a poll of every port has found the port empty (or without its controller). The game's other status reads
  (`__osPfsGetStatus`, one port each, from osPfsInitPak, osMotorInit and the CRC retries) don't count, so the pak
  code always sees the old one leave. A memory pak taken out is written first, and keeps its image in memory if it
  goes back in. CONT_CARD_PULL (0x02 with 0x01) is never reported: osPfsInitPak would return PFS_ERR_NEW_PACK, which
  the pak server takes for a damaged pak.
- The log says when the game first reads each accessory put in ("controller 2: the game reads the controller
  pak"); `-v` when one goes in or out, `-v -v` each motor change.
- **On the retail cartridge** (the Pi, 2026-10-28, scripted, `--fast`): a two-player race with controller paks on
  controllers 1 and 2 reads both (each a new file, its own ID); with rumble paks on both, the game finds both
  (osMotorInit on each port). In a minute of a gas-held race with crashes neither motor went on, for player 1 as
  for player 2: UpdateModel sets each player's rumble request (`gPaks_rumble_request`, at the PC stride) from the impact,
  scaled by the player's own rumble setting (its profile's byte 0x2B: 0 none, 1 weaker, 2 full), and the default
  player has 1 (the profile check, `CheckAndRepairPlayer`, gives a profile without valid controls the default
  controls and 1): four steps off the impact's level. While the impact's thresholds (`percentTable`) loaded in the
  ROM's byte order the scripted crashes stayed under that; with them loaded whole (ab7f592) a retail race's crashes
  turn the motor on at the default as at full (the player screen's controls, the rumble row), at strengths that
  follow the impact, and it reaches the controller (QA.md, rounds 4 and 5). The synthetic race's data turns both
  motors on, and they reach both players' controllers (`rumble2p`, `input_live_drive`). The pak server's prompt
  port (`gErrPak`) is -1 at start there, so it scans every port.
- `PS_SetRumblePercent(index, value)`, a setter of the same request, still strides by the N64's 772 bytes; nothing calls
  it, so it is left as it is (on the PC it would reach players 2-4's requests 12, 24, 36 bytes early).

**Controller pak.**
- `--pak PATH` (controller 1's; the others' are `game.pak2_file`...): default `rush2049pc.mpk`, created formatted
  when missing and written within a second of each change (`pakfile.c`'s `pc_file_write_atomic`: a temp file
  flushed to the disk, then renamed over it, then the folder flushed). Only a missing file is new: one that can't be
  read, or is damaged (`pc_pak_image_check`: not 32 KiB, no good ID block copy, no good inode table), plugs in no pak
  and is never written over. port/app's saves.c sets such a file aside before the game starts, and keeps three
  backups ([RELEASE.md](RELEASE.md) "Saves").
- **The writes are on a thread of their own** (`pakwrite.c`, since 2026-10-28). The flushes to the disk take under
  20 ms on a fast disk, but far more on a laptop's hard disk, a USB stick, a network folder, or while an antivirus
  scans the file; on the main loop that held the retraces and the audio's AI events (a stutter, the sound dropping
  out). Once a second the main loop's `pc_si_flush` copies each changed image (32 KiB, under si.c's lock, as the game
  can't change it halfway) and hands it to the writer, and returns:
  - a newer image of a file not yet written replaces the older one in its place in the line, so the files are
    written in the order they changed, each with its latest image, never an older one after a newer one;
  - one thread writes them, one at a time: a file never has two writes at once;
  - a write that fails keeps its image (unless a newer one came meanwhile) and is said once in the log
    (`[pak writer] controller 1's pak: cannot write ...`); the next flush tries it again, a second later, as before;
  - with `--fast` the flush waits for the writes. The virtual clock moves only in the main loop's waits, so the
    game's run is the same either way, a test finds the file written when the loop goes on, and a failure is said
    and tried again at the same point of every run. The thread still does the writes, so the TSan build checks it
    on every `--fast` path with a pak.
- **The end of a run waits for the writer** (`pc_si_finish`: every changed image handed over, each failed one tried
  once more, then up to 15 s; after 1 s the log says `the saves are still being written: ...`, and what is still
  not written after 15 s is named). That is the loop's end, however it stops: Quit in the F1 overlay, the quit key,
  the window's close, `--seconds`, `--frames`, `--until-state`, SIGINT and SIGTERM (the loop's handlers), SIGHUP
  (the terminal closed; main.c stops the loop, where it used to end the process at once), and on Windows the
  console's close, a logoff or a shutdown with a console (`win32.c`'s `SetConsoleCtrlHandler`: the handler stops the
  loop and waits while it writes, as Windows ends the process when the handler returns or 5 s after; a loop that
  doesn't stop in 3 s gets the writes done from the handler). And `--stop-at-gfx`'s `_exit`. A GUI program's
  logoff or shutdown without a console (`WM_ENDSESSION`, which SDL doesn't pass on) isn't covered: the last second
  of changes can be lost there, as before.
- **A fault** (`misc.c`'s signal handler, `win32.c`'s exception filter, `pc_fatal`, a stub, port/app's SIGABRT
  handler) calls `pc_si_fault_flush` before the log is closed: the writer finishes what it has (up to 2 s), and a
  changed image not handed over yet is written too, if that is safe: si.c's lock had within 50 ms (so no pak
  transfer was in progress on the thread that faulted) and the image a good pak (`pc_pak_image_check`). Nothing waits
  on a lock: the locks are only tried. When the writer is the thread that faulted, nothing more is written. A crash
  mid-write still leaves the old file or the new one (the atomic write).
- **Test hooks**: `RUSH_SIM_PAK_WRITE_MS` holds each write that long (a slow disk), `RUSH_SIM_PAK_REWRITE=1` writes
  every memory pak at every flush, changed or not, and `RUSH_SIM_PAK_WRITE_SYNC=1` writes on the main loop, as
  before. With `-v` each hand-over is logged with its retrace, and the end says how many writes there were and the
  longest.
- `--no-pak` plugs in nothing on any port, and `--rumble` a rumble pak on controller 1 (0x8000 probe, 0xC000
  motor).
- **Saves as files** (`game.saves = files`, 2026-11-04, [SAVES.md](SAVES.md)): the port's image is built from the
  player's folder (`pc_si_set_pak_store`, `store_load`: pakstore.c's `pc_store_build`) and handed to the writer with
  a write of its own (`pc_pakw_submit_fn`: `pc_store_write` splits it into the folder's files). The game sees a pak
  as above; a change of which saves are on it is made as a hot swap (`swap_pak`) only when main's check says the game
  can take one (`pc_si_set_swap_check`), else at the next start. With `pak` nothing here changed.
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
- A retrace that comes a whole period late (a slow graphics task, a swap waiting for vsync) is caught up, so the
  game keeps its 60 a second: it counts its time in retraces, and its audio renders the buffers the AI has passed
  at each one, skipping some when it finds too many ([AUDIO.md](AUDIO.md) §7). A stall of 6 periods or more (a
  debugger, a long hitch) resynchronizes and drops them.
- The late ones come one at a time. In real time each waits until the game's threads are done with the one before
  (`pc_os_quiet`): the system is idle, or the baton is back with the thread that held it when that retrace went out,
  with no thread above it waiting. The wait is at most 2 ms. Sent at once, a burst of them reached the scheduler's
  clients together, and the audio manager, whose 128-message queue also takes a command per sound source per
  retrace, overflowed it on a loaded host. With `--fast` they come at once, as before. Nothing else goes first: a
  frame between two (a high frame rate) is not drawn while a retrace is due, so a swap that waits for vsync can't
  hold the retraces behind ([FRAMERATE.md](FRAMERATE.md) 2, "The retraces keep their 60 Hz").
- At exit the loop writes `N retraces in T s; C came a whole retrace late and were caught up, D dropped (...); L
  late ones went out before the game was done with the last; at most R late one after another` (L: those the 2 ms
  wait sent anyway; R: the longest run, one or two where the catch-up works, hundreds where the loop held the
  retraces a retrace behind). On rocky (load
  2), `realtime_check` with slow presents (`RUSH_SIM_PRESENT_MS=20`): the fullest 128-message queue peaked at 24
  and 27 messages, down from 28 and 40 when they came at once. One late retrace in each run went out at the 2 ms
  limit, and none were dropped.
- Between deadlines it sleeps until a thread submits work.
- Once a second it hands the changed controller paks to their writer thread (`pc_si_flush`, section 3 "Controller
  pak"), which writes them to the disk while the loop goes on; at exit it waits for them (`pc_si_finish`).
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
  exit the loop says how many it skipped. With a swap that waits for vsync the ticks follow the display's vblanks
  instead, one each, drawn as late as they make it, never across a retrace; the game's frames no tick showed are
  shown at the next retrace's present (FRAMERATE.md 2). The vblanks are at the display's own period and vblank times
  where the platform gives them (`pc_gfx_display_timing`), and on a display that keeps its phase against the
  retraces (60.000 Hz) the retrace grid is moved once to sit halfway between two vblanks, its 60 Hz unchanged.

| Option | |
|---|---|
| `--rom PATH`, `--no-rom-check`, `--no-rom` | the ROM; run another dump; bring-up on a blank cartridge (data stays zero) |
| `--headless`, `--window` | EGL surfaceless, WGL on Windows (on Linux the default without `DISPLAY`/`WAYLAND_DISPLAY`); an SDL2 window |
| `--seconds N`, `--frames N`, `--stop-at-gfx` | stop after N s, N graphics tasks, or at the first one |
| `--fast` | the virtual clock (section 1): as fast as the host can, the same run on any host; `--seconds` is game time |
| `--paced` | the virtual clock paced to the wall clock (section 10): `--fast`'s run in real time, online play's clock (`RUSH_PACED=1` too) |
| `--until-state MASK[:N]` | stop once the game's state word (gGameState) has a bit of MASK and N more graphics tasks have run; with `--seconds`, not getting there is exit code 1. Every change of the state word is logged, with the players, windows, cars, mode (gRace) and track, and the first car's position, speed and steering input ([SYNTH.md](SYNTH.md)); in whole feet, or as exact hex floats with `RUSH_STATE_EXACT` set |
| `--screenshot-at N`, `--screenshot PATH` | a PNG once N graphics tasks have run (default `rush2049pc-N.png`) |
| `--press T:BUTTON[@PORT][:DUR],...`, `--press-file PATH` | scripted input in v5's format; `--press-via K[:T]` sends it through SDL as live input (FRONTEND.md "Testing live input") |
| `--pak PATH`, `--no-pak`, `--rumble`, `--expansion-pak` | accessories; 8 MiB of RDRAM |
| `--audio BACKEND`, `--dump-audio PATH` | port/audio's output (`RUSH2049_AUDIO`, `RUSH2049_DUMP_AUDIO` too) |
| `-v`, `--trace-os`, `--status N`, `--selftest` | logging; switches; a status line every N s; the self-test |
| `--log PATH`, `--no-log` | the log file ([RELEASE.md](RELEASE.md) "The log") |
| `--version`, `-h`, `--help` | the version and the build; the options |
| `--fullscreen`, `--exclusive`, `--resolution R`, `--size WxH`, `--scale N`, `--widescreen A`, `--log-input` | the window and live input ([FRONTEND.md](FRONTEND.md)) |
| `--frame-rate R` | frames shown a second: `original`, `display` or 30-1000 ([FRAMERATE.md](FRAMERATE.md)) |
| `--log-physics N` | every N retraces, read as the state log is, the model thread's tick and time and each car's position, velocity, lap and place (exact with `RUSH_STATE_EXACT`): `game.frame_rate`'s comparison ([FRAMERATE.md](FRAMERATE.md) 7, `tools/port/fps_compare.py`) |
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
| lib_5610 | inflate (with the hand-written `unzipdata_from_rom`, `lzss_decode`) | pi.c |
| lib_34a0 | the IDO libc (`sprintf`, `modff`, the formatter core `fcvt`) | the host's; libc.c |

All of their data (`__osTimerList`, `osViClock`, the VI manager's state...) had users only inside this set.

**Compiled natively:** everything else. That covers `gu*`, libm, `memset`/`strlen`/`memcpy` (as `n64_*`),
`osContInit` and the controller packing, the whole `osPfs*` file system, `osMotorInit`, `__osSiGetAccess`, the
SDK scheduler (`scheduler.c`), the VI timing helpers, the boot logic (`main`, `idle_thread_entry`, `main_thread_entry`),
the MusyX v1 driver (`hardware.c`), the sequencer and libgsm C.

**`TARGET_PC` edits in `src/`**, each preprocessing to the same tokens as before without `TARGET_PC` (checked with
gcc's preprocessor against main; the IDO build itself was not run):

| File | Edit |
|---|---|
| `boot.c` | the idle loop calls `pc_os_idle`; `main_thread_entry`'s game-BSS `bzero` is skipped (zero at startup; the BSS with pointers lives outside that range) |
| `scheduler.c` | the scheduler's layout and its N64 names (below); the retraces between two swaps, `PC_SWAP_RETRACES`: 2, or 1 with `game.frame_rate` 60 ([FRAMERATE.md](FRAMERATE.md) 7) |
| `timer.c` | UpdateFrameTimer gives each frame's retraces to `pc_game_frame_note` (`game.frame_rate`'s steps) |
| `hardware.c` | `__unaligned`; the native task records (`unknown44` a pointer, the real types in `sound_hw_build_subframe`, `sizeof` for the SFD and delay sizes); the DMA caches registered big-endian; host pointers count as RAM (`sound_hw_set_voice_sample`, `sound_hw_translate_address`); two busy-waits; the renderer's wrapping range goes on where the cap stopped it (`sound_hw_service_frame`, [AUDIO.md](AUDIO.md) §7) |
| `groups.c`, `sequencer.c` | `sound_start_song_in_group`/`sound_start_song` take pointer-sized integers (a stack address passed through a u32) |
| `contpfs.c` | `__osIdCheckSum` adds to a pointer, not a truncated one |
| `pfsdeletefile.c` | `__osPfsReleasePages` builds its first link through the byte fields, like every other link |
| `rom_tu.h` | declarations of `pc_mem_register_be`/`unregister_be` |

**The scheduler at the game's offsets.** port/data gave the game's N64 names for the scheduler's storage their own
storage, apart from `gScheduler`. The game sent its graphics tasks to `scheduler_task_queue`, stopped `scheduler_thread`, and
polled `gRenderInProgress`; all three missed the scheduler. Under `TARGET_PC`, `scheduler.c` lays `gScheduler` out with
the offsets the game reads it at:
- `retraceCount` at 0x27C: the game's frame clock, read as `frameCount`, `tick`, `[159]`, `scheduler_retrace_count`;
- storage-only queues and thread at 0x40, 0x78 and 0xB0;
- host-sized pointers after them.

It defines those names as aliases into its own storage: `gScheduler`, `scheduler_interrupt_queue`, `scheduler_task_queue`, `scheduler_thread`,
`scheduler_retrace_count`, `scheduler_retrace_count`, `scheduler_current_rsp_task` (`curRSPTask`), `gRenderInProgress`, and
`gSeconds`/`94`/`98`.

The scheduler reads tasks through `include/m2c_types.h`'s `OSScTask`. Its PC version has the OSTask in place with
host-sized pointers, as the game builds its tasks (`EndGraphics.c`'s `Frame`, `musyx_submit_audio_task`'s
audio task). Before that, the scheduler read a task's done queue at its `output_buff` and blocked after the first
graphics task ([MUSYX_DATA.md](MUSYX_DATA.md) §8.4).

**The names missing from the symbol file.** By `rom_symbol_aliases.ld`, `osEPiRawReadIo`, `osEPiRawStartDma`,
`osEPiRawWriteIo`, `__osPiDeviceBusy`, `__osPiGetCmdQueue` and `__osTLBLookup` are the decomp's names for other
functions at those addresses. All their callers are in replaced units, so they are defined in pi.c and misc.c with
that meaning (no-ops, or the cartridge). `__osPfsDataChecksum` is `__osContDataCrc`, which `__osContRamRead`
calls.

## 6. Busy-waits

The game polls memory that another thread or an interrupt changes. v5 found 14 such loops in the N64 code; in the
C they are 12 loops, since overlay A inlined `clearTrackMap` into two callers. Each calls `pc_os_spin_yield`
under `TARGET_PC`:

| Function | File | Waits for |
|---|---|---|
| `GameLoop` | `src/blob/groups/front_end/GameLoop.c` | `gRenderInProgress` (pending graphics tasks) = 0 |
| `UnloadWaveInstance` (`acquire_decompress_lock`) | `src/blob/groups/heap_release/UnloadWaveInstance.c` | the same |
| `ReplaceWave` (v5: `ReplaceWave`) | `src/blob/groups/waves/replacewave.c` | the same |
| `InitWaves` (v5: `tire_compound_set`) | `src/blob/InitWaves.c` | the same |
| `clearTrackMap` (v5: also in `trackscr_animTrackMap`, `trackscr_UninitScreen`) | `src/ovl_a/groups/trackscr/trackscr.c` | the same |
| `clearTrackMaps` | the same file | the same |
| `audio_retrace_thread_entry` | `src/blob/audio_retrace_thread_entry.c` | the audio task stops being the scheduler's current RSP task |
| `audio_sfx_shutdown` (two) | `src/blob/groups/heap_release/audio_sfx_shutdown.c` | `gStopAllEffects` = 0, then `gCmdQ.head` = 0 |
| `InitControlSys` | `src/blob/InitControlSys.c` | `cont_bit_pattern` (controller bit pattern) ≠ 0x80 |
| `sound_hw_free_dma` | `src/rom/sound/hardware.c` | `sound_dma_busy` = 0 (MusyX) |
| `sound_hw_remove_callback` | `src/rom/sound/hardware.c` | `sound_drain_count` ≤ 0 (MusyX) |

Weak copies of these functions in other group files are never linked in, so they are left alone.

## 7. gen_stubs and the executable

`rush2049pc` links the game, port/data's generated data and loader (`pcdata_rt`), `pcultra`, and `pcaudio` and
`pcgfx` whole (`$<LINK_LIBRARY:WHOLE_ARCHIVE>`), so their strong definitions beat `defaults.c`'s weak ones. Further
parts go in `PC_PORT_OBJECT_LIBS` or `PC_PORT_ARCHIVES`. gen_stubs counts the PC layer's definitions; its
references (libc, zlib, pthreads) are left to the linker. It also writes:

- `aliases.ld`, an implicit linker script:
  - `PROVIDE` aliases for names differing only in the case of their address digits (`func_8001fe58`);
  - aliases for address names of a defined symbol (`AnimateBSpark` = `AnimateBSpark`;
    `AnimateBrakeLights` = `AnimateBrakeLights`, a callback's address);
  - the absolute symbols of section 2.
- `stubs.c`: whatever is still missing calls `pc_stub_hit`. It prints the name, the N64 thread and a backtrace
  symbolized with addr2line (PIE offsets), then aborts. Faults print the same report.
- `stubs_standalone.c` (`--standalone`) for programs that link the game without the PC layer
  (`rush2049pc_datacheck`).

Now: 0 function stubs, 1 data placeholder (`ShowCull`), 75 aliases, 6 absolute symbols. The last function stubs were
the libgsm kernels, now `gsm.c` (§8).

## 8. Verification

- `rush2049pc --selftest` (also CTest `ultra_selftest`) passes 31 of 31 in about 1.5 s; `--selftest --fast`
  (`ultra_selftest_fast`) runs the same on the virtual clock, with the clock's own check in place of the
  device-paced AI's and without the yield (real time only): 30 of 30 in under 0.1 s. It covers:
  - threads: preemption on start, priorities, FIFO among equals and yield, `osSetThreadPri` both ways,
    `osStopThread` on a waiting thread;
  - queues: FIFO, jam, full and empty, a blocking send;
  - events: `osSetEventMesg`; the VI retrace at 60 Hz (12 in about 200 ms);
  - time: one-shot and periodic timers and `osStopTimer`; `osGetTime`'s rate;
  - busy-waits: both yield paths;
  - memory: the window and its KSEG1 alias, the absolute symbols, game data in its arena's block (at the N64
    address's offset in a page, above 4 GiB), host addresses above 4 GiB, big-endian ranges;
  - RSP tasks and DP buffers, a queued graphics task yielding (real time only), and the AI rate and event;
  - in real time, the AI paced by a device: 30 pulls of 512 frames from a host thread, 10 ms apart, answered by
    88 buffers, only the first pull short;
  - the SI through the game's own C: `osContInit`, `osContGetReadData`, `osPfsInitPak`, `osPfsFreeBlocks`, and a
    three-page note allocated, written, read back after a new `osPfsInitPak`, and deleted;
  - each port's accessory (`pc_si_test_pads` holds the pads the test sets): `osContStartQuery`'s status per port, a
    note on controller 2's pak alone and its own ID, controller 3's rumble pak (auto, a pad that can rumble) driving
    its motor alone, nothing on 4 (`PFS_ERR_NOPACK`), a controller pak refusing `osMotorInit` (`PFS_ERR_DEVICE`);
    a hot swap from controller pak to rumble pak (out at once, still out through a single-port status read, in
    after the poll) and back (the note still there); auto following its pad's rumble and its unplugging.
- CTest `ultra_gsm` (`port/ultra/tests/test_gsm.c`, 2 s) checks `gsm.c`'s three GSM kernels against their own MIPS
  code, interpreted from the words in `asm/us/boot_data/kernels.s`, and against a GSM 06.10 reference decoder
  written from the spec. Kernel by kernel on random and edge inputs, and over 6,160 frames through the game's own
  frame decoder (`gsm_decode_frame`). The kernels keep the assembly's arithmetic: 32-bit products, and sums that wrap
  where the spec saturates (the filter state and `msr` stay 32-bit, `drp` wraps to 16 bits). So they match the
  MIPS code always, and the spec until the spec saturates something.
- All 32 CTests of the other parts still pass with the PC layer linked in.
- CTest `ultra_pakwrite` (`port/ultra/tests/test_pakwrite.c`, 1 s) checks the paks' writer with a fake write that is
  slow, held or failing: a submit returns at once while a write takes 200 ms (38 more meanwhile, none held); a
  newer image replaces a queued one in its place (A, B, C queued behind A's write, then A and B again: written A, B's
  newer, C, A's newest); one write at a time; a failed write kept, not tried again on its own, tried by
  `pc_pakw_retry`, and dropped for a newer image; the wait at the end and its timeout; the fault path; and the real
  atomic write into a folder with a space.
- CTest `pak_realtime` (`tools/port/pakwrite_check.py`, POSIX, 105 s alone) runs the synthetic pak path in real time
  with each write held 200 ms and the pak written every second (`RUSH_SIM_PAK_WRITE_MS`, `RUSH_SIM_PAK_REWRITE`): on
  rocky 47 writes, 21 of them in the race, cost 0 retraces (none caught up, none dropped), and the pak is good at
  exit. SIGTERM during a held 3 s write waits for it, and the file is written. The blank cartridge's boot
  (`--no-rom`), which faults in its first retraces, during a held 1.5 s write: the fault handler waits for it, and
  the file is written. The control, the same writes on the main loop (`RUSH_SIM_PAK_WRITE_SYNC=1`), dropped 561
  and 573 retraces in two runs.
- **Boot on a blank cartridge** (`rush2049pc --no-rom --no-pak --audio null`). The game's own boot runs on the PC
  layer: `main` starts the idle thread, the idle thread starts `main_thread_entry`, and `main_thread_entry` creates the queues and
  starts the scheduler (thread 4, priority 12), the audio thread (8) and the render thread (5). It goes into
  `InitSystem`, and the heap initializes in the window (`game_bss_end`). The first fault is in game code:
  `InitPlayer` (`src/blob/groups/save/group.c:108`, from `SaveInit`, from
  `InitSystem`) dereferences a handle, and handle tables are `u32` slots. That is agent D's pointer work. It
  happens before the first graphics task, so the Phase 2 goal (`--stop-at-gfx`) is not reached yet.
- With the ROM: not run (no ROM here).

## 9. What's left

- **Game code (agent D)** on the boot path:
  - `InitPlayer`'s handles (above), then whatever follows.
  - `audio_retrace_thread_entry`'s scheduler view reads `cur` at 0x274 as a native pointer. The scheduler's `curRSPTask` is
    at native offset 0x2B0 (`scheduler_current_rsp_task`); the view needs `TARGET_PC`.
  - The scheduler's `OSScTask`/`Frame` records in the arenas (`frame_records`) are larger natively than their N64
    slots.
  - `TB_Sprintf` passes a pointer into its own frame as an IDO va_list. Natively it needs a real `va_list`;
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

## 10. Paced mode: the virtual clock in real time (online play's clock)

Written 2026-11-06 for online play ([NETPLAY.md](NETPLAY.md) 1.2, 5 phase 0.1-0.2, 11). Delay-based lockstep needs
two PCs to take the same course from the same inputs, which only the virtual clock gives (section 1; real time's
course depends on the host, NETPLAY.md E7), at the pace of real time.

**What it is.** `--paced` (or `RUSH_PACED=1` in the environment, for tests) turns on `--fast`'s virtual clock with
everything `--fast` does on the device side (audio tasks on the main loop, no yields, AI events from the main loop
only), and paces it: the clock moves to its next deadline only when every N64 thread waits *and* the wall clock has
reached that deadline's wall time. It is a mode a session turns on, not a setting: off, nothing changes. A paced run
takes `--fast`'s course to the bit (the proof below); what the wall clock does (a slow host, a stall, a vsync swap, a
hold) changes when the N64 side moves, never what it does.

**The wait** (`sched.c`, `pc_os_wait_main_paced`). `wait_main_virtual`'s rules, then a sleep to the deadline's wall
time with the scheduler's lock released: a condition variable's timed wait on Linux, the wake-up event and the
high-resolution waitable timer on Windows (as the real-time wait has them, WINDOWS.md 5). A thread's
`pc_os_wake_main` still ends it at once with the clock where it is, as with `--fast`; a nudge (`pc_os_nudge_main`,
from the time sync) makes it look again with no work for the N64 side. A busy-wait's 1 ms nap is 1 ms of N64 time,
paced too. Each sleep is at most 50 ms, so `pc_quit` is seen during a long wait (a session's start ahead).

**The map** (`pace.c`). N64 time v is due at wall time `epoch + v`, on `pc_real_ns`'s clock (`CLOCK_MONOTONIC`;
QueryPerformanceCounter through winpthreads on Windows; `pc_pace_wall_ns`). The epoch is set when the loop starts (the
N64 time then is due then), or by the session (`pc_pace_set_start`: N64 time 0 at an agreed wall time, so retrace N at
`start + N/60 s`). It moves with:

| What | How |
|---|---|
| a hold that says so: `hold()` (the pause key, focus lost, the F1 overlay) | the epoch later by the time held (`pc_pace_hold`); the retraces keep their N64 times (with `--fast` a hold puts the next retrace a period after the clock's moment, which moves the grid; paced mode doesn't) |
| a lag: anything that holds the clock without saying so (a slow host, a stalled present, a debugger, a thread computing long, a gate at the retrace waiting for a remote player's input) | a move whose wall time has gone by is made at once. A lag over 2 ms and up to two retraces is caught up smoothly: a retrace comes no sooner than three quarters of a retrace after the last one came (12.5 ms apart, not a burst), the moves between them as soon as they can (from the last retrace's own time, so a delay of the loop's, a blocking swap, never adds to the next); with coarse waits (below) at once. A longer lag re-bases: the epoch moves so the move is due now. Counted, and the exit line says them |
| the pads gate ([LOCKSTEP.md](LOCKSTEP.md): a retrace held until its pads are known, a remote player's in a session) | the network library's rule ([NETWORK.md](NETWORK.md) 1, `net_pace_adjust_ns`): pads that came more than half a retrace after the retrace's wall time re-base (`pc_pace_stall`: the retrace taken as due at their arrival, the next one a retrace after it, never catching up); a smaller lateness stays on the grid, caught up as any lag. The exit line counts both |
| time sync (the network layer, any thread) | `pc_pace_adjust_ns(per_retrace)`: every retrace that much longer or shorter in wall time until changed, at most 1 ms; `pc_pace_shift_ns(ns)`: once. `pc_pace_info` gives the last retrace that went out, the wall time it was due and the time its move came, the next one's due time, the lag, the counts |
| the vblank lock (`vb_lock`, FRAMERATE.md 2) | where real time moves the retraces to sit between two vblanks, paced mode shifts the epoch: the retraces are N64 time |

**For a session** (port/net's clock contract, `net.h`): `pc_pace_set_start(net_start_time)` (both on `pc_real_ns`'s
clock), then each retrace, once `net_pads` gave its pads, `pc_pace_adjust_ns(net_pace_adjust_ns(s))` (0 to 1 ms, the
interval before the next retrace); the re-base after a stall is the loop's own (above).

`RUSH_PACE_SPEED=S` (tests) runs N64 time S times as fast as the wall clock (`epoch + v / S`). `RUSH_SIM_HOLD=VI:MS`
(tests, any mode) holds the game at retrace VI for MS of wall time, as the pause key does (`hold()`).

**The two meanings of the virtual clock.** `pc_time_virtual()` meant both "the N64 side is deterministic" and "nothing
is real time". It now means the first; `pc_time_free()` (virtual and not paced: `--fast` alone) the second;
`pc_time_paced()` paced mode. Each site:

| Site | Meaning | Paced mode |
|---|---|---|
| `rcp.c` `osSpTaskStartGo` (audio tasks at once), `osSpTaskYield`, `ai_pulled`, `RUSH_SIM_GFX_MS` | the N64 side | as `--fast`: on the main loop, no yields, AI events from the main loop, the RDP's hold in N64 time |
| `sched.c` `idle_changed`, the busy-wait's nap | the N64 side | as `--fast` (the nap paced) |
| `sched.c` `pc_os_wait_main` | the N64 side | the paced wait above |
| `si.c` `pc_si_flush` (waits for the pak writes) | the N64 side: a write's result at the same point of every run | as `--fast`: the wait costs wall time only |
| `selftest.c` (yields, the device-paced AI) | the N64 side | as `--fast`; the clock's own check is paced: 2 s of N64 time in 2 s |
| `loop.c` the deadline, timers, RSP tasks, AI, the retrace's event and pads | the N64 side | as `--fast`, after a wait that returned with every N64 thread waiting |
| `loop.c` `pc_os_quiet_mark`, `vi_late_cap`, the late and dropped retraces | real time's catch-up | never: the virtual clock doesn't pass a retrace (the pacing catches up instead) |
| `loop.c` `sim_swap`, `sim_hold`, `display_timing` (the vsync and present models) | nothing real time | real time: a present's time is wall time only |
| `loop.c` `pc_gfx_auto_frame_rate`'s vsync, `pc_gfx_set_own_paced`, the stale frame shown at its retrace | nothing real time | real time (render-only) |
| `loop.c` the frames between (`video.frame_rate`), paced by vblanks or not | nothing real time (drawn when the loop passes, not woken for) | real time on the wall clock: the retraces' times are mapped to the wall (`pc_pace_wall_of`); their wakes are the presentation's own (`render_wake`), never deadlines of the N64 clock; a wait that ends for one returns 2 and the pass does no N64 work. A tick that finds an RSP task queued waits for the N64 pass (when every thread waits), with no wake of its own. "The host is behind" (no frames between) is a retrace a retrace late on the pacing |
| `loop.c` the 30 s hang check | `--fast`: no retrace for 30 s | the waits have seen an N64 thread compute without a wait for 30 s (`pc_os_busy_ns`): holds, gates and the pacing take wall time legitimately |
| `loop.c` the wait's lateness (`wake_late`, the vblank pacing's) | real time's timed wait | the paced wait's timed wakes; but paced mode never takes the coarse waits' way of pacing ticks by the vblanks (each drawn as soon as the last swap returns, the swap the clock): it keeps the loop in swaps, and the N64 side moves only in the loop's waits, one move a refresh (on the Pi 5, whose waits return 2 ms late now and then, the race ran at 17 retraces a second). With coarse waits and a vsync swap, paced mode's frames between mostly miss their vblank and the game's own are shown; the game keeps its pace |

So a vsync swap that blocks, frames drawn between, `RUSH_SIM_PRESENT_MS` and `RUSH_SIM_VSYNC_HZ` cost paced mode wall
time only: the main loop does no N64 work meanwhile and the clock doesn't move. A host too slow for the pace makes the
game slower in wall time (re-based), never different.

**Audio** ([AUDIO.md](AUDIO.md) 11). The AI's queue is the null output's on the N64 clock whatever the backend, so its
events are `--fast`'s; a device is fed from a tap, a FIFO of about 50 ms whose converter is nudged to hold its level.

**The exit lines.** `paced: N retraces in W s of wall time, R a second (P without the T s held or re-based); H holds
(...), B re-based after a lag (... the longest ...), C retraces caught up a lag of over 2 ms (the most ...); time sync:
...` and `paced: the host's work a retrace (...)`: the wall time between two retraces less what the main thread slept
with every N64 thread waiting, its mean, median, 99th percentile and most, against 16.67 ms: the host's headroom.
Each re-based lag (the first 20) is said as it happens, `paced: a lag of L ms re-based after retrace N (N64 time ...):
WHOSE (busy B ms of the S since that retrace)`, and at exit `paced: of the re-based lags, K the host's work (...), J
waits that ran long (...)`: the host's work when the time since the last retrace less the main thread's sleep is half
the lag or more (the game, the renderer, a host that gave the process no CPU; before the first retrace, the start's),
else a wait that ran long (a timer's late wake). `paced_check.py` sets the host's work aside (up to 8 lags): the Windows
runner's software renderer (Mesa's llvmpipe, no shader cache of its own there) builds each new shader at its first
draw, 40-50 ms, six times a race at the same retraces (2, 484, 1024, 1412, 1416, 1472 of `race`, ba3a8c0's run
42317924327); the renderer says a shader slow to build (`gfx: a new shader took ...`, over 8 ms) where a driver
compiles at the link, and at exit how many it built.

**The proof** (2026-11-06). The paths: `synth_compare.py --exe A --exe A --env-b RUSH_PACED=1` (the program against
itself, `--fast` against paced; `--args-b` gives the second run options of its own, `--link` the mods paths their
mods). The real-time checks: `tools/port/paced_check.py` (`--fast` with the null output, then paced; the trace and the
WAV compared, the exit lines judged).

| Run | Result |
|---|---|
| every path on the virtual clock (62; `pcsetup` is the host's clock only), RelWithDebInfo, rocky, at the real pace, 8 at once (the mods paths with their mods, `--link`) | the same trace, every path: 201,792 retraces in 3,363 s of wall time, every run 60.000 a second, none re-based, 1 retrace caught up a lag (18.9 ms); the host's work a retrace 0.38-0.92 ms on average, 99% under 8.75 ms on every path |
| the same at `RUSH_PACE_SPEED=4`, Debug, 6 at once | the same, every path (CTest `paced_same_<path>`) |
| `drive`, `race2p` under `RUSH_SIM_PRESENT_MS=40`, `RUSH_SIM_TIMER_STEP_US=15625`, `RUSH_SIM_VSYNC_HZ=59.94` with `video.frame_rate = 120`, `RUSH_SIM_GFX_MS=40` | the same as `--fast`'s under each (`paced_model_*`; the RDP's model changes `--fast`'s course too, as NETPLAY.md E4 found, and paced mode takes that one) |
| `race`, real pace (`paced_realtime`) | the trace and the WAV `--fast`'s; 2161 retraces in 36.017 s, 60.000 a second, none re-based |
| `race`, each present held 40 ms (`paced_slow_host`: a host too slow for the pace) | the same trace and sound; the game slowed to 29 retraces a second of wall time (74.6 s for 36 s of game time), 965 lags re-based |
| `drive` at 4 times the pace beside 4 busy processes (`paced_load`) | the same trace |
| `race` on the Pi 5 beside 4 busy processes (one a core), and the same on real time's clock for contrast (`paced_check.py --load 4 --realtime`) | paced: `--fast`'s trace, 59.88 retraces a second (2 lags re-based, 127 caught up); real time: not `--fast`'s from its third event |
| `race` held 3 s at retrace 600 (`paced_hold`), stopped whole 2 s by SIGSTOP (`paced_stop`) | the course `--fast`'s without either; rocky: 3.004 s held, 1.997 s re-based, the rest 60.000 a second, no lag past 3.3 ms after them (no burst) |
| `race` with SDL's disk driver fed from the tap (`paced_audio`) | the trace and the WAV `--fast`'s with the null output; the FIFO 35-67 ms, mean 50.9 against 50; the converter nudged -0.77% on average (the disk driver's slowness); no underrun, nothing dropped |
| `race` with the timed waits in Windows' coarse steps (`paced_coarse`) | rocky and the Pi: 60.002 a second, none re-based, about 1860 retraces caught up from a step (the most 17.5 and 29.5 ms) |
| the presentation's models, paced (`realtime_check.py --paced`): a 59.94 Hz vsync swap with frames between at display and 120, precise waits (`paced_vsync`); the Windows desktop's model, the vsync swap and coarse waits with the game's own frames (`paced_desktop`) | rocky: 60.000 a second, 2.6% and 2.5% of the vblanks lost to skipped ticks, 674 and 666 frames drawn between; the Pi: 60.000, 4.9% and 3.6% lost, 295 and 298 drawn (real time on the same Pi: 150 and 0); the desktop model 60.000 a second (rocky) |
| `race2p` with the pads gate holding retraces at random, up to 40 ms (`RUSH_SIM_GATE=7,0.05,40`, the paced run alone; `paced_gate`) | the trace, the state hash at all 2763 retraces (47 with the window) and the sound `--fast`'s without the gate; the Pi: 146 retraces waited, 119 of them over half a retrace and re-based (2.8 s), 27 kept on the grid; the rest 60.000 a second |
| every path, paced at `RUSH_PACE_SPEED=4` with the gate (`RUSH_SIM_GATE=11,0.05,20`), against `--fast` without it, rocky | the same, every path; 10,826 waits, 9,873 of them re-based, 953 kept on the grid |
| `race` with `audio.latency = 100` and SDL's disk driver fed from the tap, against `--fast`'s null output at 0 (`paced_latency`) | the trace and the state hash every retrace, the window every 60, the same: the AI's queue doesn't depend on the latency or the device in paced mode (with `--fast` the same change makes the window differ from retrace 0) |
| `--selftest --paced` (`ultra_selftest_paced`) | 40 of 40; a 2 s wait in 2 s of wall time |
| a 10-minute run (`idle.press`, `--seconds 600`), RelWithDebInfo, rocky, SDL's disk driver fed from the tap, `-v` | 36,000 retraces in 600.000 s of wall time, 60.000 a second, none re-based or caught up (the most lag 6.0 ms); the tap's FIFO 31.4/50.0/64.1 ms (least/mean/most) against 50, nudged -0.66% on average (-1.86% to +0.03%), no underrun, nothing dropped |
| default off: every path, the build before (f9a7177, and after merging main, 543e5b0) against this one, RelWithDebInfo, 2 runs each | the same, every path |
| the merged tree (main's pads gate, state hash and port/net): every path at the real pace again; the whole suite, RelWithDebInfo | 62 of 62 the same, every run 60.000 a second, none re-based; 540 of 540 tests pass |

On real time's clock the same race differs from `--fast`'s even on an idle host ([NETPLAY.md](NETPLAY.md) E7). The
work a retrace, the synthetic races headless (`race`, `race2p`, `race12`): rocky (Debug) 0.68 ms of
16.67 on average, 99% under 4.5 ms; the Pi 5 (RelWithDebInfo) 0.65-0.75 ms, 99% under 4.25-5 ms (a game frame, two
retraces at original, twice that). The retail
cartridge's numbers stay off the repository.

**Not done here.** The session's wiring (`port/net` into the loop, NETWORK.md 10) uses the API above. The pause key, focus loss and the F1 overlay still hold the game (re-based); a session will disable
them. While a hold stops the clock, port/input's UI timers that read `pc_host_ns()` (the overlay's key-capture
timeout, menu repeat, a status line's fade, the cursor's hide) stand still too, as with `--fast`; a session's overlay
won't hold the game, and otherwise they want a clock of their own. Without SDL's audio (headless builds) the tap is
never on: `--audio null` is `--fast`'s null output.
