#!/usr/bin/env python3
"""Do two builds compile the game to the same code? Compare their game objects unit by unit, debug info left out.

    python3 tools/port/objsame.py BUILD_A BUILD_B                     every game object (BUILD/game_objects.txt)
    python3 tools/port/objsame.py BUILD_A BUILD_B src/blob/AbortCar.c   only these units (source paths, or parts of them)
    python3 tools/port/objsame.py A B --list types_objects.txt        another object list of the build (the -O0 copy)
    python3 tools/port/objsame.py A B --explained FILE                units FILE lists may differ (`path  reason`)
    python3 tools/port/objsame.py A.o B.o                             two objects

A unit is `same` when everything the linker and the program get from it is: every allocated section (code, data,
read-only data, BSS, .eh_frame ...) byte for byte with the same size, flags and alignment; every relocation in them
(offset, type, addend, and the symbol by name: a global by its name, a local by where it points, so a static's name
doesn't count); the global symbols each defines (section, offset, size, type, binding, visibility), the commons and
the names it leaves undefined; the local symbols as places (section, offset, size, type), without their names. Not
compared: the debug sections (.debug_*, their relocations), .comment and the notes no code reads, the symbol table's
order, the names of sections symbols and file symbols. So a change of declarations, types that lay out alike,
local variables' or statics' names, comments or line numbers gives `same` for every unit, and any change of
instructions, data, relocated references or exported symbols shows, with the section, the offset and the function
it falls in.

It is the cheap, exact check of a change that should not change code (headers, names, comments): seconds for the
1,078 game objects, against minutes for the synthetic gate (docs/port/PCONLY.md 2), and stronger where it applies,
since it sees code no synthetic path runs. A unit that is not `same` is then explained, or run through the gate.

Line numbers: the default builds compile none into code. The checking builds do: RUSH_CHECK_CASTS and RUSH_NOFIXED
pass __FILE__ and __LINE__ to their PC_N64W checks (port/include/pc_prelude.h), so a change that moves lines in a
file that uses PC_N64W changes that unit's code there. Compare default builds (Debug's build/pc, RelWithDebInfo's
build/opt), and prove the checking builds with their own checks (docs/port/NOFIXED.md).

Units are matched by their source path (an object's path after `CMakeFiles/<target>.dir/`). Exit status: 0 when
every unit compared is `same` (or listed in --explained), 1 when one differs or exists in one build only, 2 on bad
arguments. ELF objects (Linux, both byte orders, 32 or 64 bit); stdlib only.
"""
import argparse
import collections
import concurrent.futures
import os
import re
import struct
import sys
from pathlib import Path

SHT_SYMTAB, SHT_RELA, SHT_REL, SHT_NOBITS, SHT_GROUP = 2, 4, 9, 8, 17
SHF_ALLOC = 0x2
SHN_UNDEF, SHN_ABS, SHN_COMMON = 0, 0xFFF1, 0xFFF2
STB_LOCAL = 0
STT_SECTION, STT_FILE = 3, 4
STT_NAMES = {0: "notype", 1: "object", 2: "func", 3: "section", 4: "file", 5: "common", 6: "tls"}
STB_NAMES = {0: "local", 1: "global", 2: "weak", 10: "unique"}


class Elf:
    """The parts of an ELF relocatable object objsame compares."""

    def __init__(self, path):
        self.path = path
        data = Path(path).read_bytes()
        if data[:4] != b"\x7fELF":
            raise ValueError(f"{path}: not an ELF object")
        cls, enc = data[4], data[5]
        self.is64 = cls == 2
        e = "<" if enc == 1 else ">"
        if self.is64:
            (_, _, _, _, _, shoff, _, _, _, _, shentsize, shnum, shstrndx) = struct.unpack_from(
                e + "HHIQQQIHHHHHH", data, 16)
            shfmt = e + "IIQQQQIIQQ"
        else:
            (_, _, _, _, _, shoff, _, _, _, _, shentsize, shnum, shstrndx) = struct.unpack_from(
                e + "HHIIIIIHHHHHH", data, 16)
            shfmt = e + "IIIIIIIIII"
        self.sections = []
        for i in range(shnum):
            name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
                shfmt, data, shoff + i * shentsize)
            self.sections.append({"name": name, "type": typ, "flags": flags, "off": off, "size": size,
                                  "link": link, "info": info, "align": align, "entsize": entsize})
        strtab = self._bytes(data, self.sections[shstrndx])
        for s in self.sections:
            s["name"] = _cstr(strtab, s["name"])
        # a section's key: its name, and its rank among sections of that name (-ffunction-sections, COMDAT groups)
        seen = collections.Counter()
        for i, s in enumerate(self.sections):
            s["key"] = f"{s['name']}#{seen[s['name']]}" if seen[s["name"]] else s["name"]
            seen[s["name"]] += 1
            s["data"] = b"" if s["type"] == SHT_NOBITS else self._bytes(data, s)
        self.symbols = []
        for s in self.sections:
            if s["type"] != SHT_SYMTAB:
                continue
            names = self._bytes(data, self.sections[s["link"]])
            esz = 24 if self.is64 else 16
            for j in range(s["size"] // esz):
                if self.is64:
                    nm, info, other, shndx, value, size = struct.unpack_from(e + "IBBHQQ", data, s["off"] + j * esz)
                else:
                    nm, value, size, info, other, shndx = struct.unpack_from(e + "IIIBBH", data, s["off"] + j * esz)
                self.symbols.append({"name": _cstr(names, nm), "bind": info >> 4, "type": info & 0xF,
                                     "vis": other & 3, "shndx": shndx, "value": value, "size": size})
        self.relocs = {}   # target section index -> [(offset, type, symbol index, addend)]
        for s in self.sections:
            if s["type"] not in (SHT_RELA, SHT_REL):
                continue
            rela = s["type"] == SHT_RELA
            if self.is64:
                fmt, esz = (e + "QQq", 24) if rela else (e + "QQ", 16)
            else:
                fmt, esz = (e + "IIi", 12) if rela else (e + "II", 8)
            out = []
            for j in range(s["size"] // esz):
                f = struct.unpack_from(fmt, data, s["off"] + j * esz)
                off, info = f[0], f[1]
                sym, typ = (info >> 32, info & 0xFFFFFFFF) if self.is64 else (info >> 8, info & 0xFF)
                out.append((off, typ, sym, f[2] if rela else None))
            self.relocs.setdefault(s["info"], []).extend(out)

    @staticmethod
    def _bytes(data, s):
        return data[s["off"]:s["off"] + s["size"]]

    def compared_sections(self):
        """Index -> section for what the program gets: the allocated sections (code, data, BSS, .eh_frame ...)."""
        return {i: s for i, s in enumerate(self.sections)
                if s["flags"] & SHF_ALLOC and s["type"] not in (SHT_RELA, SHT_REL, SHT_SYMTAB)}

    def sym_key(self, idx, addend):
        """A relocation's target as text: a global by name (and the addend), a local by its section and offset."""
        if idx == 0:
            return f"<none>{addend:+#x}" if addend is not None else "<none>"
        s = self.symbols[idx]
        a = addend or 0
        if s["bind"] != STB_LOCAL:
            return f"{s['name']}{a:+#x}" if a else s["name"]
        if s["shndx"] == SHN_ABS:
            return f"<abs {s['value'] + a:#x}>"
        if 0 < s["shndx"] < len(self.sections):
            return f"<{self.sections[s['shndx']]['key']}+{s['value'] + a:#x}>"
        return f"<local {s['name']} {a:+#x}>"

    def func_at(self, shndx, off):
        """The function or object symbol covering an offset of a section: for saying where a difference is."""
        best = None
        for s in self.symbols:
            if s["shndx"] == shndx and s["type"] in (1, 2) and s["value"] <= off < s["value"] + max(s["size"], 1):
                best = s["name"]
        return best


def _cstr(blob, off):
    end = blob.find(b"\0", off)
    return blob[off:end if end >= 0 else len(blob)].decode("utf-8", "replace")


def describe(o):
    """What objsame compares in one object, as plain values: {sections}, {relocations}, {symbols}."""
    secs = o.compared_sections()
    sections = {}
    for i, s in secs.items():
        sections[s["key"]] = (s["type"], s["flags"], s["align"], s["entsize"], s["size"], s["data"])
    relocs = {}
    for i, s in secs.items():
        rl = sorted((off, typ, o.sym_key(sym, add)) for off, typ, sym, add in o.relocs.get(i, ()))
        if rl:
            relocs[s["key"]] = rl
    glob, undef, common = {}, set(), {}
    local = collections.Counter()
    for s in o.symbols[1:]:
        if s["type"] in (STT_SECTION, STT_FILE):
            continue
        if s["shndx"] == SHN_UNDEF:
            if s["bind"] != STB_LOCAL:
                undef.add((s["name"], STB_NAMES.get(s["bind"], s["bind"])))
            continue
        if s["shndx"] == SHN_COMMON:
            common[s["name"]] = (s["size"], s["value"])
            continue
        where = "ABS" if s["shndx"] == SHN_ABS else o.sections[s["shndx"]]["key"] if s["shndx"] < len(o.sections) \
            else s["shndx"]
        if s["bind"] == STB_LOCAL:
            # GCC's local labels for statics (`counter.0`) and the statics themselves count by place, not name
            local[(where, s["value"], s["size"], STT_NAMES.get(s["type"], s["type"]))] += 1
        else:
            glob[s["name"]] = (where, s["value"], s["size"], STT_NAMES.get(s["type"], s["type"]),
                               STB_NAMES.get(s["bind"], s["bind"]), s["vis"])
    return {"sections": sections, "relocs": relocs, "globals": glob, "undefined": undef, "commons": common,
            "locals": local}


def compare(pa, pb, detail=3):
    """[] when the objects are the same, else a list of differences (text), at most `detail` per kind."""
    oa, ob = Elf(pa), Elf(pb)
    da, db = describe(oa), describe(ob)
    out = []
    sa, sb = da["sections"], db["sections"]
    for k in sorted(set(sa) | set(sb)):
        if k not in sa or k not in sb:
            out.append(f"section {k} only in {'B' if k not in sa else 'A'}")
            continue
        (ta, fa, aa, ea, za, ca), (tb, fb, ab, eb, zb, cb) = sa[k], sb[k]
        if (ta, fa, aa, ea) != (tb, fb, ab, eb):
            out.append(f"section {k}: type/flags/align {ta}/{fa:#x}/{aa} vs {tb}/{fb:#x}/{ab}")
        if za != zb:
            out.append(f"section {k}: size {za:#x} vs {zb:#x}" + _first_diff(oa, k, ca, cb))
        elif ca != cb:
            out.append(f"section {k}: bytes differ" + _first_diff(oa, k, ca, cb))
    ra, rb = da["relocs"], db["relocs"]
    for k in sorted(set(ra) | set(rb)):
        la, lb = ra.get(k, []), rb.get(k, [])
        if la != lb:
            sla, slb = set(la), set(lb)
            only_a = sorted(sla - slb)[:detail]
            only_b = sorted(slb - sla)[:detail]
            idx = _index(oa, k)
            where = ""
            if only_a or only_b:
                first = (only_a or only_b)[0][0]
                fn = oa.func_at(idx, first) if idx is not None else None
                where = f" (first at {k}+{first:#x}{' in ' + fn if fn else ''})"
            out.append(f"relocations in {k}: {len(la)} vs {len(lb)}{where}; "
                       f"A only {_fmt_rel(only_a)}; B only {_fmt_rel(only_b)}")
    for kind in ("globals", "commons"):
        ga, gb = da[kind], db[kind]
        diff = sorted(n for n in set(ga) | set(gb) if ga.get(n) != gb.get(n))
        if diff:
            out.append(f"{kind}: " + ", ".join(
                f"{n} {ga.get(n, '-')} vs {gb.get(n, '-')}" for n in diff[:detail])
                + (f" (+{len(diff) - detail})" if len(diff) > detail else ""))
    ua, ub = da["undefined"], db["undefined"]
    if ua != ub:
        out.append(f"undefined: A only {sorted(n for n, _ in ua - ub)[:detail]}, "
                   f"B only {sorted(n for n, _ in ub - ua)[:detail]}")
    if da["locals"] != db["locals"]:
        la, lb = da["locals"], db["locals"]
        out.append(f"local symbols: A only {sorted((la - lb).elements())[:detail]}, "
                   f"B only {sorted((lb - la).elements())[:detail]}")
    return out


def _index(o, key):
    for i, s in enumerate(o.sections):
        if s["key"] == key:
            return i
    return None


def _first_diff(o, key, a, b):
    n = min(len(a), len(b))
    off = next((i for i in range(n) if a[i] != b[i]), n)
    idx = _index(o, key)
    fn = o.func_at(idx, off) if idx is not None else None
    return f" from {key}+{off:#x}" + (f" (in {fn})" if fn else "")


def _fmt_rel(rl):
    return "[" + ", ".join(f"{off:#x}:{typ}:{sym}" for off, typ, sym in rl) + "]" if rl else "[]"


UNIT = re.compile(r"CMakeFiles/[^/]+\.dir/(?:\./)?(.*?)(?:\.o|\.obj)$")


def unit_of(path):
    """The source path an object compiles (CMake: CMakeFiles/<target>.dir/<source path>.o)."""
    m = UNIT.search(str(path).replace(os.sep, "/"))
    return m.group(1) if m else str(path)


def objects(build, listname):
    lst = Path(build) / listname
    if not lst.exists():
        sys.exit(f"objsame: no {lst} (a configured and built CMake build directory?)")
    return {unit_of(p): p for p in lst.read_text().split()}


def load_explained(path):
    """`unit  reason` lines (# comments): units that may differ, each with why that is harmless."""
    out = {}
    for line in Path(path).read_text().splitlines():
        line = line.split("#", 1)[0].strip() if not line.lstrip().startswith("#") else ""
        if line:
            f = line.split(None, 1)
            out[f[0]] = f[1] if len(f) > 1 else ""
    return out


def _job(args):
    unit, pa, pb, detail = args
    try:
        return unit, compare(pa, pb, detail)
    except (OSError, ValueError) as e:
        return unit, [f"error: {e}"]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", help="build directory A (or an object)")
    ap.add_argument("b", help="build directory B (or an object)")
    ap.add_argument("units", nargs="*", help="only units whose source path contains one of these")
    ap.add_argument("--list", default="game_objects.txt", help="the build's object list (default game_objects.txt)")
    ap.add_argument("--explained", type=Path, help="`unit  reason` lines: units allowed to differ")
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--detail", type=int, default=3, help="differences shown per kind (default 3)")
    ap.add_argument("-q", "--quiet", action="store_true", help="only the units that differ and the summary")
    a = ap.parse_intermixed_args(argv)
    if Path(a.a).is_file() and Path(a.b).is_file():
        d = compare(a.a, a.b, a.detail)
        print("same" if not d else "DIFFERS\n  " + "\n  ".join(d))
        return 1 if d else 0
    ua, ub = objects(a.a, a.list), objects(a.b, a.list)
    units = sorted(set(ua) | set(ub))
    if a.units:
        units = [u for u in units if any(p in u for p in a.units)]
    explained = load_explained(a.explained) if a.explained else {}
    counts = collections.Counter()
    bad = []
    jobs = [(u, ua[u], ub[u], a.detail) for u in units if u in ua and u in ub]
    for u in units:
        if u not in ua or u not in ub:
            counts["only in one"] += 1
            bad.append(u)
            print(f"objsame: {u:60s} only in {'A' if u in ua else 'B'}")
    with concurrent.futures.ProcessPoolExecutor(max_workers=max(1, a.jobs)) as ex:
        for u, d in sorted(ex.map(_job, jobs, chunksize=16)):
            if not d:
                counts["same"] += 1
                if not a.quiet:
                    print(f"objsame: {u:60s} same")
            elif u in explained:
                counts["differs, explained"] += 1
                print(f"objsame: {u:60s} differs, explained: {explained[u]}")
                if not a.quiet:
                    for line in d:
                        print(f"    {line}")
            else:
                counts["DIFFERS"] += 1
                bad.append(u)
                print(f"objsame: {u:60s} DIFFERS")
                for line in d:
                    print(f"    {line}")
    stale = sorted(u for u in explained if u not in set(units))
    if stale and not a.units:
        print(f"objsame: {len(stale)} --explained units not in the builds: {', '.join(stale[:5])}")
    total = len(units)
    print(f"objsame: {total} units: " + ", ".join(f"{v} {k}" for k, v in sorted(counts.items(),
                                                                              key=lambda kv: -kv[1])))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
