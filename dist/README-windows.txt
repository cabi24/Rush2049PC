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

Or drag the ROM file onto rush2049pc.exe, whatever it is called and wherever it is. From a command prompt:
rush2049pc.exe D:\roms\rush2049.z64 (or --rom PATH). rush2049pc.exe --help lists every option.

Where the program looks for the ROM, in this order:
  - the file dropped on it, or named on the command line;
  - game.rom in rush2049pc.ini, when it is a full path (C:\...);
  - otherwise baserom.us.z64 (or the name game.rom gives) in this folder, then in %APPDATA%\rush2049pc, then
    in the folder the program was started in.
If there is no ROM, or the file is not the right one, a message box says where it looked, what the file is,
and what is needed.

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

Settings and saves
------------------
This folder is portable: because rush2049pc.ini is next to rush2049pc.exe, everything the program keeps is in
this folder:
  rush2049pc.ini    the settings; the first run fills it with every setting and a comment on each,
                    commented out at its default (remove the ; to keep a value)
  rush2049pc.mpk    the saves (the game's controller pak)
  screenshots\      F12's pictures
  rush2049pc.log    what the program printed last time (rush2049pc.old.log: the time before)
Without rush2049pc.ini next to it, the program keeps them in %APPDATA%\rush2049pc instead.

The F1 overlay changes most settings while you play and can save them into rush2049pc.ini. To move to a new
version, copy rush2049pc.mpk and rush2049pc.ini into its folder.

Troubleshooting
---------------
- "no ROM": the ROM isn't where the program looked. Put it in this folder as baserom.us.z64, or drop it on
  rush2049pc.exe.
- "wrong ROM": the message says what the file is: another game, another region (the port runs the USA
  cartridge only), a bad or modified dump, or an archive to extract first.
- No window, or "no renderer": update the graphics driver; the game needs OpenGL 3.
- The log: everything the console shows is also written to rush2049pc.log, in this folder (or in
  %APPDATA%\rush2049pc without rush2049pc.ini: type that into Explorer's address bar). The console's first
  line says where. The run before is kept as rush2049pc.old.log.
- If the program stops with an error, a message box says where the log is, and the console window stays
  open until you press a key.
- Problems: https://github.com/cabi24/Rush2049PC/issues (attach rush2049pc.log; rush2049pc.exe --version
  says which build you have)

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2, zlib and the mingw-w64 and GCC runtime libraries; their licences are in LICENSES\.
