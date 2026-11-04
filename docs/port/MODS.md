# Mods: the game's files from a mods folder (port/mods, port/assets/check.c)

Written 2026-11-04. A mod is a folder (or a .zip of one) in the mods folder that holds files to use in place of the
cartridge's: cars, tracks, fonts, the front end's and the HUD's models and textures, text, sounds. The player's ROM
is never changed: at start the program reads the mods that are on, checks every file, and puts the ones it takes in a
second region of the cartridge, where the game reads them as it reads its own. A file the game can't use is refused
with one line in the log and the cartridge's file is used. With no mod that is on, nothing is different from before:
the file table, the reads and the memory are the N64's.

| Piece | Where |
|---|---|
| Reading the mods, settling each wave's file, the text overrides, the memory, the file table | `port/mods/mods.c`, `mods.h` |
| A mod as a .zip | `port/mods/zip.c` |
| The rules a file must meet | `port/assets/check.c` (`pc_assets_check`), `port/assets/musyx.c` (`pc_assets_musyx_check`) |
| The cartridge's mods region | `port/ultra/pi.c` (`pc_rom_set_mods`, `mods_at`), `PC_ROM_MODS_BASE` in `port/ultra/ultra.h` |
| Start: the ROM read first, then the mods, then the window; the file table after the game's data | `port/main.c` (`mods_prepare`, `mods_install`) |
| The settings | `port/config/config.c` (`game.mods`, `game.mods_dir`, `game.mods_order`, `[mods]`) |
| The overlay's Mods page, About's Mods row | `port/input/menu.c` (`mods_page`, `mod_move`, `pc_menu_set_mods`) |
| The mods' texture packs | `port/gfx/gfx_tex.c` (`mods_read`, `pcgfx_tex_mod_has`), `port/gfx/gfx_rdp.c` (`tex_lookup`) |
| For modders: list, extract, check, new | `tools/mods/rushmods.py` |
| Tests | `port/assets/tests/test_mods.c` (`assets_mods`), `mods_tool`, `frontend.mods`, `frontend.menu.mods`, `tools/port/mods_synth.py` and the `mods_*` paths |

## 1. Using mods

- **Where.** `mods/` in the data folder ([RELEASE.md](RELEASE.md) "Files"): next to the program in a portable copy
  (the release archives ship it, with a README), else `~/.local/share/rush2049pc/mods` on Linux
  (`$XDG_DATA_HOME/rush2049pc/mods`), `%APPDATA%\rush2049pc\mods` on Windows. `game.mods_dir` names another (a name
  alone is in the data folder, a full path as it is); `--mods-dir PATH` too, from the working directory. The program
  doesn't make the folder.
- **On and off.** `game.mods = on` (the default) uses the mods that are on; `off`, or `--no-mods`, uses the cartridge's
  files only (the mods are still listed on the Mods page). Each mod is on unless rush2049pc.ini's `[mods]` lists its
  folder `off`:

  ```
  [game]
  mods_order = red-cars, long-track
  [mods]
  old-hud = off
  ```

- **The order.** Of two mods with a file for the same wave, the first in the order's is used and the log names both.
  The order is `game.mods_order`'s folders first, as listed, then the other mods by their mod.ini `priority` (higher
  first), then by folder name. A file that is refused gives way to the next mod's for that wave, then to the
  cartridge's. Text overrides work string by string: the first mod's string wins.
- **The log at start** (the console and rush2049pc.log) says what was found and used, in one line, then a line for
  each problem:

  ```
  rush2049pc: mods: 2 on: Red Cars 1.2 (13 files), Long Track 0.3 (4 files, text, textures) in /home/me/.local/share/rush2049pc/mods
  rush2049pc: mods: Red Cars: cars/car04.bin (wave 91, car04) refused: model 'BODY' LOD 0 runs past the file's end (at 0x1A2C8) before its G_ENDDL; the next mod's or the cartridge's file is used
  rush2049pc: mods: wave 101, track1: Long Track's tracks/track1.bin and Old Track's tracks/track1.bin; Long Track is first in the order: its file is used
  rush2049pc: mods: game.memory raised to 32 MiB: the files need up to 18.9 MB more heap than the cartridge's (a car file counted for each of a race's 6 cars)
  rush2049pc: mods: 17 of the cartridge's files replaced (40213 KiB)
  ```

  (the names are made up). With no mods: `rush2049pc: mods: none in DIR`.
- **The overlay** (F1 > Mods): `Mods` (`game.mods`), then each mod found in the order in effect, `1. Red Cars 1.2` with
  `on` or `off` (`on (unusable)` for one that can't be used: no mod.ini, another game's); the footer says who made it
  and what it gives ("by Someone: 13 files (1 refused), textures; at the next start"), or why it is unusable. Left and
  Right turn it on and off; Enter (or A) picks it up, Up and Down move it, Enter again puts it down. Then the folder and
  Defaults (every mod on, the order mod.ini's). Save writes `[mods]` and `game.mods_order` like any setting. Every
  change applies at the next start (the items are marked `*`): the files are read before the game's memory is mapped,
  and the game holds them from then on. About's `Mods` row says which mods are in use.

## 2. Making a mod

```
mods/
  red-cars/                 the folder's name is the mod's key in rush2049pc.ini: no = ; # [ ] , or quotes
    mod.ini
    cars/car01.bin          a file by its name ...
    wave_090.bin            ... or by its wave number; any folders, any depth
    cars/car02.lzs          the cartridge's stored form: LZSS (.lzs) or raw DEFLATE (.deflate)
    textures/               a texture pack, PNG files named as the dump names them (TEXTURES.md)
    text/en.txt             text overrides: en, es, it, de, nl, fr
    README.txt              anything else named README*, *.txt, *.md, *.png, *.jpg, *.html is left alone
```

**mod.ini**, in rush2049pc.ini's format:

```
[mod]
name = Red Cars
author = Someone
version = 1.2
description = Every car red
game = us
priority = 0
```

- `game` says which game it is for: `us` (or `usa`, `NRUE`), San Francisco Rush 2049 (USA), the cartridge the port runs
  (its file table is the USA cartridge's). Missing, it is taken as `us` with a line in the log; anything else and the
  mod isn't used. `priority` is -1000 to 1000 (0); `name` is the folder's when missing. Other keys are logged and left.
- **A file's name** is its wave's name or `wave_NNN` (any digits, `wave88` too), then `.bin` or no extension for the
  file as the game reads it (decompressed: what `rushmods.py extract` writes), `.lzs` or `.deflate` for the stored form,
  which the program decompresses at start (a bad stream: refused). The folder it is in doesn't matter. Names are
  section 3's. A file of another extension, or a name that is no wave, is logged and left; waves 69-75 (no game code
  loads them) too. A mod with two files for one wave uses the first by path order, and says so.
- **A .zip** is read whole (stored or deflated entries; no zip64, no encryption), its files as a folder's; a zip of one
  folder (as zipping a mod's folder makes) is that folder. Its key is its name without `.zip`. Its texture pack is
  unpacked into `mods_cache/<mod>/textures` in the data folder at each start, for the texture code.
- **Text overrides** (`text/en.txt`, and es, it, de, nl, fr for the other banks, waves 0-5): the three sections of
  string offsets and a string's number:

  ```
  [single]
  12 = "PRESS START"
  13 = Two\nlines
  [taba]
  0 = \xA9 2026
  ```

  `[single]`, `[taba]` and `[tab2b]` are sections 0, 2 and 5 of the text bank (ASSETS.md 3.3; `[0]`, `[2]`, `[5]`
  too). `\n` is a new line, `\t` a tab, `\xHH` a byte, `\\` a backslash; a value in double quotes keeps its spaces (a
  quote inside is `\x22`). Each new string is put after the bank's end and its offset pointed there; the other strings
  and the tables stay. A number the bank doesn't have is logged and left. The overrides go over the bank in use, a
  mod's replacement of the whole bank or the cartridge's.
- **Textures**: `textures/` is a texture pack as video.textures' (TEXTURES.md "Texture packs"): PNG files named
  `rgba16_32x32_<hash>.png` as `video.texture_dump` names them, any whole multiple of the size, in folders of any
  depth. The mods' packs are used in every textures mode: with `original` a texture a mod has is drawn from it and
  the rest as on the N64; with `upscaled` the rest is upscaled; with `pack` the mods' come before the player's own
  pack. The first mod in the order wins a texture.

**tools/mods/rushmods.py** (Python 3.8, stdlib only), on your own ROM:

```
python3 tools/mods/rushmods.py names                         every wave's name, folder and format
python3 tools/mods/rushmods.py list baserom.us.z64           the ROM's files: wave, name, format, kind, size, stored size
python3 tools/mods/rushmods.py extract baserom.us.z64 out/ cars/car01 101-119 --text
python3 tools/mods/rushmods.py new red-cars --dir mods --author Someone
python3 tools/mods/rushmods.py check mods/red-cars --rom baserom.us.z64
```

`extract` writes `out/<folder>/<name>.bin` (with `--stored`, the stored form), and with `--text` every string of the six
banks as `out/text/<language>.txt`, the overrides' form: copy the lines you change into your mod. `check` says, file by
file, what the game will say: `ok` with the size against the cartridge's, or `REFUSED` and why; then mod.ini's, the
names' and the overrides' problems. Without `--rom` the rules against the cartridge's own file (section 4) are
skipped. **Files extracted from your ROM are the ROM's: keep them to yourself.** A mod you share should hold only your
own work.

A typical edit: extract a car, change its texels in the file's IMAG chunk (the TXHD records say where each texture
is and its format) or its vertex colours in its display lists' vertices (16-byte `Vtx`, the colour at +12), keep the
records' names, check it, and put it in a mod. The formats are ASSETS.md 3 and LOADERS.md.

## 3. The names

| Waves | Names (folder) | Format |
|---|---|---|
| 0-5 | `text_en`, `text_es`, `text_it`, `text_de`, `text_nl`, `text_fr` (text) | text bank |
| 6, 7, 8, 9 | `project`, `pool`, `sampledir`, `samples` (sound) | MusyX project, pool, sample directory; sample data |
| 10-21 | `song01` ... `song12` (sound) | MusyX song |
| 22-37, 38-53 | `fonttex00` ... `fonttex15`, `fontdat00` ... `fontdat15` (fonts) | font WAD, font data |
| 54-60 | `attract`, `interface`, `selcar`, `selcirc`, `selcom`, `selopt`, `seltrk` (frontend) | WAD |
| 61-68 | `raceextra`, `hud`, `hudbattle`, `hudcom`, `hudlogo`, `hudmulti`, `hudstunt`, `targetsnopak` (race) | WAD |
| 69-75 | (none: no game code loads them) | |
| 76-81 | `weapons`, `wings`, `vehicles`, `vehiclesnopak`, `wheels`, `wheelsnopak` (race) | WAD |
| 82-87 | `targetst1` ... `targetst6` (race) | WAD |
| 88-100 | `car01` ... `car13` (cars) | WAD |
| 101-119 | `track1` ... `track6`, `battle1` ... `battle8`, `stunt1` ... `stunt4`, `obstacle` (tracks) | WAD with paths |
| 120-138 | the same with `_world`: `track1_world` ... `obstacle_world` (tracks) | world WAD |
| 139-157 | ... `_road` (tracks) | road |
| 158-176, 177-182 | ... `_path`; `track1_pathback` ... `track6_pathback` (tracks) | path |

The names are MAT3's where its file is the same (ASSETS.md 2.3), with numbers from 1 for the cars and tracks; the
others (61, 79, 81, 6-9) say what the file is. `rushmods.py names` prints the table; ctest `mods_tool` checks it is
mods.c's.

## 4. What a file must be (check.c)

At start, every file of a mod that is on goes through `pc_assets_check` (`port/assets/check.c:722`) as the game will
read it, with the cartridge's own file for the wave. It changes nothing, so a file is refused before the game ever sees
it. The rules are what the converters ([LOADERS.md](LOADERS.md)) and the game's set-up need to stay inside the file,
and, against the cartridge's file, what the game looks up in it:

| Format | The file's own rules | Against the cartridge's file |
|---|---|---|
| WAD (22-37, 54-119) | the TOC inside the file, not relocated, at most 4,096 entries, every entry's offset in the file; the first OBHD, TXHD, PLHD and PTHD chunks 4-aligned with their records before the next chunk; OBHD, TXHD and PLHD names ending within 16 bytes and not going down in strcmp order (the game bsearches them, `entity_name_copy` with `func_8008AD04`; one retail font WAD has a name twice, which bsearch takes); every LOD's display list 8-aligned in the file, the relocation's walk (`entity_lod_select`: one pass, E1 04 followed, to the first 0xDF) and the first G_VTX's vertex chain (`func_800966D8`) inside it and ending, and the mirror pass's walk (`display_list_traverse`, flags 2: calls at most 16 deep, branches and E1 04 jumps) inside it, ending at the word 0xDF000000, with no loop; TXLD 8-aligned in the file and its G_SETTIMG targets inside it; a texture 1-1024 a side, size 0-3, its texels (or its load list) starting in the file (IMAG plus texBase, the game's 32-bit sum); a palette's entries inside it; a path's points inside it; for a track (101-119) each path's trigPtr 0 or a path record (the camera follows it, `CamScene.link`) | every model, texture and palette name of the cartridge's file is there; a track has as many paths as the cartridge's or more |
| world (120-138) | the WAD's rules, then WHDR (its entries before the next chunk, names in order), WOBJ, GDAT and GTLD before the next chunk; every node a world reaches through next and child a WOBJ record in the file; the quadtree's children inside GDAT and its leaves' targets inside GTLD | every world name of the cartridge's file is there |
| road (139-157) | the five sections, the vertex list and the surface list fit (`road.c`); a quadtree; each node's parent a node; each quarter's child a node (its mask bit set) or 0 or a surface list inside the list, whose run-length words (`func_800ADCE0`) give exactly its count, at most 35 surfaces (`list[36]` in `entity_update` and `camera_trigger_check`; four leaves into `camera_victory`'s `polys[142]`), each a surface; each surface's vertex list offset inside the vertex list | |
| path (158-182) | the walk `path.c` makes (ReadPathData's) fits: the checkpoints (0-10), the resurrection path, the alternative paths, the four drone paths | a resurrection path, a drone path or checkpoints the cartridge's has are not empty |
| text (0-5) | the six sections fit; every string offset after the tables and a NUL-ended string in the file | each section has as many items as the cartridge's or more (the game reads them by number); each of the cartridge's strings keeps its printf conversions in order (`%d`, `%s`, ...; `%%` is none): the game formats some strings with arguments, and a string with others would have them read as what they aren't |
| font data (38-53) | the letters and the kerning table fit; each letter's texture page under the font's count | as many letters as the cartridge's or more, no more texture pages (the font WAD's) |
| MusyX (6-8, 10-21) | not all zero; `musyx.c`'s walk (every field the sound library reads, inside the file and at one width) finds no record that doesn't parse | |
| samples (9) | not empty | |

A refused file's line names the mod, the file, its wave and the first rule it broke. `rushmods.py check` applies the
same rules (ctest `mods_tool`: the same verdict as check.c's on 4,801 files, the good files of `assets_mods` and 600
changed copies of each), plus the folder's own: mod.ini, names, text overrides, the texture pack's names.

**On the retail cartridge** every one of the 176 files the game loads and the sample data passes, each against itself
(`rushmods.py check` of `tools/port/mods_retail.py`'s identity mod, the Pi, the files under the ignored build/), and
the game takes them all from the mods' region (section 8).

**What isn't checked, and so is the modder's.** The rules keep the program inside the file while it loads and sets it
up, and keep what the game looks up there. They can't say a file is right: any bytes in display lists, texels, vertex
positions, floats, sound macros or road surfaces are taken as they come, as the N64 takes them, and a wrong one can
draw garbage, drive oddly or (in the game's own code later, as on the N64) fault. The road's vertex lists past their
offset, a path's indices into its points and a song's notes are read only as far as the converters read them.

## 5. Larger files and the memory

A file larger than the cartridge's needs more heap: the game allocates each wave's block by its size in the file
table (`gWaveSizes`, [MEMORY.md](MEMORY.md) 5), which the mods set. At start, before the RDRAM window is mapped
(`plan_memory`, `mods.c:978`):

- **The growth** (`growth`, `mods.c:915`): each file's 32-byte block less the cartridge's; a car file's counted for
  each car of a race (LoadCars loads every car's own copy: 6, or `game.cars`); of the files the game holds one at a
  time (a song, a track's geometry, world, road and path), the largest; a text bank's against the first bank's block,
  which every language is read into (below); the rest added up. The sample data isn't heap.
- **Any growth at game.memory original raises it**, to the first of 16, 32, 64, 128 and 256 MiB that holds 8 MiB and
  the growth. 16 MiB at least, because at the N64's 8 MiB the heap's free space is split by the runtime images' block
  at 0x8038A400, and LoadCars compares the free total with a car file's size while the file needs one block
  (MEMORY.md 5's finding: with car files 3 MB larger, 2.98 MB free and the largest block 1.79 MB, a null block). From
  16 MiB the free space past that block is one block. The log says so ("game.memory raised to 32 MiB: ...") and About's
  Memory row shows the size; the setting itself (and the overlay's Memory) stays what the player chose, and the game
  sees what MEMORY.md 1 says of a size (its Expansion Pak features, as at `original`).
- A `game.memory` size that holds it already is kept. More than 256 MiB: the last mod's larger files are refused, one
  at a time, until the rest fit.
- **Without the Expansion Pak** (`game.expansion_pak = off`, `memory = original`: the 4 MiB console) a larger file is
  refused: a size would give the game the Expansion Pak's features ([MEMORY.md](MEMORY.md) 1), which the player
  turned off. A file of the cartridge's size or smaller is used.

## 6. How a mod's file reaches the game

1. **Before the window** (`mods_prepare`, `port/main.c:390`): main reads the ROM first now (it used to after mapping
   the window: nothing in the ROM's read needs the window), then `pc_mods_scan` (`mods.c:781`) reads the folder and
   each mod that is on, `pc_mods_cart_read` (`mods.c:250`) the cartridge's file table from the ROM's boot and game
   images (port/data's image table: `gWaveSizes` and `D_8002E860` in the raw boot image, `D_8011B5BC` and
   `D_80123564` in the inflated game image), and `pc_mods_prepare` (`mods.c:1030`) checks the files, settles each
   wave's, makes the text banks, and the memory.
2. **The window** is mapped at the setting's size or the mods' (`port/main.c:683`).
3. **After the game's data** (`mods_install`, `port/main.c:742`; `pc_mods_install`, `mods.c:1164`): the files are
   copied one after another (16-aligned) into a buffer that `pc_rom_set_mods` makes the cartridge's region at
   `PC_ROM_MODS_BASE` = 0x04000000, past the largest ROM the port takes (64 MiB), and below 0x0FC00000, so `0xB0000000
   | offset` reaches it too. The file table's entries of each replaced wave become that offset, the plain file's size
   and kind 0 (raw). `pc_rom_copy` (the PI DMA), `lzss_decode` and `inflate_entry` read the region as the ROM
   (`mods_at`, `pi.c:148`).
4. **The game** then does what it does for any file: LoadWave allocates the block by the size, `_ReadWave` DMAs the
   file into it, `pc_wave_loaded` converts it, SetupWaveModel relocates it. Nothing in the game C changed.

Three files need more:

- **The text banks.** The game reads the first bank (`D_8002E860`) into a block and ReplaceWave reads every other
  language into the same block. When a mod's bank (or its overrides) is larger than that first bank, the first bank's
  size in the table becomes the largest (and its place in the region has the room after it), so the block holds them.
- **The samples** (wave 9, `samples`) are no wave the game loads: the sample directory (wave 8) gives each sample's
  offset from the sample base, ROM 0x39B40 (`(s32)D_00039B40`, MUSYX_DATA.md). A mod's sample data goes in the region
  and the sample directory in use (the mod's, or a copy of the cartridge's) gets every record's offset moved by the
  difference (`SampleRecord.offset` at +4, `include/boot_tail_sample_contract.h`), so `0xB0039B40 + offset` is the
  sample's address in the region. The sample DMA reads it there.
- **Stored forms** (.lzs, .deflate) are decompressed at start (bounded: `pc_mods_lzss`, `pc_mods_inflate`), so the
  game reads every mod's file raw.

## 7. What can be replaced

| What | How | Notes |
|---|---|---|
| Cars: models, textures, palettes (88-100) | the car's WAD | the game finds a car's models and textures by name: keep the cartridge's names; a car larger than the cartridge's raises the memory (6 or `game.cars` copies at once) |
| Tracks: geometry, objects' paths (101-119), placement (120-138), collision (139-157), drones' and checkpoints' paths (158-182) | each file | a track is four files that refer to each other only by name and number: the world names the geometry's models, the road is its own |
| The race's and the front end's models and textures, the HUD, weapons, wheels (54-68, 76-87) | the WAD | |
| Fonts (22-53) | the font WAD and the font data | |
| Text (0-5) | the bank, or text overrides | |
| Sounds: the project, the pool, the sample directory (6-8), the songs (10-21), the sample data (9) | each file | the sound library reads them as converted: sfx and keymap ids must be the ones the game asks for |
| Any texture the game draws | a texture pack in the mod | by the texture's identity, not by file: TEXTURES.md |

**What can't**: the game's code and its initialized data (the car physics tables, the track tables, the menus' and the
HUD's layouts, the drone schedules: game and boot `.data`, filled from the ROM's images, not files: ASSETS.md 3.9),
the runtime images, and the waves no game code loads (69-75). A file is replaced whole: there is no patching of part
of one. Mods for another region's cartridge don't apply (the port runs the USA cartridge's file table).

## 8. Tests and proof

**The tests** (each in ctest):

| Test | What |
|---|---|
| `assets_mods` (`test_mods.c`) | the names both ways; LZSS and DEFLATE round trips, bad streams and the cap; a good file of each kind (text, project, car WAD, track WAD, font, world, road, path) passes, against itself too; every truncation of each, a block of exactly its size (ASan reads past it), and 1,000 garbage files of each: refused; 3,000 small mutations of each: those that pass are converted in the RDRAM window, and no converter refuses or cuts one; a WAD in a world's place, a world in a car's: refused. A mods folder: mod.ini's fields, an unknown key, no mod.ini, another game's, a name rush2049pc.ini can't hold, files by name and number, a stored LZSS file, an unknown name, a zip; the order by priority, by `game.mods_order`, `[mods]` off; two mods for one file (the first wins), a refused file giving way to the next mod's; text overrides over an LZSS bank (the first mod's string wins, a bad section ignored, the bank checked); the file table pointed at the region; moved samples (the directory's offsets lead to the samples' new place); the memory: original raised to 16 for a slightly larger text bank, to 32 for a car 3 MB larger (6 cars), to 64 with 12 cars, kept at 64, refused without the Expansion Pak, refused past 256 MiB, and no growth: original kept |
| `mods_tool` | `rushmods.py selftest`: its names are mods.c's; its verdict is check.c's on the same 4,801 files (2,228 taken, 2,573 refused); a mod folder's report (a second file for a wave, a bad override line), a cut file refused |
| `frontend.mods` | the settings: the defaults, `game.mods` on and off, `mods_dir`, `mods_order`, `[mods]` flags any case, set again, a bad value refused, written and read back, the defaults commented out, a bad line skipped |
| `frontend.menu.mods` | the Mods page: after Game; the order from `game.mods_order` then priority; a mod that can't be used; the folder; Left and Right; a mod moved up (and kept first at the top); Save writes the order and the flag; Defaults |
| `frontend.menu.pages` | About's Mods row |
| `mods_synth` | `tools/port/mods_synth.py` makes the synthetic mods from the synthetic cartridge's own files |
| `synth_boot_mods_colour` | track 1's ground magenta: 10,709 of the picture's 70,400 pixels; nothing refused |
| `synth_boot_mods_conflict`, `_order` | two grounds, magenta (priority 1) and green: the magenta one, the log line naming both; with `game.mods_order=green`, the green one |
| `synth_boot_mods_big` | every car file 3 MB larger at `original`: memory raised to 32 MiB (heap_check: 32 MiB, the peak past 17 MiB, nothing short), the picture drive's byte for byte |
| `synth_boot_mods_bad` | a broken file of every kind (garbage or cut in half; a road as a car; an LZSS stream that doesn't decompress): 21 refused, one line each, nothing replaced, the picture drive's byte for byte |
| `synth_boot_mods_off` | the colour mod with `--no-mods`: "mods: off", drive's picture |
| `synth_boot_mods_text` | every English `[single]` string 24 letters long: the HUD's black text grows from drive's 491 pixels to 1,009 |
| `synth_boot_mods_dump`, `_texture`, `_texture_up` | drive's two textures dumped and a mod's pack made from them, each clear: at `original` and at `upscaled` (scale 2) the HUD's text is gone (0 black pixels) |
| `synth_boot_mods_fuzz_*` | a file of every kind (21 files) cut to a quarter, a half, three quarters, one byte short, 64 KiB of zeros or of garbage longer, garbage of its size: the cut and garbage ones refused (21 of 21; one byte short: 19, two used), the longer ones used (memory 16 MiB): the race runs |

## 9. Not done, or not tested

- Mods apply at the next start, not at the next load: the files are read before the memory is mapped, and changing
  a wave's file under a loaded game (with a background load in flight) isn't safe.
- Sounds on the synthetic cartridge: it has no samples, so no test hears a mod's sound there.
- macOS (no build) and Windows: the mods code is plain C with dirent; it built in the cross build but ran only on Linux.
- A zip's texture pack is unpacked into the data folder at each start; a zip64 or encrypted zip is refused.
