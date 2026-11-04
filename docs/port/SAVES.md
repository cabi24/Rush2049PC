# Saves as files: game.saves (port/ultra/pakfs.c, pakstore.c)

Written 2026-11-04. The N64 game saves only to the controller pak: 32 KiB, 16 notes, 123 pages of 256 bytes. A
profile takes 9 pages, a ghost (an 88-byte header and its zipped samples) a few to a few dozen, so a pak holds a
handful of ghosts, and a player can't give one to a friend. Until now each player's pak was a 32 KiB `.mpk` image
(`rush2049pc.mpk`, [ULTRA.md](ULTRA.md) 3 "Controller pak", [RELEASE.md](RELEASE.md) "Saves"). `[game] saves = files`
keeps it as a folder instead: a file per profile and per ghost, any number of ghosts, ghosts shared as `.r49ghost`
files. `pak`, the default, is the `.mpk` exactly as before.

| Piece | Where |
|---|---|
| The formats: a pak's notes, an image of notes, the save files, the game's checks, the files' names | `port/ultra/pakfs.c`, `pakfs.h` |
| Each player's saves folder: reading it, what goes on the pak, what the game writes, imports, exports | `port/ultra/pakstore.c` |
| The pak built from the folder, the writes handed to it, the swap | `port/ultra/si.c` (`store_load`, `hand_over`, `swap_pak`, `pc_si_set_pak_store`) |
| A write of its own per file for the writer thread | `port/ultra/pakwrite.c` (`pc_pakw_submit_fn`) |
| The settings, `--saves` | `port/config/config.c`, `port/main.c` |
| When a swap is safe; the game's name prefix for imports; the F1 page's side | `port/main.c` (`swap_safe`, `saves_game_names`, `g_saves_ui`) |
| The F1 Saves page | `port/input/menu.c` (`fill_saves`, `fill_save_item`, `saves_action`) |
| The tool | `tools/port/ghost.py` |

No game C changed: the game's pak code (paksys, the save code, the Pfs C) runs as it does with `pak`.

## 1. The settings

| Setting | Default | |
|---|---|---|
| `game.saves`, `saves2`, `saves3`, `saves4` | `pak` | how each controller's pak is kept: `pak` (its `.mpk`, `game.pak_file`...) or `files` (a folder) |
| `game.saves_dir`, `saves2_dir`... | `saves/p1`, `saves/p2`... | each controller's folder; a relative path is in the data folder ([RELEASE.md](RELEASE.md) "Files") |

`--saves pak|files` sets all four; `--set game.saves2=files` one. F1 > Saves > Kept as changes it from the next
start. A controller whose accessory isn't a controller pak (`game.pak2 = auto`...) uses neither until one is chosen;
its folder is read then (`pc_store_configure`).

## 2. The design: the game sees a pak, the folder keeps the notes

**Chosen:** the game's pak code is left untouched. When a controller's pak goes in (at start, or a hot swap), the
pak image the game reads is built from the folder's files (`pc_store_build`); when the game changes the image (si.c
marks it dirty, at most once a second, ULTRA.md 3), the writer thread splits it back into files (`pc_store_write`),
diffing its notes against what it was given: a note added becomes a file, one changed in place (a profile's
settings and records) has its file written again, one deleted goes to `trash/`.

**Why.** Everything the game does to a pak (create, overwrite, delete a profile or a ghost, format, its repairs, its
pak-full and damaged-pak prompts) happens in its own code, on a pak, as on the N64; nothing in the game C had to know
about files, so the default stays the N64 game exactly, and so does every path through the game's save code with
files. The other way, replacing the Pfs C or paksys with file operations, would have meant changing the game's C
across paksys, the save code and overlay A's screens, all of which assume 16 notes and 123 pages, and testing every
one of their paths again; and the libultra Pfs C caches the inode table, so even swapping the image under it is only
safe through a pull and a put-back (below).

**The diff** (`pc_store_write`, on the writer thread, under the store's lock): the image's notes are read as
osPfsFileState reads them (`pc_pak_notes`: the inode table, or its backup; a broken chain left out). For each of the
16 directory slots, the note there against the file the game was given there:

| The image | The file it was given there | What happens |
|---|---|---|
| the same bytes | | nothing |
| the same note (codes, extension, name), other bytes, a profile or another game's note | yes | the file written again (a profile's settings, records) |
| the same note, other bytes, a ghost | yes | the old file to `trash/`, the new ghost a file of its own (SaveGhost deletes the old best and makes the new one by the same name) |
| another note, or none | yes | the old file to `trash/` (the game deleted it); the new one, if any, a file |
| a note | none | a new file |
| the game's note that isn't whole yet: a ghost whose samples' checksum is wrong, or neither a profile nor a ghost | | waits: the image the game finishes it in comes next (the flush of a ghost being written caught it halfway) |

A file that can't be written fails the write: the writer keeps the image and tries again a second later, said once in
the log, as with the `.mpk` (ULTRA.md 3); what was done stays done, so a retry does only the rest. The end of a run,
a fault and `--fast` wait for the writer as before.

**Nothing is removed.** What the game deletes, and what the player deletes on the Saves page, is moved to `trash/`
under its name (`NAME (2).r49ghost` when taken). A ghost the game replaces with a new best is in `trash/` too: the
game deleted that note itself. A ghost that isn't on the pak the game sees can't be touched by the game.

## 3. The folder

```
saves/p1/
  ALICE.r49save                                    a profile
  track 1 forward - ALICE - 1m02.35s.r49ghost       a ghost: track (1-6), way, player, time
  note 4E414B45 12.r49note                         another game's note (from an imported .mpk)
  pak-id.bin      the pak's first page: its label and ID block, so the game sees one pak from run to run
  pak.txt         the notes on the pak the game had last: "slot<TAB>file" lines
  pins.txt        the ghosts pinned to the pak, one file a line, in order
  imported.txt    the record of the .mpk imported at the first start with files (it is imported once)
  import/         .r49ghost files dropped here are taken at start and by Saves > Import ghosts
    done/         ... imported (or already in the library)
    refused/      ... refused (the log and the Saves page say why)
  export/         ghosts exported from the Saves page
  trash/          everything deleted, by the game or the player
  backup/         NAME.bak, .bak2, .bak3: each file's last three versions that started a run
```

- **Names.** From the note's own bytes (`pc_save_file_name`): a profile's name, a ghost's track, way, player and
  time, made safe for Windows and Linux (`\ / : * ? " < > |` and control characters become `_`, no trailing dot or
  space, `CON`, `NUL`, `COM1`... get a `_`). A name in use becomes `NAME (2).ext`. The file's name is only a label:
  the game sees the note's own name on the pak.
- **Backups**, as `saves.c` keeps them for the `.mpk`: at start, a good file that differs from its `backup/NAME.bak`
  is copied there, `.bak` to `.bak2`, `.bak2` to `.bak3`; an unchanged one makes none.
- **A damaged file** (its CRC wrong, too short, a header that disagrees with the note, a ghost that fails 4.3's check)
  is renamed `NAME.corrupt-DATE`, unchanged, and the newest good backup is put in its place; the log says so.
- **Every write** is `pc_file_write_atomic`'s: a temp file flushed to the disk, renamed over the file, the folder
  flushed. A crash leaves the old file or the new one.
- **A folder that can't be made** gives that controller no pak this run (the log says why). A folder that can be read
  but not written works in memory, and its writes are tried again each second, as a failing `.mpk` write is.
- **One folder, one controller.** A folder another controller has (`game.saves2_dir` the same as `saves_dir`, in any
  spelling `same_dir` takes as one: `/` or `\`, a trailing one, any case on Windows) gives the higher-numbered
  controller no pak this run, with the log's `the saves folder ... is controller 1's: give each its own`. The writes
  find their store by its folder (`pc_store_write`) and the writer queues by path, so two would diff each other's
  images.

## 4. The formats

### 4.1 The pak image (`pakfs.c`)

The native Pfs C's `.mpk` layout (ULTRA.md 3): page 0 the label and the ID block (with three copies), page 1 the
inode table and page 2 its backup, pages 3-4 the 16 directory entries of 32 bytes, pages 5-127 the notes' pages. A
directory entry: u32 game code, u16 company code, the start page (`{bank, page}`), u8 status, s8 reserved, u16 data
sum, the extension (4) and the name (16) in the pak's character set; its integers in host order, as the game's
natively compiled libultra writes them.

- **Reading** (`pc_pak_notes`) follows the chain from the start page through the inode table as `osPfsFileState`
  does (a native u16 below 5 ends it; 1, PFS_EOF, ends it rightly); a page outside 5-127, a loop, more than 123 pages
  or bank 1 is a damaged note, left out and counted.
- **Building** (`pc_pak_build`): `pc_pak_format`'s blank pak, page 0 the folder's (`pak-id.bin`), each note at its
  own slot when free (else the first free one), its pages one after another from page 5 in slot order, links and
  PFS_EOF in the inode table, its checksum, the backup copy. More than 16 notes or 123 pages is refused.
- **The status byte is kept.** `osPfsAllocateFile` doesn't set it (it is whatever its stack held), and
  `osPfsReadWriteFile` sets bit 1 (DIR_STATUS_OCCUPIED) at a note's first write and reads only a note that has it
  (`src/rom/lib_b9f0.c`). An imported ghost gets the bit on the pak.

### 4.2 The save file: `.r49save`, `.r49ghost`, `.r49note`

One format for all three, so an exported ghost is a library file and an imported one is read the same way. A
128-byte PC header, then the note's pages exactly as the game wrote them (the N64's layout; its words are the game's
native writes, little-endian on every platform the port builds for). The header's integers are little-endian on any
host:

| Offset | Size | |
|---:|---:|---|
| 0x00 | 4 | `R49S` |
| 0x04 | 2 | version, 1 |
| 0x06 | 2 | header size, 128 |
| 0x08 | 1 | kind: 0 another game's note, 1 a profile, 2 a ghost |
| 0x09 | 1 | origin: 0 the game (this player's pak), 1 imported, 2 exported |
| 0x0A | 1 | the directory slot it had (0-15), 0xFF none |
| 0x0B | 1 | `L`: the byte order of the note's words |
| 0x0C | 4 | the pages' size: 256 to 31,488 bytes, whole pages |
| 0x10 | 4 | CRC-32 (zlib's) of the whole file with this field zero |
| 0x14 | 32 | the directory entry (its start page zero) |
| 0x34 | 1 | track (0-5), 0xFF |
| 0x35 | 1 | way: 0 forward, 1 backward, 0xFF |
| 0x36 | 1 | car (0-12), 0xFF |
| 0x37 | 1 | 0 |
| 0x38 | 4 | a ghost's time in ms, 0xFFFFFFFF |
| 0x3C | 16 | the player's name, NUL-padded |
| 0x4C | 32 | the port's version that wrote it (`git describe`), NUL-padded |
| 0x6C | 20 | 0 |
| 0x80 | | the note's pages |

The kind and the fields from 0x34 are taken from the note's bytes for the tools and the file's name; a reader takes
the note's own (`pc_note_info`), and a header whose kind disagrees with its note is a damaged file. The game's
version is the note's game and company codes in the directory entry (`NRUE`, `5D`) and its ghost revision (17).

**What the note's bytes say.** The game's notes have game code 0x4E525545 and company 0x3544 (`group.c`'s
`PS_CreateFile`). A ghost is one whose byte 4 is 17 and whose header checksum (the save code's `format_string_parse`,
`pc_save_hash`, over +4..+0x44) is right; a profile one of at least 2,064 bytes whose byte 12 is 16 and whose first
block's checksum (over +4..+28) is right ([POINTERS.md](POINTERS.md) 9.10). No ROM string is needed: the extensions
and names the game gives its notes are the cartridge's and are never written in the port's code.

### 4.3 The ghost check (`pc_ghost_check`)

A ghost file is someone else's: before it goes on a pak, everything the game will trust is checked.

| Check | Why |
|---|---|
| the PC header: magic, version, header size, the pages' size and the file's agree, CRC-32, byte order, kind, slot | a damaged or foreign file |
| the game's codes, kind ghost | only ghosts are imported (a "ghost" with a profile's extension would be read as a profile) |
| revision 17, the header's checksum | the game's own check (`draw_ui_element`) |
| track 0-5, way 0-1, car 0-12 | GhostStart indexes the car tables (`[5][13]`) and the profile's track records by them |
| frame time in (0, 0.1] s, race time finite in [0, 36000) | the replay's step and the ghost list's order |
| zipped size 1 to the note's room, its checksum | the game's own check over the data |
| unzipped size a whole number of 3-byte frames, 1 to 72,000 frames (20 minutes) | the decode buffer is allocated from it |
| the samples inflated (raw deflate, as `inflate_entry_alt`) to exactly the unzipped size, and that checksum | `inflate_entry_alt` has no output bound on the PC but the RDRAM window's end: a stream longer than the header says would run on through the game's heap |

The same check runs on every ghost file in the folder itself and on the backup put back for one (`pakstore.c`'s
`read_note`; `ghost.py` the same): a ghost dropped beside the others, not into `import/`, is someone else's too. One
that fails is a damaged file (section 3): set aside as `.corrupt-DATE`, its newest good backup put back.

A refused file is moved to `import/refused/`, with one line in the log (`controller 1's saves: x.r49ghost refused:
its samples don't unzip to the 4500 bytes its header says`) and the reason on the Saves page. Not checked: the car
settings' twelve bytes (handling, engine, colours, tire scales) beyond the header's checksum; their ranges are the
car setup screen's tables, which are the cartridge's.

## 5. What is on the pak the game sees

The game's own structures hold 16 notes and 123 pages a pak, 64 file records (`D_80144DC0`) and 64 ghost records
(`_NewGhost`) over the four paks (POINTERS.md 9.10, CASTS.md); a folder holds any number. `choose` in `pakstore.c`
fills the pak, in this order, each note only while it fits and only one of each note name (two of one name can't be
on one pak):

1. the profiles and other games' notes, at the slots they had;
2. the pinned ghosts, in the order they were pinned;
3. the ghosts the game had on it last time (`pak.txt`), at their slots: the pak stays as the game left it;
4. each player's fastest ghost for each track and way (an imported ghost counted apart from the player's own), by
   track, way and time, while one note and 24 pages stay free for the game's next save.

So the profiles, a player's pins, and the ghosts the game itself saved stay on, and new imports go on first. The page
lists every file and whether it is on the pak.

**An imported ghost on the pak.** Its note keeps its bytes, but on the pak it gets (`place`): the game's own name
prefix and ghost extension (from the game's data at run time, `saves_game_names` in main: the cartridge's, never
kept), and its name's last two characters the digits 00-99. The game's own ghost names end in the way's letter, so
a renamed ghost can never take the name of one the game is about to save (SaveGhost would fail with "exists"), and a
friend called ALICE racing beside the player's own ALICE gets on the same pak. When the game changes such a note in place (its
status byte), its file is written again with its own name and extension, not those (`pc_store_write`). The game reads a ghost's player, track
and time from its header, not its name.

**A foreign ghost races as the player's own.** The track select's ghost choice (`func_803A0498`, overlay A) takes
the ghosts of the track and way from every pak, the player's own best first and then the nearest times around it, or
the fastest when the player has none; the player cycles them with the options. Nothing about a ghost's owner is
checked there.

### 5.1 A change of what is on it: a hot swap when safe, else the next start

A pin, an unpin, an import, or a delete of a note that is on the pak changes what the pak should hold. The libultra
Pfs C caches the inode table and the game keeps pointers to the pak's file, profile and ghost records, so the image
is never changed under the game: the pak is taken out and put back, as a player would (`swap_pak`: the last image
handed to the writer and written, the pak out at once, built again, in after the game's next poll of every port
found it empty; ULTRA.md 3 "Hot swaps").

**When.** Taking a pak out frees its records (`audio_volume_pan`, the files' RemoveFile): a profile a player is on is
taken off the player (`input_rec0[i].handle = 0`), and a ghost chosen for the next race leaves a stale handle in
`gGhosts`. So si.c swaps only when main's `swap_safe` says so, at a flush (once a second): the game at its start
prompt, "press start" or main menu (gGameState 2, 4, 0x10), no player on a saved profile (each input record's profile
handle one of the default profiles, `&D_80146150[k]`) and no ghost chosen (`D_80152698` empty). Until then the change
waits, at most until the next start, where the pak is built with it. The Saves page says so ("the pak changes at the
title or next start"). A note deleted from the page while it is on the pak stays on until then (the game still has
it), and goes to `trash/` at the swap.

## 6. Moving between pak and files

**pak to files.** At the first start with files, when the folder has no notes and no `imported.txt`, the player's
`.mpk` (`game.pak_file`...) is read, never written: each note becomes a file (origin 0, its slot kept), page 0 is
`pak-id.bin`, `pak.txt` lists the slots, `imported.txt` records it, and the log names each file. A damaged `.mpk` or
one in N64 byte order is not imported (the log says why). It happens once: an emptied folder isn't filled again
(`ghost.py mpk2files` does it by hand).

**files to pak.** Saves > Export to the .mpk writes the pak the game sees (the same choice as section 5) to the
player's `game.pak_file`, the old one kept as `.bak` (`.bak2`, `.bak3`, as `saves.c` rotates them); then Kept as:
pak and a restart. `ghost.py files2mpk` does the same to any path.

**What a round trip keeps.** mpk -> files -> mpk gives the same notes at the same slots: each note's directory entry
byte for byte (game and company codes, status, reserved, data sum, extension, name) but its start page, and its
pages byte for byte, and page 0 (the label and the ID block) byte for byte. Not kept: where the pages are (they are
laid out from page 5 in slot order), free pages' bytes, free directory entries' bytes, and the inode table's entries
for free pages (all PFS_PAGE_NOT_USED). A pak the game filled without deleting, laid out from page 5 already, comes
back byte for byte (`ultra_pakstore` checks one).

## 7. Sharing ghosts

- **Export:** Saves > a ghost > Export (.r49ghost) writes a copy to the folder's `export/` (origin 2). `ghost.py
  export` does it from a folder or an `.mpk`.
- **Import:** drop the file into the player's `import/` (Saves > Open the saves folder) and start the game or choose
  Saves > Import ghosts; or `ghost.py import FILE --to FOLDER`. A good ghost goes into the library, pinned (so it goes
  on the pak first), and the file moves to `import/done/`; one already in the library (the same note) too; a bad one
  to `import/refused/`, with its reason.

## 8. The F1 Saves page

Main page > Saves (after Mods). Rows:

- **Player** P1-P4 (Left, Right): whose saves.
- **Kept as**: `pak (.mpk)` or `files (a folder)`, the setting, from the next start (`*`).
- With files: **Folder**; **Import** (what the import folder gave at start, when it gave anything); **Import ghosts**;
  **Export to the .mpk**; **Open the saves folder** (on a desktop); then each profile and ghost (`T1 fwd ALICE
  1:02.35`), with `on the pak` or `kept`, `pinned`, `import`, `to the trash`. Choosing one opens its page: kind,
  player, track and way, time, car, pages, on the pak, file; **Pin to the pak** / **Unpin**, **Export (.r49ghost)**,
  **Delete** (choose it twice: to `trash/`).
- With pak: **Kept in** (the `.mpk`) and what files gives, and that switching imports this pak's saves and leaves the
  `.mpk` as it is.

Each action's result is the footer's message and a line in the log.

## 9. The tool: `tools/port/ghost.py`

Stdlib Python 3.9+, the same formats and rules as `pakfs.c` and `pakstore.c`:

| Command | |
|---|---|
| `info FILE...` | what an `.mpk`, a save file or a folder holds |
| `validate FILE...` | each file checked as an import is; exit status the number refused |
| `export SRC NOTE --out DIR` | a ghost out of an `.mpk` or a folder into a `.r49ghost` |
| `import FILE --to FOLDER` | a ghost into a folder, as Import ghosts does |
| `mpk2files MPK --to FOLDER` | the first start's import |
| `files2mpk FOLDER --out MPK` | the export to an `.mpk`: the same choice as section 5 (`--controller N` for a new pak's ID; `--game-names PREFIX:EXT`, the pak's characters as hex, for the names an import gets on the pak, which only the game's data knows) |
| `make --track T --backward 0\|1 --name NAME --time S [--frames N] [--car C] --out FILE` | a synthetic ghost from scratch, for tests (never from the ROM) |

`ghost_tool` (10.1) checks it byte for byte against the C.

## 10. Tests and proofs

### 10.1 Unit and program tests

| Test | What |
|---|---|
| `ultra_pakstore` (`port/ultra/tests/test_pakstore.c`, 122 checks) | images and notes: four notes out and back at their slots, the status byte kept, scattered pages read and laid out again byte for byte, the backup inode table, three kinds of broken chain, a full pak (16 notes, 123 pages) and one note or page more refused; save files: a ghost's round trip, its header and name, nine kinds of damage refused (a data byte, the magic, the version, the size, the byte order, the kind, truncated three ways), oversized; the ghost check: sixteen hostile fields refused (revision, both checksums, track -1 and 6, way 2, car 13, sizes 0 and past the note, not whole frames, a billion frames, an inflate longer and one shorter than the header says, frame time 0, a NaN time); Windows' names; the folder: an `.mpk` imported (a file each, readable names, the marker, the ID, the `.mpk` byte for byte unchanged), the pak built from them byte for byte the `.mpk`, exported back (the same notes, the old one as `.bak`), not imported twice even emptied, a damaged `.mpk` not imported; the game's writes: a profile added, the same image again, changed in place and read back after a restart with the same ID, a ghost not whole waiting, whole a file, a new best replacing the old (in the trash), a status byte changed (in place), a note never written (waits), renamed, deleted, an unwritable folder failing and then written, a folder that can't be made; the choice: 2 profiles and 30 ghosts of three players, the reserve, the fastest on, a pin and its swap, an unpin keeping it, a delete off the pak at once and one on the pak at the swap, exports; imports: a friend's ghost of the player's own name beside the player's own (renamed, the game's prefix and extension), one already there, a profile and garbage refused, the game deleting the import, an import from elsewhere, an empty import folder; a damaged file restored from its backup; a fuzz: 20,000 mutated ghost files (truncated, extended, bytes, hostile counts, the CRC made right half the time) and 2,000 random images: never a crash, every refusal with a reason. Built with `-fsanitize=address,undefined` on the Pi: 122 of 122, no report |
| `--selftest` (`port/ultra/selftest.c` `t_store`) | the game's own Pfs C on a pak built from two files: both notes found, one at slot 6, with their bytes; a note it allocates and writes becomes a file, one it deletes goes to the trash |
| `ghost_tool` (`tools/port/test_ghost.py`, 375 checks) | `ghost.py` against the C: every file `ultra_pakstore` leaves read as the C reads it, its `.mpk` made into the C's own files (but the writer and the CRC) and back into the `.mpk` byte for byte, and with `test_ghost_c.c` (the C's reader, ghost check, store export and import) the same verdicts and reasons on 42 damaged and 360 fuzzed files, the same images of every folder, the same imports |
| `saves_files` (`tools/port/saves_check.py`) | on the synthetic cartridge with `--saves files`: **import** (two `ghost.py make` ghosts imported, pinned, on the pak, the game reading it; ten hostile files refused, each with its reason, the game racing on), **migrate** (an `.mpk` of two ghosts imported, left as it was, its ID kept, made again by `files2mpk` byte for byte, not imported a second time), **game** (the paths `profile` and `ghost` and their next runs: CreatePlayer's profile the file `SYN.r49save`, read back after a restart and changed in place, deleted to `trash/`; SaveGhost's ghost a file, valid by `ghost.py`, raced after a restart with players 2, the one SaveGhost replaced in `trash/`), **race** (a `ghost.py` ghost imported and raced in the ghost race on track 0 backward: players 2) |
| `saves_files_swap` (real time, alone) | a pin from the F1 Saves page at the main menu (`tools/port/saves/swap.press`): the pak taken out and put back once the overlay closes, the game reading it again, the pinned ghost on it; the same pin during a race (`swap_race.press`): no swap there, and the next start's pak has the pin |
| `frontend.saves` | the settings: `pak` by default, written commented out, `files`, refusals, `--set`, read back |
| `frontend.menu.saves` | the page over a mock of main's side: the player, Kept as, the folder, a refusal at start, the actions, the rows' states, a ghost's page, Pin, Export, Delete asked twice, Import, Export to the .mpk, a player with the `.mpk`, the setting from the page |
| `synth_boot_profile`, `synth_boot_ghost` | the two new synthetic paths with the `.mpk` ([SYNTH.md](SYNTH.md) 2), their checks and next runs |

### 10.2 Proofs: `pak` is the N64 game exactly

"Base" is a build of main 9d5bcac (`git archive` into an ignored folder on the Pi, `~/rush2049pc/pc-saves-base` on the
build host), "branch" this branch. Every setting at its default (`saves = pak`).

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2 --shots`, base against the branch, Debug, build host (x86-64, GCC 14), on the base's own cartridge: its 36 paths (at 94c7478; the `.mpk` code has not changed since) | the same trace and screenshots, every run: 36 of 36 |
| the same on the branch's cartridge (the save values of [SYNTH.md](SYNTH.md) 1): the 38 paths, `profile` and `ghost` among them, which make, read, change and delete a profile and save and race a ghost on the `.mpk` | 38 of 38 the same, `profile` and `ghost` among them (Debug, build host; finished after the first draft) |
| the same 38, Pi 5 (aarch64, GCC 12), under the lock | the same trace and screenshots, every run: 38 of 38 |
| the six retail scripts (`race`, `race2p`, `battle2p`, `stunt`, `battlequit`, `pause`: `synth_compare.py --script`, [MORECARS.md](MORECARS.md) 10.4), base against the branch, Pi 5, retail cartridge, `--runs 2 --same`, under the lock | the same trace, every run, each reaching its state: 6 of 6 (at b7b8f8a, and again at 38f7c33) |
| the `.mpk` files the pak paths write, base against the branch (build host): `pak`, `pak4p` (three paks) | byte for byte the same |
| the same for `profile`, `profile2`, `ghost`, `ghost2` (a profile, then a profile and a ghost) | the same notes but for the bytes that differ between two runs of either build: each directory entry's status and reserved bytes (osPfsAllocateFile leaves them as its stack had them), a profile's second id word (CreatePlayer stores a host handle there) and the checksums over it |
| ctest, build host: Debug `build/pc`, RelWithDebInfo `build/opt` | 300 of 300 each (at 2c55b57 + the tool and the scenarios, 38f7c33; `assets_rom` skipped: no ROM there); before them 295 of 295 each at 81f1881 |
| Windows cross build (Pi, mingw-w64 GCC 12): a fresh configure at 8682644, then again at f066368 | builds, every test program with it (`test_pakstore.exe`, `test_ghost_c.exe`); gen_data's check of the executable: OK, 26 decoupled, 1 reserved in the window; one warning, `test_pakstore.c`'s profile used only by the POSIX-only case, fixed in f060cd9 |
| `n64_neutral.py --base 09da2f0` | passes: 1,018 files, 92 code-exact, 926 shifted (on the Pi; no game C changed) |
| the N64 IDO check | not needed: no game C changed (`git diff 9d5bcac -- src include asm` is empty) |
| `offset_check --strict`, `alias_check` (RelWithDebInfo), `layout_check`, `endian_check` (and `--check-loader`), `bounds_check` (RelWithDebInfo), build host | `offset_check`: 0 open, 59,477 accesses placed; `alias_check`: none open; `layout_check`: 0 open, 48,222 accesses; `endian_check`: 2 open value conflicts in 1 storage and 3,525 puns, as main's; `--check-loader`: 96 places, 0 wrong; `bounds_check`: none new. No game C changed |

### 10.3 Retail, game.saves = files

The retail cartridge on the Pi, under the lock, at 38f7c33 (Debug, `--fast` but for the F1 run), every file of these
runs (logs, screenshots, the saves) in the ignored `build/retail/files/`, XDG_DATA_HOME there:

| Run | What it showed |
|---|---|
| `profile.press`, `--saves files` | the player screen's name entry made a profile of three letters; CreatePlayer's note became `p1/<name>.r49save` (2,432 bytes: the header and 9 pages), the log saying `the game saved` it; no `.mpk` made |
| `again/profile2.press` | at the next start the pak held it (`1 of 1 profiles`), the player screen listed and took it, and a single race on track 0 ran (0x400000, 6 cars); the file was written again in place, its bytes changed, nothing in `trash/` |
| `again/delete2.press` | the player screen's delete: `was deleted by the game: moved to .../trash/`, the folder without a profile |
| a `ghost.py make` ghost (track 0 forward, synthetic samples) in `p1/import/`, then a ghost race on track 0 (`build/retail/race_retail.press`: `race_import.press` without its RIGHT, which on the retail track select moves to track 1) | imported, pinned, on the pak with the retail cartridge's own prefix and ghost extension (main reads them from the game's data); the game read it as a ghost (not deleted) and the race had it: `players 2, cars 2, mode 2` |
| `tools/port/saves/export.press` (real time): F1 > Saves > the ghost > Export | `exported to .../p1/export/track 1 forward - FRIEND - 0m32.00s.r49ghost` |
| that file copied into `p2/import/`; `--set game.pak=none --set game.pak2=memory`, a C-left on controller 2 so it is plugged in; the same ghost race by player 1 | player 2's folder took it in, the game read controller 2's pak, and player 1's race had the ghost: `players 2, cars 2`; the HUD's place reads 1 of 2 |

A ghost made by the retail game itself and a race of the retail game's own ghost were not run: see 11.

## 11. What is left

- **A ghost the retail game saves.** On the retail cartridge no scripted ghost race was run to its finish (the
  scripted car doesn't drive three laps there), so SaveGhost's write into a folder ran on the synthetic cartridge only
  (`saves_files`' game scenario). There the game's save prompt after a ghost race (`func_800F1930`) opens only for a
  profile whose name is the game's `D_801211CC` (LoadPlayerData sets `D_801148E4`); for any other profile
  `billboard_render` keeps the new ghost in memory (SaveGhost's "limbo", `func_800CB748`), and where the retail game
  then puts it on the pak was not looked into.
- **The car settings** of an imported ghost (handling, engine, colours, tire scales) are checked by its header's
  checksum only; their ranges are the car setup screen's tables, which are the cartridge's (4.3).
- **An import after a round trip.** An imported ghost written into an `.mpk` (Export to the .mpk, `files2mpk`) and
  imported again from it is an ordinary note under the name it had on the pak (its last two characters digits): a
  later import given the same digits takes that name on the pak, and as a pin comes first the older one leaves the
  pak (it stays in the folder).
- **Swaps** only at the start prompt, "press start" and the main menu with no saved profile in use: in a session where
  the players are on their profiles, a pin or an import reaches the game at the next start.
- **One program at a time** per folder: two running programs on one saves folder would each write what their own
  game has (as two on one `.mpk` would).
- **Not run:** the Windows program (the cross build only), a real display (the page was drawn by the headless
  renderer: `frontend_menu_test pictures`), Open the saves folder on a desktop, a folder on a network drive.
- The bytes that differ between runs of either build in a pak (10.2) are the game's own: osPfsAllocateFile's
  status byte and CreatePlayer's handle in the profile's id (POINTERS.md 9.10).
