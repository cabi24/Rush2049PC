Rush 2049 PC for Linux - beta
=============================

An unofficial PC port of San Francisco Rush 2049 (Nintendo 64, USA),
compiled from the decompilation's C. It holds no game data: you need your
own ROM of the cartridge. https://github.com/cabi24/Rush2049PC

You need x86-64 or aarch64 (64-bit ARM, such as a Raspberry Pi 4 or 5),
glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36 or newer), and
OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver). SDL2 is built in; X11
or Wayland, PulseAudio, PipeWire or ALSA, and controllers are used through
the system's own libraries.


Starting
--------
1. Run ./rush2049pc (or ./rush2049pc ~/roms/rush2049.z64 to name the ROM).
2. The first time, a box says no ROM was found. With zenity or kdialog
   installed it has Open ROM...: choose your ROM file, San Francisco
   Rush 2049 (USA), SHA-1 3f99351d7bb61656614bdb2aa1a90cfe55d1922c.
   .z64, .v64 and .n64 dumps all work; a .zip must be extracted first.
3. The program checks the file. If it is not the right one, it says what
   it is (another game, another region, a bad dump) and offers Choose
   another... . A good one is kept as game.rom in rush2049pc.ini, so from
   then on the game starts at once.

Or copy the ROM next to the program as baserom.us.z64 first: then nothing
is asked (and without zenity or kdialog, that is the way).

The program looks for the ROM only here, in this order: the path on the
command line (or --rom PATH); game.rom in rush2049pc.ini when it is a full
path; else baserom.us.z64 next to the program, then in
~/.local/share/rush2049pc ($XDG_DATA_HOME), then in the working directory.
./rush2049pc --help lists the options.


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
  F12         a screenshot (PNG, in screenshots/)
  Pause       hold the game; press again to go on
  Escape      quit at once

The keyboard and the first controller play player 1; more controllers take
players 2-4 in the order they are plugged in. Every key and button can be
changed in F1 > Keyboard and F1 > Controllers, with the stick's dead zone
and sensitivity.


The settings overlay (F1)
-------------------------
F1 holds the game and opens the settings: Video, Audio, Keyboard,
Controllers, Game, Mods, Saves, Help, About, then Save to rush2049pc.ini,
Resume and Quit.
Arrows or the D-pad move, Left/Right change a value, Enter or A chooses,
Escape or B goes back. The mouse works too: point, click (left or right of
a value changes it), the wheel scrolls, the right button goes back.

Most changes apply at once. A few apply at the next race or the next
screen, as said below, and those marked * at the next start. Save keeps
them in rush2049pc.ini; changes not saved are lost when the program ends.
Help lists what each key and button does with your bindings.

Every setting has a name, such as video.draw_distance: in rush2049pc.ini
that is the line draw_distance = ... in the [video] section. The file has
every setting with a comment; one left at its default is commented out
(remove the ; to keep a value). --set video.draw_distance=far sets one for
a single run.

The settings new since the beta are at original by default: the N64's
game exactly.

F1 > Presets > PC enhanced (--preset enhanced): the recommended settings.

F1 > Video
  Render scale   native (the default): drawn at the window's own pixels,
                 sharp at any size and on a high-DPI display. N64 (1x) is
                 the original 320x240 look; 2x-8x and auto (the largest
                 whole multiple) too. A slow GPU: lower it.
  Fullscreen     F11. Borderless by default; Fullscreen mode exclusive
                 switches the display to Resolution.
  Resolution     desktop, 720p ... 2160p: exclusive fullscreen's mode;
                 in borderless fullscreen the size the game is drawn at
                 (1080p on a big panel is less work for the GPU).
  Window size    the window opens at two thirds of the desktop and
                 remembers its size and place.
  Widescreen     auto (the window's shape, the default), off (4:3),
                 16:10, 16:9, 21:9. The 3D view shows more at the sides.
  Vsync          on (the default): no tearing, and no more frames than
                 the display shows.
  Frame rate     original (the game's own frames, the default), 60, 120,
                 144 or display: the frames shown a second. The frames
                 between the game's are drawn from its last two; the
                 game's speed is unchanged. display: one for each of the
                 display's refreshes (with Vsync on). At once.
                 video.frame_rate
  Textures       original; upscaled: the 3D world's textures 2x, or 4x
                 at render scale 4 or more (native in a window of 960 rows
                 or more), the HUD and menus as they are; it needs a
                 render scale of 2 or more (native: 480 rows). New ones
                 are upscaled in the background as they appear and kept in
                 texture_cache/ for the next run. pack: a texture pack.
  Race HUD (widescreen)
                 screen edges (the default) or centered (4:3, as on the
                 N64). Menus stay 4:3 either way.
  Draw distance  original (N64), far (2x) or max (all). How far the
                 track, the cars and the scenery are drawn: far draws
                 everything twice as far, max the whole track and every
                 model at any distance. At once. video.draw_distance
  Detail         original or high: high keeps every car and model at its
                 most detailed wherever it is drawn, and gives split
                 screen one player's models. At once (split screen's
                 models from the next track). video.detail
  Fog            original, far (2x) or off: the fog that the track
                 select's fog option sets, twice as far away, or none (the
                 sky in its place from the next track). At once. video.fog
  Far textures   original (N64), sharp or anisotropic. The N64 picks a
                 texture's smaller copies for its 320x240 screen, so at a
                 PC's resolution the road a few car lengths ahead looks
                 blurred. sharp picks them for your screen's pixels;
                 anisotropic also keeps the far road sharp along its
                 length. At once. video.texture_lod

F1 > Audio
  Volume         0-100.
  Output *       the sound output (auto, sdl, alsa...).
  Voices *       original (32), 48, 64 or 96: how many sounds play at
                 once. With the N64's 32, a race of 12 plays your engine
                 and the nearest four or five others'; at 64 every engine
                 in earshot is heard with the effects. At the next start.
                 audio.voices
  Drones' engines
                 original, nearest 2, 3, 4, 6 or 8, or all: which other
                 cars' engines you hear. nearest N: the N nearest to your
                 view; all: every one in earshot (with Voices 64 or more).
                 At once. audio.engines
  Audio latency  in rush2049pc.ini only: [audio] latency = 0 (from the
                 device). If the sound crackles or drops out, try 60 or
                 100 (ms): more delay, fewer dropouts.

F1 > Game
  Cars in a race original (6), or 7-12. Then raise the track select's
                 Drones option, which goes up to one less than this (its
                 default, 5, still races 6 cars). For single races, one or
                 two players; the circuit stays 6 cars. From the next
                 race. game.cars
  Memory *       original (N64), 16, 32, 64, 128 or 256 MB: the game's
                 memory, the N64's 8 MB by default. More gives its heap
                 room: a race's cars, larger files from mods. About shows
                 how much a run used. At the next start. game.memory
  Game frame rate
                 original (30) or 60. At 60 the game itself draws 60
                 frames a second: the menus, the HUD, the cameras and the
                 effects step 60 times a second, at the N64's speed; the
                 car handling is the same. From the next screen (a menu, a
                 race). game.frame_rate. If the PC can't keep 60 in a
                 race, the log and About say so; the game keeps its speed.

F1 > Mods, F1 > Saves: see Mods and Saves below.


Recommended settings
--------------------
For a typical PC (a GPU from the last ten years, a 60 Hz display):
  F1 > Game    Game frame rate 60, Cars in a race 12, Memory 64 MB
  F1 > Video   Draw distance far (or max), Far textures anisotropic,
               Vsync on, Render scale native
  F1 > Audio   Voices 64, Drones' engines all
Then Save to rush2049pc.ini, quit and start again (Memory and Voices
apply then). To race 12 cars, raise the track select's Drones option to
11. (12 cars fit in the N64's 8 MB too; 64 MB leaves room to spare.)

On a 144 Hz (or faster) display: the same, and Frame rate display
(video.frame_rate = display): a frame for each refresh, the game's own 60
and frames drawn between them.

On a slow PC (an integrated GPU, an old laptop, a Raspberry Pi 5: it keeps
60 in the menus, but not in a race of 12):
- Game frame rate original if the log or F1 > About says the PC can't keep
  60 in a race: the game then shows its frames unevenly. Frame rate 60
  with it is smoother than 30, for about half the work of the game at 60.
- Draw distance far or original, not max (max draws two to five times the
  triangles); Far textures sharp (anisotropic reads four times the
  texels for the world's textures).
- Fewer cars, a lower Render scale, or Resolution 1080p in fullscreen.
- Voices and engines cost little: they can stay.


Mods
----
The mods folder next to the program holds mods: each a folder (or a .zip)
with a mod.ini and files that stand in for the cartridge's (cars, tracks,
fonts, the HUD and menus, text, sounds). Your ROM is never changed.
mods/README.txt says what goes in it. (In an installed copy the folder is
~/.local/share/rush2049pc/mods; make it there.) F1 > Mods lists the mods
found: Left/Right turn one on and off, Enter picks it up to move it in the
order (the first wins a file two mods have). Changes apply at the next
start (game.mods, game.mods_order). The log says which mods are in use and
which files it refused; F1 > About says which mods are in use. --no-mods
plays the cartridge's files only.


Making a mod
------------
The tool is tools/mods/rushmods.py in the source
(https://github.com/cabi24/Rush2049PC): plain Python 3.8 or later, run
on your own ROM. mods/README.txt has the same steps.
1. Make the mod's folder in the mods folder next to the program:
     python3 rushmods.py new red-cars --dir PATH-TO/mods --author You
   It writes mod.ini, an empty textures/ and text/, and a README.txt.
2. Unpack the file you want to change:
     python3 rushmods.py extract baserom.us.z64 out car01
   writes out/cars/car01.bin. "rushmods.py names" lists every file's
   name; --text also writes every string, in the text overrides' form.
3. Change it, and put it in the mod: mods/red-cars/cars/car01.bin.
4. Check the mod as the game will:
     python3 rushmods.py check PATH-TO/mods/red-cars --rom baserom.us.z64
   says ok, or REFUSED and why, for each file.
5. Start the game. F1 > Mods lists the mod, on; the log's "mods:" lines
   say which files it uses and which it refused.
docs/port/MODS.md in the source has the names, the formats and the limits.
Files unpacked from your ROM are the ROM's: share only your own work.


Saves and other files
---------------------
This folder is portable: since rush2049pc.ini is next to the program,
everything it keeps is here:
  rush2049pc.ini        the settings
  rush2049pc.mpk        your saves (player 1's controller pak)
  rush2049pc-p2.mpk ... players 2-4's controller paks, if they have one
  rush2049pc.mpk.bak, .bak2, .bak3
                        backups: the saves as the last three runs that
                        changed them started (newest first)
  saves/p1/ ...         your saves as files, with game.saves = files
                        (below)
  mods/                 mods (above)
  mods_cache/           a .zip mod's textures, unpacked; safe to delete
  screenshots/          F12's pictures
  texture_cache/        upscaled textures; safe to delete
  rush2049pc.log        what the program printed this run
  rush2049pc.old.log    the run before
Without rush2049pc.ini next to it (an installed copy), the settings are in
~/.config/rush2049pc ($XDG_CONFIG_HOME) and the rest in
~/.local/share/rush2049pc ($XDG_DATA_HOME).

The saves are written whole or not at all. A damaged rush2049pc.mpk is
never written over: it is renamed rush2049pc.mpk.corrupt-DATE, the newest
good backup takes its place, and the program says so. To restore by hand,
close the program and copy rush2049pc.mpk.bak over rush2049pc.mpk.

To move to a new version, copy rush2049pc.mpk and rush2049pc.ini (and
saves/, if you keep your saves as files) into its folder.

Each player's N64 accessory is in F1 > Controllers (P1-P4 accessory):
player 1 has the controller pak (the saves); players 2-4 are on auto, a
rumble pak when their pad can rumble.

Saves as files (F1 > Saves > Kept as *, game.saves): pak (.mpk), the
default, is the one 32 KB controller pak image above, which holds only a
handful of ghosts. files (a folder) keeps each profile and each ghost as a
file of its own in saves/p1, any number of ghosts; the game still sees a
controller pak, with your profiles, the ghosts it saved and the fastest of
the rest that fit. It applies at the next start. The first start with
files copies the saves out of rush2049pc.mpk, which is left as it is;
Export to the .mpk, on the same page, goes back. Player, at the top of the
page, picks whose saves (saves/p2 ... and game.saves2 ... for players
2-4). With files the page lists every profile and ghost: choose one to Pin
to the pak (always on it), Export (.r49ghost) or Delete. Nothing is ever
removed: a deleted save goes to the folder's trash/, and each file keeps
three backups in backup/.


Sharing ghosts
--------------
1. F1 > Saves > Kept as: files (a folder). Then, on the main page, Save
   to rush2049pc.ini; quit and start again. Your friend does the same.
2. Race a ghost race. A ghost the game saves on the pak becomes a file in
   saves/p1 (see Known limits).
3. F1 > Saves, choose the ghost (such as T1 fwd ALICE 1:02.35: the track,
   the way, the player, the time), then Export (.r49ghost). The file goes
   in saves/p1/export/; F1 > Saves > Open the saves folder shows it.
4. Send the .r49ghost file. Your friend puts it in their saves/p1/import/.
5. They start the game, or choose F1 > Saves > Import ghosts (the ghost
   then reaches the game at the title screen, or at the next start). It is
   checked and put on the pak, and a ghost race on that track and way
   offers it beside their own. A file that fails the check goes to
   import/refused/, and the log says why.
docs/port/SAVES.md in the source has the rest, and tools/port/ghost.py
reads and converts the files.


Reporting a bug
---------------
https://github.com/cabi24/Rush2049PC/issues
1. F1 > About > Copy to the clipboard, and paste it into the report: the
   version, the graphics driver, the sound output, the controllers and
   where the files are.
2. Attach rush2049pc.log (rush2049pc.old.log if it happened in the run
   before). F1 > About > Open the data folder shows where it is.
3. Say what you did and what happened.
Everything the program prints is in the log too; ./rush2049pc --version
says which build you have.


Known limits
------------
- Played on one Windows laptop so far; the rest by scripted test runs.
- In loud scenes the sound clips, as the game's own mixer saturates.
- Split screen keeps its HUD in the middle.
- A ghost is written to the controller pak only after the game's save
  prompt.
- Run one copy of the program at a time on one set of saves: two at once
  each write their own game's saves over the other's.
- A high frame rate at a laptop panel's full resolution (2880x1620): an
  integrated GPU draws few frames between the game's own. Resolution
  1080p in fullscreen, or a whole render scale, gives it room.


Troubleshooting
---------------
- No window, or "no graphics": update Mesa or the GPU driver. Without
  DISPLAY or WAYLAND_DISPLAY the program draws off screen (--window asks
  for a window).
- No Open ROM... button: install zenity (or kdialog), or copy the ROM next
  to the program as baserom.us.z64.
- A controller does nothing: F1 > Controllers shows who plays each player
  and what the stick gives, and the log names each controller found. A pad
  the program doesn't know takes a player at its first button press;
  rebind it there, or put SDL's gamecontrollerdb.txt (with a line for it)
  next to rush2049pc.ini.


Licences
--------
This package contains no game data. The program includes SDL2 and uses
the system's zlib; their licences are in LICENSES/.
