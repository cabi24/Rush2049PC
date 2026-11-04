# Running with no fixed address (item 8)

Today the game's RDRAM window is mapped at host 0x80000000 (KSEG0) and 0xA0000000 (KSEG1), and a 32-bit N64 address
of game data is its host address ([ULTRA.md](ULTRA.md) 2, [DATA.md](DATA.md) 3). That costs: AddressSanitizer can't
run on x86-64 (its shadow starts at 0x7FFF8000, [SANITIZE.md](SANITIZE.md) 1.1), macOS arm64 can't map anything
below 4 GiB (`__PAGEZERO`), the program needs `MAP_FIXED_NOREPLACE` and `MapViewOfFileEx` at fixed places and a GOT
fix-up (`pc_mem_fix_absolute`), and arrays at their N64 addresses can't grow ([DECOUPLE.md](DECOUPLE.md)). The goal:
no fixed address at all, which gives macOS support and ASan on x86-64.

This is phase A (2026-11-04, on main 25cfd39): the design, decided; the inventory, as a tool; and a guard mode that
moves the window and reports every use of an N64 address as a host pointer. Nothing in the default build changes
(section 8). Phases B to D do the work (section 6).

| Piece | Where |
|---|---|
| The design | this file, sections 1-2 |
| The inventory | `tools/port/nofixed_audit.py` (section 3); its counts in section 4 |
| The guard mode | `RUSH2049_NOFIXED=guard`: `port/ultra/memory.c` (`guard_init`, `pc_mem_fault_note`, `guard_cut`), the fault handler `port/ultra/misc.c`, the loader `port/data/pc_data.c` (`pc_data_set_window`), `port/main.c`, `port/ultra/selftest.c` (section 5) |

## 1. What relies on the fixed window today

Measured on the build host (x86-64, GCC 14.3, binutils 2.41, Debug), 25cfd39 with this branch:

| What | How much | Where it comes from |
|---|---|---|
| Names at their N64 addresses | 1,748 arena names (boot 195, game 1,071, runtime image A 432, B 50; with the 2 end markers), 1 reserved (`pc_matrix_spill`), 5 RDRAM aliases (`osTvType`/`D_80000300`, `D_80000318`, `D_8017A640`, `D_8038A400`) | `data.c`'s absolute `.set`s (gen_data), `aliases.ld`'s `ABSOLUTE()` (gen_stubs) |
| Who reaches them by name | 780 of the 1,308 game objects (1,755 names); 3 PC-layer objects: `port/main.c` (18 names), `port/ultra/pools.c` (5), `port/ultra/selftest.c` (7) | nm -u |
| The GOT fix-up | aarch64, binutils 2.40: 1,802 GOT slots with RELATIVE relocations put back (ULTRA.md 2); x86-64, binutils 2.41: 6, the `aliases.ld` names' (measured: `--selftest -v -v`, `readelf -r`); there data.c's `.set` names get GOT slots that hold the bare value with no relocation | `port/ultra/absfix.c` |
| The fixed mappings | `map_at(0x80000000)`, `map_at(0xA0000000)` (memfd), `MapViewOfFileEx` twice; Linux and Windows stop without them | `port/ultra/memory.c` |
| "Below 4 GiB is an N64 address" | the renderer (`gfx_rsp.c` `is_host`), the audio interpreter (`musyx.c:112-117`), the loader's check that own storage is high, `lib_11640.c`'s `PC_ONLY` tests, the self-test | section 2.4 |
| Game C that converts between a pointer and a 32-bit integer | section 4 | the compiler, the sources |

The x86-64 count matters for the design: whether an assembler-absolute symbol's GOT slot in a PIE gets a relocation
depends on the toolchain (binutils 2.40 on aarch64 gives one, 2.41 on x86-64 doesn't), so a run-time rewrite of the
slots, the obvious way to move absolute names, would have nothing to find on the build host; on PE the `.refptr`
slots of absolute symbols get no base relocation either (WINDOWS.md 3), and on Mach-O dyld has nothing to rebase for
an absolute symbol (expected: macOS has never been built).

## 2. The design

### 2.1 The window: a section of the executable

Two ways to put the window at some host address W were weighed:

- **An anonymous mapping** (`mmap(NULL)`, `VirtualAlloc`) wherever the system puts it. The names would then have to be
  moved at run time: every code reference to an arena name goes through a slot the linker filled with the absolute
  value (an ELF GOT slot, a PE `.refptr` slot, a Mach-O non-lazy pointer), and the program would rewrite those slots.
  Section 1's measurement rules that out: only binutils 2.40's aarch64 leaves a relocation to find the slot by; on
  x86-64 with binutils 2.41 and on PE the slots carry none, and a code reference a file makes to its own definition
  is PC-relative, with no slot at all (WINDOWS.md 3's six tentative definitions).
- **A section of the executable**: a zero-fill section `.n64window`, `pc_window`, defined in `data.c`, holding the
  whole window, with every name a symbol inside it. The names are then ordinary relocatable symbols: each format's
  own loader moves them with the image (ELF RELATIVE relocations, PE base relocations, Mach-O rebases), there is
  nothing to fix up, and PC-relative references work because the window is in the image.

**Decided: the section, on every platform.** W = `pc_window`. Its properties:

- **Size**: `PC_RDRAM_MAX` (256 MiB), the largest `game.memory`; `game.memory` sets osMemSize and how much of it the game
  uses. On Linux and macOS a zero-fill section is demand-paged: untouched pages cost nothing, as the memfd's do today.
  On Windows a PE image's writable uninitialized data is expected to be charged to the commit limit when the image
  loads: 256 MiB on every start, where today `original` commits 8 MiB (MEMORY.md 1, "The cost"). Phase D measures it;
  if it is too much, the Windows build takes a smaller maximum (a build constant), not another mechanism.
- **Alignment**: 64 KiB (`.p2align 16`): a multiple of every page size it runs on (4 KiB x86-64, 16 KiB Apple and Pi
  5, 64 KiB) and of Windows' allocation granularity; the game's own alignments (8-byte data, 32-byte heap blocks)
  follow from it.
- **W is above 4 GiB on all three**: a Linux PIE loads at 0x55.../0xaaaa..., macOS arm64 maps nothing below 4 GiB
  (`__PAGEZERO`), and the PE image links at 0x140000000 with high-entropy ASLR. So the renderer's and the audio
  interpreter's rule (a value below 4 GiB is an N64 address, one above a host pointer) stays as it is.
- **No KSEG1 view.** K1 addresses are a translation like K0 ones (2.3); nothing maps 0xA0000000.
- **Measured**: a scratch build on the build host (not committed) with `pc_window` as a 256 MiB `.n64window`
  (`@nobits`, 64 KiB aligned) and every arena name `.set` relative to it linked and ran: `nm` shows the names as `B` at
  `pc_window` + offset, the GOT fix-up found 1 slot (the cartridge alias), and the loader filled the arenas there
  (section 5.3).

### 2.2 The names, relative to the window

gen_data emits each arena name as `.set D_x, pc_window + (a - 0x80000000)` in the same assembler block it uses
today, beside the `.globl` and `.size` it already has; gen_stubs emits each RDRAM alias as
`PROVIDE(x = pc_window + off)`; `reserved.txt`'s items get the same `.set`. Game C names them as before: no game C
change. Weighed against:

- **A linker script** (`D_x = ADDR(.n64window) + off;`): GNU ld does it for ELF and PE, but Mach-O's ld64 has no
  linker scripts, so it can't be the one form.
- **An assembler section with `.org`s** (`.org 0x12xxxx; D_x:`): labels in one section in address order. It is the
  same thing as a relative `.set` for the assembler, but needs the names sorted and can't express a name twice or a
  name inside another's extent (the 49 inside own storage are already `.set base + off`).

What each format allows:

| Format | The relative `.set` | References | What changes |
|---|---|---|---|
| ELF (Linux PIE, both arches) | a section-relative symbol (`B`/`b` in nm) | GOT slots with RELATIVE relocations, or PC-relative: GOT relaxation can come back | `absfix.c` and the x86-64 flags `-fPIC -Wa,-mrelax-relocations=no -Wl,--no-relax` (CMakeLists.txt:161-164) are no longer needed |
| PE (mingw-w64) | the same, in a `.bss`-class section | `.refptr` slots with base relocations; a file's own definitions PC-relative | WINDOWS.md 3's six `PC_ONLY(extern)` tentative definitions could go back; `weak.ld` unchanged |
| Mach-O (macOS arm64) | `D_x = _pc_window + off` is an `N_SECT` symbol; `N64_SYM` already adds the underscore | non-lazy pointers rebased by dyld, or ADRP+ADD | clang marks C files `.subsections_via_symbols`, which lets ld64 split the section into atoms at every name: each interior name needs `.alt_entry` (or the window in a `.s` of its own) so the offsets stay; the zero-fill section is `.zerofill __DATA,__n64window` |

`gen_data.py --check` then checks every arena name at `pc_window` + its offset and nothing absolute but the cartridge
names; the loader's tables give each arena's host as `pc_window + off`, and `at_n64_address` becomes "inside the
window".

### 2.3 The one translation

Every boundary between a 32-bit N64 address and a host pointer takes one translation, the one `pc_n64_ptr` and
`pc_n64_addr` already do (`memory.c`):

- **N64 to host**: K0 (0x80000000-0x9FFFFFFF), K1 (0xA0000000-0xBFFFFFFF) and physical (below the window's size) all
  to W + (a & 0x1FFFFFFF); 0 to NULL; past the window NULL (a cartridge address 0xB0000000 | off is past it at every
  size, MEMORY.md 2).
- **Host to N64**: a pointer into the window to 0x80000000 + (p - W) (K0); NULL to 0; anything else is no N64
  address (0 from `pc_n64_addr`; the boundary decides what that means: section 2.4).

In the fixed build W = 0x80000000 and both are the identity for window addresses, so every boundary that phase B
makes translate gives the same values there: game C changes can be proven on the fixed build (synth_compare the same
before and after) before the moved one runs. For speed, the prelude gets inline forms against an exported
`pc_window_base` (one load, a mask, an add, a compare); the function call stays for the PC layer.

### 2.4 Every boundary, and what it takes

| Boundary | Today | Phase B |
|---|---|---|
| `N64P(x)`: a 4-byte N64 address field (`enum n64p_X`, 130 declarations in 43 files) read as a pointer: 229 uses in 42 files | 48 PC definitions: 42 `_Generic` casts and 5 plain casts, all zero-extending `(T *)(uintptr_t)(x)`, and 1 translator (`pc_voice_node`, lib_1f5b0.c); 48 N64 sides `x` | the 47 casts take the inline N64-to-host translation (`(T *)pc_n64p(x)`); `pc_voice_node` already translates; the uses don't change |
| `N64W(x)`, `PC_N64W(x)`: a pointer stored in such a field: 92 + 3 uses | 41 PC definitions are `PC_N64W(x)`, the prelude's `(unsigned int)(uintptr_t)(x)`; 1 translator (`pc_voice_node_address`) | `PC_N64W` becomes host to N64, and a non-NULL pointer outside the window aborts with its line (`RUSH_CHECK_CASTS`'s check, on in every moved build) |
| Explicit casts of another size (`(u32)p`, `(T *)u32`) | 111 int-to-pointer (23 live, 88 in weak copies), 206 pointer-to-int (103 live, 103 weak) | each live one read: an N64-world address (the heap's arithmetic, a converted file's relocation, a song's offsets) takes the translation through a file macro whose N64 side is the original tokens (`#define N64A(x) (u32)x`); a small integer, an id, an ignored argument (CASTS.md 5.1) stays. Weak copies are never linked: left |
| Implicit conversions (`-Wint-conversion`) | 60 (49 live, 11 weak) in 29 files | the same, live ones |
| Silent: through `uintptr_t` to a narrower type | 5 (`FONT_SLOT`, `PACKET`, zlib's `audio_reverb_update`, `DMA_SOURCE`, lib_11640.c's `PC_ONLY`) | read each |
| Silent: a pointer from an integer through `uintptr_t` | 72 in 33 files (not counting the N64P definitions; `func_800E313C.c`'s `M2C_N64P` among them) | read each: most hold a pointer already |
| Literal addresses in live code (section 4.2) | 14 | 2 to change: `PrevMaxPath((u8 *)0x8038A400, ..., (u8 *)0x803BB380, ...)` in `heap_release/overlay_loader.c:78, 82` (and its 3 weak copies); the rest are N64 values (`h->end = D_80000318 \| 0x80000000`, `display_list_traverse`'s K0 that `DL_AT` translates) |
| `osVirtualToPhysical`, `rcpaddr.c` | `pc_n64_addr` of the pointer, then the N64's rule; pure N64-address arithmetic | nothing |
| The heap (blocks, handles) | the allocator works in 32-bit N64 addresses: its headers keep them (`enum n64p_Block`...), its start is `(Heap *)(((u32)D_8017A640 + 31) & ~31)` (`audio_heap/group.c:310`), its API passes `(u32)p` (POINTERS.md 4.1); handle slots are 8 bytes and hold host pointers (9.10) | the start and the `(u32)p`s translate (the explicit-cast rows); headers stay N64 addresses through N64P/N64W; handle slots stay host pointers |
| The loader (`pc_data.c`) | arenas at their N64 addresses (`host == n64_lo`), heap values kept as they are | done in phase A behind `pc_data_set_window`: an arena is filled at the window's place for it and a kept RDRAM value is translated; phase B gives the tables `pc_window + off` and drops the hook |
| port/assets' converted files (u32 fields holding addresses) | the game relocates in place with `+= (u32)base` (SetupWaveModel, MBSetupWad, LoadTrack); readers zero-extend (`WADP`, N64P) | `(u32)base` is the block's N64 address (explicit-cast rows: `MBSetupWad.c:33, 36`, `waves.c`); readers translate (N64P rows); `port/assets/world.c:223` and `:239` read the GTLD keys through an N64 address taken as a pointer: `pc_n64_ptr` |
| The renderer, the audio interpreter | below 4 GiB: `pc_n64_ptr`; above: a host pointer (`gfx_rsp.c:45-90`, `musyx.c:105-117`) | nothing: W is above 4 GiB (2.1) |
| PI DMA, inflate, LZSS (`pi.c`) | take the game's host pointer; inflate's room from `pc_n64_addr(dst)` | nothing |
| The runtime images' arenas in the heap | the tables' hosts 0x80393B34, 0x803AF978 in the heap block at 0x8038A400 | `pc_window + off`, as every arena |
| `reserved.txt` | `pc_matrix_spill` absolute at 0x8014A250 | relative, as the arenas |
| The mods' region (`pi.c` `mods_at`) | cartridge offsets 0x04000000-0x0FC00000 | nothing: cartridge space, not RAM |
| The controller pak and the saves | the PIF and pak files (`si.c`, `pakfs.c`, `pakstore.c`) see bytes; the game's Pfs C keeps its state in the boot arena by name; the save records hold host pointers (POINTERS.md 9.10) | nothing beyond the names (2.2) |
| The StatLL lists' N64-flagged links (`pc_statll_link`, `pc_statll_set_link`, `pc_prelude.h:149-157`) | zero-extend and truncate | translate |
| The PC layer's by-name references (`main.c`, `pools.c`, `selftest.c`) | absolute names through the GOT | nothing (2.2); the self-test's "at its N64 address" checks become "at `pc_window` + offset" |
| The test programs' own windows | 19 `MAP_FIXED`/fix-up sites in 13 test files and 116 literal window addresses in 12 (`support.c`, `wad_game.c`, `test_pc_data.c`, `test_musyx.c`, the gfx and audio tests: `port.*.tests`, section 4.1), and `rush2049pc_datacheck`'s window (`datacheck.c:93-94`) | phase C: each test's window and tables at a host address of its own |

### 2.5 The truncations the compiler doesn't flag, and how to find them all

`(u32)(uintptr_t)p` and `(s32)(intptr_t)p` compile silently, so do s32 arithmetic on an address held as an integer,
an address stored through a union or a byte array, and a `memcpy` of a pointer into a 4-byte field. Static lists
bound them, and the guard catches what runs:

- **Statically**: the casts through a pointer-sized integer (`src.trunc`, `src.zext`, `port.trunc`, `port.zext` in the
  audit); the implicit conversions (`-Wint-conversion`, which the build hides); integer declarations of data other
  files declare as pointers (gen_data's "Pointer conflicts", 106 symbols, DATA.md "Conflicts"); u32 fields holding
  addresses in ROM data (the loader's `addr-word` lines, DATA.md 4); unions and byte views over addresses
  (`data_layout.txt`'s "Unions copied as bytes" and "Arena element conflicts", `endian_check`'s puns); `intptr_t`
  storage that receives an `(s32)` (POINTERS.md 5, "Widening N64-world storage").
- **At run time, the guard** (section 5): an N64 address taken as a host pointer faults in the reserved
  0x80000000-0xBFFFFFFF; a window pointer cut to 32 bits faults in the truncation trap below 4 GiB, or is caught when
  it reaches `pc_n64_ptr` (the renderer and the audio take such values without dereferencing them, so a fault alone
  would miss them); an N64 address sign-extended from an s32 faults at 0xFFFFFFFF8xxxxxxx, a kernel address, and the
  note says so (not seen yet: no run has made one).
  `RUSH_CHECK_CASTS` (on in moved builds) stops a host pointer stored through `PC_N64W`. The guard is complete for
  what runs, so the proof is coverage: every synthetic path and the retail scripts (phase C).

## 3. The inventory: `tools/port/nofixed_audit.py`

```
python3 tools/port/nofixed_audit.py --build build/pc [--jobs 8] [--out build/nofixed_audit.txt] [--json FILE]
    [--sites CLASS,...|all] [--baseline FILE] [--write-baseline FILE]
```

Stdlib Python; 21 s on the build host with the compiler pass (1,308 files at -j8) and nm, 16 s on the Pi 5 for the
sources alone (no `--build`). Three sources:

1. **The compiler**: every game C file in `compile_commands.json` (those built with `TARGET_PC`) compiled again with
   `-fsyntax-only`, `-Wno-int-conversion` taken out and `-Wint-conversion -Wint-to-pointer-cast -Wpointer-to-int-cast`
   on. Each warning is placed at its use site (the outermost "in expansion of macro" note) and marked `.weak` when
   its function is in the file's `build/pc/weak/*.h` (a weak copy: never linked). `--warnings LOG` parses a build log
   instead. The raw counts are kept too (`context.cc_warnings.*`), and they are the measured ones: 111, 206, 60.
2. **The sources**, with comments and strings told from code and the lines only the N64 build compiles (inside
   `#ifndef TARGET_PC`, or the `#else` of `#ifdef TARGET_PC`) marked `.n64only`; with `--build`, code in a `.c` the PC
   doesn't compile (the libultra units port/ultra replaces) is `.unbuilt`.
3. **The tables** (`--build`): `pc-data/data_layout.json`, `pc-data/aliases.ld`, and nm over the game's objects and
   the PC layer's.

The report lists the counts by class, then the sites (`file:line: text`) of the classes `--sites` names (default:
the compiler's, the sources', the PC layer's). `--baseline` fails when a class's count grew past the file's, so a CI
job can hold the line while phase B shrinks them (no job is added in phase A).

## 4. The counts (25cfd39 with this branch, build host, Debug)

### 4.1 By class

| Class | Sites | Files | What |
|---|---:|---:|---|
| `symbols.arena.{boot,game,ovl_a,ovl_b}` | 195, 1,071, 432, 50 | | arena names at their N64 addresses (1,748) |
| `symbols.reserved`, `symbols.aliases.rdram`, `.cartridge` | 1, 5, 1 | | |
| `symbols.by_name.game_objects`, `.port_objects` | 780, 3 | | objects reaching them by name (1,755 and 28 names) |
| `symbols.inside_own`, `.own`, `.decoupled` | 49, 390, 38 | | context: not window addresses |
| `cc.int_to_ptr` (`.weak`) | 23 (88) | 10 (17) | casts to a pointer from an integer of another size |
| `cc.ptr_to_int` (`.weak`) | 103 (103) | 35 (22) | casts to an integer of another size from a pointer |
| `cc.implicit.int_to_ptr` (`.weak`) | 32 (7) | 16 (4) | an integer used as a pointer without a cast |
| `cc.implicit.ptr_to_int` (`.weak`) | 17 (4) | 9 (3) | a pointer used as an integer without a cast |
| `src.n64p`, `src.n64w`, `src.pc_n64w` | 229, 92, 3 | 42, 19, 3 | the uses |
| `src.def.n64p.{generic,cast,function,n64}` | 42, 5, 1, 48 | | N64P's definitions |
| `src.def.n64w.{pc_n64w,function,n64}` | 41, 1, 42 | | N64W's definitions |
| `src.n64p_field` | 130 | 43 | `enum n64p_X` fields |
| `src.trunc`, `src.zext` | 5, 72 | 5, 33 | silent conversions |
| `src.lit.*` | section 4.2 | | |
| `port.mapping`, `.rdram_base`, `.host_test`, `.translate`, `.trunc`, `.zext`, `.lit` | 26, 27, 15, 37, 11, 20, 52 | 7, 8, 7, 9, 4, 9, 4 | the PC layer's programs (the guard mode's own lines in `memory.c` among them); `.tests`, the test programs: 19, 10, 2, 55, 20, 47, 116 |

### 4.2 The 882 lines with a literal 0x80xxxxxx (src/, nonmatching/)

`grep -E '0x80[0-9A-Fa-f]{6}'` over every file of `src/` and `nonmatching/` gives 882 lines in 414 files. The audit
classifies each:

| Class | Lines | What |
|---|---:|---|
| `src.lit.comment` | 775 | in a comment: function headers' addresses, notes |
| `src.lit.annotation` | 34 | a comment beside a declaration: `static s8 lastSpacing; /* 0x8015419C */`, `extern ... gWaveInstance[]; /* 0x80156D38 */`, a deleted stub's prototype |
| `src.lit.metadata` | 9 | `group.json` notes |
| `src.lit.code.bit` | 30 (+3 `.n64only`, 1 `.unbuilt`) | 0x80000000 as a bit: scene-node and blit flags (`D_8012E700[...].flags ^= 0x80000000`), the MusyX ids and states, IDO's float-to-unsigned (`(s32)(f - 2.1474836e9f) \| 0x80000000`), MBSetupWad's "relocated already" test |
| `src.lit.code.region` | 2 | a test of an N64 address's top bits: `lib_11640.c:2030`, `lib_25bb0.c:486` (MEMORY.md 2) |
| `src.lit.code.address` | 14 (+7 `.n64only`, 7 `.unbuilt`) | the overlay loaders' `(u8 *)0x8038A400` and `0x803BB380` (8 lines in 4 files, 2 of them live), `display_list_traverse.c:36, 55` (a K0 address `DL_AT` translates), `func_800966D8.c:3808` (`pc_n64_ptr` already), `audio_heap/group.c:312-313` (the heap's end as an N64 value), `rom_tu.h:319` (`K0BASE`). The `.n64only` ones are m2c's lui halves whose PC versions are symbol-relative (POINTERS.md 7); the `.unbuilt` ones libultra's exception vectors and `osPhysicalToVirtual` |

The literal K1 and cartridge values in code are counted apart: 70 hardware registers (`0xA4xxxxxx`, 62 of them in
units the PC doesn't compile), 6 K1/cartridge (`func_8010FD60.c:11`'s `| 0xB0000000`, the cartridge address, stays).

### 4.3 The 512 N64P/N64W lines

330 lines have `N64P(` and 182 `N64W(` (`grep -F`, which counts `PC_N64W(` and `M2C_N64P(` too). The audit's split:

| | Uses | PC definitions | N64 definitions | Other |
|---|---:|---:|---:|---|
| `N64P(`, 330 lines | 229 | 48 (42 `_Generic`, 5 casts, 1 translator) | 48 | 3 are `func_800E313C.c`'s `M2C_N64P` (a zero-extending read of a u32 field), 2 in comments |
| `N64W(`, 182 lines | 92 | 42 (41 `PC_N64W(x)`, 1 translator) | 42 | 3 `PC_N64W` uses outside them (`func_8010DAF8.c:816`, `FOG_TABLE`, `PAL_BASE`), 1 on an N64-only line (`level_objects/group.c:200`), 2 in comments |

Every one is a place that translates already, by name, with a zero-extension or a truncation as its PC definition.
Phase B changes the 48 and 42 PC definitions and the prelude's `PC_N64W`, not the 321 uses (and `M2C_N64P` with the
silent sites).

## 5. The guard mode: `RUSH2049_NOFIXED=guard`

Off by default; Linux (any POSIX that has `MAP_FIXED_NOREPLACE`); on Windows the variable says it is ignored.
`pc_mem_init` then:

- reserves 0x80000000-0xBFFFFFFF with no access (the old window, KSEG0 and KSEG1), so an N64 address taken as a host
  pointer faults at once;
- maps the window anonymously at 0x5540000000 (or 0x6640000000, 0x4440000000, 0x7740000000, else wherever: all below
  2^39, which every 64-bit Linux address space reaches), and reserves its low 32 bits' range (0x40000000) with no access
  too, the **truncation trap**: a window pointer cut to 32 bits faults there;
- maps no KSEG1 view: `pc_n64_ptr` translates K0, K1 and physical addresses alike, and in this mode it aborts with a
  report when it is handed a value in the trap's range (a cut window pointer handed to the renderer or the audio);
- `main` hands the loader the translation (`pc_data_set_window(pc_n64_ptr)`, also for a host that can't map
  0x80000000), so the arenas are filled at the window's place for them.

The fault handler adds one line when the faulting address is one of those, with the data name it was:

    rush2049pc: Segmentation fault at address 0x8002e580 in N64 thread main
    rush2049pc: guard: an N64 KSEG0 address, 0x8002E580: D_8002E580+0x0, used as a host pointer; the window has 0x8002E580 at 0x554002e580 (docs/port/NOFIXED.md 5)

### 5.1 What it reaches, and where it stops

Build host, the branch's Debug build, the synthetic cartridge (`--no-rom-check --headless --no-config --audio null
--no-pak --fast`):

| Run | Reaches | Stops at, and why |
|---|---|---|
| the program's start | the options and settings, the ROM and its check, the mods' preparation, the window, the fault handler | |
| `--selftest` | all 28 checks it runs pass: the threads, queues, events, timers, busy-waits, the window (K0, K1, physical through the translation, `osVirtualToPhysical`, `pc_rcp_reloc`), the absolute names' addresses and the low-memory words through the window, the RCP and AI checks, and `guard: window elsewhere` | the controller and pak checks are skipped in this mode (a line says so): without the skip `osContInit` (game C, `src/rom/lib_a050.c:20`) faults at 0x8002C3B0, boot data it reaches by name |
| the synthetic cartridge | `pc_os_initialize` (the low-memory globals through `pc_n64_ptr`), the loader: all four images, every arena filled in the moved window, its summary line the same as the fixed run's | `mods_install` (`port/mods/mods.c:1173`, from `port/main.c:571`) reads `gWaveSizes` (`D_8002E580`, boot data) by name. The arena names are absolute: everything after this that touches game data by name faults the same way, which is what phase B's relative names remove |

### 5.2 The unit tests under the guard

Not measured yet: `RUSH2049_NOFIXED=guard ctest --test-dir build/pc -LE realtime` was queued behind the section 8
runs when this was written. Programs that map their own window (the test programs of section 2.4's last row) don't
read the variable; programs that use port/ultra's window and touch game data by name stop as section 5.1 says.

### 5.3 With the names relative (a scratch experiment)

To see what phase B meets after the names, a scratch copy on the build host (not committed) had gen_data and
gen_stubs emit the names relative to a `pc_window` section and the guard take that section as its window:

| Step | Reaches | Stops at |
|---|---|---|
| names relative | the loader (the same summary line), the renderer's start, the boot thread, `game_init`, `game_late_init` | `func_800E7D04` (`audio_heap/group.c:310-312`): the heap's start `(Heap *)(((u32)D_8017A640 + 31) & ~31)` cuts the window pointer to 32 bits; the fault is in what would be the trap's range (the section's low 32 bits were 0xFAE4A640: no trap there, the note names it "a window address cut to 32 bits") |
| and N64P/N64W translating, that line through `pc_n64_addr` | the heap's start | `func_800E7B44` (`audio_heap/group.c:215`): `heap->first = N64W((Block *)(((u32)heap + 67) & ~31))`, the next `cc.ptr_to_int` of section 4 |

So the inventory's live casts are the work order, and the heap's (`audio_heap`, `heap_release`, `alloc_at`,
`func_80095EF4`, the frees in the save code) come first.

## 6. The phases after this one

| Phase | What changes | How it is proven | Size |
|---|---|---|---|
| **B**: relative names, the translation, the compile-time sites; the synthetic cartridge boots moved | a build option `RUSH_NOFIXED` (off): gen_data, gen_stubs and `reserved.txt` as section 2.2, `data.c`'s `.n64window`, the tables' `pc_window + off`, `gen_data --check`, memory.c's window = the section (with the guard's reservation), no fix-up; the prelude's inline translations, the 48 N64P and 42 N64W PC definitions, `PC_N64W` checked; the live compile-time sites of section 4 (126 explicit casts, 49 implicit, 77 silent to read, 2 literal pointers), `pc_statll_link`, `world.c`; the self-test's checks | the fixed build: `synth_compare --same --runs 2` before against after, every synthetic path (the translation is the identity there), ctest Debug and RelWithDebInfo, n64_neutral, the IDO check, the Windows cross build; the moved build: the self-test, the loader's summary line the same, `drive` and `race2p` the same as the fixed build (`synth_compare --exe fixed --exe moved`), the guard on; `nofixed_audit --baseline` in CI | about 250 sites to read, 150-200 to change, in some 60 game files and 10 PC files; 2-3 sessions |
| **C**: every synthetic path and the retail scripts the same moved as fixed; an x86-64 ASan CI job | the run-time sites the guard finds along every path (data through memory, ROM-driven paths); the test programs' own windows (section 2.4's last row); `cmake/sanitize.cmake` allowing `address` on x86-64 with `RUSH_NOFIXED` | `synth_compare --exe fixed --exe moved --same --runs 2` on all paths (56 today), the six retail scripts on the Pi the same; ctest moved; the new CI job (ubuntu-24.04: ASan + UBSan, `RUSH_NOFIXED`, every test the arm job runs) | unknown until the guard has run: CASTS.md found 8 such bugs in 331 sites; 2-4 sessions |
| **D**: macOS CI; Windows moved; the default switched | macOS arm64: Mach-O's `.alt_entry`/`.zerofill` (2.2), thunks and weak references, the fault handler's Darwin `ucontext`, no memfd; a GitHub `macos-14` job (build, unit tests, synthetic paths); Windows: the section in the PE, its commit charge measured (2.1); the default flipped to moved when every check of B and C says the same on every platform; then the fixed mode retired (`absfix.c`, `MAP_FIXED`, the `ABSOLUTE` aliases, the x86-64 flags) | the CI jobs; synth_compare fixed against moved on Linux, Windows and macOS; the retail scripts on the Pi | 2-3 sessions, more if the macOS build finds more (it has never been built) |

## 7. What stays as it is, and why

- **The default build, until phase D.** It is the proven one; the moved build earns the default by being the same.
- **N64 addresses inside the N64 world.** The N64-layout records (the heap's headers, the WAD records, the voices'
  macro pointers, the 130 `enum n64p_` fields) keep 32-bit N64 addresses: that is their layout, ROM data holds them, and
  they translate only where they become pointers. "No fixed address" means no fixed *host* address.
- **The cartridge's names.** `D_00039B40` is `ABSOLUTE(0xB0039B40)`, a cartridge address the PI and MusyX callbacks
  take as a number; it is no RAM address and stays absolute.
- **The 4 GiB rule** (a value below 4 GiB is an N64 address). It holds with the window in the image (2.1); it needs the
  executable PIE and host data high, as today.
- **Windows' low reservations** (`reserve_low`): they keep host data above 4 GiB, which the rule still needs.

## 8. Proof that the default build is unchanged

| Check | Result |
|---|---|
| `synth_compare.py --same --runs 2`, a build of 25cfd39 against the branch, Debug, build host (x86-64, GCC 14.3), every synthetic path | 56 of 56 the same (twice: on the guard's first form and on the final branch) |
| ctest, build host: Debug and RelWithDebInfo | 353 of 353 each on the merge into main (c6ece2d with this branch; `assets_rom` skipped: no ROM there). Three runs of the self-test failed once on the branch while the host was loaded: the self-test's own virtual-clock check, fixed on main in c6ece2d |
| `data.c`, `data_symbols.txt` | the same files as 25cfd39's, and Debug's the same as RelWithDebInfo's |
| `n64_neutral.py --base 09da2f0` | passes: 1,027 files, 90 code-exact, 937 shifted (no file under `src/`, `include/` or `nonmatching/` changed) |
| Windows cross build (Pi, mingw-w64, the lock) | 3,497 of 3,497 targets; gen_data's PE check OK |
| The IDO check | not needed: no file under `src/` or `include/` changed |
