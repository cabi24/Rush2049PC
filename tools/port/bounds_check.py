#!/usr/bin/env python3
"""Find game code that reaches outside the extent its own file declares for a data symbol (docs/port/OPTIMIZED.md 2.7).

    python3 tools/port/bounds_check.py [--build build/pc] [-j 2] [--reuse] [--list]
    python3 tools/port/bounds_check.py --reuse --only src/blob/foo.c --dump     # one file's accesses

The game C often declares a data symbol smaller than its code uses it: `extern f32 D_80140620;` written through
`((f32 (*)[2]) &D_80140620)[i][1]`, `(&D_x)[n]`, `*(T *)((u8 *)&D_x + off)`. IDO doesn't mind. GCC, optimizing, takes
the declaration's size for the object's. Once the loop optimizer has made an indexed reference of such an access
(`MEM[(float *)&D_x + 4B + ivtmp_5 * 8]`, a TARGET_MEM_REF), get_ref_base_and_extent gives it offset 0 and the
declared size as its extent: two stores at different places of a 4-byte object are two stores to the same 4 bytes,
and dead-store elimination drops the first. That made the release build unable to steer (mingw-w64 12, GCC 13).
-fno-aggressive-loop-optimizations doesn't touch it, and no warning says so.

Every live game file (compile_commands.json's game.dir, as endian_check.py) is compiled again with its own flags plus
-O2 -g, GCC's array-bounds warnings and two GIMPLE dumps, into <build>/bounds/. Per file, four sources:

    view   accesses through a pointer computed from a symbol's address plus a variable (`_5 = &D_x + _4;
           MEM[(f32 *)_5 + 4B] = y;`, in the "alias" dump, after inlining) or through a view's variable index, where
           the file declares the symbol with a size (not an incomplete array) the variable can take it past. A
           function that reaches two or more places of such a symbol, one a write, is the class's shape:
           `two offsets` (an array's own index, `D_x[i].f`, is inside it as GCC takes it, and isn't counted);
    const  accesses at a constant offset past the declaration (GCC keeps those offsets exact);
    tmr    the loop optimizer's indexed references on a symbol's address (the "ivopts" dump, before the late
           passes that act on them). Two at the declared size in one function, or one beside another access, with a
           write: `clamped` (this compiler makes the references that are taken for one place);
    warn   GCC's -Warray-bounds=2, -Wstringop-overflow=4, -Wstringop-overread and -Wzero-length-bounds, by the
           symbol their note names.

The weak context copies (build/pc/weak/*.h) never run and are skipped. A finding is keyed by (file, symbol, source);
REVIEWED_FILE lists the keys read and judged, each with its verdict. Exit status 1 if a key is not in it, or if a
`two offsets` or `clamped` finding's verdict doesn't start with "fixed" or "judged" (one read for that shape).
--list prints every finding with its verdict; --write-reviewed writes the keys found in REVIEWED_FILE's format.
Stdlib and binutils, Python 3.9+.
"""
import argparse
import collections
import json
import multiprocessing
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import endian_check as ec  # noqa: E402
import gen_data as gd  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
REVIEWED_FILE = Path(__file__).resolve().parent / "bounds_reviewed.txt"
WARN_FLAGS = ["-Warray-bounds=2", "-Wstringop-overflow=4", "-Wstringop-overread", "-Wzero-length-bounds"]
DUMP_FLAGS = ["-O2", "-g", "-fdump-tree-alias-lineno", "-fdump-tree-ivopts-lineno"] + WARN_FLAGS
WARN_OPTS = ("array-bounds", "stringop-overflow", "stringop-overread", "zero-length-bounds")


# ---------------------------------------------------------------- compiling

def game_commands(build):
    """endian_check's game.dir compiles, skipping entries without an object (the Windows build's resources)."""
    out = []
    for e in json.loads((build / "compile_commands.json").read_text()):
        cmd = shlex.split(e["command"])
        if "-o" in cmd and "/game.dir/" in "/" + cmd[cmd.index("-o") + 1]:
            out.append((e["file"], cmd, e.get("directory", str(build))))
    return out


def compile_one(job):
    src, cmd, cwd, cache, reuse = job
    uid = ec.unit_id(src)
    obj = cache / (uid + ".o")
    warn = cache / (uid + ".warn")
    dumps = list(cache.glob(uid + ".c.*t.alias"))
    if reuse and dumps and obj.exists() and warn.exists() and obj.stat().st_mtime >= Path(src).stat().st_mtime:
        return uid, None
    cmd = list(cmd)
    i = cmd.index("-o")
    cmd[i + 1] = str(obj)
    cmd = [c for c in cmd if c != "-w"] + DUMP_FLAGS   # after the build's flags: the same at every build type
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, env=dict(os.environ, LC_ALL="C"))   # ASCII quotes
    warn.write_text(r.stderr)
    return uid, (r.stderr[-400:] if r.returncode else None)


# ---------------------------------------------------------------- types

def size_of(t):
    if t is None:
        return None
    if t[0] == "ptr":
        return 8
    if t[0] == "arr":
        e = size_of(t[1])
        return None if e is None or t[2] is None else e * t[2]
    return gd.host_size(t[1])


def canon_t(t):
    """endian_check's ('c', canon) | ('ptr', inner) | ('arr', elem, n) as a gen_data canon tuple."""
    if t is None:
        return None
    if t[0] == "ptr":
        return ("ptr",)
    if t[0] == "arr":
        e = canon_t(t[1])
        return ("array", e, t[2]) if e else None
    return t[1]


def walk(c, chain):
    """(offset, canon type, reach) after a ref chain [('.', field) | ('[', index)] from canon type c; None where
    the chain can't be followed. reach: 0, or the end of the farthest array the chain indexes by a variable (inf
    for an array of unknown length)."""
    off, var = 0, 0
    for kind, x in chain:
        if c is None:
            return None
        if kind == ".":
            if c[0] not in ("struct", "union"):
                return None
            m = next((m for m in c[3] if m[0] == x), None)
            if m is None:
                return None
            off += m[1] or 0
            c = m[2]
        else:
            if c[0] != "array":
                return None
            es = gd.host_size(c[1])
            if es is None:
                return None
            if ec._INT.match(x):
                off += int(x) * es
            else:
                var = max(var, off + es * c[2] if c[2] else float("inf"))
            c = c[1]
    return off, c, var


# ---------------------------------------------------------------- the GIMPLE

_SSA_NAME = re.compile(r"^(?:[A-Za-z_][\w.]*)?_\d+(?:\((?:D|ab)\))?$")
_ADDR_PLUS = re.compile(r"^&(.+?) \+ (\S+)$")
_PTR_PLUS = re.compile(r"^(\S+) \+ (\S+)$")
_CAST = re.compile(r"^\(([^()]*)\) (\S+)$")
_PHI = re.compile(r"^PHI <(.*)>$")


def is_ssa(s):
    return bool(_SSA_NAME.match(s))


def parse_ref(s):
    """endian_check's parse_ref, and `(*_5)[1]`, `(*_5).f`: a dereference with a chain."""
    s = s.strip()
    if s.startswith("(*"):
        j = ec.balanced(s, 0, "(", ")")
        prim, chain = ec.parse_ref(s[1:j])
        return prim, chain + ec._chain(s, j + 1)
    return ec.parse_ref(s)


class Unit:
    def __init__(self, uid, T, weak):
        self.uid, self.T, self.weak = uid, T, weak
        self.src = uid.replace("__", "/") + ".c"
        self.decl = {}   # symbol -> (canon type, size)
        for name, (toff, ext) in T.gvars.items():
            if toff is None:
                continue
            c = gd.canon(T.dies, toff, T.memo)
            self.decl[name] = (c, gd.host_size(c))
        self.acc = []    # (func, sym, off, width, var, rw, line, via, index)

    def pointee(self, f, p):
        s = f.decls.get(p) or f.params.get(p)
        if not s:
            # an SSA name of a declared pointer (`e_2` of `EmitA * e;`) has its variable's type: until 2026-10-28
            # its fields were all taken for one place of unknown width, which hid two places written
            m = ec._SSA.match(p)
            s = (f.decls.get(m.group(1)) or f.params.get(m.group(1))) if m and m.group(1) else None
        t = self.T.from_str(s or "")
        return canon_t(t[1]) if t and t[0] == "ptr" else None

    def beyond(self, sym, base, reach):
        """Whether arrays indexed by a variable from `base` in the symbol may run past its declaration."""
        size = self.decl[sym][1]
        return bool(reach) and size is not None and base + reach > size

    def ptype(self, s):
        t = self.T.from_str(s)
        return canon_t(t[1]) if t and t[0] == "ptr" else None

    def ref(self, f, derived, s):
        """(sym, offset, width, variable?, via, index) of a memory reference to a symbol, or None. via: decl (the
        symbol's own type), mem (a MEM_REF at a constant offset), view (through a pointer computed from its
        address, or a view's variable index); index: the variable parts' names."""
        try:
            prim, chain = parse_ref(s)
        except (ValueError, IndexError):
            return None
        if prim[0] == "var":
            sym = prim[1]
            if sym not in self.decl:
                return None
            r = walk(self.decl[sym][0], chain)
            return (sym, r[0], gd.host_size(r[1]), False, "decl", var_names(chain)) if r else None
        if prim[0] in ("deref", "arrow"):
            p = prim[1]
            if p not in derived:
                return None
            sym, poff, pvar, idx = derived[p]
            c = self.pointee(f, p)
            r = walk(c, chain) if c else None
            if r is None:
                return (sym, poff, None, True, "view", idx) if pvar else None
            return (sym, poff + r[0], gd.host_size(r[1]), pvar or self.beyond(sym, poff, r[2]), "view",
                    join(idx, var_names(chain)))
        if prim[0] == "mem":
            _, at, ptype, base, off = prim
            c = self.ptype(ptype)
            if at:
                c = canon_t(self.T.from_str(at)) or c
            if base[0] == "addr":
                bprim, bchain = base[1]
                if bprim[0] != "var" or bprim[1] not in self.decl:
                    return None
                sym = bprim[1]
                b = walk(self.decl[sym][0], bchain)
                r = walk(c, chain) if c else None
                if b is None or r is None:
                    return None
                # a view of the symbol (another type than its own) indexed by a variable
                out = self.beyond(sym, b[0] + off, r[2])
                return (sym, b[0] + off + r[0], gd.host_size(r[1]), out, "view" if out else "mem",
                        join(var_names(bchain), var_names(chain)))
            if base[0] == "ssa" and base[1] in derived:
                sym, poff, pvar, idx = derived[base[1]]
                r = walk(c, chain) if c else None
                if r is None:
                    return (sym, poff + off, None, True, "view", idx) if pvar else None
                return (sym, poff + off + r[0], gd.host_size(r[1]), pvar or self.beyond(sym, poff + off, r[2]),
                        "view", join(idx, var_names(chain)))
        return None

    def address(self, f, derived, s):
        """(sym, offset, variable?, index) of an address expression, or None."""
        s = s.strip()
        m = _CAST.match(s)
        if m:
            s = m.group(2)
        if s in derived:
            return derived[s]
        if s.startswith("&"):
            inner, plus = s[1:], None
            m = _ADDR_PLUS.match(s)
            if m:
                inner, plus = m.group(1), m.group(2)
            try:
                prim, chain = parse_ref(inner)
            except (ValueError, IndexError):
                return None
            if prim[0] == "var" and prim[1] in self.decl:
                r = walk(self.decl[prim[1]][0], chain)
                if r is None:
                    return None
                # an index of the symbol's own array type: inside the declaration, as GCC takes it
                sym, off, var, idx = prim[1], r[0], False, var_names(chain)
            elif prim[0] == "mem" and prim[3][0] == "addr" and prim[3][1][0][0] == "var" \
                    and prim[3][1][0][1] in self.decl:
                sym = prim[3][1][0][1]
                b = walk(self.decl[sym][0], prim[3][1][1])
                c = self.ptype(prim[2])
                r = walk(c, chain) if c else (0, None, False) if not chain else None
                if b is None or r is None:
                    return None
                off, var = b[0] + prim[4] + r[0], self.beyond(sym, b[0] + prim[4], r[2])
                idx = join(var_names(prim[3][1][1]), var_names(chain))
            elif prim[0] in ("deref", "arrow") or (prim[0] == "mem" and prim[3][0] == "ssa"):
                p = prim[1] if prim[0] != "mem" else prim[3][1]
                if p not in derived:
                    return None
                sym, poff, pvar, idx = derived[p]
                if prim[0] == "mem":
                    c = self.ptype(prim[2])
                    poff += prim[4]
                else:
                    c = self.pointee(f, p)
                r = walk(c, chain) if c else (0, None, False) if not chain else None
                if r is None:
                    return sym, poff, True, idx
                off, var, idx = poff + r[0], pvar or self.beyond(sym, poff, r[2]), join(idx, var_names(chain))
            else:
                return None
            if plus is not None:
                if ec._INT.match(plus):
                    off += int(plus)
                else:
                    var, idx = True, join(idx, plus)
            return sym, off, var, idx
        m = _PTR_PLUS.match(s)
        if m and m.group(1) in derived:
            sym, off, var, idx = derived[m.group(1)]
            if ec._INT.match(m.group(2)):
                return sym, off + int(m.group(2)), var, idx
            return sym, off, True, join(idx, m.group(2))
        return None

    def function(self, f):
        derived = {}
        stmts = []
        for line, text in f.stmts:
            m = re.match(r"^(.+?) (=|={v}) (.+)$", text.rstrip(";").strip())
            if m:
                stmts.append((line, m.group(1).strip(), m.group(3).strip()))
        # pointers computed from a symbol's address, to a fixed point (PHIs, copies)
        for _ in range(4):
            n = len(derived)
            for line, lhs, rhs in stmts:
                if not is_ssa(lhs) or lhs in derived:
                    continue
                pm = _PHI.match(rhs)
                if pm:
                    args = [re.sub(r"\(\d+\)$", "", a.strip()) for a in pm.group(1).split(",")]
                    ds = [derived.get(a) for a in args if not ec._INT.match(a)]
                    if ds and all(ds) and len({d[0] for d in ds}) == 1:
                        # a pointer that moves (a loop's): another place each time round, past the declaration
                        # unless it is an array's
                        sym = ds[0][0]
                        moves = any(d[2] for d in ds) or self.decl[sym][0][0] != "array"
                        derived[lhs] = (sym, min(d[1] for d in ds), moves, lhs)
                    continue
                a = self.address(f, derived, rhs)
                if a:
                    derived[lhs] = a
            if len(derived) == n:
                break
        for line, lhs, rhs in stmts:
            if not is_ssa(lhs):
                r = self.ref(f, derived, lhs)
                if r:
                    self.acc.append((f.name, r[0], r[1], r[2], r[3], "W", line, r[4], r[5]))
            # GIMPLE loads one memory reference per statement; an operator outside its brackets is arithmetic
            # (inside them, `MEM[(s8 *)p_14 + 1600B]` is the reference's own offset: until 2026-10-28 such loads
            # were taken for additions and not listed, which hid a read beside a write at another offset)
            load = not rhs.startswith(("&", "PHI", "(")) or rhs.startswith("(*")
            if load and not re.search(r" [-+*/%&|^<>]=? | [<>]=? ", bare(rhs)) \
                    and not re.match(r"^\w+ \(", rhs):
                r = self.ref(f, derived, rhs)
                if r:
                    self.acc.append((f.name, r[0], r[1], r[2], r[3], "R", line, r[4], r[5]))


def bare(s):
    """s with its brackets emptied: a MEM_REF's `[(s8 *)p_14 + 1600B]` is one operand, not an addition."""
    while True:
        t = re.sub(r"\[[^\[\]]*\]", "", s)
        if t == s:
            return t
        s = t


def var_names(chain):
    return ",".join(x for k, x in chain if k == "[" and not ec._INT.match(x)) or None


def join(a, b):
    return ",".join(x for x in (a, b) if x) or None


_TMR = re.compile(r"MEM\s*(?:<([^<>]*)>\s*)?\[\(([^()]*)\)&(\w+)((?: \+ [^\]]+)+)\]")


def tmrs(text, T, weak):
    """(func, sym, width, rw, line, terms) of the final GIMPLE's TARGET_MEM_REFs on a symbol's address with an
    index."""
    out = []
    for f in ec.read_gimple(text):
        if f.name in weak:
            continue
        for line, s in f.stmts:
            for m in _TMR.finditer(s):
                terms = [x.strip() for x in m.group(4).split(" + ") if x.strip()]
                if all(re.match(r"^-?\d+B$", x) for x in terms):
                    continue   # a constant offset: a MEM_REF, exact
                if m.group(1):
                    t = T.from_str(m.group(1))
                else:
                    tp = T.from_str(m.group(2))
                    t = tp[1] if tp and tp[0] == "ptr" else None
                eq = s.find(" = ")
                rw = "W" if eq < 0 or s.find(m.group(0)) < eq else "R"
                out.append((f.name, m.group(3), size_of(t), rw, line, " + ".join(terms)))
    return out


def warnings(text, unit):
    """(func, sym, option, line, message) of GCC's warnings in this file."""
    out, func, last = [], "?", None
    for line in text.splitlines():
        m = re.search(r": In function '([^']+)'", line)
        if m:
            func = m.group(1)
            continue
        m = re.match(r"^(.*?):(\d+):\d+: warning: (.*) \[-W([\w-]+)(?:=\d*)?\]$", line)
        if m:
            if m.group(4) in WARN_OPTS:
                last = [func, None, m.group(4), int(m.group(2)), m.group(3)]
                out.append(last)
            else:
                last = None
            continue
        m = re.search(r"note: (?:while referencing|at offset .* (?:into|to) (?:destination |source )?object|object) "
                      r"'([^']+)'", line) or \
            re.search(r"note: '([^']+)' declared here", line)
        if m and last is not None and last[1] is None:
            last[1] = m.group(1)
    return [tuple(x) for x in out]


def analyze_unit(job):
    uid, cache, build, readelf = job
    obj = cache / (uid + ".o")
    alias = sorted(cache.glob(uid + ".c.*t.alias"))
    opt = sorted(cache.glob(uid + ".c.*t.ivopts"))
    if not obj.exists() or not alias:
        return uid, None
    out = subprocess.run(gd.dwarf_info_command(readelf) + [str(obj)], capture_output=True, text=True).stdout
    dies = next(iter(gd.parse_readelf(out, str(obj)).values()))
    T = ec.Types(dies)
    weak = ec.weak_names(build, uid)
    u = Unit(uid, T, weak)
    for f in ec.read_gimple(alias[0].read_text(errors="replace")):
        if f.name not in weak:
            u.function(f)
    tm = tmrs(opt[0].read_text(errors="replace"), T, weak) if opt else []
    wr = cache / (uid + ".warn")
    wl = [w for w in (warnings(wr.read_text(errors="replace"), u.src) if wr.exists() else []) if w[0] not in weak]
    decl = {s: (gd.display(dies, T.gvars[s][0]), d[1], d[0][0]) for s, d in u.decl.items()}
    return uid, dict(src=u.src, acc=u.acc, tmr=tm, warn=wl, decl=decl)


# ---------------------------------------------------------------- findings

def findings(r):
    """{(sym, source): {sev, lines, funcs, detail}} of one unit. Sources: view (a variable offset through a pointer
    computed from the symbol's address, or a view's index), const (a constant offset outside the declaration), tmr,
    warn. Severities: 'two offsets' (one function reaches two places of the symbol, one a write, one through a
    variable), 'clamped' (this compiler's indexed TARGET_MEM_REFs there, with another place or a write)."""
    out = collections.defaultdict(lambda: {"sev": set(), "lines": set(), "funcs": set(), "detail": set()})
    decl = r["decl"]
    by = collections.defaultdict(list)
    for a in r["acc"]:
        by[(a[0], a[1])].append(a)
    far = {}
    for (func, sym), v in by.items():
        size = decl.get(sym, ("?", None))[1]
        if size is None:
            continue   # an incomplete array: GCC assumes nothing
        var_out = [a for a in v if a[7] == "view" and a[4]]
        const_out = [a for a in v if a[3] and not a[4] and (a[2] < 0 or a[2] + a[3] > size)]
        far[(func, sym)] = var_out + const_out
        for src, lst in (("view", var_out), ("const", const_out)):
            if lst:
                e = out[(sym, src)]
                e["funcs"].add(func)
                e["lines"] |= {a[6] for a in lst}
        shapes = {(a[2], a[3], a[8] if a[4] else None) for a in v}
        whole = decl[sym][2] != "array" or const_out   # an array's views may stay inside it
        if var_out and whole and len(shapes) >= 2 and any(a[5] == "W" for a in v):
            e = out[(sym, "view")]
            e["sev"].add("two offsets")
            e["detail"].add(f"{func}: " + ", ".join(sorted(f"{o:+d}:{w}{'[' + i + ']' if i else ''}"
                                                           for o, w, i in shapes if o is not None)))
    tby = collections.defaultdict(list)
    for t in r["tmr"]:
        tby[(t[0], t[1])].append(t)
    for (func, sym), v in tby.items():
        size = decl.get(sym, ("?", None))[1]
        if size is None:
            continue
        e = out[(sym, "tmr")]
        e["funcs"].add(func)
        e["lines"] |= {t[4] for t in v}
        others = [a for a in by.get((func, sym), []) if a[7] != "view"]   # the views are the TMRs now
        exact = {t[5] for t in v if t[2] == size}
        writes = any(t[3] == "W" for t in v) or any(a[5] == "W" for a in others)
        beyond = [a for a in others if a[3] and (a[2] < 0 or a[2] + a[3] > size)]
        if writes and (len(exact) >= 2 or (exact and others) or beyond):
            e["sev"].add("clamped")
            e["detail"].add(f"{func}: {len(v)} indexed {sorted({t[5] for t in v})[:3]}"
                            f"{', at the declared size' if exact else ''}"
                            f"{', ' + str(len(others)) + ' direct' if others else ''}")
    for func, sym, opt, line, msg in r["warn"]:
        e = out[(sym or "?", "warn")]
        e["funcs"].add(func)
        e["lines"].add(line)
        e["detail"].add(f"{opt}: {msg[:120]}")
    return out


def load_reviewed():
    rev = {}
    if not REVIEWED_FILE.exists():
        return rev
    for line in REVIEWED_FILE.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        p = line.split(None, 3)
        if len(p) >= 3:
            rev[(p[0], p[1], p[2])] = p[3] if len(p) > 3 else ""
    return rev


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("-j", "--jobs", type=int, default=2)
    ap.add_argument("--reuse", action="store_true", help="reuse the dumps in <build>/bounds newer than their source")
    ap.add_argument("--only", default="", help="comma-separated source paths")
    ap.add_argument("--dump", action="store_true", help="print the --only files' accesses")
    ap.add_argument("--list", action="store_true", help="print every finding, reviewed or not")
    ap.add_argument("--write-reviewed", type=Path, help="write the findings' keys in REVIEWED_FILE's format")
    ap.add_argument("--readelf", default="readelf")
    a = ap.parse_args()
    build = a.build.resolve()
    cache = build / "bounds"
    cache.mkdir(parents=True, exist_ok=True)
    cmds = game_commands(build)
    if a.only:
        want = {str((ROOT / p).resolve()) for p in a.only.split(",") if p}
        cmds = [c for c in cmds if str(Path(c[0]).resolve()) in want]
    with multiprocessing.Pool(a.jobs) as pool:
        for uid, err in pool.imap_unordered(compile_one, [(s, c, w, cache, a.reuse) for s, c, w in cmds],
                                            chunksize=4):
            if err:
                print(f"bounds_check: compile failed: {uid}: {err}", file=sys.stderr)
        results = dict(pool.imap_unordered(analyze_unit, [(ec.unit_id(s), cache, build, a.readelf)
                                                          for s, _, _ in cmds], chunksize=4))
    if a.dump:
        for uid, r in sorted(results.items()):
            if not r:
                continue
            for x in r["acc"]:
                var = f"var[{x[8]}] " if x[4] else ""
                print(f"{r['src']}:{x[6]} {x[0]} {x[5]} {x[1]}{x[2]:+d}:{x[3]} {var}{x[7]} "
                      f"(declared {r['decl'].get(x[1])})")
            for x in r["tmr"]:
                print(f"{r['src']}:{x[4]} {x[0]} {x[3]} TMR {x[1]}:{x[2]} {x[5]} (declared {r['decl'].get(x[1])})")
            for x in r["warn"]:
                print(f"{r['src']}:{x[3]} {x[0]} warn {x[1]} {x[2]}: {x[4]}")
    rev = load_reviewed()
    seen, new, open_ = set(), [], []
    rows = []
    for uid, r in sorted(results.items()):
        if not r:
            continue
        for (sym, source), e in sorted(findings(r).items()):
            key = (r["src"], sym, source)
            seen.add(key)
            sev = ",".join(sorted(e["sev"]))
            decl = r["decl"].get(sym, ("?", None))
            rows.append((key, sev, e, decl))
            if key not in rev:
                new.append(key)
            elif e["sev"] and not rev[key].startswith(("fixed", "judged")):
                open_.append(key)
    n = {s: sum(1 for k in seen if k[2] == s) for s in ("view", "const", "tmr", "warn")}
    print(f"bounds_check: {len(results)} files; (file, symbol) pairs reached outside the declaration: {n['view']} "
          f"through a variable, {n['const']} at a constant offset, {n['tmr']} by an indexed TARGET_MEM_REF, "
          f"{n['warn']} with GCC warnings; "
          f"{sum(1 for x in rows if 'two offsets' in x[1])} two offsets, "
          f"{sum(1 for x in rows if 'clamped' in x[1])} clamped")
    for key, sev, e, decl in rows:
        if a.list or key in new or key in open_:
            tag = "NEW " if key in new else "open" if key in open_ else "    "
            print(f"{tag} {key[0]} {key[1]} {key[2]} [{sev or '-'}] declared {decl[0]} ({decl[1]} bytes) in "
                  f"{', '.join(sorted(e['funcs'])[:4])} lines {','.join(map(str, sorted(e['lines'])[:6]))}")
            for d in sorted(e["detail"])[:4]:
                print(f"         {d}")
            if key in rev and a.list:
                print(f"         verdict: {rev[key]}")
    if a.write_reviewed:
        with open(a.write_reviewed, "w") as fh:
            for key, sev, e, decl in rows:
                fh.write(f"{key[0]} {key[1]} {key[2]} {rev.get(key, 'TODO [' + (sev or '-') + ']')}\n")
    scanned = {r["src"] for r in results.values() if r}
    stale = sorted(k for k in rev if k not in seen and (not a.only or k[0] in scanned))
    if stale and not a.only:
        print(f"bounds_check: {len(stale)} reviewed keys not found by this compiler (kept: other GCC versions)")
    bad = len(new) + len(open_)
    print(f"bounds_check: {len(new)} new, {len(open_)} open" if bad else "bounds_check: none new")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
