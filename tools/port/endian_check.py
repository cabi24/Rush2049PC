#!/usr/bin/env python3
"""Find byte-order dependence in the game C: storage written at one width and read at another.

    python3 tools/port/endian_check.py [--build build/pc] [-j 2] [--reuse] [--out build/pc/endian/report.txt]
    python3 tools/port/endian_check.py --reuse --only src/blob/foo.c --dump      # one file's accesses
    python3 tools/port/endian_check.py --selftest --check-loader                 # ctest endian_check

On the N64 (big-endian) the first byte of a word is its high byte; on the PC it is the low byte. Wherever the
game writes a value at one width and reads its bytes at another (a u32 colour read as u8[4], an s32 handle read
back as its low s16, flags written as a word and tested as a byte), or reads data the PC loader byte-swapped by a
different width than the code uses, the PC sees other values than the N64. docs/port/ENDIAN.md 2.

How it works:
1. Every live game file (game.dir in compile_commands.json) is compiled again with its own flags plus
   -O1 -fno-inline -fdump-tree-forwprop1-lineno, into build/pc/endian/. GCC's GIMPLE at that point has every
   memory access as one expression with its access type and its base: `MEM[(u8 *)p_3 + 61B]`,
   `D_80140BF0[_5].prim[1]`, `MEM <unsigned int> [(char * {ref-all})&c]`. The object's DWARF gives the
   declared types (gen_data.py's parser), so each access has a host offset, an N64 position and a width.
2. Each access is put in a storage: a data symbol, what a function's parameter points to, what a pointer loaded
   from a storage position points to, what one call returned, a local. A pointer argument connects the caller's
   storage with the callee's parameter at its offset.
3. Values are made (constants, arithmetic, conversions, the loader's elements of initialized data) and
   interpreted (shifted, masked, ordered, compared with a constant, converted, used as an array index), and move
   unchanged in between (same-width loads and stores, arguments, returns, struct assignments). A value made at one
   (position, width) and interpreted at an overlapping other one is a conflict (report section 1).
4. The other sections list every storage accessed at mixed widths (2), s16 reads of a word's second half (2b),
   initialized data read at a width other than the loader's element (3), copies between views (4), initialized
   data passed as bytes (4b) and puns in one file's code (5).
5. Weak context copies (build/pc/weak/*.h) never run and are skipped.

Scores are heuristics; every site needs reading. Conflicts that were read and judged are in REVIEWED and LEFT.
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
import gen_data as gd  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
DUMP_FLAGS = ["-O1", "-fno-inline", "-fdump-tree-forwprop1-lineno"]
BASE_SIZES = {"char": 1, "signed char": 1, "unsigned char": 1, "_Bool": 1, "short int": 2, "short unsigned int": 2,
              "int": 4, "unsigned int": 4, "long int": 8, "long unsigned int": 8, "long long int": 8,
              "long long unsigned int": 8, "float": 4, "double": 8, "long double": 16, "sizetype": 8,
              "ssizetype": 8, "bitsizetype": 16, "__int128": 16, "__int128 unsigned": 16, "void": None}
N64_BASE = dict(gd.N64_BASE, **{"long int": 4, "long unsigned int": 4, "sizetype": 4, "ssizetype": 4})
COPY_FUNCS = {"n64_memcpy": (0, 1, 2), "memcpy": (0, 1, 2), "__builtin_memcpy": (0, 1, 2),
              "n64_bcopy": (1, 0, 2), "bcopy": (1, 0, 2), "memmove": (0, 1, 2), "__builtin_memmove": (0, 1, 2)}


def rel(path):
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


# ---------------------------------------------------------------- 1. compiling with GIMPLE dumps

def game_commands(build):
    """(source, command list) of every game.dir compile in compile_commands.json."""
    out = []
    for e in json.loads((build / "compile_commands.json").read_text()):
        cmd = shlex.split(e["command"])
        o = cmd[cmd.index("-o") + 1]
        if "/game.dir/" not in "/" + o:
            continue
        out.append((e["file"], cmd, e.get("directory", str(build))))
    return out


def unit_id(src):
    return rel(src)[:-2].replace("/", "__")


def compile_one(job):
    src, cmd, cwd, cache, reuse = job
    uid = unit_id(src)
    obj = cache / (uid + ".o")
    dumps = list(cache.glob(uid + ".c.*t.forwprop1"))
    if reuse and dumps and obj.exists() and dumps[0].stat().st_mtime >= Path(src).stat().st_mtime:
        return uid, None
    cmd = list(cmd)
    i = cmd.index("-o")
    cmd[i + 1] = str(obj)
    # after the build's own flags, so the analysis is the same in every build type: its -O1 over an optimized
    # build's -O2, and -g where the build has none (Release)
    cmd = cmd + DUMP_FLAGS + ["-g"]
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return uid, (r.stderr[-400:] if r.returncode else None)


# ---------------------------------------------------------------- 2. types (DWARF)

class Types:
    """One object's DWARF: names -> types, global variables, layouts."""

    def __init__(self, dies):
        self.dies, self.memo = dies, {}
        self.typedefs, self.tags, self.bases, self.gvars = {}, {}, {}, {}
        self.cache = {}
        for off, d in dies.items():
            n = gd._name(d.attrs["DW_AT_name"]) if "DW_AT_name" in d.attrs else None
            if d.tag == "DW_TAG_typedef" and n:
                self.typedefs.setdefault(n, off)
            elif d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and n:
                kw = "struct " if d.tag == "DW_TAG_structure_type" else "union "
                if "DW_AT_declaration" not in d.attrs or kw + n not in self.tags:
                    if kw + n not in self.tags or "DW_AT_declaration" in dies[self.tags[kw + n]].attrs:
                        self.tags[kw + n] = off
            elif d.tag == "DW_TAG_base_type" and n:
                self.bases.setdefault(n, off)
        # File-scope variables: the CU's direct children.
        cu = next((d for d in dies.values() if d.tag == "DW_TAG_compile_unit"), None)
        for d in (cu.children if cu else ()):
            if d.tag != "DW_TAG_variable":
                continue
            spec = dies.get(gd._ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
            src = spec or d
            if "DW_AT_name" not in src.attrs:
                continue
            toff = gd._ref(d) if "DW_AT_type" in d.attrs else gd._ref(src)
            name = gd._name(src.attrs["DW_AT_name"])
            if name not in self.gvars or self.gvars[name][0] is None:
                self.gvars[name] = (toff, "DW_AT_external" in src.attrs)

    def strip(self, off):
        seen = 0
        while off is not None and seen < 20:
            d = self.dies.get(off)
            if d is None or d.tag not in ("DW_TAG_typedef", "DW_TAG_const_type", "DW_TAG_volatile_type",
                                          "DW_TAG_restrict_type", "DW_TAG_atomic_type"):
                return off
            off, seen = gd._ref(d), seen + 1
        return off

    def from_die(self, off):
        """('ptr', pointee) | ('c', canon) for a DIE."""
        key = ("die", off)
        if key in self.cache:
            return self.cache[key]
        s = self.strip(off)
        d = self.dies.get(s) if s is not None else None
        if d is not None and d.tag == "DW_TAG_pointer_type":
            self.cache[key] = None   # recursion guard
            r = ("ptr", self.from_die(gd._ref(d)) if "DW_AT_type" in d.attrs else ("c", ("void",)), off)
        else:
            r = ("c", gd.canon(self.dies, off, self.memo), off)
        self.cache[key] = r
        return r

    def from_str(self, s):
        """A GIMPLE type spelling to ('ptr', inner) | ('c', canon) | ('arr', elem, n), or None."""
        s0 = s
        if s0 in self.cache:
            return self.cache[s0]
        s = s.replace("{ref-all}", "")
        s = re.sub(r"\b(const|volatile|restrict|__restrict)\b", "", s)
        s = " ".join(s.split())
        r = None
        if "(*" in s:
            r = ("ptr", ("c", ("func",), None), None)
        elif s.endswith("*"):
            inner = self.from_str(s[:-1].strip())
            r = ("ptr", inner or ("c", ("void",), None), None)
        else:
            m = re.match(r"^(.*)\[(-?\d+|<unknown>)?(?::(-?\d+))?\]$", s)   # `T[4]`, `T[0:3]`, `T[<unknown>]`
            if m:
                e = self.from_str(m.group(1).strip())
                if m.group(3) is not None and _INT.match(m.group(2) or ""):
                    n = int(m.group(3)) - int(m.group(2)) + 1
                elif m.group(2) is not None and _INT.match(m.group(2)):
                    n = int(m.group(2))
                else:
                    n = None
                r = ("arr", e, n) if e else None
            elif s.startswith(("struct ", "union ")):
                # GCC names an anonymous struct after its typedef: `typedef struct {...} X` prints `struct X`
                off = self.tags.get(s, self.typedefs.get(s.split(" ", 1)[1]))
                r = self.from_die(off) if off is not None else None
            elif s in self.typedefs:
                r = self.from_die(self.typedefs[s])
            elif s in self.bases:
                r = self.from_die(self.bases[s])
            elif s in BASE_SIZES and BASE_SIZES[s]:
                r = ("c", ("base", s, BASE_SIZES[s]), None)
            elif s == "void":
                r = ("c", ("void",), None)
        self.cache[s0] = r
        return r


def tsize(t, n64=False):
    """Host (or N64) size of a type from Types."""
    if t is None:
        return None
    if t[0] == "ptr":
        return 4 if n64 else 8
    if t[0] == "arr":
        e = tsize(t[1], n64)
        return None if e is None or t[2] is None else e * t[2]
    c = t[1]
    if c[0] == "base":
        return N64_BASE.get(c[1], c[2]) if n64 else c[2]
    return gd.n64_layout(c)[0] if n64 else gd.host_size(c)


def as_canon(t):
    """The canon tuple of a Types type (pointers become ('ptr',))."""
    if t is None:
        return None
    if t[0] == "ptr":
        return ("ptr",)
    if t[0] == "arr":
        e = as_canon(t[1])
        return None if e is None else ("array", e, t[2])
    return t[1]


def scalar_class(c):
    if c[0] == "ptr":
        return "p"
    if c[0] == "enum":
        return "i"
    if c[0] == "base":
        return "f" if c[1] in ("float", "double", "long double") else "i"
    return None


def n64_members(c):
    """(name, host offset, N64 offset, type) of a struct's or union's members."""
    out, off = [], 0
    for name, hoff, mt in c[3]:
        s, a = gd.n64_layout(mt)
        if c[0] == "struct":
            off = gd.align(off, a)
            out.append((name, hoff or 0, off, mt))
            off += s or 0
        else:
            out.append((name, 0, 0, mt))
    return out


def scalars(c, lo, hi, hbase=0, nbase=0, path="", out=None, union=None, cap=4096):
    """Scalars of canon type c overlapping host bytes [lo, hi), as (host off, N64 off, host width, N64 width, class,
    path, in byte array, union path)."""
    if out is None:
        out = []
    if len(out) > cap or c is None:
        return out
    k = c[0]
    if k in ("base", "ptr", "enum"):
        hw = gd.host_size(c) or 0
        if hbase < hi and hbase + hw > lo:
            nw = 4 if k == "ptr" else (c[1] if k == "enum" else N64_BASE.get(c[1], c[2]))
            out.append((hbase, nbase, hw, nw, scalar_class(c), path, False, union))
    elif k == "array":
        e, n = c[1], c[2]
        eh = gd.host_size(e) or 0
        en = gd.n64_layout(e)[0] or 0
        if eh <= 0:
            return out
        first = max(0, (lo - hbase) // eh)
        last = (hi - 1 - hbase) // eh
        if n is not None:
            last = min(last, n - 1)
        is_bytes = e[0] == "base" and eh == 1
        for i in range(first, last + 1):
            if is_bytes:
                if hbase + i < hi and hbase + i + 1 > lo:
                    out.append((hbase + i, nbase + i, 1, 1, "i", f"{path}[{i}]", True, union))
            else:
                scalars(e, lo, hi, hbase + i * eh, nbase + i * en, f"{path}[{i}]", out, union, cap)
    elif k in ("struct", "union"):
        size = c[2] or 0
        if hbase >= hi or hbase + size <= lo:
            return out
        for name, hoff, noff, mt in n64_members(c):
            scalars(mt, lo, hi, hbase + hoff, nbase + noff, f"{path}.{name}" if path else name, out,
                    (union or path or "<u>") if k == "union" else union, cap)
    return out


def host_to_n64(c, h):
    """N64 offset of host offset h in canon type c, through the scalar or pad it falls on."""
    if c is None:
        return h
    sc = scalars(c, h, h + 1, cap=8)
    if not sc:
        size = gd.host_size(c)
        # past the declared type (a symbol declared smaller than its storage, or m2c's offsets past a view):
        # host and N64 agree there when the type has no pointers
        return h if size is not None and (h >= size or h < 0) and gd.n64_layout(c)[0] == size else None
    s = sc[0]
    return s[1] + min(h - s[0], max(s[3] - 1, 0))


# ---------------------------------------------------------------- 3. GIMPLE

_LOC = re.compile(r"\[[^\[\]\s]+:(\d+):\d+(?: discrim \d+)?\] ")   # gcc 14 adds discriminators
_FUNC = re.compile(r"^;; Function (\S+) \(")
# A call to a global function of the same file, made through GCC's local alias of it: `func_8001D5C0.localalias
# (state_15(D));`. With -fPIC -fno-semantic-interposition (the x86-64 build's flags, CMakeLists.txt) GCC calls the
# file's own definition through `<name>.localalias`, which binds locally; PIE code (aarch64's) calls `<name>` for most
# of them. The call is to <name>: read as such, the analysis is the same on both (docs/port/LAYOUT.md 9.6).
_LOCALALIAS = re.compile(r"\b([A-Za-z_$][\w$]*)\.localalias\b")
_SSA = re.compile(r"^(.*?)_(\d+)(?:\((?:D|ab)\))?$")
_INT = re.compile(r"^-?\d+$")


def balanced(s, i, op, cl):
    depth = 0
    for j in range(i, len(s)):
        if s[j] == op:
            depth += 1
        elif s[j] == cl:
            depth -= 1
            if depth == 0:
                return j
    raise ValueError("unbalanced")


# a name, or a GIMPLE temporary named after one (`D_80153F08.0_12`, `iftmp.6_167`); `x.f` is a member access
_IDENT = re.compile(r"[A-Za-z_$][\w$]*(?:\.\d+_\d+)?(?:\((?:D|ab)\))?")


def parse_ref(s):
    """A GIMPLE memory or variable reference: (primary, chain) with primary
    ('mem', access type str|None, pointer type str, base, offset) | ('deref', ssa) | ('arrow', ssa) | ('var', name)
    and chain [('.', field) | ('[', index)]; base is ('addr', ref) | ('ssa', name) | ('int', value)."""
    s = s.strip()
    i = 0
    if s.startswith("MEM"):
        i = 3
        at = None
        while i < len(s) and s[i] == " ":
            i += 1
        if s[i] == "<":
            j = balanced(s, i, "<", ">")
            at, i = s[i + 1:j].strip(), j + 1
        while s[i] == " ":
            i += 1
        if s[i] != "[":
            raise ValueError("mem")
        j = balanced(s, i, "[", "]")
        inner = s[i + 1:j]
        i = j + 1
        if not inner.startswith("("):
            raise ValueError("mem type")
        k = balanced(inner, 0, "(", ")")
        ptype = inner[1:k].strip()
        rest = inner[k + 1:].strip()
        off = 0
        m = re.search(r"\s([+-])\s(-?\d+)B$", rest)
        if m:
            off = int(m.group(2)) * (1 if m.group(1) == "+" else -1)
            if off >= 1 << 63:
                off -= 1 << 64
            rest = rest[:m.start()].strip()
        if rest.startswith("&"):
            base = ("addr", parse_ref(rest[1:]))
        elif _INT.match(rest):
            base = ("int", int(rest))
        else:
            base = ("ssa", rest)
        prim = ("mem", at, ptype, base, off)
    elif s.startswith("*"):
        m = _IDENT.match(s, 1)
        if not m:
            raise ValueError("deref")
        prim, i = ("deref", m.group(0)), m.end()
    else:
        m = _IDENT.match(s)
        if not m:
            raise ValueError("ident")
        name, i = m.group(0), m.end()
        if s.startswith("->", i):
            m2 = _IDENT.match(s, i + 2)
            if not m2:
                raise ValueError("arrow")
            prim = ("arrow", name)
            i = m2.end()
            chain = [(".", m2.group(0))]
            return prim, chain + _chain(s, i)
        prim = ("var", name)
    return prim, _chain(s, i)


def _chain(s, i):
    chain = []
    while i < len(s):
        if s[i] == ".":
            m = _IDENT.match(s, i + 1)
            if not m:
                raise ValueError("field")
            chain.append((".", m.group(0)))
            i = m.end()
        elif s[i] == "[":
            j = balanced(s, i, "[", "]")
            chain.append(("[", s[i + 1:j].strip()))
            i = j + 1
        else:
            raise ValueError("chain: " + s[i:i + 20])
    return chain


def split_args(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([<{":
            depth += 1
        elif ch in ")]>}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_int(v):
    try:
        return int(v, 0)
    except (ValueError, TypeError):
        return None


class Func:
    def __init__(self, name):
        self.name, self.decls, self.params, self.stmts = name, {}, {}, []


def read_gimple(text):
    """Functions of a forwprop1 dump: Func with local declarations (name -> type str), parameters and statements
    (line, text)."""
    funcs, f, state = [], None, None
    for raw in text.splitlines():
        m = _FUNC.match(raw)
        if m:
            f = Func(m.group(1))
            funcs.append(f)
            state = "sig"
            continue
        if f is None:
            continue
        if state == "sig":
            if raw.strip() == "{":
                state = "decl"
            elif "(" in raw and raw.rstrip().endswith(")"):
                p = raw[raw.index("(") + 1:raw.rindex(")")]
                for a in split_args(p):
                    mm = re.match(r"^(.*?)([A-Za-z_]\w*)$", a.strip())
                    if mm and a.strip() != "void":
                        f.params[mm.group(2)] = mm.group(1).strip()
            continue
        if raw.startswith("}"):
            f, state = None, None
            continue
        line = raw.strip()
        if state == "decl":
            if not line:
                state = "body"
                continue
            if line.endswith(";") and not line.startswith(("<bb", "[", "#")):
                d = line[:-1]
                mm = re.match(r"^(.*?)\s*([A-Za-z_.$][\w.$]*)((?:\[[^\]]*\])*)$", d)
                if mm:
                    t = mm.group(1).strip()
                    if mm.group(3):
                        t = t + mm.group(3)
                    f.decls[mm.group(2)] = t
                continue
            state = "body"
        if state == "body" and line:
            m = _LOC.search(line)
            ln = int(m.group(1)) if m else 0
            f.stmts.append((ln, _LOCALALIAS.sub(r"\1", _LOC.sub("", line))))
    return funcs


# ---------------------------------------------------------------- 4. per object: accesses

Access = collections.namedtuple("Access", "unit func line rw kind width n64w base hpos pos stride elem "
                                          "pun detail value flow text")


_NAMES = _IDENT
_BINOP = re.compile(r"^(\S+) (\+|-|\*|/|%|<<|>>|&|\||\^|==|!=|<|>|<=|>=|r<<|r>>|w\*|h\*|/\[ex\]|&&|\|\||"
                    r"max|min) (\S+)$")


def eq_use(other):
    return ("test0",) if other in ("0", "0B") else ("eqc",) if _INT.match(other) else ("eqv",)


class DefUse:
    """Where an SSA value comes from (a constant, a load of some width, arithmetic, a parameter, a call) and what
    it is used for (stored, tested, shifted or masked, passed on), within one function."""

    def __init__(self, an, f):
        self.an, self.f = an, f
        self.defs, self.uses, self.load_w = {}, collections.defaultdict(list), {}
        self.call_args = []   # (callee, argument index, argument text, line)
        self.first_line = {}   # SSA name -> the first line it appears on
        for ln, st in f.stmts:
            try:
                self.scan(st, ln)
                for n in _NAMES.findall(st):
                    if ln and n not in self.first_line:
                        self.first_line[n] = ln
            except (ValueError, IndexError):
                pass

    def ssa_in(self, s):
        return [n for n in _NAMES.findall(s) if self.an.is_ssa(self.f, n)]

    def addr_uses(self, s):
        """SSA names in an address or memory expression: an array index is interpreted (its value picks the
        element), a base pointer is not."""
        idx = set(re.findall(r"\[([\w.$]+(?:\((?:D|ab)\))?)\]", s))
        for n in self.ssa_in(s):
            self.uses[n].append(("index",) if n in idx else ("addr",))

    def call_uses(self, fn, args, ln):
        for k, a in enumerate(split_args(args)):
            self.call_args.append((fn, k, a, ln))
            if self.an.is_ssa(self.f, a):
                self.uses[a].append(("call", fn, k))
            else:
                self.addr_uses(a)

    def roots(self, x, depth=0, seen=None):
        """Where x's value comes from without a conversion: ('load', ssa), ('param', ssa), ('const', value) or
        ('other', kind)."""
        seen = seen if seen is not None else set()
        if x in seen or depth > 12:
            return []
        seen.add(x)
        d = self.defs.get(x)
        if d is None:
            return [("param", x)] if x.endswith("(D)") else [("other", "?")]
        if d[0] == "load":
            return [("load", x)]
        if d[0] == "const":
            return [("const", d[1])]
        if d[0] == "flow":
            src = self.roots(d[1], depth + 1, seen)
            if d[2]:
                w = tsize(self.an.T.from_str(d[2]))
                if w is not None:
                    src = [r if r[0] != "load" or self.load_w.get(r[1], 0) <= w else ("other", "convert")
                           for r in src]
            return src
        if d[0] == "phi":
            out = []
            for a in d[1]:
                out += [("const", parse_int(a))] if _INT.match(a) else self.roots(a, depth + 1, seen)
            return out
        return [("other", d[0])]

    def scan(self, st, ln=0):
        an, f = self.an, self.f
        m = re.match(r"^# ([\w.$]+(?:\((?:D|ab)\))?) = PHI <(.*)>$", st)
        if m:
            args = [re.sub(r"\(\d+\)$", "", a).strip() for a in split_args(m.group(2))]
            self.defs[m.group(1)] = ("phi", args)
            for a in args:
                self.uses[a].append(("flow", m.group(1)))
            return
        if st.startswith(("if (", "switch (")):
            inner = st[st.index("(") + 1:st.rindex(")")].strip()
            mm = _BINOP.match(inner)
            for n in self.ssa_in(inner):
                if mm and mm.group(2) in ("==", "!="):
                    self.uses[n].append(eq_use(mm.group(3) if mm.group(1) == n else mm.group(1)))
                else:
                    self.uses[n].append(("cmp",))
            return
        if st.startswith("return ") and st.endswith(";"):
            for n in self.ssa_in(st[7:-1]):
                self.uses[n].append(("ret",))
            return
        if not st.endswith(";"):
            return
        body = st[:-1]
        m = re.match(r"^(.*?) (?:=|=\{v\}) (.*)$", body)
        if m is None:
            mm = re.match(r"^([\w.$]+) \((.*)\)$", body)
            if mm:
                self.call_uses(mm.group(1), mm.group(2), ln)
            return
        lhs, rhs = m.group(1).strip(), m.group(2).strip()
        if not an.is_ssa(f, lhs):
            self.addr_uses(lhs)
            if an.is_ssa(f, rhs):
                self.uses[rhs].append(("store", lhs))
            return
        mm = re.match(r"^([\w.$]+) \((.*)\)$", rhs)
        if _INT.match(rhs) or re.match(r"^-?\d+\.\d+(e[+-]?\d+)?$", rhs):
            self.defs[lhs] = ("const", parse_int(rhs))
        elif mm:
            self.defs[lhs] = ("call", mm.group(1))
            self.call_uses(mm.group(1), mm.group(2), ln)
        elif an.is_mem_expr(f, rhs):
            self.defs[lhs] = ("load",)
            self.addr_uses(rhs)
        elif rhs.startswith("&"):
            self.defs[lhs] = ("addr",)
            self.addr_uses(rhs)
        else:
            c = re.match(r"^\(([^()]*(?:\([^()]*\)[^()]*)*)\) (\S+)$", rhs)
            b = _BINOP.match(rhs)
            if c and an.is_ssa(f, c.group(2)):
                self.defs[lhs] = ("flow", c.group(2), c.group(1))
                self.uses[c.group(2)].append(("flow", lhs))
            elif an.is_ssa(f, rhs):
                self.defs[lhs] = ("flow", rhs, None)
                self.uses[rhs].append(("flow", lhs))
            elif b:
                self.defs[lhs] = ("arith", b.group(2))
                for n, other in ((b.group(1), b.group(3)), (b.group(3), b.group(1))):
                    if not an.is_ssa(f, n):
                        continue
                    if b.group(2) in ("==", "!="):
                        self.uses[n].append(eq_use(other))
                    elif b.group(2) in ("<", ">", "<=", ">="):
                        self.uses[n].append(("cmp",))
                    else:
                        self.uses[n].append(("arith", b.group(2)))
            else:
                self.defs[lhs] = ("arith", "?")
                for n in self.ssa_in(rhs):
                    self.uses[n].append(("arith", "?"))

    def origin(self, x, depth=0, seen=None):
        """Origins of SSA value x: const:N, load:W, convert, arith, param, call:F."""
        seen = seen if seen is not None else set()
        if x in seen or depth > 12:
            return set()
        seen.add(x)
        d = self.defs.get(x)
        if d is None:
            return {"param"} if x.endswith("(D)") else {"?"}
        if d[0] == "const":
            return {f"const:{d[1]}"}
        if d[0] == "load":
            return {f"load:{self.load_w.get(x, '?')}"}
        if d[0] == "flow":
            src = self.origin(d[1], depth + 1, seen)
            if d[2]:   # a cast: a narrowing of a loaded value is a conversion
                w = tsize(self.an.T.from_str(d[2]))
                if w is not None:
                    src = {o if not o.startswith("load:") or o == "load:?" or int(o[5:]) <= w else "convert"
                           for o in src}
            return src
        if d[0] == "phi":
            out = set()
            for a in d[1]:
                out |= {f"const:{parse_int(a)}"} if _INT.match(a) else self.origin(a, depth + 1, seen)
            return out
        if d[0] == "call":
            return {f"call:{d[1]}"}
        return {d[0]}

    def use(self, x, depth=0, seen=None):
        """Uses of SSA value x: store:LHS, test0, eqc, eqv, cmp, arith:OP, call:F, ret, addr."""
        seen = seen if seen is not None else set()
        if x in seen or depth > 12:
            return set()
        seen.add(x)
        out = set()
        for u in self.uses.get(x, ()):
            if u[0] == "flow":
                out |= self.use(u[1], depth + 1, seen)
            elif u[0] == "store":
                out.add((x, u[1]))
            elif u[0] in ("arith", "call"):
                out.add(f"{u[0]}:{u[1]}")
            else:
                out.add(u[0])
        return out

    def sinks(self, x, depth=0, seen=None):
        """Where x's value goes without a conversion: ('store', lhs), ('call', callee, index), ('ret',), or
        ('use', kind) where it is interpreted (arithmetic, ordering, compared with a constant)."""
        seen = seen if seen is not None else set()
        if x in seen or depth > 12:
            return []
        seen.add(x)
        out = []
        for u in self.uses.get(x, ()):
            if u[0] == "flow":
                d = self.defs.get(u[1])
                w = tsize(self.an.T.from_str(d[2])) if d and d[0] == "flow" and d[2] else None
                if w is not None and w < self.load_w.get(x, w):
                    out.append(("use", "convert"))
                else:
                    out += self.sinks(u[1], depth + 1, seen)
            elif u[0] == "store":
                out.append(("store", u[1], x))
            elif u[0] == "call":
                out.append(u)
            elif u[0] == "ret":
                out.append(("ret",))
            elif u[0] in ("arith", "cmp", "eqc", "index"):
                out.append(("use", u[0] if u[0] != "arith" else "arith:" + u[1]))
        return out


class Analyzer:
    def __init__(self, unit, types, weak):
        self.unit, self.T, self.weak = unit, types, weak
        self.accesses, self.links, self.copies, self.calls, self.errors = [], [], [], [], 0
        self.edges, self.made, self.used, self.aggcopies, self.byvalue = [], [], [], [], []

    # -- helpers
    def is_ssa(self, f, name):
        if name in f.decls and (name.startswith("_") or re.search(r"\.\d+_\d+$", name)):
            return True
        if re.match(r"^_\d+(\((D|ab)\))?$", name):
            return True
        m = _SSA.match(name)
        return bool(m and (m.group(1) in f.decls or m.group(1) in f.params) and name not in f.decls)

    def ssa_type(self, f, name):
        if name in f.decls:
            return self.T.from_str(f.decls[name])
        m = _SSA.match(name)
        if m:
            t = f.decls.get(m.group(1)) or f.params.get(m.group(1))
            if t:
                return self.T.from_str(t)
        return None

    def ssa_root(self, name):
        m = _SSA.match(name)
        return m.group(1) if m and m.group(1) else name

    def var_type(self, f, name):
        """Type of a memory variable: local, parameter (rare: addressable) or global."""
        if name in f.decls:
            return self.T.from_str(f.decls[name]), ("local", f.name, name)
        if name in f.params:
            return self.T.from_str(f.params[name]), ("local", f.name, name)
        g = self.T.gvars.get(name)
        if g:
            return self.T.from_die(g[0]), ("sym", name)
        return None, ("sym", name)

    def walk_chain(self, t, chain):
        """Apply a component chain to type t: (type, host offset or None, var-index stride info, path)."""
        off, var, path = 0, None, ""
        for kind, v in chain:
            if t is None:
                return None, None, var, path
            c = as_canon(t) if t[0] != "c" else t[1]
            if kind == ".":
                if c is None or c[0] not in ("struct", "union"):
                    return None, None, var, path
                mem = next(((h, mt) for n, h, mt in c[3] if n == v), None)
                if mem is None:
                    return None, None, var, path
                off += mem[0] or 0
                t = ("c", mem[1], None) if mem[1][0] != "ptr" else ("ptr", ("c", ("void",), None), None)
                path += "." + v
            else:
                if t[0] == "arr":
                    e = t[1]
                elif c is not None and c[0] == "array":
                    e = ("c", c[1], None) if c[1][0] != "ptr" else ("ptr", ("c", ("void",), None), None)
                else:
                    return None, None, var, path
                es = tsize(e) or 0
                idx = parse_int(v)
                if idx is None:
                    if var is None:
                        var = (off, es, tsize(e, True) or 0)   # offset of the array, element host/N64 size
                    path += "[i]"
                else:
                    off += idx * es
                    path += f"[{idx}]"
                t = e
        return t, off, var, path

    # -- resolution of one reference
    def resolve(self, f, ref, ptrsrc):
        """(base type, base identity, host offset of the access in the base, access type, var info, pun check
        type) for a parsed reference, or None."""
        prim, chain = ref
        if prim[0] == "var":
            name = prim[1]
            if self.is_ssa(f, name):
                return None
            bt, ident = self.var_type(f, name)
            at, off, var, path = self.walk_chain(bt, chain)
            return dict(bt=bt, ident=ident, off=off, at=at, var=var, cast=None, path=name + path)
        if prim[0] in ("deref", "arrow"):
            name = prim[1]
            pt = self.ssa_type(f, name) if self.is_ssa(f, name) else None
            pointee = pt[1] if pt and pt[0] == "ptr" else None
            ident, bt, boff, bvar = self.ptr_ident(f, name, pointee, ptrsrc)
            at, off, var, path = self.walk_chain(pointee, chain)
            cast = (pointee, boff) if bt is not pointee else None
            return dict(bt=bt, ident=ident, off=None if off is None or boff is None else off + boff,
                        at=at, var=var or bvar, cast=cast, path=f"{name}->{path.lstrip('.')}")
        # MEM
        _, at_s, pt_s, base, moff = prim
        ptype = self.T.from_str(pt_s)
        acc = self.T.from_str(at_s) if at_s else (ptype[1] if ptype and ptype[0] == "ptr" else None)
        if base[0] == "addr":
            r = self.resolve(f, base[1], ptrsrc)
            if r is None:
                return None
            bt, ident = r["bt"], r["ident"]
            boff = None if r["off"] is None else r["off"] + moff
            bvar = r["var"]
            name = r["path"]
        elif base[0] == "int":
            bt, ident, boff, bvar, name = None, ("abs", base[1] + moff), 0, None, hex(base[1])
        else:
            name = base[1]
            pt = self.ssa_type(f, name) if self.is_ssa(f, name) else None
            pointee = pt[1] if pt and pt[0] == "ptr" else None
            ident, bt, poff, bvar = self.ptr_ident(f, name, pointee, ptrsrc)
            boff = None if poff is None else poff + moff
        at, coff, var, path = self.walk_chain(acc, chain)
        return dict(bt=bt, ident=ident, off=None if coff is None or boff is None else boff + coff, at=at,
                    var=var or bvar, cast=(acc, boff), path=f"MEM[{name}+{moff}]{path}")

    def ptr_ident(self, f, name, pointee, ptrsrc):
        """(identity, declared type, offset, var info) of the storage a pointer SSA name points into:
        ('sym', S) with S's type when it was set from &S in this function; ('param', F, k) for what parameter k
        points to; ('ldssa', x) for what a pointer loaded from memory points to (made ('ld', access) after the
        function); ('retp', F) for what F returned; ('local', F, v) for a local's address; else ('anon', F, v)."""
        if name in ptrsrc:
            ident, bt, off, var = ptrsrc[name]
            return ident, bt or pointee, off, var
        return ("anon", f.name, self.ssa_root(name)), pointee, 0, None

    # -- pointer sources in a function
    def pointer_sources(self, f):
        """SSA name -> (identity, declared type or None, offset or None, var info) for pointers whose storage is
        known (ptr_ident)."""
        src = {}
        pnames = list(f.params)
        for n in set(_NAMES.findall(" ".join(st for ln, st in f.stmts))):
            m = _SSA.match(n)
            if n.endswith("(D)") and m and m.group(1) in pnames:
                t = self.T.from_str(f.params[m.group(1)])
                if t and t[0] == "ptr":
                    src[n] = (("param", f.name, pnames.index(m.group(1))), None, 0, None)
        for _ in range(5):   # to a fixed point: PHIs and what derives from them resolve over passes
            before = dict(src)
            for ln, st in f.stmts:
                m = re.match(r"^([\w.$]+(?:\((?:D|ab)\))?) (?:=|=\{v\}) (.*);$", st) or \
                    re.match(r"^# ([\w.$]+) = PHI <(.*)>$", st)
                if not m:
                    continue
                lhs, rhs = m.group(1), m.group(2).strip()
                phi = st.startswith("# ") and "PHI" in st
                if not self.is_ssa(f, lhs):
                    continue
                if phi:
                    args = [re.sub(r"\(\d+\)$", "", a).strip() for a in split_args(rhs)]
                    found, offs, vars_ = set(), set(), set()
                    for a in args:
                        r = self._addr_ident(f, a[1:], src) if a.startswith("&") else src.get(a)
                        if r:
                            found.add(r[0])
                            offs.add(r[2])
                            vars_.add(r[3])
                        elif self.ssa_root(a) == self.ssa_root(lhs) or a == "0B":
                            continue
                        else:
                            found.add(None)
                    syms = {i for i in found if i and i[0] == "sym"}
                    if len(syms) == 1:   # a walker starting from a parameter and going on through a table
                        found = syms
                    if len(found) == 1 and None not in found:
                        ident = found.pop()
                        bt = next((src[a][1] for a in args if a in src and src[a][0] == ident), None)
                        src[lhs] = (ident, bt, offs.pop() if len(offs) == 1 else None,
                                    vars_.pop() if len(vars_) == 1 else None)
                    continue
                lt = self.ssa_type(f, lhs)
                if lt and lt[0] == "ptr":
                    mm = re.match(r"^([\w.$]+) \(", rhs)
                    if mm and not rhs.startswith("("):
                        src[lhs] = (("retp", mm.group(1), f"{f.name}:{ln}"), None, 0, None)   # one call: helpers return many kinds
                        continue
                    if self.is_mem_expr(f, rhs):
                        src[lhs] = (("ldssa", lhs), None, 0, None)
                        continue
                rhs = re.sub(r"^\([^()]*(?:\([^()]*\)[^()]*)*\) ", "", rhs)   # casts
                if rhs.startswith("&"):
                    r = self._addr_ident(f, rhs[1:], src)
                    if r:
                        src[lhs] = r
                    continue
                mm = re.match(r"^([\w.$]+(?:\((?:D|ab)\))?|&\S+) \+ (\S+)$", rhs)
                if mm:
                    a, b = mm.group(1), mm.group(2)
                    base = self._addr_ident(f, a[1:], src) if a.startswith("&") else src.get(a)
                    if base:
                        k = parse_int(b)
                        src[lhs] = (base[0], base[1], None if k is None or base[2] is None else base[2] + k, base[3])
                    continue
                if rhs in src:
                    src[lhs] = src[rhs]
            if src == before:
                break
        return src

    def _addr_ident(self, f, s, src):
        try:
            ref = parse_ref(s)
        except (ValueError, IndexError):
            return None
        r = self.resolve(f, ref, src)
        if r and r["ident"][0] in ("sym", "param", "ldssa", "retp", "local"):
            return (r["ident"], r["bt"], r["off"], r["var"])
        return None

    # -- the statement pass
    def run(self, funcs):
        for f in funcs:
            if f.name in self.weak:
                continue
            ptrsrc = self.pointer_sources(f)
            self.du = DefUse(self, f)
            self.pending, self.store_w, self.load_of, self.line_of = [], {}, {}, {}
            start, cstart = len(self.accesses), len(self.calls)
            for ln, st in f.stmts:
                try:
                    self.statement(f, ln, st, ptrsrc)
                except (ValueError, IndexError, KeyError, TypeError):
                    self.errors += 1

            def fix(ident):   # a loaded pointer's storage is named by the load
                if ident and ident[0] == "ldssa":
                    i = self.load_of.get(ident[1])
                    return ("ld", i) if i is not None else ("anon", f.name, self.ssa_root(ident[1]))
                return ident
            for i in range(start, len(self.accesses)):
                if self.accesses[i].base[0] == "ldssa":
                    self.accesses[i] = self.accesses[i]._replace(base=fix(self.accesses[i].base))
            for i in range(cstart, len(self.calls)):
                fn, idents = self.calls[i]
                self.calls[i] = (fn, [(fix(x[0]), x[1]) if x else None for x in idents])
            for i, (rw, x, w) in self.pending:   # once every load's width is known
                self.accesses[i] = self.accesses[i]._replace(flow=self.flow(rw, x, w))
            try:
                self.value_flow(f)
            except (ValueError, IndexError, KeyError, TypeError):
                self.errors += 1

    def value_flow(self, f):
        """Edges along which a value moves unchanged: a load stored at the same width, a parameter or return
        value stored or passed on; and the places where values are made (constants, arithmetic) or interpreted,
        on parameter and return nodes ('p', F, k) / ('r', F). Access nodes are ('a', index)."""
        du, site = self.du, lambda ln: (self.unit, f.name, ln)
        pnames = list(f.params)
        ddef = {}
        for n in du.uses:
            m = _SSA.match(n)
            if n.endswith("(D)") and m and m.group(1) in pnames:
                ddef[n] = pnames.index(m.group(1))

        def src_nodes(x, ln, dst):
            for kind, v in du.roots(x):
                if kind == "load" and v in self.load_of:
                    self.edges.append((("a", self.load_of[v]), dst))
                elif kind == "param" and v in ddef:
                    self.edges.append((("p", f.name, ddef[v]), dst))
                elif kind == "const" and v not in (0, -1, None):
                    self.made.append((dst, "const", v, site(ln)))
                elif kind == "other" and v not in ("addr", "?"):
                    if v == "call":
                        d = du.defs.get(x)
                        callee = d[1] if d and d[0] == "call" else None
                        if callee:
                            self.edges.append((("r", callee), dst))
                            continue
                    self.made.append((dst, v, None, site(ln)))

        for i, (rw, x, w) in self.pending:
            if rw != "W":
                continue
            if isinstance(x, str):
                n = len(self.edges)
                src_nodes(x, self.accesses[i].line, ("a", i))
                new = self.edges[n:]
                del self.edges[n:]
                for s, d in new:   # a load stored at another width is a conversion
                    if s[0] == "a" and self.accesses[s[1]].width != w:
                        self.made.append((d, "convert", None, site(self.accesses[i].line)))
                    else:
                        self.edges.append((s, d))
            elif isinstance(x, int) and x not in (0, -1):
                self.made.append((("a", i), "const", x, site(self.accesses[i].line)))
        for fn, k, a, ln in du.call_args:
            dst = ("p", fn, k)
            if _INT.match(a):
                if int(a) not in (0, -1):
                    self.made.append((dst, "const", int(a), site(ln)))
            elif self.is_ssa(f, a):
                src_nodes(a, ln, dst)
        for ln, st in f.stmts:
            if st.startswith("return ") and st.endswith(";"):
                v = st[7:-1].strip()
                if self.is_ssa(f, v):
                    src_nodes(v, ln, ("r", f.name))
                elif _INT.match(v) and int(v) not in (0, -1):
                    self.made.append((("r", f.name), "const", int(v), site(ln)))
        # interpretations of loads, parameters and returned values
        for x, node in list(((v, ("a", i)) for v, i in self.load_of.items())) + \
                [(n, ("p", f.name, k)) for n, k in ddef.items()]:
            for s in du.sinks(x):
                if s[0] == "use":
                    self.used.append((node, s[1], site(self.line_of.get(x) or du.first_line.get(x, 0))))
                elif s[0] == "call":
                    self.edges.append((node, ("p", s[1], s[2])))
                elif s[0] == "ret":
                    self.edges.append((node, ("r", f.name)))
        for x, d in du.defs.items():
            if d[0] == "call":
                for s in du.sinks(x):
                    if s[0] == "use":
                        self.used.append((("r", d[1]), s[1], site(self.line_of.get(x) or du.first_line.get(x, 0))))
                    elif s[0] == "call":
                        self.edges.append((("r", d[1]), ("p", s[1], s[2])))

    def flow(self, rw, x, w):
        """How a written value comes about, or how a read value is used. 'copy': it only moves between memory at
        the same width, or is compared for equality; 'zero': 0 or -1, the same bytes in any order; otherwise what
        makes or interprets it (const, arith:>>, call:f, param, convert)."""
        if x is None:
            return "copy"
        if rw == "W":
            if isinstance(x, int):
                return "zero" if x in (0, -1) else "const"
            org = self.du.origin(x)
            if not org:
                return "?"
            if all(o == f"load:{w}" for o in org):
                return "copy"
            if all(o in ("const:0", "const:-1") for o in org):
                return "zero"
            rest = {o for o in org if o != f"load:{w}" and o not in ("const:0", "const:-1")}
            return ",".join(sorted({"const" if o.startswith("const:") else "convert" if o.startswith("load:")
                                    else o for o in rest}))
        out = set()
        for u in self.du.use(x):
            if isinstance(u, tuple):
                out.add("copy" if self.store_w.get(u) in (None, w) else "convert")
            elif u in ("test0", "eqv", "addr"):
                out.add("copy")
            else:
                out.add(u)
        if not out:
            return "unused"
        if out == {"copy"}:
            return "copy"
        return ",".join(sorted(out - {"copy"}))

    def statement(self, f, ln, st, ptrsrc):
        if st.startswith(("#", "<bb", "goto", "if ", "switch", "return", "else", "case", "default", "<L", "__asm")):
            if st.startswith("return ") and st.endswith(";"):
                v = st[7:-1].strip()
                if v and not self.is_ssa(f, v) and not _INT.match(v):
                    self.ref_access(f, ln, v, "R", ptrsrc, st)
            return
        if not st.endswith(";"):
            return
        body = st[:-1]
        m = re.match(r"^(.*?) (?:=|=\{v\}) (.*)$", body)
        if m is None:
            self.call(f, ln, body, None, ptrsrc, st)
            return
        lhs, rhs = m.group(1).strip(), m.group(2).strip()
        if rhs == "{CLOBBER}" or rhs.startswith("{CLOBBER"):
            return
        lhs_mem = not self.is_ssa(f, lhs)
        value = None
        wi = None
        if lhs_mem:
            value = parse_int(rhs) if _INT.match(rhs) else None
            i = wi = self.ref_access(f, ln, lhs, "W", ptrsrc, st, value=value)
            if i is not None:
                w = self.accesses[i].width
                if self.is_ssa(f, rhs):
                    self.pending.append((i, ("W", rhs, w)))
                    self.store_w[(rhs, lhs)] = w
                else:
                    self.pending.append((i, ("W", value, w)))
        if re.match(r"^[\w.$]+ \(", rhs) or rhs.startswith("__builtin"):
            self.call(f, ln, rhs, lhs, ptrsrc, st)
            return
        if self.is_mem_expr(f, rhs):
            i = self.ref_access(f, ln, rhs, "R", ptrsrc, st)
            if i is not None and wi is not None and self.accesses[i].kind == "agg":
                self.aggcopies.append((i, wi))   # a struct assignment: its bytes move as they are
            if i is not None:
                w = self.accesses[i].width
                if not lhs_mem:
                    self.du.load_w[lhs] = w
                    self.load_of[lhs] = i
                    self.line_of[lhs] = ln
                self.pending.append((i, ("R", None if lhs_mem else lhs, w)))
        elif rhs.startswith("BIT_FIELD_REF") or rhs.startswith("VIEW_CONVERT_EXPR"):
            mm = re.match(r"^(BIT_FIELD_REF|VIEW_CONVERT_EXPR)\s*<(.*)>\s*(?:\((.*)\))?$", rhs)
            if mm:
                inner = mm.group(3) if mm.group(1) == "VIEW_CONVERT_EXPR" else split_args(mm.group(2))[0]
                if inner and self.is_mem_expr(f, inner):
                    self.ref_access(f, ln, inner, "R", ptrsrc, st, note=mm.group(1))

    def is_mem_expr(self, f, s):
        s = s.strip()
        if not s or _INT.match(s) or s.startswith(("&", '"', "(", "-")) or " " in s and not s.startswith("MEM"):
            return False
        if re.match(r"^-?\d+\.\d+(e[+-]?\d+)?$", s) or s.endswith("B") and _INT.match(s[:-1]):
            return False
        if s.startswith("MEM") or s.startswith("*") or "->" in s:
            return True
        m = _IDENT.match(s)
        if not m:
            return False
        if self.is_ssa(f, m.group(0)) and m.end() == len(s):
            return False
        return True

    def call(self, f, ln, s, lhs, ptrsrc, st):
        m = re.match(r"^([\w.$]+) \((.*)\)$", s)
        if not m:
            return
        fn, args = m.group(1), split_args(m.group(2))
        for k, a in enumerate(args):
            if self.is_mem_expr(f, a):
                i = self.ref_access(f, ln, a, "R", ptrsrc, st, note="by value")
                if i is not None and self.accesses[i].kind == "agg":
                    self.byvalue.append((i, fn, k))   # a struct passed by value: the callee's parameter's bytes
        # what each pointer argument points into, to join the caller's view with the callee's parameter type
        idents = []
        for a in args:
            d = self.describe_ptr(f, a, ptrsrc) if a.startswith("&") or self.is_ssa(f, a) else None
            ok = d and d[0][0] in ("sym", "param", "ldssa", "retp", "local") and d[1] is not None
            idents.append((d[0], d[3]) if ok else None)   # the storage and the N64 offset into it
        if any(idents):
            self.calls.append((fn, idents))
        if fn in COPY_FUNCS and len(args) == 3:
            d, s_, n = (args[i] for i in COPY_FUNCS[fn])
            self.copies.append((self.unit, f.name, ln, d, s_, n, self.describe_ptr(f, d, ptrsrc),
                                self.describe_ptr(f, s_, ptrsrc)))

    def elem_size(self, ident):
        """Host size of a symbol's array element (struct arrays only)."""
        if ident[0] != "sym" or ident[1] not in self.T.gvars:
            return None
        t = as_canon(self.T.from_die(self.T.gvars[ident[1]][0]))
        if t is None or t[0] != "array":
            return None
        while t[0] == "array":
            t = t[1]
        return gd.host_size(t) if t[0] in ("struct", "union") else None

    def describe_ptr(self, f, a, ptrsrc):
        """(identity, host offset, type view, N64 offset) of a pointer argument."""
        def n64(bt, off):
            c = as_canon(bt)
            return off if off is None or c is None or c[0] in ("void", "func", "opaque", "ptr") else \
                host_to_n64(c, off)
        try:
            if a.startswith("&"):
                r = self.resolve(f, parse_ref(a[1:]), ptrsrc)
                if r:
                    return (r["ident"], r["off"], self.view_of(r["at"]), n64(r["bt"], r["off"]))
            elif self.is_ssa(f, a):
                pt = self.ssa_type(f, a)
                pointee = pt[1] if pt and pt[0] == "ptr" else None
                ident, bt, off, _ = self.ptr_ident(f, a, pointee, ptrsrc)
                return (ident, off, self.view_of(pointee), n64(bt, off))
        except (ValueError, IndexError):
            pass
        return None

    def view_of(self, t):
        c = as_canon(t)
        if c is None:
            return None
        return tuple((h, w, cls) for h, n, w, nw, cls, p, b, u in scalars(c, 0, gd.host_size(c) or 0, cap=64)) \
            if gd.host_size(c) else None

    def ref_access(self, f, ln, s, rw, ptrsrc, st, value=None, note=None):
        """Record one access; its index in self.accesses, or None."""
        try:
            ref = parse_ref(s)
        except (ValueError, IndexError):
            self.errors += 1
            return None
        r = self.resolve(f, ref, ptrsrc)
        if r is None:
            return None
        at = r["at"]
        if at is None:
            return None
        w = tsize(at)
        nw = tsize(at, True)
        ac = as_canon(at)
        kind = scalar_class(ac) if ac is not None and ac[0] in ("base", "ptr", "enum") else "agg"
        if w is None or w <= 0:
            return None
        ident, bt, off = r["ident"], r["bt"], r["off"]
        btc = as_canon(bt)
        # the pun check: the cast type against the base's declared type at that offset
        pun, detail = None, None
        cast, cast_off = r["cast"] if r["cast"] else (None, None)
        if kind == "p":
            pass   # pointer slots are docs/port/POINTERS.md's: a pointer is never read in parts
        elif cast is not None and btc is not None and cast_off is not None and btc[0] not in ("void", "func", "opaque"):
            pun, detail = self.check_cast(btc, cast_off, cast, at, off, w, kind)
        elif btc is not None and off is not None and btc[0] in ("struct", "union", "array") and kind != "agg":
            # typed chain access: only unions can pun here
            sc = scalars(btc, off, off + w, cap=16)
            if any(x[7] for x in sc):
                widths = {(x[0], x[2]) for x in sc}
                if len(widths) > 1:
                    pun, detail = "union", self.fmt_scalars(sc, off, w)
        # position: N64 offset in the symbol (or element)
        pos, stride, elem = None, None, None
        var = r["var"]
        if off is not None:
            if btc is not None and btc[0] not in ("void", "func", "opaque", "ptr"):
                pos = host_to_n64(btc, off)
            else:
                pos = off
        elif var is not None:
            # var-indexed array: offset inside the element
            aoff, es, ens = var
            if es:
                sub = r.get("in_elem")
                stride = (es, ens)
        if ident[0] == "sym" and btc is not None:
            ec = btc
            while ec[0] == "array":
                ec = ec[1]
            if ec[0] in ("struct", "union"):
                elem = (gd.host_size(ec), gd.n64_layout(ec)[0], type_key_canon(self.T, bt))
        self.accesses.append(Access(self.unit, f.name, ln, rw, kind, w, nw, ident, off, pos, stride, elem, pun,
                                    detail, value, None, (note + ": " if note else "") + st[:200]))
        return len(self.accesses) - 1

    def fmt_scalars(self, sc, off, w):
        parts = []
        for h, n, hw, nw, cls, p, b, u in sc[:6]:
            parts.append(f"{p or '?'}:{hw}@{h - off:+d}" + ("(bytes)" if b else "") + (" in union" if u else ""))
        return ", ".join(parts) + (" ..." if len(sc) > 6 else "")

    def check_cast(self, btc, cast_off, cast, at, off, w, kind):
        """Compare a MEM's access type with the base's declared type at its offset."""
        cc = as_canon(cast)
        if cc is None:
            return None, None
        cw = tsize(cast)
        if cw is None or cw <= 0:
            return None, None
        if gd.host_size(btc) is None and btc[0] != "array":
            return None, None
        dec = scalars(btc, cast_off, cast_off + cw, cap=64)
        acc = scalars(cc, 0, cw, cap=64)
        if not dec:
            size = gd.host_size(btc)
            if size is not None and (cast_off >= size or cast_off < 0):
                return "outside", f"offset {cast_off} outside the {size}-byte declared type"
            return None, None
        if any(x[4] == "p" for x in dec):
            return None, None
        ds = {(h - cast_off, hw) for h, n, hw, nw, cls, p, b, u in dec}
        as_ = {(h, hw) for h, n, hw, nw, cls, p, b, u in acc}
        if ds == as_ or (len(acc) == 1 and len(dec) == 1 and dec[0][2] == acc[0][2] and dec[0][0] == cast_off):
            return None, None
        # merge byte runs: u8[4] read as bytes is fine; only width differences matter
        if all(hw == 1 for _, hw in ds) and all(hw == 1 for _, hw in as_):
            return None, None
        if all(x[6] and is_pad(x[5]) for x in dec) or all(x[6] and is_pad(x[5]) for x in acc):
            return "pad", self.fmt_scalars(dec, cast_off, cw)
        narrow = len(acc) == 1 and any(x[2] > acc[0][2] for x in dec)
        if narrow:
            kind_ = "narrow"
        elif len(acc) == 1 and len(dec) > 1:
            kind_ = "wide"
        else:
            kind_ = "mixed"
        return kind_, self.fmt_scalars(dec, cast_off, cw)


_PAD = re.compile(r"(^|\.)(_?pad|unknown|unk|opaque|filler|fill|tail|res|reserved|spare|skip|gap|field|bytes|raw)"
                  r"[_0-9A-Fa-fx]*(\[\d+\])*$")


def is_pad(path):
    """A byte array m2c or a view left unnamed (pad0EC, unk3C, opaque64): its bytes have no declared width."""
    return bool(_PAD.search(re.sub(r"\[\d+\]$", "", path or "")))


def type_key(T, t):
    """(name, N64 size) of a struct pointee type, for record families."""
    if t is None or t[0] != "c":
        return None
    return type_key_canon(T, t)


def type_key_canon(T, t):
    if t is None:
        return None
    if t[0] == "arr":
        return type_key_canon(T, t[1])
    if t[0] != "c":
        return None
    c = t[1]
    while c[0] == "array":
        c = c[1]
    if c[0] not in ("struct", "union"):
        return None
    name = c[1]
    if name is None and t[2] is not None:
        d = T.dies.get(t[2])
        if d is not None and d.tag == "DW_TAG_typedef" and "DW_AT_name" in d.attrs:
            name = gd._name(d.attrs["DW_AT_name"])
        else:   # array of a typedef: look at the element's DIE
            off = t[2]
            for _ in range(6):
                d = T.dies.get(off)
                if d is None:
                    break
                if d.tag == "DW_TAG_typedef":
                    name = gd._name(d.attrs.get("DW_AT_name", ""))
                    break
                off = gd._ref(d)
    if not name:
        return None
    return (name, gd.n64_layout(c)[0])


def weak_names(build, uid):
    p = build / "weak" / (uid + ".h")
    if not p.exists():
        return set()
    return set(re.findall(r"#pragma weak (\w+)", p.read_text()))


def analyze_unit(job):
    """Parse one unit's dump and DWARF; return its accesses, family links, copies."""
    uid, cache, build, readelf = job
    dumps = sorted(cache.glob(uid + ".c.*t.forwprop1"))
    obj = cache / (uid + ".o")
    if not dumps or not obj.exists():
        return uid, None
    out = subprocess.run([readelf, "--debug-dump=info", str(obj)], capture_output=True, text=True).stdout
    dies = gd.parse_readelf(out, str(obj))[str(obj)]
    T = Types(dies)
    an = Analyzer(uid.replace("__", "/") + ".c", T, weak_names(build, uid))
    funcs = read_gimple(dumps[0].read_text(errors="replace"))
    an.run(funcs)
    # family links: a symbol's element struct type in this file
    links = []
    for name, (toff, ext) in T.gvars.items():
        if toff is None:
            continue
        key = type_key_canon(T, T.from_die(toff))
        if key:
            links.append((name, key))
    # the struct pointee types of each live function's parameters, for the callers' arguments
    params = {}
    for f in funcs:
        if f.name in an.weak:
            continue
        keys = []
        for pname, ptype in f.params.items():
            t = T.from_str(ptype)
            key = type_key(T, t[1]) if t and t[0] == "ptr" else None
            if key is None and t and t[0] == "ptr":
                key = ("scalar", tsize(t[1]) or 0)   # `u32 *colour`, `u8 *rgba`, `void *`
            keys.append(key)
        if any(keys):
            params[f.name] = keys
    pnames = {f.name: list(f.params) for f in funcs if f.name not in an.weak}
    return uid, dict(accesses=[tuple(a) for a in an.accesses], links=links, copies=an.copies, calls=an.calls,
                     params=params, pnames=pnames, edges=an.edges, made=an.made, used=an.used,
                     aggcopies=an.aggcopies, byvalue=an.byvalue, errors=an.errors,
                     funcs=sum(1 for f in funcs if f.name not in an.weak))


# ---------------------------------------------------------------- 5. the loader's element widths

KIND_W = {1: 1, 2: 2, 3: 4, 4: 4, 5: 8, 6: 8, 7: 4, 8: 4, 9: 4}


def loader_map(build):
    """symbol -> (n64 address, initialized, {host offset: element N64 width}) from data.c and data_symbols.txt."""
    dc = (build / "pc-data" / "data.c").read_text()
    runs = [tuple(int(x) for x in m.groups()) for m in re.finditer(
        r"^\s*\{(\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+)\},$", dc[dc.index("pc_runs["):], re.M)]
    arenas = [(int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5)), int(m.group(6)))
              for m in re.finditer(r"\{(\d+), 0x([0-9A-F]+), 0x([0-9A-F]+), 0x([0-9A-F]+), \(uint8_t \*\)\(uintptr_t\)"
                                   r"0x[0-9A-F]+u, (\d+), (\d+)\}", dc)]
    syms = {}
    for m in re.finditer(r'\{"(\w+)", 0x([0-9A-F]+), (\d+), (\d+), \w+, (\d+), (\d+), \d+, ([A-Z_|]+)\}', dc):
        syms[m.group(1)] = (int(m.group(2), 16), int(m.group(3)), int(m.group(4)), int(m.group(5)),
                            int(m.group(6)), m.group(7))

    def fill(rs, base_off, limit=None):
        mp = {}
        for n64o, ho, cnt, rep, ns, hs, kind in rs:
            w = KIND_W.get(kind, 1)
            for r_ in range(max(rep, 1)):
                for i in range(cnt):
                    o = n64o + r_ * ns + i * w - base_off
                    if limit is None or 0 <= o < limit:
                        for j in range(w):   # every byte of the element: (width, its start)
                            mp[o + j] = (w, o)
        return mp

    arena_maps = []
    for lo, init, hi, first, count in arenas:
        arena_maps.append((lo, init, hi, fill(runs[first:first + count], 0)))
    table = {}
    for line in (build / "pc-data" / "data_symbols.txt").read_text().splitlines():
        p = line.split("\t")
        if len(p) < 7 or not p[0].startswith("0x"):
            continue
        addr, storage, n64s = int(p[0], 16), p[1], int(p[2]) if p[2].isdigit() else 0
        for name in p[6].split():
            if storage == "arena":
                for lo, init, hi, mp in arena_maps:
                    if lo <= addr < hi:
                        table[name] = ("arena", addr, addr < init, (lo, mp), n64s)
            elif name in syms:
                a, ns, hs, first, count, flags = syms[name]
                # C-defined data (PC_SYM_CDEF) is the PC compiler's, in host order: not filled by the loader
                table[name] = ("own", a, "BSS" not in flags and "CDEF" not in flags, runs[first:first + count], ns)
    return table


def loader_element(entry, host_off, n64_off):
    """(width, offset of its first byte) of the element the loader swapped over this byte of the symbol: an
    undescribed byte is copied as it is (width 1). None if the symbol is not initialized data."""
    storage, addr, init, data, n64s = entry
    if not init or n64_off is None:
        return None
    if storage == "arena":
        lo, mp = data
        w, start = mp.get(addr + n64_off - lo, (1, addr + n64_off - lo))
        return w, start - (addr - lo)
    # own storage: runs by host offset
    for n64o, ho, cnt, rep, ns, hs, kind in data:
        w = KIND_W.get(kind, 1)
        hw = 8 if kind in (7, 8, 9) else w
        for r_ in range(max(rep, 1)):
            start = ho + r_ * hs
            if start <= host_off < start + cnt * hw:
                return w, n64_off - (host_off - start) % hw
    return 1, n64_off


def loader_width(entry, host_off, n64_off):
    """Element width the loader swapped at this offset of the symbol (None if not initialized data)."""
    e = loader_element(entry, host_off, n64_off)
    return e[0] if e else None


# ---------------------------------------------------------------- 6. analysis over all units

class UF:
    """Record families. A family holds at most one data symbol: a join that would put two symbols' views in one
    family is refused (they are different storage that some view's type name or a generic function shares)."""

    def __init__(self):
        self.p, self.sym = {}, {}

    def find(self, x):
        self.p.setdefault(x, x)
        if x[0] == "sym":
            self.sym.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a == b:
            return True
        sa, sb = self.sym.get(a), self.sym.get(b)
        if sa and sb and sa != sb:
            return False
        root, other = (a, b) if a[0] == "sym" or (b[0] != "sym" and str(a) < str(b)) else (b, a)
        self.p[other] = root
        if sa or sb:
            self.sym[root] = sa or sb
        return True


# Conflicts that were read and judged (docs/port/ENDIAN.md): (source file, regex on the access or the function, why).
# A conflict whose wide writes, or whose narrow reads, or whose interpreting functions all match one rule is listed
# under "Reviewed" with the reason instead of being counted.
REVIEWED = [
    (r"src/blob/hud_render\.c|src/blob/func_8010E0FC\.c|src/blob/entity_collision_detect\.c",
     r"id\.part\.index|EntityHdr", "fixed: the PC view's index is the s32 handle's low half"),
    (r"src/blob/groups/entity_spawn_init/entity_spawn_init\.c", r"handle\.part\.id",
     "fixed: the PC view's id is the s32 handle's low half"),
    (r"src/blob/func_80092FE0\.c|src/blob/groups/carcolor/carcolor\.c", r"PlayerRec|halves\.id",
     "fixed: the PC views' a..f and id are the s32 handles' low halves"),
    (r"src/ovl_b/groups/runtime_b_resource_callbacks/group\.c", r"handle\.parts\.index",
     "fixed: the PC view's index is the s32 handle's low half"),
    (r"src/blob/groups/route_cross/group\.c", r"TrackerLen",
     "not a conflict: LEN(k) is element k+1's word at +8, the counts read are element 0's"),
    (r"src/blob/entity_lod_select\.c", r"^entity_lod_select$",
     "not a conflict: `if (cmd[0] & 0x00FF0000) {}` reads a big-endian command word natively and discards it "
     "(kept for matching); every other read goes through pc_be32_get"),
    (r"src/rom/lib_(b120|b300|b570|b9f0|be40|bfe0|f700)\.c", r".",
     "by design: the controller pak image keeps native u16 links next to {bank, page} bytes (docs/port/ULTRA.md)"),
]
# Initialized data whose conflicts are left to other parts (docs/port/ENDIAN.md 4): a conflict whose values all come
# from the loader's elements of these symbols is listed as reviewed with the reason.
LEFT = {
    "gTargInfo's PC layout (POINTERS.md 9.9)": {"D_80117530"},
}
# Initialized data passed by address to a `void *` parameter that was read and judged (report section 4b,
# docs/port/ENDIAN.md 3.4): (function, argument) -> why.
BYTES_REVIEWED = {
    ("arb_rate_set", 1): "a Vp (s16) for gSPViewport, which the renderer reads as s16",
    ("func_8008E26C", 1): "MBOX_NewObject's matrix, f32 read as words",
    ("camera_target_track", 1): "a Vec3, f32 read as words",
}


def reviewed(writes, reads, uses):
    """The reason a conflict was judged, or None."""
    for unit_re, what_re, why in REVIEWED:
        def ok(a):
            return re.search(unit_re, a.unit) and (re.search(what_re, a.text) or re.search(what_re, a.func))
        if writes and all(ok(a) for a in writes):
            return why
        if reads and all(ok(a) for a in reads):
            return why
        if uses and all(re.search(unit_re, u) and re.search(what_re, fn) for k, (u, fn, ln) in uses):
            return why
    return None


CAP = 24   # descriptions kept per node
GENERIC = 12   # an untyped pointer parameter given more storages than this belongs to a generic helper
# Parameters whose callers' storages are not joined, because the record they point to depends on another argument
# (the tool has no call contexts): (function, parameter index) -> why
NOT_FOLLOWED = {
    ("save_write_data", 0): "StartBlast's data is an s16 car slot with linkToCar, an f32 position without; the "
                            "callers that pass positions do not link",
}


def value_conflicts(results, fam, norm, loader=None):
    """[(score, family, (pos, width) made, made, (pos, width) used, uses)] over the value-flow graph. Initialized
    data is made by the PC loader, element by element at the width it swapped (loader_map)."""
    def key(u, accs, n):
        if n[0] == "a":
            a = accs[n[1]]
            p = norm(a)
            if p is None or a.kind in ("agg", "p") or a.base[0] == "abs":
                return None
            return (fam(a), p, a.n64w)
        if n[0] == "p":
            return (("argval", n[1], n[2]), 0, 0)
        return (("retval", n[1]), 0, 0)

    made, used = collections.defaultdict(set), collections.defaultdict(set)
    succ, pred = collections.defaultdict(set), collections.defaultdict(set)
    reads, writes = set(), set()
    sites = collections.defaultdict(list)   # node -> the accesses there
    for u, r in results.items():
        if not r:
            continue
        accs = [Access(*a) for a in r["accesses"]]
        for i, a in enumerate(accs):
            k = key(u, accs, ("a", i))
            if k:
                (reads if a.rw == "R" else writes).add(k)
                sites[k].append(a)
                e = loader.get(k[0][1]) if loader and k[0][0] == "sym" and a.rw == "R" else None
                if e and e[2] and a.pos is not None:
                    el = loader_element(e, a.hpos if a.hpos is not None else a.pos, a.pos)
                    if el:   # the element the loader swapped over the access's first byte, at its own place
                        lw, start = el
                        made[(k[0], k[1] - (a.pos - start), lw)].add(
                            ("rom", None, (f"{k[0][1]} ({e[0]} 0x{e[1] + start:08X})", "the PC loader", 0)))
        for n, kind, val, s in r["made"]:
            k = key(u, accs, n)
            if k and len(made[k]) < CAP:
                made[k].add((kind, val, s))
        for n, kind, s in r["used"]:
            k = key(u, accs, n)
            if k and len(used[k]) < CAP:
                used[k].add((kind, s))
        for s_, d in r["edges"]:
            ks, kd = key(u, accs, s_), key(u, accs, d)
            if ks and kd and ks != kd:
                succ[ks].add(kd)
                pred[kd].add(ks)

    # what a pointer parameter points to is each caller's storage at the same offsets: where the callee reads it,
    # values come from the callers' storage (and its interpretations are of the callers' bytes); where it writes
    # it, values go to the callers' storage. Values do not cross from one caller's storage to another's.
    by_family = collections.defaultdict(set)
    for k in reads | writes:
        by_family[k[0]].add(k)
    # struct assignments and structs passed by value: their bytes move as they are, so every scalar place inside
    # them moves (and the loader's elements of initialized data with them)
    pnames, agg = {}, []
    for r in results.values():
        pnames.update(r.get("pnames", {}) if r else {})
    for u, r in results.items():
        if not r:
            continue
        accs = [Access(*a) for a in r["accesses"]]
        for si, di in r.get("aggcopies", ()):
            s, d = accs[si], accs[di]
            if norm(s) is not None and norm(d) is not None:
                agg.append((fam(s), norm(s), s.n64w, fam(d), norm(d), s))
        for si, fn, k in r.get("byvalue", ()):
            s, names = accs[si], pnames.get(fn)
            if norm(s) is not None and names and k < len(names):
                agg.append((fam(s), norm(s), s.n64w, ("local", fn, names[k]), 0, s))
    for fs, ps, n, fd, pd, s in agg:
        n = min(n or 0, 256)
        e = loader.get(fs[1]) if loader and fs[0] == "sym" else None
        if e and e[2] and s.pos is not None:
            for off in range(n):
                el = loader_element(e, (s.hpos or 0) + off, s.pos + off)
                if el and el[1] == s.pos + off:   # an element starts here
                    made[(fd, pd + off, el[0])].add(
                        ("rom", None, (f"{fs[1]} ({e[0]} 0x{e[1] + s.pos + off:08X})", "the PC loader", 0)))
        for k in list(by_family.get(fs, ())):
            if ps <= k[1] < ps + n:
                k2 = (fd, pd + k[1] - ps, k[2])
                succ[k].add(k2)
                pred[k2].add(k)
        for k2 in list(by_family.get(fd, ())):
            if pd <= k2[1] < pd + n:
                k = (fs, ps + k2[1] - pd, k2[2])
                succ[k].add(k2)
                pred[k2].add(k)
    esize = getattr(norm, "esize", {})
    for fp, args in getattr(fam, "shared", ()):
        for k in by_family.get(fp, ()):
            for fa, off in args:
                if fa == fp:
                    continue
                e = esize[fa].most_common(1)[0][0] if esize.get(fa) else 0
                k2 = (fa, (k[1] + off) % e if e else k[1] + off, k[2])
                if k in reads:
                    succ[k2].add(k)
                    pred[k].add(k2)
                if k in writes:
                    succ[k].add(k2)
                    pred[k2].add(k)

    def close(sets, nxt):
        work = list(sets)
        while work:
            k = work.pop()
            for k2 in nxt.get(k, ()):
                before = len(sets[k2])
                if before >= CAP:
                    continue
                sets[k2] |= set(list(sets[k])[:CAP - before])
                if len(sets[k2]) != before:
                    work.append(k2)

    close(made, succ)
    close(used, pred)
    by_fam = collections.defaultdict(set)
    for k in set(made) | set(used):
        if k[0][0] not in ("argval", "retval") and k[2]:
            by_fam[k[0]].add((k[1], k[2]))
    rank = {"const": 5, "rom": 5, "arith": 4, "convert": 4}
    out = []
    for f, shapes in by_fam.items():
        shapes = sorted(shapes)
        for i, (p, wa) in enumerate(shapes):
            for q, wb in shapes:
                if (p, wa) == (q, wb) or not (p < q + wb and q < p + wa):
                    continue
                m, us = made.get((f, p, wa)), used.get((f, q, wb))
                if not m or not us:
                    continue
                score = max(rank.get(k, 3) for k, v, s in m)
                if any(s[0] != s2[0] for k, v, s in m for k2, s2 in us):
                    score += 1
                why = reviewed([a for a in sites.get((f, p, wa), ()) if a.rw == "W"],
                               [a for a in sites.get((f, q, wb), ()) if a.rw == "R"], us)
                for reason, syms in LEFT.items():
                    if not why and all(k == "rom" and s[0].split()[0] in syms for k, v, s in m):
                        why = "left: " + reason
                out.append((score, f, (p, wa), m, (q, wb), us, why))
    out.sort(key=lambda x: (-x[0], str(x[1]), x[2], x[4]))
    return out


NEUTRAL = {"W": ("copy", "zero"), "R": ("copy", "unused")}


def interp(a):
    """The access makes or interprets the value (not a same-width move, a zero or an equality test)."""
    return a.flow not in NEUTRAL[a.rw]


def symbol_aliases(build):
    """name -> the first name at its N64 address (data_symbols.txt)."""
    out = {}
    for line in (build / "pc-data" / "data_symbols.txt").read_text().splitlines():
        p = line.split("\t")
        if len(p) >= 7 and p[0].startswith("0x"):
            names = p[6].split()
            for n in names:
                out[n] = names[0]
    return out


def families(results, alias):
    """fam(access) -> the storage it reaches: a data symbol (aliases at one address are one), what a function's
    parameter k points to ('param', F, k), what a pointer loaded from a storage position points to ('pt',
    storage, position), what a function returns ('retp', F), a local, or one function's pointer variable.
    A pointer argument joins the caller's storage with the callee's ('param', F, k), unless callers pass that
    parameter several symbols (a list function's header, a colour helper): then it joins none of them."""
    per_unit = {}
    for r in results.values():
        if r and r["accesses"]:
            accs = [Access(*a) for a in r["accesses"]]
            per_unit[accs[0].unit] = accs
    memo = {}

    def canon(ident, unit, depth=0):
        if ident[0] == "sym":
            return ("sym", alias.get(ident[1], ident[1]))
        if ident[0] == "ld" and depth < 6:
            k = (unit, ident[1])
            if k not in memo:
                memo[k] = None
                a = per_unit.get(unit, [None] * (ident[1] + 1))[ident[1]]
                memo[k] = ("pt", canon(a.base, unit, depth + 1), a.pos) if a else ("anon", unit, ident[1])
            return memo[k] or ("anon", unit, ident[1])
        return tuple(ident)

    # only parameters that point to a struct join storage: a `void *` one (memcpy, bzero, a buffer) takes anything
    typed = {}
    for r in results.values():
        typed.update(r["params"] if r else {})
    links, untyped = collections.defaultdict(set), set()
    for r in results.values():
        if not r or not r["accesses"]:
            continue
        unit = Access(*r["accesses"][0]).unit
        for fn, idents in r["calls"]:
            keys = typed.get(fn, ())
            for k, ident in enumerate(idents):
                if ident and ident[0][0] not in ("ldssa", "anon") and k < len(keys) and keys[k]:
                    links[("param", fn, k)].add((canon(ident[0], unit), ident[1] or 0))
                    if keys[k][0] == "scalar" and keys[k][1] <= 1:
                        untyped.add(("param", fn, k))
    # A parameter's storage is each caller's at the same offsets. Families are not joined (a helper's parameter
    # takes many records, and joins would chain them together); value_conflicts follows values across instead.
    # A `void *` or byte-pointer parameter given more than GENERIC storages (memcpy, bzero, a buffer helper) is left
    # out: what it copies would go from every caller's source to every caller's destination. So are the parameters
    # in NOT_FOLLOWED.
    fam = lambda a: canon(a.base, a.unit)
    fam.shared = [(node, idents) for node, idents in links.items()
                  if (node not in untyped or len(idents) <= GENERIC) and node[1:] not in NOT_FOLLOWED]
    return fam


def analyze(results, alias):
    """(accesses, fam, norm): every access, its storage family, and its N64 position there (in one element for
    arrays of structs)."""
    acc = [Access(*a) for r in results.values() if r for a in r["accesses"]]
    fam = families(results, alias)
    esize = collections.defaultdict(collections.Counter)
    for a in acc:
        if a.elem and a.elem[1]:
            esize[fam(a)][a.elem[1]] += 1

    def norm(a):
        if a.pos is None:
            return None
        f = fam(a)
        if esize.get(f):
            e = esize[f].most_common(1)[0][0]
            if e and (a.base[0] != "sym" or (a.elem and a.elem[1] == e)):
                return a.pos % e
        return a.pos
    norm.esize = esize
    return acc, fam, norm


def low_halves(acc, fam, norm):
    """(storage, position, the read, the word's 4-byte accesses) for each s16 read at +2 of a place that is also
    accessed as a 4-byte integer: the word's low half on the N64, its high half on the PC."""
    by = collections.defaultdict(list)
    for a in acc:
        p = norm(a)
        if p is not None and a.kind == "i" and a.base[0] != "abs":
            by[fam(a)].append((p, a))
    out = []
    for f, items in by.items():
        words = collections.defaultdict(list)
        for p, a in items:
            if a.n64w == 4:
                words[p].append(a)
        for p, a in items:
            if a.n64w == 2 and a.rw == "R" and p % 4 == 2 and p - 2 in words:
                out.append((f, p, a, words[p - 2]))
    return sorted(out, key=lambda x: (str(x[0]), x[1], x[2].unit, x[2].line))


def report(results, build, out, loader):
    acc, fam, norm = analyze(results, symbol_aliases(build))
    lines = []
    w = lines.append

    def site(a):
        return f"{a.unit}:{a.line} {a.func}"

    def initialized(f):
        e = loader.get(f[1]) if f[0] == "sym" else None
        return bool(e and e[2])

    # -- 0. value flow: a value made at one width that reaches a read at another width of the same bytes
    vf = value_conflicts(results, fam, norm, loader)
    w("## 1. Value conflicts: a value made at one width is interpreted at another width of the same bytes")
    w("")
    w("A value is made where it is a constant (not 0 or -1), arithmetic, a conversion, the PC loader's element of")
    w("initialized data or an unknown call's result, and interpreted where it is shifted, masked, ordered, compared")
    w("with a constant, converted or used as an array index. In between it may move unchanged: copied at the same")
    w("width or in a struct assignment, passed as an argument, returned (followed across files).")
    w("Score: 5 a constant or ROM data, 4 arithmetic or a conversion, 3 a call's or parameter's value; +1 across")
    w("files. Conflicts already judged (REVIEWED and LEFT in the tool) are listed last, with the reason.")
    w("")
    open_ = [c for c in vf if not c[6]]
    hist = collections.Counter(min(c[0], 6) for c in open_)
    w("open conflicts by score: " + ", ".join(f"{k}: {v}" for k, v in sorted(hist.items(), reverse=True))
      + f"; reviewed: {len(vf) - len(open_)}")
    for score, f, (p, wa), made, (q, wb), uses, why in open_ + [c for c in vf if c[6]]:
        w(f"  [{score if not why else 'reviewed'}] {fmt_ident(f)}: made at +0x{p:X} as {wa}B, interpreted at "
          f"+0x{q:X} as {wb}B" + (" (initialized data)" if initialized(f) else "") + (f": {why}" if why else ""))
        for kind, val, (u, fn, ln) in sorted(made, key=str)[:12]:
            v = f" {val:#x}" if isinstance(val, int) else ""
            w(f"      made: {kind}{v}  {u}:{ln} {fn}")
        if len(made) > 12:
            w(f"      made: ... {len(made) - 12} more")
        for kind, (u, fn, ln) in sorted(uses, key=str)[:12]:
            w(f"      used: {kind}  {u}:{ln} {fn}")
        if len(uses) > 12:
            w(f"      used: ... {len(uses) - 12} more")
    w("")
    # -- 1. cross-view and in-function conflicts
    groups = collections.defaultdict(list)
    for a in acc:
        p = norm(a)
        if p is None or a.kind in ("agg", "p") or a.base[0] == "abs":
            continue
        groups[fam(a)].append((p, a))
    clusters = []
    for f, items in groups.items():
        items.sort(key=lambda x: (x[0], -x[1].n64w))
        cur, end = [], -1
        for p, a in items:
            if cur and p >= end:
                clusters.append((f, cur))
                cur, end = [], -1
            cur.append((p, a))
            end = max(end, p + (a.n64w or a.width))
        if cur:
            clusters.append((f, cur))
    scored = []
    for f, cl in clusters:
        if len({(p, a.n64w) for p, a in cl}) < 2:
            continue
        pairs = [(x, y) for x in cl for y in cl if x[1].rw == "W" and y[1].rw == "R"
                 and (x[0], x[1].n64w) != (y[0], y[1].n64w)
                 and x[0] < y[0] + y[1].n64w and y[0] < x[0] + x[1].n64w]
        both = [(x, y) for x, y in pairs if interp(x[1]) and interp(y[1])]
        one = [(x, y) for x, y in pairs if interp(x[1]) or interp(y[1])]
        rom = initialized(f) and any(interp(a) and a.rw == "R" for p, a in cl)
        score = 4 if both else 2 if one else 1 if pairs or rom else 0
        if both and any(x[1].flow == "const" for x, y in both):
            score += 1
        if any(x[1].unit != y[1].unit for x, y in (both or one)):
            score += 1
        scored.append((score, f, cl, rom))
    scored.sort(key=lambda x: (-x[0], x[1][0] in ("local", "anon"), str(x[1])))
    w("## 2. Mixed widths: the same bytes accessed at different widths (every access, with its flow)")
    w("")
    w("Score: 4 = a value made at one width (constant, arithmetic, parameter) is interpreted at another (shifted,")
    w("masked, compared, passed on); +1 if the writer stores a constant; +1 if the two are in different files.")
    w("2 = one side only moves the value (copy). 1 = moves on both sides, or ROM data read at its own width.")
    w("Flows: copy (moved at the same width, or compared for equality), zero (0 or -1), const, arith:OP, param,")
    w("call:F, convert (read at another width before the store), cmp, eqc (compared with a constant), index, ret.")
    w("")
    hist = collections.Counter(min(s_, 6) for s_, f, cl, rom in scored)
    w("clusters by score: " + ", ".join(f"{k}: {v}" for k, v in sorted(hist.items(), reverse=True)))
    for score, f, cl, rom in scored:
        lo = min(p for p, a in cl)
        where = " (in one function)" if f[0] in ("local", "anon") else ""
        w(f"  [{score}] {fmt_ident(f)}{where} +0x{lo:X}" + (" (initialized data)" if initialized(f) else "") + ":")
        seen = set()
        for p, a in sorted(cl, key=lambda x: (x[0], x[1].n64w, x[1].rw, x[1].unit, x[1].line)):
            k = (p, a.n64w, a.rw, a.flow, a.unit, a.func)
            if k in seen:
                continue
            seen.add(k)
            val = f" = {a.value:#x}" if a.value is not None else ""
            w(f"      +0x{p:X} {a.rw} {a.n64w}B {a.flow}{val}  {site(a)}  {a.text[:150]}")
    w("")
    # -- 1b. the second s16 of a word read where the word is also accessed whole: the N64's low half, the PC's high
    w("## 2b. Low halves: an s16 read at +2 of a place that is also accessed as a 4-byte integer")
    w("")
    w("On the N64 that s16 is the word's low half; on the PC it is the high half (a handle's 0). A PC view that")
    w("reads the low half first is at +0 and not listed.")
    w("")
    halves = low_halves(acc, fam, norm)
    for f, p, a, whole in halves:
        w(f"  {fmt_ident(f)} +0x{p:X}: {site(a)}  {a.text[:140]}")
        for b in sorted(whole, key=lambda b: (b.unit, b.line))[:4]:
            w(f"      the word: {b.rw} {b.flow}  {site(b)}")
    w("")
    # -- 2. initialized data read at a width other than the loader's
    w("## 3. ROM data: initialized data accessed at a width other than the element width the loader swapped")
    w("")
    rom = collections.defaultdict(list)
    moved = collections.Counter()
    for a in acc:
        if a.base[0] != "sym" or a.kind in ("agg", "p"):
            continue
        e = loader.get(a.base[1])
        if not e or not e[2] or a.pos is None:
            continue
        el = loader_element(e, a.hpos, a.pos)
        if el is None or (el[0] == a.n64w and el[1] == a.pos) or (el[0] == 1 and a.n64w == 1):
            continue
        if el[0] < a.n64w and not interp(a):
            moved[a.base[1]] += 1   # a word that only moves the bytes keeps their order (a colour union's copy)
            continue
        rom[a.base[1]].append((a.pos, el[0], a))
    for sym in sorted(rom):
        w(f"  {sym} ({loader[sym][0]} 0x{loader[sym][1]:08X}):")
        seen = set()
        for pos, lw, a in sorted(rom[sym], key=lambda x: (x[0], x[2].unit, x[2].line)):
            k = (pos, a.rw, a.n64w, a.unit, a.func)
            if k not in seen:
                seen.add(k)
                w(f"      +0x{pos:X} {a.rw} {a.n64w}B {a.flow} over {lw}B elements  {site(a)}  {a.text[:140]}")
    if moved:
        w(f"  Not listed: {sum(moved.values())} accesses that only move narrower elements as a wider value (a copy "
          f"keeps their byte order; section 1 checks where the value is read): "
          + ", ".join(f"{s_} ({n})" for s_, n in sorted(moved.items())))
    w("")
    # -- 3. copies between views
    w("## 4. Copies (memcpy/bcopy) between views whose scalar widths differ")
    w("")
    ncopies = 0

    def fmt(v):
        return " ".join(f"{w_}@{h}" for h, w_, c in v[2][:10]) + (" ..." if len(v[2]) > 10 else "")

    for r in results.values():
        for unit, func, ln, d, s_, n, dd, sd in (r["copies"] if r else ()):
            if not dd or not sd or not dd[2] or not sd[2]:
                continue
            # overlapping scalars of different widths (one view having more fields than the other is fine)
            if any(h1 < h2 + w2 and h2 < h1 + w1 and (h1, w1) != (h2, w2)
                   for h1, w1, c1 in dd[2] for h2, w2, c2 in sd[2]):
                ncopies += 1
                w(f"  {unit}:{ln} {func}: copy {s_} -> {d} ({n}): {fmt_ident(sd[0])} [{fmt(sd)}] -> "
                  f"{fmt_ident(dd[0])} [{fmt(dd)}]")
    w("")
    # -- 3b. initialized data passed by address to a function that takes bytes
    w("## 4b. ROM data passed by address as bytes (`u8 *`, `void *`) where the loader swapped words")
    w("")
    typed = {}
    for r in results.values():
        typed.update(r["params"] if r else {})
    seen = set()
    passed_reviewed = []
    for r in results.values():
        if not r or not r["accesses"]:
            continue
        unit = Access(*r["accesses"][0]).unit
        for fn, idents in r["calls"]:
            keys = typed.get(fn, ())
            if fn in COPY_FUNCS or fn in ("math_utility", "n64_bzero", "bzero", "n64_memset"):
                continue   # copies keep what they copy; the destination's readers are checked where they read
            for k, ident in enumerate(idents):
                if not ident or ident[0][0] != "sym" or k >= len(keys) or not keys[k] or keys[k][0] != "scalar" \
                        or keys[k][1] > 1:
                    continue
                e = loader.get(ident[0][1])
                if not e or not e[2] or ident[1] is None:
                    continue
                lw = loader_width(e, ident[1], ident[1])
                if lw and lw > 1 and (ident[0][1], ident[1], fn, k) not in seen:
                    seen.add((ident[0][1], ident[1], fn, k))
                    why = BYTES_REVIEWED.get((fn, k))
                    (passed_reviewed.append if why else w)(
                        f"  {ident[0][1]}+0x{ident[1]:X} ({lw}B elements) -> {fn} argument {k} "
                        f"({'u8 *' if keys[k][1] == 1 else 'void *'}), from {unit}" + (f": {why}" if why else ""))
    if passed_reviewed:
        w(f"  Reviewed ({len(passed_reviewed)}):")
        for line in passed_reviewed:
            w("  " + line)
    w("")
    # -- 4. puns in one file's code
    puns = [a for a in acc if a.pun]
    order = {"narrow": 0, "wide": 1, "mixed": 2, "union": 3, "pad": 4, "outside": 5}
    w("## 5. Puns: an access whose width differs from the declared scalar it falls on (interpreting ones listed)")
    w("")
    by_kind = collections.Counter(a.pun for a in puns)
    w("counts: " + ", ".join(f"{k} {v}" for k, v in sorted(by_kind.items(), key=lambda x: order.get(x[0], 9))))
    for a in sorted(puns, key=lambda a: (order.get(a.pun, 9), a.unit, a.line)):
        if a.pun in ("outside", "pad") or not interp(a):
            continue
        val = f" = {a.value:#x}" if a.value is not None else ""
        w(f"  {a.pun:6} {a.rw} {a.width}B {a.flow}{val} {site(a)}: {fmt_base(a)}; declared {a.detail}")
        w(f"         {a.text}")
    w("")
    errors = sum(r["errors"] for r in results.values() if r)
    funcs = sum(r["funcs"] for r in results.values() if r)
    summary = (f"{len(results)} units, {funcs} live functions, {len(acc)} accesses; {len(open_)} open value "
               f"conflicts in {len({c[1] for c in open_})} storages ({len(vf) - len(open_)} reviewed), "
               f"{len(scored)} mixed-width clusters, {len(halves)} low-half reads, "
               f"{sum(len(v) for v in rom.values())} ROM-width accesses in "
               f"{len(rom)} symbols, {ncopies} copies between views, {len(puns)} puns; {errors} statements not parsed")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("# endian_check report (tools/port/endian_check.py; docs/port/ENDIAN.md)\n\n" + summary + "\n\n"
                   + "\n".join(lines) + "\n")
    print(summary)
    print(f"report: {rel(out)}")


def fmt_ident(i):
    if i[0] == "sym":
        return i[1]
    if i[0] == "param":
        return f"*{i[1]}.arg{i[2]}"
    if i[0] == "pt":
        return f"*({fmt_ident(i[1])}+0x{i[2] or 0:X})"
    if i[0] == "retp":
        return f"*{i[1]}() in {i[2]}" if len(i) > 2 else f"*{i[1]}()"
    if i[0] == "local":
        return f"{i[1]}.{i[2]}"
    if i[0] == "anon":
        return f"{i[1]}.*{i[2]}"
    return f"{i[0]} {i[-1]}"


def fmt_family(f):
    return fmt_ident(f)


def fmt_base(a):
    s = fmt_ident(a.base)
    if a.pos is not None:
        s += f"+0x{a.pos:X}"
    return s


SELFTEST_C = r"""
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef short s16; typedef int s32;
typedef union Color { u8 c[4]; u32 w; } Color;
typedef struct Rec { u32 flags; Color col; s16 hi, lo; u8 b[4]; u32 packed; } Rec;
Rec gRec[8];
extern void sink(int);
/* a word constant written in one function, read as bytes by another: conflict */
void set_color(Rec *r) { r->col.w = 0xFF8000FF; }
void draw(Rec *r) { sink(r->col.c[0] << 24 | r->col.c[1]); }
void use1(void) { set_color(&gRec[1]); draw(&gRec[2]); }
/* bytes written, the word read and shifted: conflict */
void set_bytes(int i, u8 r, u8 g) { gRec[i].b[0] = r + 1; gRec[i].b[1] = g; }
u32 get_word(int i) { return *(u32 *)gRec[i].b >> 24; }
/* two s16 halves written, read as one s32: conflict */
void set_halves(int i) { gRec[i].hi = 3; gRec[i].lo = 4; }
s32 get_pair(int i) { return *(s32 *)&gRec[i].hi + 1; }
/* a local union: conflict */
int local_union(int v) { Color c; c.w = v * 3; return c.c[0] + 1; }
/* a word made in a caller's local, copied through a parameter into the records: conflict (with draw) */
void copy_color(Rec *dst, Color *src) { dst->col.w = src->w; }
void caller(void) { Color c; c.w = 0x11223344; copy_color(&gRec[3], &c); }
/* bytes copied as a word, read as bytes: no conflict */
void neutral(void) { Color c; c.c[0] = 1; c.c[1] = 2; c.c[2] = 3; c.c[3] = 4; copy_color(&gRec[4], &c); }
u32 neutral_flags(int i) { return gRec[i].flags & 4; }
/* a word moved by a struct assignment, and a union passed by value, into the records: conflicts (with draw) */
typedef struct Word { u32 w; } Word;
Word gWordSrc;
void make_word(void) { gWordSrc.w = 0x10203040; }
void struct_copy(void) { Word t = gWordSrc; copy_color(&gRec[5], (Color *)&t); }
void take_color(Color c, Rec *r) { r->col.w = c.w; }
void by_value(void) { Color c; c.w = 0x55667788; take_color(c, &gRec[6]); }
/* a handle kept as an s32 and read back as its second s16, the N64's low half (section 2b), returned and used as an
 * array index (a conflict) */
typedef union Handle { s32 whole; struct { s16 high, index; } part; } Handle;
Handle gHandles[4];
void keep_handle(int i) { gHandles[i].whole = i * 3 + 1; }
s16 handle_index(int i) { return gHandles[i].part.index; }
extern int gTable[];
int lookup(int i) { return gTable[gHandles[i].part.index]; }
"""
# (family, position made, width made, position used, width used, a function that made it) the self-test must
# find, and (family, position) it must not
SELFTEST_EXPECT = [(("sym", "gRec"), 0x4, 4, 0x4, 1, "set_color"), (("sym", "gRec"), 0x4, 4, 0x4, 1, "caller"),
                   (("sym", "gRec"), 0xC, 1, 0xC, 4, "set_bytes"), (("sym", "gRec"), 0x8, 2, 0x8, 4, "set_halves"),
                   (("local", "local_union", "c"), 0, 4, 0, 1, "local_union"),
                   (("sym", "gRec"), 0x4, 4, 0x4, 1, "make_word"), (("sym", "gRec"), 0x4, 4, 0x4, 1, "by_value"),
                   (("sym", "gHandles"), 0x0, 4, 0x2, 2, "keep_handle")]
SELFTEST_ABSENT = [(("local", "neutral", "c"), None), (("sym", "gRec"), 0x0)]
# (family, position, function) of the low-half reads (report section 2b) it must find, and no others
SELFTEST_HALVES = [(("sym", "gHandles"), 0x2, "handle_index"), (("sym", "gHandles"), 0x2, "lookup")]


# Initialized data whose declarations were made to agree with how the game reads it (docs/port/ENDIAN.md): the
# element width the PC loader must swap at (symbol, N64 offset), from build/pc/pc-data/data.c.
LOADER_EXPECT = [("D_80114264", 0x0, 4), ("D_80114264", 0xC, 4), ("D_801164A4", 0x0, 2), ("D_801164A4", 0x28, 2),
                 ("D_80117100", 0x0, 4), ("D_80117100", 0x10, 4), ("D_80116148", 0x0, 1), ("D_80116148", 0x5, 1),
                 ("D_803AF9A8", 0x0, 4), ("D_803AF9A8", 0x3C, 1), ("D_80111950", 0x0, 1), ("D_8011195C", 0x0, 1),
                 ("D_80111960", 0x0, 1), ("D_8011028C", 0x0, 1), ("D_8011EA40", 0x0, 2), ("D_8011EA40", 0xC, 2),
                 ("D_8011EA90", 0x6, 2), ("D_803B46A4", 0x0, 1), ("D_803B46A4", 0x3, 1),
                 ("D_80120E50", 0x0, 1), ("D_80120E50", 0x3, 1),
                 # the engine sound table (mode_select/d5e64.c): rev points u16, levels f32, in both 32-byte entries
                 ("D_8010FD80", 0x0, 4), ("D_8010FD80", 0x4, 2), ("D_8010FD80", 0xC, 2), ("D_8010FD80", 0x10, 4),
                 ("D_8010FD80", 0x18, 4), ("D_8010FD80", 0x24, 2), ("D_8010FD80", 0x30, 4)]
# The colour unions gen_data copies as bytes (a byte member covers them), D_801146BC's element 1 under
# D_801146C0's word view, unions without such a member (Vtx, Gfx), and the boot's flags after the viewport offset
# table, which its s16[32] declarations ran over (docs/port/DATA.md 1, 2).
LOADER_EXPECT += [(sym, off, 1) for sym in (
    "D_801146BC", "D_801146C0", "D_80116DB0", "D_80118E28", "D_80118E2C", "D_8011AD8C", "D_8011ADC0", "D_8011AE58",
    "D_8011B558", "D_8011B578", "D_80394270", "D_803942A0", "D_803942A4", "D_803942E8", "D_803B3054", "D_803B3058",
    "D_803B3458", "D_803B345C", "D_803B8328", "D_803B8354", "D_803B8394", "D_803B8398") for off in (0x0, 0x3)]
LOADER_EXPECT += [("D_801146BC", 0x4, 1), ("D_801146BC", 0x7, 1), ("D_8011B558", 0x10, 1), ("D_8011EF10", 0x0, 2),
                  ("D_8011EF10", 0xC, 1), ("D_8002E4A0", 0x0, 4), ("gViewportOffsetX", 0x0, 2),
                  ("gViewportOffsetX", 0x10, 2), ("gViewportOffsetY", 0x10, 2), ("gViewportPendingFrames", 0x0, 2),
                  ("gInitFlag", 0x0, 1), ("gInitFlag", 0x1, 1), ("gDmaInitialized", 0x0, 1)]


def check_loader(build):
    loader = loader_map(build)
    bad = 0
    for sym, off, want in LOADER_EXPECT:
        e = loader.get(sym)
        got = loader_width(e, off, off) if e else None
        bad += got != want
        print(f"{'ok  ' if got == want else 'BAD '} {sym}+0x{off:X}: loader element {got}B, want {want}B")
    print(f"check-loader: {len(LOADER_EXPECT)} places, {bad} wrong")
    return 1 if bad else 0


def selftest(build, readelf):
    """Run the detector on SELFTEST_C and check what it finds."""
    d = build / "endian" / "selftest"
    d.mkdir(parents=True, exist_ok=True)
    src = d / "selftest.c"
    src.write_text(SELFTEST_C)
    uid = "selftest"
    r = subprocess.run(["cc", "-std=gnu89", "-g", "-w", "-fno-strict-aliasing", *DUMP_FLAGS, "-c", str(src),
                        "-o", str(d / (uid + ".o"))], cwd=d, capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        return 1
    _, res = analyze_unit((uid, d, build, readelf))
    results = {uid: res}
    acc, fam, norm = analyze(results, {})
    found = [(f, p, wa, q, wb, {s_[1] for k, v, s_ in m}) for s, f, (p, wa), m, (q, wb), u, why in
             value_conflicts(results, fam, norm)]
    bad = 0
    for e in SELFTEST_EXPECT:
        ok = any(x[:5] == e[:5] and e[5] in x[5] for x in found)
        bad += not ok
        print(f"{'ok  ' if ok else 'MISS'} {fmt_ident(e[0])}: made +0x{e[1]:X}/{e[2]}B by {e[5]}, "
              f"used +0x{e[3]:X}/{e[4]}B")
    for e in SELFTEST_ABSENT:
        hit = [x for x in found if x[0] == e[0] and (e[1] is None or x[1] == e[1] or x[3] == e[1])]
        bad += bool(hit)
        print(f"{'ok  ' if not hit else 'FALSE'} no conflict in {fmt_ident(e[0])}" +
              (f" at +0x{e[1]:X}" if e[1] is not None else ""))
    halves = {(f, p, a.func) for f, p, a, whole in low_halves(acc, fam, norm)}
    for e in SELFTEST_HALVES:
        ok = e in halves
        bad += not ok
        print(f"{'ok  ' if ok else 'MISS'} low half of {fmt_ident(e[0])} +0x{e[1] - 2:X} read by {e[2]}")
    bad += len(halves - set(SELFTEST_HALVES))
    print(f"selftest: {len(found)} conflicts, {len(halves)} low-half reads, {bad} wrong")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("-j", "--jobs", type=int, default=2)
    ap.add_argument("--reuse", action="store_true", help="reuse the dumps in build/pc/endian that are newer than their source")
    ap.add_argument("--only", default="", help="comma-separated source paths to analyze (with --dump)")
    ap.add_argument("--dump", action="store_true", help="print every access of the --only files")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--readelf", default="readelf")
    ap.add_argument("--selftest", action="store_true", help="check the detector on synthetic cases")
    ap.add_argument("--check-loader", action="store_true",
                    help="check the loader's element widths where declarations were made to agree (LOADER_EXPECT)")
    a = ap.parse_args()
    build = a.build.resolve()
    cache = build / "endian"
    cache.mkdir(parents=True, exist_ok=True)
    if a.selftest or a.check_loader:
        return (selftest(build, a.readelf) if a.selftest else 0) | (check_loader(build) if a.check_loader else 0)
    cmds = game_commands(build)
    if a.only:
        want = {str((ROOT / p).resolve()) for p in a.only.split(",") if p}
        cmds = [c for c in cmds if str(Path(c[0]).resolve()) in want]
    jobs = [(src, cmd, cwd, cache, a.reuse) for src, cmd, cwd in cmds]
    with multiprocessing.Pool(a.jobs) as pool:
        for uid, err in pool.imap_unordered(compile_one, jobs, chunksize=4):
            if err:
                print(f"compile failed: {uid}: {err}", file=sys.stderr)
        results = dict(pool.imap_unordered(analyze_unit, [(unit_id(s), cache, build, a.readelf) for s, _, _ in cmds],
                                           chunksize=4))
    if a.dump:
        for uid, r in sorted(results.items()):
            if not r:
                continue
            for x in r["accesses"]:
                x = Access(*x)
                print(f"{x.unit}:{x.line} {x.func} {x.rw} {x.kind}{x.width} {fmt_base(x)} [{x.flow}] "
                      f"{'pun=' + x.pun + ' ' + str(x.detail) if x.pun else ''} | {x.text[:160]}")
            print(f"{uid}: {r['errors']} not parsed")
        return 0
    report(results, build, a.out or cache / "report.txt", loader_map(build))
    return 0


if __name__ == "__main__":
    sys.exit(main())
