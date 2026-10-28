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
This folder is portable: because rush2049pc.ini is next to rush2049pc.exe, everything the program keeps is in
this folder:
  rush2049pc.ini    the settings; the first run fills it with every setting and a comment on each,
                    commented out at its default (remove the ; to keep a value)
  rush2049pc.mpk    the saves (the game's controller pak)
  screenshots\      F12's pictures
  texture_cache\    upscaled textures kept for the next run (Textures: upscaled)
  rush2049pc.log    what the program printed last time (rush2049pc.old.log: the time before)
Without rush2049pc.ini next to it, the program keeps them in %APPDATA%\rush2049pc instead.

The F1 overlay changes most settings while you play and can save them into rush2049pc.ini. To move to a new
version, copy rush2049pc.mpk and rush2049pc.ini into its folder.

Picture
-------
The game is drawn at the window's own pixels by default (F1 > Video > Render scale: native), so it is sharp at
any size and at any Windows display scaling: a 2880x1620 screen at 200% gets all 2880x1620 pixels. Render
scale N64 (1x) is the original 320x240 in square pixels; 2x-8x and auto (the largest whole multiple) are there
too. Widescreen is auto (the window's shape). Fullscreen (F11) is borderless over the desktop; Fullscreen mode
exclusive switches the display to the Resolution setting (720p ... 1620p = 2880x1620, 2160p). The window opens
at two thirds of the desktop and remembers its size and place. A slow GPU: Resolution 1080p in fullscreen, or
a lower render scale.

Textures
--------
The F1 overlay's Video page has Textures:
  original   the game's own textures
  upscaled   the 3D world's textures (road, buildings, signs, cars, sky) upscaled 2x, or 4x from render scale
             4 (native at 1080p and up); the HUD and menus as they are. It needs a render scale of 2 or more
             (native in a window of 480 rows or more, or 2x-8x).
             New textures are upscaled in the background as they appear, and kept in texture_cache\, so the next
             run has them at once. Deleting that folder is safe.
  pack       a texture pack: PNG replacements in the textures\ folder (video.texture_pack in rush2049pc.ini)
To make a pack from your own ROM: set video.texture_dump = dump in rush2049pc.ini and play; every texture drawn is
written into dump\ as PNG. Edit or upscale them (tools\texpack\texpack.py in the source runs an upscaler you
install, such as realesrgan-ncnn-vulkan, over the whole folder), put the results in textures\ under the same names
(any whole multiple of the size), and choose pack. Packs are made from your own ROM: don't share them.

Troubleshooting
---------------
- "no ROM": the ROM isn't where the program looked. Put it in this folder as baserom.us.z64, or drop it on
  rush2049pc.exe.
- "wrong ROM": the message says what the file is: another game, another region (the port runs the USA
  cartridge only), a bad or modified dump, or an archive to extract first.
- No window, or "no renderer": update the graphics driver; the game needs OpenGL 3.
- A controller does nothing, or its stick doesn't steer: F1 > Controllers shows who plays each player and
  what the stick gives, live, and the log names each controller found and its player. A pad SDL doesn't know
  gets a generic mapping and takes a player at its first button press: rebind it in F1 > Controllers, or put
  SDL's gamecontrollerdb.txt (with a line for it) in this folder. Xbox pads go through XInput.
- The log: everything the console shows is also written to rush2049pc.log, in this folder (or in
  %APPDATA%\rush2049pc without rush2049pc.ini: type that into Explorer's address bar). The console's first
  line says where. The run before is kept as rush2049pc.old.log.
- If the program stops with an error, a message box says where the log is, and the console window stays
  open until you press a key.
- Problems: https://github.com/cabi24/Rush2049PC/issues (attach rush2049pc.log; rush2049pc.exe --version
  says which build you have). F1 > About > Copy to the clipboard gives the version, driver and paths to paste
  in, and Open the data folder shows the log's folder in Explorer.

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2, zlib and the mingw-w64 and GCC runtime libraries; their licences are in LICENSES\.
