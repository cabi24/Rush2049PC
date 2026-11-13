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
   every second, every screenshot byte for byte and every sound byte for byte, along all 64 synthetic paths.
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
renamed`); `renamed` tells gen_data.py to treat the name as it treated the address name (no extent by gap ends at it).
The data's types no longer come from the files' declarations at all but from include/game's, one per datum
(`gen_data.py --decls`, section 4 "Wave 2: headers"), so a name or a type a map renames changes no layout.

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
   The plan, its census (`tools/port/window_census.py`) and its 14 steps: [NATIVE.md](NATIVE.md).

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

### Wave 2: what the names left (2026-11-08)

One agent took what the apply left in the game's code (each commit through the gate, section 2):

- **rename.py** after the apply: a path to a file moved one folder deeper keeps its first folder (src/rom/x.c to
  src/rom/sound/y.c), a relative `#include` follows its file or the file it includes, a declaration whose
  initialiser has commas in parentheses is read as one, an English word that is an old name (`countdown`) is
  renamed only where it reads as code, a file is moved with a name only when it defines it, and prose that lists an
  old name beside its new one is left as it is; each with a test (test_rename.py's AfterTheApply).
- **The last address names the code used**: `tools/port/names/wave2.tsv`, 58 rows (47 from MAT3: the overlays'
  spellings of PitchUV, RollUV, ZOID_ResetWindows and CreatePlayer, `model`, RaceMultiBlit and the pause screen's
  MultiBlits, controlsys.c's statics, percentTable, drone_sched, gMatrices, zlib's tables ...), the 30 `#define NAME
  D_x` they made identities, 16 function-local statics and three boot calls that kept an address spelling. No
  address name is used in a function body, an initialiser or a macro now; left: the m2c preambles' declarations,
  include/game_globals.h (no file includes it), gen_data's test fixtures (test_pc_data_decls.c: named for their
  addresses on purpose) and AudioInitSys's ROM offset `D_00039B40`.
- **Retail stubs**: the 107 `_stub`/`stub_*` functions (105 empty, two copies of ConvertHeap and FindWaveInstance
  whose callers call those), and 16 more empty ones with MAT3 names nothing called (CleanupOverlay, SetDefaultOptions,
  si_lock_init ... and ExitSystem, whose one call did nothing). GhostEnd's call to the empty DeallocateMemory_stub
  went too: MAT3 frees the unfinished ghost recording there and the PC never has (a comment says so; restoring it is
  a behaviour change to decide). Left: seven empty stubs whose code lives in a group as `pc_<name>`
  (Check2WheelSide, GetSkid, ReadShifter ...: the function table reaches the empty one) and the called empty
  heap_lock/heap_unlock.
- **Copies under other names**: 182 functions that were the same code as a global one are gone, their callers call it:
  text_lock and text_unlock (77 copies: gfx_lock_B7FC ...), HAL_SetFont (41) and HAL_SetFontInternal (2), TB_SetJust
  (15), TB_CenterText (10), TB_RightText (5), ZOID_SetColor (7), AudKillSong (5), Hidden (6), si_unlock (4),
  sound_list_init (3), view_bind, AudSetMusicVol, AudChangeEffectPitch, HAL_SetTextDepth, and init_camera_on_track's
  wod_rand, wod_random, wod_dist2d (game_rand, Random, Distance2D). UnloadWaveInstance's static
  acquire_decompress_lock is wait_for_render (the global of that name is the decompressor's lock). Left: copies of
  each other with no global (the pak lock's queue_init family, item_refresh_*, getB6_*), and MAT3 pairs that are the
  same code (AudPauseAllEffects/AudStopAllEffects, controlPakDamaged/controlPakFull, ControlRumblePak/ControlDamaged).
- **Alias macros**: powerups.c's volatile view of gFrameSeconds (a volatile declaration, as MAT3's), circuitscr.c's
  u32 HAL_GetTextWidth (a typed helper, text_width), carscr.c's gCarComponents view (a typed local: the declaration
  the loader converts the table by stays). Left: typed view macros over preamble declarations of the wrong width
  (controlsys's gResetTick ... gRepeatTime, gBlitInstanceList (x[0]) ...), for the shared headers.
- **Interior labels** (NAMING.md 7 items 8 and 11): 104 names of an address inside another object are gone from
  the code (224 uses in 90 game files, 75 in 18 tests): each is the element or field of the object, reading the
  same bytes (`gObjList[156]`, `gText.single[n]`, `gOptions[9]`, `gCheckpoints.points[cp]`, `carTransGearRatio[class]
  [gear + 1]`, `gObjList[def]` for `gObjList_biased[def + 314]`), with a typed local view where a preamble's
  declaration can't express it; the macros that stood for labels are gone; the loops end at `gOPPFuncs + 10` and
  `gStuntInfo + 4`; the overlay loader's BSS bounds are `extra_overlay_bss_start` and `start_overlay_bss_start`. The
  labels nothing reads left the loader's tables (drop.tsv); eighteen keep their symbol_addrs.us.txt line, each ending
  the extent by gap of the object before it. Left: resultPane4 and object_list_scale_view, two latent port bugs that
  folding would change (animResults writes resultPane[4] past the PC's own storage; the trophy's scale goes to the
  arena's ObjInstanceList, not the PC's), and seven whose bytes lie past their object's extent, where folding grows the
  object and so the layout (unitmatrix_origin, gWingAngVDrag_1, controls_idMap_conflict, start_screen_egg,
  FSCLS_from_one, the tire scales' rows).
- **Locals** sharing a global's name: DoDrones' lists, maxpath's `interp`.
- **Original-game bugs**, commented where they are and kept: InitPane, RemoveBlit, FindWinner, CalcUnlockedStuff,
  ZOID_ChangeTexture, UpdateModel, SetPakError, CalcChecksumInternal, updateBG (`/* Original game: ... */`).

### Wave 2: headers (2026-11-08)

Every game file began with the context m2c wrote: 3,500 to 4,200 lines of typedefs, structs and externs, then its
own code (AnimateKnockdown.c: 4,223 lines, its function at line 4,195). The game's shared declarations are
`include/game` now, and each file holds its code and only what is its own ([docs/code/README.md](../code/README.md)
"The headers"). Made on 2e88259e and brought onto the merged tree 23cfc766 (wave 2's leftovers and port addresses:
the headers written again from its DWARF, the conversion run again on it). The headers' commits change no game code
(objsame.py: every unit `same`, data.c the merged tree's); the conversion went through the gate (ctest 567 of 567,
assets_rom skipped as on the base, and the 60 realtime tests; every one of the 63 synthetic paths `same` against the
base, shots and audio; the RUSH_NOFIXED build's `--check` OK and its 63 paths `same` as the fixed build's):

1. **The data's layout from one declaration per datum.** include/game declares each datum once, with the type
   gen_data.py chose from the files' declarations until then; `port/data/game_decls.c` compiles those declarations
   alone and `gen_data.py --decls` lays every datum out by them ([DATA.md](DATA.md) 1), so which file declares what,
   and how, decides nothing. `EXTENT(n)` gives an array its extent there only; the game's files see it open-ended,
   as before. gen_data's tie-break by the type names the maps replaced is gone (one declaration has no ties). Proof:
   data.c the base's but for the type names in its comments, data_symbols.txt the base's in every column but the
   type's spelling, in Debug, RelWithDebInfo and RUSH_NOFIXED, gen_data `--check` OK in each, no datum placed by the
   files' declarations. `tools/port/objsame.py` compares two builds' game objects unit by unit, debug info left out
   (ctest `objsame_tool`).
2. **The headers**: `basic.h`, `types.h` (libultra's types, then 184 structures printed from the merged tree's DWARF,
   so each lays out as it did; 259 structures named only through pointers), 16 subsystem headers (2,054 data in N64
   address order, 1,937 prototypes from the definitions), `m2c.h`, `view.h`. Gone, as nothing included them: the 57
   arcade headers `include/game/*.h` (125 of their names clashed with real signatures) and `include/game_types.h`,
   `game_globals.h`, `macros.h`. Left to their files: 33 functions that return a pointer to a structure of their own
   file, 4 that take or return a structure by value, the 8 C library names the build renames `n64_*`, and 35
   whose callers disagree with the definition (another number of arguments;
   a 32-bit integer passed where the definition takes a pointer, which through the prototype would be sign-extended:
   `RemoveBlit`, `ZOID_SetTexture`, `AudEmitEffect` ... the per-area cleanup makes them agree).
3. **The conversion**, `tools/port/to_headers.py --build build/pc --apply` (re-runnable: a second run changes
   nothing; its docstring has the rules): the 918 files of src/blob, src/ovl_a, src/ovl_b and nonmatching/ include
   game.h and keep their code, their own types (150 renamed in their file by a macro where the headers give the name
   another type: `#define Gfx GfxView`), 1,764 views of shared data (`VIEW(Cam152[], gCamera);` and `#define gCamera
   gCamera_view`: the datum's own symbol through an assembler label, so the code is the old declaration's), and their
   declarations of what the headers lack. The four types still named after an address (`D_8012E6E0_Node` ...) have
   names (`tools/port/names/headers.tsv`). src/ went from 1,061,648 lines to 87,883; lines with an address name in
   src/, include/, port/ and nonmatching/ from 227,997 to 110 (src 26, include 4, port 79, nonmatching 1).
   objsame against the base: 886 of the 971 units `same` in Debug, 901 in RelWithDebInfo; each of the others
   (90, five of them only -O2's) makes a call through the definition's prototype where the file had its own
   declaration (`docs/port/headers-units.txt`, one line per unit with what changed, in PROTOTYPES.md's classes, none
   harmful; a call that was unprototyped now also lacks x86-64's `mov $0, %eax` for variadic callees), and the tree
   converted with the headers but without their prototypes is `same` in every unit but ReadPathData.c (GCC orders the
   two GOT loads of `gResPath.alt = gAltPaths = 0` by declaration). The checks: offset_check `--strict` no open
   finding, alias_check none open, bounds_check none new (a view's findings are its datum's), names_check no error;
   nofixed_audit's implicit pointer and integer conversions grew (int_to_ptr 23 to 24, ptr_to_int 12 to 66,
   int-conversion 35 to 90; its port classes are the merged port's) because calls the files' own declarations hid now convert where the compiler sees them
   (a pointer passed where the definition takes a 32-bit integer, which the callee read 32 bits of before too): the
   baseline is the new count. The game files' warnings are the base's. ctest `to_headers_tool` converts a made-up
   tree.

After merging other work that edits the game's files: apply the maps (`rename.py --apply`, which renames in
include/game too), build Debug, `to_headers.py --build build/pc --apply`, build, and check with objsame against the
merged tree before the conversion and the gate. to_headers reports a name the headers declare that a file gives
another kind; a prototype a merged definition no longer matches is a compile error to fix in the header.

### Wave 2b: readable, area by area (2026-11-09)

Six agents, one area each, on the headers' tree 4c3c3c2a: a comment at the top of every file and before every
function, names for m2c's parameters and locals (`arg0`, `var_v1`, `sp2C`, `temp_f0`, `phi_*`), struct fields for
raw offsets (`M2C_FIELD`, `*(T *)((u8 *)p + 0x5A)`) with MAT3's field names where the layout is MAT3's, named
constants (MAT3's enums and flags where it has them), m2c's leftovers cleaned, and a page per area:

| Area | Page | Files | Functions commented | Notes |
|---|---|---|---|---|
| system, memory, input | [system.md](../code/system.md) | 143 | ~360 | `GAME_STATE_*`, threads, scheduler messages; game/heap.h (MAT3's heap records) |
| cars, physics, maths, scene | [cars.md](../code/cars.md) | 272 | 327 | ~450 offsets to fields; tModelData/tCarData views; 144 constants |
| track, race, cameras | [race.md](../code/race.md) | 133 | 208 | 15 shared types (paths, targets, stree, cameras); the race kinds and `GS_*` in race.h |
| front end | [frontend.md](../code/frontend.md) | 93 | 262 | ~480 constants for ~1,300 literals |
| saves, text, blits, HUD | [ui.md](../code/ui.md) | 222 | 303 | tSaveData byte-exact, tProfile, ghosts, tPak, tBlit; ~150 constants |
| audio | [audio.md](../code/audio.md) | 94 | ~460 | game/audiomgr.h (MAT3's sound manager records); MusyX's 62 macro opcodes |

The proof is objsame where a unit's code is unchanged (comments, names, types that lay out alike), and the gate
where it is not: at -O0 a field compiles to a folded displacement where the raw offset took an add, so most field
conversions change their unit's code without changing what it does; each area's commit messages list its changed
units and why. Each area passed the gate on its own tree (63 of 63 paths `same`, Debug and RelWithDebInfo; ctest;
the realtime tests). Merging the six met clashes between one area's new shared types and another area's file views
(`tPadState`, `tMiniModelData`, `streetyp`, `CameraView`, `TargInfo`, `ObjectInstance`'s field names); each was fixed
to the shared type where the code stays the same (objsame against both parents), else kept as a file view under its
own name.

Port bugs found on the way and fixed toward the original game: the attract demo's cars started on one drone-path point
(SetResurrectionData read N64 offset 0x338 of the PC car record; the two demo paths change from the demo race on,
every other path is `same`); StartHeadLights' palette without PC_N64P (wrong only under RUSH_NOFIXED); NewListener's
`free` byte at its N64 offset; move_blit_chain's N64 offsets and PS_SetRumblePercent's N64 record size (both
uncalled). Possible original-game bugs are marked `/* Original game: ... */` and kept.

Left for a follow-up: one spelling per constant across areas (`GAME_STATE_*` against MAT3's `GS_*`, the race kinds
and button bits defined in several places); the names the areas found wrong (each report lists its evidence: the pak
error screens `cs_draw*_DC` are MAT3's `_DrawPakErr*`, `rear_traction_by_tune` is `carEngineFwdForce` ...); files
still named after an address or misnamed; the shared records some files still view on their own (gPlayers, gPaks,
the profile, `struct Target`); the 33 + 4 + 35 functions the headers leave to their files.

### Wave 2c: one of each (2026-11-09)

Five agents on main 254c4b01 took what wave 2b left, and a sixth wrote wave 3's plan; the reference builds of
254c4b01 (`~/rush2049pc/w2c-ref` on Rocky) were both objsame's reference and the gate's base (`pconly_gate.sh
--base`). The five merged in the order prototypes, constants, views (game side, then system side), names; their
conflicts were nearly all one side taking out or rewriting lines next to the other's, merged edit by edit, and each
merge commit lists the hunks done by hand.

| Agent | What | Tool to re-run after merging |
|---|---|---|
| prototypes | every game function declared once in `include/game` (63 more; the files keep only the C library's and libultra's); the 35 calls that disagreed with their definitions made to agree, none a port bug on the PC | `tools/port/proto_check.py` |
| constants | one name per meaning, defined once in its header: gGameState's bits are MAT3's `GS_*` (race.h), the race kinds, buttons, menu sounds, audio commands, tracks; the files' conversion abbreviations are the prelude's `PC_N64P`/`PC_N64W`/`PC_N64A`; new `game/gbi.h`, `game/threads.h`, `game/zlib_deflate.h`, `src/rom/sound/musyx.h`. Duplicated macro names 407 to 56, each left one listed with its reason | `tools/port/one_spelling.py --apply` (ctest `one_spelling` fails while a merge brings a copy back) |
| views, game side | 692 of 693 file views of scene, cars, physics, maths, track, race, camera and HUD data gone: unitmatrix `f32[3][3]` (`unitmatrix_origin` after it), the model tables split by what they hold (MAT3's names), `gCamera` a `tCamera`, `CarParts`, `tStatList`, the stunt and battle records with MAT3's fields | |
| views, system side | 645 of 755 gone: the input words and the frame timer by MAT3's types (volatile where another thread writes them), gOptions `s8[41]`, gJustPlayers, the text tables, the list headers, the pak and pak error records | |
| names | the names wave 2b found wrong (the pak error dialogs are MAT3's `DrawPakErr*`, `carEngineFwdForce`, `carTransTraction_by_gear`, `UpdateArrowVis` ...), the last address names (none left in src/, include/, port/ but a gen_data test's on purpose), 56 files renamed for what they hold, the address-suffixed copies calling their globals, pc_car's and pc_model's fields by MAT3's names | `tools/port/rename.py --apply` (map `names/wave2c.tsv`); `objsame.py --renames MAP` reads a build through a map |

Views left: 109 (of 1,409), each where the canonical type would change the data loader's layout (`gNumModels` is
MAT3's `u8` but the loader gives it 4 bytes; `gTireRims`, `gUnlockedCars`, `gBattlePoints` ...), where one record
still has several struct copies (the menu buttons' five), or in the frame pipeline that wave 3 changes anyway.

Port bugs fixed toward the original game: the results screen's middle pane (`resultPane` had four entries, the
N64's and MAT3's five, so `resultPane[4]` was the next variable, `rumble_msg`); the trophy's sheen scale (the N64
writes the scene node's `w`, the PC wrote an unused copy); MusyX's SET_PITCH at an exact power-of-two rate read past
its table (a new assets_musyx test); the Windows build without SDL2 didn't link (mingw's linker and a lone weak
definition). After wave 2b, CI's UBSan job found `tCarSettings.settings` typed 10 bytes where the checksum and the
setup screen use 12. No synthetic path changes for any of them; the gate on the merged tree is 63 of 63 `same`.

### Wave 3: the memory model (2026-11-09 to 2026-11-13)

The plan and each step's results are [NATIVE.md](NATIVE.md) (its census, `tools/port/window_census.py`; until wave 3.5
its crossing counter and arena guard, section 9; since then the N64 guard, `tools/port/n64_guard.py`, section 17). Done: step 1 (the counter and the guard), wave 3.1
(steps 2, 3, 4: the car record's, the level objects', the sky's and palettes' pointers; the images by pointer), wave
3.2 (step 5, the arenas out of the RDRAM window as blocks of host memory; step 6, the heap's own code on host pointers
with the N64's accounting; step 8, the text bank, the world and the paths as native records; a synthetic path
`language`, so the gate is 64 paths), wave 3.3 (step 7, the WAD's records and the font letters native; step 9, MusyX
on pointers, the samples named by cartridge address) and wave 3.4 (step 10, the files' display lists built in the PC's
form with host pointers when they load, `relocate_model_dl` renamed `prepare_model_dl`; step 11, the last holders of
heap addresses (the ghosts' samples, the frame's colour buffer, the once-per-frame functions) and the tests' own
windows) and wave 3.5 (step 12, the window gone: the game heap is one block of PC memory wherever the OS maps it, with
the N64's size, alignment modulo 64 KiB and accounting; libultra's boot parameters are ordinary variables; the RDRAM
window and everything that existed for it are deleted: the views at 0x80000000 and 0xA0000000, `pc_n64_ptr` and
`pc_n64_addr`, the prelude's `PC_N64P`, `PC_N64A`, `PC_N64W` and `PC_SLOT_*`, `absfix.c`, the linker script's absolute
names, the renderer's and the audio interpreter's 4 GiB rule, the moved build `RUSH_NOFIXED` and its CI job, and the
x86-64 `-fPIC`/no-relaxation flags; `nofixed_audit.py` is `n64_audit.py`). Along the 64 paths the game and the PC layer crossed between a host pointer and an N64 address 73.2 million
times at 243 places before step 1, 30.6 million at 47 after wave 3.2, 27.0 million at 20 after wave 3.3 and 2.0
million at 6 after wave 3.4 (the renderer's images and present, which named heap blocks, and the boot's low-memory
parameters), and none after wave 3.5, when the counter went with the window. Its successor, the N64 guard
(`RUSH2049_N64_GUARD=1`), reserves 0x80000000-0xBFFFFFFF with no access and maps the heap above 4 GiB with a trap
below it, so an N64 address or a heap pointer cut to 32 bits faults; the 64 paths and the lead's 42 retail scripts run
under it with no fault, the retail-only inflate of DEFLATEd files among them. With the heap at a different address in
each process, the state hash names a heap pointer by its offset; the gate's first run found a chance match on a 64 MiB
heap (a decoupled record's numbers read as a heap pointer in one run only), and the heap rule now applies only where a
heap pointer can be. AddressSanitizer runs on x86-64 as well since wave 3.5, in CI. CI's sanitizer jobs found one bug
in wave 3.4 after its push (`qsort` given a null list when a WAD loads no vertices: every game boot under UBSan; fixed
in 392a1aa3); the lead now runs the UBSan set before a wave is pushed. The gate compares the data loader's summary with
`--data-moved` since step 2 (its pointer counts by class move as data leaves the arenas). Step 9 found the synthetic
cartridge silent (MusyX's pitch and volume tables were zero), so the gate's `--audio` had compared silence until then;
`synth_rom.py --sounds` makes a cartridge whose sound bank and a song are heard, and a step that touches the sound
compares on it too. Wave 3.4 found the retail runs of `synth_compare --script` taking none of a script's own options,
so their `--shots` had compared no screenshot; they do now, and the lead's retail check of step 10 compared
screenshots of every race track forward, backward and mirrored.

Wave 3.6 (steps 13 and 14, two agents side by side, merged by the lead) finished the plan. Step 13: every datum of
the images' data and BSS is an object of its own (1,599 names in 1,592 objects, and 78 unnamed for bytes no datum
names), filled by the loader from the image's bytes or zeroed; the four arena blocks are gone, own storage keeps no
order (`-fno-toplevel-reorder` gone), the runtime images' C data is put back datum by datum (`overlays.ld` and
`concat.cmake` gone), and data.c and the game C's globals have ASan's redzones (`PC_NO_SANITIZE` on data.c and
`--param asan-globals=0` removed). ASan over every synthetic path found 11 walks from one datum into the next; the
lead's run of the retail scripts under ASan on the Pi found two more no synthetic path reaches (the name censor past
its table, `DetectDroneAbort` reading a drone's input record past the four), each fixed in the C (NATIVE.md 18.2).
Names inside other data fell from 12 to 4; the state hash names each datum's object (its values differ from wave
3.5's builds). Step 14: the colour words are `tRGBA` (`PC_RGBA` gone, the census's `be.rgba.game` 10 → 0), the start
and track screens' buttons have their fields so the loader converts them, and the big-endian ranges are texel memory
(each WAD's texels and TLUTs, `pc_mem_register_texels`) and sample memory (the sound caches,
`pc_mem_register_samples`): `be.ranges` 35 → 32, no registration left that doesn't hold texels or samples
(NATIVE.md 19). The lead's check of the merged tree: the gate 64 of 64 `same` with ctest 572 of 572, the audible
cartridge and the optimized build 64 of 64, the guard 64 of 64, ASan over all 65 paths on both cartridges with no
report, CI's ASan set 399 of 399 and UBSan set 546 of 546 on Rocky; on the Pi, the mingw build, datacheck and
assetcheck (no `texels:` line), the 44 retail scripts with screenshots and sound against wave 3.5's build, each
under the guard and under ASan, a texture pack from the base's dump (1,354 textures replaced in both), and the saves
(the game's pages byte for byte; the PC header differs only in its CRC, the version string and libultra's two unset
directory bytes, as between any two builds). What the plan's census counted is done: what is left of N64 addresses
is the cartridge's format (the loader's decoding of its pointers, the bus addresses of its streams, the display
lists' segments), the heap's N64 accounting, and the guard's range.

