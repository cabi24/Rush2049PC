# Compiler settings

Everything in the cartridge that was C was built by SGI's IDO compilers. This page records which compiler and
which flags reproduce each part, and why. The per-function and per-TU settings the build uses are in
`manifest/<image>.json` (game code, runtime images) and `src/rom/opt_overrides.mk` (static code).

## Compilers

| Code | Compiler | Where it is selected |
|---|---|---|
| libultra, libc, libgcc-style helpers (static `src/rom/lib_*.c` below 0x11050) | IDO 5.3 | Makefile default `CC` |
| Rush-specific boot-tail TUs `lib_11050` `lib_113e0` `lib_11640` `lib_15930` `lib_15a10` `lib_16320` `lib_17dc0` `lib_1a660` `lib_1cf90` `lib_1ece0` `lib_1f340` `lib_1f5b0` `lib_207b0` `lib_21210` `lib_22300` `lib_257f0` `lib_25bb0` `lib_26ab0` `lib_26f60` | IDO 7.1 | `opt_overrides.mk`: `CC := $(IDO71_CC)` |
| The inflate TU `lib_5610` | IDO 5.3 through `cc -S` and as1 `-O1` (below) | `opt_overrides.mk`: `CC := $(IDO53S_CC)` |
| Game code and runtime images A/B | IDO 5.3 | `tools/compile_single.sh`, `tools/compile_group.py` |

Both compilers are decompals' ido-static-recomp v1.2 builds (`make setup`). The recompiled binaries are x86-64
Linux programs that need 4 KB pages.

### IDO 7.1 in the boot tail

A narrow (`u8`/`u16`) parameter homed with `sw a0` and narrowed in place (`andi a0,a0,0xffff`) is IDO 7.1 code;
IDO 5.3 narrows into a temporary (`andi t6,a0,0xffff ... move a0,t6`). The in-place form appears throughout the
boot tail (0x80010450 onwards) and nowhere in libultra. Example: `void f(unsigned short id) { g(id); }` at
`-g0 -O2 -mips2` matches the ROM only with 7.1. The libultra TUs, compiled with 7.1, no longer match; the
boot-tail TUs that do not use the marker are byte-identical under both compilers. Flags are the same for both.

### IDO 5.3 `-S` recipe (inflate TU)

The inflate/lzss TU (0x80004A10..0x800069FC) was not built by `cc -c -O2`. It is IDO 5.3 `cc -S -O2` output with
every `.loc` line removed, assembled by as1 at `-O1` with `-g`. `tools/ido_sas/cc` does exactly this:

    cc -S <flags> FILE.c                       # same IDO 5.3, same flags (-g0 -O2 -mips2 -G 0 -non_shared)
    drop "\t.loc\t", "\t.option\tO<n>" and " #" comment lines
    cc -c <flags, -O1, -g> -o OUT FILE.s       # as0 + as1 at -O1, debug on

* **as1 `-O1`, not `-O2`.** Retail has only fill-from-above delay slots, never a branch-likely or a branch-target
  fill, and keeps the redundant reload after a store (`gInflateInPtr += 2; ... gInflateInPtr[-1]`) that as1 `-O2`
  deletes.
* **No `.loc`.** as1 treats each `.loc` as a scheduling barrier; with them the prologue `sw ra`/`sw s0` and the
  first global stores keep source order, without them as1 interleaves them as retail does.
* **`-g` on the assembly step.** Without it as1 `-O1` moves the `sltiu at` of a macro branch (`bgeu reg,imm`)
  into the delay slot of the branch before it (huft_build, inflate_dynamic); retail leaves a nop there.
* `cc -S` ignores `-o` (it writes `<basename>.s` into the working directory), so the driver runs it in a
  temporary directory with absolute `-I` paths.

Two routines in the same TU (`lzss_decode`, `inflate_io_wait`) and the two `inflate_entry` variants are
hand-written or hand-edited assembly and stay `GLOBAL_ASM`.

## Flags

Common to everything: `-mips2 -G 0 -non_shared`, plus `-Wab,-r4300_mul` (R4300 multiply workaround in as1) for
the game code and most static TUs. Debug and optimisation level vary per TU or per function:

| Level | Where |
|---|---|
| `-g0 -O2` | most of libultra (gu, io), libc; most game singles |
| `-g0 -O1` | libultra `os` functions and a few io TUs (`opt_overrides.mk` lists 19 TUs) |
| `-g1 -O1` | the SDK scheduler and the Rush frame-timing/boot code next to it (`lib_1050_sdk`, `lib_1050`, `lib_21f0`, `lib_2cf0`, `lib_3140`, `lib_3330`, `lib_3390`, `lib_34a0`) |
| `-g0 -O1 -mips3 -32` | ultralib `libc/ll.c` and `llcvt.c` (64-bit helpers, `lib_a5f0`, `lib_e4f0`); the object gets `EF_MIPS_32BITMODE` (`tools/o32abi.py`) so GNU ld links it beside the `-mips2` objects |
| `-g0 -O3` | game code and runtime-image functions whose callers or callees were optimised across files (below); a few singles |

At `-g1 -O1` every function ends in extra `jr ra; nop` pairs, so the function extents differ from `-g0` builds;
as1 does not move code across statements at that level (an empty statement changes delay-slot filling), and
`volatile` keeps `-O1` loads in program order.

## Whole-program `-O3` groups (interprocedural register allocation)

Many game functions read `t0`-`t3` or `s` registers on entry without writing them, and their callers load those
registers just before the `jal` (every caller of 0x80097384 does `move t0,zero; move t1,zero`). O32 passes
arguments 5-8 on the stack, so this is not the standard convention: it is IDO `-O3` interprocedural register
allocation, which gives a static function whose call sites are all visible a private calling convention, and lets
callers keep values live across calls to callees known not to clobber them.

It only reproduces when the functions are compiled together, as one program, with the linker told which
procedures stay external. The `cc` driver always passes `-preserve_dead_code` to `uld`, which keeps every global
external and defeats IPA, so `tools/compile_group.py` runs the stages by hand:

    cc -j <flags> <files>                     # ucode only, per file
    uld -L/usr/lib/mips2/nonshared -_SYSTYPE_SVR4 -mips2 -non_shared -g0 -no_AutoGnum -kp keep.txt <units> -ko linked
    usplit -mips2 -o split -t st linked
    umerge -Olimit 5000 -mips2 -EB -g0 -O3 split -o merged -t st
    uopt -G 0 -Olimit 5000 -mips2 -EB -g0 -O3 merged opt -t st optlog
    ugen -G 0 -mips2 -EB -g0 -O3 opt -o gen -t st -temp ugtmp
    as1 -elf -G 0 -p0 -mips2 -EB -g0 -O3 -r4300_mul -Olimit 5000 gen -o group.o -t st

`keep.txt` (the group's `keep` list) names the procedures that stay external; everything else takes part in IPA
and may be inlined. A group's `members` are the functions it supplies to the image; its `context` functions are
compiled with it (their presence shapes register allocation) but come from elsewhere. The build slices each
member out of the group object and relocates it to its own image address.

Behaviours that decide matches inside a group (each observed on several functions):

* `umerge` inlines a static callee by source size; a function that retail keeps out of line needs a stand-in
  caller in `keep` or a size above the threshold.
* `uopt` decides register conflicts per basic block; a block boundary that emits no code (`do {} while (0);`,
  `if (x) {}`, a compiled-out test) changes colouring.
* `as1` breaks some scheduling ties by source line, so the line on which a declaration or helper sits can matter.
* `uopt` never unrolls a 3-trip loop and folds a `-1` argument back to a constant.

## C89

IDO 5.3 is a C89 compiler: declare locals at the start of a block, no compound literals, no GNU inline assembly
(use `GLOBAL_ASM`), and use the project's own math declarations rather than a host `<math.h>`.
