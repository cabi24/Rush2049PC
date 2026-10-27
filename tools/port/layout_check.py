#!/usr/bin/env python3
"""Find views that disagree on a record's PC layout, following each access through the code to the storage it reaches.

    python3 tools/port/layout_check.py [--build build/pc] [-j 2] [--reuse] [--out build/pc/endian/layout.txt]
    python3 tools/port/layout_check.py --reuse --storage D_80143FC8      # every access that reaches a storage
    python3 tools/port/layout_check.py --selftest                         # ctest layout_check

Every shared record has one PC layout (docs/port/POINTERS.md 9.1). view_check.py and view_audit.py compare the views
declared on a named storage; this follows the code instead, so it also sees a record reached through a pointer whose
struct has another name: a list walk, a parameter, a slot a pointer was stored in, a function's result. docs/port/
LAYOUT.md.

How it works:
1. The accesses are endian_check.py's (GCC's GIMPLE of every live game file, the PC objects' DWARF; the same
   compile cache, build/pc/endian), with each access's view (its base's declared struct), field, host offset, and
   the N64 position the view gives it. A view has no opinion on the N64 position where it is bytes, void, a scalar
   read past its extent, or past the struct's end. Pointers are followed further than endian_check does: scaled
   indexes (`p + i * K`, keeping the offset in the element), pointers made from u32 loads (N64P), list walks, tree
   walks (`pp = &(*pp)->right`) and increments through PHIs, pointer and function-address stores into slots, calls
   through loaded pointers, and returns.
2. Each access's storage is put on a root: aliases and names inside a symbol (data_symbols.txt) are their container
   at its offset; what a parameter points to is each caller's argument (host and N64 offsets added), a function's
   result is what it returns from a symbol or from that call's arguments (else the call's own storage: a heap
   block), what a slot points to is what was stored there (LINKS: what a list or pool function's call links), a
   list's `next` and a tree node's links point to their own records, and a function stored into a slot takes the
   arguments of every call through that slot. Positions are host offsets (what the PC code really addresses).
3. The reference layout of a root is gen_data's chosen declaration for a symbol (its padding named from the symbol's
   other declarations of that size), SYMBOL_REFERENCE's, or REFERENCE's for a heap or pool record reached through a
   view named there. Element positions are taken in the reference's element.
4. Findings: (1) views off the reference (one that still hits fields of its widths only matters if the N64 build
   has it too), or, without one, an N64 position at another host offset than most views give it or a host offset
   at another N64 position; (1b) an access with no view opinion, or a pointer argument,
   inside a field of the reference (a raw N64 offset into a record whose PC layout moved it); (2) strides
   (`base + i * K`, `p += K`, a typed index) that are not the element's PC size, `* 952` with the N64 size above
   all; (3) an 8-byte pointer read or written where the reference has a u32 (a window address), or a u32 over a
   pointer.
Scores are heuristics; every finding needs reading. Those read and judged are in REVIEWED. Stdlib and binutils,
Python 3.9+.
"""
import argparse
import bisect
import collections
import multiprocessing
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import endian_check as ec  # noqa: E402
import gen_data as gd  # noqa: E402

ROOT = ec.ROOT
Access = ec.Access
_INT = ec._INT
_SSA = ec._SSA
_ASSIGN = re.compile(r"^([\w.$]+(?:\((?:D|ab)\))?) (?:=|=\{v\}) (.*);$")
_PHI = re.compile(r"^# ([\w.$]+(?:\((?:D|ab)\))?) = PHI <(.*)>$")
_CAST = re.compile(r"^\(([^()]*(?:\([^()]*\)[^()]*)*)\) (\S+)$")
U64 = 1 << 64


def s64(v):
    return v - U64 if v >= 1 << 63 else v


# ---------------------------------------------------------------- 1. one unit

def view_name(T, t):
    """A struct view's name for reports: its tag or typedef, `T[]` for an array of them, or the scalar's spelling."""
    if t is None:
        return "?"
    if t[0] == "ptr":
        return "ptr"
    if t[0] == "arr":
        return view_name(T, t[1]) + "[]"
    c = t[1]
    k = ec.type_key_canon(T, t)
    if k:
        return k[0] + ("[]" if c[0] == "array" else "")
    while c[0] == "array":
        c = c[1]
    if c[0] == "base":
        return c[1]
    return c[0] if c[0] not in ("struct", "union") else f"{c[0]} {c[1] or '{...}'}"


def is_bytes(c):
    while c is not None and c[0] == "array":
        c = c[1]
    return c is None or c[0] in ("void", "func", "opaque") or (c[0] == "base" and c[2] == 1)


class LAnalyzer(ec.Analyzer):
    """endian_check's Analyzer, keeping what a layout check needs: each access's view and field, the strides,
    the pointers stored into slots, list links, call sites with both offsets, and what each function returns."""

    def __init__(self, unit, types, weak):
        super().__init__(unit, types, weak)
        self.extra = {}        # access index -> (view, path, the N64 position the view gives it or None, None)
        self.strides = []      # (func, line, ident, base host offset, K, how, view element (host, N64), view)
        self.selflinks = []    # (ident, slot host offset): the pointer there points to a record of the same storage
        self.treelinks = []    # the same, for a record stored through a walk down a tree (pp = &(*pp)->right)
        self.pstores = []      # (store access index, value ident, value host offset, value N64 offset)
        self.sites = []        # (func, line, callee, [(ident, host offset, N64 offset) | None])
        self.rets = collections.defaultdict(list)   # func -> [(ident, host offset, N64 offset)]
        self.fstores = []      # (store access index, function): a function's address stored into a slot
        self.icalls = []       # (index of the load of the called pointer, [(ident, host offset, N64 offset) | None])

    # -- pointers
    def pointer_sources(self, f):
        """endian_check's pointer sources (Analyzer.pointer_sources), and: `p + i * K` (the offset in the element is
        kept and the stride recorded), `(T *) x` of an integer loaded from memory (a u32 window address read as a
        pointer), PHIs whose other arguments come from the PHI itself (a list walk `n = n->next`, an increment
        `p += K`), whatever their names."""
        defs, self.scale, self.from_load, self.from_ptr = {}, {}, {}, {}
        for ln, st in f.stmts:
            m = _ASSIGN.match(st)
            if m and self.is_ssa(f, m.group(1)):
                defs[m.group(1)] = (m.group(2).strip(), ln)
        self.defs_text = defs
        for _ in range(3):   # integers: scaled indexes, and conversions of loads and of pointers
            for x, (rhs, ln) in defs.items():
                lt = self.ssa_type(f, x)
                if lt and lt[0] == "ptr":
                    continue
                mm = ec._BINOP.match(rhs)
                c = _CAST.match(rhs)
                if mm and mm.group(2) in ("*", "<<") and _INT.match(mm.group(3)) and self.is_ssa(f, mm.group(1)):
                    k = int(mm.group(3))
                    k = s64(k % U64) if mm.group(2) == "*" else (1 << k if k < 32 else 0)
                    inner = self.scale.get(mm.group(1))
                    self.scale[x] = (inner[0] * k, inner[1] * k) if inner else (k, 0)
                elif mm and mm.group(2) == "+" and _INT.match(mm.group(3)) and mm.group(1) in self.scale:
                    k, c0 = self.scale[mm.group(1)]
                    self.scale[x] = (k, c0 + s64(int(mm.group(3)) % U64))
                elif mm and mm.group(2) == "+" and _INT.match(mm.group(3)) and mm.group(1) in self.from_load:
                    y, c0 = self.from_load[mm.group(1)]   # a pointer held as an integer, plus an offset
                    self.from_load[x] = (y, c0 + s64(int(mm.group(3)) % U64))
                elif c and self.is_ssa(f, c.group(2)):
                    y = c.group(2)
                    for d in (self.scale, self.from_load, self.from_ptr):
                        if y in d:
                            d[x] = d[y]
                    yt = self.ssa_type(f, y)
                    if yt and yt[0] == "ptr":
                        self.from_ptr[x] = y
                elif self.is_mem_expr(f, rhs):
                    self.from_load[x] = (x, 0)
        src = {}
        pnames = list(f.params)
        for n in set(ec._NAMES.findall(" ".join(st for ln, st in f.stmts))):
            m = _SSA.match(n)
            if n.endswith("(D)") and m and m.group(1) in pnames:
                t = self.T.from_str(f.params[m.group(1)])
                if t and t[0] == "ptr":
                    src[n] = (("param", f.name, pnames.index(m.group(1))), None, 0, None)
        steps = {}   # (line, pointer, how) -> (base pointer or None, K)
        for _ in range(6):
            before = dict(src)
            for ln, st in f.stmts:
                m = _ASSIGN.match(st) or _PHI.match(st)
                if not m:
                    continue
                lhs, rhs = m.group(1), m.group(2).strip()
                if not self.is_ssa(f, lhs):
                    continue
                if st.startswith("# "):
                    self.phi_source(f, ln, lhs, rhs, src, steps)
                    continue
                lt = self.ssa_type(f, lhs)
                if lt and lt[0] == "ptr":
                    mm = re.match(r"^([\w.$]+) \(", rhs)
                    if mm and not rhs.startswith("("):
                        src[lhs] = (("retp", mm.group(1), f"{f.name}:{ln}"), None, 0, None)
                        continue
                    if self.is_mem_expr(f, rhs):
                        src[lhs] = (("ldssa", lhs), None, 0, None)
                        continue
                    c = _CAST.match(rhs)
                    if c and c.group(2) in self.from_load:
                        y, c0 = self.from_load[c.group(2)]
                        src[lhs] = (("ldssa", y), None, c0, None)
                        continue
                rhs = re.sub(r"^\([^()]*(?:\([^()]*\)[^()]*)*\) ", "", rhs)   # casts
                mm = re.match(r"^([\w.$]+(?:\((?:D|ab)\))?|&\S+) \+ (\S+)$", rhs)
                if rhs.startswith("&") and not mm:
                    r = self._addr_ident(f, rhs[1:], src)
                    if r:
                        src[lhs] = r
                    continue
                if mm:
                    a, b = mm.group(1), mm.group(2)
                    base = self._addr_ident(f, a[1:], src) if a.startswith("&") else src.get(a)
                    if base:
                        k = ec.parse_int(b)
                        if k is None and b in self.scale:
                            k2, c0 = self.scale[b]
                            src[lhs] = (base[0], base[1], None if base[2] is None else base[2] + c0, base[3])
                            steps[(ln, lhs, "index")] = (base, k2)
                        else:
                            src[lhs] = (base[0], base[1], None if k is None or base[2] is None else base[2] + s64(k % U64),
                                        base[3])
                    continue
                if rhs in src:
                    src[lhs] = src[rhs]
            if src == before:
                break
        pt_name = {}
        for (ln, lhs, how), (base, k) in sorted(steps.items(), key=str):
            if base is None:
                base = src.get(lhs)
            if base is None:
                continue
            if how == "link":
                if base[2] is not None:
                    self.selflinks.append((base[0], base[2] + k))
                continue
            pt = self.ssa_type(f, lhs)
            if lhs not in pt_name:
                pt_name[lhs] = view_name(self.T, pt[1]) if pt and pt[0] == "ptr" else "?"
            self.strides.append((f.name, ln, base[0], base[2], k, how, None, pt_name[lhs]))
        return src

    def phi_source(self, f, ln, lhs, rhs, src, steps):
        """A PHI's storage: the one its arguments share, leaving out 0, arguments named after it (a loop's own
        value, as endian_check does) and the PHI's own steps (phi + K, a link loaded from it), which are recorded."""
        args = [re.sub(r"\(\d+\)$", "", a).strip() for a in ec.split_args(rhs)]
        found, offs, vars_ = set(), set(), set()
        for a in args:
            how = self.self_ref(f, lhs, a)
            if how:
                steps[(ln, lhs, how[0])] = (None, how[1])
                continue
            r = self._addr_ident(f, a[1:], src) if a.startswith("&") else src.get(a)
            if r and r[0][0] == "ldssa" and r[2] is not None and self.loaded_from(f, r[0][1]) == lhs:
                self.treewalk[lhs].add(r[2])   # a walk down a list or tree: pp = &(*pp)->right
                continue
            if r:
                found.add(r[0])
                offs.add(r[2])
                vars_.add(r[3])
            elif self.ssa_root(a) == self.ssa_root(lhs) or a == "0B":
                continue
            else:
                found.add(None)
        syms = {i for i in found if i and i[0] == "sym"}
        if len(syms) == 1:
            found = syms
        if len(found) == 1 and None not in found:
            ident = found.pop()
            bt = next((src[a][1] for a in args if a in src and src[a][0] == ident), None)
            src[lhs] = (ident, bt, offs.pop() if len(offs) == 1 else None, vars_.pop() if len(vars_) == 1 else None)

    def loaded_from(self, f, x):
        """The SSA pointer whose record the SSA value x was loaded from, or None."""
        d = self.defs_text.get(x)
        return self.mem_base(f, d[0]) if d is not None and self.is_mem_expr(f, d[0]) else None

    def self_ref(self, f, phi, a):
        """('incr', K) if a is phi + K, ('link', slot host offset) if a is a pointer loaded from phi's record,
        else None."""
        x, seen = a, 0
        while seen < 4:
            d = self.defs_text.get(x)
            if d is None:
                return None
            rhs = d[0]
            c = _CAST.match(rhs)
            if c and self.is_ssa(f, c.group(2)):
                x, seen = c.group(2), seen + 1
                continue
            mm = re.match(r"^([\w.$]+(?:\((?:D|ab)\))?) \+ (-?\d+)$", rhs)
            if mm and mm.group(1) == phi:
                return ("incr", s64(int(mm.group(2)) % U64))
            if self.is_mem_expr(f, rhs):
                try:
                    prim, chain = ec.parse_ref(rhs)
                except (ValueError, IndexError):
                    return None
                base = prim[1] if prim[0] in ("deref", "arrow") else \
                    prim[3][1] if prim[0] == "mem" and prim[3][0] == "ssa" else None
                if base != phi:
                    return None
                pt = self.ssa_type(f, phi)
                r = self.resolve(f, (prim, chain), {phi: (("phi", phi), pt[1] if pt and pt[0] == "ptr" else None, 0,
                                                         None)})
                return ("link", r["off"]) if r and r["off"] is not None else None
            return None
        return None

    # -- accesses
    def ref_access(self, f, ln, s, rw, ptrsrc, st, value=None, note=None):
        i = super().ref_access(f, ln, s, rw, ptrsrc, st, value, note)
        if i is None:
            return None
        try:
            r = self.resolve(f, ec.parse_ref(s), ptrsrc)
        except (ValueError, IndexError):
            r = None
        if r is None:
            return i
        a = self.accesses[i]
        bt, off = r["bt"], r["off"]
        btc = ec.as_canon(bt)
        cast, coff = r["cast"] if r["cast"] else (None, None)
        cc = ec.as_canon(cast)
        vn = view_name(self.T, bt)
        if cc is not None and elem(cc)[0] in ("struct", "union") and coff is not None and off is not None:
            # through a pointer cast to a struct: that struct is the code's view, from where the pointer points
            nb = 0 if coff == 0 else self.opinion(btc, coff, 1)
            ni = self.opinion(cc, off - coff, a.width)
            opinion = None if nb is None or ni is None else nb + ni
            vn = view_name(self.T, cast)
        else:
            opinion = self.opinion(btc, off, a.width)
        self.extra[i] = (vn, r["path"], opinion, None)
        if r["var"] is not None and r["var"][1] and not is_bytes(btc):
            aoff, es, ens = r["var"]
            self.strides.append((f.name, ln, a.base, aoff, es, "typed", (es, ens), vn))
        return i

    @staticmethod
    def opinion(c, off, width):
        """The N64 position a view gives host offset off, or None where it has no opinion: bytes, void, or outside
        the view (a T[] declaration covers any element)."""
        if c is None or off is None or is_bytes(c) or off < 0:
            return None
        hs = gd.host_size(c)
        if not (c[0] == "array" and c[2] is None) and (hs is None or off + width > hs):
            return None
        return ec.host_to_n64(c, off)

    def pdesc(self, f, a, ptrsrc):
        """(storage, host offset, N64 offset or None) of a pointer value: the N64 offset only where its view has an
        opinion (not bytes or void), or at offset 0."""
        try:
            if a.startswith("&"):
                r = self.resolve(f, ec.parse_ref(a[1:]), ptrsrc)
                if r is None or r["off"] is None:
                    return None
                ident, bt, off = r["ident"], r["bt"], r["off"]
            elif self.is_ssa(f, a) and a in ptrsrc:
                pt = self.ssa_type(f, a)
                ident, bt, off, _ = self.ptr_ident(f, a, pt[1] if pt and pt[0] == "ptr" else None, ptrsrc)
                if off is None:
                    return None
            else:
                return None
        except (ValueError, IndexError):
            return None
        c = ec.as_canon(bt)
        n = 0 if off == 0 else None
        if c is not None and not is_bytes(c) and c[0] != "ptr":
            n = ec.host_to_n64(c, off)
        return ident, off, n

    def statement(self, f, ln, st, ptrsrc):
        n0 = len(self.accesses)
        super().statement(f, ln, st, ptrsrc)
        if st.startswith("return ") and st.endswith(";"):
            d = self.pdesc(f, st[7:-1].strip(), ptrsrc)
            if d and d[0][0] != "anon":
                self.rets[f.name].append(d)
            return
        m = re.match(r"^(.*?) (?:=|=\{v\}) (.*);$", st)
        if not m or len(self.accesses) == n0 or self.accesses[n0].rw != "W":
            return
        rhs = m.group(2).strip()
        fn = self.function_named(f, rhs)
        if fn:
            self.fstores.append((n0, fn))
            return
        v = self.from_ptr.get(rhs, rhs) if self.is_ssa(f, rhs) else rhs
        d = self.pdesc(f, v, ptrsrc)
        if d and d[0][0] != "anon":
            self.pstores.append((n0,) + d)
            walk = self.treewalk.get(self.mem_base(f, m.group(1).strip()))
            for h in sorted(walk or ()):   # stored where the walk ended: a node of the tree, linked at those slots
                if d[1] is not None:
                    self.treelinks.append((d[0], d[1] + h))

    def mem_base(self, f, text):
        """The SSA pointer a memory reference goes through (*p, p->f, MEM[p + k]), or None."""
        try:
            prim, chain = ec.parse_ref(text)
        except (ValueError, IndexError):
            return None
        return prim[1] if prim[0] in ("deref", "arrow") else \
            prim[3][1] if prim[0] == "mem" and prim[3][0] == "ssa" else None

    def function_named(self, f, v, depth=0):
        """The function whose address the value v is (`slot = func;`, or an SSA name set to it), or None."""
        if depth > 3:
            return None
        if self.is_ssa(f, v):
            d = self.defs_text.get(v)
            if d is None:
                return None
            c = _CAST.match(d[0])
            return self.function_named(f, c.group(2) if c else d[0], depth + 1)
        if re.match(r"^[A-Za-z_][\w.$]*$", v) and v not in f.decls and v not in f.params and v not in self.T.gvars:
            return v
        return None

    def called_load(self, f, fn, depth=0):
        """The access that loaded an indirect call's function pointer (through casts), or None."""
        if fn in self.load_of:
            return self.load_of[fn]
        d = self.defs_text.get(fn)
        c = _CAST.match(d[0]) if d else None
        return self.called_load(f, c.group(2), depth + 1) if c and depth < 3 else None

    def call(self, f, ln, s, lhs, ptrsrc, st):
        super().call(f, ln, s, lhs, ptrsrc, st)
        m = re.match(r"^([\w.$]+) \((.*)\)$", s)
        if not m:
            return
        out = [self.pdesc(f, a, ptrsrc) for a in ec.split_args(m.group(2))]
        if self.is_ssa(f, m.group(1)):   # through a pointer: the slot it was loaded from says which functions
            i = self.called_load(f, m.group(1))
            if i is not None and any(out):
                self.icalls.append((i, out))
        elif any(out):
            self.sites.append((f.name, ln, m.group(1), out))

    def run(self, funcs):
        for f in funcs:
            if f.name in self.weak:
                continue
            marks = (len(self.accesses), len(self.calls), len(self.strides), len(self.selflinks),
                     len(self.pstores), len(self.sites), len(self.icalls), len(self.treelinks))
            self.treewalk = collections.defaultdict(set)   # PHI -> slots it steps through: pp = &(*pp)->right
            ptrsrc = self.pointer_sources(f)
            self.du = ec.DefUse(self, f)
            self.pending, self.store_w, self.load_of, self.line_of = [], {}, {}, {}
            n_rets = len(self.rets.get(f.name, ()))
            for ln, st in f.stmts:
                try:
                    self.statement(f, ln, st, ptrsrc)
                except (ValueError, IndexError, KeyError, TypeError):
                    self.errors += 1

            def fix(ident):
                if ident and ident[0] == "ldssa":
                    i = self.load_of.get(ident[1])
                    return ("ld", i) if i is not None else ("anon", f.name, self.ssa_root(ident[1]))
                if ident and ident[0] == "phi":
                    return ("anon", f.name, self.ssa_root(ident[1]))
                return ident
            for i in range(marks[0], len(self.accesses)):
                self.accesses[i] = self.accesses[i]._replace(base=fix(self.accesses[i].base))
            for i in range(marks[1], len(self.calls)):
                fn, idents = self.calls[i]
                self.calls[i] = (fn, [(fix(x[0]), x[1]) if x else None for x in idents])
            for i in range(marks[2], len(self.strides)):
                s = self.strides[i]
                self.strides[i] = s[:2] + (fix(s[2]),) + s[3:]
            for i in range(marks[3], len(self.selflinks)):
                self.selflinks[i] = (fix(self.selflinks[i][0]), self.selflinks[i][1])
            for i in range(marks[7], len(self.treelinks)):
                self.treelinks[i] = (fix(self.treelinks[i][0]), self.treelinks[i][1])
            for i in range(marks[4], len(self.pstores)):
                p = self.pstores[i]
                self.pstores[i] = (p[0], fix(p[1]), p[2], p[3])
            for i in range(marks[5], len(self.sites)):
                s = self.sites[i]
                self.sites[i] = s[:3] + ([(fix(x[0]),) + x[1:] if x else None for x in s[3]],)
            for i in range(marks[6], len(self.icalls)):
                c = self.icalls[i]
                self.icalls[i] = (c[0], [(fix(x[0]),) + x[1:] if x else None for x in c[1]])
            rl = self.rets.get(f.name)
            if rl:
                for i in range(n_rets, len(rl)):
                    rl[i] = (fix(rl[i][0]),) + rl[i][1:]
            for i, (rw, x, w) in self.pending:
                self.accesses[i] = self.accesses[i]._replace(flow=self.flow(rw, x, w))


def analyze_unit(job):
    """One unit's accesses and links (LAnalyzer), and the declarations of its data symbols."""
    uid, cache, build, readelf = job[:4]
    source = job[4] if len(job) > 4 else None   # the unit's source, when it isn't ROOT / unit (the selftest)
    dumps = sorted(cache.glob(uid + ".c.*t.forwprop1"))
    obj = cache / (uid + ".o")
    if not dumps or not obj.exists():
        return uid, None
    out = subprocess.run([readelf, "--debug-dump=info", str(obj)], capture_output=True, text=True).stdout
    dies = gd.parse_readelf(out, str(obj))[str(obj)]
    T = ec.Types(dies)
    an = LAnalyzer(uid.replace("__", "/") + ".c", T, ec.weak_names(build, uid))
    funcs = ec.read_gimple(dumps[0].read_text(errors="replace"))
    an.run(funcs)
    used = {a.base[1] for a in an.accesses if a.base[0] == "sym"}
    used |= {x[0][1] for s in an.sites for x in s[3] if x and x[0][0] == "sym"}
    decls = {}
    for name, (toff, ext) in T.gvars.items():
        if toff is None or name not in used:
            continue
        c = gd.canon(dies, toff, T.memo)
        e = c
        while e[0] == "array":
            e = e[1]
        if e[0] in ("struct", "union") or c[0] == "array":
            decls[name] = (gd.display(dies, toff), c)
    return uid, dict(unit=an.unit, accesses=[tuple(a) for a in an.accesses], extra=an.extra, strides=an.strides,
                     selflinks=an.selflinks, treelinks=an.treelinks, pstores=an.pstores, sites=an.sites,
                     rets=dict(an.rets), decls=decls, fstores=an.fstores, icalls=an.icalls,
                     view_types=view_types(an, T), pc_views=pc_views(dies, source or ROOT / an.unit),
                     errors=an.errors,
                     defined=[f.name for f in funcs if f.name not in an.weak],
                     funcs=sum(1 for f in funcs if f.name not in an.weak))


_DEFINED_PC = re.compile(r"defined\s*\(?\s*TARGET_PC\s*\)?")


def pc_lines(path):
    """The lines of a source file that only the PC build compiles (`#ifdef TARGET_PC` and the like, or the
    `#else` of `#ifndef TARGET_PC`), as a set of line numbers."""
    try:
        text = path.read_text(errors="replace").splitlines()
    except OSError:
        return set()
    out, stack = set(), []   # per open conditional: 'pc', 'n64' or None, for the branch we are in
    for i, line in enumerate(text, 1):
        m = re.match(r"^\s*#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)$", line)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("ifdef", "ifndef", "if"):
                if d == "if":
                    pc, n64 = bool(_DEFINED_PC.fullmatch(rest)), bool(re.fullmatch(r"!\s*" + _DEFINED_PC.pattern, rest))
                else:
                    pc, n64 = rest == "TARGET_PC" and d == "ifdef", rest == "TARGET_PC" and d == "ifndef"
                stack.append("pc" if pc else "n64" if n64 else None)
            elif d == "else" and stack:   # (after an #elif of `#ifdef TARGET_PC`, every branch is the N64's)
                stack[-1] = {"pc": "n64", "n64": "pc", "n64 only": "n64 only"}.get(stack[-1])
            elif d == "elif" and stack:
                stack[-1] = "n64 only" if stack[-1] in ("pc", "n64 only") else None
            elif d == "endif" and stack:
                stack.pop()
            continue
        if "pc" in stack:
            out.add(i)
    return out


def pc_views(dies, path):
    """The names of the struct, union and typedef views the unit's own source declares for the PC build only."""
    lines = None
    out = set()
    for d in dies.values():
        if d.tag not in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_typedef") or "DW_AT_name" not in d.attrs:
            continue
        if gd._num(d.attrs.get("DW_AT_decl_file")) != 1:
            continue
        if lines is None:
            lines = pc_lines(path)
        if gd._num(d.attrs.get("DW_AT_decl_line")) in lines:
            out.add(gd._name(d.attrs["DW_AT_name"]))
    return sorted(out)


def view_types(an, T):
    """view name -> canon type, for the views REFERENCE names."""
    out = {}
    for entry in REFERENCE + list(SYMBOL_REFERENCE.values()):
        unit_re, vname = entry[3:5] if len(entry) > 3 else entry[:2]
        if not re.search(unit_re, an.unit):
            continue
        t = T.from_str("struct " + vname) or T.from_str(vname)
        if t and t[0] == "c":
            out[vname] = t[1]
    return out


# ---------------------------------------------------------------- 2. storages and their roots

class Symbols:
    """data_symbols.txt: names at one address (the first is the storage), names inside another symbol's storage
    (container, host offset, N64 offset), and the type gen_data chose."""

    def __init__(self, build):
        self.alias, self.inner, self.chosen, self.addr, self.falias = {}, {}, {}, {}, {}
        ld = build / "pc-data" / "aliases.ld"   # PROVIDE(func_8009539C = draw_sprites): names of one function
        if ld.exists():
            for m in re.finditer(r"PROVIDE\((\w+) = (\w+)\);", ld.read_text()):
                self.falias[m.group(1)] = m.group(2)
        path = build / "pc-data" / "data_symbols.txt"
        if not path.exists():
            return
        inside = []
        for line in path.read_text().splitlines():
            p = line.split("\t")
            if len(p) < 7 or not p[0].startswith("0x"):
                continue
            addr, names = int(p[0], 16), p[6].split()
            for n in names:
                self.alias[n] = names[0]
            self.addr[names[0]] = addr
            self.chosen[names[0]] = p[5]
            m = re.match(r"in (\w+)\+0x([0-9A-Fa-f]+)", p[1])
            if m:
                inside.append((names[0], m.group(1), int(m.group(2), 16), addr))
        for n, cont, hoff, addr in inside:
            c = self.alias.get(cont, cont)
            if c in self.addr:
                self.inner[n] = (c, hoff, addr - self.addr[c])


def elem(c):
    while c is not None and c[0] == "array":
        c = c[1]
    return c


class Ref:
    """A reference layout: the element (struct) of a declaration, its scalars by host offset, and its arrays."""

    def __init__(self, c, origin):
        self.c, self.origin = c, origin   # origin: (unit, view name)
        self.from_view = False            # given by REFERENCE, not by the symbol's declaration
        self.e = elem(c)
        self.eh, self.en = gd.host_size(self.e) or 0, gd.n64_layout(self.e)[0] or 0
        self.array = c[0] == "array"
        self.runs = gd.program(self.e)
        self.by_h, self.all_h = {}, collections.defaultdict(list)   # the first scalar at a host offset; all
        for x in ec.scalars(self.e, 0, self.eh, cap=1 << 16):
            self.by_h.setdefault(x[0], x)
            self.all_h[x[0]].append(x)   # a union's members
        self.starts = sorted(self.by_h)
        self.arrays = []   # (host start, host end, element host size, element N64 size)
        self._arrays(self.e, 0)

    def merge(self, others):
        """Name the reference's unnamed bytes from other declarations of the same storage and size: a field of
        theirs that covers only padding here (gen_data's choice may be a padded view)."""
        def blank(h):
            x = self.covering(h)
            return x is None or (x[6] and PAD_NAME.search(x[5] or ""))
        for c in others:
            e = elem(c)
            if e is None or e is self.e or gd.host_size(e) != self.eh or gd.n64_layout(e)[0] != self.en:
                continue
            for x in ec.scalars(e, 0, self.eh, cap=1 << 16):
                if x[6] or PAD_NAME.search(x[5] or "") or not all(blank(h) for h in range(x[0], x[0] + x[2])):
                    continue
                for h in range(x[0], x[0] + x[2]):
                    self.by_h.pop(h, None)
                    self.all_h.pop(h, None)
                self.by_h[x[0]] = x
                self.all_h[x[0]] = [x]
                self.starts = sorted(self.by_h)

    def _arrays(self, c, h):
        if c[0] in ("struct", "union"):
            for name, hoff, mt in c[3]:
                self._arrays(mt, h + (hoff or 0) if c[0] == "struct" else h)
        elif c[0] == "array" and c[2]:
            e = c[1]
            eh, en = gd.host_size(e) or 0, gd.n64_layout(e)[0] or 0
            if eh > 1:
                self.arrays.append((h, h + eh * c[2], eh, en))
                self._arrays(e, h)

    def n2h(self, n):
        return gd.map_offset(self.runs, n) if self.runs else n

    def at(self, h):
        """The scalar whose first byte is host offset h: (host, N64, host width, N64 width, class, path, ...)."""
        return self.by_h.get(h)

    def covering(self, h):
        """The scalar host offset h falls in, or None (padding)."""
        i = bisect.bisect_right(self.starts, h) - 1
        if i < 0:
            return None
        x = self.by_h[self.starts[i]]
        return x if x[0] <= h < x[0] + x[2] else None

    def strides(self, b):
        """(host, N64) strides an index at host offset b may take: the element's, and every array's around b."""
        out = {(self.eh, self.en)}
        if b is not None:
            out |= {(eh, en) for lo, hi, eh, en in self.arrays if lo <= b < hi}
        return out

    def name(self):
        return f"{self.origin[0]} {self.origin[1]}"


Entry = collections.namedtuple("Entry", "h n width kind rw flow unit func line view path value text via")


def fmt_func(f):
    return f if isinstance(f, str) else f"{f[0]}@{Path(f[1]).name if f[1] else '?'}"


def fmt_root(r):
    k = r[0]
    if k == "sym":
        return r[1]
    if k == "pt":
        return f"*({fmt_root(r[1])}+0x{r[2]:X})"
    if k == "param":
        return f"*{fmt_func(r[1])}.arg{r[2]}"
    if k == "retp":
        return f"*{fmt_func(r[1])}() at {r[2]}"
    if k == "local":
        return f"{r[1]}.{r[2]}"
    if k == "anon":
        return f"{r[1]}.*{r[2]}"
    if k == "ptx":
        return f"*(load {r[2]} in {r[1]})"
    return " ".join(str(x) for x in r)


class Flow:
    """Every access put on the root storages it reaches (section 2 of the docstring)."""
    CAP = 24   # a storage that expands to more roots than this is a generic one (a helper's parameter): kept as it is

    def __init__(self, results, syms):
        self.syms = syms
        self.units = {r["unit"]: r for _, r in sorted(results.items()) if r}
        self.accs = {u: [Access(*a) for a in r["accesses"]] for u, r in self.units.items()}
        defined = collections.defaultdict(set)
        for u, r in self.units.items():
            for fn in r["defined"]:
                defined[fn].add(u)
        # a function defined in several units (static helpers) is named with its unit where a call or an access
        # is in one of those units
        self.multi = {fn: us for fn, us in defined.items() if len(us) > 1}
        self.callers = collections.defaultdict(list)   # (function, k) -> [(unit, ident, h, n)]
        self.sites = {}                                # (function, "caller:line", unit) -> args
        self.rets = collections.defaultdict(list)      # function -> [(unit, ident, h, n)]
        for u, r in self.units.items():
            for fn, ln, callee, args in r["sites"]:
                fk = self.fkey(callee, u)
                self.sites[(fk, f"{fn}:{ln}", u)] = args
                for k, x in enumerate(args):
                    if x:
                        self.callers[(fk, k)].append((u,) + tuple(x))
            for fn, lst in r["rets"].items():
                for x in lst:
                    self.rets[self.fkey(fn, u)].append((u,) + tuple(x))
        self.refs = self.symbol_refs()
        self.memo, self.links, self.stores, self.generic = {}, set(), {}, set()
        self.add_links("selflinks")
        for npass in range(3):   # what a slot points to is what was stored in it (slots reached through slots)
            self.memo = {}
            if npass == 2:
                self.indirect_callers()
                self.memo = {}
            self.add_links("treelinks")
            stores = collections.defaultdict(dict)
            for u, r in self.units.items():
                for fn, ln, callee, args in r["sites"]:
                    spec = LINKS.get(callee)
                    if not spec or max(spec[0], spec[2]) >= len(args) or not args[spec[0]] or not args[spec[2]]:
                        continue
                    (lid, lh, _), offs, (vid, vh, vn) = args[spec[0]], spec[1], args[spec[2]]
                    b, dh, dn = self.key(lid, u)
                    for root, dh2, dn2 in self.expand(b):
                        for o in offs:
                            stores[("pt", root, self.norm_h(root, lh + dh + dh2 + o))][(u, vid, vh, vn)] = None
                accs = self.accs[u]
                for i, ident, h, n in r["pstores"]:
                    a = accs[i]
                    if a.hpos is None:
                        continue
                    b, dh, dn = self.key(a.base, u)
                    roots = self.expand(b)
                    if len({x[0] for x in roots}) != 1 or roots[0][0] in self.generic:
                        continue   # a list or pool function's own store: its callers' calls say what (LINKS)
                    root, dh2, dn2 = roots[0]
                    stores[("pt", root, self.norm_h(root, a.hpos + dh + dh2))][(u, ident, h, n)] = None
            self.stores = stores
        self.memo = {}
        self.add_links("treelinks")
        self.buckets = self.place()
        self.view_refs()

    def add_links(self, which):
        """The slots whose pointer points to another record of the same storage: a list's next, from a walk
        along it (before any store is followed, which a list function's own list would take for every list), or a
        tree's left and right, on the records stored where a walk down the tree ended (with the stores so far: the
        tree's insert takes its node as a parameter). The memo starts again when there are new ones."""
        n = len(self.links)
        for u, r in self.units.items():
            for ident, h in r[which]:
                b, dh, dn = self.key(ident, u)
                for root, dh2, dn2 in self.expand(b):
                    self.links.add((root, self.norm_h(root, h + dh + dh2)))
        if len(self.links) != n:
            self.memo = {}

    def indirect_callers(self):
        """A function stored into a slot is called by every indirect call through that slot: its parameters take
        those calls' arguments (the sorted-tag tree's order, a visual's animator)."""
        fslots = collections.defaultdict(set)
        for u, r in self.units.items():
            accs = self.accs[u]
            for i, fn in r.get("fstores", ()):
                a = accs[i]
                if a.hpos is None:
                    continue
                b, dh, dn = self.key(a.base, u)
                for root, dh2, dn2 in self.expand(b):
                    fslots[(root, self.norm_h(root, a.hpos + dh + dh2))].add(self.fkey(self.syms.falias.get(fn, fn), u))
        for u, r in self.units.items():
            accs = self.accs[u]
            for i, args in r.get("icalls", ()):
                a = accs[i]
                if a.hpos is None:
                    continue
                b, dh, dn = self.key(a.base, u)
                for root, dh2, dn2 in self.expand(b):
                    for fk in fslots.get((root, self.norm_h(root, a.hpos + dh + dh2)), ()):
                        for k, x in enumerate(args):
                            if x:
                                self.callers[(fk, k)].append((u,) + tuple(x))

    # -- references
    def symbol_refs(self):
        """root -> Ref for data symbols: the declaration gen_data chose (data_symbols.txt), else the most
        informative one (gen_data's score), as gen_data would choose it."""
        decls = collections.defaultdict(collections.Counter)
        where = {}
        for u, r in self.units.items():
            for name, (disp, c) in r["decls"].items():
                n = self.syms.alias.get(name, name)
                if n in self.syms.inner:
                    continue
                decls[n][(disp, c)] += 1
                where.setdefault((n, disp, c), u)
        refs = {}
        for n, cands in decls.items():
            want = self.syms.chosen.get(n)
            pick = None
            if want:
                base = re.sub(r"(\[\d*\])+$", "", want.replace(" (extent by gap)", "").strip())
                same = [(cnt, d, c) for (d, c), cnt in cands.items() if re.sub(r"(\[\d*\])+$", "", d) == base]
                if same:
                    pick = max(same, key=lambda x: (x[0], repr(x[2])))[1:]
                elif cands:
                    continue   # gen_data chose a declaration of no struct: no reference
            if pick is None:
                pick = max(cands.items(), key=lambda kv: (gd.score(kv[0][1], gd.program(kv[0][1])), kv[1],
                                                          repr(kv[0][1])))[0]
            e = elem(pick[1])
            if e is not None and gd.host_size(e) and (e[0] in ("struct", "union") or (
                    pick[1][0] == "array" and gd.host_size(e) != gd.n64_layout(e)[0])):   # or an array of pointers
                refs[("sym", n)] = Ref(pick[1], (where[(n,) + pick], pick[0]))
                refs[("sym", n)].merge([c for (d, c), cnt in cands.most_common()])
        return refs

    def view_refs(self):
        """The roots that a REFERENCE view reaches take it, unless a symbol's declaration is theirs."""
        for root, entries in self.buckets.items():
            over = SYMBOL_REFERENCE.get(root[1]) if root[0] == "sym" else None
            if root in self.refs and not over:
                continue
            for entry in ([over] if over else REFERENCE):
                unit_re, vname = entry[:2]
                hit = next((e for e in entries if e.view == vname and re.search(unit_re, e.unit)), None)
                if over and not hit:   # a symbol's reference given by name: from any unit that has the view
                    u = next((u for u, r in self.units.items() if re.search(unit_re, u) and vname in r["view_types"]),
                             None)
                    hit = Entry(0, None, 0, "", "", "", u, "", 0, vname, "", None, "", "") if u else None
                if not hit:
                    continue
                src = (hit.unit, vname)
                if len(entry) > 3:   # the layout is another unit's view of the same records
                    src = next(((u, entry[4]) for u, r in self.units.items() if re.search(entry[3], u)
                                and entry[4] in r["view_types"]), None)
                c = self.units[src[0]]["view_types"].get(src[1]) if src else None
                if c is not None:
                    self.refs[root] = Ref(c, src)
                    self.refs[root].from_view = True
                    break
        for root, entries in self.buckets.items():   # positions in the reference's element
            ref = self.refs.get(root)
            if ref and ref.eh and (ref.array or root[0] != "sym" or ref.from_view):
                self.buckets[root] = [e._replace(h=e.h % ref.eh, n=None if e.n is None else e.n % ref.en)
                                      for e in entries]

    # -- keys and roots
    def key(self, ident, unit, depth=0):
        """(storage, host delta, N64 delta) of an access's base: a symbol is its container, a loaded pointer what
        the slot it was loaded from points to."""
        k = ident[0]
        if k == "sym":
            n = self.syms.alias.get(ident[1], ident[1])
            if n in self.syms.inner:
                c, h, nn = self.syms.inner[n]
                return ("sym", c), h, nn
            return ("sym", n), 0, 0
        if k == "ld":
            a = self.accs[unit][ident[1]]
            if a.hpos is None or depth > 8:
                return ("ptx", unit, ident[1]), 0, 0
            b, dh, dn = self.key(a.base, unit, depth + 1)
            return ("pt", b, a.hpos + dh), 0, 0
        if k == "param":
            return ("param", self.fkey(ident[1], unit), ident[2]), 0, 0
        if k == "retp":
            return ("retp", self.fkey(ident[1], unit), ident[2], unit), 0, 0
        return tuple(ident), 0, 0

    def fkey(self, fn, unit):
        """A function's name, with its unit when several units define one of that name."""
        us = self.multi.get(fn)
        return fn if us is None else (fn, unit if unit in us else None)

    def norm_h(self, root, h):
        ref = self.refs.get(root)
        return h % ref.eh if ref and ref.array and ref.eh and h is not None else h

    def expand(self, node, stack=()):
        """[(root, host delta, N64 delta or None)] for a storage."""
        if node in self.memo:
            return self.memo[node]
        if node in stack or len(stack) > 10:
            return [(node, 0, 0)]
        stack = stack + (node,)
        out = []
        kind = node[0]
        if kind == "param":
            for u, ident, h, n in self.callers.get((node[1], node[2]), ()):
                b, dh, dn = self.key(ident, u)
                for root, dh2, dn2 in self.expand(b, stack):
                    out.append((root, dh + dh2 + h, None if n is None or dn2 is None else dn + dn2 + n))
        elif kind == "retp":
            # what the function returns from a symbol or from this call's arguments; anything else it made itself
            # (an allocator's block, its own callee's result) is this call's own storage
            args = self.sites.get((node[1], node[2], node[3]))
            call = (node[3], args) if args is not None else None
            for u, ident, h, n in self.rets.get(node[1], ()):
                b, dh, dn = self.key(ident, u)
                b = self.subst(b, node[1], call)
                if b is None:
                    continue
                for root, dh2, dn2 in self.expand(b[0], stack):
                    out.append((root, dh + dh2 + b[1] + h,
                                None if n is None or dn2 is None or b[2] is None else dn + dn2 + b[2] + n))
            if not out:
                out = [(node, 0, 0)]
        elif kind == "pt":
            for root, dh, dn in self.expand(node[1], stack):
                h = self.norm_h(root, node[2] + dh)
                if (root, h) in self.links:   # a list's link: another record of the same storage
                    out.append((root, 0, 0))
                    continue
                p = ("pt", root, h)
                got = []
                for u, ident, hv, nv in self.stores.get(p, ()):
                    b, dh2, dn2 = self.key(ident, u)
                    vs = self.expand(b, stack)
                    if len({v[0] for v in vs}) > 1 or vs[0][0] in self.generic:
                        continue   # a value that is any of several storages (a helper's parameter) says nothing
                    for r2, dh3, dn3 in vs:
                        got.append((r2, dh2 + dh3 + hv, None if nv is None or dn3 is None else dn2 + dn3 + nv))
                # the slot points to what was stored there, when that is a few storages (a list or pool function's
                # parameter, which takes every caller's records, is not followed; nor is the slot itself, a swap)
                got = [g for g in got if g[0] != p]
                some = {g[0] for g in got}
                out += got if got and len(some) <= 4 else [(p, 0, 0)]
        else:
            out = [(node, 0, 0)]
        out = list(dict.fromkeys(out))
        if not out or len({o[0] for o in out}) > self.CAP:
            out = [(node, 0, 0)]
            self.generic.add(node)
        self.memo[node] = out
        return out

    def subst(self, b, fn, call):
        """A returned storage with the call's arguments for the function's parameters: (storage, h, n) or None."""
        if b[0] == "param" and b[1] == fn:
            if call is None:
                return None
            u, args = call
            x = args[b[2]] if b[2] < len(args) else None
            if not x:
                return None
            k, dh, dn = self.key(x[0], u)
            return k, dh + x[1], None if x[2] is None else dn + x[2]
        if b[0] == "pt":
            inner = self.subst(b[1], fn, call)
            if inner is None:
                return None
            return ("pt", inner[0], b[2] + inner[1]), 0, 0
        return (b, 0, 0) if b[0] == "sym" else None

    def place(self):
        """root -> [Entry]: every access with a known offset, on each root its storage reaches."""
        out = collections.defaultdict(list)
        for u, r in self.units.items():
            accs, extra = self.accs[u], r["extra"]
            for i, a in enumerate(accs):
                if a.hpos is None or a.base[0] == "abs" or i not in extra:
                    continue
                vn, path, opinion, _ = extra[i]
                b, dh, dn = self.key(a.base, u)
                via = fmt_root(b)
                for root, dh2, dn2 in self.expand(b):
                    n = None if opinion is None or dn2 is None else opinion + dn + dn2
                    out[root].append(Entry(self.norm_h(root, a.hpos + dh + dh2), n, a.width, a.kind, a.rw, a.flow,
                                           u, a.func, a.line, vn, path, a.value, a.text, via))
        return out

    def place_args(self):
        """root -> [(host offset, unit, func, line, callee, argument)]: the pointers passed to functions."""
        out = collections.defaultdict(list)
        for u, r in self.units.items():
            for fn, ln, callee, args in r["sites"]:
                for k, x in enumerate(args):
                    if not x or x[0][0] == "anon":
                        continue
                    b, dh, dn = self.key(x[0], u)
                    for root, dh2, dn2 in self.expand(b):
                        out[root].append((x[1] + dh + dh2, u, fn, ln, callee, k))
        return out

    def place_strides(self):
        """root -> [(base host offset, K, how, view element, unit, func, line, view)]."""
        out = collections.defaultdict(list)
        for u, r in self.units.items():
            for fn, ln, ident, boff, k, how, velem, vn in r["strides"]:
                if k is None or abs(k) <= 1:
                    continue
                b, dh, dn = self.key(ident, u)
                for root, dh2, dn2 in self.expand(b):
                    bo = None if boff is None else self.norm_h(root, boff + dh + dh2)
                    out[root].append((bo, abs(k), how, velem, u, fn, ln, vn))
        return out


# ---------------------------------------------------------------- 3. findings

# sides: the accesses a finding is about, as one list, or two (the 8-byte and the 4-byte ones) where either may be
# the wrong one
Finding = collections.namedtuple("Finding", "kind score root title lines sides why")
INTERP_FLOWS = ("copy", "zero", "unused")


def site(e):
    return f"{e.unit}:{e.line} {e.func}"


def fmt_entry(e, ref=None):
    n = f"N64 0x{e.n:X}" if e.n is not None else "N64 ?"
    r = ""
    if ref is not None and e.n is not None:
        r = f" (reference host 0x{ref.n2h(e.n):X})"
    return f"{e.rw} {e.width}B {n} at host 0x{e.h:X}{r}  {e.view} {e.path}  {site(e)}"


def layout_findings(flow):
    out = []
    for root, entries in flow.buckets.items():
        ref = flow.refs.get(root)
        typed = [e for e in entries if e.n is not None and e.kind != "agg"]
        if not typed:
            continue
        views = collections.defaultdict(list)
        if ref is not None:
            for e in typed:
                if e.n < ref.en and ref.n2h(e.n) != e.h:
                    views[(e.unit, e.view)].append(e)
            for (u, vn), es in views.items():
                if (u, vn) == ref.origin:
                    continue
                es = dedup(es)
                # an access that lands on the start of a reference scalar of its own width and class reaches that
                # field whatever N64 position its view claims (a PC view padded in host bytes); one that does not
                # reaches part of a field, or another kind of field
                wrong = [e for e in es if not fits(ref, e)]
                lines = [fmt_entry(e, ref) + ("" if e in wrong else "  [hits a field of its width]")
                         for e in es[:10]] + ([f"... {len(es) - 10} more"] if len(es) > 10 else [])
                if wrong:
                    out.append(Finding("layout", 5, root, f"{vn} in {u} is off the reference {ref.name()} at "
                                       f"{len({e.n for e in es})} N64 positions", lines, [es], None))
                elif re.sub(r"\[\]$", "", vn) in flow.units[u].get("pc_views", ()):
                    out.append(Finding("layout", 2, root, f"{vn} in {u} gives other N64 positions than the reference "
                                       f"{ref.name()}, but each access hits a field of its width there (a PC view "
                                       f"padded in host bytes?)", lines, [es], None))
                else:   # the N64 build's view too: its N64 positions are the N64's fields, not these
                    out.append(Finding("layout", 4, root, f"{vn} in {u} gives other N64 positions than the reference "
                                       f"{ref.name()}; each access hits a field of its width there, but the view is "
                                       f"the N64 build's too", lines, [es], None))
            continue
        # no reference: the mapping most views give, position by position
        support = collections.defaultdict(set)
        for e in typed:
            support[(e.n, e.h)].add((e.unit, e.view))
        by_n, by_h = collections.defaultdict(dict), collections.defaultdict(dict)
        for (n, h), vs in support.items():
            by_n[n][h] = vs
            by_h[h][n] = vs
        bad = collections.defaultdict(list)
        others = collections.defaultdict(set)
        for groups, pick in ((by_n, lambda e: e.h), (by_h, lambda e: e.n)):
            for pos, alts in groups.items():
                if len(alts) < 2:
                    continue
                ranked = sorted(alts.items(), key=lambda kv: -len(kv[1]))
                top = ranked[0][0] if len(ranked[0][1]) > len(ranked[1][1]) else None
                for e in typed:
                    if (pos == e.n if groups is by_n else pos == e.h) and (top is None or pick(e) != top):
                        bad[(e.unit, e.view)].append(e)
                        others[(e.unit, e.view)] |= {v for k, vs in alts.items() if k != pick(e) for v in vs}
        for (u, vn), es in bad.items():
            es = dedup(es)
            other = sorted({f"{v} ({uu})" for uu, v in others[(u, vn)] if (uu, v) != (u, vn)})
            lines = [fmt_entry(e) for e in es[:10]] + ([f"... {len(es) - 10} more"] if len(es) > 10 else [])
            lines.append("other views there: " + ", ".join(other[:6]) + (" ..." if len(other) > 6 else ""))
            out.append(Finding("layout", 4, root, f"{vn} in {u} disagrees with other views at "
                               f"{len({(e.n, e.h) for e in es})} places", lines, [es], None))
    return out


# Members a view or a reference leaves unnamed (m2c's unknown ranges, padding): they give no field there.
PAD_NAME = re.compile(r"(^|\.)(_?pad|unk|unknown|opaque|filler|fill|tail|rest|res|reserved|spare|skip|gap|bytes|raw)",
                      re.I)


def ref_bytes(ref, h):
    """The reference has no scalar at host offset h, only bytes (or nothing)."""
    x = ref.covering(h)
    return x is None or bool(x[6])


def fits(ref, e):
    """The access starts a reference scalar of its width and class (an integer, a float, a pointer)."""
    x = ref.covering(e.h)
    if x is not None and x[6]:   # the reference's bytes: an unknown range says nothing, a name takes bytes
        return bool(ec.is_pad(x[5]) or PAD_NAME.search(x[5] or "")) or e.width == 1
    word = {"p", "i"} if e.width == 8 else set()   # intptr_t and pointers are one class
    return any(y[2] == e.width and (e.kind == y[4] or {e.kind, y[4]} <= word) for y in ref.all_h.get(e.h, ()))


def dedup(es):
    seen, out = set(), []
    for e in sorted(es, key=lambda e: (e.n if e.n is not None else -1, e.h, e.unit, e.line)):
        k = (e.n, e.h, e.rw, e.width, e.func)
        if k not in seen:
            seen.add(k)
            out.append(e)
    return out


def stride_findings(flow):
    out = []
    for root, strides in flow.place_strides().items():
        ref = flow.refs.get(root)
        for b, k, how, velem, u, fn, ln, vn in strides:
            if ref is None or not ref.eh:
                continue
            ok = ref.strides(b)
            if any(h and k % h == 0 for h, n in ok):
                continue
            n64 = [(h, n) for h, n in ok if n and n != h and k % n == 0]
            if not n64 and k < ref.en:
                continue   # inside the element where the reference has no array: the view's own scalars
            what = {"index": f"an index `* {k}`", "incr": f"a pointer step of {k}", "typed": f"a {vn} index"}[how]
            if n64:
                h, n = n64[0]
                score = 5
                title = (f"{what} over {fmt_root(root)}: the N64 size of its element (N64 0x{n:X}, PC 0x{h:X})"
                         if k == n else f"{what} over {fmt_root(root)}: a multiple of its element's N64 size "
                         f"(N64 0x{n:X}, PC 0x{h:X})")
            else:
                score = 2
                title = (f"{what} over {fmt_root(root)}: not a multiple of the PC element "
                         f"(0x{ref.eh:X}, or an array's: {sorted(h for h, n in ok)})")
            base = f"+0x{b:X}" if b is not None else "+?"
            out.append(Finding("stride", score, root, title,
                               [f"{u}:{ln} {fn}: base {base}, reference {ref.name()}"],
                               [[Entry(b or 0, None, k, how, "", "", u, fn, ln, vn, "", None, "", "")]], None))
    return out


def width_findings(flow):
    out = []
    for root, entries in flow.buckets.items():
        ref = flow.refs.get(root)
        if ref is not None:
            for e in entries:
                if e.kind == "agg" or e.kind == "f":
                    continue
                x = ref.at(e.h)
                if x is None:
                    continue
                if e.width == 8 and x[2] == 4 and x[4] != "p":
                    score = 5 if e.kind == "p" else 4
                    out.append(Finding("width", score, root, f"an 8-byte {'pointer' if e.kind == 'p' else 'integer'} "
                                       f"where the reference {ref.name()} has the 4-byte {x[5]} (a u32 window "
                                       f"address)", [fmt_entry(e, ref)], [[e]], None))
                elif e.width == 4 and x[4] == "p" and x[2] == 8:
                    used = e.flow not in INTERP_FLOWS or (e.rw == "W" and e.value not in (0, None))
                    score = 4 if e.rw == "W" and e.flow not in ("zero",) else 3 if used else 1
                    out.append(Finding("width", score, root, f"4 bytes of the reference's pointer {x[5]} "
                                       f"({ref.name()})", [fmt_entry(e, ref) + f" [{e.flow}]"], [[e]], None))
            # where the reference has only bytes (a padded view), the views still have to agree with each other
            entries = [e for e in entries if ref_bytes(ref, e.h)]
        at = collections.defaultdict(list)
        for e in entries:
            if e.kind in ("p", "i") and e.width in (4, 8):
                at[e.h].append(e)
        for h, es in at.items():
            wide = [e for e in es if e.width == 8]
            narrow = [e for e in es if e.width == 4 and e.kind == "i"]
            if wide and narrow:
                wide, narrow = dedup(wide), dedup(narrow)
                out.append(Finding("width", 4, root, f"host +0x{h:X} is read or written as 8 bytes and as 4",
                                   [fmt_entry(e) + f" [{e.flow}]" for e in (wide + narrow)[:10]], [wide, narrow],
                                   None))
    return out


def merge_width(found):
    """One finding per (root, view, unit, kind of width) with every access."""
    groups = collections.OrderedDict()
    for f in found:
        if len(f.sides) != 1:
            groups[id(f)] = f
            continue
        e = f.sides[0][0]
        k = (f.root, e.unit, e.view, f.title.split(" (")[0] if f.kind == "width" else f.title, f.score)
        if k in groups:
            g = groups[k]
            groups[k] = g._replace(lines=g.lines + f.lines, sides=[g.sides[0] + f.sides[0]])
        else:
            groups[k] = f
    return list(groups.values())


def apply_reviewed(found):
    out = []
    for f in found:
        why = None
        for unit_re, what_re, reason in REVIEWED:
            if any(all(re.search(unit_re, e.unit) and any(re.search(what_re, x or "") for x in
                                                           (e.func, e.view, e.path, e.text, fmt_root(f.root)))
                       for e in side) for side in f.sides):
                why = reason
                break
        out.append(f._replace(why=why))
    return out


def inside_findings(flow):
    """Untyped accesses (raw offsets, byte views) and pointer arguments at a host offset inside a scalar of the
    reference, not at its start: a raw N64 offset into a record whose PC layout moved it."""
    out = []

    def hit(ref, h, width):
        x = ref.covering(h)
        if x is None or x[0] == h or x[2] < 2 or x[6] or PAD_NAME.search(x[5] or ""):
            return None
        return x
    args = flow.place_args()
    for root in sorted(set(flow.buckets) | set(args), key=fmt_root):
        ref = flow.refs.get(root)
        if ref is None or not ref.eh:
            continue
        norm = (lambda h: h % ref.eh) if ref.array or root[0] != "sym" or ref.from_view else (lambda h: h)
        for e in flow.buckets.get(root, ()):
            x = None if e.n is not None or e.kind == "agg" else hit(ref, e.h, e.width)
            if x:
                out.append(Finding("inside", 5 if x[4] == "p" else 4, root,
                                   f"{e.width}-byte {'write' if e.rw == 'W' else 'read'} at host +0x{e.h:X}, inside the "
                                   f"reference's {x[5]} (host +0x{x[0]:X}, {x[2]} bytes; {ref.name()})",
                                   [fmt_entry(e) + f" [{e.flow}]"], [[e]], None))
                continue
            y = None if e.n is not None or e.kind == "agg" else ref.at(e.h)
            if y and e.width < y[2] and not y[6] and not PAD_NAME.search(y[5] or "") and \
                    not any(z[2] <= e.width for z in ref.all_h.get(e.h, ())):
                # the first bytes of a wider field: a raw N64 offset that lands on another field, or a part of a word
                # (the N64's high bytes, the PC's low ones)
                out.append(Finding("inside", 3, root,
                                   f"{e.width}-byte {'write' if e.rw == 'W' else 'read'} of the first bytes of the "
                                   f"reference's {y[5]} (host +0x{y[0]:X}, {y[2]} bytes; {ref.name()})",
                                   [fmt_entry(e) + f" [{e.flow}]"], [[e]], None))
        for h, u, fn, ln, callee, k in args.get(root, ()):
            hn = norm(h)
            x = hit(ref, hn, 1)
            if x:
                e = Entry(hn, None, 0, "arg", "", "", u, fn, ln, callee, f"argument {k}", None, "", "")
                out.append(Finding("inside", 5 if x[4] == "p" else 4, root,
                                   f"a pointer to host +0x{hn:X} passed to {callee}, inside the reference's {x[5]} "
                                   f"(host +0x{x[0]:X}, {x[2]} bytes; {ref.name()})",
                                   [f"{u}:{ln} {fn}: {callee} argument {k}"], [[e]], None))
    return out


def findings(flow):
    found = layout_findings(flow) + merge_width(inside_findings(flow)) + merge_width(stride_findings(flow)) + \
        merge_width(width_findings(flow))
    found = apply_reviewed(found)
    order = {"layout": 0, "inside": 1, "stride": 2, "width": 3}
    return sorted(found, key=lambda f: (f.why is not None, -f.score, order[f.kind], fmt_root(f.root), f.title))


# Functions that put one argument's records behind slots of another's: callee -> (argument with the slots, their host
# offsets, argument the slots point into). Their parameters take every caller's lists, so the stores inside them are
# not followed; each call says which list and which records.
LINKS = {
    "func_80091FBC": (0, (8,), 1),                     # InsertLLE(list, object, before): the list's head
    "pool_init": (0, (0x10, 0x18, 0x20), 1),           # InitStatLL's set-up(pool, mem, n, size): mem, active, free
    "struct_fields_init": (0, (0x10, 0x18, 0x20), 1),  # overlay B's InitStatLL(pool, mem, size, n, flags)
}
# Views that give a storage reached only through pointers its reference layout: (unit regex, view name, why), and
# optionally (unit regex, view name) of the layout when it is another view's (records that reach the two views
# through paths the flow does not follow, such as message queues).
REFERENCE = [
    (r"src/blob/groups/level_objects/group\.c$", "Node",
     "the level-object nodes (the targets): N64 layout in the arena, 32-bit links and state (POINTERS.md 10.2)"),
    (r"src/blob/groups/level_objects/group\.c$", "Matrix64",
     "the dead-reckoning records D_80150F38: N64 layout, body a u32 window address (POINTERS.md 10.2)"),
    (r"src/blob/groups/engine_sound_sync/audiomgr_thread\.c$", "tFxSequence",
     "the audio manager's FX tags, MAT3 audiomgr.c's tFxSequence: InsertLLE's two links are pointers"),
    (r"src/blob/groups/menu_save_options/hud_setup\.c$", "Voice",
     "the FX tag pool D_80110244 that hud_setup allocates: the tags", r"engine_sound_sync/audiomgr_thread\.c$",
     "tFxSequence"),
    (r"src/blob/groups/engine_sound_sync/audiomgr_thread\.c$", "anon_ll_id_3c",
     "the audio manager's sequence tags, MAT3 audiomgr.c's: InsertLLE's two links are pointers"),
    (r"src/blob/groups/engine_sound_sync/audiomgr_thread\.c$", "anon_te_tag_c",
     "the sorted-tag tree's nodes, MAT3 audiomgr.c's: left, right and the tag, pointers"),
    (r"src/blob/groups/menu_save_options/hud_setup\.c$", "Chan",
     "the sequence tag pool D_80110270 that hud_setup allocates", r"engine_sound_sync/audiomgr_thread\.c$",
     "anon_ll_id_3c"),
    (r"src/blob/groups/engine_sound_sync/engine_sound_sync\.c$", "Source",
     "the 3D-sound sources (an FX tag's data): InsertLLE's two links are pointers"),
    (r"src/blob/groups/entity_helpers/group\.c$", "CamSlot", "an FX tag's data, a 3D-sound source",
     r"src/blob/groups/engine_sound_sync/engine_sound_sync\.c$", "Source"),
    (r"src/blob/sound_control\.c$", "Blit", "a blit, sound_control.c's (POINTERS.md 9.3)"),
    (r".", "Blit", "a blit: sound_control.c's layout (POINTERS.md 9.3)", r"src/blob/sound_control\.c$", "Blit"),
]
# Symbols whose reference is not gen_data's choice, because the chosen declaration is one of the views in question:
# symbol -> (unit regex, view name, why).
SYMBOL_REFERENCE = {
    "D_80142DD8": (r"src/blob/groups/engine_sound_sync/audiomgr_thread\.c$", "tAudioMsg",
                   "the audio manager's messages, MAT3 audiomgr.c's tAudioMsg (gen_data takes transform's Msg)"),
}
# Findings read and judged: (unit regex, regex on the function, view, field, access text or storage, why). A finding
# whose accesses all match one rule (or one side's, for an 8-byte and 4-byte pair) is listed as reviewed.
REVIEWED = [
    (r".", r"^pc_statll_(set_)?link$",
     "by design: the StatLL helpers take a link at the width the list's n64 flag gives (POINTERS.md 10.2); the other "
     "width never runs on that list"),
    (r"src/blob/dma_wait_complete\.c$|src/rom/lib_1050_sdk\.c$", r"^(Sched|PcSched)$",
     "by design: dma_wait_complete's Sched is a PC view of the native scheduler (lib_1050_sdk.c's PcSched), padded "
     "in host bytes to curRSPTask (ULTRA.md); both reach host +0x2B0"),
    (r"src/blob/groups/engine_sound_sync/engine_sound_sync\.c$", r"^Effect$",
     "by design: engine_sound_sync's Effect is a PC view of the FX tag padded in host bytes; each access is the "
     "tag's field"),
    (r"src/rom/lib_17dc0\.c$", r"^(Seq_|SequenceContext)",
     "by design: lib_17dc0.c's PC views of a sequence slot are padded in host bytes (SEQ_PC_OFF, "
     "include/sequence_context.h), so each access is the slot's field whatever N64 position the padding gives"),
    (r"src/blob/groups/(updatefx/updatefx|entity_lookup/group)\.c$", r"^(AudioMsg|Msg)$",
     "by design: a stop or interrupt message's tag follows the header (tAudioMsg's startEffect, host +8); the "
     "union's N64 positions are its largest member's"),
    (r"src/blob/groups/menu_save_options/ctx_slot\.c$", r"^DecodeGhost$",
     "by design: a heap handle slot is 8 bytes holding a zero-extended window address, and a u32 reader takes its "
     "low half (POINTERS.md 9.10)"),
    (r"src/blob/camera_lerp_position\.c$", r"Rec20",
     "harmless: camera_lerp_position clears the mine slots' count and its padding with an intptr_t 0"),
    (r"src/blob/race_init_helper\.c$", r"^race_init_helper$",
     "by design: the s16 read at +0xF4 is the car's s32 node handle's low half, the PC's first s16 (ENDIAN.md 3.1)"),
    (r"src/blob/groups/blast/blast\.c$", r"^save_write_data$",
     "by design: StartBlast's data is an s16 car slot with linkToCar, an f32 position without (endian_check's "
     "NOT_FOLLOWED)"),
    (r"src/rom/", r"__osSiDmaBuffer|OSPifRam",
     "by design: libultra builds the PIF commands byte by byte in its word buffer, as the SI emulation reads it"),
    (r"src/blob/func_800AED20\.c$", r"^func_800AED20$",
     "by design: the 3D-sound source's active byte; engine_sound_sync.c's Source names it and the queued byte as a "
     "u32 flags word that no code reads whole"),
    (r"src/blob/func_800A1910\.c$", r"^func_800A1910$",
     "by design: func_800A1910 compares bytes (memcmp); its callers pass typed pointers"),
    (r"src/blob/groups/audio_pitch_adjust/ap\.c$|audio_effect_setup_b132\.c$|src/blob/func_8009211C\.c$",
     r"^\*caller_[ab]\.arg0$",
     "dead: ap.c's caller_a and caller_b are stand-in callers kept for matching (never called); their walk through "
     "the tag's list field is not the tags' link"),
]


# ---------------------------------------------------------------- 4. report

TITLES = {"layout": "## 1. Layouts: views of one storage that put a field at different places",
          "inside": "## 1b. Raw offsets: an untyped access or a pointer argument inside a field of the reference",
          "stride": "## 2. Strides: an index or a pointer step over a storage that is not its PC element size",
          "width": "## 3. Pointer widths: 8 bytes over a u32 window address, or 4 bytes of a pointer"}


def run_analysis(results, build):
    syms = Symbols(build)
    return Flow(results, syms)


def report(flow, out):
    found = findings(flow)
    lines = []
    w = lines.append
    for kind in ("layout", "inside", "stride", "width"):
        fs = [f for f in found if f.kind == kind]
        open_ = [f for f in fs if not f.why]
        w(TITLES[kind])
        w("")
        hist = collections.Counter(f.score for f in open_)
        w("open by score: " + (", ".join(f"{k}: {v}" for k, v in sorted(hist.items(), reverse=True)) or "none")
          + f"; reviewed: {len(fs) - len(open_)}")
        for f in fs:
            ref = flow.refs.get(f.root)
            w(f"  [{f.score if not f.why else 'reviewed'}] {fmt_root(f.root)}: {f.title}"
              + (f": {f.why}" if f.why else ""))
            if kind == "layout" and ref:
                w(f"      reference: {ref.name()}, element N64 0x{ref.en:X}, host 0x{ref.eh:X}")
            for line in f.lines[:14]:
                w(f"      {line}")
            if len(f.lines) > 14:
                w(f"      ... {len(f.lines) - 14} more")
        w("")
    n_acc = sum(len(v) for v in flow.accs.values())
    placed = sum(len(v) for v in flow.buckets.values())
    open_all = [f for f in found if not f.why]
    summary = (f"{len(flow.units)} units, {n_acc} accesses ({placed} placed on {len(flow.buckets)} storages, "
               f"{len(flow.refs)} with a reference layout); open findings: "
               + ", ".join(f"{k} {sum(1 for f in open_all if f.kind == k)}" for k in ("layout", "inside", "stride", "width"))
               + f" ({sum(1 for f in open_all if f.score >= 4)} scored 4 or 5); reviewed {len(found) - len(open_all)}")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("# layout_check report (tools/port/layout_check.py; docs/port/LAYOUT.md)\n\n" + summary + "\n\n"
                   + "\n".join(lines) + "\n")
    print(summary)
    print(f"report: {ec.rel(out)}")
    return found


def dump_storage(flow, pattern):
    for root in sorted(flow.buckets, key=fmt_root):
        if not re.search(pattern, fmt_root(root)):
            continue
        ref = flow.refs.get(root)
        print(f"{fmt_root(root)}: {len(flow.buckets[root])} accesses"
              + (f", reference {ref.name()} (N64 0x{ref.en:X}, host 0x{ref.eh:X})" if ref else ""))
        for e in sorted(flow.buckets[root], key=lambda e: (e.h, e.n if e.n is not None else -1, e.unit, e.line)):
            print(f"    {fmt_entry(e, ref)} [{e.flow}] via {e.via}")


# ---------------------------------------------------------------- 5. main

def collect(build, jobs, reuse, only, readelf):
    cache = build / "endian"
    cache.mkdir(parents=True, exist_ok=True)
    cmds = ec.game_commands(build)
    if only:
        want = {str((ROOT / p).resolve()) for p in only.split(",") if p}
        cmds = [c for c in cmds if str(Path(c[0]).resolve()) in want]
    with multiprocessing.Pool(jobs) as pool:
        for uid, err in pool.imap_unordered(ec.compile_one, [(s, c, d, cache, reuse) for s, c, d in cmds],
                                            chunksize=4):
            if err:
                print(f"compile failed: {uid}: {err}", file=sys.stderr)
        return dict(pool.imap_unordered(analyze_unit, [(ec.unit_id(s), cache, build, readelf) for s, _, _ in cmds],
                                        chunksize=4))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("-j", "--jobs", type=int, default=2)
    ap.add_argument("--reuse", action="store_true", help="reuse the dumps in build/pc/endian newer than their source")
    ap.add_argument("--only", default="", help="comma-separated source paths to analyze")
    ap.add_argument("--storage", help="print every access placed on the storages matching this regex")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--readelf", default="readelf")
    ap.add_argument("--selftest", action="store_true", help="check the detector on synthetic cases")
    a = ap.parse_args()
    build = a.build.resolve()
    if a.selftest:
        return selftest(build, a.readelf)
    results = collect(build, a.jobs, a.reuse, a.only, a.readelf)
    flow = run_analysis(results, build)
    if a.storage:
        dump_storage(flow, a.storage)
        return 0
    report(flow, a.out or build / "endian" / "layout.txt")
    return 0


SELFTEST_C = r"""
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef short s16; typedef int s32;
typedef signed char s8; typedef float f32; typedef __UINTPTR_TYPE__ uintptr_t;
extern void show(const char *);
/* 1. Level-object nodes keep their N64 layout, linked by 32-bit window addresses (the reference, SELFTEST_REFERENCE).
 * A walk whose view links them by a host pointer reads 8 bytes at +0 and puts the sound and state 4 bytes late. */
typedef struct Node { u32 next; u8 flags; u8 pad5[3]; u32 key; u8 padc[0x54]; s32 sound; s8 state; } Node;
typedef struct Pool { u8 b0; s32 count; Node *head; } Pool;
Pool gPool;
void ref_walk(void) { Node *n; for (n = gPool.head; n; n = (Node *)(uintptr_t)n->next) { n->state = 2; n->sound = -1; } }
typedef struct Target { struct Target *next; u8 pad4[92]; s32 sound; s8 state; } Target;
void bad_walk(void) { Target *t; for (t = (Target *)gPool.head; t; t = t->next) { t->state = 2; t->sound = -1; } }
/* 2. A record reached through a parameter whose struct has another name: the caller's view has the u32 body, the
 * callee's an 8-byte pointer, so its position is 4 bytes late (random_float's Reckon). */
typedef struct Body { f32 r[4]; f32 mass; } Body;
typedef struct RecA { u32 body; f32 uv[9]; f32 pos[3]; } RecA;
typedef struct RecB { Body *body; f32 uv[9]; f32 pos[3]; } RecB;
RecA *gRecs;
f32 use_b(RecB *r) { return r->body->mass + r->pos[0]; }
f32 caller_ab(int i) { RecA *a = gRecs + i; a->body = 0x80100000u; a->pos[0] = 1.0f; return use_b((RecB *)a); }
/* 3. Nodes stored into slots and read back through a view with a host-pointer link (overlay B's Object). */
typedef struct Slot { Node *object; s16 active, count; } Slot;
Slot *gSlots;
void fill_slots(void) { Node *n; int k = 0; for (n = gPool.head; n; n = (Node *)(uintptr_t)n->next) gSlots[k++].object = n; }
typedef struct Obj { struct Obj *next; u8 flags; } Obj;
int use_slots(int i) { return ((Obj *)gSlots[i].object)->flags; }
/* 4. m2c byte arithmetic at the N64 stride of a record that is longer on the PC (func_800FBE60's * 952), and the same
 * at the PC stride. */
typedef struct Car { u8 a[0x10]; void *vis[2]; u8 b[0x10]; f32 speed; } Car;
Car gCars[4];
void bad_stride(int i) { *(f32 *)((u8 *)gCars + i * 0x2C + 0x28) = 0.0f; }
void good_stride(int i) { *(f32 *)((u8 *)gCars + i * sizeof(Car) + 0x30) = 0.0f; }
/* 5. A typed index through a view of the N64 element size. */
typedef struct Rec16 { void *p; u32 a, b; } Rec16;
Rec16 gRec16[8];
typedef struct Rec12 { u32 p, a, b; } Rec12;
u32 typed_bad(int i) { return ((Rec12 *)gRec16)[i].a; }
u32 typed_good(int i) { return gRec16[i].a; }
/* 6. A raw N64 offset into a record whose PC layout moved the field: a pointer into the middle of another field (the
 * profile's name at +20). */
typedef struct Prof { void *next, *prev, *file; u32 id[2]; char name[13]; } Prof;
Prof gProf[4];
void bad_name(int k) { show((char *)&gProf[k] + 20); }
void good_name(int k) { show(gProf[k].name); }
/* 7. Pointer width: a u32 read of a pointer slot, used as a value. */
typedef struct Holder { void *ptr; u32 count; } Holder;
Holder gHolder;
u32 low_half(void) { return *(u32 *)&gHolder.ptr + 1; }
/* 8. A sorted tree: the insert walks down it (pp = &(*pp)->right) and calls its order through the tree's function
 * pointer; the order (draw_sprites) read the node's tag and the tag's priority at the N64 offsets: the node's +8 is
 * its right link on the PC, so the priority read lands inside another node's link. */
typedef struct Tag { void *link[2]; s32 id; s32 prio; } Tag;
typedef struct TNode { struct TNode *left, *right; Tag *tag; } TNode;
typedef struct Tree { s32 (*cmp)(TNode *, TNode *); s32 count; TNode *root; } Tree;
Tree gTree, gTree2;
TNode gTNodes[8], gTNodes2[8];
void tree_insert(Tree *t, TNode **pp, TNode *n) {
    while (1) {
        if (*pp == 0) { *pp = n; n->left = 0; (*pp)->right = 0; t->count++; return; }
        if (t->cmp(n, *pp) < 0) pp = &(*pp)->left; else pp = &(*pp)->right;
    }
}
#define PRIO_N64(x) (*(s32 *)(*(u8 **)((u8 *)(x) + 8) + 0xC))
s32 order_bad(void *a, void *b) { return PRIO_N64(a) - PRIO_N64(b); }
s32 order_good(void *a, void *b) { return ((TNode *)a)->tag->prio - ((TNode *)b)->tag->prio; }
typedef s32 (*Order)(TNode *, TNode *);
void tree_setup(void) { gTree.cmp = (Order)order_bad; gTree2.cmp = (Order)order_good; }
void tree_add(int i) {
    tree_insert(&gTree, &gTree.root, &gTNodes[i]);
    tree_insert(&gTree2, &gTree2.root, &gTNodes2[i]);
}
/* 9. Views padded in bytes up to a field. One the PC build alone has reaches the PC's field (a PC view, 2); one the
 * N64 build has too gives the N64's field (+8, b) where the PC has another (a), as func_8008BEA4's In (4). */
#ifdef TARGET_PC
typedef struct PadPC { u8 pad[8]; u32 a; } PadPC;
#endif
typedef struct PadAll { u8 pad[8]; u32 b; } PadAll;
u32 pad_pc(int i) { return ((PadPC *)&gRec16[i])->a; }
u32 pad_all(int i) { return ((PadAll *)&gRec16[i])->b; }
"""
SELFTEST_REFERENCE = [(r"selftest\.c$", "Node", "the synthetic nodes: N64 layout, 32-bit links")]
# (kind, storage regex, regex on the title and lines) the selftest must find, and must not
SELFTEST_EXPECT = [
    ("layout", r"^\*\(gPool\+0x8\)$", r"Target in .* off the reference .*Node"),
    ("width", r"^\*\(gPool\+0x8\)$", r"8-byte pointer .* 4-byte next.*bad_walk"),
    ("layout", r"^\*\(gRecs\+0x0\)$", r"RecB in .* disagrees"),
    ("width", r"^\*\(gRecs\+0x0\)$", r"8 bytes and as 4"),
    ("layout", r"^\*\(gPool\+0x8\)$", r"Obj in .* off the reference"),
    ("stride", r"^gCars$", r"`\* 44`.*N64 size.*bad_stride"),
    ("stride", r"^gRec16$", r"`\* 12`.*N64 size.*typed_bad"),
    ("layout", r"^gRec16$", r"Rec12 in .* off the reference .*Rec16.*typed_bad"),
    ("inside", r"^gProf$", r"host \+0x14 passed to show.*inside the reference's file.*bad_name"),
    ("width", r"^gHolder$", r"4 bytes of the reference's pointer ptr.*low_half"),
    ("inside", r"^gTNodes$", r"4-byte read at host \+0xC, inside the reference's right.*order_bad"),
    ("layout", r"^gRec16$", r"PadPC in .*a PC view padded in host bytes"),
    ("layout", r"^gRec16$", r"PadAll in .*the view is the N64 build's too.*pad_all"),
]
SELFTEST_ABSENT = [
    (r".", r"ref_walk"), (r".", r"good_stride"), (r".", r"typed_good"), (r".", r"good_name"), (r".", r"fill_slots"),
    (r".", r"order_good|tree_insert"),
]


def selftest(build, readelf):
    """Run the check on SELFTEST_C, with SELFTEST_REFERENCE as REFERENCE and no review table."""
    global REFERENCE, REVIEWED
    d = build / "endian" / "layout_selftest"
    d.mkdir(parents=True, exist_ok=True)
    src = d / "selftest.c"
    src.write_text(SELFTEST_C)
    r = subprocess.run(["cc", "-std=gnu89", "-g", "-w", "-fno-strict-aliasing", "-DTARGET_PC", *ec.DUMP_FLAGS,
                        "-c", str(src), "-o", str(d / "selftest.o")], cwd=d, capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        return 1
    saved = REFERENCE, REVIEWED
    REFERENCE, REVIEWED = SELFTEST_REFERENCE, []
    try:
        _, res = analyze_unit(("selftest", d, build, readelf, src))
        flow = Flow({"selftest": res}, Symbols(d / "none"))
        found = findings(flow)
    finally:
        REFERENCE, REVIEWED = saved
    text = [(f.kind, fmt_root(f.root), f.title + " | " + " | ".join(f.lines)) for f in found]
    bad = 0
    for kind, root_re, what_re in SELFTEST_EXPECT:
        ok = any(k == kind and re.search(root_re, r) and re.search(what_re, t) for k, r, t in text)
        bad += not ok
        print(f"{'ok  ' if ok else 'MISS'} {kind} {root_re}: {what_re}")
    for root_re, what_re in SELFTEST_ABSENT:
        hit = [t for k, r, t in text if re.search(root_re, r) and re.search(what_re, t)]
        bad += bool(hit)
        print(f"{'ok  ' if not hit else 'FALSE'} nothing about {what_re}" + (f": {hit[0][:120]}" if hit else ""))
    print(f"selftest: {len(found)} findings, {bad} wrong")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
