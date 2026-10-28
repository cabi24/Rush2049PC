# Windows x86-64 (mingw-w64)

The PC build on 64-bit Windows, compiled with mingw-w64's GCC, cross from Linux or natively in MSYS2. Item 5 of
[PORT_PLAN.md](../PORT_PLAN.md)'s "Next". Written 2026-10-27, without a Windows machine: the cross build was checked
here (a Pi 5, Debian 12's mingw-w64 GCC 12.2, binutils 2.40), and the programs ran only in CI (section 9).

## 1. Building and running

**Cross from Linux.** On Debian or Ubuntu, install `gcc-mingw-w64-x86-64-posix`, `binutils-mingw-w64-x86-64`,
`mingw-w64-x86-64-dev` and `libz-mingw-w64-dev`, or the `mingw-w64` metapackage. `RUSH_FETCH_SDL2` also needs
`g++-mingw-w64-x86-64-posix`. Then:

```
cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake [-DRUSH_FETCH_SDL2=ON]
cmake --build build/win
```

- `cmake/mingw-w64.cmake` takes `x86_64-w64-mingw32-gcc-posix` (winpthreads) when it exists, and the matching nm,
  objdump and windres.
- Libraries are looked up in the mingw-w64 sysroot and in `RUSH_MINGW_ROOTS`, a list of extra prefixes (for a zlib
  built for Windows elsewhere: `-DRUSH_MINGW_ROOTS=/path/to/prefix`, holding `include/zlib.h` and `lib/libz.a`).
- `-DRUSH_FETCH_SDL2=ON` builds SDL2 2.30.9 from source and links it statically. Without SDL2 the build is headless
  only, as on Linux.

**Native, in MSYS2** (the UCRT64 shell):

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-zlib [mingw-w64-ucrt-x86_64-SDL2]
cmake -S . -B build/pc -G Ninja && cmake --build build/pc && ctest --test-dir build/pc
```

**The program.** `rush2049pc.exe` is a GUI program: no console window opens with it, and from cmd.exe or
PowerShell it prints into their window (section 10). It takes the same options as on Linux ([ULTRA.md](ULTRA.md)
§4).

- zlib and the MinGW runtime are linked in. An MSYS2 build with MSYS2's SDL2 also needs `SDL2.dll`.
- Settings: `rush2049pc.ini` next to the program (a portable install), else `%APPDATA%\rush2049pc\rush2049pc.ini`.
  `XDG_CONFIG_HOME` and `HOME` are not used. The controller pak, screenshots and `rush2049pc.log` go next to the
  program when the ini is there, else into `%APPDATA%\rush2049pc` ([RELEASE.md](RELEASE.md) "Files").
- A double-clicked program has no console to show what it prints: an error or a fault shows a message box with
  what went wrong and the log's path (section 10, [RELEASE.md](RELEASE.md) "The log"). The log's first lines name
  the commit and the Actions run, so its `rush2049pc.exe+0x...` frames can be symbolized with that run's exe.
- The executable has an icon and version information (section 10): Explorer's Properties, Details tab, shows the
  version as `--version` gives it.
- A ROM dropped on `rush2049pc.exe` arrives as its first argument, which the program takes for `--rom`.
- **Ctrl-Break** prints the N64 threads' states (Ctrl-\ on Linux). Ctrl-C quits.
- **Headless rendering** needs an OpenGL 3 driver. With a GPU driver installed there is one. Windows' own renderer
  is GL 1.1. Without a GPU, Mesa's `opengl32.dll` next to the program gives llvmpipe (mesa-dist-win, `x64/*.dll`).
- **Faults** print the exception, the faulting address, the N64 thread and a backtrace, then exit with the
  exception code (no Windows Error Reporting dialog). Frames are symbolized when `addr2line` is on PATH (MSYS2's
  binutils). Otherwise they print as `rush2049pc.exe+0x...`. On the build machine,
  `x86_64-w64-mingw32-addr2line -f -e rush2049pc.exe` names that offset plus 0x140000000, the link-time image
  base.

## 2. The RDRAM window

As on Linux ([ULTRA.md](ULTRA.md) §2), 8 MiB at host 0x80000000 (KSEG0) and the same memory at 0xA0000000 (KSEG1):

- `CreateFileMappingW(INVALID_HANDLE_VALUE, ...)` makes a pagefile-backed section, and two `MapViewOfFileEx` calls
  view it at the fixed addresses (`port/ultra/memory.c`). A fixed-address view fails rather than moving when the
  range is taken.
- The window is required, as on Linux: the game's data lives there.
- The test programs that map a window of their own use anonymous mappings. `port/include/win/sys/mman.h` gives
  them `mmap`/`munmap`/`mprotect` on `VirtualAlloc`, with `MAP_FIXED_NOREPLACE`'s meaning. It is on the include path
  of Windows builds only.

## 3. Absolute symbols in a PE image

The game and port/data use about 1,800 names at absolute N64 addresses: port/data's arenas and `aliases.ld`'s
low-memory globals and cartridge offsets ([DATA.md](DATA.md) §3). On Linux, code reaches them through the GOT, and
`absfix.c` puts the slots back after the dynamic loader relocates them.

**The image stays above 4 GiB.** The port reads every value below 4 GiB as an N64 address:

- the renderer's address rule ([GFX.md](GFX.md), "Addresses");
- the audio interpreter's native-form pointers (`port/audio/musyx.c`);
- port/data's check that own storage is high (`pc_data.c`);
- the PC-only tests in `lib_11640.c` that tell a host pointer by `>> 32`;
- the self-test's "host addresses above 4 GiB".

So the executable keeps mingw-w64's default image base, 0x140000000, and high-entropy ASLR (`--dynamicbase`,
`--high-entropy-va`). High-entropy ASLR is also what normally puts the heap and the thread stacks above 4 GiB. Not
always: under gdb the heap, the main stack and an N64 thread's stack started at a few MiB. So:

- once `pc_mem_init` has mapped the window, it reserves (without committing) every free range below 4 GiB. What
  the process maps after that, the N64 threads' stacks and new heap segments among them, lands above 4 GiB;
- what came before `main` can't be moved, so `pc_mem_init` warns when the heap or the main stack is low. That
  happens under gdb. The main thread runs the renderer, not game code.

A low fixed image base, which would let PC-relative code reach the window, was rejected. Code, static data and
own storage would all sit below 4 GiB, where the rules above take them for N64 addresses. An image that
doesn't opt into ASLR also gets its heap and stacks at low addresses.

**Code reaches externs through `.refptr` slots.** For x86-64 PE targets, GCC's default code model (medium, as PIC)
loads the address of every variable declared `extern` from an 8-byte `.refptr.X` slot, which the linker fills
with an `ADDR64` relocation.

- For an absolute symbol, the slot holds the N64 address itself.
- ld emits no base relocation for a reference to an absolute symbol, so the slot keeps its value when the loader
  moves the image. In the linked program: 1,805 slots of absolute symbols, all holding their value, none with a
  base relocation; 569 slots of relocatable symbols, all with one.
- Initialized pointers to absolute symbols are the same.
- `absfix.c` has nothing to do on Windows.
- `-mcmodel=large` was not needed. It would not have helped either: on PE it still reaches a file's own
  definitions PC-relatively.

**What a slot doesn't cover: a file's own definitions.** GCC reaches a variable its file defines, even a
tentative definition (a common), PC-relatively (`REL32`), which can't reach 0x80xxxxxx from above 4 GiB. Six
window symbols were tentatively defined in eleven game files. They are declarations on the PC now
(`PC_ONLY(extern)`, as `audio_occlusion.c` already did for alignment), and their sizes are in
`port/data/extents.txt`:

- D_80110680
- D_80118E20 (gJust)
- D_8012E688
- D_80146180
- D_80149AF8
- D_8014A250

A new one shows up as a link error, `relocation truncated to fit: IMAGE_REL_AMD64_REL32 against symbol 'D_...'
defined in *ABS* section`, and gets the same fix.

**Weak references.** data.c reaches the game's functions and C-defined data through weak references, which are
NULL when nothing defines the symbol ([DATA.md](DATA.md) §4). On PE, ld 2.40 rejects an undefined weak reference
outright, and one resolved to a `PROVIDE(x = 0)` can get a base relocation, which makes it nonzero once the image
moves. So on Windows:

- data.c references `pc_weak.<name>` instead;
- `gen_data.py --weak-script` writes `pc-data/weak.ld`, with lines `PROVIDE(pc_weak.X = DEFINED(X) ? X : 0);`;
- a missing symbol is an absolute 0, with no base relocation;
- `DEFINED` doesn't pull archive members, as an ELF weak reference doesn't.

The 85 names that come out 0 are the same as Linux's undefined weak references.

**Weak definitions.** gen_weak's `#pragma weak` copies ([PORT_PLAN.md](../PORT_PLAN.md), Phase 0) work on PE: a
strong definition beats the weak ones, and calls inside the copying file reach the strong one. Some details
differ:

- GNU as keeps a weak definition's body under `.weak.<name>.<a global the file defines>`, the first it meets. A
  file whose globals are all weak copies got `.weak.<name>.` in every such file, and ld reported them as multiple
  definitions. `gen_weak.py --pe` adds `__attribute__((no_reorder)) char pc_weak_anchor_<file> = 0;` to each header
  that has pragmas. `no_reorder` keeps it first in an optimized build too, where GCC reorders top-level definitions:
  without it, the RelWithDebInfo build's two copies of `path_D169C_path.c` both named their bodies after the
  common `D_80152818` ([OPTIMIZED.md](OPTIMIZED.md)).
- nm shows a PE weak definition as `w`, as it shows an undefined weak reference. `n64syms.py` counts it as `W`
  when its `.weak.` body is in a section.
- gen_weak skips the compiler's `.refptr.*` names.

**The rest of the generators.**

- *DWARF.* gen_data reads the DWARF with objdump (`--dwarf=info`), since readelf reads only ELF. The DIEs print in
  the same form.
- *Pointer-sized typedefs.* On Windows, `uintptr_t` and `intptr_t` are `long long`. They now count as pointer-sized
  (4 bytes on the N64) by name, with `size_t` and `ptrdiff_t`. Before that, `Gfx` lists and every `intptr_t`
  symbol had the wrong N64 layout.
- *Same result.* gen_data then decides exactly as on Linux: 1,777 arena symbols, 389 own, 46 inside own storage,
  the same `data_symbols.txt`.
- *Thunks.* The thunks are naked functions, since COFF has no `.previous`.
- *Checks.* `gen_data.py --check --pe` skips symbol sizes, which COFF doesn't have, and checks that nothing is
  linked inside the window or its KSEG1 alias.
- *zcfree.* The game's deflate code defines `zcfree`. On Linux the shared zlib's is hidden, but a static zlib's is
  global, so on Windows the game's is compiled as `n64_zcfree`.

## 4. LLP64 and the calling convention

`long` is 32 bits on Windows, as on the N64.

- *Plain game code.* Game code that keeps N64 semantics in `long` is closer to the N64 there.
- *Data layouts.* These come from DWARF, so `long` fields simply stay 4 bytes.
- *Pointer-sized `unsigned long`.* PC-only game code that held a pointer in an `unsigned long` truncated it. Four
  places now use `uintptr_t`, with the N64's tokens unchanged:
  - `lib_15a10.c`'s `func_8001558C`/`func_800156E8`;
  - `lib_11640.c`'s two `>> 32` tests;
  - `rom_tu.h`'s `pc_mem_register_be` declaration;
  - overlay B's `(unsigned long)&D_8002EB94`, through `N64_ULONG`.
- *How they were found.* Diffing the `-Wpointer-to-int-cast`, `-Wint-to-pointer-cast` and `-Wshift-count-overflow`
  warnings of a full Windows game build against Linux's found these four, and one more: `lib_11640.c:1719`
  masks a pointer's low word, which the PC-only test before it bounds.
- *Implicit conversions.* Pointer-to-`long` conversions without a cast (`-Wno-int-conversion`) aren't in that
  diff. A grep of the PC-only code for `long` found no others.
- *The PC layer.* It uses `size_t` and `uintptr_t`.
- *Formats.* On mingw-w64, a `printf` format attribute checks Microsoft's formats, so `pc_log` uses
  `__MINGW_PRINTF_FORMAT`; its printf family takes C99's `%zu`.

**Stack arguments.** The Microsoft x64 convention passes the first four arguments in registers, whatever their
class, and every later one in an 8-byte stack slot. Linux passes six integer arguments in registers (x86-64; eight
on aarch64).

- *What differs.* A caller whose prototype says `s32` stores 4 bytes of the slot (`movl $0, 0x20(%rsp)`). A
  definition that reads a pointer there gets the upper half of whatever the slot held. In a register a 32-bit move
  zero-extends, so on Linux a constant 0 for a pointer arrives as NULL, and [PROTOTYPES.md](PROTOTYPES.md) counted
  such views harmless. `char` and `short` arguments are extended and stored as 4 bytes, so they are fine.
- *What it broke.* Every synthetic boot faulted 0.4 s in, when `playgame_state_change` entered state 0x2:
  `resource_slot_clear` calls `audio_frame_sync(id, 0, 0, 0, 0)` (LoadWave) through an all-`s32` prototype, and
  the definition's fifth parameter is the heap. It read 0x00007FF6_00000000, the upper half of a host address left
  in the slot, and `audio_helper` faulted reading the heap's first block at +8. The boot without a ROM stops before
  that.
- *The check.* `tools/port/proto_check.py --abi win64` applies the convention to a Linux build's DWARF and reports
  such views as `stack` ([PROTOTYPES.md](PROTOTYPES.md) 3.6). It found 20 views of three functions, all passing a
  constant 0: LoadWave's heap (5 views), InitPane's (`ambient_sound_set`) sixth argument, the pane's animation (14),
  and one weak copy of `input_process_controller` whose only callers are matching stand-ins nothing calls. The first
  two pass a pointer-sized argument on the PC now. Nothing harmful is left.
- *Not checked.* Calls through function pointers, which proto_check doesn't see. The 14 synthetic paths (the front
  end, races for 1-4 players, circuit, stunt, battle, the demo, the pause screens, the end of a race, a pak) run
  without another fault.

## 5. Threads and time

- **N64 threads** are winpthreads threads, as they are pthreads on Linux (`sched.c` unchanged in design).
- **Timed waits don't use winpthreads' timeouts.** winpthreads judges a timed wait's deadline by
  `GetTickCount64`, which moves in 15.6 ms steps whatever the timer period. That is too coarse for a 60 Hz retrace
  and the 1 ms busy-wait naps. When the scheduler used them:
  - the self-test's lower-writer busy-wait took 3.1 s, 200 naps of 15.6 ms;
  - a 100 ms OS timer fired 24 ms late.

  Windows' own timeouts follow the timer period. The loop asks for 1 ms (`timeBeginPeriod(1)`, with the opt-out from
  Windows 11's power throttling of timer requests, `SetProcessInformation` with
  `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION`). A probe on the runner (4 CPUs) measured:

  | Wait | Default period | After `timeBeginPeriod(1)` |
  |---|---|---|
  | `WaitForSingleObject`, 1 ms timeout | 10-11 ms | 1.3 ms |
  | `Sleep(1)` | 9-13 ms | 2.0 ms |
  | `pthread_cond_timedwait`, `nanosleep`, 1 ms | 15-16 ms | 15-16 ms |
  | high-resolution waitable timer, 1 ms | 1.5 ms | 1.5 ms |

  A condition variable round trip between two threads took 18-25 µs, and `SetEvent` woke a waiting thread in
  9-13 µs.
- **The two timed waits** (`sched.c`) use a high-resolution waitable timer instead (`CreateWaitableTimerExW` with
  `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, Windows 10 1803 and later; a plain one before):
  - a busy-wait naps on it with the scheduler lock released, and an event that comes meanwhile is seen when the
    nap ends, at most 1 ms later;
  - the main thread waits for it or for its auto-reset wake-up event.

  Everything else is the same condition variables as on Linux.
- **The virtual clock** is as fast as on Linux. On the runner the self-test with `--fast` once reported 687 ms of N64
  time in 8 s of real time. With a timestamp on each check, the scheduler's part of that run took 4 ms. The other
  8 s went to the AI test's first `osAiSetFrequency`, which opened the default audio output: with SDL2 in the build
  that is SDL's audio initialization, which on a machine without an audio endpoint takes 8 s before it fails and
  the null output takes over. (The self-test in real time lost the same 8 s: 9 s instead of 0.8.) The self-test
  uses the null output now, unless `RUSH2049_AUDIO` names another. The game opens SDL's audio only with
  `audio = auto` or `sdl`; CI's runs pass `--audio null`.
- **Clocks.** `clock_gettime(CLOCK_MONOTONIC)` is winpthreads' (QueryPerformanceCounter, 100 ns steps). So is
  `nanosleep`, in the 15.6 ms steps above; only the pause's hold loop (`loop.c`) uses it.
- **Signals.** Windows has no SIGQUIT. Ctrl-Break (SIGBREAK) prints the thread dump. Ctrl-C quits.

## 6. Faults

`port/ultra/win32.c` stands in for misc.c's signal handler and backtrace.

- **Catching.** `SetUnhandledExceptionFilter` gets every exception no handler took, in any thread.
- **Unwinding.** From the exception's own context, with `RtlLookupFunctionEntry`/`RtlVirtualUnwind` and the image's
  `.pdata`. GCC writes unwind tables for every function. A frame without one is treated as a leaf, and so is a
  call to address 0.
- **The report.** It prints the exception (for an access violation, read, write or execute, and the data address),
  the N64 thread and the frames, then ends the process with the exception code. `SetErrorMode` keeps Windows
  Error Reporting's dialog away.
- **Symbols.** Frames in the executable are symbolized with `addr2line -f -p -e rush2049pc.exe -j .text`, by
  their offset in `.text`. That offset doesn't move with the image.
- **Stack overflow.** `SetThreadStackGuarantee` leaves each thread 64 KiB for the filter.

## 7. Graphics, audio, input

- **Headless GL** is WGL on a hidden window (`port/gfx/gfx_wgl.c`, in place of `gfx_egl.c`; it keeps EGL's entry
  points). It tries:
  1. desktop GL 3.3 core;
  2. desktop GL 3.1;
  3. GLES 3.0, when the driver offers ES profiles (`WGL_EXT_create_context_es2_profile`).

  With `RUSH_GFX_API=gles` (`video.api = gles`) GLES comes first. The window (SDL) takes the same order. `auto`, the
  default, is desktop GL on Windows since 2026-10-27 (GLES before): it is what Windows GL drivers are made and
  tested for, and their ES profile is a side path. GL 1.1 entry points come from `opengl32.dll` itself. Rendering
  goes to framebuffer objects, as with EGL.
- **GL declarations.** mingw-w64 has no `GLES3/gl3.h`. `gfx_gl.c` takes `GL/glcorearb.h` there, which has the same
  names and no prototypes.
- **SDL2** is optional, as on Linux: window, keyboard, controllers and audio. Without it the build is headless with
  null/WAV audio and scripted input. ALSA is Linux only.
- **SDL test driver.** SDL's offscreen video driver makes its GL contexts through EGL, so the SDL window tests use
  SDL's `windows` driver on Windows, and open real windows.
- **Window readback.** `gfx.all.sdl-window` runs every renderer case through the window there, but with
  `--no-window-readback`. Its window comparison reads the back buffer after the swap, which GL leaves undefined.
  SDL's offscreen driver keeps it; WGL with llvmpipe gives an earlier frame.

## 8. Files and paths

- `pc_config_exe_dir` uses `GetModuleFileNameA`. Paths take either separator.
- The config directory is created component by component. Making a drive or an existing directory fails with
  EACCES there, so those are skipped.
- `rename` fails on Windows when the target exists. The `.mpk` and the settings file are replaced with
  `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING)`, through `pc_rename_replace`.
- `port/include/pc_compat.h` has the rest of what mingw-w64 lacks: `setenv`, `unsetenv` (the CRT's environment)
  and a two-argument `mkdir`.

## 9. CI

`.github/workflows/pc.yml`:

| Job | Runner | What |
|---|---|---|
| `windows-cross` | ubuntu-24.04, in `debian:12` | the toolchain file, Debian 12's mingw-w64 (GCC 12.2) and zlib, SDL2 built from source; uploads `rush2049pc.exe` |
| `windows-run` | windows-latest | that executable: `--selftest`, the boot without a ROM (`grep "wave 0 loaded"`), and the synthetic cartridge's paths ([SYNTH.md](SYNTH.md): generated from the cross build's data report, which `windows-cross` uploads with the executable) |
| `windows` | windows-latest | MSYS2 UCRT64 with its SDL2: the build, `ctest` (with `ultra_selftest_fast` and the synthetic cartridge's `synth_boot_*`), `--selftest` and the boot without a ROM |

- **OpenGL.** The runners have no GPU. Mesa 26.2.4's llvmpipe (mesa-dist-win, SHA-256 pinned, `GALLIUM_DRIVER=llvmpipe`)
  is copied next to the programs.
- **Retries.** A failing test is run again under gdb.
- **The other jobs.** The Linux and n64-neutral jobs are unchanged.

**Results** (run 40558632666, 2026-10-27; all six jobs pass):

- `windows`: 114 of 114 tests in 127 s.
  - The self-test: 0.9 s, and 0.04 s on the virtual clock.
  - The 14 synthetic paths reach their states, the same as on Linux (players, windows, cars, mode, track; the
    frame counts within a few).
- `windows-run`: the self-test, the boot without a ROM and the 14 paths with the cross-built program. The paths on
  the virtual clock take 1.1 to 6.0 s each, and the real-time race 36 s. The Linux runners are in the same range:
  in the same run 0.6 to 3.3 s (x86-64) and 1.2 to 9.6 s (aarch64), and in run 40551639233 1.0 to 6.4 s (x86-64).

Before the synthetic paths (run 40503877913, 2026-10-27):

- `windows`: 63 of 63 tests, among them:
  - the 21 renderer tests on llvmpipe, headless and in an SDL window;
  - the SDL keyboard, controller, rumble and focus tests;
  - the audio tests, with SDL's disk driver;
  - the data loader on the synthetic ROM;
  - the GSM kernels.
- The self-test passes 26 of 26 in both Windows jobs.
- **The boot without a ROM** goes as on Linux, on both executables (MSYS2's GCC 16.2 and Debian's GCC 12.2):
  - waves 0, 6, 7 and 8 load;
  - then an access violation at address 0xC in `audio_helper`, from `AllocateMemoryHeap` ← `func_800A4CB8` ←
    `car_shadow_render` ← … ← `game_init`, symbolized. That is the blank cartridge's zero data in AudioSfxInit,
    the same fault Linux stops at.

## 10. A Windows program: the console, the icon, the version

`rush2049pc.exe` behaves as a Windows game does: no console window, an icon, version information. Written
2026-10-27; checked on the cross build's PE header (`win_pe`) and, for what CI can see, by `app_firstrun`. Not yet on
a desktop (section 11).

### The console

**A GUI program.** The executable is linked with `-mwindows` (CMake's `WIN32_EXECUTABLE`), so its PE subsystem is
GUI (2), and Windows never makes a console for it. `-DRUSH_WINDOWS_CONSOLE=ON` builds the console program it was
before.

Why not a console program that hides or frees its console when it is the console's only process? Windows makes a
console program's console in `CreateProcess`, from the PE header, before any of the program runs. So:

- the program can only remove the console after it has appeared: a window that flashes up at every launch;
- on Windows 11 the console is usually Windows Terminal's (the default terminal since 22H2). That window is the
  terminal's: `ShowWindow(GetConsoleWindow(), SW_HIDE)` doesn't hide it, and after `FreeConsole` the terminal still
  waits on the program, so its tab stays open until the game ends.

A GUI program never gets a console it didn't ask for, wherever it is started from: Explorer, a shortcut, a launcher.

**Where the output goes.** First thing in `main` (`pc_app_startup`, `port/app/startup.c`), for stdin, stdout and
stderr:

| Started from | Its standard handles | What the program does |
|---|---|---|
| a script, CI, a launcher that redirects (`> file`, a pipe) | files or pipes | nothing: they are used as they are |
| cmd.exe, PowerShell, Windows Terminal | none: a GUI program gets no console | `AttachConsole(ATTACH_PARENT_PROCESS)`, and the streams without a handle are opened on `CONIN$`/`CONOUT$`: the output appears in the prompt's window |
| Explorer (a double-click, a ROM dropped on it), a shortcut | none, and no console to attach to | "quiet": stdin is `NUL`; stdout and stderr go into a pipe whose reader keeps the last 4 KiB |

- A `NUL` the user asked for (`2>nul`) stays: attaching leaves a handle the parent passed alone.
- The C runtime set up its descriptors before `main`, from the handles it had. A stream without one is reopened with
  `freopen` on the descriptor it should have (0, 1 or 2: its slot is closed first, and the lowest free descriptor is
  taken), so that the log's redirection of fd 2 ([RELEASE.md](RELEASE.md) "The log") still gets everything printed
  to `stderr`. msvcrt leaves such a stream on its descriptor with no handle behind it; the UCRT also sets the
  stream's own descriptor to -2.
- `SetStdHandle` then gives Windows the same handles, for the fault handler's `WriteFile` and for SDL.

**The cost:** cmd.exe and PowerShell don't wait for a GUI program. `rush2049pc.exe --help` prints after the prompt
has come back (Enter gives a new prompt), and `%ERRORLEVEL%` or `$LASTEXITCODE` isn't the program's. A batch file
does wait. To wait from a prompt: `start /wait rush2049pc.exe --version` (cmd.exe), `rush2049pc.exe --version |
Out-Host` (PowerShell), or redirect the output (`> out.txt`), which works from anywhere. Ctrl-C and Ctrl-Break
reach an attached program as before (section 5).

**Errors with nobody to see them.** When the program is quiet:

- the log is written as always: `log.c` copies stderr to `rush2049pc.log` and to what fd 2 was, the pipe;
- an error exit (`pc_app_finish` with a nonzero code: no ROM with `--headless`, a command-line or settings error, no
  RDRAM window, ...) that no message box has explained yet shows one, "Rush 2049 PC stopped", with the last lines
  printed (up to 12; those before the usage text, after an unknown option), the log's path and where to report it;
- a fault shows its box ("Rush 2049 PC stopped", the fault's line) with or without a window;
- there is no "press a key" wait: there is no console window to keep open. The wait stays for a console build and
  for `RUSH2049_WAIT_KEY=1`.

`RUSH2049_CONSOLE=none` makes any run quiet, on Linux too; with `RUSH2049_MESSAGE_BOX=print` the boxes are printed to
the real stderr. `app_firstrun` uses it: an error (two ROMs dropped), an unknown option (the box has the error, not
the usage), no ROM with a window (the ROM's own box and no second one, the log written), `--version` (nothing), and on
Windows a fault (its box, the log with the report). It also checks that `--help` and `--print-config` reach a pipe.

**Also:**

- The C runtime's own messages (`abort`, `assert`) go to stderr, `_set_error_mode(_OUT_TO_STDERR)`. A GUI program's
  would be message boxes, which would hold a test run until someone clicks them.
- The fault handler runs `addr2line` with `CreateProcess` and `CREATE_NO_WINDOW` (`win32.c`'s `run_tool`), not
  `_popen`, which runs `cmd.exe`: a console program, which a GUI program without a console would show in a new
  console window.

### Icon and version

- **`port/app/rush2049pc.rc`**, compiled by the toolchain's windres (`enable_language(RC)`):
  - icon 1, `dist/rush2049pc.ico`: `dist/rush2049pc.svg` drawn at 16, 20, 24, 32, 40, 48, 64, 128 and 256 px (the
    small and large icons at 100% to 250% scaling, and 256), each at its own size; BMP below 64 px, PNG from 64;
  - VERSIONINFO: ProductName `Rush2049PC`, FileDescription `Rush2049PC (unofficial PC port)` (Task Manager shows
    it), a comment that it is an unofficial port that holds no game data, FileVersion and ProductVersion as
    `git describe` gives them (the log's version), and FILEVERSION their numbers: `v1.2.0-3-gabc1234` is 1.2.0.3,
    0.0.0.0 before any tag. No company, and none of the game's names.
- `cmake/version.cmake` writes `pc_version_rc.h` next to `pc_version.c` at each build, when it changes.
- **`tools/port/make_icon.py`** makes the .ico and `port/app/icon_data.h` from the SVG, with pycairo. Both are
  committed, so a build needs neither. It draws only what the SVG uses (`rect`, `path` with M, L, H, V and Z, fill,
  stroke) and stops at anything else.
- **The window's icon.** SDL 2.30 registers its window class with the executable's first icon group and leaves the
  small icon to Windows, which picks the size for the DPI. So the window, the taskbar and Alt-Tab get the .ico's own
  images. `SDL_SetWindowIcon` would replace them with one scaled image, so it is used on Linux only
  (`port/app/icon.c`, a 64 px copy).
- **The window's title** ends with the version: "San Francisco Rush 2049 - Rush2049PC v1.0", "(paused)" after the
  name while the game is held.
- **`win_pe`** (`tools/port/pe_check.py`; Python only, so it runs on the host that cross-compiles too): the subsystem,
  the image base 0x140000000 with high-entropy ASLR (section 3), the icon's nine sizes, and the version strings and
  numbers against the build's `pc_version_rc.h`.

### The process

- **The timer period**: `timeBeginPeriod(1)` at startup, for the whole run. The main loop's own request (section 5)
  stays; requests nest. Since Windows 10 2004 the period is the process's own, and SDL asks for 1 ms as well while it
  is initialized.
- **Power throttling**: `SetProcessInformation(ProcessPowerThrottling)` turns off both the timer-resolution and the
  execution-speed throttling (EcoQoS) for the process, the documented opt-outs, so a headless run, or the game in
  the background with `pause_on_focus_loss` off, keeps its timing. Looked up (Windows 8 and later); a Windows that
  doesn't know the timer flag refuses the pair, and gets execution speed alone. It moved here from the main loop:
  a second call with one flag would hand the other back to the system.
- **Game Mode**: no documented API asks for it (`HasExpandedResources` only reports it). Windows decides, from its
  list of games or the Game Bar's "Remember this is a game". Nothing is done.
- **Alt-Tab in fullscreen.** The port's fullscreen is SDL's desktop fullscreen (a borderless window).
  `SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS` stays at SDL's default, which minimizes only exclusive fullscreen, whose
  display mode has to be put back; a borderless window stays where it is. Whether the game holds meanwhile is
  `video.pause_on_focus_loss` (off by default: it keeps running, controllers too). An exclusive mode, if one comes,
  will minimize on Alt-Tab, as it should.

## 11. Untested

- A real Windows desktop: a GPU driver's WGL contexts (only llvmpipe ran), a visible window, fullscreen,
  vsync, high DPI.
- The console (section 10) from cmd.exe, PowerShell and Windows Terminal, and a double-click's message boxes: CI
  runs the program with redirected output only, and its quiet case through `RUSH2049_CONSOLE=none`. The icon in
  Explorer, the taskbar and Alt-Tab.
- Audio through a real device (SDL's WASAPI/DirectSound). CI uses the null backend and SDL's disk driver. On a
  machine with no audio endpoint, as the runners, `audio = auto` spends about 8 s in SDL's audio initialization
  before it falls back to null (section 5).
- Controllers and rumble (XInput, DirectInput through SDL), and the keyboard on a live window.
- Anything with the ROM, as on Linux.
- Windows on arm64, and 32-bit Windows (the port needs a 64-bit host).
- MSVC and clang-cl (the build relies on GCC's attributes, `-fcommon`, `#pragma weak` and binutils).
- A heap that starts below 4 GiB, as under gdb. Small allocations keep coming from it, so a host pointer the
  converters malloc could read as an N64 address there; `memory.c` warns.
- Third-party DLLs that need low addresses, which the reservation below 4 GiB would refuse (none showed up with
  Mesa, SDL2 or the CRT).
