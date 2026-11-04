Mods for Rush 2049 PC
=====================

Put each mod in this folder: a folder of its own (or a .zip of one), with a
mod.ini in it. A mod's files stand in for the cartridge's: cars, tracks,
textures, text and sounds. Your ROM is never changed; with no mods here (or
with game.mods = off, or --no-mods) the game is the N64's.

  mods/
    red-cars/
      mod.ini            name, author, version, game = us, priority
      cars/car01.bin     a file by its name (or wave_088.bin, by number)
      textures/          a texture pack: PNG files named as the dump names them
      text/en.txt        text overrides: [single] then 12 = NEW TEXT

At start the program checks every file of the mods that are on. A file the
game can't use (cut short, the wrong kind, a bad header) is refused with one
line in rush2049pc.log, and the cartridge's own file is used instead. Of two
mods with a file for the same thing, the one with the higher priority (or
first on the settings overlay's Mods page) wins, and the log says so. When
the mods' files are larger than the cartridge's, the game is given more
memory, as game.memory would, and the log says how much.

F1 > Mods lists the mods found: turn them on and off and put them in order
there. Changes apply at the next start. The log and F1 > About say which
mods are in use.

Making a mod: tools/mods/rushmods.py (in the source, plain Python) lists
your ROM's files, unpacks them as mods use them, makes a mod's skeleton and
checks a mod. docs/port/MODS.md has the names, the formats and the limits.
Files unpacked from your ROM are the ROM's: share only your own work.
