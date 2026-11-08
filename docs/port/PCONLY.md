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

`tools/port/rename.py` applies the maps to the whole tree at merge time (the C, the headers, the data loader's symbol
tables and extents, the sanitizer suppressions, the docs), and `tools/port/names_check.py` checks them (duplicates,
collisions, reserved words, addresses left). The N64 address stays in exactly one place: the data loader's symbol
tables (`asm/us/*/symbols.json`), which say where in the cartridge's image each datum lives; while the data comes
from the cartridge, that is the one thing that must know it.

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
