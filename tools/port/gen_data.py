#!/usr/bin/env python3
"""Generate data.c: host storage for the game's data symbols and the descriptors that fill it from the ROM.

    python3 tools/port/gen_data.py --objects-list build/pc/game_objects.txt --out build/pc/pc-data/data.c
    python3 tools/port/gen_data.py --check build/pc/rush2049pc_datacheck --layout build/pc/pc-data/data_layout.json
    python3 tools/port/gen_data.py --test-tables test_pc_data_decls.c.o --out build/pc/pc-data/test_pc_data_gen.h

The game C declares data at fixed N64 addresses that nothing defines: the boot segment's, the game image's and
the two runtime images' initialized data, and their BSS. This tool gives every such symbol host storage and
writes the descriptors port/data/pc_data.c uses to fill it from the user's ROM at startup (docs/port/DATA.md).
No ROM bytes are read here or written into the build.

1. Types come from the game objects' DWARF (readelf): each file's own declaration. When files disagree, the
   most informative wins (struct over pointer over words over bytes) and the disagreement is reported. The
   objects must be unoptimized (-O0): an optimized object's DWARF leaves out the variables whose uses the
   optimizer removed, so the tables would depend on the build type. An optimized build passes a -O0 copy of the
   game objects (types_objects.txt) and the objects it links (--link-objects-list, for the runtime images' C
   data); gen_data refuses DWARF whose producer line records optimization (docs/port/OPTIMIZED.md).
2. Each type is laid out twice: for the N64 (MIPS o32: pointers and long are 4 bytes, double is 8 aligned 8)
   and for the host (the DWARF offsets). The result is a field program: runs of (kind, count, N64 offset, host
   offset), repeated with strides for arrays. A union is copied as bytes when a member made of bytes covers it
   (the colour unions), else laid out by its largest member.
3. Storage. Each image's data and BSS is an arena laid out as on the N64, at its N64 address in the RDRAM
   window port/ultra maps at host 0x80000000, and a symbol with no pointers whose two layouts agree is an
   absolute symbol at its N64 address there (an assembler .set), so code that indexes from one symbol into the
   next keeps working and a 32-bit N64 address of it is a host address. A symbol with pointers, or whose host
   layout differs, gets its own storage in the executable; symbols inside its N64 extent are defined at the
   matching host offsets. Where declarations overlap in an arena at other element widths (bytes included), the
   most informative one decides and the rest are reported.
4. Addresses: the referencing image's symbol table (asm/us/<image>/symbols.json, what the matching build links
   against), then symbol_addrs.us.txt and undefined_syms_auto.us.txt, then D_XXXXXXXX names. Functions known by
   address (layout/*.json, the symbol file) go in a table, through weak references, so the loader can turn N64
   function pointers into host ones; the game's data defined in C is listed the same way.
5. Names the game calls under another label of a defined function (audio_dma_sync = AllocateMemoryHeap) get a
   branch thunk.

Outputs: data.c, data_layout.txt (what was decided, the conflicts, what is left), data_layout.json (for
--check, which verifies placements and sizes in the linked executable, and that nothing else is linked at a
window address). Stdlib and binutils, Python 3.9+.

Windows (PE, docs/port/WINDOWS.md): pass objdump as --readelf (DWARF of COFF objects), and --weak-script. An
undefined weak reference doesn't come out as 0 there, so data.c's weak references name `pc_weak.<symbol>`, which
the script defines as the symbol when something defines it and as 0 otherwise; thunks are naked functions (COFF
has no section stack for `.previous`).
"""
import argparse
import bisect
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

# Element kinds (port/data/pc_data.h).
K_U8, K_U16, K_U32, K_F32, K_U64, K_F64, K_PTR, K_S32_64, K_U32_64 = range(1, 10)
KIND_NAMES = {K_U8: "u8", K_U16: "u16", K_U32: "u32", K_F32: "f32", K_U64: "u64", K_F64: "f64", K_PTR: "ptr",
              K_S32_64: "s32>s64", K_U32_64: "u32>u64"}
N64_ESZ = {K_U8: 1, K_U16: 2, K_U32: 4, K_F32: 4, K_U64: 8, K_F64: 8, K_PTR: 4, K_S32_64: 4, K_U32_64: 4}
HOST_PTR = 8   # from DWARF pointer types; checked as they are read
IMAGE_INDEX = {img[0]: i for i, img in enumerate(n64syms.IMAGES)}
# The game's libc names compile as n64_<name> (CMakeLists.txt).
RENAMED = {n: "n64_" + n for n in ("main", "memcpy", "memset", "memchr", "strlen", "strchr", "sinf", "cosf")}
# A runtime image's function whose name the other image's function at the same address has (both are linked in):
# its PC name (nonmatching/ovl_a/func_8038A400.c).
IMAGE_RENAMED = {("ovl_a", "func_8038A400"): "updateWarningScreen"}
# N64 (IDO, o32) sizes of the C base types, by DWARF name.
N64_BASE = {"char": 1, "signed char": 1, "unsigned char": 1, "_Bool": 1, "short int": 2, "short unsigned int": 2,
            "int": 4, "unsigned int": 4, "long int": 4, "long unsigned int": 4, "long long int": 8,
            "long long unsigned int": 8, "float": 4, "double": 8, "long double": 8}
# Typedefs that are pointer-sized on the host and 4 bytes on the N64 (port/include/pc_prelude.h). Windows spells
# them `long long`, so they are told from a real 64-bit integer by name.
POINTER_SIZED = {"uintptr_t", "intptr_t", "size_t", "ssize_t", "ptrdiff_t"}


# N64 pointers at or above this (outside the runtime images) point into the game heap and stay as they are.
HEAP_LO = 0x8017A640
# The RDRAM window (port/include/pc_port.h: PC_RDRAM_BASE, PC_RDRAM_SIZE). The arenas are at their N64 addresses in
# it; the executable itself must link below it (port/ultra/memory.c's absolute-symbol fix-up relies on that).
WINDOW_LO, WINDOW_HI = 0x80000000, 0x80800000
for _img, _lo, _init, _hi in n64syms.ARENAS:   # boot and game below the heap; the runtime images' inside it
    assert WINDOW_LO <= _lo <= _init <= _hi <= (WINDOW_HI if _img.startswith("ovl") else HEAP_LO), _img
# What data.c itself defines for the game C.
PC_EXPORTS = {"pc_data_init", "pc_data_overlay_load"}


def esz_host(kind):
    return HOST_PTR if kind == K_PTR else 8 if kind in (K_S32_64, K_U32_64) else N64_ESZ[kind]


def align(x, a):
    return (x + a - 1) // a * a


# ---------------------------------------------------------------- DWARF (readelf text)

_HDR = re.compile(r"^ <(\d+)><([0-9a-f]+)>: Abbrev Number: \d+(?: \((DW_TAG_\w+)\))?")
_ATTR = re.compile(r"^\s+<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s?(.*)$")
_REF = re.compile(r"<0x([0-9a-f]+)>")
INTERN = {}


class Die:
    __slots__ = ("tag", "attrs", "children")

    def __init__(self, tag):
        self.tag, self.attrs, self.children = tag, {}, []


def _num(v):
    v = v.split()[0] if v else "0"
    try:
        return int(v, 0)
    except ValueError:
        return None


def _name(v):
    if v.startswith("(indirect"):
        return v.split("): ", 1)[1].strip()
    return v.strip()


_OBJDUMP_FILE = re.compile(r"^(\S.*):\s+file format \S+$")


def dwarf_info_command(tool):
    """The command that prints .debug_info: readelf's, or objdump's for PE objects (readelf reads only ELF; both
    print the DIEs in the same form)."""
    return [tool, "--dwarf=info"] if "objdump" in Path(tool).name else [tool, "--debug-dump=info"]


def parse_readelf(text, single=None):
    """{object: {offset: Die}} from `readelf --debug-dump=info` output (one CU per object), or from
    `objdump --dwarf=info`, which starts each object with `path:     file format ...`."""
    objs, dies, stack, cur = {}, None, [], None
    if single:
        dies = objs.setdefault(single, {})
    for line in text.splitlines():
        if line.startswith("File: "):
            dies, stack, cur = objs.setdefault(line[6:].strip(), {}), [], None
            continue
        m = _OBJDUMP_FILE.match(line) if line[:1] not in ("", " ") else None
        if m:
            dies, stack, cur = objs.setdefault(single or m.group(1), {}), [], None
            continue
        if line.startswith(" <"):
            m = _HDR.match(line)
            if m:
                depth, tag = int(m.group(1)), m.group(3)
                if tag is None:
                    cur = None
                    continue
                cur = Die(tag)
                dies[int(m.group(2), 16)] = cur
                del stack[depth:]
                if stack:
                    stack[-1].children.append(cur)
                stack.append(cur)
                continue
        if cur is not None:
            m = _ATTR.match(line)
            if m:
                cur.attrs[m.group(1)] = m.group(2)
    return objs


def _ref(d, attr="DW_AT_type"):
    v = d.attrs.get(attr)
    m = _REF.search(v) if v else None
    return int(m.group(1), 16) if m else None


def canon(dies, off, memo):
    """Canonical type tuple for the DIE at off: ('base', name, size), ('ptr',), ('enum', size),
    ('struct'|'union', tag, size, ((member, host offset, type), ...)), ('array', elem, count|None),
    ('opaque', tag), ('void',) or ('func',). Typedefs and qualifiers are dropped."""
    global HOST_PTR
    if off is None:
        return ("void",)
    if off in memo:
        return memo[off]
    d = dies.get(off)
    tag = d.tag if d else None
    if tag in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type", "DW_TAG_restrict_type",
               "DW_TAG_atomic_type"):
        r = canon(dies, _ref(d), memo)
        if tag == "DW_TAG_typedef" and r[0] == "base" and r[2] == 8 and \
                _name(d.attrs.get("DW_AT_name", "")) in POINTER_SIZED:
            # 4 bytes on the N64 like a pointer; `long long` on Windows (LLP64), `long` elsewhere
            r = ("base", "long unsigned int" if "unsigned" in r[1] else "long int", 8)
    elif tag == "DW_TAG_base_type":
        r = ("base", _name(d.attrs.get("DW_AT_name", "?")), _num(d.attrs.get("DW_AT_byte_size")))
    elif tag == "DW_TAG_pointer_type":
        HOST_PTR = _num(d.attrs.get("DW_AT_byte_size", "8")) or 8
        r = ("ptr",)
    elif tag == "DW_TAG_enumeration_type":
        r = ("enum", _num(d.attrs.get("DW_AT_byte_size", "4")))
    elif tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
        tname = _name(d.attrs["DW_AT_name"]) if "DW_AT_name" in d.attrs else None
        if "DW_AT_declaration" in d.attrs:
            r = ("opaque", tname)
        else:
            members = tuple((_name(m.attrs.get("DW_AT_name", "")), _num(m.attrs.get("DW_AT_data_member_location", "0")),
                             canon(dies, _ref(m), memo)) for m in d.children if m.tag == "DW_TAG_member")
            r = ("struct" if tag == "DW_TAG_structure_type" else "union", tname,
                 _num(d.attrs.get("DW_AT_byte_size", "0")), members)
    elif tag == "DW_TAG_array_type":
        r = canon(dies, _ref(d), memo)
        dims = []
        for s in d.children:
            if s.tag != "DW_TAG_subrange_type":
                continue
            if "DW_AT_count" in s.attrs:
                dims.append(_num(s.attrs["DW_AT_count"]))
            elif "DW_AT_upper_bound" in s.attrs:
                ub = _num(s.attrs["DW_AT_upper_bound"])
                dims.append(0 if ub is None or ub < 0 or ub >= 1 << 40 else ub + 1)
            else:
                dims.append(None)
        for n in reversed(dims or [None]):
            r = ("array", r, n)
    elif tag == "DW_TAG_subroutine_type":
        r = ("func",)
    else:
        r = ("void",)
    r = INTERN.setdefault(r, r)
    memo[off] = r
    return r


def display(dies, off, depth=0):
    """A C-like spelling of the declared type, with typedef names, for reports."""
    if off is None:
        return "void"
    d = dies.get(off)
    if d is None or depth > 8:
        return "?"
    tag = d.tag
    if tag in ("DW_TAG_typedef", "DW_TAG_base_type"):
        return _name(d.attrs.get("DW_AT_name", "?"))
    if tag in ("DW_TAG_const_type", "DW_TAG_volatile_type"):
        q = "const " if tag == "DW_TAG_const_type" else "volatile "
        inner = display(dies, _ref(d), depth + 1)
        return inner if inner.startswith(q) else q + inner
    if tag == "DW_TAG_pointer_type":
        return display(dies, _ref(d), depth + 1) + " *"
    if tag in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_enumeration_type"):
        kw = {"DW_TAG_structure_type": "struct", "DW_TAG_union_type": "union"}.get(tag, "enum")
        return f"{kw} {_name(d.attrs['DW_AT_name'])}" if "DW_AT_name" in d.attrs else f"{kw} {{...}}"
    if tag == "DW_TAG_array_type":
        dims = ""
        for s in d.children:
            if s.tag == "DW_TAG_subrange_type":
                ub = _num(s.attrs["DW_AT_upper_bound"]) if "DW_AT_upper_bound" in s.attrs else None
                dims += f"[{ub + 1}]" if ub is not None and 0 <= ub < 1 << 40 else "[]"
        return display(dies, _ref(d), depth + 1) + dims
    if tag == "DW_TAG_subroutine_type":
        return "fn()"
    return "?"


_OPT_FLAG = re.compile(r"(?:^|\s)-O(\S*)")


def optimization(producer):
    """The optimization level a compile unit's DW_AT_producer records ('0', '2', 's', ''...: the last -O switch;
    GCC records its switches by default, -grecord-gcc-switches), or None when it records none (clang)."""
    levels = _OPT_FLAG.findall(producer)
    return levels[-1] if levels else None


def read_variables(objects, readelf, wanted):
    """name -> [(object, type, display, is_definition)] for the external variables in `wanted`, and struct tag ->
    Counter of complete definitions (to complete a type another file left incomplete). Exits if an object's DWARF
    is of optimized code, which leaves out the declarations of what the optimizer removed (docs/port/OPTIMIZED.md)."""
    found = collections.defaultdict(list)
    tags = collections.defaultdict(collections.Counter)
    optimized = []
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([*dwarf_info_command(readelf), *batch], capture_output=True, text=True,
                             check=True).stdout
        for obj, dies in parse_readelf(out, batch[0] if len(batch) == 1 else None).items():
            memo = {}
            for off, d in dies.items():
                if d.tag == "DW_TAG_compile_unit":
                    level = optimization(_name(d.attrs.get("DW_AT_producer", "")))
                    if level not in (None, "0"):
                        optimized.append(f"{obj} (-O{level})")
                if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and "DW_AT_declaration" not in d.attrs \
                        and "DW_AT_name" in d.attrs:
                    tags[_name(d.attrs["DW_AT_name"])][canon(dies, off, memo)] += 1
                if d.tag != "DW_TAG_variable":
                    continue
                spec = dies.get(_ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
                src = spec or d
                if "DW_AT_external" not in src.attrs or "DW_AT_name" not in src.attrs:
                    continue
                name = _name(src.attrs["DW_AT_name"])
                if name not in wanted:
                    continue
                toff = _ref(d) if "DW_AT_type" in d.attrs else _ref(src)
                found[name].append((obj, canon(dies, toff, memo), display(dies, toff),
                                    "DW_AT_declaration" not in d.attrs))
    if optimized:
        sys.exit(f"gen_data: {len(optimized)} objects have the DWARF of optimized code, which leaves out the "
                 f"declarations the optimizer made unused; pass objects compiled with -O0 -g (types_objects.txt): "
                 + ", ".join(optimized[:3]))
    return found, tags


def complete(t, tags, depth=0):
    """Replace incomplete structs with the one complete definition other files give (most common if several)."""
    if depth > 6:
        return t
    if t[0] == "opaque" and t[1] in tags:
        return tags[t[1]].most_common(1)[0][0]
    if t[0] == "array":
        e = complete(t[1], tags, depth + 1)
        return t if e is t[1] else INTERN.setdefault(("array", e, t[2]), ("array", e, t[2]))
    return t


# ---------------------------------------------------------------- layouts

_LAYOUT = {}


def n64_layout(t):
    """(size or None, alignment) of a canonical type under the N64's o32 ABI."""
    r = _LAYOUT.get(t)
    if r is not None:
        return r
    k = t[0]
    if k == "base":
        n = N64_BASE.get(t[1], t[2])
        r = (n, n)
    elif k in ("ptr", "enum"):
        r = (4, 4) if k == "ptr" else (t[1], t[1])
    elif k in ("struct", "union"):
        off = size = 0
        al = 1
        for _, _, mt in t[3]:
            s, a = n64_layout(mt)
            s = s or 0
            al = max(al, a)
            if k == "struct":
                off = align(off, a) + s
            else:
                size = max(size, s)
        r = (align(off if k == "struct" else size, al), al)
    elif k == "array":
        s, a = n64_layout(t[1])
        r = (None if t[2] is None or s is None else s * t[2], a)
    else:
        r = (None, 1)
    _LAYOUT[t] = r
    return r


def host_size(t):
    k = t[0]
    if k == "base":
        return t[2]
    if k == "ptr":
        return HOST_PTR
    if k == "enum":
        return t[1]
    if k in ("struct", "union"):
        return t[2]
    if k == "array":
        s = host_size(t[1])
        return None if s is None or t[2] is None else s * t[2]
    return None


def base_kind(name, size):
    if name == "float":
        return K_F32
    if name in ("double", "long double"):
        return K_F64 if size == 8 else None
    n = N64_BASE.get(name)
    if n is None:
        return None
    if n == 4 and size == 8:
        return K_U32_64 if "unsigned" in name else K_S32_64
    return {1: K_U8, 2: K_U16, 4: K_U32, 8: K_U64}[n]


def merge(runs):
    out = []
    for r in runs:
        if out:
            p = out[-1]
            if p[0] == r[0] and p[4] == 1 and r[4] == 1 and r[2] == p[2] + p[1] * N64_ESZ[p[0]] \
                    and r[3] == p[3] + p[1] * esz_host(p[0]):
                out[-1] = (p[0], p[1] + r[1], p[2], p[3], 1, 0, 0)
                continue
        out.append(r)
    return out


def shift(r, dn, dh):
    return (r[0], r[1], r[2] + dn, r[3] + dh, r[4], r[5], r[6])


_PROG = {}
LAYOUT_ODDITIES = set()


def program(t):
    """Field program of a canonical type: runs (kind, count, n64 offset, host offset, rep, n64 stride, host
    stride), offsets relative to the start. Unknown parts (opaque, void) give no runs."""
    r = _PROG.get(t)
    if r is not None:
        return r
    k = t[0]
    if k == "base":
        kind = base_kind(t[1], t[2])
        r = [(kind, 1, 0, 0, 1, 0, 0)] if kind else []
    elif k == "ptr":
        r = [(K_PTR, 1, 0, 0, 1, 0, 0)]
    elif k == "enum":
        r = [({1: K_U8, 2: K_U16, 4: K_U32, 8: K_U64}[t[1]], 1, 0, 0, 1, 0, 0)]
    elif k == "struct":
        runs, off = [], 0
        for _, hoff, mt in t[3]:
            s, a = n64_layout(mt)
            off = align(off, a)
            runs += [shift(x, off, hoff) for x in program(mt)]
            off += s or 0
        r = merge(runs)
    elif k == "union":
        r = union_program(t)
    elif k == "array":
        r = replicate(program(t[1]), t[2] or 0, n64_layout(t[1])[0] or 0, host_size(t[1]) or 0)
    else:
        r = []
    _PROG[t] = r
    return r


BYTE_UNIONS = set()   # unions laid out as bytes (union_program), for the report


def union_program(t):
    """A union's runs. One with a member made only of bytes (a u8 array or a struct of u8s) that covers the whole
    union is the game's byte view of that storage: the colour unions ({ u8 c[4]; u32 rgba; }) are R, G, B, A in
    memory, and their words only copy those bytes or test them for equality, so they are copied as bytes, not
    swapped (docs/port/ENDIAN.md 4). Not when a member has a pointer or a long, whose host layout differs.
    Otherwise the largest member with the most non-byte elements sets the layout."""
    size = n64_layout(t)[0] or 0
    progs = [(m, program(m[2])) for m in t[3]]
    if size and not any(r[0] in (K_PTR, K_S32_64, K_U32_64) for _, p in progs for r in p):
        for m, p in progs:
            if p and all(r[0] == K_U8 for r in p) and sum(r[1] * r[4] for r in p) == size \
                    and (n64_layout(m[2])[0] or 0) == size:
                BYTE_UNIONS.add(t)
                return p
    best = max(progs, key=lambda mp: (n64_layout(mp[0][2])[0] or 0, sum(1 for x in mp[1] if x[0] != K_U8)),
               default=None)
    return best[1] if best else []


def replicate(e, n, sn, sh):
    if n == 0 or not e:
        return []
    if n == 1:
        return e
    if len(e) == 1:
        r = e[0]
        if r[4] == 1 and r[2] == 0 and r[3] == 0 and r[1] * N64_ESZ[r[0]] == sn and r[1] * esz_host(r[0]) == sh:
            return [(r[0], r[1] * n, 0, 0, 1, 0, 0)]
    if all(r[4] == 1 for r in e):
        return [(r[0], r[1], r[2], r[3], n, sn, sh) for r in e]
    out = []
    for i in range(n):
        out += [shift(r, i * sn, i * sh) for r in e]
    return out


def elements(runs, skip_u8=False):
    """(n64 offset, kind) of every element."""
    for kind, count, n64o, _, rep, ns, _ in runs:
        if skip_u8 and kind == K_U8:
            continue
        esz = N64_ESZ[kind]
        for i in range(rep):
            base = n64o + i * ns
            for j in range(count):
                yield base + j * esz, kind


def map_offset(runs, off):
    """Host offset of N64 offset `off` inside a symbol, through its runs (padding maps linearly)."""
    best = (0, 0)
    for kind, count, n64o, hosto, rep, ns, hs in runs:
        if off < n64o:
            continue
        en, eh = N64_ESZ[kind], esz_host(kind)
        i = min((off - n64o) // ns, rep - 1) if ns else 0
        within = off - n64o - i * ns
        j = within // en
        if j < count:
            return hosto + i * hs + j * eh + min(within - j * en, eh - 1)
        end_n = n64o + i * ns + count * en
        if end_n <= off and end_n >= best[0]:
            best = (end_n, hosto + i * hs + count * eh)
    return best[1] + (off - best[0])


def contains(t, kinds):
    if t[0] in kinds:
        return True
    if t[0] == "array":
        return contains(t[1], kinds)
    if t[0] in ("struct", "union"):
        return any(contains(m[2], kinds) for m in t[3])
    return False


# ---------------------------------------------------------------- symbols

class Sym:
    """One N64 data address and the names the game gives it."""

    def __init__(self, addr):
        self.addr, self.names, self.decls, self.images = addr, set(), [], set()
        self.sfsize = None       # N64 size from the symbol file or port/data/extents.txt
        self.defsize = None      # N64 size of the game C's (tentative) definition
        self.arena = self.bss = None
        self.end_marker = False
        self.t = self.disp = None
        self.runs = []
        self.n64_size = self.host_size = 0
        self.own = False
        self.parent = None       # own symbol this one lies inside
        self.parent_off = 0
        self.extent_by_gap = False
        self.cdef = False        # defined by the game C (listed for pointers, not filled)
        self.cdef_letter = None


def resolve_address(name, images, tables, symfile, extra):
    """(address, source, conflicts): the referencing images' symbol tables first, then the symbol file."""
    cands = []
    for img in ("game", "ovl_a", "ovl_b"):
        if img in images and name in tables[img]:
            cands.append((tables[img][name], img))
    if not cands or "boot" in images:
        if name in symfile:
            cands.append((symfile[name][0], "symbol file"))
        elif name in extra:
            cands.append((extra[name][0], "undefined_syms_auto"))
    if not cands:
        for img in ("game", "ovl_a", "ovl_b"):
            if name in tables[img]:
                cands.append((tables[img][name], img))
                break
    if not cands:
        a = n64syms.address_of(name, {})
        if a is not None:
            cands.append((a, "name"))
    if not cands:
        return None, None, []
    addrs = sorted({a for a, _ in cands})
    return cands[0][0], cands[0][1], (cands if len(addrs) > 1 else [])


def arena_of(addr, images):
    """(arena index, is BSS, is end marker) or None. Image B's data lies inside image A's code range, so an
    address there is B's only for names image A's C does not use alone."""
    for i, (img, lo, init, hi) in enumerate(n64syms.ARENAS):
        if lo <= addr <= hi and img == "ovl_b" and images <= {"ovl_a"}:
            return None
        if lo <= addr < hi:
            return i, addr >= init, False
        if addr == hi and img.startswith("ovl"):
            return i, True, True
    return None


def is_function_name(name, symfile):
    return name.startswith("func_") or (name in symfile and symfile[name][1] == "func")


class Decoupled:
    """A game data item the PC owns outright (port/data/decoupled.txt, docs/port/DECOUPLE.md): its N64 symbol is
    retired, with every name inside its N64 extent, and the PC symbol has storage in data.c and no N64 address."""

    def __init__(self, pc, n64, size, aliases, lo, retired, init=False, period=0):
        self.pc, self.n64, self.size, self.aliases, self.lo, self.retired = pc, n64, size, aliases, lo, retired
        self.t = self.disp = None
        self.host_size = 0
        # init=image: N64 data the image initializes, which the loader fills the storage's first elements from (the
        # runs and host bytes of that part), the rest continuing it with a period of `period` elements (host bytes).
        self.init, self.period = init, period
        self.init_runs, self.init_host, self.period_host = [], 0, 0


def load_decoupled(path, tables, symfile, refs, defs):
    """The rules of port/data/decoupled.txt: `PC_NAME N64_NAME N64_SIZE [ALIAS ...] [init=image [period=K]]`. Each
    N64 name, alias and symbol-table name in [address, address + size) is retired: a game object that still
    references or defines one is an error (it would reach the N64 address the PC no longer gives the data). The PC
    name must not be defined by the game C: data.c defines it. The range is BSS of the game or the boot segment or,
    with init=image, initialized data of one of them: on each load of that image (once) the loader fills the PC
    storage's first elements from the image's N64_SIZE bytes there, and the rest of the storage continues them with
    a period of K elements (element i is element i - K), or stays zero."""
    out, bad = [], []
    if not path.exists():
        return out
    for n, line in enumerate(path.read_text().splitlines(), 1):
        f = line.split("#", 1)[0].split()
        if not f:
            continue
        if len(f) < 3:
            sys.exit(f"{path}:{n}: expected PC_NAME N64_NAME N64_SIZE [ALIAS ...]")
        pc, n64, size = f[0], f[1], int(f[2], 0)
        aliases = [x for x in f[3:] if "=" not in x]
        opts = dict(x.split("=", 1) for x in f[3:] if "=" in x)
        if set(opts) - {"init", "period"} or opts.get("init", "image") != "image" or ("period" in opts
                                                                                        and "init" not in opts):
            sys.exit(f"{path}:{n}: the options are init=image and, with it, period=K")
        init, period = "init" in opts, int(opts.get("period", "0"), 0)
        cands = [t[n64] for t in tables.values() if n64 in t] + ([symfile[n64][0]] if n64 in symfile else [])
        if not cands and re.fullmatch(r"D_[0-9A-F]{8}", n64):
            cands = [int(n64[2:], 16)]
        if not cands or len(set(cands)) != 1:
            sys.exit(f"{path}:{n}: no single N64 address for {n64}")
        lo = cands[0]
        # Data of an image that stays loaded only (a runtime image's load would not clear or fill it again): BSS, or
        # with init=image initialized data, which the loader fills from the image (unconverted otherwise).
        w, w_end = arena_of(lo, {"game"}), arena_of(lo + size - 1, {"game"})
        if not w or w[1] == init or w_end != w or n64syms.ARENAS[w[0]][0] not in ("boot", "game"):
            sys.exit(f"{path}:{n}: {n64} (0x{lo:08X}, {size} B) is not inside the game's or the boot segment's "
                     + ("initialized data (init=image)" if init else "BSS (initialized data needs init=image)"))
        inside = {name for t in tables.values() for name, x in t.items() if lo <= x < lo + size}
        inside |= {name for name, (x, kind, _) in symfile.items() if kind == "data" and lo <= x < lo + size}
        retired = sorted({n64, *aliases} | inside)
        for name in retired:
            users = sorted(Path(o).name for o in refs.get(name, ())) + sorted(Path(o).name for o, _, _ in
                                                                                 defs.get(name, ()))
            if users:
                bad.append(f"{name} (retired for {pc}, docs/port/DECOUPLE.md): {', '.join(users[:4])}"
                           + (" ..." if len(users) > 4 else ""))
        if defs.get(pc):
            bad.append(f"{pc}: defined by {', '.join(sorted(Path(o).name for o, _, _ in defs[pc]))}; data.c defines it")
        out.append(Decoupled(pc, n64, size, aliases, lo, retired, init, period))
        out[-1].image = n64syms.ARENAS[w[0]][0]
    if bad:
        sys.exit("gen_data: game objects still reach retired N64 names (port/data/decoupled.txt):\n  "
                 + "\n  ".join(bad))
    return out


def decoupled_type(d, decls, tags):
    """The PC symbol's one type: every reader's declaration must be the same (the shared PC declaration)."""
    types = collections.Counter()
    disp = {}
    for obj, t, dsp, _ in decls:
        t = complete(t, tags)
        types[t] += 1
        disp.setdefault(t, (dsp, Path(obj).name))
    if not types:
        sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): no game object declares it")
    if len(types) > 1:
        sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): the readers disagree on its type: "
                 + "; ".join(f"{dsp} ({obj}, {n} files)" for t, n in types.items() for dsp, obj in [disp[t]]))
    d.t = next(iter(types))
    d.disp = disp[d.t][0]
    d.host_size = host_size(d.t) or 0
    if d.t[0] == "array" and d.t[2] is None or not d.host_size:
        sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): {d.disp} has no size")
    if d.init:
        # The part the image fills: the type, or an array's first N64_SIZE bytes of elements (the PC's is longer).
        part, n = d.t, 1
        if d.t[0] == "array":
            es = n64_layout(d.t[1])[0] or 0
            n = d.size // es if es and d.size % es == 0 else 0
            if not 0 < n <= d.t[2]:
                sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): {d.size} B is not elements of {d.disp}")
            part = INTERN.setdefault(("array", d.t[1], n), ("array", d.t[1], n))
        elif n64_layout(d.t)[0] != d.size or d.period:
            sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): {d.disp} is not {d.size} B on the N64")
        if not 0 <= d.period <= n:
            sys.exit(f"gen_data: {d.pc} (port/data/decoupled.txt): period {d.period} is more than its {n} N64 elements")
        d.init_runs, d.init_host = program(part), host_size(part) or 0
        d.period_host = d.period * host_size(d.t[1]) if d.period else 0


# ---------------------------------------------------------------- reserved window ranges (port/data/reserved.txt)
RESERVED = []   # the Reserved items generate() placed, for emit() and the report


class Reserved:
    """PC data that must sit in the RDRAM window (something keeps its address as a 32-bit N64 address) and has no N64
    symbol: data.c defines it absolute at an address inside a range a decoupled item freed (docs/port/MORECARS.md 8),
    at the size its one declaration gives; the loader's tables don't have it, and the arena's BSS zeroes it."""

    def __init__(self, pc, lo, where):
        self.pc, self.lo, self.where = pc, lo, where
        self.t = self.disp = None
        self.size = 0


def load_reserved(path, defs):
    """The rules of port/data/reserved.txt: `PC_NAME ADDRESS`. The PC name must not be defined by the game C."""
    out = []
    if not path.exists():
        return out
    for n, line in enumerate(path.read_text().splitlines(), 1):
        f = line.split("#", 1)[0].split()
        if not f:
            continue
        if len(f) != 2:
            sys.exit(f"{path}:{n}: expected PC_NAME ADDRESS")
        if defs.get(f[0]):
            sys.exit(f"{path}:{n}: {f[0]} is defined by {', '.join(sorted(Path(o).name for o, _, _ in defs[f[0]]))}; "
                     f"data.c defines it")
        out.append(Reserved(f[0], int(f[1], 0), f"{path.name}:{n}"))
    return out


def reserved_place(r, decls, tags, decoupled, syms, others):
    """A reservation's one type (every reader's declaration the same, laid out alike on the N64 and the host: the
    window holds it as the N64 would) and its range: inside the N64 range of a decoupled item (window bytes the PC no
    longer gives any N64 symbol), over no placed symbol's extent and no other reservation."""
    types, disp = collections.Counter(), {}
    for obj, t, dsp, _ in decls:
        t = complete(t, tags)
        types[t] += 1
        disp.setdefault(t, dsp)
    if len(types) != 1:
        sys.exit(f"gen_data: {r.pc} ({r.where}): " + ("no game object declares it" if not types else
                 "the readers disagree on its type: " + "; ".join(disp[t] for t in types)))
    r.t = next(iter(types))
    r.disp = disp[r.t]
    r.size = host_size(r.t) or 0
    runs = program(r.t)
    if not r.size or n64_layout(r.t)[0] != r.size or \
            any(x[0] in (K_PTR, K_S32_64, K_U32_64) or x[2] != x[3] or x[5] != x[6] for x in runs):
        sys.exit(f"gen_data: {r.pc} ({r.where}): {r.disp} needs one layout on the N64 and the host, with a size")
    hi = r.lo + r.size
    w = arena_of(r.lo, {"game"})
    if not w or not w[1] or arena_of(hi - 1, {"game"}) != w:   # BSS: the loader zeroes it, copies nothing into it
        sys.exit(f"gen_data: {r.pc} ({r.where}): 0x{r.lo:08X}-0x{hi:08X} is not inside the game's BSS")
    if not any(d.lo <= r.lo and hi <= d.lo + d.size for d in decoupled):
        sys.exit(f"gen_data: {r.pc} ({r.where}): 0x{r.lo:08X}-0x{hi:08X} is not inside a range a decoupled item "
                 f"freed (port/data/decoupled.txt)")
    for s in syms.values():
        if not s.cdef and not s.end_marker and s.addr < hi and r.lo < s.addr + max(s.n64_size, 1):
            sys.exit(f"gen_data: {r.pc} ({r.where}): 0x{r.lo:08X}-0x{hi:08X} overlaps {sorted(s.names)[0]} "
                     f"(0x{s.addr:08X}, {s.n64_size} B)")
    for o in others:
        if o.lo < hi and r.lo < o.lo + o.size:
            sys.exit(f"gen_data: {r.pc} ({r.where}): overlaps {o.pc} ({o.where})")


def reserved_lines(layout):
    """data.c's lines for the reservations (absolute symbols, as the arenas' are), and their layout entries."""
    layout["reserved"] = [[r.pc, r.lo, r.size] for r in RESERVED]
    if not RESERVED:
        return []
    L = ["", "/* PC data in the RDRAM window, in ranges decoupled items freed (port/data/reserved.txt,",
         " * docs/port/MORECARS.md 8): absolute, zeroed with the arena's BSS, not in the loader's tables. */", "__asm__("]
    for r in RESERVED:
        L.append(f"    \".globl \" N64_SYM(\"{r.pc}\") \"\\n.set \" N64_SYM(\"{r.pc}\") \", 0x{r.lo:08X}\""
                 f" PC_SIZE(\"{r.pc}\", {r.size}) \"\\n\"   /* {r.disp} */")
    return L + [");"]


def check_reserved(layout, addr, size, letter, pe):
    """--check: each reservation absolute at its address (and its size, on ELF)."""
    bad = []
    for name, lo, n in layout.get("reserved", []):
        if name not in addr:
            bad.append(f"reserved {name}: not in the executable")
        elif letter[name] not in "Aa" or addr[name] != lo:
            bad.append(f"reserved {name}: {letter[name]} 0x{addr[name]:X}, expected absolute 0x{lo:08X}")
        elif not pe and name in size and size[name] != n:
            bad.append(f"reserved {name}: size {size[name]} != {n}")
    return bad


def score(t, runs):
    """Most informative declaration first: struct over pointer over words over bytes; between two structs, an
    opaque one (no union, bytes only: `struct { u8 pad[164]; }`, an m2c record) after one with wider elements. A
    tie with the real record must not leave its words in the ROM's byte order: the cheat codes' button masks
    (`D_803B3460`: `Egg` in cheatscr.c, `Rec164` in func_8038FF90.c) were swapped by nobody (docs/port/QA.md).
    A union whose bytes cover it (the colours) is not opaque: its bytes are the layout (DATA.md 2)."""
    has_struct = contains(t, ("struct", "union"))
    has_ptr = any(r[0] == K_PTR for r in runs)
    width = max((N64_ESZ[r[0]] for r in runs), default=0)
    rank = 5 if has_struct and has_ptr else 4 if has_struct else 3 if has_ptr else 2 if width > 1 else \
        1 if runs else 0
    opaque = has_struct and width <= 1 and not contains(t, ("union",))
    return rank, 0 if opaque else 1, n64_layout(t)[0] or 0


def signature(runs, limit):
    """Element classes below `limit`, for conflict checks: bytes are left out; floats count as words."""
    cls = {K_F32: K_U32, K_F64: K_U64, K_S32_64: K_U32, K_U32_64: K_U32}
    return tuple((o, cls.get(k, k)) for o, k in elements(runs, True) if o < limit)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--objects-list", type=Path, help="the game objects, compiled -O0, one per line")
    ap.add_argument("--link-objects-list", type=Path,
                    help="the game objects the executable links, if not those (an optimized build)")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--readelf", default="readelf", help="readelf, or objdump for PE objects")
    ap.add_argument("--root", type=Path, default=n64syms.ROOT)
    ap.add_argument("--weak-script", type=Path,
                    help="PE: also write the linker script that resolves data.c's weak references")
    ap.add_argument("--check", type=Path, help="linked executable to verify against --layout")
    ap.add_argument("--layout", type=Path)
    ap.add_argument("--test-tables", type=Path, help="an object of test declarations: write pc_data_test's header")
    ap.add_argument("--pe", action="store_true", help="--check: the executable is PE (Windows)")
    a = ap.parse_args()
    if a.check:
        sys.exit(check(a.check, a.layout, a.nm, a.pe))
    if a.test_tables:
        if not a.out:
            ap.error("--test-tables needs --out")
        return test_tables(a)
    if not a.objects_list or not a.out:
        ap.error("--objects-list and --out are required")
    generate(a)


def generate(a):
    root = a.root
    objects = a.objects_list.read_text().split()
    nmtab = n64syms.nm_objects(objects, a.nm)
    obj_image = {o: n64syms.object_image(o) for o in objects}
    defs = collections.defaultdict(list)          # name -> [(object, letter, size)]
    refs = collections.defaultdict(set)           # name -> {object}
    for o, (d, r) in nmtab.items():
        for n, (letter, size) in d.items():
            defs[n].append((o, letter, size))
        for n in r:
            refs[n].add(o)
    undefined = {n for n in refs if n not in defs}
    commons = {n for n, d in defs.items() if all(l == "C" for _, l, _ in d)
               and any(obj_image.get(o) in ("game", "ovl_a", "ovl_b") for o, _, _ in d)}
    symfile = n64syms.load_symbols(root / "symbol_addrs.us.txt")
    extents = {}
    for line in (root / "port" / "data" / "extents.txt").read_text().splitlines():
        f = line.split("#", 1)[0].split()
        if len(f) == 2:
            extents[f[0]] = int(f[1], 0)
    extra = n64syms.load_symbols(root / "undefined_syms_auto.us.txt")
    tables = {img: n64syms.load_image_symbols(img, root) for img in ("game", "ovl_a", "ovl_b")}
    report = collections.defaultdict(list)
    decoupled = load_decoupled(root / "port" / "data" / "decoupled.txt", tables, symfile, refs, defs)
    dec_pc = {d.pc for d in decoupled}
    reserved = load_reserved(root / "port" / "data" / "reserved.txt", defs)
    dec_pc |= {r.pc for r in reserved}   # placed as decoupled data is: not by an N64 address

    # Addresses of what we place (undefined data, commons) and of the game's own data (for pointers).
    syms, skipped, addr_conflicts = {}, collections.defaultdict(list), []
    name_addr = {}
    for name in sorted(undefined | commons | {n for n, d in defs.items() if any(l in "DdBbRrGgSs" for _, l, _ in d)}):
        images = {obj_image.get(o) for o in refs.get(name, ())} | {obj_image.get(o) for o, _, _ in defs.get(name, ())}
        images.discard(None)
        addr, source, conf = resolve_address(name, images, tables, symfile, extra)
        if conf:
            addr_conflicts.append((name, conf))
        placed = name in undefined or name in commons
        if is_function_name(name, symfile) or name in PC_EXPORTS or name in dec_pc:
            continue
        if addr is None:
            if name in undefined:
                skipped["no address"].append(name)
            continue
        where = arena_of(addr, images)
        if where is None:
            region = n64syms.region_of(addr)
            in_code = any(lo <= addr < hi for lo, hi in n64syms.CODE.values())
            if name in undefined and (not in_code or name.startswith("D_")):
                if region == "overlay" and addr < n64syms.CODE["ovl_a"][1]:
                    region = "overlay code"
                skipped[region].append((name, addr))
            continue
        name_addr[name] = addr
        s = syms.setdefault(addr, Sym(addr))
        s.names.add(name)
        s.images |= images
        s.arena, s.bss, s.end_marker = where
        if not placed:
            s.cdef = True
            s.cdef_letter = defs[name][0][1]
        if name in symfile and symfile[name][2]:
            s.sfsize = max(s.sfsize or 0, symfile[name][2])
        elif name in extents:
            s.sfsize = max(s.sfsize or 0, extents[name])
    # A C-defined name and a placed name at one address: the C definition wins; the placed name aliases it.
    cdef_with_placed = [s for s in syms.values() if s.cdef and any(n in undefined or n in commons for n in s.names)]
    for s in cdef_with_placed:
        report["C definition shares an address with an undefined name (the name stays undefined)"].append(
            f"0x{s.addr:08X} {' '.join(sorted(s.names))}")
        for n in list(s.names):
            if n in undefined or n in commons:
                s.names.discard(n)
                skipped["shares a C definition's address"].append((n, s.addr))

    # Types from DWARF.
    wanted = {n for s in syms.values() for n in s.names} | dec_pc
    found, tags = read_variables(objects, a.readelf, wanted)
    for d in decoupled:
        decoupled_type(d, found.get(d.pc, ()), tags)
    for s in syms.values():
        for n in sorted(s.names):
            for obj, t, disp, is_def in found.get(n, ()):
                if s.cdef and not is_def:
                    continue
                t = complete(t, tags)
                s.decls.append((n, obj, t, disp))
                if is_def and not s.cdef and n64_layout(t)[0]:
                    s.defsize = max(s.defsize or 0, n64_layout(t)[0])

    # Extents: symbol-file (or extents.txt) size, else the game C's definition, else the largest complete
    # declaration or the gap to the next address.
    known = collections.defaultdict(set)
    for addr, s in syms.items():
        known[s.arena].add(addr)
    for name, (addr, kind, _) in symfile.items():
        w = arena_of(addr, {"game"})
        if w and kind == "data":
            known[w[0]].add(addr)
    for i, (_, lo, init, hi) in enumerate(n64syms.ARENAS):
        known[i] |= {init, hi}
    for d in decoupled:   # a retired range still ends the gap before it, as its symbol did
        w = arena_of(d.lo, {"game", "ovl_a", "ovl_b"})
        if w:
            known[w[0]].add(d.lo)
    known = {i: sorted(v) for i, v in known.items()}

    def gap(s):
        k = known[s.arena]
        j = bisect.bisect_right(k, s.addr)
        return (k[j] if j < len(k) else n64syms.ARENAS[s.arena][3]) - s.addr

    pointer_conflicts, width_conflicts, untyped = [], [], []
    selfcheck, size_mismatch, past_gap, crossing, cut = [], [], [], [], []
    # Addresses the image symbol tables know (table starts the asm or the compiled C refers to): an incomplete
    # array whose gap extent crosses one may be swallowing a table no C declares.
    soft = collections.defaultdict(set)
    for img, tab in tables.items():
        for x in tab.values():
            w = arena_of(x, {img})
            if w and not w[2]:
                soft[w[0]].add(x)
    soft = {i: sorted(v) for i, v in soft.items()}
    for s in sorted(syms.values(), key=lambda x: x.addr):
        if s.end_marker:
            continue
        sized = [n64_layout(t)[0] for _, _, t, _ in s.decls if n64_layout(t)[0]]
        # A known size: the symbol file's (or extents.txt's), else the game C's own definition of the symbol (a
        # tentative definition: what the linker compares data.c's size with). Incomplete declarations (T x[],
        # struct tags no file completes) take it, else the gap to the next known address, or the largest
        # complete declaration if that is longer. Arrays declared longer than a known size are cut to it.
        bound = s.sfsize or s.defsize
        extent = bound or max([gap(s)] + sized)
        cands = {}
        for n, obj, t, disp in s.decls:
            tt = t
            es = n64_layout(tt[1])[0] if tt[0] == "array" else None
            if tt[0] == "array" and tt[2] is None:
                if es:
                    tt = INTERN.setdefault(("array", tt[1], max(1, extent // es)), ("array", tt[1], max(1, extent // es)))
            elif tt[0] == "array" and bound and es and tt[2] * es > bound:
                tt = INTERN.setdefault(("array", tt[1], max(1, bound // es)), ("array", tt[1], max(1, bound // es)))
                cut.append((s, disp, Path(obj).name.replace(".c.o", ".c"), t[2] * es, bound))
            elif tt[0] in ("opaque", "void", "func"):
                tt = ("array", ("base", "unsigned char", 1), extent)
            c = cands.setdefault(tt, [0, disp, []])
            c[0] += 1
            c[2].append((n, obj))
        if not cands:
            untyped.append(s)
            tt = ("array", ("base", "unsigned char", 1), extent)
            cands[tt] = [0, "u8[] (no declaration with a type)", []]
        # With a known size, declarations of exactly that size first, then larger ones (a view that runs over the
        # next object), then smaller (a view of part of it); then the most informative.
        def fit(t):
            n = n64_layout(t)[0] or 0
            return 2 if not bound or n == bound else 1 if n > bound else 0
        ranked = sorted(cands.items(), key=lambda kv: (fit(kv[0]), score(kv[0], program(kv[0])), kv[1][0], kv[1][1],
                                                       repr(kv[0])), reverse=True)
        s.t, (cnt, s.disp, _) = ranked[0]
        s.extent_by_gap = not bound and s.t not in [t for _, _, t, _ in s.decls]
        s.runs = program(s.t)
        s.n64_size = n64_layout(s.t)[0] or 0
        s.host_size = host_size(s.t) or 0
        s.own = any(r[0] in (K_PTR, K_S32_64, K_U32_64) or r[2] != r[3] or r[5] != r[6] for r in s.runs) \
            or s.n64_size != s.host_size
        if s.own and not s.cdef and not any(r[0] in (K_PTR, K_S32_64, K_U32_64) for r in s.runs):
            LAYOUT_ODDITIES.add(f"0x{s.addr:08X} {sorted(s.names)[0]}: {s.disp}")
        # Self-checks: the program stays inside both sizes; the N64 size agrees with the symbol file, and
        # (for information) whether it runs past the next known address.
        for kind, count, n64o, hosto, rep, ns, hs in s.runs:
            end_n = n64o + (rep - 1) * ns + count * N64_ESZ[kind]
            end_h = hosto + (rep - 1) * hs + count * esz_host(kind)
            if end_n > s.n64_size or end_h > s.host_size:
                selfcheck.append(f"0x{s.addr:08X} {sorted(s.names)[0]}: run {KIND_NAMES[kind]} ends at N64 "
                                 f"{end_n}/{s.n64_size}, host {end_h}/{s.host_size}")
        if bound and s.n64_size != bound:
            size_mismatch.append(f"0x{s.addr:08X} {sorted(s.names)[0]}: {s.disp} is {s.n64_size} B on the N64, "
                                 f"{'the symbol file says' if s.sfsize else 'its C definition'} {bound}")
        if s.extent_by_gap and not s.cdef and any(r[0] != K_U8 for r in s.runs):
            inside = [x for x in soft.get(s.arena, ()) if s.addr < x < s.addr + s.n64_size and x not in syms]
            if inside:
                crossing.append(f"0x{s.addr:08X} {sorted(s.names)[0]}: {s.disp}, {s.n64_size} B by gap, crosses "
                                f"{len(inside)}: " + ", ".join(f"0x{x:08X}" for x in inside[:4])
                                + (" ..." if len(inside) > 4 else ""))
        if s.n64_size > gap(s) and not s.cdef:
            past_gap.append(f"0x{s.addr:08X} {sorted(s.names)[0]}: {s.disp} {s.n64_size} B, next known address "
                            f"+0x{gap(s):X}")
        # Conflicts with the other declarations.
        if len(ranked) > 1:
            mine = signature(s.runs, s.n64_size)
            for t2, (cnt2, disp2, who) in ranked[1:]:
                lim = min(s.n64_size, n64_layout(t2)[0] or 0)
                p1 = {o for o, k in mine if k == K_PTR and o < lim}
                other = signature(program(t2), lim)
                p2 = {o for o, k in other if k == K_PTR}
                files = sorted({Path(o).name.replace(".c.o", ".c") for _, o in who})
                line = (f"0x{s.addr:08X} {'/'.join(sorted(s.names))}: chose {s.disp} ({cnt}), "
                        f"also {disp2} ({cnt2}: {', '.join(files[:3])}{' ...' if len(files) > 3 else ''})")
                if p1 != p2:
                    pointer_conflicts.append(line)
                elif tuple(x for x in mine if x[0] < lim) != other:
                    width_conflicts.append(line)

    # Own storage swallows the symbols inside its N64 extent. One whose own declaration needs host storage (a
    # pointer or a long) where the parent's layout has none gets a host slot the parent sized for something
    # else, so it overlaps its neighbours there: usually a sign that the parent's extent is too long (player_array
    # ran over the globals after it). Listed.
    ordered = sorted((s for s in syms.values() if not s.end_marker), key=lambda x: x.addr)
    overlaps, inside_conflicts = [], []
    for s in ordered:
        if s.parent is not None or not s.own or s.cdef:
            continue
        parent_kinds = None
        for u in ordered:
            if u is s or u.addr < s.addr or u.addr >= s.addr + max(s.n64_size, 1) or u.arena != s.arena:
                continue
            if u.cdef:
                overlaps.append(f"0x{u.addr:08X} {sorted(u.names)[0]} (C-defined) inside own 0x{s.addr:08X} "
                                f"{sorted(s.names)[0]}")
                continue
            wide_kinds = (K_PTR, K_S32_64, K_U32_64)   # 4 bytes on the N64, 8 here
            if parent_kinds is None:
                parent_kinds = dict(elements(s.runs))
            off = u.addr - s.addr
            bad = [o for o, k in elements(u.runs) if k in wide_kinds and parent_kinds.get(off + o) not in wide_kinds]
            if bad or off + u.n64_size > s.n64_size:
                inside_conflicts.append(
                    f"0x{u.addr:08X} {sorted(u.names)[0]} ({u.disp}) in own 0x{s.addr:08X} {sorted(s.names)[0]}"
                    f"+0x{off:X} ({s.disp}, {s.n64_size} B): "
                    + (f"{len(bad)} of its pointers/longs not in the parent's layout" if bad
                       else f"ends at +0x{off + u.n64_size:X}"))
            u.parent, u.parent_off, u.own = s, map_offset(s.runs, u.addr - s.addr), False
    # An arena symbol whose extent runs into own storage, or two own symbols that overlap.
    own_list = [s for s in ordered if s.own and s.parent is None and not s.cdef]
    for s in ordered:
        if s.parent is None and not s.cdef:
            for o in own_list:
                if o is not s and s.addr < o.addr < s.addr + s.n64_size:
                    overlaps.append(f"0x{s.addr:08X} {sorted(s.names)[0]} ({s.disp}, {s.n64_size} B) runs into "
                                    f"own 0x{o.addr:08X} {sorted(o.names)[0]}")

    # Arena runs: every element of every placed symbol, own ones too (their pointers swap as words there).
    arena_runs = {}
    elem_conflicts = []
    for i, (img, lo, init, hi) in enumerate(n64syms.ARENAS):
        members = [s for s in ordered if s.arena == i and s.parent is None and not s.cdef and not s.bss]
        arena_runs[i], conflicts = arena_elements(members, lo, init)
        elem_conflicts += conflicts

    # Functions by address, for function pointers.
    funcs, func_clash = {}, []
    defined_in = {n: {obj_image.get(o) for o, l, _ in d if l in "TtWw"} for n, d in defs.items()}
    entries = []
    for img in ("game", "ovl_a", "ovl_b"):
        entries += [(addr, name, img) for addr, name in n64syms.load_layout_functions(img, root)]
    static = json.loads((root / "manifest" / "static.json").read_text())
    for name, info in static["asm"].items():
        entries.append((int(info["vaddr"], 16), name, "boot"))
    for name, (addr, kind, _) in symfile.items():
        if kind == "func" and n64syms.CODE["boot"][0] <= addr < n64syms.CODE["boot"][1]:
            entries.append((addr, name, "boot"))
    for addr, name, img in entries:
        host = IMAGE_RENAMED.get((img, name), RENAMED.get(name, name))
        where = defined_in.get(host)
        if where and img not in where:
            func_clash.append(f"{img} 0x{addr:08X} {name}: defined only by {', '.join(sorted(where))}")
            continue
        key = (addr, img)
        prev = funcs.get(key)
        if prev is None or (not defined_in.get(prev) and where):
            funcs[key] = host

    # Thunks: names the C calls that are another label of a defined function.
    by_addr = collections.defaultdict(set)
    for (addr, img), host in funcs.items():
        if defined_in.get(host):
            by_addr[addr].add((img, host))
    thunks, func_aliases_left = [], []
    # rom_symbol_aliases.ld: the boot C's historical libultra labels, linked by the libultra layer.
    rom_aliases = set(re.findall(r"^\s*(\w+)\s*=\s*\w+\s*;", (root / "rom_symbol_aliases.ld").read_text(), re.M))
    for name in sorted(undefined):
        if name in name_addr or name in rom_aliases or name.startswith("D_"):
            continue
        images = {obj_image.get(o) for o in refs[name]} - {None}
        if images <= {"boot"}:
            continue
        addr, source, _ = resolve_address(name, images, tables, symfile, extra)
        if addr is None or name in symfile or source in ("name", None):
            continue
        targets = sorted(h for img, h in by_addr.get(addr, ()) if img in images or img in ("game", "boot"))
        if targets:
            thunks.append((name, targets[0], addr))
        elif any(lo <= addr < hi for lo, hi in n64syms.CODE.values()):
            others = sorted({n for (ad, _), n in funcs.items() if ad == addr})
            func_aliases_left.append(f"{name} = 0x{addr:08X} ({'/'.join(others) or 'no name'}: no C yet)")

    # The runtime images' C data as the executable has it: an optimized object may have fewer statics.
    linked = a.link_objects_list.read_text().split() if a.link_objects_list else objects
    ovl_data = {img: [(Path(o).name.replace('.c.obj', '.c').replace('.c.o', '.c'), n, k) for o in linked
                      if n64syms.object_image(o) == img for n, k in image_data(o, a.nm)] for img in OVL_RANGES}
    syms_by_name = {n: s for s in ordered for n in s.names}
    reach = reach_rules(root / "port" / "data" / "reached.txt", tags, syms_by_name, ordered)
    for r in reserved:
        reserved_place(r, found.get(r.pc, ()), tags, decoupled, syms, RESERVED)
        RESERVED.append(r)
    emit(a, syms, ordered, arena_runs, funcs, defined_in, thunks, nmtab, obj_image, ovl_data, reach, decoupled)

    # Report.
    own = [s for s in ordered if s.own and s.parent is None and not s.cdef]
    arena_syms = [s for s in ordered if not s.own and s.parent is None and not s.cdef]
    inside = [s for s in ordered if s.parent is not None]
    cdefs = [s for s in ordered if s.cdef]
    n_names = sum(len(s.names) for s in syms.values() if not s.cdef)
    L = ["gen_data layout report (tools/port/gen_data.py). Regenerated by every build.", "",
         f"Placed: {n_names} names at {len(syms) - len(cdefs)} addresses: {len(arena_syms)} in N64-layout arenas, "
         f"{len(own)} with their own storage, {len(inside)} inside own storage, "
         f"{sum(len(s.names) for s in syms.values() if s.end_marker)} end markers.",
         f"Initialized (filled from the ROM): {sum(1 for s in ordered if not s.cdef and not s.bss)}; "
         f"BSS (zeroed): {sum(1 for s in ordered if not s.cdef and s.bss)}.",
         f"Game-C data listed for pointers: {len(cdefs)}. Functions by address: {len(funcs)} "
         f"({sum(1 for h in funcs.values() if defined_in.get(h))} with C). Thunks: {len(thunks)}.",
         f"Runs: {sum(len(s.runs) for s in own) + sum(len(r) for r in arena_runs.values())}.", ""]

    def section(title, rows):
        L.append(f"## {title} ({len(rows)})")
        L.extend("  " + r for r in rows)
        L.append("")

    section("Own storage (pointers or a host layout that differs)",
            [f"0x{s.addr:08X} {'/'.join(sorted(s.names))}: {s.disp}, N64 {s.n64_size} B, host {s.host_size} B"
             f"{' (bss)' if s.bss else ''}{' (extent by gap)' if s.extent_by_gap else ''}" for s in own])
    section("Inside own storage", [f"0x{s.addr:08X} {'/'.join(sorted(s.names))} = {sorted(s.parent.names)[0]} + "
                                   f"0x{s.parent_off:X}" for s in inside])
    section("Pointer conflicts: files disagree on where pointers are (agent D)", pointer_conflicts)
    section("Width conflicts: files disagree on element sizes", width_conflicts)
    groups = [(g, OVERLAPS_REVIEWED.get((sorted(g[0].names)[0], sorted(g[3].names)[0])))
              for g in group_conflicts(elem_conflicts)]
    section("Arena element conflicts: overlapping declarations at other widths, bytes included (the first in rank "
            "kept; a size in the declaration is the fix)",
            [f"0x{g[0].addr + g[1]:08X} {fmt_conflict(g)}" for g, why in groups if not why])
    section("Arena element conflicts read and judged (OVERLAPS_REVIEWED)",
            [f"0x{g[0].addr + g[1]:08X} {fmt_conflict(g)}: {why}" for g, why in groups if why])
    section("Unions copied as bytes: a member made of bytes covers them (docs/port/ENDIAN.md 4)",
            [f"0x{s.addr:08X} {'/'.join(sorted(s.names))}: {s.disp} ({', '.join(sorted(u))}){' (bss)' if s.bss else ''}"
             for s in ordered if not s.cdef for u in [byte_unions_in(s.t)] if u])
    section("Self-check failures (runs outside the symbol)", selfcheck)
    section("N64 size differs from the symbol file, extents.txt or the C definition", size_mismatch)
    cut_files = collections.defaultdict(set)
    for s, disp, obj, n, bound in cut:
        cut_files[(s.addr, sorted(s.names)[0], disp, n, bound)].add(obj)
    section("Array declarations longer than the known size (cut to it)",
            [f"0x{ad:08X} {name}: {disp} ({n} B) in {len(f)} file{'s' if len(f) > 1 else ''} "
             f"({', '.join(sorted(f)[:3])}{' ...' if len(f) > 3 else ''}), size {bound} B"
             for (ad, name, disp, n, bound), f in sorted(cut_files.items())])
    section("Inside own storage, but its own declaration has pointers or a long where the storage has none, or "
            "runs past its end", inside_conflicts)
    section("Declared N64 size runs past the next known address (nested names or mistyped sizes)", past_gap)
    section("Gap extents crossing symbol-table addresses (give the declaration a size, or port/data/extents.txt)",
            crossing)
    section("Overlaps", overlaps)
    section("Decoupled: game data the PC owns, with no N64 address (port/data/decoupled.txt, docs/port/DECOUPLE.md)",
            [f"{d.pc}: {d.disp}, host {d.host_size} B in data.c"
             + (f", its first {d.init_host} B filled from the image (initialized data)"
                + (f", the rest continued with a period of {d.period}" if d.period else "") if d.init else "")
             + f"; retired {d.n64} at 0x{d.lo:08X}, {d.size} B, and every name inside it: {', '.join(d.retired)}"
             for d in decoupled])
    section("Reserved: PC data in the RDRAM window, in ranges decoupled items freed (port/data/reserved.txt, "
            "docs/port/MORECARS.md 8)",
            [f"0x{r.lo:08X}-0x{r.lo + r.size:08X} {r.pc}: {r.disp}, {r.size} B ({r.where})" for r in RESERVED])
    section("Layouts that differ without pointers or long (check for packing)", sorted(LAYOUT_ODDITIES))
    section("No declaration with a type (filled as bytes)", [f"0x{s.addr:08X} {'/'.join(sorted(s.names))}"
                                                             for s in untyped])
    section("Address conflicts between symbol tables (first used)",
            [f"{n}: " + ", ".join(f"{src} 0x{ad:08X}" for ad, src in c) for n, c in addr_conflicts])
    section("Function thunks", [f"{n} -> {t} (0x{ad:08X})" for n, t, ad in thunks])
    section("Function names left (alias of a function with no C yet)", func_aliases_left)
    section("Function addresses skipped (name defined by the other image)", func_clash)
    for k, v in sorted(skipped.items()):
        section(f"Not placed: {k}", [x if isinstance(x, str) else f"{x[0]} 0x{x[1]:08X}" for x in v])
    for k, v in report.items():
        section(k, v)
    section("Runtime-image C data, put back on each load of its image (its C range, port/data/overlays.ld)",
            sorted(f"{img} {f}: {n}{'' if k in 'db' else ' (global)'}" for img, v in ovl_data.items()
                   for f, n, k in v if k in "dbDB"))
    section("Runtime-image C data that is read-only (never changes)",
            sorted(f"{img} {f}: {n}" for img, v in ovl_data.items() for f, n, k in v if k in "rR"))
    (a.out.parent / "data_layout.txt").write_text("\n".join(L) + "\n")
    # Every placed symbol, one per line: address, storage, N64 and host size, kinds, chosen type, names.
    rows = ["# N64 address\tstorage\tN64 size\thost size\tkinds\ttype\tnames"]
    for s in sorted(syms.values(), key=lambda x: x.addr):
        where = ("C-defined" if s.cdef else "end marker" if s.end_marker else
                 f"in {sorted(s.parent.names)[0]}+0x{s.parent_off:X}" if s.parent else
                 "own" if s.own else "arena") + (" bss" if s.bss else "")
        kinds = sorted({KIND_NAMES[r[0]] for r in s.runs})
        rows.append(f"0x{s.addr:08X}\t{where}\t{s.n64_size}\t{s.host_size}\t{','.join(kinds) or '-'}\t"
                    f"{s.disp or '-'}{' (extent by gap)' if s.extent_by_gap else ''}\t{' '.join(sorted(s.names))}")
    (a.out.parent / "data_symbols.txt").write_text("\n".join(rows) + "\n")
    if selfcheck:
        sys.exit(f"gen_data: {len(selfcheck)} self-check failures; see {a.out.parent / 'data_layout.txt'}")
    if func_clash:
        # A runtime image's function under the other image's name: its callers and its data would reach the other
        # image's code (IMAGE_RENAMED is the fix).
        sys.exit("gen_data: function addresses whose name the other image defines:\n  " + "\n  ".join(func_clash))
    print(f"gen_data: {n_names} names ({len(arena_syms)} arena, {len(own)} own, {len(inside)} inside own), "
          f"{len(cdefs)} C-defined, {len(funcs)} functions, {len(thunks)} thunks; "
          f"{len(pointer_conflicts)} pointer conflicts -> {a.out}")


def arena_elements(members, lo, init):
    """(runs, conflicts) for one arena's initialized part [lo, init): the runs byte-swap every element of the
    members (Syms) in place. Where declarations overlap (one runs into the next symbol, or a name lies inside
    another's array), the members are taken in order, own storage first and then the most informative
    declaration (score), and an element over bytes an earlier one already has, at another start or width, is left
    out and listed: (symbol, offset, kind, the symbol that has the bytes, its offset, its element width). Byte
    elements take part, so a word view over a byte view (an inner name inside a colour union array, a declaration
    that runs over a flag) is listed, and whichever ranks first decides; a size in the declaration is the fix."""
    taken, start, owner, conflicts = {}, {}, {}, []   # start: arena offset -> (start, width) of the element there
    for s in sorted(members, key=lambda s: (not s.own, tuple(-x for x in score(s.t, s.runs)))):
        base = s.addr - lo
        for off, kind in elements(s.runs):
            o, n = base + off, N64_ESZ[kind]
            if o + n > init - lo:
                continue
            if any(o + j in start for j in range(n)):
                if start.get(o) != (o, n) or start.get(o + n - 1) != (o, n):
                    held = next(start[o + j] for j in range(n) if o + j in start)
                    conflicts.append((s, off, kind, owner[held[0]], held[0] + lo - owner[held[0]].addr, held[1]))
                continue
            for j in range(n):
                start[o + j] = (o, n)
            owner[o] = s
            if kind != K_U8:
                taken[o] = K_U32 if kind in (K_S32_64, K_U32_64) else kind
    return merge([(taken[o], 1, o, o, 1, 0, 0) for o in sorted(taken)]), conflicts


# Element conflicts in initialized data that were read and judged (docs/port/DATA.md 1): (symbol, the symbol whose
# elements keep the bytes) -> why those are the right ones.
OVERLAPS_REVIEWED = {
    ("D_801146C0", "D_801146BC"): "D_801146BC[1], a colour read as bytes; D_801146C0's word only copies it",
    ("D_801105A4", "D_801102B4"): "&buttonInfo[11].pos, the audio screen's title position (f32)",
    ("D_8011AC84", "D_8011A994"): "&buttonInfo[11].pos, the video screen's title position (f32)",
    ("D_803B5CFC", "D_803B5D2C"): "read as [n][0] for n = 1..4 players, in D_803B5D50; row 0 never",
    ("D_803B4679", "D_803B467C"): "read as [n][i] for n = 1..4 players, after D_803B467C; row 0 never",
    ("D_803B5CFC", "D_803B470C"): "read as [n][0] for n = 1..4 players; row 0's first 16 bytes are the model slots' "
                                  "last (extents.txt), which keep theirs",
}


def group_conflicts(conflicts):
    """Element conflicts with each run of one symbol's elements over one other's as one: (symbol, offset, kind,
    holder, its offset, its width, count)."""
    out = []
    for s, off, kind, held, hoff, hw in conflicts:
        p = out[-1] if out else None
        if p and p[0] is s and p[2] == kind and p[3] is held and p[5] == hw and off == p[1] + p[6] * N64_ESZ[kind]:
            p[6] += 1
        else:
            out.append([s, off, kind, held, hoff, hw, 1])
    return out


def fmt_conflict(g):
    s, off, kind, held, hoff, hw, n = g
    return (f"{sorted(s.names)[0]}+0x{off:X} {KIND_NAMES[kind]}{f' x{n}' if n > 1 else ''}: held by "
            f"{sorted(held.names)[0]}+0x{hoff:X} ({hw} B)")


def byte_unions_in(t, depth=0):
    """Names of the unions inside t that union_program lays out as bytes."""
    if depth > 8 or t[0] not in ("struct", "union", "array"):
        return set()
    if t[0] == "array":
        return byte_unions_in(t[1], depth + 1)
    out = {t[1] or "(anonymous)"} if t in BYTE_UNIONS else set()
    for _, _, mt in t[3]:
        out |= byte_unions_in(mt, depth + 1)
    return out


def test_tables(a):
    """--test-tables OBJ --out HEADER: what the rules above make of the D_XXXXXXXX variables one object declares
    (port/data/test_pc_data_decls.c), as a header for pc_data_test, which loads them from its synthetic image:
    the runs of one arena of initialized data, from the lowest address to the end of the last symbol, and the
    element conflicts (data_layout.txt's lines). Each name has one declaration, at the address it spells."""
    obj = str(a.test_tables)
    _, refs = n64syms.nm_objects([obj], a.nm)[obj]
    found, tags = read_variables([obj], a.readelf, {n for n in refs if re.fullmatch(r"D_[0-9A-F]{8}", n)})
    members = []
    for name, decls in sorted(found.items()):
        s = Sym(n64syms.address_of(name, {}))
        s.names.add(name)
        s.t, s.disp = complete(decls[0][1], tags), decls[0][2]
        s.runs, s.n64_size = program(s.t), n64_layout(s.t)[0] or 0
        if any(r[0] in (K_PTR, K_S32_64, K_U32_64) for r in s.runs):
            sys.exit(f"gen_data --test-tables: {name} needs own storage, which the test arena does not model")
        members.append(s)
    if not members:
        sys.exit(f"gen_data --test-tables: no D_XXXXXXXX declarations in {obj}")
    lo = min(s.addr for s in members)
    hi = align(max(s.addr + s.n64_size for s in members), 4)
    runs, conflicts = arena_elements(members, lo, hi)
    cont = " \\"
    L = [f"/* {a.out.name}: generated by tools/port/gen_data.py --test-tables from {Path(obj).name}. Do not edit.",
         " * The arena runs gen_data's layout rules give port/data/test_pc_data_decls.c's declarations, and the",
         " * element conflicts it lists, for test_pc_data.c. */",
         f"#define PC_TEST_GEN_LO 0x{lo:08X}u",
         f"#define PC_TEST_GEN_HI 0x{hi:08X}u",
         f"#define PC_TEST_GEN_NRUNS {len(runs)}",
         "#define PC_TEST_GEN_RUNS" + cont]
    L += [f"    {{{r[2]}, {r[3]}, {r[1]}, {r[4]}, {r[5]}, {r[6]}, {r[0]}}}," + cont for r in runs]
    L += ["", "#define PC_TEST_GEN_CONFLICTS" + cont]
    L += ["    " + c_str(fmt_conflict(g))[:-1] + '\\n"' + cont for g in group_conflicts(conflicts)]
    L += ['    ""', ""]
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(L))


# Each runtime image's C range (port/data/overlays.ld): its first and one-past-last symbols.
OVL_RANGES = {"ovl_a": ("pc_ovl_a_start", "pc_ovl_a_end"), "ovl_b": ("pc_ovl_b_start", "pc_ovl_b_end")}


def image_data(obj, nm):
    """[(name, nm letter)] of the data an object defines: file-local (d, b, r) and global (D, B, R) definitions.
    Commons and weak copies are left out: the linker may resolve them to another file's definition."""
    out = subprocess.run([nm, obj], capture_output=True, text=True).stdout
    return [(p[2], p[1]) for p in (l.split() for l in out.splitlines()) if len(p) == 3 and p[1] in "dbrDBR"
            and not p[2].startswith((".", "$", "pc_weak_anchor_"))]   # section names (COFF), labels


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# ---------------------------------------------------------------- tables only pointers reach

REACH_BASES = {"u8": ("unsigned char", 1), "s8": ("signed char", 1), "u16": ("short unsigned int", 2),
               "s16": ("short int", 2), "u32": ("unsigned int", 4), "s32": ("int", 4), "f32": ("float", 4),
               "float": ("float", 4)}
REACH_FIXED, REACH_UNTIL, REACH_PARENT, REACH_TABLE = range(4)


class Reach:
    """A rule of port/data/reached.txt (docs/port/DATA.md 7)."""

    def __init__(self, line, root, tspec, cspec):
        self.line, self.root, self.tspec, self.cspec = line, root, tspec, cspec
        self.sym = self.parent = None
        self.slot_n64 = self.slot_host = self.slots = 0
        self.count_kind, self.count, self.field_n64, self.field_width, self.table_sym = REACH_FIXED, 1, 0, 0, None
        self.compat = []


def struct_members(t):
    """name -> (N64 offset, host offset, type) of a canonical struct's members."""
    out, off = {}, 0
    for name, hoff, mt in t[3]:
        sz, al = n64_layout(mt)
        off = align(off, al)
        out[name] = (off, hoff, mt)
        off += sz or 0
    return out


def reach_type(spec, tags):
    m = re.fullmatch(r"(\w+)\[(\d+)\]", spec)
    if m and m.group(1) in REACH_BASES:
        name, size = REACH_BASES[m.group(1)]
        b = INTERN.setdefault(("base", name, size), ("base", name, size))
        return INTERN.setdefault(("array", b, int(m.group(2))), ("array", b, int(m.group(2))))
    if spec in REACH_BASES:
        name, size = REACH_BASES[spec]
        return INTERN.setdefault(("base", name, size), ("base", name, size))
    if spec not in tags:
        return None
    return tags[spec].most_common(1)[0][0]


def reach_rules(path, tags, syms_by_name, ordered):
    """The rules of reached.txt, each with its pointee type, runs and count rule, checked against the DWARF."""
    rules = []
    if not path.exists():
        return rules
    for n, line in enumerate(path.read_text().splitlines(), 1):
        f = line.split("#", 1)[0].split()
        if not f:
            continue
        if len(f) < 3:
            sys.exit(f"{path}:{n}: ROOT TYPE COUNT")
        r = Reach(n, f[0], f[1], f[2:])
        r.t = reach_type(r.tspec, tags)
        if r.t is None or r.t[0] in ("opaque", "void"):
            sys.exit(f"{path}:{n}: no complete type {r.tspec} in the game's DWARF")
        r.n64_size, r.host_size = n64_layout(r.t)[0] or 0, host_size(r.t) or 0
        r.runs = program(r.t)
        r.copy = any(x[0] in (K_PTR, K_S32_64, K_U32_64) or x[2] != x[3] or x[5] != x[6] for x in r.runs) \
            or r.n64_size != r.host_size
        if "." in r.root:
            tag, field = r.root.split(".", 1)
            parents = [p for p in rules if p.tspec == tag]
            if not parents or parents[0].t[0] != "struct" or field not in struct_members(parents[0].t):
                sys.exit(f"{path}:{n}: {r.root}: no earlier rule reaches a {tag} with a field {field}")
            r.parent = parents[0]
            r.slot_n64, r.slot_host, _ = struct_members(r.parent.t)[field]
        else:
            s = syms_by_name.get(r.root)
            if s is None or not s.own or not s.runs or any(x[0] != K_PTR for x in s.runs):
                sys.exit(f"{path}:{n}: {r.root} is not a placed table of pointers with storage of its own")
            r.sym, r.slots = s, s.n64_size // 4
        c = r.cspec
        if c[0] == "until" and len(c) == 2:
            if r.t[0] != "struct" or c[1] not in struct_members(r.t):
                sys.exit(f"{path}:{n}: {r.tspec} has no field {c[1]}")
            off, _, mt = struct_members(r.t)[c[1]]
            r.count_kind, r.field_n64, r.field_width = REACH_UNTIL, off, n64_layout(mt)[0]
        elif c[0] == "parent" and len(c) == 2:
            if r.parent is None or c[1] not in struct_members(r.parent.t):
                sys.exit(f"{path}:{n}: the parent has no field {c[1]}")
            off, _, mt = struct_members(r.parent.t)[c[1]]
            r.count_kind, r.field_n64, r.field_width = REACH_PARENT, off, n64_layout(mt)[0]
        elif re.fullmatch(r"\w+\[i\]", c[0]):
            t = syms_by_name.get(c[0][:-3])
            if t is None or r.sym is None or not t.runs or t.runs[0][0] not in (K_U8, K_U16, K_U32):
                sys.exit(f"{path}:{n}: {c[0]}: a root rule's count from a placed table of integers")
            r.count_kind, r.table_sym, r.field_width = REACH_TABLE, t, N64_ESZ[t.runs[0][0]]
        elif c[0].isdigit():
            r.count = int(c[0])
        else:
            sys.exit(f"{path}:{n}: COUNT is N, until FIELD, parent FIELD or TABLE[i]")
        if r.count_kind in (REACH_UNTIL, REACH_PARENT) and r.field_width not in (1, 2, 4):
            sys.exit(f"{path}:{n}: the count field must be 1, 2 or 4 bytes")
        # Storage that already holds this type: a pointer into it is resolved as usual.
        sig = (r.runs, r.n64_size, r.host_size)
        r.compat = [s for s in ordered if s.own and s.parent is None and not s.cdef and s.t[0] == "array"
                    and (program(s.t[1]), n64_layout(s.t[1])[0], host_size(s.t[1])) == sig]
        rules.append(r)
    return rules


def emit(a, syms, ordered, arena_runs, funcs, defined_in, thunks, nmtab, obj_image, ovl_data, reach=(),
         decoupled=()):
    arenas = n64syms.ARENAS
    own = [s for s in ordered if s.own and s.parent is None and not s.cdef]
    cdefs = [s for s in ordered if s.cdef]
    own_var = {id(s): f"pc_own_{i}" for i, s in enumerate(own)}
    L = ["/* data.c: generated by tools/port/gen_data.py. Do not edit; the build regenerates it.",
         " * Host storage for the game's data symbols and the descriptors port/data/pc_data.c fills it with from",
         " * the ROM at startup (docs/port/DATA.md). Holds no ROM bytes. */",
         "#include <stddef.h>", "#include <stdint.h>", "#include \"pc_data.h\"", "",
         "#if defined(__APPLE__)", "#define N64_SYM(name) \"_\" name", "#else", "#define N64_SYM(name) name",
         "#endif",
         "/* A symbol takes its own size (the linker compares it with the game's common symbols). */",
         "#if defined(__ELF__)", "#define PC_SIZE(name, n) \"\\n.size \" N64_SYM(name) \", \" #n", "#else",
         "#define PC_SIZE(name, n) \"\"", "#endif",
         "#if defined(_WIN32)   /* weak.ld defines pc_weak.<name> as the symbol, or 0 (docs/port/WINDOWS.md) */",
         f"#define PC_WEAK(id, name) extern char id[] __asm__(\"{n64syms.PE_WEAK_PREFIX}\" name);", "#else",
         "#define PC_WEAK(id, name) extern char id[] __asm__(N64_SYM(name)) __attribute__((weak));", "#endif", ""]
    L.append("/* The arenas, each image's data and BSS in N64 layout, are at their N64 addresses in the RDRAM window")
    L.append(" * port/ultra maps at host 0x80000000 (docs/port/ULTRA.md), so nothing here allocates them:")
    for img, lo, init, hi in arenas:
        L.append(f" *   {img}: 0x{lo:08X}-0x{hi:08X} (data to 0x{init:08X})")
    L.append(" */")
    L += ["", "/* Own storage: host layout. Aligned as a common symbol of that size can be (x86-64 GCC aligns",
          " * aggregates of 32 bytes or more to 32), so the game C's tentative definitions agree with it. */"]
    for s in own:
        al = 32 if s.host_size >= 32 else 16
        L.append(f"__attribute__((aligned({al}))) uint8_t {own_var[id(s)]}[{max(s.host_size, 1)}] "
                 f"__asm__(N64_SYM({c_str(sorted(s.names)[0])}));   /* N64 0x{s.addr:08X}, {s.n64_size} B: {s.disp} */")
    sets = []   # (name, base symbol or None for an absolute address, offset or address, size)
    layout = {"window": [WINDOW_LO, WINDOW_HI], "arenas": [[lo, hi] for _, lo, _, hi in arenas], "absolute": [],
              "sets": [], "own": [],
              "ovl_data": {img: sorted(n for _, n, k in v if k in "dbDB") for img, v in ovl_data.items()},
              "ovl_ranges": OVL_RANGES,
              "decoupled": [[d.pc, d.host_size, d.retired] for d in decoupled]}
    if decoupled:
        L += ["", "/* Game data the PC owns, with no N64 address (port/data/decoupled.txt, docs/port/DECOUPLE.md): zero at",
              " * startup like BSS, outside the RDRAM window, and not in the loader's tables. Aligned as own storage. */"]
        if any(d.init for d in decoupled):
            L += ["/* Those marked \"from the image\" were initialized data: the loader fills them from it (pc_inits). */"]
    for i, d in enumerate(decoupled):
        al = 32 if d.host_size >= 32 else 16
        how = (f", from the image{f' with a period of {d.period}' if d.period else ''}" if d.init else "")
        L.append(f"__attribute__((aligned({al}))) uint8_t pc_decoupled_{i}[{d.host_size}] __asm__(N64_SYM({c_str(d.pc)}));"
                 f"   /* {d.disp}; was {d.n64} at 0x{d.lo:08X}, {d.size} B{how} */")
    for s in ordered:
        if s.cdef:
            continue
        if s.parent is not None:
            base, off = sorted(s.parent.names)[0], s.parent_off
            names = sorted(s.names)
        elif s.own:
            base, off = sorted(s.names)[0], 0
            names = sorted(s.names)[1:]
            layout["own"].append([base, max(s.host_size, 1)])
        else:
            base, off = None, s.addr
            names = sorted(s.names)
        for n in names:
            sets.append((n, base, off, s.host_size))
    for s in sorted(syms.values(), key=lambda x: x.addr):
        if s.end_marker:   # one past an overlay's BSS (the loaders pass it as bssEnd)
            for n in sorted(s.names):
                sets.append((n, None, s.addr, 0))
    for n, base, off, _ in sets:
        layout["absolute" if base is None else "sets"].append([n, off] if base is None else [n, base, off])
    if sets:
        L += ["", "/* Symbols at their N64 addresses (absolute), or at the matching offset in own storage. */",
              "__asm__("]
        for n, base, off, size in sets:
            value = f"0x{off:08X}" if base is None else f"\" N64_SYM(\"{base}\") \" + 0x{off:X}"
            L.append(f"    \".globl \" N64_SYM(\"{n}\") \"\\n.set \" N64_SYM(\"{n}\") \", {value}\""
                     f" PC_SIZE(\"{n}\", {size}) \"\\n\"")
        L += [");", ""]
    L += reserved_lines(layout)   # port/data/reserved.txt
    if thunks:
        L += ["/* Labels the game calls for a function defined under another name (symbols.json). */",
              "#if defined(_WIN32)   /* COFF has no section stack (.previous): naked functions */"]
        L += [f"__attribute__((naked)) void {n}(void) {{ __asm__(\"jmp {RENAMED.get(t, t)}\"); }}"
              for n, t, _ in thunks]
        L += ["#else",
              "#if defined(__aarch64__)", "#define PC_THUNK(n, t) \".globl \" N64_SYM(n) \"\\n.p2align 2\\n\" "
              "N64_SYM(n) \":\\n\\tb \" N64_SYM(t) \"\\n\"",
              "#elif defined(__x86_64__)", "#define PC_THUNK(n, t) \".globl \" N64_SYM(n) \"\\n.p2align 4\\n\" "
              "N64_SYM(n) \":\\n\\tjmp \" N64_SYM(t) \"\\n\"",
              "#else", "#error \"gen_data: add a branch for this architecture\"", "#endif",
              "__asm__(\".text\\n\""]
        L += [f"    PC_THUNK(\"{n}\", \"{RENAMED.get(t, t)}\")" for n, t, _ in thunks]
        L += ["    \".previous\\n\");", "#endif", ""]

    # Weak references: functions and the game C's data.
    weak = {}
    for (addr, img), host in sorted(funcs.items()):
        weak.setdefault(host, f"pc_f_{len(weak)}")
    cweak = {}
    for s in cdefs:
        cweak[id(s)] = f"pc_c_{len(cweak)}"
    L.append("/* Weak references: NULL when the function or data is not linked in. */")
    for host, wid in weak.items():
        L.append(f"PC_WEAK({wid}, {c_str(host)})")
    for s in cdefs:
        L.append(f"PC_WEAK({cweak[id(s)]}, {c_str(sorted(s.names)[0])})")
    L.append("")
    if a.weak_script:
        W = ["/* weak.ld: generated by tools/port/gen_data.py. data.c's weak references on PE (docs/port/WINDOWS.md):",
             " * pc_weak.<name> is the symbol when something linked in defines it, else 0, as an undefined weak",
             " * reference is on ELF. An absolute 0 gets no base relocation, so it stays 0 when the image moves. */"]
        W += [f"PROVIDE({n64syms.PE_WEAK_PREFIX}{n} = DEFINED({n}) ? {n} : 0);"
              for n in [*weak, *(sorted(s.names)[0] for s in cdefs)]]
        a.weak_script.parent.mkdir(parents=True, exist_ok=True)
        a.weak_script.write_text("\n".join(W) + "\n")

    # Runs.
    runs = []
    arena_first = {}
    for i in range(len(arenas)):
        arena_first[i] = (len(runs), len(arena_runs[i]))
        runs += arena_runs[i]
    sym_rows = []
    for s in sorted(own + cdefs, key=lambda x: x.addr):
        first = len(runs)
        if s.own or (s.cdef and any(r[2] != r[3] or r[0] == K_PTR for r in s.runs)):
            runs += s.runs
        img = IMAGE_INDEX[arenas[s.arena][0]]
        flags = ("PC_SYM_OWN" if not s.cdef else "PC_SYM_CDEF") + ("|PC_SYM_BSS" if s.bss or s.cdef_letter in
                                                                     ("B", "b") else "")
        host_sz = s.host_size
        if s.cdef:
            for o, (d, _) in nmtab.items():
                for n in sorted(s.names):
                    if n in d and d[n][1]:
                        host_sz = d[n][1]
        host = own_var[id(s)] if not s.cdef else cweak[id(s)]
        sym_rows.append(f"    {{{c_str(sorted(s.names)[0])}, 0x{s.addr:08X}, {s.n64_size or 1}, {host_sz or 0}, "
                        f"{host}, {first}, {len(runs) - first}, {img}, {flags}}},")
    sym_index = {}
    for i, s in enumerate(sorted(own + cdefs, key=lambda x: x.addr)):
        sym_index[id(s)] = i
    reach_rows = []
    for k, r in enumerate(reach):
        first = len(runs)
        runs += r.runs
        compat = [sym_index[id(s)] for s in r.compat if id(s) in sym_index]
        if compat:
            L.append(f"static const uint32_t pc_reach_compat_{k}[] = {{{', '.join(map(str, compat))}}};")
        kinds = ["PC_REACH_FIXED", "PC_REACH_UNTIL", "PC_REACH_PARENT", "PC_REACH_TABLE"]
        reach_rows.append(
            f"    {{{c_str(f'{r.root} -> {r.tspec}')}, {sym_index[id(r.sym)] if r.sym else -1}, "
            f"{reach.index(r.parent) if r.parent else -1}, {r.slot_n64}, {r.slot_host}, {r.slots}, {r.n64_size}, "
            f"{r.host_size}, {first}, {len(r.runs)}, {kinds[r.count_kind]}, {r.count}, {r.field_n64}, "
            f"{r.field_width}, 0x{r.table_sym.addr if r.table_sym else 0:08X}, {int(r.copy)}, "
            f"{f'pc_reach_compat_{k}' if compat else 'NULL'}, {len(compat)}}},")
    init_rows = []
    for i, d in enumerate(decoupled):
        if d.init:
            init_rows.append(f"    {{{c_str(d.pc)}, 0x{d.lo:08X}, {d.size}, {d.host_size}, {d.init_host}, "
                             f"pc_decoupled_{i}, {len(runs)}, {len(d.init_runs)}, {d.period_host}, "
                             f"{IMAGE_INDEX[d.image]}}},")
            runs += d.init_runs
    L.append(f"static const PcDataRun pc_runs[{max(len(runs), 1)}] = {{")
    for r in runs:
        L.append(f"    {{{r[2]}, {r[3]}, {r[1]}, {r[4]}, {r[5]}, {r[6]}, {r[0]}}},")
    L += ["};", ""]
    L.append(f"static const PcDataSym pc_syms[{max(len(sym_rows), 1)}] = {{")
    L += sym_rows
    L += ["};", ""]
    L.append("static const PcDataImage pc_images[] = {")
    for name, storage, rom_off, rom_len, size, base in n64syms.IMAGES:
        L.append(f"    {{{c_str(name)}, {'PC_IMG_RAW' if storage == 'raw' else 'PC_IMG_DEFLATE'}, 0x{rom_off:X}, "
                 f"{rom_len}, {size}, 0x{base:08X}}},")
    L += ["};", ""]
    L.append("/* host: the arena's N64 address, in the RDRAM window. */")
    L.append("static const PcDataArena pc_arenas[] = {")
    for i, (img, lo, init, hi) in enumerate(arenas):
        f, n = arena_first[i]
        L.append(f"    {{{IMAGE_INDEX[img]}, 0x{lo:08X}, 0x{init:08X}, 0x{hi:08X}, (uint8_t *)(uintptr_t)0x{lo:08X}u, "
                 f"{f}, {n}}},")
    L += ["};", ""]
    L.append(f"static const PcDataFunc pc_funcs[{max(len(funcs), 1)}] = {{")
    for (addr, img), host in sorted(funcs.items()):
        L.append(f"    {{0x{addr:08X}, {IMAGE_INDEX[img]}, {weak[host]}, {c_str(host)}}},")
    L += ["};", ""]
    names = sorted((s.addr, n) for s in ordered if not s.cdef for n in sorted(s.names)[:1])
    L.append(f"static const PcDataName pc_names[{max(len(names), 1)}] = {{")
    L += [f"    {{0x{ad:08X}, {c_str(n)}}}," for ad, n in names]
    L += ["};", ""]
    L += ["/* Each runtime image's C data, file-local statics included (port/data/overlays.ld, linked with -T):",
          " * put back on every load of the image. */"]
    L += [f"extern uint8_t {lo}[] __asm__(N64_SYM(\"{lo}\")), {hi}[] __asm__(N64_SYM(\"{hi}\"));"
          for lo, hi in OVL_RANGES.values()]
    L.append(f"static const PcDataCRange pc_cranges[{len(OVL_RANGES)}] = {{")
    L += [f"    {{{IMAGE_INDEX[img]}, {lo}, {hi}}}," for img, (lo, hi) in OVL_RANGES.items()]
    L += ["};", ""]
    L.append("/* Tables only pointers reach (port/data/reached.txt, docs/port/DATA.md 7). */")
    L.append(f"static const PcDataReach pc_reach[{max(len(reach_rows), 1)}] = {{")
    L += reach_rows
    L += ["};", ""]
    if init_rows:
        L += ["/* Decoupled data the image initializes (port/data/decoupled.txt init=image, docs/port/DECOUPLE.md): filled",
              " * from the image on its load, the rest continued with a period or zero. */",
              f"static const PcDataInit pc_inits[{len(init_rows)}] = {{"]
        L += init_rows
        L += ["};", ""]
    L += ["static PcDataState pc_state;",
          "const PcDataTables pc_data_tables = {",
          "    pc_images, sizeof pc_images / sizeof pc_images[0], pc_arenas, sizeof pc_arenas / sizeof pc_arenas[0],",
          f"    pc_syms, {len(sym_rows)}, pc_runs, {len(runs)}, pc_funcs, {len(funcs)}, pc_names, {len(names)},",
          f"    0x{HEAP_LO:08X},   /* the game heap starts at the end of the game's BSS (docs/port/ASSETS.md) */",
          "    &pc_state,",
          f"    pc_cranges, {len(OVL_RANGES)},",
          f"    pc_reach, {len(reach_rows)},"]
    if init_rows:
        L.append(f"    pc_inits, {len(init_rows)},")
    L += ["};", "",
          "/* pc_port.h entry points. The loader is weak here so the link test (which leaves it out) still links. */",
          "int pc_data_load_all(const PcDataTables *t, const uint8_t *rom, size_t rom_size, const char *report_path)",
          "    __attribute__((weak));",
          "int pc_data_load_image(const PcDataTables *t, int image) __attribute__((weak));", "",
          "int pc_data_init(const uint8_t *rom, size_t rom_size)", "{",
          "    return pc_data_load_all ? pc_data_load_all(&pc_data_tables, rom, rom_size, NULL) : -1;", "}", "",
          "/* PC_OVERLAY_A (0) is runtime image A (\"Start\"), PC_OVERLAY_B (1) image B (\"Extra\"). */",
          "void pc_data_overlay_load(int overlay)", "{",
          f"    if (pc_data_load_image) pc_data_load_image(&pc_data_tables, overlay ? {IMAGE_INDEX['ovl_b']} : "
          f"{IMAGE_INDEX['ovl_a']});", "}", ""]
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(L))
    (a.out.parent / "data_layout.json").write_text(json.dumps(layout))


def check(exe, layout_path, nm, pe=False):
    """Verify the linked executable: every arena symbol absolute at its N64 address, every symbol inside own
    storage at its base + offset, own storage at its host size, and nothing else linked in the RDRAM window.
    PE (pe): COFF symbols have no sizes, so own storage isn't size-checked; the image links at 0x140000000 and
    moves, so only the window and its KSEG1 alias must be free of it (no GOT fix-up relies on the rest)."""
    layout = json.loads(Path(layout_path).read_text())
    out = subprocess.run([nm, "-S", "--defined-only", str(exe)], capture_output=True, text=True, check=True).stdout
    addr, size, letter = {}, {}, {}
    every = collections.defaultdict(list)   # name -> addresses, file-locals of one name included
    for line in out.splitlines():
        p = line.split()
        if len(p) not in (3, 4):
            continue
        name, kind = p[-1], p[-2]
        every[name].append(int(p[0], 16))
        if name in letter and kind.islower() and letter[name].isupper():
            continue   # a file-local static of the same name (overlay statics such as D_80399120): the global wins
        addr[name], letter[name] = int(p[0], 16), kind
        size.pop(name, None)
        if len(p) == 4:
            size[name] = int(p[1], 16)
    bad = []
    prev = None
    for name, n in layout["own"]:
        if name not in addr:
            bad.append(f"own {name}: not in the executable")
            continue
        if size.get(name) != n and not pe:
            bad.append(f"own {name}: size {size.get(name)} != {n}")
        # In N64 address order (layout["own"]'s), as data.c defines them: game C that walks from one symbol to the
        # next relies on it (ambient_sounds_clear did), and GCC reorders definitions unless -fno-toplevel-reorder.
        if prev is not None and addr[name] <= addr[prev]:
            bad.append(f"own {name}: at 0x{addr[name]:X}, not after {prev} (0x{addr[prev]:X}): "
                       f"compile data.c with -fno-toplevel-reorder")
        prev = name
    for name, base, off in layout["sets"]:
        if name not in addr or base not in addr:
            bad.append(f"{name}: not in the executable")
        elif addr[name] - addr[base] != off:
            bad.append(f"{name}: at {base}+0x{addr[name] - addr[base]:X}, expected +0x{off:X}")
    for name, want in layout["absolute"]:
        if name not in addr:
            bad.append(f"{name}: not in the executable")
        elif letter[name] not in "Aa" or addr[name] != want:
            bad.append(f"{name}: {letter[name]} 0x{addr[name]:X}, expected absolute 0x{want:08X}")
    # The executable's own link-time addresses must stay below the window: port/ultra/memory.c tells the GOT
    # slots of absolute symbols by an addend at 0x80000000 or above.
    lo, hi = layout["window"]
    if pe:
        k1 = 0x20000000   # the KSEG1 alias of the window (port/ultra/memory.c)
        high = sorted(n for n, x in addr.items() if letter[n] not in "Aa" and (lo <= x < hi or lo + k1 <= x < hi + k1))
        bad += [f"{n}: linked at 0x{addr[n]:X}, inside the RDRAM window" for n in high]
    else:
        high = sorted(n for n, x in addr.items() if letter[n] not in "Aa" and x >= lo)
        bad += [f"{n}: linked at 0x{addr[n]:X}, at or above the RDRAM window" for n in high]
    # Each runtime image's C data inside its C range (overlays.ld), which the loader puts back on the image's loads:
    # as many symbols of each name inside it as the image's objects define.
    n_ovl = 0
    for img, names in layout.get("ovl_data", {}).items():
        lo_name, hi_name = layout["ovl_ranges"][img]
        if lo_name not in addr or hi_name not in addr:
            bad.append(f"{img}: no C range ({lo_name}, {hi_name}): link with port/data/overlays.ld")
            continue
        for name, want in collections.Counter(names).items():
            got = sum(1 for x in every.get(name, ()) if addr[lo_name] <= x < addr[hi_name])
            n_ovl += min(got, want)
            if got < want:
                bad.append(f"{img} {name}: {want - got} of {want} outside its C range")
    # Decoupled data: in the executable at its host size, outside the window; no retired N64 name defined.
    for name, n, retired in layout.get("decoupled", []):
        if name not in addr:
            bad.append(f"decoupled {name}: not in the executable")
        elif letter[name] in "Aa" or lo <= addr[name] < hi:
            bad.append(f"decoupled {name}: at 0x{addr[name]:X}, in the RDRAM window")
        elif size.get(name) != n and not pe:
            bad.append(f"decoupled {name}: size {size.get(name)} != {n}")
        bad += [f"decoupled {name}: the retired {r} is defined (0x{addr[r]:X})" for r in retired if r in addr]
    bad += check_reserved(layout, addr, size, letter, pe)   # port/data/reserved.txt
    for b in bad[:50]:
        print("gen_data check:", b)
    print(f"gen_data check: {len(layout['absolute'])} at their N64 addresses in {len(layout['arenas'])} arenas, "
          f"{len(layout['sets'])} inside own storage, {len(layout['own'])} own in N64 order, "
          f"{n_ovl} runtime-image C data in their C ranges, {len(layout.get('decoupled', []))} decoupled, "
          f"{len(layout.get('reserved', []))} reserved in the window, "
          f"nothing else in the window: {'OK' if not bad else f'{len(bad)} errors'}")
    return 1 if bad else 0


if __name__ == "__main__":
    main()
