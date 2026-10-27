# Rush 2049 PC

A native PC port of *San Francisco Rush 2049* (N64, USA). It is built from the decompilation below the way the SM64
PC port was built from the SM64 decompilation: the game's C is compiled for the PC, and a PC layer stands in for the
N64 hardware. You bring your own ROM. Its data is read at startup, and nothing derived from it is built into the
program.

**Status: first test builds.** On the retail cartridge the game runs its attract demo, the front end, and races in
every mode tried: one to four players, circuit, stunt and battle ([docs/port/BRINGUP.md](docs/port/BRINGUP.md)).
Windows x86-64 and Linux (x86-64, aarch64) builds come from the release workflow
([docs/port/RELEASE.md](docs/port/RELEASE.md)). So far it has been checked headless with scripted input; play on a
desktop GPU, with sound and real controllers, is being tested now. The plan is [docs/PORT_PLAN.md](docs/PORT_PLAN.md),
and the live numbers are in [docs/port/STATUS.md](docs/port/STATUS.md).

## Playing

### Getting it

- **A release.** The [releases page](https://github.com/cabi24/Rush2049PC/releases) has a Windows x86-64 zip and Linux
  x86-64 and aarch64 tarballs, once one is published. Between releases, each run of the Release workflow (Actions,
  "Release") has the same archives as artifacts. Each is one folder: the program, `README.txt` and the licences of
  the libraries it includes. There is no game data in it.
- **Your own build:** see [Building](#building) below.

### The ROM

You need your own ROM of *San Francisco Rush 2049* for the N64, the USA cartridge: SHA-1
`3f99351d7bb61656614bdb2aa1a90cfe55d1922c`, 12 MiB, in any byte order (`.z64`, `.v64` or `.n64`). Name it
`baserom.us.z64` and put it next to the program, or give its path: `rush2049pc PATH` (or `--rom PATH`). On Windows
you can also drop it on `rush2049pc.exe`. The program looks only in these places, in this order:

1. the path on the command line;
2. `game.rom` in `rush2049pc.ini`, when it is a full path;
3. `baserom.us.z64` (or the name `game.rom` gives) next to the program, then in the user data folder
   (`%APPDATA%\rush2049pc`, `~/.local/share/rush2049pc`), then in the working directory.

If there is no ROM, or the file isn't the right one, the program says so on the console and in a message box. It
gives where it looked, what the file is (another game, another region or revision, a bad dump, an archive to
extract first) and what it needs. [docs/port/RELEASE.md](docs/port/RELEASE.md) quotes every message.

### First run

- **Windows:** unzip the archive somewhere you can write to (not Program Files). Copy the ROM into the folder as
  `baserom.us.z64` and double-click `rush2049pc.exe`. A console window opens next to the game's window and shows
  what the program is doing. You need an OpenGL 3 driver, which any graphics driver from the last ten years has.
- **Linux:** unpack the tarball, copy the ROM in, and run `./rush2049pc`. You need glibc 2.35 or later (Ubuntu 22.04,
  Debian 12, Fedora 36 or newer) and OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver).

The first run fills `rush2049pc.ini` with every setting and a comment on each. Every run prints which ROM it
found, where the saves are and where its log is.

### Controls

| N64 | Keyboard | Controller (Xbox layout) |
|---|---|---|
| Stick | arrow keys | left stick |
| A (accelerate) | X | A, right trigger |
| B (brake) | C | X, left trigger |
| Z | Z | LB |
| L | Q | Back |
| R | E | RB |
| START | Enter | Start |
| C buttons | I, J, K, L | right stick; Y and B for C-up and C-down |
| D-pad | W, A, S, D | D-pad |

F1 (or both stick clicks) opens the settings overlay. Alt+Enter or F11 toggles fullscreen, F12 saves a screenshot,
Pause holds the game and Escape quits. Every key and button can be remapped. The details are in
[docs/port/FRONTEND.md](docs/port/FRONTEND.md).

### Settings

`rush2049pc.ini` holds them all, with comments. While you play, the F1 overlay changes the video settings
(widescreen, internal resolution, fullscreen, vsync, window size), the volume and the key and button mapping. It
can save them into the file in place. `--set SECTION.KEY=VALUE` sets any setting for one run
(`--set video.widescreen=16:9`), `--print-config` prints them all, and `rush2049pc --help` lists the options.

### Saves and other files

The release folders are portable. With `rush2049pc.ini` next to the program, everything the program keeps is in
that folder:

| File | What |
|---|---|
| `rush2049pc.ini` | the settings |
| `rush2049pc.mpk` | the saves: the game's controller pak |
| `screenshots/` | F12's pictures |
| `rush2049pc.log`, `rush2049pc.old.log` | this run's output and the run before |

Without `rush2049pc.ini` next to it (an installed copy, or a build run from its build folder), the settings go in
`%APPDATA%\rush2049pc` or `~/.config/rush2049pc`, and the rest in `%APPDATA%\rush2049pc` or
`~/.local/share/rush2049pc`. A `rush2049pc.mpk` that an earlier build left in the working directory keeps being used
there. To move to a new version, copy `rush2049pc.mpk` and `rush2049pc.ini` into its folder.

### Troubleshooting

- **"no ROM" or "wrong ROM":** the message says where the program looked and what the file is. It runs the USA
  cartridge only, and a ZIP must be extracted first. `--no-rom-check` tries another revision or a dump with other
  contents anyway; it probably won't work.
- **"no renderer", or no window:** update the graphics driver. The program needs OpenGL ES 3.0, or OpenGL 3.1 or
  later. `--set video.api=gl` tries desktop GL first.
- **No sound:** `--audio sdl` (or `alsa` on Linux) picks the output; the volume is `audio.volume`.
- **A controller isn't recognized:** put SDL's `gamecontrollerdb.txt`, with a line for it, next to `rush2049pc.ini`.
- **It stopped with an error:** the log says why. On Windows a message box gives the log's path, and the console
  waits for a key. Report it on the [issues page](https://github.com/cabi24/Rush2049PC/issues) with
  `rush2049pc.log`. Its first lines (like `rush2049pc --version`) say which build it was.

## Building

You need CMake 3.24 or later, Ninja, a C compiler (gcc or clang; mingw-w64's GCC for Windows), Python 3.9 or later
and zlib. SDL2 is optional. Without it, the program renders off screen only, with scripted input and no sound
output except ALSA on Linux. `-DRUSH_FETCH_SDL2=ON` downloads SDL2 2.30.9 (SHA-256 pinned) and links it statically.
Neither the build nor its tests needs a ROM.

### Linux

```sh
sudo apt install cmake ninja-build pkg-config python3 zlib1g-dev libegl-dev libgles-dev libsdl2-dev libasound2-dev
cmake -S . -B build/pc -G Ninja
cmake --build build/pc
ctest --test-dir build/pc                      # the PC layer's tests and the synthetic cartridge's paths
build/pc/rush2049pc                            # or rush2049pc PATH-TO-ROM; --help for the rest
python3 tools/port/census.py --build build/pc  # rewrites docs/port/STATUS.md
```

`cmake --install build/pc --prefix ~/.local` installs `bin/rush2049pc`, a desktop entry with its icon, and the
docs. `-DRUSH_INSTALL_PORTABLE=ON` installs one portable folder instead, like the release archives.

### Windows

- **In MSYS2** (the UCRT64 shell):

  ```sh
  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
            mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-SDL2
  cmake -S . -B build/pc -G Ninja && cmake --build build/pc && ctest --test-dir build/pc
  ```

  The program needs MSYS2's `SDL2.dll` (in `/ucrt64/bin`) next to it or on `PATH`.
- **Cross from Linux** (Debian or Ubuntu: `gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix
  mingw-w64-x86-64-dev libz-mingw-w64-dev`):

  ```sh
  cmake -S . -B build/win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DRUSH_FETCH_SDL2=ON
  cmake --build build/win
  ```

  This is how the release zip is built: SDL2, zlib and the C runtime are linked statically, so `rush2049pc.exe`
  needs no DLLs.

The details are in [docs/port/WINDOWS.md](docs/port/WINDOWS.md).

### Release archives

`.github/workflows/release.yml` builds the Windows zip and the Linux tarballs, then unpacks each on a clean machine
and runs it. Run it by hand (Actions, "Release", "Run workflow") to get the archives as artifacts. A pushed tag `v*`
also makes a draft GitHub release with them, which someone checks and publishes.
[docs/port/RELEASE.md](docs/port/RELEASE.md) describes it.

The N64 matching build below is unchanged and still works from the same tree.

---

# San Francisco Rush 2049 (N64) decompilation

A decompilation of *San Francisco Rush 2049* for the Nintendo 64 (USA). The C in this repository, together with
the assembly of the functions that are not C, builds a ROM that is byte-identical to the retail cartridge:

```
3f99351d7bb61656614bdb2aa1a90cfe55d1922c  build/us/rush2049.us.z64
```

Names and structure follow the game's other versions: the arcade *Rush The Rock* / *Rush 2049* source and the
GameCube and Dreamcast ports. Every function was matched against the N64 code word for word with SGI's IDO
compilers.

## State

The cartridge holds four separate code populations. Each is counted against its own denominator; the numbers
are never added together.

| Population | Runs at | Functions from C | Code bytes from C |
|---|---|---:|---:|
| Static boot/library code | linked in place, 0x80000400 | 634 / 677 | 154,372 / 162,284 (95.12%) |
| Game code | compressed image, inflated to 0x80086A50 | 1,197 / 1,216 | 542,284 / 561,860 (96.52%) |
| Runtime image A | compressed image, inflated to 0x8038A400 | 166 / 193 | 135,556 / 152,952 (88.63%) |
| Runtime image B | compressed image, inflated to 0x8038A400 | 44 / 49 | 36,296 / 38,708 (93.77%) |

* **Static code is complete.** The other 43 routines (7,816 B) were hand-written assembly in the original build
  too: libultra's assembly files (27 of them byte-identical to decompals' ultralib), the inflate helpers, the
  boot entry point and the GSM decoder kernels in the boot segment tail. With them, static code is 677/677
  functions and 99.94% of its bytes (the rest is alignment padding).
* **Game code and the runtime images are at their matching ceiling.** 19 game functions and 8 runtime-image
  functions (plus 24 eight-byte stubs that can only land with their callers) are not byte-matched. Each has a C
  draft that compiles to the right length and calls; what differs is register allocation, spill numbering or
  instruction order decided inside IDO's optimiser and assembler. These functions are built from their retail
  assembly; their best drafts are in [`nonmatching/`](nonmatching/), and
  [`docs/game-asm-ceiling.md`](docs/game-asm-ceiling.md) gives each one's identity, distance from retail and the
  compiler mechanism that stops it. They are not compiled into the ROM and are not counted above.

## Building

### Requirements

* **Your own ROM** of San Francisco Rush 2049 (USA), big-endian `.z64`, SHA-1
  `3f99351d7bb61656614bdb2aa1a90cfe55d1922c`, saved as `baserom.us.z64` in the repository root. The ROM, its
  assets and its data segment are not distributed here.
* **IDO 5.3 and IDO 7.1**, the static recompilations from decompals'
  [ido-static-recomp v1.2](https://github.com/decompals/ido-static-recomp/releases/tag/v1.2). `make setup`
  downloads both archives, checks their SHA-256 and unpacks them into `tools/ido/5.3` and `tools/ido/7.1`:

  | Archive | SHA-256 |
  |---|---|
  | `ido-5.3-recomp-linux.tar.gz` | `ab5c741561f80913d58c8b074771f23941a3edd312505a8ebed6d1dfeb65e506` |
  | `ido-7.1-recomp-linux.tar.gz` | `0d411696e178fcca34c31c3bf02011b928d7fd9c1fa7f8bf45070e0781b58e15` |

  To use copies installed elsewhere, pass `IDO53_DIR=` / `IDO71_DIR=` to make. The binaries are x86-64 Linux
  programs that need 4 KB pages (they do not run on, for example, a Raspberry Pi 5 kernel with 16 KB pages).
* **GNU binutils for MIPS**: `mips-linux-gnu-as`, `-ld`, `-objcopy`, `-nm`, `-readelf` (Debian/Ubuntu:
  `binutils-mips-linux-gnu`). Another prefix: `CROSS=`.
* **A host C compiler** (`cc`, for the zlib 1.0.4 compressor), **Python 3.9+** (standard library only) and GNU
  make.

```sh
make setup                # IDO 5.3 + 7.1 into tools/ido/ (sha256-checked)
cp /path/to/your/rom.z64 baserom.us.z64
make extract              # cut / inflate the ROM-derived inputs
make -j$(nproc)           # ... ends with "ROM matches!"
```

About 1,000 IDO compiles; a full build takes well under a minute on a desktop machine.

### What is taken from your ROM

`tools/extract_rom.py` checks the ROM's SHA-1 and writes, under the git-ignored `assets/` and `build/`: the IPL3
boot code, the data segment (ROM 0x283D0-0xC00000: RSP microcode, static data, cartridge assets), and the three
compressed code images, inflated. From the three images the build reads **data only**: their `.data`/`.rodata`
and the alignment runs between functions. Every code word comes from `src/` (C) or `asm/us/` (assembly), and the
addresses at which each compiled function's own literals, jump tables and statics are placed are recorded in
`manifest/<image>.json`.

### How the ROM is put together

1. **Static code.** `src/rom/*.c` are translation units aligned with the original object files. Each is compiled
   with the flags in `src/rom/opt_overrides.mk` (optimisation and debug level per TU; IDO 7.1 for the boot-tail
   TUs; IDO 5.3 `-S` + `as1 -O1` for the inflate TU) through asm-processor, which places the original assembly
   routines (`GLOBAL_ASM`, from `asm/us/nonmatchings/`). The whole-assembly segments are `asm/us/*.s`.
   `rush2049.us.ld` places everything at its retail address.
2. **Compressed images.** The game code and the two runtime images are separate programs that the boot code
   inflates at run time. For each, `manifest/<image>.json` lists the functions built from C: single files
   (`src/blob/<fn>.c`, `src/ovl_<x>/<fn>.c`, each with its own flags) and whole-program groups
   (`src/*/groups/<g>/`), which must be compiled together because IDO's `-O3` interprocedural register
   allocation shaped them (`tools/compile_group.py` runs IDO's passes by hand with `uld -kp`).
   `tools/build_image.py` links every compiled function alone at its address, slices the group objects per
   member, takes every other function from its assembly target, links the image, requires it to be
   byte-identical to retail, and compresses it with zlib 1.0.4 at the cartridge's exact parameters
   (`tools/zlib-1.0.4`, `tools/deflate104`: level 9, raw deflate, memLevel 8, one `Z_FINISH`).
3. **Data segment.** The three streams and the boot-tail assembly (`asm/us/boot_data/`) are composed into the
   extracted data segment in place of the originals (`tools/compose_data.py`); the original compressed bytes are
   never used. The static link and `objcopy` give the ROM, and `make` checks its SHA-1.

A change to any C function shows up as a failed image gate naming the function and the first differing word.

## Layout

```
src/rom/            static code: ROM-aligned C translation units, opt_overrides.mk
src/blob/           game code: one C file per function; groups/ for the -O3 whole-program groups
src/ovl_a, ovl_b/   runtime images A and B, same structure
include/            headers (libultra, game types)
asm/us/             assembly: whole-assembly static segments, GLOBAL_ASM routines (nonmatchings/),
                    boot-tail routines (boot_data/), the per-function assembly of the three images
                    (blob/, ovl_a/, ovl_b/, with symbols.json, extents.json and SHA256SUMS)
layout/             the three images' layouts: every function extent, the data tail
manifest/           which functions come from C, with which flags or group, and their data placements
nonmatching/        best C drafts of the functions at the matching ceiling (not built)
docs/               the matching ceiling, compiler settings, memory map, symbol notes
tools/              build helpers; asm-processor; zlib 1.0.4 (deflate half)
rush2049.us.ld      static linker script (with the other *.ld and undefined_*_auto.us.txt)
```

The C of the compressed images is one function per file because each function was matched, and is linked, at its
own address; each file carries the declarations it needs. `splat.us.yaml`, `symbol_addrs.us.txt` and
`reloc_addrs.us.txt` document how the static code was split and named; the build does not run splat.

## Documentation

* [Matching ceiling](docs/game-asm-ceiling.md): every function that is not from C, and why.
* [Compiler settings](docs/COMPILER_SETTINGS.md): IDO 5.3 / 7.1, flags, the `-S` recipe, whole-program groups.
* [Memory map](docs/memory_map.md): RAM and ROM layout, the boot segment tail, the compressed images.
* [Symbol notes](docs/SYMBOL_NOTES.md): static names corrected against ultralib.

## Legal

This repository does not contain the ROM, its assets or its data segment. `asm/us/` holds the disassembled code
of the cartridge (and, in `asm/us/blob_data/opaque.hex`, the game image's data runs) as matching targets. The C
and assembly here are a reconstruction of a commercial game; the game and its code are the copyright of their
original publisher. No license is granted for this code, and none
is implied. The third-party tools under `tools/` keep their own terms: asm-processor
(`tools/asm-processor/LICENSE`, public domain) and zlib 1.0.4 (`tools/zlib-1.0.4/README`).
