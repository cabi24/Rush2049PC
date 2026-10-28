#!/usr/bin/env python3
"""Find raw N64 offsets into records whose PC layout moved the field: m2c code that reaches a field as
`M2C_FIELD(p, T *, 0x1A)` or `*(T *)((u8 *)p + 0x1A)` through a pointer the code types as `void *` or `s32`.

    python3 tools/port/offset_check.py [--build build/pc] [-j 2] [--reuse] [--out build/pc/endian/offset.txt]
    python3 tools/port/offset_check.py --reuse --func func_80105DA8     # every raw access of the matching functions
    python3 tools/port/offset_check.py --selftest                       # ctest offset_check

docs/port/LAYOUT.md 9. layout_check.py checks a raw offset only against a storage's reference layout, and only where
it lands inside a field; a callback whose record arrives as `void *` from a table in the ROM (a blit's AnimFunc, called
by NewMultiBlit through the slot it was loaded into) has a storage of its own, with no reference, and +0x1A (the N64
blit's Hide) is the first byte of the PC blit's X. This check types each such storage:

1. **Accesses** are layout_check's (endian_check's GIMPLE, the same compile cache), each put on the storages it
   reaches (parameters, slots, returns, lists). A *raw* access is one whose view has no opinion on its N64 position:
   bytes, void, a scalar pointer read past its scalar (m2c's offsets).
2. **The record's type.** A storage's record is one whose PC layout differs from the N64's when
   - its reference layout (layout_check's: a symbol's declaration, a REFERENCE view) is such a struct; or
   - a typed view of the code reaches the same storage with such a struct (a callee's parameter: UpdateBlit's
     `Sprite`, DelBlit's `Blit`; another reader of the same slot; a caller's argument), the struct a view of most
     accesses gives; or
   - ROLES names the function's parameter: a function only the ROM's tables call (an animation callback that
     NewMultiBlit runs through the AnimFunc it copied from a MultiBlit, an object's or a menu's handler), which no C
     calls and no typed view reaches, typed by its role.
   A storage whose typed views all keep the N64 layout (the model record, the level-object nodes) holds an N64
   record, and its raw offsets are right.
3. **Findings.** For a raw access in code both builds compile (not under `#ifdef TARGET_PC`, no PC macro on its line:
   CAR_OFS, BLIT_HIDE, ...), its offset is the N64 offset: the field the N64 layout has there is at host offset
   `n2h(off)` on the PC; a finding when that is not the offset the PC code uses (5 for a write or a pointer-sized
   access, 4 for a read). An access in PC-only code, or through a PC macro, must start a field of its width (3).
   A pointer to a raw offset passed to a function is checked the same way. An offset not written on its line (an
   index, a folded pointer step) scores 2 and is listed, not counted. Raw accesses on storages nothing types are
   listed by function in offset_untyped.txt: what ROLES or the review in LAYOUT.md 9 must say.
Findings read and judged are in REVIEWED; --strict exits 1 on an open one (CI). Stdlib and binutils, Python 3.9+.
"""
import argparse
import bisect
import collections
import pickle
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import endian_check as ec  # noqa: E402
import gen_data as gd  # noqa: E402
import layout_check as lc  # noqa: E402

ROOT = ec.ROOT

# A function's parameter that only the ROM's tables (or an indirect call the flow does not follow) give a record:
# (function regex, parameter, (unit regex, view name) of the record's PC layout, why).
ROLES = []
# Findings read and judged: (unit regex, regex on the function or the storage, why).
REVIEWED = [
    (r"src/blob/func_800AED20\.c$", r"^func_800AED20$",
     "by design: _GetSource clears the 3D-sound source's active byte at EMITTER_ACTIVE (PC +0x10), which "
     "engine_sound_sync.c's Source names as the first byte of a u32 flags word no code reads whole (LAYOUT.md 7)"),
    (r"src/blob/object_action_clear\.c$", r"^object_action_clear$",
     "unreachable: no C calls object_action_clear, no asm references it, no C takes its address, and no table of "
     "the ROM's data holds it (the loader's conversions, LAYOUT.md 9.3)"),
    (r".", r"^\*pc_text_strings\(\)",
     "by design: the text bank's string table holds host pointers (port/assets/text.c), read by index (strs[22]) "
     "8 bytes wide: PC code"),
    (r"src/blob/groups/results_table/results_table\.c$", r"^(drawBattleScore|func_80102448|func_80102F30)$",
     "by design: the name pointer is made with PROFILE_OFS(20) a few lines before the call (POINTERS.md 9.10)"),
    (r"src/ovl_b/func_803936A8\.c$", r"^func_803936A8$",
     "by design: NameData is a PC-only view padded to the profile's name at +0x20 (PROFILE_OFS, POINTERS.md 9.10)"),
]


# ---------------------------------------------------------------- 1. types

SCALAR_VIEW = re.compile(r"^(\?|ptr|[us](8|16|32|64)|f(32|64)|u?intptr_t|size_t|M2C_UNK\w*)(\[\])?$|"
                         r"\b(unsigned|signed|short|long|int|char|float|double|void|_Bool)\b")


class Moved:
    """A record type whose PC layout moved fields: its canon struct, N64 and host sizes, scalars by host offset."""

    def __init__(self, c, origin):
        self.ref = lc.Ref(c, origin)
        self.c, self.origin = c, origin
        self.en, self.eh = self.ref.en, self.ref.eh
        self.by_n = {}
        for xs in self.ref.all_h.values():
            for x in xs:
                self.by_n.setdefault(x[1], x)
        self.n_starts = sorted(self.by_n)

    def key(self):
        return (self.en, self.eh, tuple((x[1], x[0], x[2], x[3]) for x in sorted(self.ref.by_h.values())))

    def family(self):
        """The record's identity across views: its sizes and where its pointers are, N64 and host."""
        return (self.en, self.eh, tuple(self.ref.n2h(n) for n in range(0, self.en, 4)))

    def named(self):
        return sum(1 for x in self.ref.by_h.values() if not x[6] and not ec.is_pad(x[5] or "")
                   and not lc.PAD_NAME.search(x[5] or ""))

    def field(self, h):
        x = self.ref.covering(h)
        return f"{x[5]}" if x else "padding"

    def n64_field(self, n):
        """The N64 layout's field at N64 offset n, by the host offset it maps to."""
        return self.field(self.ref.n2h(n))

    def n64_covering(self, n):
        i = bisect.bisect_right(self.n_starts, n) - 1
        if i < 0:
            return None
        x = self.by_n[self.n_starts[i]]
        return x if x[1] <= n < x[1] + x[3] else None

    def h2n(self, h):
        """The N64 position of host offset h (a field's, or past the last field before it)."""
        i = bisect.bisect_right(self.ref.starts, h) - 1
        if i < 0:
            return h
        x = self.ref.by_h[self.ref.starts[i]]
        if h < x[0] + x[2]:
            return x[1] + min(h - x[0], x[3] - 1)
        return x[1] + x[3] + (h - x[0] - x[2])

    def fits_pc(self, h, width, kind):
        return lc.fits(self.ref, lc.Entry(h, None, width, kind, "", "", "", "", 0, "", "", None, "", ""))

    def fits_n64(self, n, width, kind):
        """The access, at N64 offset n, starts an N64 field of its width (a pointer: 4 or 8 bytes), or is in bytes."""
        x = self.n64_covering(n)
        if x is None:
            return False
        if x[6]:
            return True
        if x[1] != n:
            return False
        return width == x[3] or (x[4] == "p" and width in (4, 8))


def pc_macros():
    """Macro names defined for the PC build only, in the game's headers (pc_prelude.h and include/)."""
    out = set()
    for p in [ROOT / "port/include/pc_prelude.h"] + sorted((ROOT / "include").rglob("*.h")):
        try:
            text = p.read_text(errors="replace").splitlines()
        except OSError:
            continue
        lines = lc.pc_lines(p) if p.name != "pc_prelude.h" else set(range(1, len(text) + 1))
        for i, line in enumerate(text, 1):
            m = re.match(r"\s*#\s*define\s+(\w+)", line)
            if m and i in lines:
                out.add(m.group(1))
    return out


class Source:
    """A unit's source: its PC-only lines and the PC macros defined in it."""
    _cache = {}

    def __init__(self, unit, macros, path=None):
        path = path or ROOT / unit
        try:
            self.text = path.read_text(errors="replace").splitlines()
        except OSError:
            self.text = []
        self.pc = lc.pc_lines(path)
        self.macros = set(macros)
        for i, line in enumerate(self.text, 1):
            m = re.match(r"\s*#\s*define\s+(\w+)", line)
            if m and i in self.pc:
                self.macros.add(m.group(1))
        self._re = re.compile(r"\b(" + "|".join(sorted(self.macros)) + r")\b") if self.macros else None

    @classmethod
    def register(cls, unit, macros, path):
        cls._cache[unit] = Source(unit, macros, path)

    @classmethod
    def get(cls, unit, macros):
        s = cls._cache.get(unit)
        if s is None:
            s = cls._cache[unit] = Source(unit, macros)
        return s

    def line(self, n):
        return self.text[n - 1] if 0 < n <= len(self.text) else ""

    def literal(self, n, off):
        """The offset is written on the line (or the two before it: a statement over several lines)."""
        text = " ".join(self.line(i) for i in range(max(1, n - 2), n + 1))
        return bool(re.search(r"\b(0[xX]0*%X|%d)\b" % (off, off), text, re.I))

    def pc_only(self, n):
        """The line is PC code: under `#ifdef TARGET_PC`, or it names a PC macro (CAR_OFS, BLIT_HIDE...)."""
        if n in self.pc:
            return "pc"
        if self._re and any(self._re.search(self.line(i)) for i in range(max(1, n - 2), n + 1)):
            return "macro"   # (or on the two lines before: a pointer made there and passed here)
        return None


# ---------------------------------------------------------------- 2. the check

Raw = collections.namedtuple("Raw", "unit func line rw width kind h root via text index stride lo bn")
Finding = collections.namedtuple("Finding", "score root func unit title lines raws why")


class Check:
    def __init__(self, flow):
        self.flow = flow
        self.macros = pc_macros()
        self.moved = {}     # (unit, view name) -> Moved
        for u, r in flow.units.items():
            for vn, c in (r.get("moved_views") or {}).items():
                self.moved[(u, vn)] = Moved(c, (u, vn))
        # the most informative view of each record (its fields named, not padded) names the fields: a view of
        # UpdateBlit's calls Hide unk1A
        self.canon = {}
        for (u, vn), m in sorted(self.moved.items()):
            f = m.family()
            best = self.canon.get(f)
            score = (m.named(), vn == "Blit", -len(u))
            if best is None or score > best[0]:
                self.canon[f] = (score, m)
        self.place()
        self.type_roots()

    def place(self):
        """Every access on each root it reaches, raw or typed: root -> [(Raw, view name or None, typed)]."""
        flow = self.flow
        self.on = collections.defaultdict(list)
        for u, r in flow.units.items():
            accs, extra = flow.accs[u], r["extra"]
            for i, a in enumerate(accs):
                if a.hpos is None or a.base[0] == "abs" or i not in extra or a.kind == "agg":
                    continue
                vn, path, opinion, _ = extra[i]
                b, dh, dn = flow.key(a.base, u)
                via = lc.fmt_root(b)
                for root, dh2, dn2 in flow.expand(b):
                    # lo: the function's own offset from its base; bn: the N64 position of the base, where the
                    # flow's steps to it were typed (a caller's &rec->field), else None
                    bn = None if dn is None or dn2 is None else dn + dn2
                    raw = Raw(u, a.func, a.line, a.rw, a.width, a.kind, a.hpos + dh + dh2, root, via, a.text, i,
                              a.stride, a.hpos, bn)
                    self.on[root].append((raw, vn, opinion is not None))
        self.args = flow.place_args()
        # pointers passed at a raw offset (`f(arg0 + 0x1A)`, an argument whose N64 position no view gives)
        self.rawargs = collections.defaultdict(list)
        for u, r in flow.units.items():
            for fn, ln, callee, args in r["sites"]:
                for k, x in enumerate(args):
                    if not x or x[0][0] == "anon" or not x[1]:
                        continue
                    b, dh, dn = flow.key(x[0], u)
                    for root, dh2, dn2 in flow.expand(b):
                        bn = None if dn is None or dn2 is None else dn + dn2
                        # an offset through bytes is the same number on both sides (x[1] == x[2]): the N64's,
                        # in code both builds compile; a typed one (&r->x) has its N64 position
                        self.rawargs[root].append(Raw(u, fn, ln, "A", 0, "arg", x[1] + dh + dh2, root,
                                                      f"{callee} argument {k}", "", None, None,
                                                      x[2] if x[2] is not None else x[1], bn))

    def view_type(self, u, vn):
        m = self.moved.get((u, vn))
        return self.canonical(m)

    def canonical(self, m):
        if m is None:
            return None
        best = self.canon.get(m.family())
        return best[1] if best else m

    def type_roots(self):
        """root -> (Moved or None, how): the record the root's storage holds."""
        flow = self.flow
        self.types = {}
        roles = []
        for fre, k, (ure, vn), why in ROLES:
            src = next((m for (u, v), m in self.moved.items() if v == vn and re.search(ure, u)), None)
            roles.append((re.compile(fre), k, src, why))
        for root, items in self.on.items():
            ref = flow.refs.get(root)
            if ref is not None and ref.eh and lc.moved(ref.e):
                m = Moved(ref.c, ref.origin)
                m.array = ref.array
                self.types[root] = (m, "reference " + ref.name())
                continue
            votes, n64 = collections.Counter(), collections.Counter()
            kinds = {}
            for raw, vn, typed in items:
                if not typed:
                    continue
                m = self.view_type(raw.unit, vn)
                if m is not None:
                    k = m.key()
                    votes[k] += 1
                    kinds.setdefault(k, (m, f"{vn} ({raw.unit}, {raw.func})"))
                elif not SCALAR_VIEW.search(vn):
                    n64[vn] += 1   # a struct view that keeps the N64 layout
            if votes:
                k, _ = votes.most_common(1)[0]
                m, how = kinds[k]
                self.types[root] = (m, "view " + how + (f"; also {len(votes) - 1} other moved views" if len(votes) > 1
                                                         else ""))
                continue
            if root[0] == "param":
                fn = root[1] if isinstance(root[1], str) else root[1][0]
                hit = next(((m, why) for fre, k, m, why in roles if k == root[2] and fre.search(fn) and m), None)
                if hit:
                    self.types[root] = (hit[0], "role: " + hit[1])
                    continue
            if n64:
                self.types[root] = (None, "N64-layout views: " + ", ".join(v for v, _ in n64.most_common(3)))
        self.propagate()

    def propagate(self):
        """Types through the calls the flow keeps apart (a helper with more callers than layout_check.Flow.CAP keeps
        its parameter as a root of its own): a storage passed at +0 to a parameter of a type has that type (a
        callback that passes its record to UpdateBlit), and a parameter whose callers' storages all have one type
        has it (a helper taking `void *`)."""
        flow = self.flow
        for _ in range(6):
            new = {}
            for root, lst in self.args.items():
                if root in self.types and self.types[root][0] is not None:
                    continue
                for h, u, fn, ln, callee, k in lst:
                    t = self.types.get(("param", flow.fkey(callee, u), k))
                    if h == 0 and t and t[0] is not None:
                        new[root] = (t[0], f"passed to {callee} at {fn}:{ln} (its argument {k}: {t[1]})")
                        break
            for (fk, k), callers in flow.callers.items():
                root = ("param", fk, k)
                if (root in self.types and self.types[root][0] is not None) or root in new or root not in self.on:
                    continue
                kinds, seen = {}, set()
                for u, ident, h, n in callers:
                    b, dh, dn = flow.key(ident, u)
                    for r2, dh2, dn2 in flow.expand(b):
                        t = self.types.get(r2)
                        seen.add(r2)
                        if t and t[0] is not None and h + dh + dh2 == 0:
                            kinds.setdefault(t[0].key(), (t[0], lc.fmt_root(r2)))
                if len(kinds) == 1 and len(seen) > 0:
                    m, where = next(iter(kinds.values()))
                    new[root] = (m, f"its callers pass {where}" + (f" and {len(seen) - 1} more" if len(seen) > 1 else ""))
            if not new:
                break
            self.types.update(new)

    def findings(self):
        out = []
        for root in sorted(set(self.on) | set(self.rawargs), key=lc.fmt_root):
            m, how = self.types.get(root, (None, None))
            if m is None:
                continue
            for raw, vn, typed in self.on.get(root, ()):
                f = self.judge(m, how, raw, vn if typed else None)
                if f:
                    out.append(f)
            for raw in self.rawargs.get(root, ()):
                f = self.judge(m, how, raw)
                if f:
                    out.append(f)
        return self.review(self.merge(out))

    def judge(self, m, how, raw, view=None):
        """A finding for a raw access on a record of type m, or None. A record known as an array (a symbol's
        reference) takes the N64 stride for an N64 offset past its element; a record known through a view or a
        callee is that one record (a prefix of what the pointer points to), and an offset past it says nothing."""
        src = Source.get(raw.unit, self.macros)
        pc = src.pc_only(raw.line)
        h = raw.h
        if h < 0 or raw.stride or not m.en or not m.eh or not raw.lo:
            return None   # an index (layout_check's strides), or no offset of the function's own
        array = getattr(m, "array", False)
        rw = "write" if raw.rw == "W" else "read"
        shaped = False
        offh = h % m.eh if array else h
        # a typed access: through a view with the PC layout, the view's field (layout_check compares the views);
        # through a struct view that keeps the N64 shape (bytes up to the field: `u8 pad0[0x1A]; s8 dirty;`), its
        # field is the N64's, and the offset is in the declaration, not on the line; through a scalar (an m2c
        # offset from `&sym`), the access must not fit the record's PC field
        shaped = view is not None and not SCALAR_VIEW.search(view) and (raw.unit, view) not in self.moved
        vm = self.moved.get((raw.unit, view)) if view is not None else None
        if view is not None and (pc or raw.kind == "arg" or m.fits_pc(offh, raw.width, raw.kind) or (
                vm is not None and vm.eh and vm.fits_pc(raw.lo % vm.eh, raw.width, raw.kind))):
            return None
        if pc == "pc":   # PC code: a host offset, which must start a field of its width
            if offh >= m.eh:
                return None
            x = m.ref.covering(offh)
            if raw.kind == "arg" or m.fits_pc(offh, raw.width, raw.kind) or x is None or x[6]:
                return None
            return Finding(3, raw.root, raw.func, raw.unit,
                           f"{raw.width}-byte {rw} in PC code at host +0x{h:X}, inside {m.field(offh)} of "
                           f"{m.origin[1]} ({how})", [], [raw], None)
        # N64 code: the function's own offset is an N64 offset, from a base whose N64 position the flow knows where
        # its steps were typed (a caller's &rec->field); else the base is taken as right on the PC, its host
        # offset's field (a raw step that made it is judged where it is written: a caller's argument)
        base = h - raw.lo
        if raw.bn is not None:
            npos = raw.bn + raw.lo
        elif array and m.eh:
            npos = (base // m.eh) * m.en + m.h2n(base % m.eh) + raw.lo
        else:
            npos = m.h2n(base) + raw.lo
        if not array and npos >= m.en:
            return None
        k, off = (npos // m.en, npos % m.en) if array else (0, npos)   # (and an N64 stride)
        want = k * m.eh + m.ref.n2h(off)
        if want == h:
            return None
        x = m.n64_covering(off)
        if x is None or (x[6] and lc.PAD_NAME.search(x[5] or "")) or ec.is_pad(x[5] or ""):
            return None   # padding in the N64 layout
        offh = h % m.eh
        if raw.kind == "arg":   # a pointer to a field: it starts one
            f_pc, f_n64 = offh in m.ref.all_h, off in m.by_n
        else:
            f_pc, f_n64 = m.fits_pc(offh, raw.width, raw.kind), m.fits_n64(off, raw.width, raw.kind)
        if f_pc and (not f_n64 or pc == "macro"):
            return None   # the PC's field at the PC's offset: code written for the host layout (CAR_OFS...)
        score = (5 if raw.rw == "W" or raw.width == 8 else 4) if f_n64 and not f_pc else 3 if not f_n64 else 4
        if raw.lo and not shaped and not src.literal(raw.line, raw.lo):
            score = min(score, 2)   # no such offset in the source: an index or a pointer step the compiler folded
        what = f"a pointer to N64 +0x{off:X} passed as {raw.via}" if raw.kind == "arg" else \
            f"{raw.width}-byte {rw} at N64 +0x{off:X}" + (f" through the N64-shaped view {view}" if shaped else "")
        return Finding(score, raw.root, raw.func, raw.unit,
                       f"{what}: the N64's {x[5]} (PC +0x{want - k * m.eh:X}); "
                       f"on the PC +0x{offh:X} is {m.field(offh)} ({m.origin[1]}, {m.origin[0]}; {how})",
                       [], [raw], None)

    def merge(self, found):
        """One finding per function and record: its accesses listed."""
        by = collections.OrderedDict()
        for f in found:
            key = (f.unit, f.func, lc.fmt_root(f.root))
            if key in by:
                g = by[key]
                by[key] = g._replace(score=max(g.score, f.score), raws=g.raws + f.raws,
                                     lines=g.lines + [f.title])
            else:
                by[key] = f._replace(lines=[f.title])
        return list(by.values())

    def review(self, found):
        out = []
        for f in found:
            why = next((r for ure, fre, r in REVIEWED if re.search(ure, f.unit)
                        and (re.search(fre, f.func) or re.search(fre, lc.fmt_root(f.root)))), None)
            out.append(f._replace(why=why))
        return sorted(out, key=lambda f: (f.why is not None, -f.score, f.unit, f.func))

    def untyped(self):
        """Raw accesses (N64 code) on roots no type reaches: function -> [(root, [Raw])]."""
        out = collections.defaultdict(list)
        for root, items in self.on.items():
            if root in self.types:
                continue
            if root[0] in ("local", "anon"):
                continue   # the function's own locals: strings, buffers
            raws = [raw for raw, vn, typed in items if not typed and raw.h > 0 and not raw.stride]
            if raws and all(r.width == 1 for r in raws) and min(r.h for r in raws) < 8:
                continue   # bytes from the start: a string or a buffer
            raws = [r for r in raws if not Source.get(r.unit, self.macros).pc_only(r.line)]
            if raws:
                out[(raws[0].unit, raws[0].func)].append((root, raws))
        return out


# ---------------------------------------------------------------- 3. report and main

def fmt_raw(r):
    return f"{r.unit}:{r.line} {r.func}: {r.rw} {r.width}B host +0x{r.h:X} via {r.via}"


def report(check, out, untyped_out):
    found = check.findings()
    open_ = [f for f in found if not f.why and f.score > 2]
    lines = []
    w = lines.append
    w("## 1. Raw offsets into moved records")
    w("")
    w("Score 2: the offset is not written on the line (an index, a folded pointer step); listed, not counted.")
    w("")
    for f in found:
        w(f"  [{f.score if not f.why else 'reviewed'}] {f.unit} {f.func} on {lc.fmt_root(f.root)}"
          + (f": {f.why}" if f.why else ""))
        for t in list(dict.fromkeys(f.lines))[:12]:
            w(f"      {t}")
        for r in f.raws[:6]:
            w(f"        {fmt_raw(r)}")
    w("")
    w("## 2. Typed storages")
    w("")
    hist = collections.Counter(how.split(" ")[0] if how else "?" for m, how in check.types.values())
    w("  " + ", ".join(f"{k}: {v}" for k, v in sorted(hist.items())))
    unt = check.untyped()
    nraw = sum(len(rs) for v in unt.values() for _, rs in v)
    summary = (f"{len(check.flow.units)} units; {sum(len(v) for v in check.on.values())} accesses placed on "
               f"{len(check.on)} storages, {sum(1 for m, _ in check.types.values() if m)} holding a moved record; "
               f"open findings {len(open_)} ({sum(1 for f in open_ if f.score >= 4)} scored 4 or 5), reviewed "
               f"{sum(1 for f in found if f.why)}, unconfirmed (score 2) {sum(1 for f in found if not f.why and f.score <= 2)}"
               f"; untyped raw accesses {nraw} in {len(unt)} functions")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("# offset_check report (tools/port/offset_check.py; docs/port/LAYOUT.md 9)\n\n" + summary + "\n\n"
                   + "\n".join(lines) + "\n")
    ul = []
    for (u, fn), roots in sorted(unt.items()):
        ul.append(f"{u} {fn}")
        for root, raws in roots:
            offs = sorted({(r.h, r.width, r.rw) for r in raws})
            ul.append(f"    {lc.fmt_root(root)}: " + " ".join(f"{rw}{wd}@0x{h:X}" for h, wd, rw in offs[:16]))
    untyped_out.write_text("\n".join(ul) + "\n")
    print(summary)
    print(f"report: {ec.rel(out)}; untyped: {ec.rel(untyped_out)}")
    return open_


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("-j", "--jobs", type=int, default=2)
    ap.add_argument("--reuse", action="store_true", help="reuse the dumps in build/pc/endian newer than their source")
    ap.add_argument("--cached", action="store_true", help="reuse the last run's analysis (build/pc/endian/offset.pickle)")
    ap.add_argument("--only", default="", help="comma-separated source paths to analyze")
    ap.add_argument("--func", help="print every access of the functions matching this regex, with its storage's type")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--readelf", default="readelf")
    ap.add_argument("--strict", action="store_true", help="exit 1 when a finding is open (CI)")
    ap.add_argument("--selftest", action="store_true", help="check the detector on synthetic cases")
    a = ap.parse_args()
    build = a.build.resolve()
    if a.selftest:
        return selftest(build, a.readelf)
    pick = build / "endian" / "offset.pickle"
    if a.cached and pick.exists():
        results = pickle.loads(pick.read_bytes())
    else:
        results = lc.collect(build, a.jobs, a.reuse, a.only, a.readelf)
        if not a.only:
            pick.write_bytes(pickle.dumps(results))
    flow = lc.run_analysis(results, build)
    check = Check(flow)
    if a.func:
        for root, items in sorted(check.on.items(), key=lambda kv: lc.fmt_root(kv[0])):
            mine = [(raw, vn, t) for raw, vn, t in items if re.search(a.func, raw.func)]
            if not mine:
                continue
            m, how = check.types.get(root, (None, None))
            print(f"{lc.fmt_root(root)}: {how or 'untyped'}")
            for raw, vn, t in sorted(mine, key=lambda x: (x[0].h, x[0].line)):
                print(f"    {'typed' if t else 'raw  '} {vn:12} {fmt_raw(raw)}")
        return 0
    out = a.out or build / "endian" / "offset.txt"
    open_ = report(check, out, out.with_name(out.stem + "_untyped.txt"))
    return 1 if a.strict and open_ else 0


SELFTEST_C = r"""
typedef unsigned char u8; typedef signed char s8; typedef short s16; typedef int s32; typedef unsigned int u32;
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
/* An invented record with two pointers first: N64 name 0, image 4, x 8, y 0xA, hide 0xC, init 0xD, func 0x10, id 0x14;
 * PC name 0, image 8, x 0x10, y 0x12, hide 0x14, init 0x15, func 0x18, id 0x20. */
typedef struct Rec { void *name, *image; s16 x, y; s8 hide, init; u8 pad[2]; s32 (*func)(struct Rec *); u32 id; } Rec;
/* A record that keeps its N64 layout (u32 window addresses). */
typedef struct Win { u32 name, image; s16 x, y; } Win;
#ifdef TARGET_PC
#define REC_HIDE 0x14
#else
#define REC_HIDE 0xC
#endif
u8 gColor[4];
Rec gRecs[2];
void update(Rec *r) { r->x += r->y; r->hide |= r->init; }   /* many callers: a root of its own (Flow.CAP) */
void win_draw(Win *w) { w->x = w->y; }
void set_y(s16 *p) { *p = 3; }
void vec_inc(s16 *v) { v[1]++; }
s32 cb_hide(void *arg0) {          /* 1. a callback at the N64 offsets: Hide is in Image on the PC */
    if (M2C_FIELD(arg0, s8 *, 0xC) != 1) {
        M2C_FIELD(arg0, s8 *, 0xC) = 1;
        update(arg0);
    }
    return 1;
}
s32 cb_image(void *arg0) {         /* 2. a pointer stored at the N64 +4, over Name's high half */
    M2C_FIELD(arg0, void **, 4) = gColor;
    update(arg0);
    return 1;
}
s32 cb_arg(void *arg0) {           /* 3. a pointer to the N64's y passed on */
    set_y((s16 *)((u8 *)arg0 + 0xA));
    update(arg0);
    return 1;
}
void touch_sym(void) {             /* 4. a symbol declared as the records, m2c code at N64 offsets */
    M2C_FIELD(&gRecs, u32 *, 0x14) = 5;
}
s32 cb_role(void *arg0) {          /* 5. a callback nothing typed reaches: SELFTEST_ROLES gives it a Rec */
    return M2C_FIELD(arg0, u32 *, 0x14);
}
#ifdef TARGET_PC
s32 cb_pc(void *arg0) {            /* PC code at the host offsets: nothing */
    M2C_FIELD(arg0, s8 *, 0x14) = 1;
    M2C_FIELD(arg0, void **, 8) = gColor;
    update(arg0);
    return 1;
}
#endif
s32 cb_macro(void *arg0) {         /* a PC macro: nothing */
    M2C_FIELD(arg0, s8 *, REC_HIDE) = 0;
    update(arg0);
    return 1;
}
s32 cb_vec(Rec *r) {               /* a typed pointer into the record, the helper's own offsets: nothing */
    vec_inc(&r->x);
    update(r);
    return 1;
}
s32 cb_win(void *arg0) {           /* an N64-layout record: nothing */
    M2C_FIELD(arg0, s16 *, 0xA) = 2;
    win_draw(arg0);
    return 1;
}
s32 cb_low(void *arg0) {           /* before the first pointer moves: nothing */
    M2C_FIELD(arg0, u32 *, 0) = 0;
    update(arg0);
    return 1;
}
typedef struct { u8 pad0[0xC]; s8 dirty; } Item;   /* 6. an N64-shaped view: bytes up to the N64's hide */
s32 cb_item(Item *item) {
    if (item->dirty) {
        item->dirty = 0;
        update((Rec *) item);
    }
    return item->dirty;
}
void caller(Rec *a, Rec *b, Rec *c, Rec *d) { update(a); update(b); update(c); update(d); }
"""
SELFTEST_ROLES = [(r"^cb_role$", 0, (r"selftest\.c$", "Rec"), "the synthetic role: a Rec")]
SELFTEST_EXPECT = [
    (r"^cb_hide$", r"1-byte write at N64 \+0xC: the N64's hide \(PC \+0x14\); on the PC \+0xC is image"),
    (r"^cb_hide$", r"1-byte read at N64 \+0xC"),
    (r"^cb_image$", r"8-byte write at N64 \+0x4: the N64's image \(PC \+0x8\); on the PC \+0x4 is name"),
    (r"^cb_arg$", r"a pointer to N64 \+0xA passed as set_y argument 0: the N64's y \(PC \+0x12\)"),
    (r"^touch_sym$", r"4-byte write at N64 \+0x14: the N64's id \(PC \+0x20\)"),
    (r"^cb_role$", r"4-byte read at N64 \+0x14: the N64's id .*role: the synthetic role"),
    (r"^cb_item$", r"1-byte write at N64 \+0xC through the N64-shaped view Item: the N64's hide \(PC \+0x14\)"),
]
SELFTEST_ABSENT = [r"^cb_pc$", r"^cb_macro$", r"^cb_vec$", r"^vec_inc$", r"^cb_win$", r"^cb_low$", r"^update$"]


def selftest(build, readelf):
    """Run the check on SELFTEST_C with SELFTEST_ROLES as ROLES and update() a generic helper (Flow.CAP 3)."""
    global ROLES, REVIEWED
    d = build / "endian" / "offset_selftest"
    d.mkdir(parents=True, exist_ok=True)
    src = d / "selftest.c"
    src.write_text(SELFTEST_C)
    r = subprocess.run(["cc", "-std=gnu89", "-g", "-w", "-fno-strict-aliasing", "-DTARGET_PC", *ec.DUMP_FLAGS,
                        "-c", str(src), "-o", str(d / "selftest.o")], cwd=d, capture_output=True, text=True)
    if r.returncode:
        print(r.stderr)
        return 1
    saved = ROLES, REVIEWED, lc.Flow.CAP
    ROLES, REVIEWED, lc.Flow.CAP = SELFTEST_ROLES, [], 3
    try:
        _, res = lc.analyze_unit(("selftest", d, build, readelf, src))
        flow = lc.Flow({"selftest": res}, lc.Symbols(d / "none"))
        Source.register(res["unit"], (), src)
        found = Check(flow).findings()
    finally:
        ROLES, REVIEWED, lc.Flow.CAP = saved
    text = [(f.func, " | ".join(f.lines)) for f in found if f.score > 2]
    bad = 0
    for func_re, what_re in SELFTEST_EXPECT:
        ok = any(re.search(func_re, fn) and re.search(what_re, t) for fn, t in text)
        bad += not ok
        print(f"{'ok  ' if ok else 'MISS'} {func_re}: {what_re}")
    for func_re in SELFTEST_ABSENT:
        hit = [t for fn, t in text if re.search(func_re, fn)]
        bad += bool(hit)
        print(f"{'ok  ' if not hit else 'FALSE'} nothing in {func_re}" + (f": {hit[0][:160]}" if hit else ""))
    print(f"selftest: {len(found)} findings, {bad} wrong")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
