#!/usr/bin/env python3
"""Give every view of a record the reference view's pointers on the PC, by splitting its padding (N64-neutral).

    python3 tools/port/view_pads.py --build build/pc --symbols D_80152818,player_array --size 952 \
        --ref func_8038C910.c [--only FILE...] [--apply]

Many files view one record through their own struct, with padding (`u8 pad0F0[0x35B - 0xF0]`) over the parts
they don't use. Where the record holds pointers, the PC layout has them 8 bytes wide, so a view whose padding
covers them is too short on the PC and every field after it is at the wrong host offset (view_audit.py finds
them). This tool rewrites such padding, on the PC only, into the same bytes with the reference view's pointers
in place, so the compiler gives every view the reference's host layout:

    u8 pad0F0[0x35B - 0xF0];
becomes
    #ifdef TARGET_PC
        u8 pad0F0[0x110 - 0xF0]; struct { void *p0; u8 b4[0x10]; void *p14; } pad0F0_110[21]; u8 pad0F0_308[...];
    #else
        u8 pad0F0[0x35B - 0xF0];
    #endif

(a periodic run of pointers, such as an array of records inside the record, becomes an anonymous struct
array). The original line stays as the #else branch, so the N64 build sees the same tokens on the same lines
(n64_neutral.py). A line already inside an `#ifdef TARGET_PC` branch is edited in place. Views come from the
build's DWARF as in view_audit.py: the element types of --symbols' declarations, and with --size every struct
of that N64 size. Padding is a member whose name starts as view_audit.PAD says, an array of a base type.

Not changed, and listed for a hand edit: a named field over a reference pointer, a padding member that starts
or ends inside one, a declaration that declares more than one member, and a line it can't find the member on.
Without --apply it only prints what it would do. Rerun the build and view_audit.py afterwards.
Stdlib and binutils, Python 3.9+.
"""
import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_data as g  # noqa: E402
import view_audit as va  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def ref_pointers(t):
    """Pointer N64 offsets of a type, and its periodic families [(start, stride, rep, [relative offsets])]."""
    single, fam = [], collections.defaultdict(set)
    for kind, count, n64o, _, rep, ns, _ in g.program(t):
        if kind != g.K_PTR:
            continue
        offs = [n64o + j * 4 for j in range(count)]
        if rep > 1:
            for o in offs:
                fam[(ns, rep)].add(o)
        else:
            single += offs
    families = []
    for (ns, rep), offs in fam.items():
        s0 = min(offs)
        families.append((s0, ns, rep, sorted(o - s0 for o in offs)))
    return sorted(single), sorted(families)


def all_pointers(single, families):
    out = set(single)
    for s0, ns, rep, rel in families:
        for i in range(rep):
            out |= {s0 + i * ns + r for r in rel}
    return sorted(out)


def hx(n):
    return f"0x{n:X}"


def element_struct(rel, ns, byte_t):
    parts, pos = [], 0
    for r in rel:
        if r > pos:
            parts.append(f"{byte_t} b{pos:X}[{hx(r - pos)}];")
        parts.append(f"void *p{r:X};")
        pos = r + 4
    if pos < ns:
        parts.append(f"{byte_t} b{pos:X}[{hx(ns - pos)}];")
    return "struct { " + " ".join(parts) + " }"


def split(name, elem_t, esz, a, b, single, families, byte_t, names=None):
    """PC members for padding `name` over N64 [a, b), or None if a pointer straddles an end. `names` maps the
    N64 offset a block or pointer starts at to (member name, element type or None)."""
    names = names or {}
    ptrs = all_pointers(single, families)
    if any(p < a < p + 4 or p < b < p + 4 for p in ptrs):
        return None
    blocks = []          # (start, end, member text with @N@ for its name)
    for s0, ns, rep, rel in families:
        i0 = max(0, -(-(a - s0) // ns))
        i1 = min(rep, (b - s0) // ns)
        if i1 > i0:
            st = s0 + i0 * ns
            et = names.get(s0, (None, None))[1] or element_struct(rel, ns, byte_t)   # the family's type
            blocks.append((st, s0 + i1 * ns, f"{et} @N@[{i1 - i0}];"))
    covered = [(s, e) for s, e, _ in blocks]
    singles = [p for p in ptrs if a <= p < b and not any(s <= p < e for s, e in covered)]
    items = sorted([(s, e, txt) for s, e, txt in blocks] + [(p, p + 4, "void *@N@;") for p in singles])
    out, pos, first = [], a, True

    def gap(lo, hi):
        nonlocal first
        if hi <= lo:
            return
        t, n = (elem_t, (hi - lo) // esz) if (hi - lo) % esz == 0 and lo % esz == 0 else (byte_t, hi - lo)
        nm = name if first else f"{name}_{lo:X}"
        first = False
        out.append(f"{t} {nm}[{hx(hi) + ' - ' + hx(lo) if t == byte_t or esz == 1 else n}];")

    for s, e, txt in items:
        gap(pos, s)
        nm = names[s][0] if s in names else f"{name}_{s:X}" if not first or s > a else name
        out.append(txt.replace("@N@", nm))
        first = False
        pos = e
    gap(pos, b)
    return " ".join(out)


def inside_pc_branch(lines, i):
    """Whether line i (0-based) is in the TARGET_PC branch of an #ifdef/#if defined(TARGET_PC)."""
    stack = []
    for j in range(i):
        s = lines[j].strip()
        if re.match(r"#\s*if", s):
            stack.append("pc" if re.match(r"#\s*(ifdef\s+TARGET_PC|if\s+defined\s*\(?\s*TARGET_PC)", s) else
                         "npc" if re.match(r"#\s*ifndef\s+TARGET_PC", s) else "other")
        elif re.match(r"#\s*else", s) and stack:
            stack[-1] = {"pc": "npc", "npc": "pc"}.get(stack[-1], stack[-1])
        elif re.match(r"#\s*endif", s) and stack:
            stack.pop()
    return "pc" in stack, "npc" in stack


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=Path("build/pc"))
    ap.add_argument("--symbols", required=True)
    ap.add_argument("--size", type=lambda x: {int(v, 0) for v in x.split(",")},
                    help="also every struct of this N64 size (comma-separated sizes: partial views)")
    ap.add_argument("--tags", help="with --size, only structs with these tags")
    ap.add_argument("--ref", required=True)
    ap.add_argument("--only", nargs="*", help="source files to edit (default: all views)")
    ap.add_argument("--name", action="append", default=[],
                    help="OFF=NAME[:TYPE]: the PC member for the pointer or periodic block at N64 offset OFF")
    ap.add_argument("--pad", help="also treat base-type arrays whose names match this regex as padding (check that "
                                  "no code reads them)")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--readelf", default="readelf")
    a = ap.parse_args()
    objects = g.n64syms.types_objects(a.build)   # -O0 DWARF, whatever the build type
    wanted = set(a.symbols.split(","))
    tags = set(a.tags.split(",")) if a.tags else None
    views = collections.defaultdict(dict)   # source -> {type: {member: line}}
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([a.readelf, "--debug-dump=info", *batch], capture_output=True, text=True,
                             check=True).stdout
        for obj, dies in g.parse_readelf(out, batch[0] if len(batch) == 1 else None).items():
            memo = {}
            lines = va.member_lines(dies)
            by_type = {}
            for off, d in dies.items():
                if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and "DW_AT_declaration" not in d.attrs:
                    by_type.setdefault(g.canon(dies, off, memo), lines.get(off, {}))
            src = obj.split("/./", 1)[-1][:-2]
            for off, d in dies.items():
                if d.tag == "DW_TAG_variable":
                    spec = dies.get(g._ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
                    s = spec or d
                    if g._name(s.attrs.get("DW_AT_name", "")) in wanted:
                        t = va.strip(g.canon(dies, g._ref(d) if "DW_AT_type" in d.attrs else g._ref(s), memo))
                        if t[0] == "struct":
                            views[src][t] = by_type.get(t, {})
                elif a.size and d.tag == "DW_TAG_structure_type" and "DW_AT_declaration" not in d.attrs:
                    t = g.canon(dies, off, memo)
                    if g.n64_layout(t)[0] in a.size and (tags is None or t[1] in tags):
                        views[src].setdefault(t, lines.get(off, {}))
    names = {}
    for x in a.name:
        off, _, rest = x.partition("=")
        nm, _, ty = rest.partition(":")
        names[int(off, 0)] = (nm, ty or None)
    ref = next((t for s in views for t in views[s] if a.ref in s), None)
    if ref is None:
        sys.exit(f"view_pads: no view in a file matching {a.ref!r}")
    single, families = ref_pointers(ref)
    ptrs = all_pointers(single, families)
    manual, edits = [], collections.defaultdict(list)    # source -> [(line, member, new text)]
    for src in sorted(views):
        if a.only and src not in a.only:
            continue
        for t, lines in views[src].items():
            off = 0
            for name, _, mt in t[3]:
                sz, al = g.n64_layout(mt)
                off = g.align(off, al)
                lo, hi = off, off + (sz or 0)
                off = hi
                cover = [p for p in ptrs if lo < p + 4 and p < hi]
                if not cover:
                    continue
                if mt[0] == "ptr" and lo in ptrs and sz == 4:
                    continue
                is_pad = (va.PAD.match(name or "") or (a.pad and re.fullmatch(a.pad, name or ""))) \
                    and mt[0] == "array" and mt[1][0] == "base"
                where = f"{src}:{lines.get(name, '?')} {t[1] or '?'}.{name} [{hx(lo)}, {hx(hi)})"
                if not is_pad:
                    if mt[0] in ("struct", "union", "array") and set(ref_pointers(mt)[0]) | set(
                            all_pointers(*ref_pointers(mt))) == {p - lo for p in cover}:
                        continue    # a nested record with the same pointers
                    manual.append(f"{where}: named field over pointer(s) {', '.join(hx(p) for p in cover[:4])}")
                    continue
                el = mt[1]
                esz = g.n64_layout(el)[0] or 1
                if split(name, "T", esz, lo, hi, single, families, "u8", names) is None:
                    manual.append(f"{where}: a pointer straddles its end")
                    continue
                edits[src].append((lines.get(name), name, (esz, lo, hi)))
    for src, items in sorted(edits.items()):
        path = ROOT / src
        text = path.read_text()
        lines = text.split("\n")
        by_line = collections.defaultdict(list)
        for ln, name, span in items:
            by_line[ln].append((name, span))
        done = []
        for ln in sorted((x for x in by_line if x), reverse=True):
            i = ln - 1
            line = lines[i]
            pc_line = line
            ok = True
            for name, (esz, lo, hi) in by_line[ln]:
                m = re.search(r"((?:(?:unsigned|signed|const|volatile|struct)\s+)*\w+)\s+" + re.escape(name)
                              + r"\s*(\[[^\]]*\])\s*;", pc_line)
                if not m:
                    manual.append(f"{src}:{ln}: member {name} not found on its line")
                    ok = False
                    break
                # pieces keep the member's element type where they divide evenly, else they are bytes
                byte_t = "u8" if re.search(r"\bu8\b", text) else "unsigned char"
                new = split(name, m.group(1), esz, lo, hi, single, families, byte_t, names)
                pc_line = pc_line[:m.start()] + new + pc_line[m.end():]
            if not ok:
                continue
            in_pc, in_n64 = inside_pc_branch(lines, i)
            if in_n64:
                manual.append(f"{src}:{ln}: member is in a non-PC branch")
                continue
            if in_pc:
                lines[i] = pc_line
            else:
                lines[i:i + 1] = ["#ifdef TARGET_PC", pc_line, "#else", line, "#endif"]
            done.append(ln)
        if not done:
            continue
        print(f"{src}: {len(done)} line(s)")
        if a.apply:
            path.write_text("\n".join(lines))
    for m in manual:
        print(f"manual: {m}")
    for ln_missing in [(s, n) for s, it in edits.items() for (ln, n, _) in it if not ln]:
        print(f"manual: {ln_missing[0]}: no line for {ln_missing[1]}")


if __name__ == "__main__":
    main()
