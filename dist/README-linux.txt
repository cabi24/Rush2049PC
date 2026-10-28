Rush 2049 PC for Linux
======================

A native PC port of San Francisco Rush 2049 (Nintendo 64, USA), compiled from the decompilation's C. It holds no
game data: you need your own ROM of the cartridge. https://github.com/cabi24/Rush2049PC

Playing
-------
1. Copy your ROM next to the program and name it baserom.us.z64. It must be San Francisco Rush 2049 (USA),
   SHA-1 3f99351d7bb61656614bdb2aa1a90cfe55d1922c. .z64, .v64 and .n64 dumps all work; a .zip must be
   extracted first.
2. Run ./rush2049pc, or ./rush2049pc ~/roms/rush2049.z64 to name the ROM. ./rush2049pc --help lists every
   option.

Where the program looks for the ROM, in this order:
  - the path on the command line (rush2049pc PATH, or --rom PATH);
  - game.rom in rush2049pc.ini, when it is a full path;
  - otherwise baserom.us.z64 (or the name game.rom gives) next to the program, then in
    ~/.local/share/rush2049pc ($XDG_DATA_HOME), then in the working directory.
If there is no ROM, or the file is not the right one, the program says where it looked, what the file is and
what is needed (in a message box too, when it has a display). With a display and zenity or kdialog installed,
the box has Open ROM...: choose the file, and the program checks it (offering another choice if it is the
wrong one) and keeps its path as game.rom in rush2049pc.ini, so it is found from then on.

You need OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver) and glibc 2.35 or later (Ubuntu 22.04, Debian 12,
Fedora 36 or newer), on x86-64 or aarch64 (64-bit ARM, such as a Raspberry Pi 4 or 5). SDL2 is built into the
program; X11 or Wayland, PulseAudio, PipeWire or ALSA, and controllers are used through the system's libraries.

Controls
--------
  N64               Keyboard          Xbox                      PlayStation
  Stick             arrow keys        left stick                left stick
  A (accelerate)    X                 A, right trigger (RT)     Cross, R2
  B (brake)         C                 X, left trigger (LT)      Square, L2
  Z                 Z                 LB                        L1
  L                 Q                 Back (View)               Share (Create)
  R                 E                 RB                        R1
  START             Enter             Start (Menu)              Options
  C buttons         I J K L           right stick; Y C-up, B C-down  right stick; Triangle, Circle
  D-pad             W A S D           D-pad                     D-pad
  Other pads: the button in the same place. F1 > Controllers rebinds every button, and sets the stick's dead
  zone and sensitivity.
  Each player's N64 accessory is in F1 > Controllers (P1-P4 accessory): a controller pak (saves), a rumble pak
  or none. Player 1 has the controller pak; players 2-4 are on auto, a rumble pak when their pad can rumble, so
  every player's pad rumbles in multiplayer. A player given a controller pak keeps their own players and ghosts
  on it (rush2049pc-p2.mpk...). A change there is a swap, as on the console.

  F1                settings overlay (both stick clicks on a controller): video, sound, key and button mapping
  Alt+Enter, F11    fullscreen
  F12               screenshot (PNG)
  Pause             hold the game
  Escape            quit
  The overlay takes the mouse too: point and click (left or right of a value changes it), the wheel scrolls, the
  right button or a click outside goes back. F1 > Help lists what each key and button does with your bindings;
  F1 > About gives the version, graphics driver, sound output, controllers and where every file is. During play
  the mouse cursor hides until the mouse moves.

Settings and saves
------------------
The release archive is portable: because rush2049pc.ini is next to the program, everything it keeps is in its
folder:
  rush2049pc.ini    the settings; the first run fills it with every setting and a comment on each,
                    commented out at its default (remove the ; to keep a value)
  rush2049pc.mpk    the saves (the game's controller pak, player 1's)
  rush2049pc-p2.mpk, -p3, -p4
                    players 2-4's controller paks, when they have one (backed up the same way)
  rush2049pc.mpk.bak, .bak2, .bak3
                    backups of the saves: the last three that a run started with (newest first)
  screenshots/      F12's pictures
  texture_cache/    upscaled textures kept for the next run (Textures: upscaled)
  rush2049pc.log    what the program printed last time (rush2049pc.old.log: the time before)
Without rush2049pc.ini next to it (an installed copy), the settings are in ~/.config/rush2049pc
($XDG_CONFIG_HOME) and the rest in ~/.local/share/rush2049pc ($XDG_DATA_HOME).

The F1 overlay changes most settings while you play and can save them into rush2049pc.ini. To move to a new
version, copy rush2049pc.mpk and rush2049pc.ini into its folder.

The saves are written whole or not at all (a temp file, synced, then renamed), so a crash or a power cut can't
leave half a file. If rush2049pc.mpk is damaged when the program starts, it is not overwritten: it is renamed
rush2049pc.mpk.corrupt-DATE, the newest good backup takes its place (or the game starts with an empty pak), and
the program says so (in a message box too, with a display).

Picture
-------
The game is drawn at the window's own pixels by default (F1 > Video > Render scale: native), so it is sharp at
any size and on a high-DPI display: a 2880x1620 screen gets all 2880x1620 pixels. Render
scale N64 (1x) is the original 320x240 in square pixels; 2x-8x and auto (the largest whole multiple) are there
too. Widescreen is auto (the window's shape); in a race the HUD then sits at the screen's edges (F1 > Video >
Race HUD: centered keeps it in the middle 4:3 area, as on the N64). Fullscreen (F11) is borderless over the
desktop; Fullscreen mode exclusive switches the display to the Resolution setting (720p ... 1620p = 2880x1620,
2160p). The window opens at two thirds of the desktop and remembers its size and place. A slow GPU: Resolution
1080p in fullscreen, or a lower render scale.

Textures
--------
The F1 overlay's Video page has Textures:
  original   the game's own textures
  upscaled   the 3D world's textures (road, buildings, signs, cars, sky) upscaled 2x, or 4x from render scale
             4 (native at 1080p and up); the HUD and menus as they are. It needs a render scale of 2 or more
             (native in a window of 480 rows or more, or 2x-8x).
             New textures are upscaled in the background as they appear, and kept in texture_cache/, so the next
             run has them at once. Deleting that folder is safe.
  pack       a texture pack: PNG replacements in the textures/ folder (video.texture_pack in rush2049pc.ini)
To make a pack from your own ROM: set video.texture_dump = dump in rush2049pc.ini and play; every texture drawn is
written into dump/ as PNG. Edit or upscale them (tools/texpack/texpack.py in the source runs an upscaler you
install, such as realesrgan-ncnn-vulkan, over the whole folder), put the results in textures/ under the same names
(any whole multiple of the size), and choose pack. Packs are made from your own ROM: don't share them.

Troubleshooting
---------------
- Everything the program prints is also written to rush2049pc.log in this folder (in
  ~/.local/share/rush2049pc without rush2049pc.ini); the run before is kept as rush2049pc.old.log.
- No Open ROM... button: install zenity (or kdialog), or copy the ROM next to the program as baserom.us.z64.
- Saves gone after a crash or a full disk: with the program closed, copy rush2049pc.mpk.bak (or .bak2,
  .bak3) over rush2049pc.mpk.
- A controller does nothing, or its stick doesn't steer: F1 > Controllers shows who plays each player and
  what the stick gives, live, and the log names each controller found and its player. A pad SDL doesn't know
  gets a generic mapping and takes a player at its first button press: rebind it in F1 > Controllers, or put
  SDL's gamecontrollerdb.txt (with a line for it) next to rush2049pc.ini.
- Problems: https://github.com/cabi24/Rush2049PC/issues (attach rush2049pc.log; ./rush2049pc --version says
  which build you have). F1 > About > Copy to the clipboard gives the version, driver and paths to paste in, and
  Open the data folder shows the log's folder in the file manager (xdg-open).

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2 and zlib; their licences are in LICENSES/.
