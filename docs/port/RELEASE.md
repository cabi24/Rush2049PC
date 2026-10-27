# The program around the game: first run, files and releases (port/app)

Written 2026-10-27. How rush2049pc finds the player's ROM and says what is wrong when it can't use it, where it
keeps what it writes, and how the release archives are built. The code is `port/app/` (`pcapp`, linked into the
executable only), `port/ultra/romcheck.c`, and `.github/workflows/release.yml`.

| File | What |
|---|---|
| `port/app/paths.c` | the program's folders (its own, the user data folder, the working directory, portable or not) and the places the ROM is looked for |
| `port/app/rom.c` | the ROM at startup: the lookup, then the console message and the message box when it fails |
| `port/app/alert.c` | message boxes: SDL2's `SDL_ShowSimpleMessageBox`, or Windows' `MessageBoxA` without SDL2 (and for a fault) |
| `port/app/log.c` | the version; the log file; the end of a run: the log emptied, on Windows the wait for a key and the fault's message box |
| `cmake/version.cmake` | the version, the commit and the build, written into `build/.../version/pc_version.c` at each build |
| `port/ultra/romcheck.c` | what a file is: byte order (swapped in place), SHA-1, header, and a sentence saying what it is when it isn't the USA cartridge |
| `port/app/tests/test_app.c` | `app_paths`: the folders and the lookup order, in temporary folders |
| `port/ultra/tests/test_romcheck.c` | `ultra_romcheck`: romcheck.c on images the test makes |
| `tools/port/firstrun_check.py` | `app_firstrun`: the program's own messages for each case below, end to end |
| `.github/workflows/release.yml` | the release archives |

## The ROM

### Where it is looked for

Only these places, in this order. No folder is ever searched.

1. **The command line**: `--rom PATH`, or a path given on its own (`rush2049pc PATH`). That is what Windows passes
   when a ROM is dropped on `rush2049pc.exe`. A relative path is from the working directory. `--set game.rom=PATH`
   counts as the command line too.
2. **`game.rom` in rush2049pc.ini, when it is a full path.**
3. **Otherwise `game.rom`'s name** (default `baserom.us.z64`; a relative path like `roms/rush.z64` works too) in:
   1. the program's folder (next to `rush2049pc` or `rush2049pc.exe`);
   2. the user data folder: `$XDG_DATA_HOME/rush2049pc`, that is `~/.local/share/rush2049pc`, on Linux;
      `%APPDATA%\rush2049pc` on Windows;
   3. the working directory (where earlier builds looked).

   A folder that is two of these (the program run from its own folder) is checked once.

A path from 1 or 2 is final: when nothing is there, the program says so and doesn't look elsewhere. With 3, the
first place where something exists is used, and if that file is no good the program says so rather than trying
the next place.

### What is checked

`romcheck.c`, on the whole file:

| Case | How it is told | Status |
|---|---|---|
| no file | `stat` | `PC_ROM_MISSING` |
| a folder, no permission, a read error | `stat`, `fopen`, `fread` | `PC_ROM_UNREADABLE` |
| an archive: ZIP, 7-Zip, RAR, gzip, bzip2, xz | its first bytes | `PC_ROM_ARCHIVE` |
| empty, under 4 KiB, over 64 MiB, or no N64 header | its size and first two bytes | `PC_ROM_NOT_ROM` |
| another game | the game code at 0x3B (`N` + `RU` for Rush 2049) and the name at 0x20 | `PC_ROM_WRONG_GAME` |
| Rush 2049 of another region or revision | the country code at 0x3E (`E` is the USA), the revision at 0x3F | `PC_ROM_OTHER_VERSION` |
| the USA header with other contents | the SHA-1, and the size: 12 MiB | `PC_ROM_BAD_DUMP` |
| San Francisco Rush 2049 (USA) | SHA-1 `3f99351d7bb61656614bdb2aa1a90cfe55d1922c` | `PC_ROM_OK` |

- **Byte orders.** `.z64` (big-endian), `.v64` (16-bit words swapped) and `.n64` (32-bit words little-endian) are told by
  the first word, whatever the file is called. The image is put into `.z64` order first, so the SHA-1 is the
  `.z64` one in every case.
- **The USA header** is the decompilation's `asm/us/header.s`: name `Rush 2049`, game code `NRUE`, revision 0. The
  SHA-1 was already in the code (`pi.c`, `datacheck.c`, `us.sha1`). The port holds nothing else of the ROM.
- **`game.rom_check = off`** (`--no-rom-check`) runs another game, another version or a bad dump anyway, with a
  warning. Files that aren't N64 ROMs are refused either way.

### What the player sees

A short message on the console. When the program would open a window (not `--headless`; on Linux, with `DISPLAY`
or `WAYLAND_DISPLAY`), the same in a message box too, before any window opens. Then it exits with code 1. On
Windows it would otherwise be easy to miss: a double-clicked program's console closes when it exits.

No ROM, Linux (the box says the same at more length):

```
rush2049pc: no ROM: baserom.us.z64 is in none of these places:
rush2049pc:   /opt/rush2049pc/baserom.us.z64 (next to the program)
rush2049pc:   /home/me/.local/share/rush2049pc/baserom.us.z64 (the user data folder)
rush2049pc:   /home/me/baserom.us.z64 (the working directory)
rush2049pc: copy your San Francisco Rush 2049 (USA) ROM next to the program as baserom.us.z64, or give its path: rush2049pc --rom PATH
```

The box on Windows, titled "Rush 2049 PC: no ROM":

```
This program plays your own copy of the San Francisco Rush 2049 (USA) cartridge, from a ROM file, and none was found.

It looked for baserom.us.z64 in:
    C:\Games\rush2049pc\baserom.us.z64 (next to the program)
    C:\Users\me\AppData\Roaming\rush2049pc\baserom.us.z64 (the user data folder)

Copy your ROM next to the program and name it baserom.us.z64, or drag the ROM file onto rush2049pc.exe. .z64, .v64 and .n64 files all work.

Or set game.rom to its path in
    C:\Users\me\AppData\Roaming\rush2049pc\rush2049pc.ini
```

A path that isn't there:

```
rush2049pc: no ROM at missing.z64 (the path given; the working directory is /home/me): there is no such file
```

The box adds that the path and the file's name are worth checking, and where the program looks without a path.

A file that can't be used, titled "Rush 2049 PC: wrong ROM" in the box:

```
rush2049pc: the ROM /home/me/baserom.us.z64 (the working directory) can't be used: it is another N64 game: "SOME OTHER RACER" (game code NXRE)
rush2049pc: its SHA-1 is 9818f6aa7748f336b7f258bd65f340524f49cc80; San Francisco Rush 2049 (USA) is 3f99351d7bb61656614bdb2aa1a90cfe55d1922c
```

The reason, case by case (after "can't be used: "):

- it is San Francisco Rush 2049 for Europe (game code NRUP), not the USA cartridge
- it is revision 1 of San Francisco Rush 2049 (USA); the port runs the first, revision 0
- its header says San Francisco Rush 2049 (USA), but its contents differ: a bad dump or a modified ROM
- its header says San Francisco Rush 2049 (USA), but it is 4194304 bytes, not 12582912 (12 MiB): a cut, padded or
  bad dump
- it is a ZIP archive, not a ROM: extract the ROM from it first
- it is 2048 bytes, too small for an N64 ROM
- it is empty
- it is not an N64 ROM: it starts with 23 20 6E 6F, not an N64 cartridge's header
- it is a folder, not a file
- it cannot be opened: Permission denied

For another region, another revision and a bad dump, the program adds that `--no-rom-check` runs the file anyway,
and that it will probably not work. A usable ROM gives one line: `rush2049pc: ROM: PATH (WHERE; .z64)`.

### Testing it without a display

- `RUSH2049_MESSAGE_BOX=print` prints the box to stderr instead of showing it, between `rush2049pc: message box:
  TITLE` and `rush2049pc: (end of message box)`. `=off` shows none.
- `app_firstrun` runs the program for each case above: no ROM; `--rom` and a dropped path that don't exist; two
  dropped paths (exit 2); an absolute `game.rom`; another game; Europe; revision 1; a bad dump as `.z64`, `.v64`
  and `.n64`; a cut dump; a short file; an empty one; a text file; a ZIP; a folder; an unreadable file (Linux, not
  root). Then the synthetic cartridge with `--no-rom-check` three ways: dropped, from the user data folder, and as a
  `.v64` in the working directory. Last, the real box call on SDL's dummy video driver with no display, which fails
  and is reported (Linux only: on Windows it would show a real box).
  - Each run has its own temporary folder as the working directory, the user data folder and the settings folder
    (`XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `HOME`, `APPDATA`), with `--no-config`.
  - The files are the synthetic cartridge and copies of it with their header changed. No retail ROM is read.
- `app_paths` checks the lookup order itself in temporary folders: each place alone, the first one winning, a
  name with a space, `--rom` with no fallback, an absolute `game.rom`, the program run from its own folder, a
  portable folder, a relative `XDG_DATA_HOME` (ignored), no `HOME`.
- `ultra_romcheck`: SHA-1 against FIPS 180-1's examples; one image in all three byte orders (the same SHA-1 and
  bytes after the swap); the USA header with other contents and another size; Europe, Japan, an unknown region,
  revision 1; another game, the synthetic cartridge's header, a name that isn't ASCII; a ZIP, a 7-Zip, a gzip, an
  ELF file, an empty file, a short one.

## Files: where the program keeps what it writes

One rule, for an installed copy (the program's folder read-only) and a portable one alike:

- **Portable**: `rush2049pc.ini` is next to the program. Everything the program keeps is in the program's folder.
  The release archives are portable: they ship an empty `rush2049pc.ini`, which the first run fills with the
  defaults and their comments (`fill_portable_ini` in `port/main.c`), as it writes a missing one elsewhere.
- **Installed**: no `rush2049pc.ini` next to the program. The settings file is in the user settings folder,
  as before (`%APPDATA%\rush2049pc`, `$XDG_CONFIG_HOME/rush2049pc`), and the rest in the user data folder:
  `%APPDATA%\rush2049pc` on Windows, `$XDG_DATA_HOME/rush2049pc` (`~/.local/share/rush2049pc`) on Linux. With no
  user folder at all (no `HOME`), the working directory.

That folder is "the data folder" (`PcAppDirs.data`). It is made when it is missing.

| What | Setting | Where |
|---|---|---|
| settings | `--config` | `rush2049pc.ini` next to the program, else the user settings folder (unchanged; FRONTEND.md) |
| saves: the controller pak | `game.pak_file`, `--pak` | `rush2049pc.mpk` in the data folder |
| F12 screenshots | `video.screenshot_dir` | `screenshots/` in the data folder (made when a window opens) |
| the log | `--log`, `--no-log` | `rush2049pc.log` and `rush2049pc.old.log` in the data folder |
| `gamecontrollerdb.txt` | | read next to `rush2049pc.ini` (unchanged) |
| `--screenshot-at`, `--dump-audio` | | as given: these are for scripted runs |
| the data report | `PC_DATA_REPORT` | `build/.../data_report.txt` of a source build, none in a release build |

- **Paths in the settings.** A name alone (`rush2049pc.mpk`, `shots`) is in the data folder; a full path is used
  as it is. A path on the command line (`--pak PATH`, `--set game.pak_file=PATH`) is from the working directory,
  as command-line paths usually are.
- **Saves from earlier builds.** They kept `rush2049pc.mpk` in the working directory. When the data folder has no
  pak and the working directory has one by that name, that one is used, read and written where it is, so no
  save is lost. The console and the log say so: `rush2049pc: saves (the controller pak): /home/me/rush2049pc.mpk,
  in the working directory, where earlier builds kept it`. Moving it into the data folder ends that.
- **The settings file** keeps its earlier locations, so nothing there moves.
- **Each run says where its saves are**: `rush2049pc: saves (the controller pak): PATH`, after the ROM line.

Tested in `app_paths` (a name alone, a full path, the command line; a new pak; an earlier build's pak in the
working directory used where it is, and not when the data folder has one or with `--pak`; the portable folder;
folders made) and `app_firstrun`: the pak written in the user data folder, an earlier build's in the working
directory used and none made, `--pak`, a portable copy (its empty ini filled, the ROM, the pak and the log next to
it, nothing in the user folder), and the screenshots folder with a window (SDL's offscreen driver).

## The log

Every game run writes what it prints into a log file as well as to the console: `rush2049pc.log` in the folder
where the program keeps what it writes (below), and the run before is kept as `rush2049pc.old.log` (`.log`, so
it opens with a double click on Windows). `--log PATH` writes another file, and `--no-log` writes none.
`--help`, `--version`, `--print-config` and `--selftest` write no log.

- **The folder.** Next to the program when `rush2049pc.ini` is there (portable). Otherwise the user data folder:
  `%APPDATA%\rush2049pc` on Windows, `$XDG_DATA_HOME/rush2049pc` (`~/.local/share/rush2049pc`) on Linux. If there
  is no user folder either, the working directory. It is made when it is missing. The console's first line names
  the file: `rush2049pc v1.0-3-gabc1234: the log is C:\Users\me\AppData\Roaming\rush2049pc\rush2049pc.log`.
- **What it starts with:** the version, the commit, the build, the platform and compiler, the time, the program's
  folder, the working directory, portable or installed, and the command line:

  ```
  rush2049pc 590baca
  commit 590baca3964ba93fd186a31cd6390dd7abc3a0fd, GitHub Actions run 40574335614 (cabi24/Rush2049PC)
  Windows x86-64, gcc 12-posix
  started 2026-10-27 21:04:11; program folder C:\Games\rush2049pc; working directory C:\Games\rush2049pc; installed
  command line: C:\Games\rush2049pc\rush2049pc.exe
  ```

  The commit and the Actions run say which build it was, so the `rush2049pc.exe+0x...` frames of a Windows
  backtrace can be symbolized with that run's exe (WINDOWS.md 1). `--version` prints the same first three lines.
- **How.** At the start of a run, stderr (fd 2) becomes the writing end of a pipe, and a thread copies what comes
  out of it to the console (fd 2 as it was) and to the file. So everything printed reaches both, by any part of
  the program or its libraries, `WriteFile` to the standard error handle included (Windows: `SetStdHandle`).
- **The end.** Before the process ends, stderr is put back. That ends the pipe, and the program waits (up to 3 s)
  until the thread has copied everything:
  - at the end of a run and on an early error (`main` → `pc_app_finish`);
  - at `--stop-at-gfx`'s `_exit` (`pc_on_exit`);
  - after a fault's report and backtrace (`pc_on_fault`, from `misc.c`'s signal handler and `win32.c`'s exception
    filter), so the report is in the file. Only calls that are safe in a signal handler are used.
  - `abort()` (`pc_fatal`, the CRT): a SIGABRT handler prints a backtrace, empties the pipe, then aborts.
- **Windows, a double-click.** The console is the program's own and closes when it ends (`GetConsoleProcessList`
  returns 1). Then, after an error exit or a fault, it prints "press a key to close this window" and waits
  (`_getch`). A program started from a command prompt shares the console and doesn't wait. `RUSH2049_WAIT_KEY=1`
  forces the wait (with stdin not a console, it reads a line from stdin), `=0` turns it off.
- **Windows, a fault** with a window also shows a message box, "Rush 2049 PC stopped", with the fault's first
  line, the log's path and where to report it. The box is `MessageBoxA`, not SDL's, since it is shown from inside
  the exception filter.

Tested by `app_firstrun` (`tools/port/firstrun_check.py`): the boot without a ROM, which faults, leaves a log with
the header, the run, the fault line and its backtrace; two runs keep the first as `rush2049pc.old.log`; a copy of
the program with `rush2049pc.ini` next to it logs next to it; `--log` and `--no-log`; `--version`; on Windows the
forced wait for a key. Every synthetic path (`synth_paths.py`) runs with `--log NAME.game.log` and checks that the
log has the state the console reached. `release.yml`'s `windows-check` checks the log of the zip's program after
its boot without a ROM (the fault and backtrace in it), `--version`'s Actions run, and the forced wait.

## Versions

`rush2049pc --version`, the log's first lines and the console's first line give the version: `git describe --tags
--always --dirty` at build time (`v1.0`, `v1.0-3-gabc1234`, `abc1234` before any tag, `-dirty` with uncommitted
changes), the full commit, and "GitHub Actions run N (owner/repo)" or "local build".

- `cmake/version.cmake` runs at every build (the `pc_version` target) and rewrites `pc_version.c` only when the
  text changes, so a build that changes nothing relinks nothing.
- **Without git** (a source archive), `cmake/version-archive.txt` gives them: `.gitattributes` marks it
  `export-subst`, so `git archive` (and GitHub's source downloads) fill in `%(describe:tags=true)` and `%H`.
  Otherwise the version is "unknown" (as in a copy synced by `tools/port/remote.sh`, which has no `.git`).

## Release archives

`.github/workflows/release.yml`. **workflow_dispatch** builds the archives as artifacts. A pushed **tag `v*`**
also makes a **draft** GitHub release with them, which a person checks and publishes. Nobody else pushes tags.

| Job | Runner | What |
|---|---|---|
| `windows` | ubuntu-24.04, in `debian:12` | cross-built as pc.yml's `windows-cross` (mingw-w64 GCC 12.2, SDL2 2.30.9 from source); `cmake --install` into `rush2049pc-<version>-windows-x86_64/`; the zip; checks what is in it |
| `windows-check` | windows-latest | the zip unzipped; from its folder: `--version`, the first run (the empty ini filled, the log next to the program), `--selftest`, the boot without a ROM (the log has its fault and backtrace), the forced wait for a key. Mesa's llvmpipe `opengl32.dll` is copied in for the run, since the runner has no GPU; it is not in the zip |
| `linux` | ubuntu-24.04 and ubuntu-24.04-arm, in `ubuntu:22.04` | x86-64 and aarch64: built with SDL2 2.30.9 from source; `cmake --install` into `rush2049pc-<version>-linux-<arch>/`; the tarball; checks what is in it, its libraries and the newest glibc symbol it needs |
| `linux-check` | the same, in `fedora:40` | the tarball unpacked on another distribution; from its folder: `--version`, `--selftest`, the first run, the boot without a ROM headless on Mesa's llvmpipe with its log |
| `release` | ubuntu-24.04 | tags only: `gh release create --draft` with the three archives |

- **The Windows zip:** `rush2049pc.exe`, `README.txt` (`dist/README-windows.txt`, CRLF line ends) and `LICENSES/`:
  - `SDL2.txt`, from the SDL2 source built;
  - `zlib.txt`, the licence comment of the `zlib.h` compiled against;
  - `mingw-w64.txt` and `gcc-runtime.txt`, Debian's copyright files for the mingw-w64 runtime (CRT, winpthreads)
    and GCC's (libgcc, under the runtime library exception).

  SDL2, zlib and the runtime are linked statically, so the program needs only Windows' own DLLs (`objdump -p`:
  ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32, OLEAUT32, OPENGL32, SETUPAPI, SHELL32, USER32, VERSION,
  WINMM). The executable is the CI build type (Debug) and is not stripped, so its backtraces can be symbolized.
- **The Linux tarballs:** `rush2049pc`, an empty `rush2049pc.ini`, `README.txt` (`dist/README-linux.txt`) and
  `LICENSES/` (`SDL2.txt`, `zlib.txt`).
  - Built in Ubuntu 22.04, so they need glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36 and newer).
  - SDL2 is built with the X11, Wayland, libdecor, PulseAudio, PipeWire, ALSA, udev and D-Bus headers there and
    linked statically. It loads those libraries at run time (SDL's default), so the program runs without them,
    and with a window wherever they are.
  - The program itself links the system's libc, libm, zlib, EGL and GLES (through GLVND or Mesa), and ALSA's
    libasound (port/audio's ALSA output). The job prints the list (`readelf -d`).
  - **No AppImage.** It would need FUSE on the player's system (or an extract step), and it must not carry
    GL, which comes from the player's driver. A folder with one program, which needs only what every desktop
    has, is as portable and simpler.
- **`cmake --install`.** `RUSH_INSTALL_PORTABLE` (the default on Windows, and the release builds) installs one
  folder: the program, an empty `rush2049pc.ini`, `README.txt`, `LICENSES/`. Otherwise the usual layout under the
  prefix:
  - `bin/rush2049pc`;
  - `share/applications/rush2049pc.desktop` (Games; it opens `application/x-n64-rom` files, so a ROM can be
    opened with it) and `share/icons/hicolor/scalable/apps/rush2049pc.svg`, a plain road of our own (no art,
    logo or trademark of the game);
  - `share/doc/rush2049pc/`: README.md, README.txt, FRONTEND.md, RELEASE.md and `LICENSES/`.

  `RUSH_EXTRA_LICENSES` adds licence files as `NAME=FILE` pairs. `RUSH_DATA_REPORT=` (empty) turns off the data
  loader's report, as the release builds do.
- **No ROM data.** The packaging step fails if the zip holds a `.z64`, `.v64`, `.n64`, `.rom` or `.bin`.
- **Registering the workflow.** GitHub dispatches only workflows it has seen. Until release.yml is on the default
  branch, a dispatch on another branch gets HTTP 404. One push of a commit with a temporary `push` trigger, to
  the CI branch only, registers it.

**Results:** release run 40572061761 (workflow_dispatch on `port-release`, commit edc9445): `windows` 1 min 36 s,
then `windows-check` unzipped the zip and passed the self-test (26 of 26) and the boot without a ROM (waves 0, 6, 7,
8, then the blank cartridge's access violation at 0xC, as in pc.yml). pc.yml run 40572065582 on the same commit
passed every job.
