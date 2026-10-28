# The program around the game: first run, files and releases (port/app)

Written 2026-10-27. How rush2049pc finds the player's ROM and says what is wrong when it can't use it, where it
keeps what it writes, and how the release archives are built. The code is `port/app/` (`pcapp`, linked into the
executable only), `port/ultra/romcheck.c`, and `.github/workflows/release.yml`.

| File | What |
|---|---|
| `port/app/paths.c` | the program's folders (its own, the user data folder, the working directory, portable or not) and the places the ROM is looked for |
| `port/app/rom.c` | the ROM at startup: the lookup, then the console message and the message box when it fails, with the "Open ROM..." dialog when one can be shown, and the choice kept in the settings |
| `port/app/pick.c` | the "Open ROM..." file dialog: Windows' `IFileOpenDialog`, zenity or kdialog on Linux; `RUSH2049_TEST_ROM_PICK` for the tests |
| `port/app/alert.c` | message boxes: SDL2's `SDL_ShowSimpleMessageBox`, or Windows' `MessageBoxA` without SDL2 (and for a fault); `pc_app_ask`, a box with buttons (`SDL_ShowMessageBox`) |
| `port/app/saves.c` | the controller pak's file at start: backed up when it changed, set aside when the game can't use it ("Saves" below) |
| `port/ultra/pakfile.c` | a pak file's image: good, in N64 byte order, or damaged; loading one; the atomic write the pak is saved with |
| `port/app/log.c` | the version; the log file; the end of a run: the log emptied, the error's or the fault's message box when nobody sees the console, on Windows a console's wait for a key |
| `port/app/startup.c` | first in `main`: on Windows the console (the parent's, or none: "quiet", the output kept for an error's box), the timer period and power throttling ([WINDOWS.md](WINDOWS.md) 10) |
| `port/app/icon.c`, `port/app/rush2049pc.rc` | the window's icon (Linux), the executable's icon and version information (Windows), from `dist/rush2049pc.svg` by `tools/port/make_icon.py` |
| `cmake/version.cmake` | the version, the commit and the build, written into `build/.../version/pc_version.c` at each build |
| `port/ultra/romcheck.c` | what a file is: byte order (swapped in place), SHA-1, header, and a sentence saying what it is when it isn't the USA cartridge |
| `port/app/tests/test_app.c` | `app_paths`: the folders and the lookup order, in temporary folders |
| `port/app/tests/test_saves.c` | `app_saves`: the pak image check, the atomic write, the backups, a damaged pak set aside, in a temporary folder |
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
or `WAYLAND_DISPLAY`), the same in a message box too, before any window opens. Where a file dialog can be shown,
the box offers one, **Open ROM...** ("Choosing the ROM" below); otherwise, or when the player quits, the program
exits with code 1. On Windows it would otherwise be easy to miss: a double-clicked program's console closes when
it exits.

No ROM, Linux (the box says the same at more length):

```
rush2049pc: no ROM: baserom.us.z64 is in none of these places:
rush2049pc:   /opt/rush2049pc/baserom.us.z64 (next to the program)
rush2049pc:   /home/me/.local/share/rush2049pc/baserom.us.z64 (the user data folder)
rush2049pc:   /home/me/baserom.us.z64 (the working directory)
rush2049pc: copy your San Francisco Rush 2049 (USA) ROM next to the program as baserom.us.z64, or give its path: ./rush2049pc --rom PATH
```

The box on Windows, titled "Rush 2049 PC: no ROM", with the buttons **Open ROM...** and **Quit** ("Choosing the
ROM" below):

```
This program plays your own copy of the San Francisco Rush 2049 (USA) cartridge, from a ROM file, and none was found.

It looked for baserom.us.z64 in:
    C:\Games\rush2049pc\baserom.us.z64 (next to the program)
    C:\Users\me\AppData\Roaming\rush2049pc\baserom.us.z64 (the user data folder)

Open ROM... lets you choose the ROM file now; the program checks it and keeps where it is in its settings (game.rom), so it finds it from then on. .z64, .v64 and .n64 files all work.

Or copy your ROM next to the program and name it baserom.us.z64, or drag the ROM file onto rush2049pc.exe.
```

Where no file dialog can be shown (Linux without zenity or kdialog, where the last line says "or start the program
with: ./rush2049pc --rom PATH"; Windows after "No file dialog could be shown."), the box asks nothing and ends:

```
Copy your ROM next to the program and name it baserom.us.z64, or drag the ROM file onto rush2049pc.exe. .z64, .v64 and .n64 files all work.

Or set game.rom to its path in
    C:\Games\rush2049pc\rush2049pc.ini
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

### Choosing the ROM (the "Open ROM..." dialog)

Written 2026-10-28. When the box above would be shown and a file dialog can be, the box asks instead of only
saying: its buttons are **Open ROM...** (Enter) and **Quit** (Escape, or closing it), and its text ends with
"Open ROM... lets you choose the ROM file now; the program checks it and keeps where it is in its settings
(game.rom), so it finds it from then on." It is asked for every case above: no ROM found, a path given (or a
`game.rom` that has moved) with nothing there, and a file found that can't be used.

- **The dialog** (`port/app/pick.c`):
  - Windows: the system's own, `IFileOpenDialog` (`CoCreateInstance`, ole32, which the program already imported
    through SDL2: no new DLL). Its filter is "N64 ROMs (*.z64, *.v64, *.n64)", then "All files". The path comes
    back in UTF-16 and is turned into the code page the C runtime's `fopen` takes, as the program's other paths
    are; a name it can't spell is tried by its short (8.3) name, and failing that the box says to copy the ROM to
    a folder such as `C:\Games`.
  - Linux: `zenity --file-selection` (GNOME and most desktops), or `kdialog --getopenfilename` (tried first when
    `XDG_CURRENT_DESKTOP` names KDE), whichever is installed, run with `posix_spawn` and read from its output.
    Their stderr goes to `/dev/null`, so GTK's warnings stay out of the log. With neither, there is no dialog, and
    the box is today's, with no question. xdg-desktop-portal's FileChooser is not used: its answer comes as a
    D-Bus signal, which needs a D-Bus library the program doesn't link.
- **The file chosen** is checked by `romcheck.c` like any other. A `.v64` or `.n64` dump of the right game is put
  into `.z64` order in memory as it is loaded (the loader always did: `pc_rom_identify` swaps the whole image
  before the SHA-1), so it plays; nothing is written back. A file that can't be used gets the same diagnosis as
  above, in a box titled "Rush 2049 PC: wrong ROM" that ends "You can choose another file, or quit.", with the
  buttons **Choose another...** and **Quit**, until one can be used or the player quits.
- **Kept.** A usable file's full path is written as `game.rom` into the settings file that was read
  (`pc_settings_save`, so the rest of the file stays as it was): `rush2049pc.ini` next to a portable program, else
  the user settings folder's. The next start finds it as "the path in the settings (game.rom)", with no box. With
  `--no-config` nothing is written (`the ROM chosen is not kept: no settings file (--no-config)`).
- **The console** says what happened: `rush2049pc: ROM: PATH (chosen; .v64)`, `rush2049pc: game.rom = PATH kept in
  INI, for the next start`, `rush2049pc: no ROM chosen (Quit)`, `... (the file dialog was cancelled)`.
- **Unchanged:** a dropped file and `--rom` are used as before (and only offered the dialog when they can't be
  used). No box asks and no dialog opens with `--headless` (on Linux, the default without a display), with
  `RUSH2049_MESSAGE_BOX=print` or `off`, or with `--no-rom`: scripted and CI runs behave as before.

### Testing it without a display

- `RUSH2049_MESSAGE_BOX=print` prints the box to stderr instead of showing it, between `rush2049pc: message box:
  TITLE` and `rush2049pc: (end of message box)`. `=off` shows none. Neither asks anything.
- `RUSH2049_TEST_ROM_PICK="ANSWER|ANSWER|..."` stands in for the player: every box is printed as with `print`,
  followed by `rush2049pc: buttons: [Open ROM...] [Quit]; chosen: ...`; a box's answer is its first button while
  answers are left, else the last (Quit). Each file dialog takes the next answer: a path (from the working directory
  when relative), `:cancel` (the player cancelled), `:fail` (no dialog could be shown), or `:dialog`: the real
  dialog, which on Linux the test makes a stand-in `zenity` or `kdialog` script on `PATH` answer, so that the
  search, `posix_spawn`, the arguments, the output and the exit status are the real code's.
- `app_firstrun` runs the program for each case above: no ROM; `--rom` and a dropped path that don't exist; two
  dropped paths (exit 2); an absolute `game.rom`; another game; Europe; revision 1; a bad dump as `.z64`, `.v64`
  and `.n64`; a cut dump; a short file; an empty one; a text file; a ZIP; a folder; an unreadable file (Linux, not
  root). Then the synthetic cartridge with `--no-rom-check` three ways: dropped, from the user data folder, and as a
  `.v64` in the working directory. Then the real box call on SDL's dummy video driver with no display, which fails
  and is reported (Linux only: on Windows it would show a real box).
  - The dialog, through `RUSH2049_TEST_ROM_PICK`: cancelled; quit at the box; another game chosen, then quit; a
    Europe ROM then a bad `.v64` dump, each with its diagnosis and "Choose another..."; no dialog (the plain box
    follows); a wrong ROM found by the lookup; a `game.rom` that has moved; none of it headless or with `--no-rom`.
    On Linux, with SDL's offscreen driver, the ROM chosen then runs: the synthetic cartridge as `.z64`; a text file
    then a `.v64` in a folder with a space; a relative `.n64`; the choice kept in `rush2049pc.ini` (the file
    checked), and the next start finding it there with no box and no dialog.
  - The saves ("Saves" below): a changed pak backed up, an unchanged one not, the backups rotated, a damaged pak
    set aside with a new one made, a short one restored from `.bak2` past a damaged `.bak`, and (Linux) the warning
    box.
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
  settings and their comments, the defaults commented out (`fill_portable_ini` in `port/main.c`), as it writes a missing one elsewhere.
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

### Saves: backups, the atomic write, a damaged pak

Written 2026-10-28. The controller pak (`rush2049pc.mpk`) is the player's progress: the game saves nowhere else.
`port/app/saves.c`'s `pc_app_saves_prepare(path, window)` runs for the pak's file before the game reads it
(`pc_si_init`), and before the window opens; it works on any pak file and its own backups only, so each player's
pak can have it.

- **Backups.** A good pak that differs from `rush2049pc.mpk.bak` (the run before changed it, or there is no
  backup yet) is copied there: `.bak2` becomes `.bak3` (the oldest goes), `.bak` becomes `.bak2`, then the new
  `.bak` is written. So the last three versions that started a run are kept, next to the pak. An unchanged pak
  makes no backup. `rush2049pc: saves backed up, since they changed: PATH.bak`.
- **The write.** si.c writes the pak within a second of each change and at the end of a run (unchanged), through
  `pakfile.c`'s `pc_file_write_atomic`: `PATH.tmp` is written, flushed to the disk (`fsync`; `_commit` on
  Windows), and renamed over the pak (`rename`; `MoveFileEx` with `MOVEFILE_WRITE_THROUGH`), then the folder is
  flushed (POSIX). A crash or a power cut leaves the old pak or the new one, never part of one. A write that fails
  is said once in the log and tried again a second later; the saves stay in memory meanwhile. The backups and a
  restored pak are written the same way.
- **A pak the game can't use** is set aside before the game sees it, since the game's libultra would write over it:
  `pc_pak_image_check` (`port/ultra/pakfile.c`) calls a pak damaged when it is not 32 KiB, when none of the four
  copies of its ID block has a right checksum (`__osGetId` would then write a new ID with `__osRepairPackId`), or
  when neither inode table (page 1, its backup page 2) has a right checksum (the game would be told the pak is
  inconsistent). A file that can't be read, or a folder, counts too. Then:
  1. it is renamed `rush2049pc.mpk.corrupt-YYYY-MM-DD` (`-2`, `-3`, ... for another the same day), unchanged;
  2. the newest of `.bak`, `.bak2`, `.bak3` that is a good pak is copied in its place; with none, si.c makes a
     new, empty pak, as on a first run;
  3. the console and the log say so, and with a window a warning box, "Rush 2049 PC: saves": what was wrong, the
     name the file is kept under, and which backup (of what date) the saves came back from, or that the pak is new.

  When it can't be renamed (a read-only folder), no pak is plugged in for that run and nothing is written over it;
  the box says to move it away or put a backup in its place. A damaged pak that reaches si.c anyway is never
  formatted over either (`pc_pak_file_load`): only a pak that doesn't exist is new. An image in N64 byte order
  (an emulator's) is left alone as before (ULTRA.md), and not backed up.
- **What the game writes is a good image.** `--selftest` checks the pak after the game's own Pfs C has allocated
  and written a note, and after it has deleted it, against `pc_pak_image_check`.

Tested by `app_saves` (`port/app/tests/test_saves.c`, in a folder with a space): the image check (formatted, a
data byte changed, short, empty, 64 KiB, one ID copy bad and all four, one inode table bad and both, zeros, N64
order); `pc_pak_file_load` (none, good, N64 order, too big, unreadable); the atomic write (new, replaced, a temp
file left by a crash, a temp file that can't be made, a read-only folder: the old file stays and no temp file is
left); the first backup, none when unchanged, four changes keeping three; a damaged pak restored from `.bak` (and
the next start making no backup), a second one the same day, `.bak` damaged too and `.bak2` used, no backup, a
folder, an unreadable pak, an N64-order pak left alone. And by `app_firstrun`, end to end (above).

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
- **Windows, a double-click.** `rush2049pc.exe` is a GUI program, so a double-click opens no console, and nobody
  sees what it prints ([WINDOWS.md](WINDOWS.md) 10). The program is "quiet": what it prints goes to the log as
  always and into a pipe whose reader keeps the last 4 KiB. An error exit that no box has explained yet (no ROM
  headless, a command-line or settings error, ...) shows a message box, "Rush 2049 PC stopped", with the last lines
  printed, the log's path and where to report it.
- **Windows, from a command prompt**, the program attaches to the prompt's console and prints there; cmd.exe and
  PowerShell don't wait for it (WINDOWS.md 10).
- **Windows, a fault** with a window, or with nobody to see the console, shows a message box, "Rush 2049 PC
  stopped", with the fault's first line, the log's path and where to report it. The box is `MessageBoxA`, not
  SDL's, since it is shown from inside the exception filter.
- **The wait for a key.** A program that owns its console, which closes when it ends (`GetConsoleProcessList`
  returns 1), prints "press a key to close this window" after an error exit or a fault and waits (`_getch`). That
  is a console build (`-DRUSH_WINDOWS_CONSOLE=ON`) double-clicked; the GUI program never owns a console.
  `RUSH2049_WAIT_KEY=1` forces the wait (with stdin not a console, it reads a line from stdin), `=0` turns it off.
- **`RUSH2049_CONSOLE=none`** makes a run quiet on any system, for the tests: with `RUSH2049_MESSAGE_BOX=print` the
  box is printed to the real stderr.

Tested by `app_firstrun` (`tools/port/firstrun_check.py`): the boot without a ROM, which faults, leaves a log with
the header, the run, the fault line and its backtrace; two runs keep the first as `rush2049pc.old.log`; a copy of
the program with `rush2049pc.ini` next to it logs next to it; `--log` and `--no-log`; `--version`, `--help` and
`--print-config` on a pipe; on Windows the forced wait for a key. Quiet (`RUSH2049_CONSOLE=none`): two ROMs dropped
(the error's box with the line, nothing printed outside it), an unknown option (the box has the error, not the
usage), no ROM with a window (the ROM's box and no second one, the log written), `--version` (nothing shown), and on
Windows a fault (its box; the log has the report). Every synthetic path (`synth_paths.py`) runs with `--log NAME.game.log` and checks that the
log has the state the console reached. `release.yml`'s `windows-check` checks the log of the zip's program after
its boot without a ROM (the fault and backtrace in it), `--version`'s Actions run, and the forced wait.

## Versions

`rush2049pc --version`, the log's first lines and the console's first line give the version: `git describe --tags
--always --dirty` at build time (`v1.0`, `v1.0-3-gabc1234`, `abc1234` before any tag, `-dirty` with uncommitted
changes), the full commit, "GitHub Actions run N (owner/repo)" or "local build", and the build type (CMake's
`$<CONFIG>`: `Debug`, `RelWithDebInfo`, ...). The settings overlay's About page shows the same, with the GL driver,
the audio output, the players, the ROM and where the files are ([FRONTEND.md](FRONTEND.md)).

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
| `windows` | ubuntu-24.04, in `debian:12` | cross-built as pc.yml's `windows-cross` (mingw-w64 GCC 12.2, SDL2 2.30.9 from source); `cmake --install` into `rush2049pc-<version>-windows-x86_64/`; the zip; checks what is in it, and its DLLs |
| `windows-check` | windows-latest | the zip unzipped; from its folder: `--version`, the first run (the empty ini filled, the log next to the program), `--selftest`, the boot without a ROM (the log has its fault and backtrace), the forced wait for a key. Mesa's llvmpipe `opengl32.dll` is copied in for the run, since the runner has no GPU; it is not in the zip |
| `linux` | ubuntu-24.04 and ubuntu-24.04-arm, in `ubuntu:22.04` | x86-64 and aarch64: built with SDL2 2.30.9 from source; `cmake --install` into `rush2049pc-<version>-linux-<arch>/`; the tarball; checks what is in it, its libraries and the newest glibc symbol it needs |
| `linux-check` | the same, in `fedora:40` | the tarball unpacked on another distribution; from its folder: `--version`, `--selftest`, the first run, the boot without a ROM headless on Mesa's llvmpipe with its log |
| `release` | ubuntu-24.04 | tags only: `gh release create --draft` with the three archives |

- **The Windows zip:** `rush2049pc.exe`, an empty `rush2049pc.ini`, `README.txt` (`dist/README-windows.txt`, CRLF line
  ends) and `LICENSES/`:
  - `SDL2.txt`, from the SDL2 source built;
  - `zlib.txt`, the licence comment of the `zlib.h` compiled against;
  - `mingw-w64.txt` and `gcc-runtime.txt`, Debian's copyright files for the mingw-w64 runtime (CRT, winpthreads)
    and GCC's (libgcc, under the runtime library exception).

  `rush2049pc.exe` is a GUI program with an icon and version information: Explorer's Properties show
  "Rush2049PC (unofficial PC port)" and the version `--version` gives ([WINDOWS.md](WINDOWS.md) 10).
  SDL2, zlib and the runtime are linked statically, so the program needs only Windows' own DLLs (`objdump -p`:
  ADVAPI32, GDI32, IMM32, KERNEL32, msvcrt, ole32, OLEAUT32, OPENGL32, SETUPAPI, SHELL32, USER32, VERSION,
  WINMM). The "Open ROM..." dialog added none: `IFileOpenDialog` is ole32's (`CoCreateInstance`), not
  comdlg32's `GetOpenFileNameW`. `pe_check.py --dlls` fails on any other DLL; `win_pe` passes it when SDL2 is built
  here (`RUSH_FETCH_SDL2`), as in the release. The executable is built RelWithDebInfo (-O2 -g,
  [OPTIMIZED.md](OPTIMIZED.md)) and is not stripped, so its backtraces can be symbolized (`addr2line -f -i`, for the
  inlined frames).
- **The Linux tarballs:** `rush2049pc`, an empty `rush2049pc.ini`, `README.txt` (`dist/README-linux.txt`) and
  `LICENSES/` (`SDL2.txt`, `zlib.txt`).
  - Built in Ubuntu 22.04, so they need glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36 and newer).
  - SDL2 is built with the X11, Wayland, libdecor, PulseAudio, PipeWire, ALSA, udev and D-Bus headers there and
    linked statically. It loads those libraries at run time (SDL's default), so the program runs without them,
    and with a window wherever they are.
  - The program itself needs only `libc.so.6`, `libm.so.6`, `libz.so.1`, `libEGL.so.1` (GLES comes through EGL)
    and `libasound.so.2` (port/audio's ALSA output), and glibc symbols up to `GLIBC_2.35`. The job prints both
    (`readelf -d`, `objdump -T`).
  - The tarball needs CMake 3.24 to build (`$<LINK_LIBRARY:WHOLE_ARCHIVE>`), and Ubuntu 22.04 has 3.22: the job
    installs CMake 3.30.5 from PyPI. Configuring with an older CMake now stops with that message.
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

**Results** (workflow_dispatch on `port-release`):

| Commit | Release run | pc.yml run | What was new |
|---|---|---|---|
| edc9445 | 40572061761 | 40572065582 | the Windows zip; unzipped: the self-test (26 of 26), the boot without a ROM |
| 590baca | 40574335614 | 40574338828 | the ROM lookup and its messages |
| d69f864 | 40576309316 | 40576314199 | the log: on windows-latest the zip's exe wrote `rush2049pc.log` with its header, the fault line and a symbolized backtrace; `--version` named the Actions run; the forced wait |
| 72d47b5 | 40578906382 | 40578911258 | the data folder; the zip's empty ini filled by the first run |
| 12bf09a | 40579937190 | | the Linux tarballs, x86-64 and aarch64, built in Ubuntu 22.04 and run on Fedora 40: `--version`, the self-test, the first run, the boot without a ROM on llvmpipe with its fault and backtrace in the log |

## Release check

Written 2026-10-28. The archives of Release run 40665715547 (main 7ce7406), as a beta tester gets them. Each one was
unpacked into a new, empty folder and run from there, with a new user's home (no `~/.local/share/rush2049pc`,
no `~/.config/rush2049pc`). Run 40666433273 (53cad66) built the same packages again, with the first-run checks
below in `linux-check` and `windows-check`. Since then only its docs and the menu's audio choices on Windows
changed. All six of its jobs passed. On windows-latest, the zip's log now has the fault's line after the run,
not ahead of it.

**What a tester gets.** The tester gets one folder:

- **Windows zip:** `rush2049pc.exe`, an empty `rush2049pc.ini`, `README.txt` and `LICENSES/` (SDL2, zlib,
  mingw-w64, GCC runtime). The exe is a GUI program with an icon and version information, and imports only
  Windows' own DLLs.
- **Linux tarballs (x86-64, aarch64):** `rush2049pc`, an empty `rush2049pc.ini`, `README.txt` and `LICENSES/`
  (SDL2, zlib). The program needs libc, libm, libz, libEGL and libasound, glibc 2.35.

None of them holds a ROM-like file, a build tree or a test program. The only build paths in the programs' loaded
data are four of SDL2's own `__FILE__` strings. The rest are in the debug information, which the backtraces use.

| Archive | Where | What was run | Result |
|---|---|---|---|
| aarch64 | the Pi 5 (Debian 12, 16 KiB pages), the retail ROM by its path | no ROM; a missing `--rom`; `--rom`; `game.rom` set by hand in the ini; "Open ROM..." answered through `RUSH2049_TEST_ROM_PICK` (SDL's offscreen driver), then a second start; three `--press-file` runs on the portable pak (create a player, race, restart) | each no-ROM case exits 1 with its message (and its box with `RUSH2049_MESSAGE_BOX=print`). The first run fills the ini, all of it commented out. The ROM chosen is kept as `rom =` and found at the next start with no box. The player created is on the pak (a note `NRUE`) and listed on the player screen after a restart. `.bak`, `.bak2`, `.bak3` rotate as the pak changes, and no backup is made when it doesn't. Every run exits 0 with the `audio:` exit line, and the log holds only expected lines |
| x86-64 | rocky (Rocky Linux 10, Intel UHD 770), the synthetic cartridge | the same without the retail ROM: `--version`, `--help`, `--print-config`, no ROM, "Open ROM...", two races (`synth/pak.press`) on the portable pak | the same results; the race reaches its state |
| Windows | inspected on the Pi; run by `windows-check` | its files, `README.txt` (CRLF), `LICENSES/`, `pe_check.py --dlls`, `objdump -p` | GUI subsystem; the icon's nine sizes; FileVersion and ProductVersion `7ce7406`; the 13 DLLs listed above; the NVIDIA and AMD GPU hints exported |

**Found and fixed.**

- **The Pi 5.** Every aarch64 tarball before 6417099 faulted at startup on a 16 KiB-page kernel, the Pi 5's
  default. `absfix.c` rounded the RELRO end up, and so made the start of `.data` read-only. Ubuntu 22.04's
  binutils aligns RELRO to 4 KiB only. Local builds (binutils 2.40, aligned to 64 KiB) and CI's 4 KiB runners
  never showed it.
- **Windows:** stderr is unbuffered once the log takes it (0600dcd). Before that, the log came in 4 KiB pieces,
  and a fault's first line came ahead of the run.
- **`release.yml`:** the first runs must exit 1, and a start with no ROM at all is checked (48d5821, 53cad66).
- **The Linux README:** the program uses the system's zlib, which it doesn't include.

**Not checked here:** a real window, sound through a real device beyond `--fast` (where its underruns are
expected), controllers, and Windows on a desktop (WINDOWS.md 11).
