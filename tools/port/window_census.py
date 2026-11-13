#!/usr/bin/env python3
"""Count what still ties the game to the N64's RDRAM window, by kind (docs/port/NATIVE.md, wave 3's measure).

    python3 tools/port/window_census.py [--build build/pc] [--no-compile] [--jobs N] [--report FILE]
        [--root DIR] [--json FILE] [--sites KIND[,KIND...]|all] [--baseline FILE] [--write-baseline FILE]

Wave 3 of the PC-only work (docs/port/PCONLY.md 4) takes the game out of the 0x80000000 window: its data into
ordinary PC storage, native pointers where records hold 32-bit N64 addresses, the heap and the runtime images as PC
memory, the last data in the N64's byte order converted. Its end is every count below at 0 but the `context.` kinds.
The census measures, it doesn't judge: each kind is a pattern, and a site it counts is something a wave-3 step has to
change, remove, or show to be no N64 address after all (an id that happens to be 0x80000000, a cartridge offset).

Output: one line per kind, always the same kinds in the same order, so two runs (two commits, two steps) compare
line by line:

    kind                                     count  files  what

A count is `-` where the source isn't there (no --build: the generated tables, nm, the compiler; no --report: the
loader's run-time lines). --sites lists every site of the kinds named (`file:line: text`), --json writes counts and
sites, --write-baseline the counts, and --baseline FILE fails (exit 1) when a kind's count grew past the file's.

Where each count comes from:

- The generated data tables (--build): pc-data/data_layout.json (gen_data: the names placed at their N64 address,
  by arena; the names at an offset inside own storage; the reserved PC data in the window; own and decoupled storage
  for context) and pc-data/aliases.ld (gen_stubs: absolute RDRAM and cartridge names). nm over the game's objects
  (game_objects.txt) and the PC layer's: the objects that reach a window name by name.
- The sources, comments and strings told from code (src/, nonmatching/, include/: the game; port/: the PC layer,
  its test programs apart): the prelude's translations (PC_N64P, PC_N64A, PC_N64W, PC_SLOT_N64/SET) used directly;
  each file's own conversion macros (a #define whose body uses one of them, or pc_n64_ptr / pc_n64_addr; the
  N64P/N64W/N64A/N64PTR families), their definitions and their uses in the files that see them; calls of pc_n64_ptr,
  pc_n64_addr and osVirtualToPhysical; fields declared with a link enum (`enum n64p_X`, `enum ChunkLink`: an enum of
  one enumerator named after it, the 4-byte N64 address fields of records kept in the N64 layout); integer fields
  whose comment says they hold a window or N64 address, or that are a loaded file's address fields by name
  (displayList, vtxList, texBase, palBase, trigPtr, pts, woff; waves.c's WADP); the StatLL lists' N64-linked form;
  heap API prototypes that take a 32-bit address; literal window addresses in code (an address, a region test on the
  top bits, a K1 or cartridge address, a hardware register, or 0x80000000 as a flag bit, which is context); the masks
  that take an N64 address apart (0x1FFFFFFF, 0x0F000000, 0xF0FFFFFF); the 24-bit relocations (pc_rcp_reloc); the
  constants of port/include/pc_n64_map.h and their uses; the heap's and the runtime images' N64 placement
  (game_bss_end, overlay_base, osMemSize as an address, allocate_memory_at, RUNTIME_IMAGE_*, N64_OVERLAY_*); the PC
  layer's rule that a value below 4 GiB is an N64 address, and the renderer's and the audio interpreter's reads of
  RDRAM by such a value; the window's mapping (MAP_FIXED, MapViewOfFileEx, the GOT fix-up); data still in the N64's
  byte order (pc_be16/32 accessors, the ranges of texel and sample memory, PC_RGBA, byte swaps in game C); comments
  in the game C that still speak of window, K0 or physical addresses.
- The compiler (--build, unless --no-compile): every game C file of compile_commands.json compiled again with
  -fsyntax-only and the warnings the build turns off, as tools/port/n64_audit.py does (its helpers are reused):
  casts between a pointer and an integer of another size, and implicit conversions between them.
- The loader's report (--report FILE, or the build's data_report.txt when it exists, which the first run of the
  program writes): its `addr-word` lines (a u32 word of ROM data that holds an N64 address), `unresolved` and
  `undescribed` lines (pointers into no symbol, or into arena bytes no symbol describes) and `reached` lines (tables
  only pointers reach, docs/port/DATA.md 7). On the synthetic cartridge these describe the synthetic data; on the
  retail cartridge they describe the ROM, so they stay out of the repository (docs/port/PCONLY.md 1).

The patterns are the ones docs/port/NATIVE.md explains; a test (tools/port/tests/test_window_census.py) runs them on a
made-up tree. Stdlib and binutils (nm), Python 3.9+; on the build host (-j 6) about 15 s with the compiler pass,
5 s without.
"""
import argparse
import collections
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import n64_audit  # noqa: E402  split_code, the compiler pass

GAME_DIRS = ("src", "nonmatching", "include")
PORT_DIRS = ("port",)
C_SUFFIXES = (".c", ".h")

# --------------------------------------------------------------------------- the kinds, in report order

KINDS = [
    # the game's data at its N64 addresses (gen_data, gen_stubs)
    ("data.arena.boot", "names at their N64 address in the boot arena (data.c's absolute .set)"),
    ("data.arena.game", "names at their N64 address in the game image's arena"),
    ("data.arena.ovl_a", "names at their N64 address in runtime image A's arena (in the heap's image block)"),
    ("data.arena.ovl_b", "names at their N64 address in runtime image B's arena"),
    ("data.inside_own", "names at an offset inside another datum's own storage (interior labels)"),
    ("data.reserved", "PC data placed in the window at a fixed address (port/data/reserved.txt)"),
    ("data.window_aliases", "absolute RDRAM names (aliases.ld: low memory, the heap's start, the images' base)"),
    ("data.by_name.game", "game objects that reach a name placed in the window (nm -u)"),
    ("data.by_name.port", "PC-layer objects that reach a name placed in the window"),
    # the 32-bit N64 addresses the code converts
    ("conv.PC_N64P.game", "PC_N64P: an N64 address taken as a host pointer, used directly in game C"),
    ("conv.PC_N64A.game", "PC_N64A: a host pointer's N64 address, used directly in game C"),
    ("conv.PC_N64W.game", "PC_N64W: a pointer stored as a 4-byte N64 address, used directly in game C"),
    ("conv.PC_SLOT.game", "PC_SLOT_N64 / PC_SLOT_SET: a heap handle's slot read or written as an N64 address"),
    ("conv.prelude.port", "the prelude's translations used in the PC layer (PC_N64P/A/W, PC_SLOT_*)"),
    ("conv.file_macros.defs", "files' own conversion macros (N64P, N64W, N64A, N64PTR ..., or a body that converts)"),
    ("conv.file_macros.uses", "uses of those macros in the files that see them"),
    ("conv.pc_n64_ptr.game", "game C calls of pc_n64_ptr / pc_n64_addr"),
    ("conv.pc_n64_ptr.port", "PC-layer calls of pc_n64_ptr / pc_n64_addr (programs, not tests)"),
    ("conv.virt_to_phys.game", "game C calls of osVirtualToPhysical (a pointer to a physical address)"),
    ("conv.silent.trunc", "game C: a pointer-sized integer cast to 32 bits (no warning)"),
    ("conv.silent.zext", "game C: a pointer made from an integer through uintptr_t (no warning)"),
    ("conv.cc.ptr_to_int", "compiler: a pointer cast to an integer of another size"),
    ("conv.cc.int_to_ptr", "compiler: an integer of another size cast to a pointer"),
    ("conv.cc.implicit", "compiler: a pointer used as an integer, or an integer as a pointer, without a cast"),
    # records that hold N64 addresses
    ("fields.link_enum", "fields declared with a link enum: 4-byte N64 addresses in N64-layout records"),
    ("fields.link_enum_types", "link enum types declared (enum n64p_X { n64p_X_ }, enum ChunkLink ...)"),
    ("fields.u32_address", "integer fields that hold a window address: by their comment, or a loaded file's by name"),
    ("fields.statll_n64", "the StatLL lists' N64-linked form (pc_statll_link, pc_statll_set_link, the n64 flag)"),
    ("api.u32_address", "game prototypes with a 32-bit address parameter (the heap's API, the WAD and DL helpers)"),
    # literal window addresses and the arithmetic on them
    ("lit.address.game", "a literal window address in game code (a pointer, a base, an end, a limit)"),
    ("lit.region.game", "a test of an N64 address's top bits in game code (RAM or cartridge?)"),
    ("lit.k1.game", "a literal KSEG1 or cartridge address in game code"),
    ("lit.hw_register.game", "a literal N64 hardware register address in game code"),
    ("lit.mask.game", "N64 address masks in game code (0x1FFFFFFF, 0x0F000000, 0xF0FFFFFF)"),
    ("lit.reloc.game", "24-bit display-list relocations in game C (pc_rcp_reloc, pc_rdram_k0_high)"),
    ("lit.window.port", "literal window addresses in PC-layer code (programs)"),
    ("n64map.uses.game", "uses of port/include/pc_n64_map.h's names in game C"),
    ("n64map.uses.port", "uses of port/include/pc_n64_map.h's names in the PC layer (programs)"),
    # the heap and the runtime images
    ("heap.window_symbols", "the heap's and images' N64 placement by name (game_bss_end, overlay_base, osMemSize, "
                            "gDecompressedDataStart)"),
    ("heap.allocate_at", "allocate_memory_at: a heap block at a fixed address"),
    ("ovl.image_block", "the runtime images' fixed block (RUNTIME_IMAGE_*, N64_OVERLAY_*)"),
    # the PC layer's view of the window
    ("port.below_4g", "PC-layer tests that take a value below 4 GiB for an N64 address (or a host pointer above)"),
    ("port.n64_reads", "the renderer's and the audio interpreter's reads of a 32-bit value as RDRAM (rdram, n64ptr)"),
    ("port.mapping", "the window's fixed mappings and the GOT fix-up (MAP_FIXED, MapViewOfFileEx, absfix)"),
    ("port.rdram_base", "PC_RDRAM_BASE / PC_RDRAM_SIZE / PC_RDRAM_MAX in the PC layer (programs)"),
    ("tests.window", "test programs' own windows: their fixed mappings, fix-ups and literal window addresses"),
    # byte order
    ("be.accessors.game", "big-endian reads and writes in game C (pc_be16/32_get/set): N64-form data"),
    ("be.accessors.port", "the same in the PC layer (programs)"),
    ("be.ranges", "data the PC keeps in the cartridge's byte order, by range: texel memory (a file's texels and TLUTs: "
                  "pc_mem_register_texels / unregister, pc_mem_is_texel_memory, pcgfx_is_texel_memory) and sample memory "
                  "(the sound caches: pc_mem_register_samples / unregister, pc_mem_is_sample_memory) (programs)"),
    ("be.rgba.game", "PC_RGBA and byte swaps in game C (colour words in the N64's register order)"),
    # the words
    ("text.window.game", "game C comment lines about window, N64, K0 or physical addresses and RDRAM"),
    # the loader's run-time report
    ("runtime.addr_word", "the loader's report: u32 words in ROM data that hold an N64 address"),
    ("runtime.unresolved", "the loader's report: pointers into no symbol"),
    ("runtime.undescribed", "the loader's report: pointers into arena bytes no symbol describes"),
    ("runtime.reached", "the loader's report: tables only pointers reach"),
    # context: what isn't the window, for scale
    ("context.own", "context: data with its own storage (host layout, outside the window)"),
    ("context.decoupled", "context: data the PC owns (port/data/decoupled.txt)"),
    ("context.cartridge_aliases", "context: cartridge addresses (aliases.ld; the PI bus, not RAM)"),
    ("context.lit_bit.game", "context: 0x80000000 as a flag or sign bit in game code"),
    ("context.n64map.defs", "context: constants port/include/pc_n64_map.h defines"),
]
KIND_NAMES = [k for k, _ in KINDS]

# --------------------------------------------------------------------------- patterns

PRELUDE_GAME = {
    "conv.PC_N64P.game": re.compile(r"\bPC_N64P\s*\("),
    "conv.PC_N64A.game": re.compile(r"\bPC_N64A\s*\("),
    "conv.PC_N64W.game": re.compile(r"\bPC_N64W\s*\("),
    "conv.PC_SLOT.game": re.compile(r"\bPC_SLOT_(?:N64|SET)\s*\("),
}
PRELUDE_ANY = re.compile(r"\bPC_(?:N64P|N64A|N64W|SLOT_N64|SLOT_SET)\s*\(")
TRANSLATE_FN = re.compile(r"\bpc_n64_(?:ptr|addr)\s*\(")
DEFINE = re.compile(r"^\s*#\s*define\s+(\w+)(\([^)]*\))?\s*(.*)$")
# The conversion macro families by name: their definitions count whatever the body (an N64P that is a _Generic cast).
CONV_FAMILY = re.compile(r"^(?:N64P|N64W|N64A|N64A0|N64AS|N64PTR|N64PTR0|M2C_N64P)$")
VIRT_TO_PHYS = re.compile(r"\bosVirtualToPhysical\s*\(")
LINK_ENUM_DECL = re.compile(r"\benum\s+(\w+)\s*\{\s*(\w+)\s*\}")
U32_ADDR_FIELD = re.compile(r"^\s*(?:volatile\s+)?(?:u32|s32|unsigned(?:\s+int)?|int|uintptr_t|intptr_t)\s+\w+"
                            r"(?:\[[^\]]*\])?\s*;.*(?:window address|N64 address)", re.I)
# The loaded files' address fields by name (docs/port/ASSETS.md 4: the WAD records' display list, vertex, texture,
# palette and path pointers, the world's name table offsets), declared as 32-bit integers or waves.c's WADP(type).
ASSET_FIELD = re.compile(r"^\s*(?:WADP\s*\(\s*\w+\s*\)|u32|s32|unsigned(?:\s+int)?)\s+(?:displayList|vtxList|texBase|"
                         r"palBase|trigPtr|pts|woff)\s*(?:\[[^\]]*\])?\s*;")
STATLL = re.compile(r"\bpc_statll_(?:set_)?link\s*\(|\bn64\s*=\s*1\b")
API_ADDR = re.compile(r"^[A-Za-z_][\w\s\*]*\b\w+\s*\([^)]*\b(?:u32|s32|unsigned int)\s+(?:address|addr|n64_addr|base|"
                      r"oldBase|vertices)\b[^)]*\)\s*;")
MASK = re.compile(r"\b0[xX](?:1[fF]{7}|0[fF]000000|[fF]0[fF]{6})[uUlL]*\b")
RELOC = re.compile(r"\bpc_rcp_reloc\s*\(|\bpc_rdram_k0_high\s*\(")
HEAP_SYMS = re.compile(r"\b(?:game_bss_end|overlay_base|gDecompressedDataStart)\b|\bosMemSize\s*\|")
ALLOCATE_AT = re.compile(r"\ballocate_memory_at\s*\(")
IMAGE_BLOCK = re.compile(r"\bRUNTIME_IMAGE_(?:START|END)\b|\bN64_OVERLAY_(?:BASE|BLOCK_END)\b")
BELOW_4G = re.compile(r"\bis_host\s*\(|\(\s*uintptr_t\s*\)\s*1\s*<<\s*32|"
                      r"\(\s*uintptr_t\s*\)[^;]*>>\s*(?:32\b|16\s*>>\s*16)|"
                      r"uintptr_t[^;]*>\s*0[xX][fF]{8}[uU]?[lL]*\b")
# The renderer's and the audio interpreter's reads of a 32-bit value as RDRAM (port/gfx/gfx_rsp.c, port/audio/musyx.c).
N64_READS = re.compile(r"\brdram\s*\(|\bn64ptr\s*\(")
MAPPING = re.compile(r"\bMAP_FIXED\w*|\bMapViewOfFileEx\b|\bpc_mem_fix_absolute\b|\bpc_mem_fixed\s*\(")
RDRAM_BASE = re.compile(r"\bPC_RDRAM_(?:BASE|SIZE|MAX)\b")
BE_ACCESS = re.compile(r"\bpc_be(?:16|32)_(?:get|set)\s*\(")
BE_RANGE = re.compile(r"\bpc_mem_(?:(?:un)?register_(?:texels|samples)|is_(?:texel|sample)_memory)\s*\(|"
                      r"\bpcgfx_is_texel_memory\s*\(")
RGBA = re.compile(r"\bPC_RGBA\s*\(|__builtin_bswap(?:16|32|64)\s*\(")
WINDOW_WORDS = re.compile(r"window address|N64 address|\bRDRAM\b|\bKSEG[01]\b|\bK0 address|physical address", re.I)
LIT = n64_audit.LIT   # 0x8xxxxxxx..0xBxxxxxxx


# --------------------------------------------------------------------------- the census


class Census:
    def __init__(self):
        self.sites = collections.defaultdict(list)   # kind -> [(file, line, text)]
        self.counts = {}                             # kind -> count, for kinds counted without sites
        self.measured = set()                        # kinds whose source was there

    def add(self, kind, path, line, text):
        assert kind in KIND_NAMES, kind
        self.sites[kind].append((str(path).replace(os.sep, "/"), line, text.strip()[:160]))   # tree paths with /
        self.measured.add(kind)

    def count(self, kind, n):
        assert kind in KIND_NAMES, kind
        self.counts[kind] = self.counts.get(kind, 0) + n
        self.measured.add(kind)

    def touch(self, *kinds):
        """The source of these kinds was read: a kind with no site is 0, not unmeasured."""
        self.measured.update(kinds)

    def total(self, kind):
        if kind not in self.measured:
            return None
        return len(self.sites[kind]) + self.counts.get(kind, 0)

    def files(self, kind):
        return len({f for f, _, _ in self.sites[kind]})


def files_under(root, dirs, suffixes):
    for d in dirs:
        base = root / d
        if base.is_dir():
            for p in sorted(base.rglob("*")):
                if p.is_file() and p.suffix in suffixes:
                    yield p


def is_test(rel):
    parts = Path(rel).parts
    return "tests" in parts or "test_overlay" in parts or Path(rel).name.startswith("test_")


def n64map_names(root):
    p = root / "port" / "include" / "pc_n64_map.h"
    return set(re.findall(r"^\s*#\s*define\s+(\w+)", p.read_text(), re.M)) - {"PC_N64_MAP_H"} if p.exists() else set()


def lit_class(value, code_line):
    """n64_audit's classes of a literal in the window's range: address, region, k1, hw_register or bit."""
    cls = n64_audit.lit_code_class(value, code_line)
    return cls.rsplit(".", 1)[-1]


def declares(code_line, *names):
    """The line declares or defines one of these functions (a type before the name), rather than calling it."""
    return any(re.match(r"\s*(?:extern\s+|static\s+)?(?:const\s+)?\w+[\s\*]+%s\s*\(" % n, code_line) for n in names)


class Source:
    """A C file: its lines, its code with comments and strings blanked, and its comments."""

    def __init__(self, path, rel):
        self.path, self.rel = path, rel
        self.posix = rel.as_posix()   # the tree path with /, on Windows too, for the comparisons by prefix and name
        text = path.read_text(errors="replace")
        self.lines = text.split("\n")
        code, comm = n64_audit.split_code(text)
        self.code, self.comm = code.split("\n"), comm.split("\n")
        # macro[k]: the name of the #define line k is part of (its first line or a continuation), else None
        self.macro, name = [], None
        for c in self.code:
            m = DEFINE.match(c)
            if m:
                name = m.group(1)
            self.macro.append(name)
            if not c.rstrip().endswith("\\"):
                name = None


def conversion_macros(src):
    """The file's own conversion macros: name -> the line its definition starts on."""
    out, start, body = {}, {}, collections.defaultdict(str)
    for k, c in enumerate(src.code):
        name = src.macro[k]
        if name:
            start.setdefault(name, k + 1)
            m = DEFINE.match(c)
            body[name] += m.group(3) if m else c
    for name, b in body.items():
        if CONV_FAMILY.match(name) or PRELUDE_ANY.search(b) or TRANSLATE_FN.search(b):
            out[name] = start[name]
    return out


def scan_game(cen, root, n64map):
    sources = [Source(p, p.relative_to(root)) for p in files_under(root, GAME_DIRS, C_SUFFIXES)]
    cen.touch(*PRELUDE_GAME, "conv.file_macros.defs", "conv.file_macros.uses", "conv.pc_n64_ptr.game",
              "conv.virt_to_phys.game", "conv.silent.trunc", "conv.silent.zext", "fields.link_enum",
              "fields.link_enum_types", "fields.u32_address", "fields.statll_n64", "api.u32_address",
              "lit.address.game", "lit.region.game", "lit.k1.game", "lit.hw_register.game", "lit.mask.game",
              "lit.reloc.game", "n64map.uses.game", "heap.window_symbols", "heap.allocate_at", "ovl.image_block",
              "be.accessors.game", "be.rgba.game", "text.window.game", "context.lit_bit.game")
    # Link enums: declared anywhere in the game C, then the fields typed with them.
    link_enums = set()
    for s in sources:
        for k, c in enumerate(s.code):
            for m in LINK_ENUM_DECL.finditer(c):
                if m.group(2) == m.group(1) + "_":
                    link_enums.add(m.group(1))
                    cen.add("fields.link_enum_types", s.rel, k + 1, s.lines[k])
    field_re = re.compile(r"\benum\s+(?:%s)\b\s*\**\s*\w+\s*(?:\[[^\]]*\])?\s*[;,)]" % "|".join(
        sorted(link_enums))) if link_enums else None
    # Conversion macros a header defines are seen by the files that include it.
    header_macros = {}
    for s in sources:
        if s.path.suffix == ".h":
            for name in conversion_macros(s):
                header_macros.setdefault(name, set()).add(s.posix)
    n64map_re = re.compile(r"\b(?:%s)\b" % "|".join(sorted(n64map))) if n64map else None
    for s in sources:
        own = conversion_macros(s)
        for name, ln in own.items():
            cen.add("conv.file_macros.defs", s.rel, ln, s.lines[ln - 1])
        seen = dict.fromkeys(own, None)
        includes = set(re.findall(r'^\s*#\s*include\s+"([^"]+)"', "\n".join(s.lines), re.M))
        for name, hdrs in header_macros.items():
            if name not in seen and (any(h.endswith(i) or Path(h).name == Path(i).name for h in hdrs for i in includes)
                                     or s.posix in hdrs):
                seen[name] = None
        use_re = re.compile(r"\b(?:%s)\s*\(" % "|".join(sorted(seen))) if seen else None
        for k, c in enumerate(s.code):
            line, ln = s.lines[k], k + 1
            if c.strip():
                d = DEFINE.match(c)
                defines = s.macro[k]
                for kind, pat in PRELUDE_GAME.items():
                    if pat.search(c) and defines not in own:
                        cen.add(kind, s.rel, ln, line)
                if use_re and defines not in seen:
                    for m in use_re.finditer(c):
                        cen.add("conv.file_macros.uses", s.rel, ln, line)
                if TRANSLATE_FN.search(c) and not declares(c, "pc_n64_ptr", "pc_n64_addr"):
                    cen.add("conv.pc_n64_ptr.game", s.rel, ln, line)
                if VIRT_TO_PHYS.search(c) and not declares(c, "osVirtualToPhysical"):
                    cen.add("conv.virt_to_phys.game", s.rel, ln, line)
                if n64_audit.TRUNC.search(c):
                    cen.add("conv.silent.trunc", s.rel, ln, line)
                if n64_audit.ZEXT.search(c) and defines not in own:
                    cen.add("conv.silent.zext", s.rel, ln, line)
                if field_re and field_re.search(c):
                    cen.add("fields.link_enum", s.rel, ln, line)
                if STATLL.search(c):
                    cen.add("fields.statll_n64", s.rel, ln, line)
                if API_ADDR.match(c) and s.posix.startswith("include/"):
                    cen.add("api.u32_address", s.rel, ln, line)
                for h in LIT.finditer(c):
                    cls = lit_class(h.group(1), c)
                    cen.add({"address": "lit.address.game", "region": "lit.region.game", "k1": "lit.k1.game",
                             "hw_register": "lit.hw_register.game", "bit": "context.lit_bit.game"}[cls],
                            s.rel, ln, line)
                    break
                if MASK.search(c):
                    cen.add("lit.mask.game", s.rel, ln, line)
                if RELOC.search(c):
                    cen.add("lit.reloc.game", s.rel, ln, line)
                if n64map_re and n64map_re.search(c):
                    cen.add("n64map.uses.game", s.rel, ln, line)
                if HEAP_SYMS.search(c) and not c.lstrip().startswith("extern"):
                    cen.add("heap.window_symbols", s.rel, ln, line)
                if ALLOCATE_AT.search(c) and not declares(c, "allocate_memory_at"):
                    cen.add("heap.allocate_at", s.rel, ln, line)
                if IMAGE_BLOCK.search(c):
                    cen.add("ovl.image_block", s.rel, ln, line)
                if BE_ACCESS.search(c):
                    cen.add("be.accessors.game", s.rel, ln, line)
                if RGBA.search(c):
                    cen.add("be.rgba.game", s.rel, ln, line)
            if U32_ADDR_FIELD.match(line) or ASSET_FIELD.match(s.code[k]):
                cen.add("fields.u32_address", s.rel, ln, line)
            if s.comm[k].strip() and WINDOW_WORDS.search(s.comm[k]):
                cen.add("text.window.game", s.rel, ln, line)


def scan_port(cen, root, n64map):
    cen.touch("conv.prelude.port", "conv.pc_n64_ptr.port", "lit.window.port", "n64map.uses.port", "port.below_4g",
              "port.n64_reads",
              "port.mapping", "port.rdram_base", "tests.window", "be.accessors.port", "be.ranges",
              "context.n64map.defs")
    n64map_re = re.compile(r"\b(?:%s)\b" % "|".join(sorted(n64map))) if n64map else None
    for p in files_under(root, PORT_DIRS, C_SUFFIXES):
        s = Source(p, p.relative_to(root))
        test = is_test(s.rel)
        n64map_h = s.rel.as_posix() == "port/include/pc_n64_map.h"
        prelude_h = s.rel.as_posix() == "port/include/pc_prelude.h"
        for k, c in enumerate(s.code):
            if not c.strip():
                continue
            line, ln = s.lines[k], k + 1
            if n64map_h:
                m = DEFINE.match(c)
                if m and m.group(1) in n64map:
                    cen.add("context.n64map.defs", s.rel, ln, line)
                continue
            if test:
                if MAPPING.search(c) or any(int(h.group(1), 16) < 0xC0000000 for h in LIT.finditer(c)) or \
                        RDRAM_BASE.search(c):
                    cen.add("tests.window", s.rel, ln, line)
                continue
            if PRELUDE_ANY.search(c) and not prelude_h:
                cen.add("conv.prelude.port", s.rel, ln, line)
            if TRANSLATE_FN.search(c) and not declares(c, "pc_n64_ptr", "pc_n64_addr"):
                cen.add("conv.pc_n64_ptr.port", s.rel, ln, line)
            if any(0x80000000 <= int(h.group(1), 16) < 0xC0000000 for h in LIT.finditer(c)):
                cen.add("lit.window.port", s.rel, ln, line)
            if n64map_re and n64map_re.search(c):
                cen.add("n64map.uses.port", s.rel, ln, line)
            if BELOW_4G.search(c):
                cen.add("port.below_4g", s.rel, ln, line)
            if N64_READS.search(c) and s.rel.as_posix() in ("port/gfx/gfx_rsp.c", "port/audio/musyx.c"):
                cen.add("port.n64_reads", s.rel, ln, line)
            if MAPPING.search(c):
                cen.add("port.mapping", s.rel, ln, line)
            if RDRAM_BASE.search(c):
                cen.add("port.rdram_base", s.rel, ln, line)
            if BE_ACCESS.search(c) and not prelude_h:
                cen.add("be.accessors.port", s.rel, ln, line)
            if BE_RANGE.search(c) and not re.match(r"\s*(?:void|int|static|extern)\b.*\)\s*;\s*$", c):
                cen.add("be.ranges", s.rel, ln, line)


def scan_tables(cen, build, jobs):
    lay_path = build / "pc-data" / "data_layout.json"
    if not lay_path.exists():
        return
    lay = json.loads(lay_path.read_text())
    cen.touch("data.arena.boot", "data.arena.game", "data.arena.ovl_a", "data.arena.ovl_b", "data.inside_own",
              "data.reserved", "data.window_aliases", "context.own", "context.decoupled", "context.cartridge_aliases")
    names = ("boot", "game", "ovl_a", "ovl_b")
    window = {}
    placed = list(lay["absolute"]) + [[n, a] for n, a in lay.get("window_names", [])]
    for name, addr in placed:
        arena = next((names[i] for i, (lo, hi) in enumerate(lay["arenas"]) if lo <= addr <= hi), None)
        window[name] = addr
        if arena:
            cen.add(f"data.arena.{arena}", "data_layout.json", 0, f"{name} 0x{addr:08X}")
        elif addr < 0xA0000000:
            cen.add("data.window_aliases", "data_layout.json", 0, f"{name} 0x{addr:08X}")
        else:
            cen.add("context.cartridge_aliases", "data_layout.json", 0, f"{name} 0x{addr:08X}")
    for name, base, off in lay["sets"]:
        cen.add("data.inside_own", "data_layout.json", 0, f"{name} = {base} + 0x{off:X}")
    for name, addr, size in lay.get("reserved", []):
        window[name] = addr
        cen.add("data.reserved", "data_layout.json", 0, f"{name} 0x{addr:08X} {size} B")
    for name, size in lay["own"]:
        cen.add("context.own", "data_layout.json", 0, f"{name} {size} B")
    for pc, size, retired in lay["decoupled"]:
        cen.add("context.decoupled", "data_layout.json", 0, f"{pc} {size} B")
    aliases = build / "pc-data" / "aliases.ld"
    if aliases.exists():
        for k, line in enumerate(aliases.read_text().splitlines()):
            m = re.match(r"PROVIDE\((\w+) = ABSOLUTE\(0x([0-9A-Fa-f]+)\)\);", line)
            if m:
                v = int(m.group(2), 16)
                window[m.group(1)] = v
                cen.add("data.window_aliases" if v < 0xA0000000 else "context.cartridge_aliases", "aliases.ld",
                        k + 1, line)
    # Who reaches the window's names by name.
    nm = os.environ.get("NM", "nm")
    ram = {n for n, v in window.items() if v < 0xA0000000}

    def undefined(o):
        r = subprocess.run([nm, "-u", o], capture_output=True, text=True)
        return o, {ln.split()[-1] for ln in r.stdout.splitlines() if ln.strip()}

    game_list = build / "game_objects.txt"
    if game_list.exists():
        objs = [o for o in game_list.read_text().split() if o.endswith((".o", ".obj"))]
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            for o, und in ex.map(undefined, objs):
                hit = sorted(und & ram)
                if hit:
                    cen.add("data.by_name.game", os.path.relpath(o, build), 0, f"{len(hit)}: {' '.join(hit[:6])}")
        cen.touch("data.by_name.game")
    port_objs = {}
    for o in sorted(build.rglob("*.o")):
        rel = o.relative_to(build).as_posix()
        m = re.search(r"/(pcultra|pcfrontend|pcapp|pcassets|pcmods|pcgfx|pcaudio|pcinput|pcnet|pcconfig|pcdata_rt)"
                      r"\.dir/(.*)$", "/" + rel)
        if m and not is_test(m.group(2)):
            port_objs.setdefault(m.group(2), str(o))
    if port_objs:
        with ThreadPoolExecutor(max_workers=jobs) as ex:
            for o, und in ex.map(undefined, port_objs.values()):
                hit = sorted(und & ram)
                if hit:
                    cen.add("data.by_name.port", os.path.relpath(o, build), 0, f"{len(hit)}: {' '.join(hit[:6])}")
        cen.touch("data.by_name.port")


def scan_compiler(cen, build, jobs):
    if not (build / "compile_commands.json").exists():
        return
    inv = n64_audit.Inventory()
    n64_audit.compiler_pass(inv, build, jobs)
    cen.touch("conv.cc.ptr_to_int", "conv.cc.int_to_ptr", "conv.cc.implicit")
    for cls, sites in inv.sites.items():
        if cls.endswith(".weak"):
            continue
        kind = {"cc.ptr_to_int": "conv.cc.ptr_to_int", "cc.int_to_ptr": "conv.cc.int_to_ptr"}.get(
            cls, "conv.cc.implicit" if cls.startswith("cc.implicit") else None)
        if kind:
            for f, line, text in sites:
                cen.add(kind, f, line, text)


def scan_report(cen, report):
    if not report or not report.exists():
        return
    cen.touch("runtime.addr_word", "runtime.unresolved", "runtime.undescribed", "runtime.reached")
    for k, line in enumerate(report.read_text(errors="replace").splitlines()):
        for prefix, kind in (("addr-word ", "runtime.addr_word"), ("unresolved ", "runtime.unresolved"),
                             ("undescribed ", "runtime.undescribed"), ("reached ", "runtime.reached")):
            if line.startswith(prefix):
                cen.add(kind, report.name, k + 1, line)


# --------------------------------------------------------------------------- the report


def report(cen, out, header):
    out.write(f"# window_census: what still ties the game to the N64's RDRAM window (docs/port/NATIVE.md){header}\n")
    for kind, what in KINDS:
        n = cen.total(kind)
        files = cen.files(kind) if n else 0
        out.write(f"{kind:30} {('-' if n is None else n):>7} {files if files else '':>6}  {what}\n")


def list_sites(cen, out, which):
    for kind in KIND_NAMES:
        if not cen.sites[kind] or not any(w in ("all", kind) or (w.endswith(".") and kind.startswith(w))
                                          for w in which):
            continue
        out.write(f"\n## {kind} ({len(cen.sites[kind])})\n")
        for f, line, text in cen.sites[kind]:
            out.write(f"{f}:{line}: {text}\n" if line else f"{f}: {text}\n")


def describe_tree(root):
    try:
        r = subprocess.run(["git", "-C", str(root), "describe", "--always", "--dirty"], capture_output=True,
                           text=True, timeout=10)
        return r.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=HERE.parents[1], help="the source tree (default: this one)")
    ap.add_argument("--build", type=Path, help="a configured and built CMake build (build/pc)")
    ap.add_argument("--no-compile", action="store_true", help="skip the compiler pass")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--report", type=Path, help="a data_report.txt (default: the build's, when it exists)")
    ap.add_argument("--json", type=Path)
    ap.add_argument("--sites", help="kinds (or prefixes ending in .) to list site by site; 'all' for every kind")
    ap.add_argument("--baseline", type=Path, help="fail if a kind's count grew past this file's")
    ap.add_argument("--write-baseline", type=Path)
    a = ap.parse_args(argv)

    root = a.root.resolve()
    build = a.build.resolve() if a.build else None
    cen = Census()
    n64map = n64map_names(root)
    scan_game(cen, root, n64map)
    scan_port(cen, root, n64map)
    if build:
        scan_tables(cen, build, a.jobs)
        if not a.no_compile:
            scan_compiler(cen, build, a.jobs)
    scan_report(cen, a.report or (build / "data_report.txt" if build else None))

    header = f" {describe_tree(root)}" + (f", build {build}" if build else "") + \
        ("" if build and not a.no_compile else ", no compiler pass")
    report(cen, sys.stdout, header)
    if a.sites:
        list_sites(cen, sys.stdout, a.sites.split(","))
    counts = {k: cen.total(k) for k in KIND_NAMES if cen.total(k) is not None}
    if a.json:
        a.json.write_text(json.dumps({"counts": counts, "sites": {k: cen.sites[k] for k in KIND_NAMES
                                                                  if cen.sites[k]}}, indent=1) + "\n")
    if a.write_baseline:
        a.write_baseline.write_text(json.dumps(counts, indent=1) + "\n")
    if a.baseline:
        base = json.loads(a.baseline.read_text())
        grew = [(k, base.get(k, 0), n) for k, n in counts.items() if not k.startswith("context.")
                and n > base.get(k, 0)]
        for k, was, now in grew:
            print(f"window_census: {k} grew from {was} to {now}", file=sys.stderr)
        if grew:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
