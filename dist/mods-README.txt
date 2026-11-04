Mods for Rush 2049 PC
=====================

Put each mod in this folder: a folder of its own (or a .zip of one), with a
mod.ini in it. A mod's files stand in for the cartridge's: cars, tracks,
fonts, the HUD's and the menus' models and textures, text and sounds. Your
ROM is never changed; with no mods here (or with game.mods = off, or
--no-mods) the game is the N64's.

  mods/
    red-cars/
      mod.ini            name, author, version, game = us, priority
      cars/car01.bin     a file by its name (or wave_088.bin, by number)
      textures/          a texture pack: PNG files named as the dump names them
      text/en.txt        text overrides: [single] then 12 = NEW TEXT

At start the program checks every file of the mods that are on. A file the
game can't use (cut short, the wrong kind, a bad header) is refused with one
line in rush2049pc.log, and the cartridge's own file is used instead. Of two
mods with a file for the same thing, the one first in the order wins, and
the log says so: the order is the one set on the settings overlay's Mods
page, then the higher mod.ini priority. When the mods' files are larger
than the cartridge's, the game is given more memory, as game.memory would,
and the log says how much (with game.expansion_pak = off, a larger file is
refused instead).

F1 > Mods lists the mods found: turn them on and off and put them in order
there. Changes apply at the next start. The log and F1 > About say which
mods are in use.


Making a mod
------------
The tool is tools/mods/rushmods.py in the source
(https://github.com/cabi24/Rush2049PC): plain Python 3.8 or later, run on
your own ROM. "python3" below is "py" on Windows.
1. Make the mod's folder, here:
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
