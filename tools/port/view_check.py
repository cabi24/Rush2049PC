#!/usr/bin/env python3
"""Check that every file's view of one storage agrees with the others on the PC layout.

    python3 tools/port/view_check.py --sym D_8012E700 [--also D_8012E708,...]   # a data symbol's views
    python3 tools/port/view_check.py --tag Blit,Voice --n64-size 0x40           # a record passed by pointer

Each game file declares its own struct for shared records, and the N64 build forgives views that disagree on
where a pointer is, since pointers are 4 bytes there. On the PC they must agree on the host layout. This reads
the PC build's DWARF (with tools/port/gen_data.py's parser and layout rules), lays every view out twice (N64 and
host), and compares, member by member at each N64 offset, the host offset and whether it is a pointer with the
reference view (the one with the most pointers, as gen_data chooses). Byte arrays (pads) are not compared.

With --sym, the views are the declarations in the objects whose live code (strong or local functions, from the
relocations) refers to the symbol, to one of its --alias names (same address) or to one of the --also names (inside
it, whose own declarations are not compared); weak context copies never run. With --tag, the
views are the complete struct definitions with one of those tags and at most the given N64 size (shorter ones are
prefixes), in every object; those whose object has no live function are marked. Exit status 1 if a live view disagrees. docs/port/POINTERS.md 9.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_data as gd  # noqa: E402

CLASS = {gd.K_F32: gd.K_U32, gd.K_F64: gd.K_U64, gd.K_S32_64: gd.K_PTR, gd.K_U32_64: gd.K_PTR}


def element_type(t):
    while t[0] == "array":
        t = t[1]
    return t


def _walk(t, n64o, hosto, out):
    """Scalars of t at (N64, host) offsets into out; byte arrays are pads, so they are left out."""
    k = t[0]
    if k in ("base", "ptr", "enum"):
        kind = gd.program(t)[0][0] if gd.program(t) else None
        if kind:
            out.setdefault(n64o, (hosto, CLASS.get(kind, kind)))
    elif k == "array":
        e = t[1]
        if e[0] == "base" and gd.n64_layout(e)[0] == 1:
            return
        n, h = gd.n64_layout(e)[0] or 0, gd.host_size(e) or 0
        for i in range(t[2] or 0):
            _walk(e, n64o + i * n, hosto + i * h, out)
    elif k == "struct":
        off = 0
        for _, hoff, mt in t[3]:
            s, a = gd.n64_layout(mt)
            off = gd.align(off, a)
            _walk(mt, n64o + off, hosto + hoff, out)
            off += s or 0
    elif k == "union":
        for _, _, mt in t[3]:
            _walk(mt, n64o, hosto, out)


def layout(t):
    """{N64 offset: (host offset, class)} of the scalar members of struct t (not byte arrays), and its (N64, host)
    size."""
    out = {}
    _walk(t, 0, 0, out)
    return out, (gd.n64_layout(t)[0], gd.host_size(t))


def live_users(objects, names):
    """Objects whose strong or local functions refer to one of `names`."""
    users = set()
    for obj in objects:
        rel = subprocess.run(["objdump", "-r", "-j", ".text", obj], capture_output=True, text=True).stdout
        offs = []
        for line in rel.splitlines():
            p = line.split()
            if len(p) >= 3 and re.match(r"^[0-9a-f]+$", p[0]) and re.split(r"[+-]", p[2])[0] in names:
                offs.append(int(p[0], 16))
        if not offs:
            continue
        nm = subprocess.run(["nm", "--defined-only", "-S", obj], capture_output=True, text=True).stdout
        for line in nm.splitlines():
            p = line.split()
            if len(p) == 4 and p[2] in "Tt":
                a, s = int(p[0], 16), int(p[1], 16)
                if any(a <= o < a + s for o in offs):
                    users.add(obj)
                    break
    return users


def has_live_code(obj):
    nm = subprocess.run(["nm", "--defined-only", obj], capture_output=True, text=True).stdout
    return any(len(p) == 3 and p[1] in "Tt" for p in (l.split() for l in nm.splitlines()))


def read_dies(objects, readelf):
    for i in range(0, len(objects), 100):
        batch = objects[i:i + 100]
        out = subprocess.run([readelf, "--debug-dump=info", *batch], capture_output=True, text=True,
                             check=True).stdout
        yield from gd.parse_readelf(out, batch[0] if len(batch) == 1 else None).items()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", type=Path, default=Path("build/pc"))
    ap.add_argument("--sym", help="data symbol whose views to compare")
    ap.add_argument("--also", default="", help="names inside the symbol's extent that live code may use instead")
    ap.add_argument("--alias", default="", help="other names of the symbol's address, whose declarations are views too")
    ap.add_argument("--tag", help="struct tags (comma-separated) of a record passed by pointer")
    ap.add_argument("--n64-size", type=lambda s: int(s, 0), help="N64 size of the --tag record")
    ap.add_argument("--readelf", default="readelf")
    a = ap.parse_args()
    if bool(a.sym) == bool(a.tag) or (a.tag and not a.n64_size):
        ap.error("give --sym, or --tag with --n64-size")
    objects = [l.strip() for l in (a.build / "game_objects.txt").read_text().split() if l.strip()]

    views = []   # (object, display, type, live)
    if a.sym:
        same = {a.sym} | {n for n in a.alias.split(",") if n}
        users = live_users(objects, same | {n for n in a.also.split(",") if n})
        found, tags = gd.read_variables(sorted(users), a.readelf, same)
        for name in sorted(same):
            for obj, t, disp, _ in found.get(name, ()):
                t = element_type(gd.complete(t, tags))
                if t[0] in ("struct", "union"):
                    views.append((obj, disp if name == a.sym else f"{name}: {disp}", t, True))
        missing = users - {v[0] for v in views}
    else:
        want = set(a.tag.split(","))
        missing = set()
        for obj, dies in read_dies(objects, a.readelf):
            memo, seen = {}, set()
            for off, d in dies.items():   # struct tags, and typedef names of (anonymous) structs
                if d.tag not in ("DW_TAG_structure_type", "DW_TAG_typedef") or "DW_AT_declaration" in d.attrs:
                    continue
                name = gd._name(d.attrs.get("DW_AT_name", ""))
                if name not in want:
                    continue
                t = gd.canon(dies, off, memo)
                if t[0] == "struct" and (gd.n64_layout(t)[0] or 0) <= a.n64_size and t not in seen:
                    seen.add(t)
                    views.append((obj, name, t, None))
        live = {o: has_live_code(o) for o in {v[0] for v in views}}
        views = [(o, d, t, live[o]) for o, d, t, _ in views]

    if not views:
        print("no views found")
        return 1
    lays = [(v, layout(v[2])) for v in views]
    ref_v, (ref, ref_size) = max(lays, key=lambda x: (sum(1 for h, c in x[1][0].values() if c == gd.K_PTR),
                                                      len(x[1][0]), x[0][3] is not False))
    rel = lambda o: o.split("game.dir/")[-1].replace("./", "").replace(".c.o", ".c")
    print(f"reference: {rel(ref_v[0])} ({ref_v[1]}): N64 {ref_size[0]} B, host {ref_size[1]} B")
    bad = 0
    for v, (lay, size) in sorted(lays, key=lambda x: rel(x[0][0])):
        problems = []
        if size != ref_size and not (a.tag and size[0] < ref_size[0]):   # a shorter --tag view is a prefix
            problems.append(f"size N64 {size[0]} / host {size[1]}")
        for n64o, (h, c) in sorted(lay.items()):
            if n64o in ref:
                rh, rc = ref[n64o]
            else:   # bytes in the reference (u8 prim[4] against a u32 colour): compare the place only
                rh, rc = gd.map_offset(gd.program(ref_v[2]), n64o), c
            if rh != h or (rc == gd.K_PTR) != (c == gd.K_PTR):   # u8[4] against a u32 colour is the same place
                problems.append(f"N64 +0x{n64o:X}: host +0x{h:X} {gd.KIND_NAMES[c]}, reference +0x{rh:X} "
                                f"{gd.KIND_NAMES[rc]}")
        tag = "" if v[3] is not False else " (no live code)"
        if problems:
            if v[3] is not False:
                bad += 1
            print(f"DIFF {rel(v[0])} ({v[1]}){tag}: " + "; ".join(problems[:6])
                  + (f" (+{len(problems) - 6})" if len(problems) > 6 else ""))
        else:
            print(f"ok   {rel(v[0])} ({v[1]}){tag}")
    for obj in sorted(missing):
        print(f"?    {rel(obj)}: no struct view in its DWARF (integer or byte declaration)")
    print(f"{len(views)} views, {bad} live ones disagree" + (f", {len(missing)} without a struct view" if missing else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
