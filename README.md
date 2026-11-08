# Rush 2049 PC

A native PC port of *San Francisco Rush 2049* (N64, USA). It is built from a decompilation of the cartridge
([Where the code comes from](#where-the-code-comes-from)) the way the SM64 PC port was built from the SM64
decompilation: the game's C is compiled for the PC, and a PC layer stands in for the N64 hardware. You bring your own
ROM. Its data is read at startup, and nothing derived from it is built into the program.

**Status: beta candidate.** On the retail cartridge the game runs its attract demo, the front end, and races in
every mode: one to four players, circuit, stunt, battle, ghosts and the unlockable tracks and arenas
([docs/port/QA.md](docs/port/QA.md)). It has been played on a Windows laptop, and the problems found there (sound,
steering, a flickering road) are fixed; [CHANGES.md](CHANGES.md) lists what changed. Windows x86-64 and Linux (x86-64,
aarch64) builds come from the release workflow ([docs/port/RELEASE.md](docs/port/RELEASE.md)). The plan is
[docs/PORT_PLAN.md](docs/PORT_PLAN.md), and the live numbers are in [docs/port/STATUS.md](docs/port/STATUS.md).

## What's new since the beta

- **The game at 60** (`game.frame_rate = 60`, F1 > Game > Game frame rate): the game itself draws 60 frames a second
  at the N64's speed, and under vsync its frames are paced by the display's refresh
  ([FRAMERATE.md](docs/port/FRAMERATE.md) 7).
- **Draw distance, detail and fog** (`video.draw_distance`, `video.detail`, `video.fog`, F1 > Video): the track,
  the cars and the scenery twice as far or all of it, the most detailed models at any distance, the fog further out
  or off ([DETAIL.md](docs/port/DETAIL.md)).
- **Far textures** (`video.texture_lod`, F1 > Video > Far textures): `sharp` or `anisotropic` keep the road ahead
  sharp at a PC's resolution ([DETAIL.md](docs/port/DETAIL.md) 2.7).
- **Up to 12 cars a race** (`game.cars`, F1 > Game > Cars in a race), then the track select's Drones option
  ([MORECARS.md](docs/port/MORECARS.md)).
- **More memory** (`game.memory`, F1 > Game > Memory): 16 to 256 MB for the game's heap
  ([MEMORY.md](docs/port/MEMORY.md)).
- **Every engine heard in a race of 12** (`audio.voices`, `audio.engines`, F1 > Audio > Voices, Drones' engines)
  ([AUDIO.md](docs/port/AUDIO.md) 10).
- **Mods** (the `mods/` folder, F1 > Mods): files that stand in for the cartridge's, checked at start; your ROM is
  never changed. `tools/mods/rushmods.py` unpacks your ROM's files, makes a mod and checks it
  ([MODS.md](docs/port/MODS.md)).
- **Saves as files, and ghosts to share** (`game.saves = files`, F1 > Saves): a file per profile and per ghost, any
  number of ghosts, `.r49ghost` files to export and import; `tools/port/ghost.py` reads and converts them
  ([SAVES.md](docs/port/SAVES.md)).

At its default each is the N64's game exactly. [CHANGES.md](CHANGES.md) ("Next") has the details, the settings
table is in [FRONTEND.md](docs/port/FRONTEND.md), and the README.txt in each archive
([Windows](dist/README-windows.txt), [Linux](dist/README-linux.txt)) says where each setting is, when it applies, and
which to pick; the mods folder's ([dist/mods-README.txt](dist/mods-README.txt)) how to make a mod.

## Playing

### Getting it

- **A release.** The [releases page](https://github.com/cabi24/Rush2049PC/releases) has a Windows x86-64 zip and Linux
  x86-64 and aarch64 tarballs, once one is published. Between releases, each run of the Release workflow (Actions,
  "Release") has the same archives as artifacts. Each is one folder: the program, an empty `rush2049pc.ini` (which
  keeps everything in that folder), `README.txt`, an empty `mods/` folder with its `README.txt`, and the licences of
  the libraries it includes. There is no game
  data in it.
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
extract first) and what it needs. The box has an **Open ROM...** button: choose the file there (Windows' own file
dialog; zenity or kdialog on Linux), and the program checks it, says what it is if it is the wrong one, and keeps
its path as `game.rom` in `rush2049pc.ini`, so it is found from then on. [docs/port/RELEASE.md](docs/port/RELEASE.md)
quotes every message.

### First run

- **Windows:** unzip the archive somewhere you can write to (not Program Files) and double-click `rush2049pc.exe`.
  The first time, it asks for the ROM: click Open ROM... and choose the file (or copy it into the folder as
  `baserom.us.z64` first). No console window opens; what the program prints is in `rush2049pc.log`. You need an
  OpenGL 3 driver, which any graphics driver from the last ten years has.
- **Linux:** unpack the tarball and run `./rush2049pc`; it asks for the ROM the same way when zenity or kdialog is
  installed, or copy the ROM in as `baserom.us.z64`. You need glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora
  36 or newer) and OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver).

The first run fills `rush2049pc.ini` with every setting and a comment on each, commented out at its default (remove
the `;` to keep a value). Every run prints which ROM it
found, where the saves are and where its log is.

### Controls

| N64 | Keyboard | Xbox | PlayStation |
|---|---|---|---|
| Stick | arrow keys | left stick | left stick |
| A (accelerate) | X | A, RT | Cross, R2 |
| B (brake) | C | X, LT | Square, L2 |
| Z | Z | LB | L1 |
| L | Q | Back | Share |
| R | E | RB | R1 |
| START | Enter | Start | Options |
| C buttons | I, J, K, L | right stick; Y and B for C-up and C-down | right stick; Triangle and Circle |
| D-pad | W, A, S, D | D-pad | D-pad |

Other pads use the button in the same place.

F1 (or both stick clicks) opens the settings overlay. Alt+Enter or F11 toggles fullscreen, F12 saves a screenshot,
Pause holds the game and Escape quits. Every key and button can be remapped, and the stick's dead zone and
sensitivity set, in the overlay. It takes the mouse too: point and click, the wheel scrolls, the right button goes
back. Its Help page lists what each key and button does with your bindings, and About gives the version, the
graphics driver, the sound output, the controllers and where the settings, saves, screenshots and log are (Copy puts
it all on the clipboard for a bug report). The mouse cursor hides during play until the mouse moves. The details are
in [docs/port/FRONTEND.md](docs/port/FRONTEND.md).

Each player has an N64 accessory, set in F1 > Controllers (P1-P4 accessory) or the settings file (`game.pak`,
`game.pak2`...): a controller pak (the saves), a rumble pak, or none. Player 1 has the controller pak, as before.
Players 2-4 are on auto: a rumble pak when their controller can rumble, so in multiplayer each player's rumble goes
to their own pad. A player can have a controller pak of their own instead (`rush2049pc-p2.mpk`...), which the game
reads as the pak in that N64 controller. A change in the overlay is a swap, as on the console: the game sees the
old one pulled out, then the new one put in.

### Settings

`rush2049pc.ini` holds them all, with comments. The picture is drawn at the window's own pixels by default
(`video.scale = native`, any display scaling, 2880 × 1620 included); `video.scale = 1` is the N64's 320 × 240 in
square pixels. Fullscreen is borderless, or exclusive at `video.resolution` (720p to 2160p, or WxH). In widescreen
the race HUD sits at the screen's edges (`video.hud = edges`); `centered` keeps it in the 4:3 frame, as on the
N64. While you play, the F1 overlay changes the video settings (render scale, widescreen, fullscreen and its mode,
resolution, window size, vsync, frame rate, pause in background, graphics API, textures, race HUD, draw distance,
detail, fog, far textures), the volume, the sound output, the voices and the drones' engines, the key and button
mapping, the stick, rumble and each player's accessory, the game's (cars in a race, memory, game frame rate), the
mods and how the saves are kept, and can save them into the file in place. The cars apply from the next race and
the game frame rate from the next screen; the graphics API, the sound output, the voices, the memory, the mods and
how the saves are kept take effect at the next start (marked `*`).
`audio.latency` (more sound queued: fewer dropouts, more delay) is in the file only. `--set SECTION.KEY=VALUE`
sets any setting for one run (`--set video.widescreen=16:9`), `--print-config` prints them all, and
`rush2049pc --help` lists the options.

### Textures

`video.textures` (Textures on the overlay's Video page): `original`; `upscaled`, the 3D world's textures upscaled 2x
(4x from internal scale 4) by a small neural network trained for N64 textures on CC0 images and built into the port
([docs/port/UPSCALE.md](docs/port/UPSCALE.md)), in the background as they appear and kept in `texture_cache/` for the next run, the HUD
and menus as they are (it needs an internal scale of 2 or more); or `pack`, PNG replacements from `textures/`. A
pack is made from your own ROM: `video.texture_dump = dump` writes every texture drawn into `dump/`, and
`tools/texpack/texpack.py` runs an upscaler you install (realesrgan-ncnn-vulkan, ...) over them. Packs are never
shipped with the port; don't share yours. [docs/port/TEXTURES.md](docs/port/TEXTURES.md) has the details.

### Saves and other files

The release folders are portable. With `rush2049pc.ini` next to the program, everything the program keeps is in
that folder:

| File | What |
|---|---|
| `rush2049pc.ini` | the settings |
| `rush2049pc.mpk` | the saves: the game's controller pak (player 1's) |
| `rush2049pc-p2.mpk`, `-p3`, `-p4` | players 2-4's controller paks, when they have one (F1 > Controllers); backed up as player 1's |
| `rush2049pc.mpk.bak`, `.bak2`, `.bak3` | the saves as the last three runs that changed them started (newest first) |
| `screenshots/` | F12's pictures |
| `saves/p1/`, `p2`... | each player's saves as files, with `game.saves = files`: a file per profile and ghost, `import/`, `export/`, `trash/`, `backup/` ([SAVES.md](docs/port/SAVES.md) 3) |
| `mods/`, `mods_cache/` | the mods ([MODS.md](docs/port/MODS.md)); a .zip mod's textures, unpacked at each start |
| `texture_cache/` | upscaled textures kept for the next run (`video.textures = upscaled`); safe to delete |
| `textures/` | a texture pack, if you make one (`video.textures = pack`) |
| `rush2049pc.log`, `rush2049pc.old.log` | this run's output and the run before |

Without `rush2049pc.ini` next to it (an installed copy, or a build run from its build folder), the settings go in
`%APPDATA%\rush2049pc` or `~/.config/rush2049pc`, and the rest in `%APPDATA%\rush2049pc` or
`~/.local/share/rush2049pc`. A `rush2049pc.mpk` that an earlier build left in the working directory keeps being used
there. To move to a new version, copy `rush2049pc.mpk` and `rush2049pc.ini` into its folder.

The saves are written whole or not at all (a temp file, then a rename), so a crash can't leave half a file. If
`rush2049pc.mpk` is damaged when the program starts, it is not overwritten: it is renamed
`rush2049pc.mpk.corrupt-DATE`, the newest good backup takes its place (or the game starts with an empty pak), and
a message box says so.

### Troubleshooting

- **"no ROM" or "wrong ROM":** the message says where the program looked and what the file is; Open ROM... lets
  you choose another. It runs the USA cartridge only, and a ZIP must be extracted first. `--no-rom-check` tries
  another revision or a dump with other contents anyway; it probably won't work.
- **Lost saves:** copy `rush2049pc.mpk.bak` (or `.bak2`, `.bak3`) over `rush2049pc.mpk` while the program isn't
  running.
- **"no renderer", or no window:** update the graphics driver. The program needs OpenGL ES 3.0, or OpenGL 3.1 or
  later. On Windows desktop GL comes first, elsewhere OpenGL ES; `--set video.api=gl` or `gles` picks the one tried
  first.
- **No sound:** `--audio sdl` (or `alsa` on Linux) picks the output; the volume is `audio.volume`. Crackle or
  dropouts: a larger `audio.latency` in `rush2049pc.ini` (in ms, 0-200; 0 sizes it from the device).
- **A controller does nothing, or its stick doesn't steer:** F1 > Controllers shows who plays each player and what
  the stick gives, and the log names each controller found and its player. A pad SDL doesn't know gets a generic
  mapping and takes a player at its first button press; rebind it there, or put SDL's `gamecontrollerdb.txt`, with a
  line for it, next to `rush2049pc.ini`. On Windows, Xbox pads go through XInput (`SDL_JOYSTICK_RAWINPUT=1` uses
  SDL's raw input instead).
- **It stopped with an error:** the log says why. On Windows a message box says what went wrong and gives the
  log's path. Report it on the [issues page](https://github.com/cabi24/Rush2049PC/issues) with
  `rush2049pc.log`. Its first lines (like `rush2049pc --version`) say which build it was; F1 > About shows where
  the log is, and its Copy item puts the version, driver and paths on the clipboard.

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

---

## Where the code comes from

The game's C comes from a decompilation of the N64 cartridge (USA) that rebuilt the ROM byte for byte with SGI's IDO
compilers. Its names and structure follow the game's other versions: the arcade *Rush The Rock* / *Rush 2049* source
and the GameCube and Dreamcast ports. On 2026-11-08 the N64 build was dropped
([docs/port/PCONLY.md](docs/port/PCONLY.md)): the C is PC code from then on, and the matching build (the Makefile, the
cartridge's assembly, the IDO settings and tools) is in the history before that date.

## Layout

```
src/rom/            the boot code's libraries the PC still runs: libultra's math and Pfs, the audio library, the
                    boot logic
src/blob/           the game code; groups/ holds files of several functions
src/ovl_a, ovl_b/   runtime images A and B (the front end's screens)
nonmatching/        functions the decompilation never byte-matched, whose C is nowhere in src/
include/            headers (libultra, game types)
port/               the PC layer: libultra, rendering, audio, input, the data loader, the program
asm/us/             the three code images' symbol tables (symbols.json), which the data loader reads, and the boot
                    tail's GSM kernels (test_gsm's reference)
layout/             the three images' layouts: every function extent, the data tail
tools/port/         the PC build's generators and checks; tools/mods, tools/texpack, tools/upscale
docs/               docs/port/ for the PC port; the memory map, the symbol notes, the matching ceiling's identities
```

[docs/memory_map.md](docs/memory_map.md) is the N64's RAM and ROM layout, which the data loader still follows;
[docs/SYMBOL_NOTES.md](docs/SYMBOL_NOTES.md) the static names corrected against ultralib;
[docs/game-asm-ceiling.md](docs/game-asm-ceiling.md) the identities of the functions the decompilation never matched.

## Legal

This repository does not contain the ROM, its assets or its data segment. The C here is a reconstruction of a
commercial game; the game and its code are the copyright of their original publisher. No license is granted for this
code, and none is implied. The libraries the program links (zlib, SDL2) keep their own terms.
