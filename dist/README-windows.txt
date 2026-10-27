Rush 2049 PC for Windows (x86-64)
=================================

A native PC port of San Francisco Rush 2049 (Nintendo 64, USA), compiled from the decompilation's C. It holds no
game data: you need your own ROM of the cartridge. https://github.com/cabi24/Rush2049PC

Playing
-------
1. Copy your ROM into this folder and name it baserom.us.z64. It must be San Francisco Rush 2049 (USA),
   SHA-1 3f99351d7bb61656614bdb2aa1a90cfe55d1922c. .z64, .v64 and .n64 dumps all work; a .zip must be
   extracted first.
2. Double-click rush2049pc.exe. A console window opens next to the game's window and shows what the program
   is doing; closing either one quits.

From a command prompt you can name the ROM instead: rush2049pc.exe --rom D:\roms\rush2049.z64
rush2049pc.exe --help lists every option.

You need a graphics driver with OpenGL 3 (any GPU from the last ten years), on 64-bit Windows 10 or 11.

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
rush2049pc.ini holds every setting, with a comment on each. The program looks for it next to rush2049pc.exe
first, and otherwise uses %APPDATA%\rush2049pc\rush2049pc.ini, which the first run writes with the defaults.
The F1 overlay changes most settings while you play and can save them into the file.

Saves
-----
The game saves to a controller pak image, rush2049pc.mpk, in the folder the program runs in (this folder when
you double-click it). Keep that file to keep your saves.

Troubleshooting
---------------
- "cannot open the ROM": the ROM isn't where the program looked. Put it in this folder as baserom.us.z64, or
  give its path with --rom.
- "has SHA-1 ...": the file is not the USA cartridge, or it is a bad or modified dump.
- No window, or "no renderer": update the graphics driver; the game needs OpenGL 3.
- To keep the console's messages, run it from a command prompt:
    rush2049pc.exe > rush2049pc.log 2>&1
- Problems and logs: https://github.com/cabi24/Rush2049PC/issues

Licences
--------
The game and its code are the copyright of their original publisher; no licence is granted for the port's code.
The program includes SDL2, zlib and the mingw-w64 and GCC runtime libraries; their licences are in LICENSES\.
