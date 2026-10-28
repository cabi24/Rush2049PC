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
what is needed (in a message box too, when it has a display).

You need OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver) and glibc 2.35 or later (Ubuntu 22.04, Debian 12,
Fedora 36 or newer), on x86-64 or aarch64 (64-bit ARM, such as a Raspberry Pi 4 or 5). SDL2 is built into the
program; X11 or Wayland, PulseAudio, PipeWire or ALSA, and controllers are used through the system's libraries.

Controls
--------
  N64               Keyboard          Controller (Xbox layout)
  Stick             arrow keys        left stick
  A (accelerate)    X                 A, right trigger
  B (brake)         C                 X, left trigger
  Z                 Z                 left shoulder (LB)
  L                 Q                 Back (View)
  R                 E                 right shoulder (RB)
  START             Enter             Start (Menu)
  C buttons         I J K L           right stick, or Y (C-up) and B (C-down)
  D-pad             W A S D           D-pad

  F1                settings overlay (both stick clicks on a controller): video, sound, key and button mapping
  Alt+Enter, F11    fullscreen
  F12               screenshot (PNG)
  Pause             hold the game
  Escape            quit

Settings and saves
------------------
The release archive is portable: because rush2049pc.ini is next to the program, everything it keeps is in its
folder:
  rush2049pc.ini    the settings; the first run fills it with every setting and a comment on each,
                    commented out at its default (remove the ; to keep a value)
  rush2049pc.mpk    the saves (the game's controller pak)
  screenshots/      F12's pictures
  rush2049pc.log    what the program printed last time (rush2049pc.old.log: the time before)
Without rush2049pc.ini next to it (an installed copy), the settings are in ~/.config/rush2049pc
($XDG_CONFIG_HOME) and the rest in ~/.local/share/rush2049pc ($XDG_DATA_HOME).

The F1 overlay changes most settings while you play and can save them into rush2049pc.ini. To move to a new
version, copy rush2049pc.mpk and rush2049pc.ini into its folder.

Troubleshooting
---------------
- Everything the program prints is also written to rush2049pc.log in this folder (in
  ~/.local/share/rush2049pc without rush2049pc.ini); the run before is kept as rush2049pc.old.log.
- Problems: https://github.com/cabi24/Rush2049PC/issues (attach rush2049pc.log; ./rush2049pc --version says
  which build you have)

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2 and zlib; their licences are in LICENSES/.
