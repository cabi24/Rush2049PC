# PC-only: the game's C without the N64 build

On 2026-11-08 the user dropped the N64 matching build. Until then the game's C under `src/` still built the original
N64 ROM byte for byte (the IDO check, `tools/port/n64_neutral.py`), so every PC change had to sit in `#ifdef
TARGET_PC` blocks or file-scope macros, nothing could move inside a function, and the code kept the N64's names
(`func_800D0A34`, `D_801111E0`), its raw field offsets (`M2C_FIELD(arg0, s8 *, 0xD)`) and its memory map (the game's
data at its N64 addresses in the RDRAM window at 0x80000000). From now on the game's C is PC code.

The goal, in the user's words: "No more memory address referrals from the n64 architecture. PC native the things.
And comment the code and clean where possible. I want it to be clear and readable."

## 1. What changed, what didn't

Gone:

- the IDO check, `n64_neutral.py` and its CI job, the "no lines added or removed inside a function" rule;
- `TARGET_PC` as a requirement: the PC branch of every conditional is simply the code;
- the N64-only code paths (`N64_ONLY`, `#else` branches of `TARGET_PC`), and the build files only IDO used.

Still in force:

- **The game plays exactly as before.** A refactor changes no behaviour at default settings: the gate (section 2) must
  pass for every change. New behaviour still goes behind a setting whose default is the original game.
- ROM rules: never search for ROMs; the retail ROM is used only on the Pi, by `--rom`, never copied (not into the
  repo, not to Rocky); nothing ROM-derived is committed (values, tables, strings, screenshots, logs, measurements).
- Provenance: no code (and no names) from bryantmh/rush2recompiled, sm64-port/sm64ex, GPL projects, GGPO,
  miniupnpc or libnatpmp.
- Work happens on Rocky (`RUSH_REMOTE=rocky tools/port/remote.sh -n NAME 'cmd'`): builds `-j8`, ctest `-j4`,
  `flock ~/rush2049pc/.realtime.lock` around realtime-labelled tests. Don't touch `~/agents/*`, `~/rush2049/*`,
  `~/ci` there, or anyone's processes.
- Commits end with the session's attribution lines (see any recent commit). Agents don't push; the lead merges.

## 2. The gate: the same game, frame for frame

There is no compiler match any more, so a change is proven by running the game. The base is a build of the commit
the wave started from, kept on Rocky:

    ~/rush2049pc/pconly-base/build/pc/rush2049pc                 the base executable (Debug)
    ~/rush2049pc/pconly-base/build/pc/synth/rush2049_synth.z64   its synthetic cartridge: use it for both programs

After a change, in your tree on Rocky:

1. `cmake --build build/pc -j8`, then the whole suite: `ctest --test-dir build/pc -j4 -LE realtime` and the realtime
   ones under the lock.
2. `python3 tools/port/synth_compare.py --exe ~/rush2049pc/pconly-base/build/pc/rush2049pc --exe build/pc/rush2049pc
   --rom ~/rush2049pc/pconly-base/build/pc/synth/rush2049_synth.z64 --same --shots --audio -j 8 --logs
   build/pconly-compare` must say every path is `same`: the state changes, the first car's exact position and speed
   every second, every screenshot byte for byte and every sound byte for byte, along all 63 synthetic paths.
   (`--audio` can report `varies` on a loaded machine for a path's last subframe; rerun that path alone before
   believing it.)
3. The lead also runs the retail cartridge on the Pi for merged game-C changes (race, battle, stunt, the front end);
   its logs never leave build/.

A trace that differs is a bug in the change, never a reason to update the base.

## 3. Names

- **The game's own names first.** The MAT3 decomp (the user's GameCube build of the same game, from its debug ELF)
  has the original names and types: `decomp/include/rush/rush_types.h` (structs, field names, enums),
  `decomp/config/2049LE/symbols.txt` (functions and data) and `docs/n64-helper.md`. A local copy is at
  `/home/cburnes/projects/rush-mods/SFRush2049-mat3-decomp` (read only; clone it again if missing:
  https://github.com/cabi24/SFRush2049-mat3-decomp). Use an original name verbatim when the N64 function or datum is
  the same thing, shown by evidence: the same strings, constants, callees, field offsets, loop shapes.
  (`tModelData` is the N64 model record, 0x808 bytes, field for field: docs/port/CARS.md.)
- **Else a name from what the code does**: functions `lower_snake_case`, verb first (`car_apply_drag`); data
  `lower_snake_case` nouns (`car_drag_by_traction`), tables plural or `_table`; types `CamelCase`. Never `unk`,
  `temp`, `data2`, `helper`.
- **No address in any name**: no `func_8…`, `D_8…`, no `_800A1234` suffixes.
- **Existing names can be wrong.** Many of the decomp's names are historical guesses its own file headers contradict
  (`hud_render` updates exhaust smoke, `assign_drones` updates records, `car_render_full` lays out the split-screen
  views). Check every name in your area, not just the `func_` ones.
- **No collisions**: a new name must not exist anywhere in `src/`, `include/`, `port/` (or the C library).

### The name maps

Each area's names are one file, `tools/port/names/<area>.tsv`, tab separated, `#` comments allowed, one row a symbol:

    old             new                     kind    source  confidence  note
    func_800D0A34   car_resistance          func    mat3    sure        MAT3 car_resistance: same drag/rolling terms, same callees
    D_801111E0      car_drag_by_traction    data    code    sure        f32[5][3], traction type x tune (model bytes 0xD, 0xB)

`kind` is func, data, type, field (old = `Struct.offset`, e.g. `tModelData.0x5AC`) or file (old = a source path);
`source` is mat3 or code; `confidence` is sure or likely. The note says what it is and the evidence, in one line.

A field row's new name may be a path into an array member (`tCarData.0x274` → `visuals[14].func`); a file row may
name a folder. Two rows may give one new name to two names of one address (aliases in the symbol tables, such as
`D_801174B4` and `state_word_a`): they become one symbol. A row's new name may be another row's old name (a chain:
`func_800DE858` becomes `no_catchup` while `no_catchup` becomes `pak_insert_file`): all the rows are one
simultaneous substitution. `tools/port/names/drop.tsv` (`name  table  note`, table blob, boot_tail, ovl_a, ovl_b or
`*` for the loader's tables, `symbol_addrs` for a line of `symbol_addrs.us.txt`) lists the labels that go instead,
before the maps apply: stale guesses, labels inside other functions, second names nothing uses, and the stale entries
whose names the maps give to other addresses (`gNumPlayers`). `rename.py --apply` keeps the rows it applied in
`tools/port/names/applied.tsv`, which names_check and a second `--apply` read.

`tools/port/names_check.py` checks the maps and the drop list (format, duplicates and disagreements across maps,
collisions with what the tree has, said with where and how: a static function in a file, a use, a table entry;
keywords, reserved and library names, addresses left, old names missing, kinds the C contradicts, labels still
used); ctest's `names_check` runs it on everything in `tools/port/names`. `tools/port/rename.py --dry-run` says what
the maps would change, per row and per file, `--apply` changes the tree at merge time and `--check` lists every
place an old name still names its symbol; its docstring has the details. In one pass over every file it renames
whole identifier tokens: the C's code and comments, the strings that name a symbol (asm and alias strings, a literal
that is exactly one name), the loader's symbol tables (`asm/us/*/symbols.json`, where it also drops drop.tsv's
labels) and `symbol_addrs.us.txt`, the tools and press scripts, `port/data/*.txt`, the suppressions, CMake, the
manifest and layouts, the docs. A file row moves its file or folder (`git mv`) and every reference to its path; a
function's file named after it (`src/ovl_a/func_8038F454.c`) moves with its name. Aliases made one keep one table
key, one symbol file line and one declaration per C file. Left and reported, for a hand edit: field rows (the
struct's layout is needed), strings that only mention a name (output), another spelling of an address name
(`func_800a3424` beside `func_800A3424`: another C identifier, often with another prototype), two declarations of
merged aliases that differ in type, folders named after a function, derived identifiers (`func_800D6E00_1`), names
built by `##`. Left as what they are, not the symbol: struct members of the same spelling (after `.` or `->`, in a
struct body: port/gfx's Windows `targetInfo`), macro parameters, function parameters and locals that shadow the
global. `tools/port/pconly_gate.sh` runs section 2's gate as one command on Rocky.

The N64 address stays in the data loader's symbol tables (`asm/us/*/symbols.json`), which say where in the
cartridge's image each datum lives; while the data comes from the cartridge, that is the one thing that must know
it. Until the tools stop reading it from a name's spelling (n64syms.address_of and kind_of: gen_data.py,
gen_stubs.py, synth_rom.py take a D_/func_ name's address and kind from its digits), rename.py also writes each
renamed address name's address and kind into `symbol_addrs.us.txt` (`car_resistance = 0x800D0A34; // type:func
renamed`); `renamed` tells gen_data.py to treat the name as it treated the address name (no extent by gap ends at it),
and gen_data.py ranks declarations that tie by the type names the maps replaced, so the maps change no layout.

## 4. The waves

1. **Wave 1 (2026-11-08)**: `flatten` makes the tree PC-only (the N64 build's files gone, `TARGET_PC` conditionals
   resolved, the duplicated weak copies of functions down to one definition each); `pctools` writes rename.py,
   names_check.py and the gate script; six naming agents write the name maps and a readable map of their area
   (`docs/code/<area>.md`), without editing code.
2. **Wave 2**: the maps applied; per-area cleanup (raw offsets to struct fields, typed tables instead of casts, local
   names, a comment on every function), the port's own N64 addresses (`port/`) replaced by names.
3. **Wave 3**: the memory model: the game's data out of the 0x80000000 window into ordinary PC storage (the recipe of
   [DECOUPLE.md](DECOUPLE.md), at scale), native pointers in data, the heap and the overlays as PC memory; the last
   data in the N64's byte order converted.

### Wave 1: flatten (2026-11-08)

Three commits, each through the gate (section 2: every one of the 63 paths `same`, the whole ctest suite):

1. **The N64 build's files out**: 1,012 files. The Makefile, splat.us.yaml, the N64 linker scripts and lists, the 180
   `group.json` IDO recipes, IDO's tools (asm-processor, zlib 1.0.4, romkit, build_image.py ...), `ido_check.sh`,
   `n64_neutral.py` and its CI job; 716 files of the cartridge's assembly; the 54 `src/rom` units port/ultra replaces;
   the 11 drafts in `nonmatching/` the PC never compiled. Kept because the PC build reads them: `asm/us/*/symbols.json`
   and `boot_data/kernels.s`, `symbol_addrs.us.txt`, `undefined_syms_auto.us.txt`, `rom_symbol_aliases.ld`,
   `layout/`.
2. **`TARGET_PC` resolved** (`tools/port/flatten_pc.py`): 3,105 conditional groups in 963 files as if `TARGET_PC`
   were defined, and 79 uses of PC_SPIN, PC_IDLE, PC_ONLY and N64_ONLY unwrapped to their PC expansion. Proved by the
   preprocessor: all 2,721 units of compile_commands.json give the base's token stream under `gcc -E -P`. `TARGET_PC`
   is no longer defined; nothing tests it. The scripts that wrote `#ifdef TARGET_PC` blocks (ptr64_*.py,
   view_pads.py) went.
3. **One definition per function** (`tools/port/drop_copies.py`): the 803 weak copies of 332 functions out, the
   one the program linked kept (a declaration left where the file still calls it); the 33 stand-in callers nothing
   called; 62 file-static copies whose -O0 code is a global's replaced by calls to it; the 227 files that then
   defined nothing; gen_weak.py, the probe, weak_copies_check.py and `manifest/` (its assembly-routine table is
   `port/data/asm_functions.txt`). Two data declarations only weak copies made had decided the data loader's layout
   (the audio queue D_801427A8's host storage, the drag table D_8011121C's extent); the live files declare them now,
   and data.c's tables are the base's but for names only the removed code used.

`src/` went from 1,349 files and 1,688,920 lines to 1,068 files and 1,451,803 lines. Left for wave 2: the 246
file-scope macros that had an N64 side (N64P, N64W, SLOT_DATA, PC_CMD_SEND ...), now single PC definitions; global
functions that are copies of each other under other names (rand: func_8008B2B4 and wod_rand; ZOID_SetColor:
func_8008E06C, di_setButtonColor, op_ZOID_SetColorP ...); the file-static copies that differ from every global; the
headers nothing includes (include/game/*.h but nine, game_types.h, game_globals.h, macros.h).

### Wave 2: the names applied (2026-11-08)

Four commits, the gate after each that changes the game (section 2):

1. **The maps fitted to the flattened tree** (names_check: 136 errors, then none): the 82 file rows of files the
   flatten deleted or this step merged out; the 16 stale lines of `symbol_addrs.us.txt` as drop.tsv rows; four
   labels drop.tsv had "inside" a function that start the next one, and that image A calls, as aliases (SetOptions,
   HAL_SetFontFixedWidth, SetControllerActive, SaveFlush). The copies whose names a map gives to another symbol, each
   the same code as the game's function, call it: 19 file statics (ZOID_*, MBOX_NewObject, AddToEnvList,
   RemoveEnvirons, setGameState, PlayGameSound, PauseTargets) and 12 second definitions (nonmatching's audio
   commands, PauseGameMusic and InitBattlePoints; InitEgg, ZOID_SetObjectDef, Hidden, GetWaveInstanceAddr,
   DeleteGhost, ReadGhostData); audiomgr_thread.c's ClearBST, WalkBST and _SendCmd, which differ, have their own
   names.
2. **The maps applied** (`rename.py --apply`): 5,609 map rows and 1,298 drop rows in the ledger, 1,328 files
   changed, 997 moved, 1,024 labels out of the loader's tables, 3,064 names added to the symbol file; a second
   `--apply` changes nothing. Put right by hand: `countdown` (UpdateGame's old name) where it was the English word
   (144 places), `renderer` in CMake, 76 full paths the move had cut short. For the build: the self-aliases of the
   scheduler's PC_ALIAS lines and two tests; `tools/port/decl_cleanup.py` (168 identity macros, 1,406 data
   declarations of functions, 714 m2c guesses beside a typed declaration); the moved boot files' `../rom_tu.h`; the
   tests' stand-ins for two names now one. gen_data.py keeps the base's layout (data.c the base's but for names):
   the symbol file's `renamed` names end no extent, and ties between declarations fall to the old type names.
3. **What the rename left to read**: derived identifiers (pc_func_800E6AE8 to pc_ReadShifter ...), other spellings
   where a file has no other name for the symbol, updateWarningScreen by its name, comments that said a name twice.
4. **The tools**: index_check.py finds indexed data by any name, gen_data.py's --check (RUSH_NOFIXED) accepts named
   data at their own offset in the window, alias_check.py's and offset_check.py's review lists by the new names.

Address names (`\b(func|D)_8xxxxxxx\b`) in src/, include/ and port/: 953,841 lines before, 339,301 after (src/blob
338,094, include 987, port 179, the overlays 40, src/rom 1). Left for the per-area cleanup: the m2c context
preambles' declarations of 959 address names nothing uses (about 310,000 lines; the maps named only what is used),
87 address names the code uses that no map names (func_80090F44, func_8009EA68, D_8014A250 ...), the 8,229 other
spellings (func_800a3424) in preambles that declare both, the preambles' four view types (D_8012E6E0_Node ...),
the 388 field rows, and addresses in comments.
