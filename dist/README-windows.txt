Rush 2049 PC for Windows (x86-64)
=================================

A native PC port of San Francisco Rush 2049 (Nintendo 64, USA), compiled from the decompilation's C. It holds no
game data: you need your own ROM of the cartridge. https://github.com/cabi24/Rush2049PC

Playing
-------
1. Double-click rush2049pc.exe.
2. The first time, it asks for your ROM: click Open ROM... and choose the file. It must be San Francisco
   Rush 2049 (USA), SHA-1 3f99351d7bb61656614bdb2aa1a90cfe55d1922c. .z64, .v64 and .n64 dumps all work; a
   .zip must be extracted first. If the file is not the right one, the program says what it is and lets you
   choose another. It remembers the file (game.rom in rush2049pc.ini), so from then on a double-click starts
   the game in its own window.

Or copy your ROM into this folder as baserom.us.z64 before the first start: then it asks nothing.

Or drag the ROM file onto rush2049pc.exe, whatever it is called and wherever it is. From a command prompt:
rush2049pc.exe D:\roms\rush2049.z64 (or --rom PATH). rush2049pc.exe --help lists every option. The program
prints into the command prompt's window, but the prompt doesn't wait for it, so its output can come after the
next prompt (press Enter for a new one). To wait for it: start /wait rush2049pc.exe --help (cmd), or
rush2049pc.exe --help | Out-Host (PowerShell).

Where the program looks for the ROM, in this order:
  - the file dropped on it, or named on the command line;
  - game.rom in rush2049pc.ini, when it is a full path (C:\...);
  - otherwise baserom.us.z64 (or the name game.rom gives) in this folder, then in %APPDATA%\rush2049pc, then
    in the folder the program was started in.
If there is no ROM, or the file is not the right one, a message box says where it looked, what the file is,
and what is needed, with Open ROM... to choose the file.

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
  Each player's N64 accessory is in F1 > Controllers (P1-P4 accessory): a controller pak (saves), a rumble pak
  or none. Player 1 has the controller pak; players 2-4 are on auto, a rumble pak when their pad can rumble, so
  in multiplayer each player's rumble goes to their own pad. A player can have a controller pak of their own
  instead (rush2049pc-p2.mpk...). A change there is a swap, as on the console.

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
  rush2049pc.mpk    the saves (the game's controller pak, player 1's)
  rush2049pc-p2.mpk, -p3, -p4
                    players 2-4's controller paks, when they have one (backed up the same way)
  rush2049pc.mpk.bak, .bak2, .bak3
                    backups of the saves: the last three that a run started with (newest first)
  screenshots\      F12's pictures
  texture_cache\    upscaled textures kept for the next run (Textures: upscaled)
  rush2049pc.log    what the program printed last time (rush2049pc.old.log: the time before)
Without rush2049pc.ini next to it, the program keeps them in %APPDATA%\rush2049pc instead.

The F1 overlay changes most settings while you play and can save them into rush2049pc.ini. To move to a new
version, copy rush2049pc.mpk and rush2049pc.ini into its folder.

The saves are written whole or not at all, so a crash or a power cut can't leave half a file. If
rush2049pc.mpk is damaged when the program starts, it is not overwritten: it is renamed
rush2049pc.mpk.corrupt-DATE, the newest good backup takes its place (or the game starts with an empty pak),
and a message box says so.

Picture
-------
The game is drawn at the window's own pixels by default (F1 > Video > Render scale: native), so it is sharp at
any size and at any Windows display scaling: a 2880x1620 screen at 200% gets all 2880x1620 pixels. Render
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
             New textures are upscaled in the background as they appear, and kept in texture_cache\, so the next
             run has them at once. Deleting that folder is safe.
  pack       a texture pack: PNG replacements in the textures\ folder (texture_pack in rush2049pc.ini's
             [video] section)
To make a pack from your own ROM: in rush2049pc.ini's [video] section, change "; texture_dump = " to
"texture_dump = dump" and play; every texture drawn is written into dump\ as PNG. Edit or upscale them
(tools\texpack\texpack.py in the source runs an upscaler you install, such as realesrgan-ncnn-vulkan, over the
whole folder), put the results in textures\ under the same names (any whole multiple of the size), and choose
pack. Packs are made from your own ROM: don't share them.

Troubleshooting
---------------
- "no ROM": the ROM isn't where the program looked. Click Open ROM... and choose it, put it in this folder as
  baserom.us.z64, or drop it on rush2049pc.exe.
- "wrong ROM": the message says what the file is: another game, another region (the port runs the USA
  cartridge only), a bad or modified dump, or an archive to extract first. Choose another... picks again.
- If the ROM you chose has moved, the program asks again. A path with letters Windows' code page can't spell
  (some non-Latin folder names) may not open: copy the ROM to a folder such as C:\Games and choose it there.
- Saves gone after a crash or a full disk: close the program and copy rush2049pc.mpk.bak (or .bak2, .bak3)
  over rush2049pc.mpk.
- No window, or "no renderer": update the graphics driver; the game needs OpenGL 3.
- A controller does nothing, or its stick doesn't steer: F1 > Controllers shows who plays each player and
  what the stick gives, live, and the log names each controller found and its player. A pad SDL doesn't know
  gets a generic mapping and takes a player at its first button press: rebind it in F1 > Controllers, or put
  SDL's gamecontrollerdb.txt (with a line for it) in this folder. Xbox pads go through XInput.
- The log: everything the program prints is written to rush2049pc.log, in this folder (or in
  %APPDATA%\rush2049pc without rush2049pc.ini: type that into Explorer's address bar). The run before is kept
  as rush2049pc.old.log.
- If the program stops with an error, a message box says what went wrong and where the log is.
- Problems: https://github.com/cabi24/Rush2049PC/issues (attach rush2049pc.log; rush2049pc.exe --version
  says which build you have). F1 > About > Copy to the clipboard gives the version, driver and paths to paste
  in, and Open the data folder shows the log's folder in Explorer.

Licences
--------
This package contains no game data: you need your own ROM of the cartridge. The program includes
SDL2, zlib and the mingw-w64 and GCC runtime libraries; their licences are in LICENSES\.
