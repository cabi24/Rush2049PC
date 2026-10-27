#!/usr/bin/env python3
"""Check that every file's view of a record agrees with one host layout (DWARF of the PC build's objects).

    python3 tools/port/view_audit.py --build build/pc --symbols D_80152818,player_array --ref func_8038C910.c
    python3 tools/port/view_audit.py --build build/pc --symbols D_8014A250 --size 0x808 --ref world_physics_tick.c

A record the game reaches through many struct declarations (the car array, the model array) needs one layout on
the PC: each view's named fields must sit at the host offsets the storage has. The views are the element types
of every declaration of --symbols (gen_data's input), and with --size every other struct whose N64 size is that
(views reached through pointers). The reference view (--ref, a substring of its object's path) gives the layout:
each N64 offset maps to a host offset through its field program, as gen_data maps it (padding linearly).

For every other view it reports:
  size     the element's host size differs from the reference's
  field    a named field whose host offset is not where the reference maps its N64 offset
  pointer  the reference has a pointer where the view has another kind (gen_data lists these as conflicts), or the
           view has a pointer where the reference has none (intptr_t and uintptr_t count as pointers)
Fields whose names start with pad/unk_pad/gap/opaque/prefix/tail/reserved/fill are padding and only checked for
pointers. --lines prints each member's source line, for editing. Exit status 1 if any view disagrees.
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

PAD = re.compile(r"^_*(pad|unk_pad|gap|opaque|prefix|tail|reserved|fill|skip|other|rest|unknown|data_\d|u8pad)",
                 re.I)


def leaves(t, n64o, hosto, path, out):
    """(n64 offset, host offset, path, kind, n64 size, is_pad) of every leaf of a canonical type."""
    k = t[0]
    if k == "struct":
        off = 0
        for name, ho, mt in t[3]:
            s, a = g.n64_layout(mt)
            off = g.align(off, a)
            if PAD.match(name or "") and mt[0] == "array" and mt[1][0] == "base":
                out.append((n64o + off, hosto + ho, path + "." + name, "pad", s or 0, True))
            else:
                leaves(mt, n64o + off, hosto + ho, path + "." + name, out)
            off += s or 0
    elif k == "union":
        for name, ho, mt in t[3]:
            leaves(mt, n64o, hosto + ho, path + "." + name, out)
    elif k == "array":
        s = g.n64_layout(t[1])[0] or 0
        h = g.host_size(t[1]) or 0
        if t[1][0] == "base" and t[2]:
            for i in range(t[2]):
                leaves(t[1], n64o + i * s, hosto + i * h, path + f"[{i}]", out)
        else:
            for i in range(t[2] or 0):
                leaves(t[1], n64o + i * s, hosto + i * h, path + f"[{i}]", out)
    elif k == "ptr":
        out.append((n64o, hosto, path, "ptr", 4, False))
    elif k == "base" and t[1] in ("long int", "long unsigned int") and t[2] == g.HOST_PTR:
        out.append((n64o, hosto, path, "ptr", 4, False))      # intptr_t/uintptr_t: pointer-sized on the host
    elif k in ("base", "enum"):
        out.append((n64o, hosto, path, t[1] if k == "base" else f"enum{t[1]}", g.n64_layout(t)[0] or 0, False))


def member_lines(dies):
    """struct DIE offset -> {member name: decl line}."""
    out = {}
    for off, d in dies.items():
        if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type"):
            out[off] = {g._name(m.attrs.get("DW_AT_name", "")): g._num(m.attrs.get("DW_AT_decl_line", "0"))
                        for m in d.children if m.tag == "DW_TAG_member"}
    return out


def strip(t):
    while t[0] == "array":
        t = t[1]
    return t


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=Path("build/pc"))
    ap.add_argument("--symbols", required=True, help="comma-separated names of the record array")
    ap.add_argument("--size", type=lambda x: {int(v, 0) for v in x.split(",")},
                    help="also every struct of this N64 size (comma-separated sizes)")
    ap.add_argument("--tags", help="with --size, only structs with these tags (comma-separated)")
    ap.add_argument("--ref", required=True, help="substring of the reference view's object path")
    ap.add_argument("--lines", action="store_true", help="print the source line of each disagreeing member")
    ap.add_argument("--readelf", default="readelf")
    a = ap.parse_args()
    objects = (a.build / "game_objects.txt").read_text().split()
    wanted = set(a.symbols.split(","))
    tags = set(a.tags.split(",")) if a.tags else None
    views = collections.defaultdict(dict)     # object -> {type: (how, lines)}
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([a.readelf, "--debug-dump=info", *batch], capture_output=True, text=True,
                             check=True).stdout
        for obj, dies in g.parse_readelf(out, batch[0] if len(batch) == 1 else None).items():
            memo = {}
            lines = member_lines(dies)
            by_type = {}
            for off, d in dies.items():
                if d.tag in ("DW_TAG_structure_type", "DW_TAG_union_type") and "DW_AT_declaration" not in d.attrs:
                    by_type.setdefault(g.canon(dies, off, memo), lines.get(off, {}))
            for off, d in dies.items():
                if d.tag == "DW_TAG_variable":
                    spec = dies.get(g._ref(d, "DW_AT_specification")) if "DW_AT_specification" in d.attrs else None
                    src = spec or d
                    if g._name(src.attrs.get("DW_AT_name", "")) in wanted:
                        toff = g._ref(d) if "DW_AT_type" in d.attrs else g._ref(src)
                        t = strip(g.canon(dies, toff, memo))
                        if t[0] in ("struct", "union"):
                            views[obj][t] = ("decl", by_type.get(t, {}))
                elif a.size and d.tag == "DW_TAG_structure_type" and "DW_AT_declaration" not in d.attrs:
                    t = g.canon(dies, off, memo)
                    if g.n64_layout(t)[0] in a.size and t not in views[obj] and (tags is None or t[1] in tags):
                        views[obj][t] = ("size", lines.get(off, {}))
    refs = [(o, t) for o in views for t in views[o] if a.ref in o]
    if not refs:
        sys.exit(f"view_audit: no view in an object matching {a.ref!r}")
    ref = refs[0][1]
    runs = g.program(ref)
    ref_size = g.host_size(ref)
    ref_leaves = []
    leaves(ref, 0, 0, "", ref_leaves)
    ref_ptr = {o for o, _, _, k, _, _ in ref_leaves if k == "ptr"}
    ref_n64 = g.n64_layout(ref)[0]
    print(f"reference: {refs[0][0].split('/./')[-1]} {ref[1]}: N64 {ref_n64} B, host {ref_size} B, "
          f"{len(ref_ptr)} pointers")
    bad = 0
    total = 0
    for obj in sorted(views):
        for t, (how, lines) in views[obj].items():
            total += 1
            rel = obj.split("/./")[-1].replace(".c.o", ".c")
            issues = []
            hs = g.host_size(t)
            ns = g.n64_layout(t)[0]
            if ns == ref_n64 and hs != ref_size:
                issues.append(f"size {hs} (reference {ref_size})")
            lv = []
            leaves(t, 0, 0, "", lv)
            for n64o, hosto, path, kind, sz, pad in lv:
                if n64o >= ref_n64:
                    continue
                top = path.split(".")[1].split("[")[0] if "." in path else path
                where = f" (line {lines.get(top)})" if a.lines and lines.get(top) else ""
                if kind == "ptr" and n64o not in ref_ptr:
                    issues.append(f"pointer {path} at N64 0x{n64o:X}: the reference has none{where}")
                cover = sorted(p for p in ref_ptr if n64o <= p < n64o + max(sz, 1))
                if cover and kind != "ptr":
                    issues.append(f"{'pad' if pad else 'field'} {path} ({kind}) at N64 0x{n64o:X} covers the "
                                  f"reference's pointer at 0x{cover[0]:X}{where}")
                    continue
                if pad:
                    continue
                want = g.map_offset(runs, n64o)
                if want != hosto:
                    issues.append(f"field {path} N64 0x{n64o:X} at host 0x{hosto:X}, reference 0x{want:X}{where}")
            if issues:
                bad += 1
                print(f"{rel}: {t[1] or '?'} ({how}, N64 {ns}, host {hs}): {len(issues)}")
                for x in issues[:12]:
                    print(f"    {x}")
                if len(issues) > 12:
                    print(f"    ... {len(issues) - 12} more")
    print(f"view_audit: {total} views, {bad} disagree with the reference")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
