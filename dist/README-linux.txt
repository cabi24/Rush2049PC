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

You need OpenGL ES 3.0 or OpenGL 3.1 (Mesa or a GPU driver), and SDL2 for the window, controllers and sound.

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

Settings
--------
rush2049pc.ini holds every setting, with a comment on each. The program looks for it next to the program first,
and otherwise uses ~/.config/rush2049pc/rush2049pc.ini ($XDG_CONFIG_HOME), which the first run writes with the
defaults. The F1 overlay changes most settings while you play and can save them into the file.

Saves
-----
The game saves to a controller pak image, rush2049pc.mpk, in the folder the program runs in.

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2 and zlib; their licences are in LICENSES/.
