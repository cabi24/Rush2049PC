#!/usr/bin/env python3
"""window_census.py on a made-up tree: each kind counts what its pattern is for and leaves its look-alikes, every kind
is reported in its place (`-` where the source isn't there), and --baseline fails when a count grows.

    python3 tools/port/tests/test_window_census.py

Exit status 0 when every check passes. Stdlib only (the made-up build has no objects, so no nm and no compiler)."""
import contextlib
import io
import json
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import window_census  # noqa: E402

FILES = {
    # The game C: one site of each pattern, and the look-alikes the census must leave.
    "src/blob/heap.c": """\
#include "game/memory.h"
#define N64P(x) _Generic((x), enum ChunkLink: (tMemoryChunk *)PC_N64P(x), \\
    default: (x))
#define LINK(p) PC_N64W(p)
#define RUNTIME_IMAGE_START 0x8038A400
enum ChunkLink { ChunkLink_ };
typedef struct Chunk {
    enum ChunkLink next;    /* the next chunk: a window address */
    u32 owner;              /* its owner, a window address */
    u32 flags;              /* not an address */
    u32 texBase;            /* a loaded file's address field, by its name */
    u32 texCount;
} Chunk;
extern u8 game_bss_end[];
void walk(Chunk *c)
{
    Chunk *n = N64P(c->next);              /* a file macro's use */
    c->next = LINK(n);                     /* another */
    c->owner = PC_N64A(n);                 /* the prelude's, directly */
    if (c->flags & 0x80000000) {           /* a flag bit: context */
        c->flags ^= 0x80000000;
    }
    n = (Chunk *)(uintptr_t)c->owner;      /* silent: a pointer from an integer */
    DeallocateMemory(N64A(n));             /* the header's macro */
    allocate_memory_at(RUNTIME_IMAGE_START, 0x100);
    n = (Chunk *)(((u32)game_bss_end + 31) & ~31);
    if ((c->owner & 0xF0000000) != 0x80000000) {   /* a region test */
        c->owner &= 0x1FFFFFFF;                     /* a mask */
    }
    c->flags = pc_be32_get(n);             /* big-endian data */
    c->flags = PC_RGBA(c->flags);
    pc_n64_ptr(c->owner);
    /* PC_N64P(x) in a comment is no use; nor is "PC_N64W(p)" in a string: */
    puts("PC_N64W(p)");
}
""",
    "include/game/memory.h": """\
#define N64A(x) PC_N64A(x)
void DeallocateMemory(u32 address);
void *allocate_memory_at(u32 addr, u32 size);
void *pc_n64_ptr(unsigned int a);
s32 CountThings(s32 count);
""",
    "src/rom/regs.h": "#define SP_STATUS_REG (*(vu32 *)0xA4040010)\nu32 cart = 0xB0039B40;\n",
    # The PC layer: programs and a test program.
    "port/include/pc_n64_map.h": "#define N64_KSEG0 0x80000000u\n#define N64_OVERLAY_BASE 0x8038A400u\n",
    "port/include/pc_prelude.h": "#define PC_N64P(x) ((uintptr_t)(unsigned int)(x))\n",
    "port/ultra/memory.c": """\
static int fixed(void) { return mmap((void *)PC_RDRAM_BASE, 8, 0, MAP_FIXED, -1, 0) != 0; }
void *pc_n64_ptr(uint32_t a) { return (void *)(uintptr_t)(a + N64_KSEG0); }
int high(void *p) { return (uintptr_t)p >> 32 != 0; }
uint32_t low_memory = 0x80000300;
void reg(void *p) { pc_mem_register_be(p, 4); }
""",
    "port/gfx/gfx_rsp.c": """\
static int is_host(uintptr_t a) { return a > 0xFFFFFFFFu; }
static const uint8_t *rdram(uint32_t phys);
const uint8_t *res(uint32_t a) { return is_host(a) ? 0 : rdram(a); }
""",
    "port/ultra/tests/test_window.c": "void map(void) { mmap((void *)0x80000000, 8, 0, MAP_FIXED, -1, 0); }\n",
}

LAYOUT = {
    "arenas": [[0x800277D0, 0x80086A50], [0x8010FD7C, 0x8017A640], [0x803AF978, 0x803BAE20],
               [0x80393B34, 0x8039B440]],
    "absolute": [["boot_table", 0x80028000], ["game_flag", 0x80110000], ["game_list", 0x80120000],
                 ["ovl_a_text", 0x803B0000]],
    "sets": [["inner_label", "own_record", 8]],
    "own": [["own_record", 16], ["own_ptrs", 64]],
    "decoupled": [["pc_links", 104, ["gLink"]]],
    "reserved": [["pc_matrix_spill", 0x8014A250, 6144]],
}
ALIASES = """\
PROVIDE(sample_bank_rom = ABSOLUTE(0xB0039B40));
PROVIDE(game_bss_end = ABSOLUTE(0x8017A640));
PROVIDE(osMemSize = ABSOLUTE(0x80000318));
"""
REPORT = """\
# pc_data report: a comment line
addr-word gFoo+0x4 0x80120000 -> game_list [arena]
unresolved gBar+0x0 0x80130000
undescribed gBaz+0x8 0x80121000 -> 0x80121000
undescribed gBaz+0xC 0x80121010 -> 0x80121010 (text)
reached tTable 0x80140000 3
"""

# kind -> the count the made-up tree must give
EXPECT = {
    "data.arena.boot": 1, "data.arena.game": 2, "data.arena.ovl_a": 1, "data.arena.ovl_b": 0,
    "data.inside_own": 1, "data.reserved": 1, "data.window_aliases": 2,
    "conv.PC_N64P.game": 0, "conv.PC_N64A.game": 1, "conv.PC_N64W.game": 0, "conv.PC_SLOT.game": 0,
    "conv.prelude.port": 0,
    "conv.file_macros.defs": 3,    # N64P (two lines, one definition), LINK, the header's N64A
    "conv.file_macros.uses": 3,    # N64P(c->next), LINK(n), N64A(n)
    "conv.pc_n64_ptr.game": 1,     # the call, not the header's prototype
    "conv.pc_n64_ptr.port": 0,     # memory.c defines it: no call
    "conv.silent.zext": 1,
    "fields.link_enum": 1, "fields.link_enum_types": 1, "fields.u32_address": 2,
    "api.u32_address": 2,          # DeallocateMemory and allocate_memory_at; not CountThings
    "lit.address.game": 1,         # RUNTIME_IMAGE_START's definition
    "lit.region.game": 1, "lit.k1.game": 1, "lit.hw_register.game": 1, "lit.mask.game": 1,
    "context.lit_bit.game": 2,
    "heap.window_symbols": 1,      # the use, not the extern
    "heap.allocate_at": 1,         # the call, not the prototype
    "ovl.image_block": 2,          # the #define (lit.address's too) and the use
    "n64map.uses.port": 1, "port.rdram_base": 1, "port.mapping": 1, "lit.window.port": 1,
    "port.below_4g": 3,            # memory.c's >> 32, is_host's definition and its use
    "port.n64_reads": 2,           # rdram's declaration and its use
    "tests.window": 1,
    "be.accessors.game": 1, "be.rgba.game": 1, "be.ranges": 1,
    "text.window.game": 2,         # the two comments about window addresses
    "runtime.addr_word": 1, "runtime.unresolved": 1, "runtime.undescribed": 2, "runtime.reached": 1,
    "context.own": 2, "context.decoupled": 1, "context.cartridge_aliases": 1, "context.n64map.defs": 2,
}
UNMEASURED = {"data.by_name.game", "data.by_name.port", "conv.cc.ptr_to_int", "conv.cc.int_to_ptr",
              "conv.cc.implicit"}


def run(args):
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        code = window_census.main(args)
    return code, out.getvalue()


def main():
    failures = []

    def check(ok, what):
        if not ok:
            failures.append(what)

    with tempfile.TemporaryDirectory() as tmp:
        root, build = Path(tmp) / "tree", Path(tmp) / "build"
        for rel, text in FILES.items():
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            (root / rel).write_text(text)
        (build / "pc-data").mkdir(parents=True)
        (build / "pc-data" / "data_layout.json").write_text(json.dumps(LAYOUT))
        (build / "pc-data" / "aliases.ld").write_text(ALIASES)
        (build / "data_report.txt").write_text(REPORT)
        js = Path(tmp) / "census.json"
        code, text = run(["--root", str(root), "--build", str(build), "--no-compile", "--json", str(js)])
        check(code == 0, f"exit status {code}")
        counts = json.loads(js.read_text())["counts"]
        for kind, want in EXPECT.items():
            check(counts.get(kind) == want, f"{kind}: {counts.get(kind)}, expected {want}")
        for kind in UNMEASURED:
            check(kind not in counts, f"{kind}: measured ({counts.get(kind)}) with no objects or compiler pass")
        # Every kind, once, in the census's order, with `-` for the unmeasured.
        lines = [ln for ln in text.splitlines() if not ln.startswith("#")]
        check([ln.split()[0] for ln in lines] == window_census.KIND_NAMES,
              "the kinds' lines are not the kinds in order")
        for ln in lines:
            if ln.split()[0] in UNMEASURED:
                check(ln.split()[1] == "-", f"unmeasured kind without '-': {ln}")
        # Without a build: the tables' kinds are unmeasured, the sources' the same.
        code, text = run(["--root", str(root), "--json", str(js)])
        bare = json.loads(js.read_text())["counts"]
        check("data.arena.game" not in bare, "data.arena.game measured without a build")
        check(bare.get("conv.file_macros.uses") == EXPECT["conv.file_macros.uses"], "sources counted differently")
        # --baseline: the same counts pass, one more site fails, a context kind growing doesn't.
        base = Path(tmp) / "base.json"
        run(["--root", str(root), "--build", str(build), "--no-compile", "--write-baseline", str(base)])
        code, _ = run(["--root", str(root), "--build", str(build), "--no-compile", "--baseline", str(base)])
        check(code == 0, "--baseline fails on the same tree")
        with (root / "src/blob/heap.c").open("a") as f:
            f.write("void more(Chunk *c) { c->owner = PC_N64A(c); }\n")
        code, _ = run(["--root", str(root), "--build", str(build), "--no-compile", "--baseline", str(base)])
        check(code == 1, "--baseline passes when conv.PC_N64A.game grew")
        b = json.loads(base.read_text())
        b["conv.PC_N64A.game"] += 1
        b["context.own"] -= 1
        base.write_text(json.dumps(b))
        code, _ = run(["--root", str(root), "--build", str(build), "--no-compile", "--baseline", str(base)])
        check(code == 0, "--baseline fails on a context kind")
        # --sites lists a kind's sites with their lines.
        code, text = run(["--root", str(root), "--sites", "heap.allocate_at"])
        check("src/blob/heap.c:25:" in text and "## heap.allocate_at (1)" in text, "--sites heap.allocate_at")

    for f in failures:
        print(f"test_window_census: {f}")
    print(f"test_window_census: {'FAIL' if failures else 'OK'} ({len(EXPECT) + 9} checks)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
