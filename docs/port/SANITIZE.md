# Sanitizer builds: AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer

Written 2026-10-28, before the beta. The night before, several real bugs came from C undefined behaviour that an
optimizing GCC exploited, and from records laid out differently than the code read them ([OPTIMIZED.md](OPTIMIZED.md)
2.7, [LAYOUT.md](LAYOUT.md) 9, [BRINGUP.md](BRINGUP.md) 15, [QA.md](QA.md)). The PC layer's own code (port/) had never
run under a sanitizer. This is the build that does that, what it found, and what it doesn't check.

```
cmake -S . -B build/asan -G Ninja -DRUSH_SANITIZE=address,undefined     # aarch64 only (section 1.1)
cmake -S . -B build/ubsan -G Ninja -DRUSH_SANITIZE=undefined            # any Linux
cmake -S . -B build/tsan -G Ninja -DRUSH_SANITIZE=thread                # x86-64 (section 1.1)
cmake --build build/asan && ctest --test-dir build/asan
```

## 1. The builds

`RUSH_SANITIZE` (cmake/sanitize.cmake) is a comma-separated list of `address`, `undefined` and `thread`. Linux only
(MinGW's GCC has no sanitizer runtimes), and `address` and `thread` don't combine. Debug is the build type the
results below come from; the option works with any.

| What | Instrumented with |
|---|---|
| port/ (ultra, input, config, app, assets, data's loader, audio, gfx, upscale) and every test program | the sanitizers asked for; UBSan with every check of `-fsanitize=undefined`, alignment included, plus `float-cast-overflow` |
| the game C (`game_flags`: the game, and the game C the test programs link) | ASan (without its global redzones, below) as asked, TSan only with `RUSH_SANITIZE_GAME_THREAD` (section 6); of UBSan only `RUSH_SANITIZE_GAME_UB` |
| `game_probe`, `game_types`, `pcdata` (data.c) (`PC_NO_SANITIZE`) | nothing |

**The game C's UBSan checks** (`RUSH_SANITIZE_GAME_UB`, a cache variable): `bounds`, `shift`,
`integer-divide-by-zero`, `float-cast-overflow`, `pointer-overflow`, `null`, `vla-bound`, `unreachable`. Left out,
because the m2c code is written that way on purpose and the build already says so, or because there are too many to
read now:
- `object-size`: the class of OPTIMIZED.md 2.7, a symbol declared narrower than the code reaches it. The game C
  under all of `undefined` (`-DRUSH_SANITIZE_GAME_UB=undefined`) over the whole ctest reaches 152 such sites; each
  was read against `tools/port/bounds_check.py`'s inventory (section 3.1), which took six declarations open-ended
  and closed two holes in the tool. The rest are views by design, judged there one by one, so the check stays out;
- `alignment`: the same run found one site, `func_8008E06C` reading a `Color` from a caller's `u8 [4]` 2 bytes off
  a word (section 3.2, fixed), and the retail paths one more, harmless (3.3). The port's own code keeps the check;
  the game C leaves it out because the m2c code reads N64 records through PC types whose alignment can be larger;
- `signed-integer-overflow`: the game C is compiled `-fwrapv`, so overflow is defined (GCC doesn't check it under
  `-fwrapv`: the full run found none); IDO's arithmetic wraps;
- `return`: C++ only in GCC and Clang;
- `float-divide-by-zero`: defined by IEEE 754 (Annex F), and the VR4300 gives the same infinities.

`bounds` checks what the declarations say. The game's declarations are views, often shorter than their data
(CMakeLists.txt's `-fno-aggressive-loop-optimizations` note), so a bounds finding is either such a view (section 4)
or a real read past an object. Both kinds turned up (section 3).

**Not instrumented.** gen_weak reads the probe's symbols and gen_data the `-O0` types copy's DWARF and nm, so
those stay as in every other build: a sanitizer build compiles `game_types` even as Debug, and `data.c`, `data_layout`
and `data_symbols.txt` are the same files as a normal build's. data.c's own storage has to stay in N64 address
order with nothing between (OPTIMIZED.md 2.2), so it gets no redzones.

### 1.1 The RDRAM window and the runtimes' memory

The window is an 8 MiB memfd mapped at 0x80000000 and again at 0xA0000000 ([ULTRA.md](ULTRA.md) 2). The runtimes'
layouts, from `ASAN_OPTIONS=verbosity=1` (GCC 12 on the Pi 5, Clang 21's and GCC 14's runtimes on the x86-64 build
host):

| Runtime | x86-64 | aarch64 (Pi 5: 47-bit, 16 KiB pages) |
|---|---|---|
| ASan | LowMem 0-0x7FFF7FFF, LowShadow 0x7FFF8000-0x8FFF6FFF, ShadowGap 0x8FFF7000-0x2008FFF6FFF | LowMem 0-0xFFFFFFFFF, shadow from 0x1000000000 |
| TSan | low application memory from 0x1000: the window is the program's | refuses to start: "unsupported VMA range, found 47, supported 39, 42 and 48" |

- **ASan on aarch64**: the window is in LowMem and the runtime maps nothing there. It works unchanged.
- **ASan on x86-64**: not possible. 0x80000000 is inside the low shadow and 0xA0000000 inside the shadow gap, and the
  shadow offset is a constant compiled into the runtime and into every check: no `ASAN_OPTIONS` moves it
  (`protect_shadow_gap=0` only maps the gap's own shadow, which covers 0xA0000000 too). Mapping the window over the
  runtime's reservations with `MAP_FIXED`, and zero pages for the window's own shadow (0x8FFF8000, 0x93FF8000),
  makes plain loads and stores work: tried, and it does. But every range check treats an address outside the
  runtime's memory as bad, so a `memcpy` of more than 64 bytes into the window, and a GCC-instrumented access of an
  odd size (`__asan_storeN`), reports "unknown-crash ... in the low shadow area": PI DMA, the data loader, zlib's
  inflate, every struct copy in game data. `cmake/sanitize.cmake` refuses `address` on x86-64; the x86-64 runs are
  UBSan and TSan.
- **TSan on x86-64** works unchanged: its `mmap` interceptor maps shadow for the window. The Pi 5's kernel has a
  47-bit address space, which TSan doesn't support.

### 1.2 The fault handler

ASan and TSan install SIGSEGV, SIGBUS, SIGFPE and SIGILL handlers before `main`, and `pc_fault_install` replaces
them. In a sanitizer build (`PC_SANITIZE`, port/ultra only) it keeps the runtime's handlers: a fault prints the
usual first line (the address and the N64 thread) and tells the app (`pc_on_fault`, the log and the message box),
then puts the runtime's handler back and returns. The instruction faults again and the runtime reports it, with
its own stack, symbolized, and thread history. A signal that was sent rather than raised by an instruction is sent
again. `pc_fault_thread_init` keeps the alternate signal stack the runtime gave each thread. CTest
`sanitize_fault_chain` (ASan builds) boots the blank cartridge, which faults in `audio_helper`, and checks for both
reports.

Tests that check a fault's own report and exit status (`app_firstrun`'s "log: a fault", -11) need the port's
handler alone, so ctest's environment sets `handle_segv=0` (and bus, fpe, ill) for ASan and TSan. A run outside ctest
gets the chained reports. The port's report is best effort on the way to dying: `backtrace()` loads libgcc's
unwinder, which allocates, and addr2line runs through `popen`, neither safe in a signal handler. TSan reports that
("signal-unsafe call inside of a signal"), so ctest's TSan options say `report_signal_unsafe=0`.

### 1.3 Threads, weak symbols, absolute symbols

- **Threads.** Each N64 thread is a host thread made by `pthread_create`, which the runtimes intercept; one runs at
  a time, handing a baton over a mutex and condition variables, which TSan sees as the happens-before it is. The 2 MiB
  stacks are enough with ASan's redzones (no stack overflow in any run).
- **Weak symbols.** gen_weak's `#pragma weak` copies and port/ultra/defaults.c's weak no-ops link as before. Strong
  and weak definitions are both instrumented; nothing about them changes.
- **Absolute symbols** (gen_stubs' `aliases.ld`, data.c's arenas) are not globals the compiler emits, so ASan
  registers nothing for them and their shadow is the window's, all addressable. The GOT fix-up
  (`pc_mem_fix_absolute`) runs as before; the runtimes' own relocations aren't at 0x80000000 or above.
- **gen_stubs** would have stubbed the runtimes' entry points (`__asan_*`, `__ubsan_*`, `__tsan_*`...), which the
  instrumented game objects reference: `n64syms.SANITIZER_PREFIXES` leaves them to the linker, and the census
  skips them.
- **The game's C data** (asan-globals=0). A runtime image's C data is one range that the loader saves at startup and
  puts back on each load of the image (port/data/overlays.ld); ASan's redzones between its globals made the loader's
  `memcpy` of that range a global-buffer-overflow at once. The game also walks from one symbol to the next where the
  N64's layout allows it (OPTIMIZED.md 2.2). So the game C's globals get no redzones (`--param asan-globals=0`);
  their accesses, and the heap and stacks, are still checked. UBSan's `bounds` still sees reads past a game table
  (section 3).
- **LeakSanitizer** doesn't scan mappings it didn't make, so a host allocation the game keeps only in RDRAM would
  look leaked: port/ultra/memory.c registers the window as a root region.

### 1.4 The x86-64 build host

Rocky Linux 10's GCC 14 has the sanitizer libraries' link scripts but not the runtimes (`libasan`, `libubsan`,
`libtsan` aren't installed). They were taken, without installing anything, from the distribution's own packages
(`dnf download libubsan libtsan libasan liblsan`, unpacked with `rpm2cpio` into `~/rush2049pc/sanitize-rt`), and
linked from there:

```
RT=$HOME/rush2049pc/sanitize-rt/usr/lib64     # with libubsan.so -> libubsan.so.1 etc. made there
cmake -S . -B build/ubsan -G Ninja -DRUSH_SANITIZE=undefined "-DCMAKE_EXE_LINKER_FLAGS=-L$RT -Wl,-rpath,$RT"
```

Ubuntu's GCC (CI) has them installed.

## 2. Running

`ctest` in a sanitizer build gets the runtimes' options from the build's `sanitize_env.cmake`, unless the variable
is set already: `halt_on_error=1` for UBSan and TSan (ASan always stops), the suppressions in
`tools/port/sanitize/`, `detect_leaks=1`, `print_stacktrace=1`, and the signal options of section 1.2. The Python
test scripts pass the environment on to the program. A finding not suppressed fails its test.

For a hunt, set the variables yourself so that one run prints every finding, one file per process:

```
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0:log_path=$PWD/ub ASAN_OPTIONS=log_path=$PWD/asan \
    ctest --test-dir build/ubsan -j3
cat ub.* | grep 'runtime error' | sort | uniq -c | sort -rn
```

In an ASan+UBSan build (GCC 12) UBSan's reports went to stderr, not to `log_path`: grep the program's output or its
log (synth_paths' `NAME.game.log`) for `runtime error`.

What was run, all Debug:

| Build | Host | Runs | Result |
|---|---|---|---|
| UBSan | x86-64 (GCC 14) | the whole ctest, 194 tests: the unit tests, the 17 synthetic paths, the real-time ones | after the fixes, every test passes with the suppressions of section 4 |
| ASan + UBSan | Pi 5 (GCC 12) | the whole ctest; 31 retail paths with `--fast` | after the fixes every test passes but `gfx.all.wide-scale6.75` and `gfx.all.native-2880x1620`, whose `eyeplane` and `seams` cases fail on the Pi's V3D in the normal build too (a few pixels' rounding at 6.75x and 2880x1620; not the sanitizers'); on the retail paths no ASan or LSan report, and UBSan's findings are 12-14 (fixed) and two suppressed (section 4) |
| TSan (port/ instrumented) | x86-64 (GCC 14) | the whole ctest | after the fixes (and main's HUD fix), races only inside Mesa and SDL (section 4) |
| UBSan, the game C under all of `undefined` (`-DRUSH_SANITIZE_GAME_UB=undefined,float-cast-overflow`, `halt_on_error=0`) | x86-64 (GCC 14) | the whole ctest, 208 tests, the 19 synthetic paths among them | 152 object-size sites in 133 lines of 52 files and one alignment site, no other check; after the fixes of 3.1 and 3.2, 116 object-size sites in 107 lines of 46 files, each judged, and none of alignment; every test passes |
| the same, with the fixes | Pi 5 (GCC 12) | 7 retail paths with `--fast` (section 3.3) | 35 object-size sites the synthetic paths don't reach, all loads, judged; one alignment site, harmless; no other check |

The retail paths ran on the Pi only, with the ROM by its path there, one at a time, with the press scripts the QA
sweep wrote ([QA.md](QA.md)), copied to the ignored `build/scratch/`: the attract demo, a drive and a single race on
each of the 8 tracks, races for 2 and 4 players, battle and its end, stunt and its end, circuit, ghost, practice, the
expansion-pak tracks, the pause screen and its quit, every front-end screen, option row and widescreen setting, the
controller pak's screens, the cheat screen, mode 5. All 31 reached their time. The ones with findings ran again
after the fixes: clean. Each ran the game time its script needs (40-130 s); 70 s of a race took
92 s of the (shared, busy) Pi's time.

## 3. Findings

| # | Where | Tool | What | Fix | Test |
|---|---|---|---|---|---|
| 1 | port/audio/audio_out.c | TSan | at exit `pc_audio_shutdown` cleared `g.inited` while the game's audio thread still pushed; `ensure_init`'s unlocked test opened the output again, or a push that had tested just before went on into the freed ring | the shutdown marks the output closed under the lock; the game's calls test it under the lock and do nothing | `audio_volume` pushes after the shutdown |
| 2 | port/ultra/rcp.c, loop.c | TSan | the real-time audio task count, written by the game's scheduler thread, read by the main thread's status lines | relaxed atomics | (TSan) |
| 3 | port/app/log.c | TSan | `L.done`, a volatile int the log's copying thread sets and `pc_app_log_close` polls | `__atomic_store_n`/`__atomic_load_n` (async-signal-safe) | (TSan) |
| 4 | port/main.c | LSan | `--set` strings strdup'd into a local array, lost when `rush_main` returned | the array is static | (LSan, every `--set` run) |
| 5 | src/ovl_b/groups/weapons/weapons.c | UBSan bounds, ASan | a car that loses its weapon (battle.type 8, none) has its row of `D_803943A4[8][13][3]` read, past the table, and not used: on the N64 the data after it, on the PC whatever the linker put there | `WEAPON_OFF` gives type 8 a row of zeros (below) | the synthetic battle paths under UBSan |
| 6 | src/blob/groups/car_resistance/group.c | UBSan bounds | `D_8011128C[mode * 13 + type]` through `extern s8 D_8011128C[13]`: a table of rows declared as its first, which an optimizer may take for every index's bound (OPTIMIZED.md 2.7) | incomplete under `TARGET_PC`, as the other files declare it `[][13]` | (UBSan) |
| 7 | src/ovl_b/groups/runtime_b_status_scene/group.c | UBSan float-cast-overflow | a score fade's alpha, 346.6 while the score's start is ahead of the clock, converted to u8: undefined in C | through s32 to its low byte, as the N64's trunc.w.s and sb (`ALPHA_U8`) | (UBSan) |
| 8 | tools/port/n64syms.py (gen_stubs) | reading the sanitizer build's stubs.c | `pc_ovl_a_start`/`end`, `pc_ovl_b_start`/`end` (overlays.ld's) were stubbed as 4 KiB arrays that the script's symbols overrode | linker-defined | the link |
| 9 | port/ultra/tests/test_romcheck.c | ASan | `image()` copied 4 bytes of the game code from `""` | copies the code's length, zeroes the rest | `ultra_romcheck` |
| 10 | port/assets/tests/test_targets.c | UBSan alignment | a node's control field (an N64 address, 4-aligned) read as a host pointer | read as u32 | `assets_targets` |
| 11 | port/gfx/tests/test_gbi.h, gfx_test.c | UBSan shift | `GBL_c1` shifts 3 by 30 places into an int's sign bit; a texture rectangle's `-4 << 10` | unsigned; the negation after the shift | the gfx tests |
| 12 | src/blob/audio_voice_set.c | UBSan float-cast-overflow (retail: quit) | `(u32)(rate * 255 + 63)` is -125.9 while a voice's start is ahead of the clock. IDO's code and x86-64 go through a signed conversion and keep the bits (0xFFFFFF83: no fade); aarch64's `fcvtzu` gives 0, so **the arm64 builds set the voice's alpha to 192 where the N64 leaves it** | a negative value through s32 (`F2U`) | (UBSan, retail) |
| 13 | src/blob/func_8010E0FC.c | UBSan shift (retail: attract demo, screens) | a coin's view mask `1 << player->slot` with slot -1 | the count's low 5 bits, as sllv, shl and lsl take (`SLOT_BIT`) | (UBSan, retail) |
| 14 | src/blob/groups/particle_knot/group.c | UBSan float-cast-overflow (retail: battle) | `func_8009F058` stores a far vertex's coordinate, 32800, into an s16 | through s32, whose low half the store keeps, as trunc.w.s and sh (`V16`) | (UBSan, retail) |
| 15 | port/gfx/gfx_hud.c, gfx_rdp.c, gfx_api.c | TSan | the widescreen HUD's offset (`pc_gfx_hud_dx`, new on main), which the game's threads ask for while they draw (`car_lights_render`), reads the frame's width (`pcgfx`) and the resolved widescreen setting (`pcgfx_cfg`), which the main thread writes while it runs the previous frame's display list and presents it. In real time only; a resize or a scale change can give one frame's HUD the other frame's offset | fixed on main by port/gfx's owners (cd8336d: the HUD reads atomics published at SETCIMG); the suppression is gone | the TSan run |
| 16 | port/assets/tests/test_targets.c | ASan | the camera record names texture 361, and the test's stand-in `D_801427C0` had 64 entries: the game read past it | 1,024 entries | `assets_targets` |
| 17 | port/assets/tests/test_text.c | ASan stack-use-after-scope | `D_8014A160` held a pointer to a block's local name, which `voice_stop` read after the block | static | `assets_text` |
| 18 | port/audio/tests/test_forms.c | LSan | the scenes' buffers were held by main's locals only | static | `audio_forms` |
| 19 | port/ultra/loop.c, rcp.c (new on main: the RSP yield) | TSan | the exit line read the audio task and graphics yield counts while the game's scheduler could still count | relaxed atomics on both sides | `synth_boot_race` under TSan |

**The first fix for 5 was wrong, and a retail run caught it.** It gave the table a ninth row under `TARGET_PC`. But
gen_data takes a C-defined symbol's size from its DWARF and maps the N64 addresses it covers to the C object
(data.c's `PC_SYM_CDEF` entry went from 1,248 to 1,404 bytes): the 156 bytes of N64 data after the table became part of
it for the loader. The 4-player retail run (`race4p`) then faulted in `particle_system` 30 s in, on a node with no
model (0xFFFF). The normal build ran it through. So the table keeps its N64 size, and the read is guarded; the
ASan+UBSan build then runs `race4p` to the same states, to the same car positions, as Debug. **The rule: never change
the size of C data that has an N64 address.**

The port's own code, run through every test and path, had no other finding under UBSan, ASan or LSan: no buffer
overflow, use after free, leak, misaligned or out-of-range access, and no other race than 1-3, 15 and 19. The 5
synthetic paths run with `detect_stack_use_after_return=1` (game C included) had no report either.

**Arch-dependent behaviour.** Finding 12 is the one that changed what the game does: a float out of an integer's
range converts differently on each host (x86-64's `cvttss2si` gives INT_MIN for anything out of an int's range and
keeps a negative value's bits for an unsigned one, aarch64's `fcvtzs`/`fcvtzu` saturate, to 0 for a negative to
unsigned), and IDO's code keeps the bits like x86-64. The game C converts floats to unsigned types in many places, so
paths not run here may hold more; UBSan's `float-cast-overflow` finds them where they run.

From the synthetic cartridge only: its camera and particle positions are NaN (its data, [SYNTH.md](SYNTH.md) 4),
and the view code converts them to integers. That is undefined too, and x86-64 gives INT_MIN where aarch64 gives 0.
None of those conversions happened on a retail path. If a NaN ever reaches them in a real game, the picture, not the
game state, differs between the builds.

### 3.1 The game C's object-size sites

The game C under all of UBSan (section 2's table) reports each access a run makes past the size its file declares
for the object: 152 sites on the synthetic paths and the unit tests. Each was joined with `tools/port/bounds_check.py`'s
accesses on its line (`--dump`: symbol, offset, width, read or write, at `-O2`) and with the address UBSan gives
(in the RDRAM window, the name whose storage holds it), and its function's other accesses to that symbol were read.

**What GCC can do with a declared size** ([OPTIMIZED.md](OPTIMIZED.md) 2.7). It cuts the extent of a reference
that has a variable offset on the symbol's own address (the loop optimizer's TARGET_MEM_REFs, an array's variable
index) to [0, declared size), and takes a pointer access wider than the declaration not to touch it. A reference
through a pointer variable at a constant offset (`MEM[(f32 *)e_5 + 84B]`) is exact to the pointer and isn't cut;
constant offsets on the symbol are exact. So:
- a store is dropped only when a later store is taken to cover it: two cut references in one function, each as wide
  as the declaration (the stick: 4-byte stores, a 4-byte symbol);
- a load is forwarded from a store to another place only under the same equality;
- one access is taken not to touch another when one is cut and the other is at a constant offset past the
  declaration, or a direct access sits beside a wider pointer access;
- a loop bound is derived from a declaration only by `-faggressive-loop-optimizations`, off for the game C (2.4).

All of these need two places of the symbol in one function, one a write: bounds_check's `two offsets` and `clamped`.

**Two holes in bounds_check**, which hid that shape, and are fixed:
- a read at a constant offset from a pointer computed from the symbol (`_8 = MEM[(s8 *)p_14 + 1600B]`) had the
  ` + ` inside its brackets taken for an addition and wasn't listed: a function that wrote one place and read
  another counted one place. 19 sites were in no finding at all;
- a view through a declared pointer variable (`e = (EmitA *) &gCamera + i; e->f54 = e->f24;`): the SSA name `e_2`
  has no declaration of its own in GCC's dump, so the tool had no pointee type and took every field for one place
  of unknown width.

With both, every site is in a finding, 11 new (file, symbol) pairs (reads only, or constant offsets: listed in
`bounds_reviewed.txt`), and six functions have the class's shape. Each is declared open-ended under `TARGET_PC`, as
2.7's fixes, and its sites are gone:

| Sites | File | Symbol (declared) | What the function reaches |
|---|---|---|---|
| 8 | `src/blob/func_800CFCA8.c` | `D_8014A250` (a 2,026-byte record) | the six cars in a loop: reads +0x7C8 and +0x714, read-modify-writes +0x7D4 |
| 14 | `src/blob/Effects_UpdateEmitters.c` | `gCamera` (`s32`) | each window's 0x98-byte camera in a loop: three stores, four read-modify-writes, 4 bytes each: the stick's shape |
| 3 | `src/blob/func_800A7D6C.c` | `D_8015F740` (`s32`) | x, y and z of one record, set to 0 |
| 5 | `src/blob/func_8008D870.c` | `D_8012E700` (`s32`) | a scene node's four 8-byte texture slots |
| 2 | `src/blob/func_800A5518.c` | `D_8017A510` (`s32`) | a ZOID window's first and bounds pointers |
| 4 | `src/blob/input_status_update.c` | `pad_config` (one record) | three fields of one of the 200 records |

None had indexed references on any of the three compilers (no *tmr* finding), so none was miscompiled in these
builds; Effects_UpdateEmitters is where a compiler that indexes its loop on the symbol would drop two of every three
stores, as with the stick. The data tables are the files before but for `D_8015F740` (arena BSS, one word in them),
now as long as the next known address allows, 2,240 bytes, as 2.7's `D_80149B50`; Debug and RelWithDebInfo agree.

**The 116 left**, by what each function does with the symbol:

| Sites | What | Where (symbol) | Verdict |
|---|---|---|---|
| 16 | one field of one record, picked by a variable, 1 or 2 bytes, read or written | `Input_SetAnalogBounds.c`, `Input_SetPadEnabledFlag.c`, `Input_SetPadSecondaryFlag.c` (`pad_config`) | safe. 2.7 declared it `[4]` as "its extent", but its storage is the 200 records at 0x80140BF0 (`Sprite[200]`) and the paths reach records 4 to 91. A cut to 160 bytes makes no reference as wide as the declaration, and no access there is at a constant offset past it. `[200]` would make the declaration true; it changes the tables' type label of `D_80140BF0` (`PadConfig[200]` for `Sprite[200]`, the layout the same), so it waits for after the beta |
| 15 | one place written (a store or a read-modify-write of that place), nothing else of the symbol in the function | `InitBlits.c` (`D_80149450`, a loop's one store), `CarSetup.c` (`D_801439F8`), `func_800CB718.c` (`D_801392D8`), `audio_effect_remove.c` (`D_80153F28`, `D_80153F48`, `D_80153F68`), `func_800A79DC.c` (`pad_config`), `func_800D1248.c` (`D_8014A96C`), `func_800E9234.c` (`D_80152574`), controller_poll's `func_800E7134` (`D_8013FED0`, `D_801403C0`: a loop's one store each, indexed on the symbol by GCC; `D_80149B30`, volatile) | safe: nothing to drop it for or forward it to; a loop's stores don't cover each other while the loop can exit |
| 77 | reads only, one place or several | the cars' records (`D_8014A250` in `CarSetup.c`, `car_select_handler.c`, `func_800CF604.c`, audio_frame's and audio_frame_update's groups, func_8008B640's group; `D_8014A258`, `D_8014AA32`, `D_8014A964`), the scene nodes (`D_8012E700` in six files), the route (`D_80151CE8`, `D_80151D18`, `D_8012E5E8`), the cameras (`gCamera`, `D_80124FD0` in `particles_spawn_emitter.c`), the controllers (`gControllerData`, `D_80149B50`), the windows (`D_8017A510` in `func_800A7428.c`), and the rest `bounds_reviewed.txt` lists | safe: nothing written |
| 8 | constant offsets past the declaration | `func_800AED20.c` (`D_801461B0`), camera_transform (`D_80146138`, `D_80146160`), `entity_flag_check` (`D_80149808`) | safe: GCC keeps them exact |

A view that runs from its name into another name's storage is 2.5's class (`alias_check.py`), which takes a variable
access to cover its name's own extent: for every site in the window, the names holding the address UBSan reports
were checked against the names its file accesses. One pair is both, `D_8014A964` and `D_8014A250` in
`car_select_handler.c` (reads only), and alias_check pairs them already (`D_8014A964` is inside `D_8014A250`'s
storage). These are the synthetic paths' sites; another path can reach others, and the same reading applies.

### 3.2 The misaligned colour

`func_8008E06C` (ZOID_SetColor) reads the colour it is given as one word, `col->w`. Image A's `func_80394DD8` passes
its `u8 buttonColor[4]`, which GCC put at 2 mod 4 (UBSan's alignment check, in 32 of the runs, from image A's front
end). An N64 `lw` there would have taken an address error, so IDO's frame had it on a word. The same in five more
locals: `func_80397AD8`'s `u8 color[4]`, `func_803998B8`'s `Color4` (a struct of four bytes only on the PC: its
`TARGET_PC` typedef reads the ROM's bytes as bytes, [ENDIAN.md](ENDIAN.md)) and three functions of
`runtime_a_callers_deep` (`Color4`, four `u8`, in both builds). The other callers' colours are unions with a word.

It didn't change the game: x86-64 and aarch64 load a misaligned word from ordinary memory and give the value an
aligned load of those bytes gives (Linux and Windows leave alignment checking off for user code; at worst a split
across a cache line costs time). A misaligned atomic (`LDXR`, `CAS`, a locked x86 access across a line) or an aligned
vector access would fault or trap, and GCC uses neither for a one-word copy. It is undefined in C. **Fix**: the six
locals are aligned to 4 under `TARGET_PC` (`PC_WORD_ALIGNED`, empty for the N64, on the declarations' own lines, so
`n64_neutral.py` sees the functions shifted, not changed). The full-UBSan run has no alignment site after it.

### 3.3 The retail paths under all of UBSan

The same game-C checks, on the Pi (GCC 12, Debug, UBSan only), with the fixes of 3.1 and 3.2, along 7 of the QA
sweep's press scripts: the front end's screens, its options rows, a 4-player race, battle, stunt and its end, the
pause screen's items, the attract demo. Each reached its time and exited 0. Besides the object-size sites the
synthetic paths reach, they reach 35 more in 11 files: all loads, each in a function bounds_check lists as reading
that symbol only, or at a constant offset (23 of them are `camera_scene_manager`'s reads of the other cars' HUD
records through `D_8014A250`, in the 4-player race). Nothing written, so nothing new to judge.

One more alignment site, `menu_control_settings`: `N64P(m->car)->TIRER[i]` takes the address of a car's tyre
positions in a `CarRec` the game keeps in the RDRAM window at a word-aligned N64 address, and the PC's `CarRec`,
whose host pointers (`input`, the visuals) make it 8-aligned, says that address can't hold one. Nothing is loaded
through the `CarRec` type there: the address goes to `func_8009E820`, which reads three floats through an `f32 *`.
Harmless on every host. Lowering the type's alignment (`packed, aligned(4)`, as transform's `MsgTarget`) would
change a type the data tables read (`D_80152818` is declared with it), so it isn't done the morning of the beta.


In `tools/port/sanitize/`, each with its reason; patterns are anchored to one function.

**ubsan.supp**
- `bounds:^audio_mixer_main$`: route_cross's `Tracker.idx` is declared with 17 slots and the records carry 20,
  which the N64 code fills (the file's own comment). A member array used past its end, as `-Warray-bounds` sees 260
  times in the game C; the writes stay inside `D_80151CE8` at its N64 layout, and GCC can't use the member's size
  to drop or move them here (the overrun reaches the next record, a different array index). Retyping it is decomp
  work.
- `bounds:^animControls$`: the pause menu's rows test `(conflict && !gConflict[p][action]) || action >= 10`, so
  with action 10 they read the next row's first flag, in the window at the N64's layout as the N64 reads it, and the
  `||` decides the result whatever was read.
- `null:^audio_helper$`: the blank cartridge only (`--no-rom`, `app_firstrun`): with every file empty no heap block
  fits, and the N64 code reads the null block's size, then faults, as on the N64. The tests expect that fault.
- `float-cast-overflow` in `func_8009F058`, `func_8009E8B4`, `func_8009D45C`, `func_8009D99C`, `func_800A557C`,
  `particle_system`, `input_deadzone_apply`, `display_update`: the synthetic cartridge's NaN positions (section 3).

**tsan.supp**: Mesa's (`libgallium`, `libLLVM`, `libEGL_mesa`), SDL's and PulseAudio's threads aren't instrumented,
so their own locking is invisible and every buffer they hand over looks like a race (hundreds of reports in the gfx
tests and the synthetic paths, each with the driver's frames on one side), and llvmpipe's JIT under EGL's display
lock looks like a lock-order inversion. A race between port/ code and the driver would be hidden too. After the fixes of
section 3, no report has port/ frames on both sides.

**lsan.supp**: `leak:libgallium`: Mesa keeps blocks to the end of the process that nothing of ours holds (the
frontend menu tests on the Pi's V3D driver).

**asan.supp**: empty.

## 5. CI

`.github/workflows/pc.yml`'s `sanitize` job: UBSan on `ubuntu-24.04` (x86-64) and ASan with UBSan on
`ubuntu-24.04-arm`, Debug, every test but `audio_realtime` and `input_live_drive`, which measure real time. The
synthetic paths are among the tests. It sets `vm.mmap_rnd_bits=28` first: GCC 13's runtimes predate 32 bits of
randomization, which can put a mapping inside their shadow. On the build host the UBSan build and its whole ctest
take under 3 minutes at -j12/-j3; on CI's 4 cores, about the Debug job's time again, less with its ccache (keyed
apart from the other jobs'). `app_firstrun` and `hfr_timing`, which start the game many times, get 1,200 s in a
sanitizer build. A finding fails the job.

TSan isn't in CI: it only runs on x86-64, half its reports are the driver's, and the scheduler's design (one N64
thread at a time) leaves it little to find. Run it after changing the threads, the audio device thread or the log.

## 6. What it doesn't check

- **TSan on the game C** (`RUSH_SANITIZE_GAME_THREAD`) is off by default: with the game's objects instrumented, the
  synthetic cartridge's boot faults at its first screen (`collision_sound_play`: `func_800B24EC`'s texture lookup
  comes back empty), which no other build does. Not understood yet; an instrumented game may run something in a
  different order. Until 2026-10-28 the option was silently off anyway: CMake drops a compile option that repeats an
  earlier one, so `-fsanitize=thread` for the game, after the port's identical flag and `-fno-sanitize=all`, never
  reached it (`SHELL:` keeps it now). The game's threads run one at a time; the races found were the port's.
- **MemorySanitizer** (uninitialized reads) is Clang-only and needs every library instrumented. ASan's stack and
  heap layout differ from Debug's, which is how an uninitialized read could show, as a run that diverges: the one
  retail path compared with Debug (`race4p`) gave the same states and car positions.
- **Inside the window** ASan sees one 8 MiB addressable block: the game's own heap and its arenas get no redzones,
  so an overflow from one game object into the next there is invisible to it (UBSan's `bounds` sees those it can
  from the declarations). That is the N64's memory model, which the port keeps on purpose.
- **x86-64 ASan** (section 1.1). The aarch64 runs cover the same C; what differs between the hosts is code under
  `#ifdef __x86_64__` (the fault handler's register, absfix's relocation type) and the C library.
- **Game C checks left out** (section 1): object-size, alignment, signed overflow. A UBSan build with
  `-DRUSH_SANITIZE_GAME_UB=undefined` turns them on; section 3.1 is the reading of what they find on the synthetic
  paths, and a new object-size site is read the same way (its symbol, the function's other places of it, whether
  bounds_check lists it). 7 retail paths have run with them (3.3), not all 31.
- **Windows**: MinGW has no sanitizers. The Windows-only code (win32.c, the WGL and console paths) is unchecked.
