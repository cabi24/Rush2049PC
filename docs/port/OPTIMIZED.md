# Optimized builds (RelWithDebInfo)

Written 2026-10-27. The default build type is Debug (`-O0 -g`), and the port was developed and tested at `-O0`. An
optimized build (`-DCMAKE_BUILD_TYPE=RelWithDebInfo`, `-O2 -g`) failed 14 of 121 tests. It passes every test now, on
x86-64 and aarch64 Linux and in the Windows cross build, and it plays the game the same as Debug, to the bit: the
synthetic cartridge's 14 paths and 14 paths on the retail cartridge give the same states at the same retraces and the
first car at the same place every second (section 5). On the retail cartridge it runs twice as fast: a race frame
takes 6.7 ms of the Pi 5's time instead of 15.3 (section 6).

```
cmake -S . -B build/rel -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/rel
ctest --test-dir build/rel
python3 tools/port/synth_compare.py --exe build/pc/rush2049pc --exe build/rel/rush2049pc --rom build/pc/synth/rush2049_synth.z64
```

Debug is unchanged. Its game objects are the same code but for three files, where one name now spells the address
another did (sections 2.2, 2.5); its data tables are the same but for those three second names (`D_8004BE7B`,
`D_80146125`, `D_803BAB50`), which no game file uses now: BSS bytes inside the symbols that name them, so the loader's
runs and symbols are the same. Along every synthetic path it agrees with the Debug build before this work.

## 1. What failed

On the x86-64 build host (Rocky Linux 10, GCC 14), RelWithDebInfo passed 107 of 121 tests:

- 13 of the 14 synthetic paths (all but `demo`, which never reaches the main menu) segfaulted at address 0 as the main
  menu came up, in `func_8008D0C0` from `ambient_sounds_clear` (section 2.2);
- `endian_check`: the loader's element widths at 4 of 78 places were missing (`D_803B8328+0x0: loader element None`):
  those symbols weren't in the tables (section 2.1).

Two more showed in CI: Ubuntu's runners aborted every frontend test (section 2.6), and the Windows cross build didn't
link (section 2.6). Then the comparison found the cars a bit apart from Debug's from the first physics step
(section 2.3), and two checks written for this found more (sections 2.4, 2.5).

The loader's summary line on the synthetic cartridge, `pointers: 0 own, 0 C data, ...`, is the same in both builds:
that cartridge's data points nowhere into own storage or game-C data. On the retail cartridge those counts aren't 0,
and they are the same in both builds too (the comparison's traces include the line).

## 2. Root causes and fixes

### 2.1 The data tables came from the optimized objects' DWARF

gen_data.py takes the game's types from the DWARF of the game objects ([DATA.md](DATA.md) 1), and the optimizer
changes what that DWARF holds. GCC describes an external variable in an object only if the object still uses it:
where the optimizer removed the uses (dead code, constant folding, inlining), the declaration is gone, and so are the
types nothing left uses. The RelWithDebInfo tables lacked `D_80117498` (a word the loader then left in the ROM's
byte order), `D_80394270` and `D_803B8328` (colour unions: `endian_check`'s missing places), and some names inside
image A's tables (`D_803B8B34`, `D_803B8B70`..`D_803B8B78`, `D_803B8B98`), so their neighbours' extents ran by the
gap over them. Struct completion (the most common complete definition of a tag) depends on which types survive too.

**Fix.** gen_data reads unoptimized objects whatever the build type. A Debug build's game objects are `-O0` already;
any other build type compiles a second copy of the game objects at `-O0 -g` (`game_types` in CMakeLists.txt, with the
game's own flags and weak headers: 61 CPU seconds on the x86-64 build host, against 90 for the game at `-O2`) and
lists them in `types_objects.txt`. The
runtime images' C data, which `gen_data.py --check` finds in the executable, still comes from the objects linked
(`--link-objects-list game_objects.txt`): an optimized object has fewer statics (`cityPos.4`, `mapPos.2`, `del.0`
folded away; `gShieldSizes` and `sWeaponFx`, never written, in read-only data). gen_data refuses an object whose
DWARF producer line records optimization (`-O` other than `-O0`), so it can't happen again unnoticed. The probe that
gen_weak reads, and pc_data_test's declarations, are compiled at `-O0` too, and view_check, view_audit, view_pads and
proto_check read `types_objects.txt`.

Result: `data.c` and `data_symbols.txt` are the same files in Debug and RelWithDebInfo, on both architectures and in
the Windows cross build, and Debug's are the files it made before. CI compares them (section 4).

### 2.2 Own storage out of order, and a walk to the next symbol

`ambient_sounds_clear` clears the four ambient sound handles `D_80114628` by walking to the address after them, where
the N64 has `D_80114638`. Both have storage of their own on the PC (they hold pointers), and the walk ended only
because data.c defines own storage in N64 address order and `-O0` emits it in that order. From `-O1` on, GCC reorders
top-level definitions (`-ftoplevel-reorder`); at `-O2` it put them in reverse, `D_80114638` before `D_80114628`, so the
walk never met its end and called `func_8008D0C0` on whatever followed.

**Fixes.**
- The walk ends at the handles' end under `TARGET_PC` (`#define D_80114638 ((&D_80114628)[4])`, the N64 tokens
  unchanged). The Debug object differs only in that bound, which Debug's layout gave the same address.
- data.c is compiled with `-fno-toplevel-reorder`, so own storage keeps data.c's order, N64 address order, in every
  build type, and any other walk from one symbol to the next behaves as in Debug.
- `gen_data.py --check` verifies that order in the linked executable (`398 own in N64 order`).

A scan of the game C for comparisons against the address of an own-storage symbol finds this walk and
`sound_handles_clear`'s, which has had a PC version since the synthetic cartridge (SYNTH.md 3).

### 2.3 Fused multiply-adds

With the tables right, the paths passed, but the comparison (section 5) found the first car's position a bit off
Debug's from the first physics step on, on the x86-64 build host: the last bit of a coordinate at first, then more.
GCC's default for GNU C, `-ffp-contract=fast`, fuses `a * b + c` into one fused multiply-add wherever the target has
one, once it optimizes: on aarch64, whose base instruction set has it, and on x86-64-v3, which is the default target of
RHEL 10's GCC, the build host's (Ubuntu's targets plain x86-64, without FMA). The RelWithDebInfo game objects had
1,429 `vfmadd`s on that host. Bisecting the objects (Debug's for some, RelWithDebInfo's for the others, relinked)
named `func_800D0424`, the car's integration step (`RWR += RWV * dt`): alone at `-O2` it moved the car.

**Fix.** `-ffp-contract=off` for the game C. The N64's VR4300 has no fused multiply-add, so each operation rounded on
its own is the game's arithmetic, and `-O0` never contracts (0 fused operations in Debug before and after).

### 2.4 Loop bounds from declared array sizes

GCC bounds a loop's iterations by the arrays it indexes (`-faggressive-loop-optimizations`, on by default), on the
rule that no access runs past a declared array. The game C's declarations are views that often do: open-ended tables
declared with one element, struct members used as arrays past their end (`-O2`'s `-Warray-bounds` sees 260 such
accesses, writes at offset 788 into a 36-byte `pad0EC` among them). GCC warned of no case it could prove, but the rule
changed the loops of 65 game objects. **Fix**: `-fno-aggressive-loop-optimizations` for the game C, as `-fwrapv` and
`-fno-strict-aliasing` already say of their rules; `-O0` doesn't use it.

### 2.5 One storage under two names

GCC's alias analysis holds that two different external variables never overlap (the symbol table's
`equal_address_to` answers "different" for a memory access). The game C sometimes names one place twice: a symbol and
an address-named symbol inside it, two names at one address, a view that runs into the next symbol. port/data puts
them where the N64 has them, so at `-O0`, where every access goes to memory in program order, the names agree.
Optimized, a store through one may be taken not to change what the other reads. A minimal case,
`A[29] += add; if (A[29] < 0) B = 3; return A[29];`, returns the sum GCC holds in a register, not the 3.

`tools/port/alias_check.py` finds such code. It takes endian_check's accesses (GCC's GIMPLE of every live function:
base symbol, position, width, read or write) and the storage of each name from the data report: the RDRAM window for
arena names, an own storage's base and host offset for the rest. An access at a variable index covers the whole
extent. It reports two names whose accesses reach the same bytes in one function, one a write; and the same between
functions of one file that the file's calls may inline into one function. Over 23,948 accesses to 1,744 names in
1,427 functions it found one function and two such files:

| Where | Names | Fix (`TARGET_PC`, same addresses) |
|---|---|---|
| image A, `updateTrackScreen` | `D_80146108[29] += add`, then the wrap to 3 through `D_80146125` (= `&gOptions[29]`) | `D_80146125` is `D_80146108[29]` |
| image A, the track map | `mapObj[6].displayList` written through `D_803BAAE8`, read as `D_803BAB50`, by functions `updateTrackScreen` may inline | `D_803BAB50` is `D_803BAAE8[6].displayList` |
| MusyX sequencer (`lib_17dc0.c`) | `func_800198C8` stores the current sequence `D_8004BE78`, the helpers it calls read its byte `D_8004BE7B` | `D_8004BE7B` is the low byte of `D_8004BE78` (section 8) |

The current `-O2` objects happened to keep the order in all three; the fixes make that a guarantee. The other pairs
it lists are read and judged in its `REVIEWED` table (the player rows `D_803B5CFC[n][0]`, which land in a table no
code writes; the MusyX context table, whose one store depends on the reads before it), and three more are never in
one function with a write. CI runs it (section 4).

### 2.6 Outside the game C

- **`realpath`'s buffer.** Ubuntu's GCC enables `_FORTIFY_SOURCE=3` when it optimizes. `port/input/tests/test_frontend.c`
  gave `realpath` a 1,024-byte buffer, and the fortified `realpath` checks for `PATH_MAX` (4,096) whatever the
  path's length: every frontend test aborted ("buffer overflow detected"). The buffer is 4,096 bytes. The game C
  includes no C library header, so fortification doesn't reach it; a RelWithDebInfo build with `-D_FORTIFY_SOURCE=3`
  passes every test on the build host.
- **Windows: the weak definitions' anchor.** GNU as on PE names a weak definition's body
  `.weak.<name>.<the first global the file defines>`, and `gen_weak.py --pe` gives each file with weak copies a global
  of its own for that ([WINDOWS.md](WINDOWS.md)). With `-ftoplevel-reorder` the anchor was no longer first: the two
  copies of `path_D169C_path.c` both named their bodies after the common `D_80152818`, and the link failed. The anchor
  is `__attribute__((no_reorder))` now.

### 2.7 Two stores taken for one: a view over a one-element declaration

Found 2026-10-28 by `input_live_drive` ([FRONTEND.md](FRONTEND.md) "Testing live input"): in the RelWithDebInfo build
from mingw-w64's GCC 12.2 (the release's) and from Ubuntu 24.04's GCC 13 on x86-64, the car didn't steer. The
controller code's `func_800E7134` writes each pad's scaled stick through `#define gStickF ((f32 (*)[2])
&D_80140620)`, and its file declared `extern f32 D_80140620;`. A 4-byte store at a variable offset into a 4-byte
object can only be at offset 0, so GCC's dead-store elimination took `gStickF[i][0] = x` and `gStickF[i][1] = y` for
two stores to the whole object and dropped the first, the x the car steers by (the raw x and its calibration were
still loaded: they are volatile). GCC 14 (the build host's) and GCC 13 on aarch64 kept it; Debug doesn't eliminate
stores. No warning. **Fix**: under `TARGET_PC` the file declares `extern f32 D_80140620[][2]`, as the steering's
reader does; an incomplete array has no size to reason from. Other views in the game C over one-element
declarations can meet the same rule where one function stores two elements without `volatile`.

**The mechanism**, read in mingw-w64 12's dumps of the file before the fix: the loop optimizer (`ivopts`) made both
stores indexed references on the symbol's address, `MEM[(float *)&D_80140620 + ivtmp * 8]` and `MEM[(float
*)&D_80140620 + 4B + ivtmp * 8]` (TARGET_MEM_REFs). GCC's `get_ref_base_and_extent` hands such a reference back as
offset 0 of the declared object, with an unknown extent cut to the declaration's size: 4 bytes. Both stores are then
"the whole of `D_80140620`", and the late dead-store pass (`dse5`) deleted the first ("Deleted dead store: MEM[(float
*)&D_80140620 + ivtmp.108_155 * 8] = v_108"). The same view of two references as one place can forward a stored value
to a load of the other place (the value numbering and dominator passes after the loop optimizer), and a reference cut
to [0, size) is taken not to touch an access past the declaration. A related rule: an access through a pointer that
is wider than a variable's declaration is taken not to alias that variable. `-fno-strict-aliasing` covers none of
these (they are offset rules, not type rules). An array of unknown size (`T x[]`) has no size to cut to; a declaration
of the true size is cut correctly. GCC 14 (the build host's) and aarch64 didn't make the references here, which is why
the scripted checks, run there, passed.

**The inventory.** `tools/port/bounds_check.py` compiles every live game file again at `-O2 -g` with the build's
flags, `-Warray-bounds=2 -Wstringop-overflow=4 -Wstringop-overread -Wzero-length-bounds` and two GIMPLE dumps, and
lists, per file and symbol, every access outside the extent the file declares (weak context copies skipped):

- *view*: through a pointer computed from the symbol's address plus a variable (`_5 = &D_x + _4; MEM[(f32 *)_5 + 4B]
  = y`), or a view's variable index, past the declaration's size (the "alias" dump, after inlining, so the functions a
  file inlines into one are one). An array's own index (`D_x[i].f`) is inside it as GCC takes it and isn't listed;
- *const*: at a constant offset past the declaration;
- *tmr*: the loop optimizer's indexed references on the symbol's address (the "ivopts" dump, before the passes that
  act on them);
- *warn*: GCC's warnings, by the symbol their note names.

The class's shape is a function that reaches **two or more places** of such a symbol, one a write, one through a
variable (*view*, `two offsets`), or two indexed references at the declared size, or one beside another access, with
a write (*tmr*, `clamped`: this compiler made the references that are taken for one place). Constant offsets alone
are exact to GCC, and a function with one place, or only reads, has nothing to confuse. On the stick file before its
fix, GCC 13 finds both: `D_80140620 view [two offsets] +0:4[_57], +4:4[_57]` and `tmr [clamped]`.

| Compiler (x86-64), with the fixes | Files | *view* | *const* | *tmr* | *warn* | `two offsets` | `clamped` |
|---|---|---|---|---|---|---|---|
| GCC 13.3, Ubuntu 24.04 (CI's runner) | 1,308 | 108 | 7 | 49 | 49 | 0 | 0 |
| GCC 14.3, Rocky Linux 10 (the build host) | 1,308 | 108 | 7 | 48 | 49 | 0 | 0 |
| mingw-w64 GCC 12.2, Debian (the Pi; the release's) | 1,308 | 108 | 7 | 46 | 50 | 0 | 0 |

(Pairs of file and symbol; the 260 accesses of 2.4 were counted per access, weak copies included. Before the fixes,
main with the stick fixed: 122 *view* pairs and 20 `two offsets` on GCC 13 and on GCC 14. The counts are the tool's
and the declarations' after the UBSan cross-check below; before it, 109 *view*, 1 *const* and 53-54 *warn*.)
None of the 20 was `clamped` on these compilers: their loops index through `int` arithmetic the loop optimizer
doesn't fold into the symbol's address, or have one place per iteration. But each is the shape the stick had, and
which compiler folds which loop changes with versions and targets, so all 20 are declared whole.

**The fixes** (e353117), declarations only, under `TARGET_PC`, the `#else` the N64's: open-ended (`T x[]`) where the
file only takes the address; the true size where the data tables already had it; and where the file also uses the
symbol as one element (`x = v`, `(&x)[i]`), `#define x (x[0])` after it, so that every use means what it did (`&x` is
the same `T *`, `x` the same lvalue), as 2.2's second names do.

| File | Symbol | Declared | Was | What the function reaches |
|---|---|---|---|---|
| `src/blob/Input_SetAnalogBounds.c`, `Input_SetPadEnabledFlag.c`, `Input_SetPadSecondaryFlag.c` | `pad_config` | `PadConfig [4]` + macro | `PadConfig` | `(&pad_config)[pad].unk18`... per pad |
| `src/blob/camera_smooth_lerp.c` | `D_80154660` | `s32 []` | `s32` | five words of a record, by a byte pointer |
| `src/blob/car_mass_set.c`, `groups/camera_aspect_ratio/group.c` | `D_8012E700` | `s32 []` | `s32` | the 0x44-byte nodes' fields |
| `src/blob/func_80092484.c` | `D_801392D8` | `s32 []` + macro | `s32` | `&(&D_801392D8)[i]`, twice |
| `src/blob/func_800D4D84.c`, `func_800D5828.c` | `D_8014A250` | `D_8014A250_Record []` | a 2,026-byte record | the cars, 0x808 bytes each, fields to +2052 |
| `src/blob/func_800E9234.c` | `D_801526E0`, `D_801526F8` | `f32 []` + macro | `f32` | `(&x)[car] = 5.0f`, `= 3.0f` |
| `src/blob/func_800F8E90.c` | `D_801526A8` | `s32 []` | `s32` | +0, +4, +8 of 12-byte records |
| `src/blob/func_800FBE60.c` | `D_80152218` | `s32 []` | `s32` | x, y, z of each car's 12 bytes in a loop: the stick's shape, three stores |
| `src/blob/groups/controller_poll/group.c` | `D_80149AF8` (`gStickCal`), `D_80149B10` (`gPrevPressed`), `D_80149B50` (`gStickRaw`), `D_80149B90` (`gRepeatTime`) | `s8 [4][2]` + macro, `s32 [4]`, `s32 [2]`, `s32 [128]` | `s8`, `s32` | per pad, in the loop that had the stick's bug |
| `src/blob/groups/func_800E56F8/group.c` | `D_801440AD` | `s8 []` + macro | `s8` | the rumble byte of each pak record, three stores |
| `src/blob/groups/read_path_data/physics_response.c` | `D_80151CE8` | `Route []` + macro | `Route` (80 B) | the header and `(&D_80151CE8)[i]`'s records |
| `src/blob/save_load_data.c` | `D_80139320` | `s32 []` | `s32` | four words of 64-byte records |

The data tables: gen_data ranks every file's declaration of a symbol (DATA.md 1), so a new one can change the
winner. The first try (`[]` for all) changed `pad_config`'s and `D_80149AF8`'s winning types (no layout); with
their true sizes the winners are the old ones. `data.c` and `data_symbols.txt` are the files before but for two
arena BSS symbols the tables had as one word and that are now as long as the code reaches: `D_80149B50` 4 -> 8 bytes
and `D_80149B90` 4 -> 512 (the next known address bounds both; they sit in the RDRAM window at their N64 addresses, so
nothing moves). Debug and RelWithDebInfo give the same files, on GCC 13 and 14.

The other findings were judged by class, and each is in `tools/port/bounds_reviewed.txt` with its verdict: a *view*
whose functions reach one place of the symbol, or only read it; *const* offsets; *tmr* references with no other place
or write beside them; *warn* findings of symbols no function writes two places of. The `-Warray-bounds` findings are
mostly reads at a constant index past a one-element declaration, and `pad0EC[788]`-style members (2.4), which GCC
addresses exactly.

**The proof.** With the fixes, RelWithDebInfo against Debug along the 16 synthetic paths, `synth_compare.py --same
--runs 2`: the same trace, to the bit, every path, with GCC 14 (the build host) and with GCC 13 (Ubuntu 24.04, a
container on the build host: the CI runner's compiler, one that dropped the stick's x); `input_check.py` on GCC 13's
RelWithDebInfo program: all passed (the stick steers right and left). The Windows program runs only in CI
(`windows-run`: the paths and the steering check, on the cross-built RelWithDebInfo program); on the Pi the cross
build compiles with the fixes, and mingw-w64 12 finds no `two offsets` or `clamped` place with them (without the
stick's fix its `dse5` drops the x, as above). The full ctest passes on the build host in Debug and RelWithDebInfo
(190 of 190 each). Without the fixes the gate fails on the stick's file: `NEW ... D_80140620 view [two offsets]`.

**Flags considered** (the stick file before its fix, mingw-w64 12 at `-O2`, stores of the stick left):
`-fno-tree-dse` 2 and `-fno-ivopts` 2 (each stops this instance: no dead-store pass, or no indexed references to
cut); `-fno-ipa-modref`, `-fno-delete-null-pointer-checks`, `-fno-strict-aliasing` (already on) 1. Neither of the two
covers the class: without dead-store elimination the forwarding and the disjointness remain, and without `ivopts` the
member-array and wider-pointer rules remain, at the cost of every loop's addressing. Not used: the declarations are
the fix, and `bounds_check.py` keeps them so.

**The gate.** CI's Linux x86-64 RelWithDebInfo job runs `bounds_check.py` (section 4): a finding not in
`tools/port/bounds_reviewed.txt`, or a `two offsets` or `clamped` one whose verdict doesn't start with "fixed" or
"judged", fails it. The list holds what GCC 13 (the runner's), GCC 14 and mingw-w64 12 find; aarch64's GCC inlines
and warns otherwise, so the step runs on x86-64. A new finding is read like these: where its function reaches two
places of the symbol, declare the symbol whole; otherwise add it with its verdict.

**The compilers agree on the list** (checked 2026-10-28 on main, `--write-reviewed` per compiler): GCC 13.3 (the
Ubuntu 24.04 container on the build host, CI's runner's compiler), GCC 14.3 (the build host) and mingw-w64 12.2 (the
Pi's cross build, the release's compiler) each pass with none new, and each finds exactly the keys the list tags with
it: GCC 13 213 of the 219, GCC 14 212, mingw-w64 211 (after the cross-check below; before it 212, 211 and 210 of
218). The entries some compiler doesn't find are *tmr* references the loop optimizer makes or not, judged safe, and
one *warn*:

| Found by | Entries |
|---|---|
| GCC 13 and mingw-w64 12, not GCC 14 | `func_800A150C.c` D_8011EAEC, `ctx_newghost.c` pc_ghost_slot, `lib_1f5b0.c` D_80050440 (*tmr*) |
| GCC 14 only | `controller_poll/group.c` D_80149B64 and D_80156958 (*tmr*) |
| GCC 13 and 14, not mingw-w64 12 | `func_800F0914.c` D_80143FE0, `controlscr/caller.c` D_8015418C, `controlscr/rows.c` D_8014A10C, D_8015418C and D_80154618, `runtime_b_status_scene` D_803950C0 (*tmr*) |
| mingw-w64 12 only | `func_800B4DA4.c` D_80146108, `sfx_position_3d.c` D_8011B3A0 and D_80143F68 (*tmr*); `runtime_b_nearest_steering` `?` (*warn*: a note that names no symbol) |

So a GCC 13 run reports 6 reviewed keys it doesn't find, GCC 14 7 and mingw-w64 8 ("kept: other GCC versions");
none has a key the list lacks. A GCC update on the runner can add or drop *tmr* entries of this kind: a new one fails
the step until it is read and added with its compiler's tag.

**The UBSan cross-check** (later on 2026-10-28). The game C under all of UBSan reports each access a run makes past
its file's declaration: 152 object-size sites over the whole ctest ([SANITIZE.md](SANITIZE.md) 3.1). Read against
the inventory, they showed two holes in bounds_check, each of which hid a second place of a symbol in one function:
a read at a constant offset from a derived pointer (`_8 = MEM[(s8 *)p_14 + 1600B]`) was taken for an addition and
not listed; and a view through a declared pointer variable (`e = (EmitA *) &gCamera + i; e->f54 = ...`) had no
pointee type (the SSA name `e_2` isn't declared in the dump), so all its fields were one place. With both fixed, six
functions have the class's shape, and are declared open-ended under `TARGET_PC` like the fixes above:

| File | Symbol | Was | What the function reaches |
|---|---|---|---|
| `src/blob/func_800CFCA8.c` | `D_8014A250` | a 2,026-byte record | the six cars in a loop: reads +0x7C8 and +0x714, read-modify-writes +0x7D4 |
| `src/blob/Effects_UpdateEmitters.c` | `gCamera` | `s32` | each window's camera in a loop: three stores and four read-modify-writes, 4 bytes each |
| `src/blob/func_800A7D6C.c` | `D_8015F740` | `s32` | x, y and z of one record |
| `src/blob/func_8008D870.c` | `D_8012E700` | `s32` | a scene node's four texture slots |
| `src/blob/func_800A5518.c` | `D_8017A510` | `s32` | a window's first and bounds |
| `src/blob/input_status_update.c` | `pad_config` | `PadConfig` | three fields of one record (with the Input_ files' macro) |

None had *tmr* references on the three compilers, so this build compiled them as written; Effects_UpdateEmitters is
the stick's shape exactly (4-byte stores into a 4-byte symbol, in a loop). The tables are the files before but for
`D_8015F740`, arena BSS the tables had as one word, now 2,240 bytes to the next known address (as `D_80149B50`
above); Debug and RelWithDebInfo agree. The Input_ files' `pad_config[4]` isn't its extent: the storage is the 200
records at 0x80140BF0 and the paths reach records 4 to 91. Each of those functions reaches one field of the record
a variable picks, 1 or 2 bytes, so the cut to 160 bytes gives no reference as wide as the declaration and nothing at
a constant offset past it: no store or load to confuse. `[200]` would change the tables' type label of `D_80140BF0`
(`PadConfig[200]` for `Sprite[200]`, the layout the same), and waits for after the beta.

## 3. The flags

| Flag | Where | Why | Debug |
|---|---|---|---|
| `-ffp-contract=off` | game C (`game_flags`) | each float operation rounded on its own, as the VR4300 does (2.3) | no change: `-O0` never fuses |
| `-fno-aggressive-loop-optimizations` | game C | the declarations are views shorter than their data (2.4) | no change |
| `-fno-toplevel-reorder` | data.c (`pcdata`) | own storage in N64 address order (2.2) | no change: `-O0` keeps the order |
| `-O0 -g -UNDEBUG` | `game_types`, the second copy of the game objects for gen_data's types (non-Debug only) | DWARF with every declaration (2.1) | not built: the game objects are used |
| `-O0` | `game_probe` (gen_weak) and `pc_data_test_decls` | the same at every build type; the probe is quicker | no change |
| `no_reorder` (attribute) | gen_weak's PE anchor | first global of its file (2.6) | no change |

Already there, and needed for the same reason: `-fwrapv`, `-fno-strict-aliasing`, `-fcommon`.

Considered and not used:
- `-fno-delete-null-pointer-checks`: the port maps nothing at address 0, so a path where GCC drops a NULL test after a
  dereference faults at `-O0` too.
- `-ftrivial-auto-var-init`: would give uninitialized locals a value Debug doesn't, which changes Debug or makes the
  builds differ. The comparisons found no path where the two builds differ.
- Making the busy-waits volatile: each one in live code calls `pc_os_spin_yield` (`PC_SPIN`), an external call, which
  makes the compiler reload what the loop reads; a busy-wait without it would hang at `-O0` too, under the
  one-thread-at-a-time scheduler.
- `-U_FORTIFY_SOURCE` for the game: no game file includes a C library header.

## 4. The checks that keep it so

- gen_data refuses optimized DWARF (2.1); `gen_data.py --check` verifies the own storage's order (2.2).
- CI (`.github/workflows/pc.yml`) builds Linux on both architectures and the Windows cross build as Debug and as
  RelWithDebInfo, and runs every test, the boot without a ROM and the synthetic paths in each. The RelWithDebInfo Linux
  jobs also run `alias_check.py`, and the x86-64 one `bounds_check.py` (2.7). Then:
  - `optimized` (per architecture): the two builds' `data.c` and `data_symbols.txt` must be the same files, and
    `synth_compare.py --same` runs every path with both programs, twice each, on the virtual clock: every trace must
    be the same, to the bit. Its runs go four at once (`-j 4`): each is a process of its own on the virtual clock,
    whose course doesn't depend on what else the host runs;
  - `optimized-windows`: the cross builds' `data.c` and `data_symbols.txt` must be the same files;
  - `windows-run` runs the self-test, the boot without a ROM and the 14 paths with both cross-built programs.

## 5. The same game

`tools/port/synth_compare.py` runs every press script's path with two programs, several times each, and compares their
traces: the loader's summary line (without its table's size since DECOUPLE.md 4), every state change and the retrace it
happened at, and a status line each second of game time with the players, windows, cars, mode, track, pause state, and
the first car's position and speed as exact floats (`RUSH_STATE_EXACT`). It runs on the virtual clock (`--fast`), where
the main loop works only while every N64 thread waits, so a run takes the same course every time ([ULTRA.md](ULTRA.md)
1). Before that change (made for the high frame rate while this work went on) a run was the same only to within a
graphics task, and on the Pi 5 two runs of one Debug build drifted apart in most races; this work had an opt-in mode of
its own for it at first, and the results below were all repeated on the merged clock.

| | Paths | Result (Debug against RelWithDebInfo, 2 or 3 runs each) |
|---|---|---|
| synthetic cartridge, x86-64 (build host) | 14 | the same trace, every run, every path |
| synthetic cartridge, aarch64 (Pi 5) | 14 | the same; and the same as x86-64's, path for path |
| retail cartridge, aarch64 (Pi 5) | 14 press scripts: the front end and its screens, races for 1-4 players (3 and 4 in a stunt arena), circuit, the attract demo, driving, pause and its screens, a retirement through the results back to the main menu, a fresh controller pak | the same trace, every run, every path; every run reached its state |

Before the fix of 2.3 the synthetic races differed from the first second of physics on the x86-64 host, by a bit or
two of the car's position at first. The retail runs and their logs stay on the Pi (nothing from the cartridge is in the
repository); `synth_compare.py --rom <rom>` repeats them.

Debug against the Debug build before this work (`--coarse`, since that program prints whole feet, and its clock was
the one before the change above): every synthetic path agrees, within that clock's tolerance.

## 6. Speed

Wall time of the `--fast` paths (the virtual clock: as fast as the host goes), the least of three runs, and the host
time a race frame costs: the drive path to 300 and to 1,500 graphics tasks into the race, the difference over 1,200.

| Host | Cartridge | Debug | RelWithDebInfo | |
|---|---|---|---|---|
| x86-64 (i5-14500T) | synthetic, the 14 paths | 9.90 s | 7.13 s | 1.39x |
| | synthetic, a race frame | 0.78 ms | 0.47 ms | 1.66x |
| aarch64 (Pi 5) | synthetic, the 14 paths | 32.78 s | 25.55 s | 1.28x |
| | synthetic, a race frame | 2.39 ms | 1.75 ms | 1.37x |
| | retail, the 14 press scripts (least of two runs) | 211.51 s | 101.09 s | 2.09x |
| | retail, a race frame (`drive`, gas and steering) | 15.34 ms | 6.69 ms | 2.29x |

In real time, headless (the VI at 60 Hz, a frame every second retrace), the CPU time a race frame costs, from the
process's user and system time over 600 frames of the `drive` race (300 to 900): on the Pi 5 with the retail
cartridge 12.45 ms in Debug and 5.60 ms in RelWithDebInfo (2.22x) of the 33 ms a frame has; on the x86-64 host with
the synthetic cartridge 1.45 ms and 1.38 ms.

The synthetic cartridge's models are boxes and its textures solid, so its frames are mostly the renderer's and the GPU
driver's, which the build type doesn't change; the retail cartridge's are mostly the game's.

**Release** (`-O3`, no `-g`) works the same way: on the x86-64 build host it passes all 143 tests, its tables are the
same files as Debug's, and all 14 synthetic paths give Debug's traces to the bit. Nothing above needs the game
objects' own debug information: gen_data reads the `-O0` copy, and endian_check and alias_check compile their own
with `-O1 -g` after the build's flags. CI tests RelWithDebInfo; a release built as Release should get the same CI
entries first.

## 7. Windows

The mingw-w64 cross build (`cmake/mingw-w64.cmake`) builds as RelWithDebInfo with the anchor fix of 2.6, and CI runs
it on Windows: the self-test, the boot without a ROM and all 14 synthetic paths. Its tables are the same files as the
Debug cross build's.

## 8. Found on the way: the sequencer's byte

`func_800198C8` stores the current sequence slot as a word, `D_8004BE78 = i` (0 to 7), before it runs that slot's
sequence. `func_80020610(controller, channel, slot, value)` takes the slot third: `func_80017D38` passes it as
`(unsigned char)D_8004BE78` for some controllers, but `func_800177EC` (controller 130), `func_80017824` (controller 7)
and `func_80017D38`'s other controller events pass the byte `D_8004BE7B`. On the N64 that byte is the word's low
byte, the slot; the PC holds the word little-endian, so byte 3 is its high byte, 0, and those messages of every
sequence went to slot 0's channels, in Debug too; `endian_check.py` doesn't see it (the word is BSS, stored as a
word and read through another symbol). The optimized build kept that byte 3, so that it would do what Debug did; the
fix came after, in both: `src/rom/lib_17dc0.c` reads the low byte, `#define D_8004BE7B ((u8)D_8004BE78)`, as the
N64 does.
