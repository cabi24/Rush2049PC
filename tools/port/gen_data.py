#!/usr/bin/env python3
"""Generate data.c: host storage for the game's data symbols and the descriptors that fill it from the ROM.

    python3 tools/port/gen_data.py --objects-list build/pc/game_objects.txt --out build/pc/pc-data/data.c
    python3 tools/port/gen_data.py --check build/pc/rush2049pc_datacheck --layout build/pc/pc-data/data_layout.json

The game C declares data at fixed N64 addresses that nothing defines: the boot segment's, the game image's and
the two runtime images' initialized data, and their BSS. This tool gives every such symbol host storage and
writes the descriptors port/data/pc_data.c uses to fill it from the user's ROM at startup (docs/port/DATA.md).
No ROM bytes are read here or written into the build.

1. Types come from the game objects' DWARF (readelf): each file's own declaration. When files disagree, the
   most informative wins (struct over pointer over words over bytes) and the disagreement is reported.
2. Each type is laid out twice: for the N64 (MIPS o32: pointers and long are 4 bytes, double is 8 aligned 8)
   and for the host (the DWARF offsets). The result is a field program: runs of (kind, count, N64 offset, host
   offset), repeated with strides for arrays.
3. Storage. Each image's data and BSS is one zeroed arena laid out as on the N64, and a symbol with no
   pointers whose two layouts agree is defined at its N64 offset there (an assembler .set), so code that
   indexes from one symbol into the next keeps working. A symbol with pointers, or whose host layout differs,
   gets its own storage; symbols inside its N64 extent are defined at the matching host offsets.
4. Addresses: the referencing image's symbol table (asm/us/<image>/symbols.json, what the matching build links
   against), then symbol_addrs.us.txt and undefined_syms_auto.us.txt, then D_XXXXXXXX names. Functions known by
   address (layout/*.json, the symbol file) go in a table, through weak references, so the loader can turn N64
   function pointers into host ones; the game's data defined in C is listed the same way.
5. Names the game calls under another label of a defined function (audio_dma_sync = AllocateMemoryHeap) get a
   branch thunk.

Outputs: data.c, data_layout.txt (what was decided, the conflicts, what is left), data_layout.json (for
--check, which verifies placements and sizes in the linked executable). Stdlib and binutils, Python 3.9+.
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
# N64 (IDO, o32) sizes of the C base types, by DWARF name.
N64_BASE = {"char": 1, "signed char": 1, "unsigned char": 1, "_Bool": 1, "short int": 2, "short unsigned int": 2,
            "int": 4, "unsigned int": 4, "long int": 4, "long unsigned int": 4, "long long int": 8,
            "long long unsigned int": 8, "float": 4, "double": 8, "long double": 8}


# N64 pointers at or above this (outside the runtime images) point into the game heap and stay as they are.
HEAP_LO = 0x8017A640
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


def parse_readelf(text, single=None):
    """{object: {offset: Die}} from `readelf --debug-dump=info` output (one CU per object)."""
    objs, dies, stack, cur = {}, None, [], None
    if single:
        dies = objs.setdefault(single, {})
    for line in text.splitlines():
        if line.startswith("File: "):
            dies, stack, cur = objs.setdefault(line[6:].strip(), {}), [], None
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


def read_variables(objects, readelf, wanted):
    """name -> [(object, type, display, is_definition)] for the external variables in `wanted`, and struct tag ->
    Counter of complete definitions (to complete a type another file left incomplete)."""
    found = collections.defaultdict(list)
    tags = collections.defaultdict(collections.Counter)
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([readelf, "--debug-dump=info", *batch], capture_output=True, text=True,
                             check=True).stdout
        for obj, dies in parse_readelf(out, batch[0] if len(batch) == 1 else None).items():
            memo = {}
            for off, d in dies.items():
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
        best = max(t[3], key=lambda m: (n64_layout(m[2])[0] or 0, sum(1 for x in program(m[2]) if x[0] != K_U8)),
                   default=None)
        r = program(best[2]) if best else []
    elif k == "array":
        r = replicate(program(t[1]), t[2] or 0, n64_layout(t[1])[0] or 0, host_size(t[1]) or 0)
    else:
        r = []
    _PROG[t] = r
    return r


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
        self.sfsize = None
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


def score(t, runs):
    """Most informative declaration first: struct over pointer over words over bytes."""
    has_struct = contains(t, ("struct", "union"))
    has_ptr = any(r[0] == K_PTR for r in runs)
    width = max((N64_ESZ[r[0]] for r in runs), default=0)
    rank = 5 if has_struct and has_ptr else 4 if has_struct else 3 if has_ptr else 2 if width > 1 else \
        1 if runs else 0
    return rank, n64_layout(t)[0] or 0


def signature(runs, limit):
    """Element classes below `limit`, for conflict checks: bytes are left out; floats count as words."""
    cls = {K_F32: K_U32, K_F64: K_U64, K_S32_64: K_U32, K_U32_64: K_U32}
    return tuple((o, cls.get(k, k)) for o, k in elements(runs, True) if o < limit)


SWAP = {K_U16: 2, K_U32: 4, K_F32: 4, K_PTR: 4, K_S32_64: 4, K_U32_64: 4, K_U64: 8, K_F64: 8}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--objects-list", type=Path, help="the game objects, one per line")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--nm", default="nm")
    ap.add_argument("--readelf", default="readelf")
    ap.add_argument("--root", type=Path, default=n64syms.ROOT)
    ap.add_argument("--check", type=Path, help="linked executable to verify against --layout")
    ap.add_argument("--layout", type=Path)
    a = ap.parse_args()
    if a.check:
        sys.exit(check(a.check, a.layout, a.nm))
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
        if is_function_name(name, symfile) or name in PC_EXPORTS:
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
    wanted = {n for s in syms.values() for n in s.names}
    found, tags = read_variables(objects, a.readelf, wanted)
    for s in syms.values():
        for n in sorted(s.names):
            for obj, t, disp, is_def in found.get(n, ()):
                if s.cdef and not is_def:
                    continue
                s.decls.append((n, obj, complete(t, tags), disp))

    # Extents: symbol-file size, else the largest complete declaration, else the gap to the next address.
    known = collections.defaultdict(set)
    for addr, s in syms.items():
        known[s.arena].add(addr)
    for name, (addr, kind, _) in symfile.items():
        w = arena_of(addr, {"game"})
        if w and kind == "data":
            known[w[0]].add(addr)
    for i, (_, lo, init, hi) in enumerate(n64syms.ARENAS):
        known[i] |= {init, hi}
    known = {i: sorted(v) for i, v in known.items()}

    def gap(s):
        k = known[s.arena]
        j = bisect.bisect_right(k, s.addr)
        return (k[j] if j < len(k) else n64syms.ARENAS[s.arena][3]) - s.addr

    pointer_conflicts, width_conflicts, untyped = [], [], []
    selfcheck, size_mismatch, past_gap, crossing = [], [], [], []
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
        # Incomplete declarations (T x[], struct tags no file completes) take the symbol-file size, else the
        # gap to the next known address, or the largest complete declaration if that is longer.
        extent = s.sfsize or max([gap(s)] + sized)
        cands = {}
        for n, obj, t, disp in s.decls:
            tt = t
            if tt[0] == "array" and tt[2] is None:
                es = n64_layout(tt[1])[0]
                if es:
                    tt = INTERN.setdefault(("array", tt[1], max(1, extent // es)), ("array", tt[1], max(1, extent // es)))
            elif tt[0] in ("opaque", "void", "func"):
                tt = ("array", ("base", "unsigned char", 1), extent)
            c = cands.setdefault(tt, [0, disp, []])
            c[0] += 1
            c[2].append((n, obj))
        if not cands:
            untyped.append(s)
            tt = ("array", ("base", "unsigned char", 1), extent)
            cands[tt] = [0, "u8[] (no declaration with a type)", []]
        ranked = sorted(cands.items(), key=lambda kv: (score(kv[0], program(kv[0])), kv[1][0], kv[1][1], repr(kv[0])), reverse=True)
        s.t, (cnt, s.disp, _) = ranked[0]
        s.extent_by_gap = not s.sfsize and s.t not in [t for _, _, t, _ in s.decls]
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
        if s.sfsize and s.n64_size != s.sfsize:
            size_mismatch.append(f"0x{s.addr:08X} {sorted(s.names)[0]}: {s.disp} is {s.n64_size} B on the N64, "
                                 f"the symbol file says {s.sfsize}")
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

    # Own storage swallows the symbols inside its N64 extent.
    ordered = sorted((s for s in syms.values() if not s.end_marker), key=lambda x: x.addr)
    overlaps = []
    for s in ordered:
        if s.parent is not None or not s.own or s.cdef:
            continue
        for u in ordered:
            if u is s or u.addr < s.addr or u.addr >= s.addr + max(s.n64_size, 1) or u.arena != s.arena:
                continue
            if u.cdef:
                overlaps.append(f"0x{u.addr:08X} {sorted(u.names)[0]} (C-defined) inside own 0x{s.addr:08X} "
                                f"{sorted(s.names)[0]}")
                continue
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
        taken = {}
        cover = bytearray(init - lo)
        members = [s for s in ordered if s.arena == i and s.parent is None and not s.cdef and not s.bss]
        members.sort(key=lambda s: (not s.own, tuple(-x for x in score(s.t, s.runs))))
        for s in members:
            base = s.addr - lo
            for off, kind in elements(s.runs, True):
                kk = K_U32 if kind in (K_S32_64, K_U32_64) else kind
                o, n = base + off, N64_ESZ[kind]
                if o + n > init - lo:
                    continue
                if any(cover[o:o + n]):
                    if taken.get(o) is None or SWAP[taken[o]] != n:
                        elem_conflicts.append((s, off, kind))
                    continue
                cover[o:o + n] = b"\1" * n
                taken[o] = kk
        runs = []
        for o in sorted(taken):
            runs.append((taken[o], 1, o, o, 1, 0, 0))
        arena_runs[i] = merge(runs)

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
        host = RENAMED.get(name, name)
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

    emit(a, syms, ordered, arena_runs, funcs, defined_in, thunks, nmtab, obj_image)

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
    section("Arena element conflicts (first declaration kept)",
            [f"0x{s.addr + off:08X} {sorted(s.names)[0]}+0x{off:X} {KIND_NAMES[k]}" for s, off, k in elem_conflicts])
    section("Self-check failures (runs outside the symbol)", selfcheck)
    section("N64 size differs from the symbol file", size_mismatch)
    section("Declared N64 size runs past the next known address (nested names or mistyped sizes)", past_gap)
    section("Gap extents crossing symbol-table addresses (give the declaration a size, or port/data/extents.txt)",
            crossing)
    section("Overlaps", overlaps)
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
    statics = sorted(f"{Path(o).name.replace('.c.o', '.c')}: {n}" for o in objects if obj_image[o] in ("ovl_a", "ovl_b")
                     for n in local_data(o, a.nm))
    section("Overlay statics not reset on overlay load (file-local; need a section or a reset hook)", statics)
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
    print(f"gen_data: {n_names} names ({len(arena_syms)} arena, {len(own)} own, {len(inside)} inside own), "
          f"{len(cdefs)} C-defined, {len(funcs)} functions, {len(thunks)} thunks; "
          f"{len(pointer_conflicts)} pointer conflicts -> {a.out}")


def local_data(obj, nm):
    out = subprocess.run([nm, obj], capture_output=True, text=True).stdout
    return [l.split()[-1] for l in out.splitlines() if len(l.split()) == 3 and l.split()[1] in "dbr"
            and not l.split()[-1].startswith((".L", "$"))]


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit(a, syms, ordered, arena_runs, funcs, defined_in, thunks, nmtab, obj_image):
    arenas = n64syms.ARENAS
    arena_var = [f"pc_n64_{img}" for img, _, _, _ in arenas]
    own = [s for s in ordered if s.own and s.parent is None and not s.cdef]
    cdefs = [s for s in ordered if s.cdef]
    own_var = {id(s): f"pc_own_{i}" for i, s in enumerate(own)}
    L = ["/* data.c: generated by tools/port/gen_data.py. Do not edit; the build regenerates it.",
         " * Host storage for the game's data symbols and the descriptors port/data/pc_data.c fills it with from",
         " * the ROM at startup (docs/port/DATA.md). Holds no ROM bytes. */",
         "#include <stddef.h>", "#include <stdint.h>", "#include \"pc_data.h\"", "",
         "#if defined(__APPLE__)", "#define N64_SYM(name) \"_\" name", "#else", "#define N64_SYM(name) name",
         "#endif", "#define PC_WEAK(id, name) extern char id[] __asm__(N64_SYM(name)) __attribute__((weak));", ""]
    L.append("/* One arena per image's data and BSS, laid out as on the N64. */")
    for (img, lo, init, hi), var in zip(arenas, arena_var):
        L.append(f"__attribute__((aligned(16))) uint8_t {var}[0x{(lo & 15) + hi - lo:X}];   "
                 f"/* N64 0x{lo:08X}-0x{hi:08X} from +0x{lo & 15:X}: host addresses keep the N64 alignment */")
    L += ["", "/* Own storage: host layout. */"]
    for s in own:
        L.append(f"__attribute__((aligned(16))) uint8_t {own_var[id(s)]}[{max(s.host_size, 1)}] "
                 f"__asm__(N64_SYM({c_str(sorted(s.names)[0])}));   /* N64 0x{s.addr:08X}, {s.n64_size} B: {s.disp} */")
    sets = []
    layout = {"arenas": {}, "sets": [], "own": []}
    for i, (img, lo, init, hi) in enumerate(arenas):
        layout["arenas"][arena_var[i]] = [lo, (lo & 15) + hi - lo]
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
            base, off = arena_var[s.arena], s.addr - (arenas[s.arena][1] & ~15)
            names = sorted(s.names)
        for n in names:
            sets.append((n, base, off))
            layout["sets"].append([n, base, off])
    for s in sorted(syms.values(), key=lambda x: x.addr):
        if s.end_marker:   # one past an overlay's BSS (the loaders pass it as bssEnd)
            for n in sorted(s.names):
                sets.append((n, arena_var[s.arena], s.addr - (arenas[s.arena][1] & ~15)))
                layout["sets"].append(list(sets[-1]))
    if sets:
        L += ["", "/* Symbols at their N64 offsets in the arenas, or at the matching offset in own storage. */",
              "__asm__("]
        for n, base, off in sets:
            L.append(f"    \".globl \" N64_SYM(\"{n}\") \"\\n.set \" N64_SYM(\"{n}\") \", \" N64_SYM(\"{base}\")"
                     f" \" + 0x{off:X}\\n\"")
        L += [");", ""]
    if thunks:
        L += ["/* Labels the game calls for a function defined under another name (symbols.json). */",
              "#if defined(__aarch64__)", "#define PC_THUNK(n, t) \".globl \" N64_SYM(n) \"\\n.p2align 2\\n\" "
              "N64_SYM(n) \":\\n\\tb \" N64_SYM(t) \"\\n\"",
              "#elif defined(__x86_64__)", "#define PC_THUNK(n, t) \".globl \" N64_SYM(n) \"\\n.p2align 4\\n\" "
              "N64_SYM(n) \":\\n\\tjmp \" N64_SYM(t) \"\\n\"",
              "#else", "#error \"gen_data: add a branch for this architecture\"", "#endif",
              "__asm__(\".text\\n\""]
        L += [f"    PC_THUNK(\"{n}\", \"{RENAMED.get(t, t)}\")" for n, t, _ in thunks]
        L += ["    \".previous\\n\");", ""]

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
    L.append("static const PcDataArena pc_arenas[] = {")
    for i, (img, lo, init, hi) in enumerate(arenas):
        f, n = arena_first[i]
        L.append(f"    {{{IMAGE_INDEX[img]}, 0x{lo:08X}, 0x{init:08X}, 0x{hi:08X}, {arena_var[i]} + {lo & 15}, {f}, {n}}},")
    L += ["};", ""]
    L.append(f"static const PcDataFunc pc_funcs[{max(len(funcs), 1)}] = {{")
    for (addr, img), host in sorted(funcs.items()):
        L.append(f"    {{0x{addr:08X}, {IMAGE_INDEX[img]}, {weak[host]}, {c_str(host)}}},")
    L += ["};", ""]
    names = sorted((s.addr, n) for s in ordered if not s.cdef for n in sorted(s.names)[:1])
    L.append(f"static const PcDataName pc_names[{max(len(names), 1)}] = {{")
    L += [f"    {{0x{ad:08X}, {c_str(n)}}}," for ad, n in names]
    L += ["};", ""]
    L += ["static PcDataState pc_state;",
          "const PcDataTables pc_data_tables = {",
          "    pc_images, sizeof pc_images / sizeof pc_images[0], pc_arenas, sizeof pc_arenas / sizeof pc_arenas[0],",
          f"    pc_syms, {len(sym_rows)}, pc_runs, {len(runs)}, pc_funcs, {len(funcs)}, pc_names, {len(names)},",
          f"    0x{HEAP_LO:08X},   /* the game heap starts at the end of the game's BSS (docs/port/ASSETS.md) */",
          "    &pc_state,",
          "};", "",
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


def check(exe, layout_path, nm):
    """Verify the linked executable: every .set symbol at its base + offset, own storage at its host size."""
    layout = json.loads(Path(layout_path).read_text())
    out = subprocess.run([nm, "-S", "--defined-only", str(exe)], capture_output=True, text=True, check=True).stdout
    addr, size = {}, {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4:
            addr[p[3]], size[p[3]] = int(p[0], 16), int(p[1], 16)
        elif len(p) == 3:
            addr[p[2]] = int(p[0], 16)
    bad = []
    for var, (lo, n) in layout["arenas"].items():
        if size.get(var) != n:
            bad.append(f"arena {var}: size {size.get(var)} != {n}")
    for name, n in layout["own"]:
        if size.get(name) != n:
            bad.append(f"own {name}: size {size.get(name)} != {n}")
    for name, base, off in layout["sets"]:
        if name not in addr or base not in addr:
            bad.append(f"{name}: not in the executable")
        elif addr[name] - addr[base] != off:
            bad.append(f"{name}: at {base}+0x{addr[name] - addr[base]:X}, expected +0x{off:X}")
    for b in bad[:50]:
        print("gen_data check:", b)
    print(f"gen_data check: {len(layout['sets'])} placements, {len(layout['own'])} own, "
          f"{len(layout['arenas'])} arenas: {'OK' if not bad else f'{len(bad)} errors'}")
    return 1 if bad else 0


if __name__ == "__main__":
    main()
