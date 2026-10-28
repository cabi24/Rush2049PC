Rush 2049 PC for Windows (x86-64) - beta
========================================

An unofficial PC port of San Francisco Rush 2049 (Nintendo 64, USA),
compiled from the decompilation's C. It holds no game data: you need your
own ROM of the cartridge. https://github.com/cabi24/Rush2049PC

You need 64-bit Windows 10 or 11 and a graphics driver with OpenGL 3.1 or
later (any GPU from the last ten years). Unzip the folder somewhere you can
write to (not Program Files).


Starting
--------
1. Double-click rush2049pc.exe.
2. The first time, a box says no ROM was found. Click Open ROM... and
   choose your ROM file: San Francisco Rush 2049 (USA),
   SHA-1 3f99351d7bb61656614bdb2aa1a90cfe55d1922c. .z64, .v64 and .n64
   dumps all work; a .zip must be extracted first.
3. The program checks the file. If it is not the right one, it says what
   it is (another game, another region, a bad dump) and offers Choose
   another... . A good one is kept as game.rom in rush2049pc.ini, so from
   then on a double-click starts the game.

Or copy the ROM into this folder as baserom.us.z64 first: then nothing is
asked. Or drag the ROM file onto rush2049pc.exe.

The program looks for the ROM only here, in this order: the file dropped
on it (or named on the command line); game.rom in rush2049pc.ini when it
is a full path; else baserom.us.z64 in this folder, then in
%APPDATA%\rush2049pc, then in the folder it was started from.


Controls
--------
  N64             Keyboard    Xbox pad            PlayStation pad
  Stick           arrows      left stick          left stick
  A (accelerate)  X           A or RT             Cross or R2
  B (brake)       C           X or LT             Square or L2
  Z               Z           LB                  L1
  L               Q           Back (View)         Share (Create)
  R               E           RB                  R1
  START           Enter       Start (Menu)        Options
  C-up, C-down    I, K        Y, B; or RS up,     Triangle, Circle;
                              down                or RS up, down
  C-left, right   J, L        RS left, right      RS left, right
  D-pad           W A S D     D-pad               D-pad
  (RS: the right stick. Other pads: the button in the same place.)

  F1          the settings overlay (on a pad: both stick clicks together)
  Alt+Enter   fullscreen on and off (F11 too)
  F12         a screenshot (PNG, in screenshots\)
  Pause       hold the game; press again to go on
  Escape      quit at once

The keyboard and the first controller play player 1; more controllers take
players 2-4 in the order they are plugged in. Every key and button can be
changed in F1 > Keyboard and F1 > Controllers, with the stick's dead zone
and sensitivity.


The settings overlay (F1)
-------------------------
F1 holds the game and opens the settings: Video, Audio, Keyboard,
Controllers, Help, About, then Save to rush2049pc.ini, Resume and Quit.
Arrows or the D-pad move, Left/Right change a value, Enter or A chooses,
Escape or B goes back. The mouse works too: point, click (left or right of
a value changes it), the wheel scrolls, the right button goes back.

Changes apply at once (those marked * at the next start). Save keeps them
in rush2049pc.ini; changes not saved are lost when the program ends.
Help lists what each key and button does with your bindings.

The settings most worth knowing (F1 > Video unless said):
  Render scale   native (the default): drawn at the window's own pixels,
                 sharp at any size and any Windows display scaling.
                 N64 (1x) is the original 320x240 look; 2x-8x and auto
                 (the largest whole multiple) too. A slow GPU: lower it.
  Fullscreen     F11. Borderless by default; Fullscreen mode exclusive
                 switches the display to Resolution.
  Resolution     desktop, 720p ... 2160p: exclusive fullscreen's mode;
                 in borderless fullscreen the size the game is drawn at
                 (1080p on a big panel is less work for the GPU).
  Window size    the window opens at two thirds of the desktop and
                 remembers its size and place.
  Widescreen     auto (the window's shape, the default), off (4:3),
                 16:10, 16:9, 21:9. The 3D view shows more at the sides.
  Race HUD       screen edges (the default) or centered (4:3, as on the
                 N64). Menus stay 4:3 either way.
  Textures       original; upscaled: the 3D world's textures 2x, or 4x
                 at render scale 4 or more (native in a window of 960 rows
                 or more), the HUD and menus as they are; it needs a
                 render scale of 2 or more (native: 480 rows). New ones
                 are upscaled in the background as they appear and kept in
                 texture_cache\ for the next run. pack: a texture pack.
  Frame rate     original (the game's own 30, the default), 60, 120,
                 144 or display. The frames between the game's are drawn
                 from its last two; the game's speed is unchanged.
  Vsync          on (the default): no tearing, and no more frames than
                 the display shows.
  Audio          F1 > Audio: Volume, and Output * (the sound device).
  Audio latency  in rush2049pc.ini only: [audio] latency = 0 (from the
                 device). If the sound crackles or drops out, try 60 or
                 100 (ms): more delay, fewer dropouts.

rush2049pc.ini has every setting with a comment; one left at its default
is commented out (remove the ; to keep a value).


Saves and other files
---------------------
This folder is portable: since rush2049pc.ini is next to rush2049pc.exe,
everything the program keeps is here:
  rush2049pc.ini        the settings
  rush2049pc.mpk        your saves (player 1's controller pak)
  rush2049pc-p2.mpk ... players 2-4's controller paks, if they have one
  rush2049pc.mpk.bak, .bak2, .bak3
                        backups: the saves as the last three runs that
                        changed them started (newest first)
  screenshots\          F12's pictures
  texture_cache\        upscaled textures; safe to delete
  rush2049pc.log        what the program printed this run
  rush2049pc.old.log    the run before
Without rush2049pc.ini next to it, these go in %APPDATA%\rush2049pc.

The saves are written whole or not at all. A damaged rush2049pc.mpk is
never written over: it is renamed rush2049pc.mpk.corrupt-DATE, the newest
good backup takes its place, and a box says so. To restore by hand, close
the program and copy rush2049pc.mpk.bak over rush2049pc.mpk.

To move to a new version, copy rush2049pc.mpk and rush2049pc.ini into its
folder.

Each player's N64 accessory is in F1 > Controllers (P1-P4 accessory):
player 1 has the controller pak (the saves); players 2-4 are on auto, a
rumble pak when their pad can rumble.


Reporting a bug
---------------
https://github.com/cabi24/Rush2049PC/issues
1. F1 > About > Copy to the clipboard, and paste it into the report: the
   version, the graphics driver, the sound output, the controllers and
   where the files are.
2. Attach rush2049pc.log (rush2049pc.old.log if it happened in the run
   before). F1 > About > Open the data folder shows where it is.
3. Say what you did and what happened.
If the program stops with an error, a box says what went wrong and where
the log is.


Known limits
------------
- Played on one Windows laptop so far; the rest by scripted test runs.
- In loud scenes the sound clips, as the game's own mixer saturates.
- Split screen keeps its HUD in the middle.
- A ghost is written to the controller pak only after the game's save
  prompt.
- A high frame rate at a laptop panel's full resolution (2880x1620): an
  integrated GPU draws few frames between the game's own. Resolution
  1080p in fullscreen, or a whole render scale, gives it room.


Troubleshooting
---------------
- No window, or "no graphics": update the graphics driver.
- A controller does nothing: F1 > Controllers shows who plays each player
  and what the stick gives, and the log names each controller found. A pad
  the program doesn't know takes a player at its first button press;
  rebind it there, or put SDL's gamecontrollerdb.txt (with a line for it)
  in this folder.
- A ROM in a folder whose name has letters Windows' code page can't spell
  may not open: copy it to a folder such as C:\Games and choose it there.
- From a command prompt: rush2049pc.exe --help lists the options. The
  prompt doesn't wait for the program (its output can come after the next
  prompt): start /wait rush2049pc.exe --help waits.


Licences
--------
This package contains no game data. The program includes SDL2, zlib and
the mingw-w64 and GCC runtime libraries; their licences are in LICENSES\.
