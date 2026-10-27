#!/usr/bin/env python3
"""Find game code that reaches one storage through two names, which an optimizing compiler takes for two objects.

    python3 tools/port/alias_check.py [--build build/pc] [-j 2] [--reuse]

GCC's alias analysis holds that two different external variables never overlap (for a memory access, the symbol
table's equal_address_to answers "different"). The game C often names one place twice: a symbol and a second,
address-named symbol inside it (gOptions D_80146108 and D_80146125 = &gOptions[29]), two names at one address, or
a view that indexes from one symbol into the next. port/data puts them where the N64 has them, so at -O0 every access
goes to memory in program order and the names agree. Optimized, a store through one name may be taken not to change
what the other reads: after `D_80146108[29] += add; if (D_80146108[29] < 0) D_80146125 = 3;` GCC may go on with the
sum it holds in a register (docs/port/OPTIMIZED.md). The fix is one name on the PC (a TARGET_PC macro).

Method: tools/port/endian_check.py's accesses (GCC's GIMPLE of every live function: base symbol, position, width,
read or write; --reuse keeps its dumps in <build>/endian), and the storage of every name from the data report
(<build>/pc-data/data_symbols.txt and data_layout.json): the RDRAM window for arena names, an own storage's base
and host offset for the rest. An access at a known position covers its bytes; one at a variable index or through a
pointer derived from the symbol covers the symbol's whole extent. Then, per function:

    hazard    two names, accesses whose storage overlaps, at least one a write: the check fails

and per file, because the optimizer inlines within a file (there is no link-time optimization): the same between
functions of one file that may end up in one function, the one a function calls through the file's own non-weak
definitions, transitively (a weak copy is never inlined). Pairs read and judged are in REVIEWED. Exit status 1 if a
function has a hazard, or a file has a pair that may be inlined together and isn't in REVIEWED. The call graph comes
from the source text, so check a new hit by hand. Stdlib only, Python 3.9+.
"""
import argparse
import collections
import json
import multiprocessing
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import endian_check as ec  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

# (source, name, name): pairs between functions of one file, read and judged.
_PLAYER_ROWS = ("func_80397FF4 reads only D_803B5CFC[n][0] with n = gNumPlayers, 1 to 4: 0x803B5D5C + 96 (n - 1), in "
                "the layout table D_803B5D50, which no code writes. Row 0, over this name, is never read "
                "(docs/port/DATA.md 3)")
REVIEWED = {
    ("src/ovl_a/groups/runtime_a_availability_refresh/group.c", "D_803B470C", "D_803B5CFC"): _PLAYER_ROWS,
    **{("src/ovl_a/groups/runtime_a_availability_refresh/group.c", "D_803B5CFC", f"D_803B5D{x}"): _PLAYER_ROWS
       for x in ("0C", "10", "14", "18", "1C", "20", "24", "28")},
    ("src/rom/lib_17dc0.c", "D_80043EB8", "D_800443E0"):
        "func_80019420 stores D_800443E0[index][selector] once, with the index func_80017644 finds by reading the "
        "contexts' identifier and inactiveFC1: the store depends on those reads, and nothing reads after it",
}


def storage(build):
    """name -> (root, start, size): where each placed name's bytes are. Arena names share the root 'window' (their
    N64 addresses); own storage is its own root, with the names inside it at their host offsets."""
    syms = {}
    for line in (build / "pc-data/data_symbols.txt").read_text().splitlines():
        p = line.split("\t")
        if len(p) < 7 or not p[0].startswith("0x"):
            continue
        for n in p[6].split():
            syms[n] = (int(p[0], 16), p[1], int(p[2]), int(p[3]))
    roots = {}
    own_at = collections.defaultdict(list)
    for n, (addr, kind, n64, host) in syms.items():
        if kind.startswith("arena"):
            roots[n] = ("window", addr, max(n64, host))
        elif kind.startswith("own"):
            own_at[addr].append(n)
        elif kind.startswith("C-defined"):
            roots[n] = ("c:" + n, 0, host)
    for addr, names in own_at.items():
        base = sorted(names)[0]   # data.c's storage symbol; the others are aliases of it
        for n in names:
            roots[n] = ("own:" + base, 0, syms[n][3])
    for n, base, off in json.loads((build / "pc-data/data_layout.json").read_text())["sets"]:
        if base in roots and n in syms:
            r = roots[base]
            roots[n] = (r[0], r[1] + off, syms[n][3])
    return roots


def accesses(results, roots):
    """(unit, function) -> [(name, root, lo, hi, rw, line)] of the live code's accesses to placed names."""
    out = collections.defaultdict(list)
    for r in results.values():
        if not r:
            continue
        for x in r["accesses"]:
            a = ec.Access(*x)
            if not (isinstance(a.base, tuple) and a.base[0] == "sym") or a.base[1] not in roots:
                continue
            root, start, size = roots[a.base[1]]
            # A variable index covers the whole extent: endian_check gives the position in one element, with a
            # stride when the element is a scalar, and for a field of a struct element only its GIMPLE shows it.
            var = a.stride or re.search(rf"\b{re.escape(a.base[1])}\[(?!-?\d+\])", a.text)
            if a.pos is not None and a.hpos is not None and not var:
                lo = start + a.hpos
                hi = lo + max(a.width or 1, 1)
            else:
                lo, hi = start, start + max(size, a.width or 1)
            out[(a.unit, a.func)].append((a.base[1], root, lo, hi, a.rw, a.line))
    return out


def pairs(acc):
    """{(name, name): [(line, line)]}: two names whose accesses overlap, one of them a write."""
    by_name = collections.defaultdict(list)
    for e in acc:
        by_name[e[0]].append(e)
    names = sorted(by_name)
    found = collections.defaultdict(set)
    for i, x in enumerate(names):
        for y in names[i + 1:]:
            for a in by_name[x]:
                for b in by_name[y]:
                    if a[1] == b[1] and a[2] < b[3] and b[2] < a[3] and ("W" in a[4] or "W" in b[4]):
                        found[(x, y)].add((a[5], b[5]))
    return found


def call_graph(src):
    """function -> the names it calls, for each function the file defines (from the source text: a definition
    starts at column 0 and ends at a closing brace at column 0)."""
    calls, cur = collections.defaultdict(set), None
    for line in Path(src).read_text(errors="replace").splitlines():
        m = re.match(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;]*$", line)
        if m and m.group(1) not in ("if", "while", "for", "switch", "return", "sizeof"):
            cur = m.group(1)
            calls[cur]
            continue
        if line.startswith("}"):
            cur = None
        elif cur:
            calls[cur] |= set(re.findall(r"\b(\w+)\s*\(", line))
    return calls


def together(calls, weak):
    """function -> the functions that may be inlined into it in this file: what it calls, transitively, among the
    file's own non-weak definitions (a weak copy is never inlined), and itself."""
    defined = {f for f in calls if f not in weak}
    reach = {}
    for f in defined:
        seen, todo = {f}, [f]
        while todo:
            for g in calls.get(todo.pop(), ()):
                if g in defined and g not in seen:
                    seen.add(g)
                    todo.append(g)
        reach[f] = seen
    return reach


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("-j", "--jobs", type=int, default=2)
    ap.add_argument("--reuse", action="store_true", help="reuse the dumps in <build>/endian newer than their source")
    ap.add_argument("--readelf", default="readelf")
    a = ap.parse_args()
    build = a.build.resolve()
    cache = build / "endian"
    cache.mkdir(parents=True, exist_ok=True)
    cmds = ec.game_commands(build)
    with multiprocessing.Pool(a.jobs) as pool:
        for uid, err in pool.imap_unordered(ec.compile_one, [(s, c, w, cache, a.reuse) for s, c, w in cmds],
                                            chunksize=4):
            if err:
                print(f"alias_check: compile failed: {uid}: {err}", file=sys.stderr)
        results = dict(pool.imap_unordered(ec.analyze_unit, [(ec.unit_id(s), cache, build, a.readelf)
                                                             for s, _, _ in cmds], chunksize=4))
    roots = storage(build)
    acc = accesses(results, roots)
    bad = 0
    n_acc = sum(len(v) for v in acc.values())
    print(f"alias_check: {n_acc} accesses to {len({e[0] for v in acc.values() for e in v})} placed names in "
          f"{len(acc)} functions")
    for (unit, func), v in sorted(acc.items()):
        for (x, y), lines in sorted(pairs(v).items()):
            bad += 1
            ls = ", ".join(f"{p}/{q}" for p, q in sorted(lines)[:4])
            print(f"hazard  {unit} {func}: {x} and {y} reach the same bytes (lines {ls})")
    per_unit = collections.defaultdict(list)
    for (unit, func), v in acc.items():
        per_unit[unit] += [e + (func,) for e in v]
    for unit, v in sorted(per_unit.items()):
        found = pairs([e[:6] for e in v])
        if not found:
            continue
        funcs = collections.defaultdict(set)
        for e in v:
            funcs[e[0]].add(e[6])
        reach = together(call_graph(ROOT / unit), ec.weak_names(build, ec.unit_id(str(ROOT / unit))))
        for (x, y) in sorted(found):
            fx, fy = funcs[x], funcs[y]
            if fx & fy and len(fx | fy) == 1:
                continue   # one function: reported above
            # functions into which the file's functions that reach x and y may be inlined, so that what they do
            # (one a write) is one function's
            linked = sorted(g for g, r in reach.items()
                            if (x, y) in pairs([e[:6] for e in v if e[6] in r and e[0] in (x, y)]))
            key = (unit, x, y)
            if key in REVIEWED:
                print(f"file    {unit}: {x} ({', '.join(sorted(fx))}) and {y} ({', '.join(sorted(fy))}): "
                      f"reviewed: {REVIEWED[key]}")
            elif linked:
                bad += 1
                print(f"file    {unit}: {x} ({', '.join(sorted(fx))}) and {y} ({', '.join(sorted(fy))}): "
                      f"may be inlined together into {', '.join(linked[:6])}")
            else:
                print(f"file    {unit}: {x} ({', '.join(sorted(fx))}) and {y} ({', '.join(sorted(fy))}): "
                      f"never in one function with a write, inlined or not")
    print(f"alias_check: {bad} open" if bad else "alias_check: none open")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
