#!/usr/bin/env python3
"""Check a run's scene line against the draw distance and detail settings (docs/port/DETAIL.md 6).

    python3 tools/port/detail_check.py LOG --level original|far|max [--detail original|high] [--world N]
        [--lod-cut N] [--range N] [--radius N] [--coarse N] [--all-parts]

The exit's line `rush2049pc: scene: ...` (port/config/detail.c) gives each count's mean and most over the views of
a world. The synthetic cartridge's detail world (tools/port/synth_rom.py, DETAIL_PARTS) is made so each setting lets
a known number of its six parts through: four at original, five at far, all six at max. Each option checks a count's
most a view: --world the parts the visibility tables let through, --lod-cut the nodes past their last level of
detail, --range those out of the matrices' range, --radius those past the fog's radius, --coarse those drawn at a
level past their first; --all-parts that every part of the world was let through. Exit 1 on a mismatch.
"""
import argparse
import re
import sys

NAMES = {
    "drawn": "nodes drawn",
    "coarse": "at a level of detail past their first",
    "parts": "world parts",
    "world": "let through by the visibility tables",
    "hidden": "hidden",
    "view": "hidden in that view",
    "range": "out of the matrices' range",
    "radius": "past the fog's radius",
    "lod": "past their last level of detail",
}


def parse(text):
    m = None
    for line in text.splitlines():
        if line.startswith("rush2049pc: scene: "):
            m = line
    if m is None:
        return None
    head = re.match(r"rush2049pc: scene: draw_distance (\w+), detail (\w+); (\d+) views of a world \(of (\d+) drawn\), "
                    r"a view's mean and most:(.*); (\d+) graphics tasks, triangles ([\d.]+) a task, at most (\d+)$", m)
    if not head:
        raise SystemExit(f"detail_check: the scene line doesn't read: {m}")
    out = {"level": head.group(1), "detail": head.group(2), "views": int(head.group(3)), "walks": int(head.group(4)),
           "tasks": int(head.group(6)), "tris": float(head.group(7)), "tris_most": int(head.group(8))}
    for part in head.group(5).split(";"):
        part = part.strip()
        k = re.match(r"(.*) ([\d.]+), (\d+)$", part)
        if not k:
            raise SystemExit(f"detail_check: a count doesn't read: {part}")
        for key, name in NAMES.items():
            if k.group(1) == name:
                out[key] = (float(k.group(2)), int(k.group(3)))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("log")
    ap.add_argument("--level", required=True, choices=("original", "far", "max"))
    ap.add_argument("--detail", default="original", choices=("original", "high"))
    for k in ("world", "lod-cut", "range", "radius", "coarse"):
        ap.add_argument("--" + k, type=int)
    ap.add_argument("--all-parts", action="store_true")
    a = ap.parse_args()
    s = parse(open(a.log, errors="replace").read())
    if s is None:
        print(f"detail_check: no scene line in {a.log}")
        return 1
    bad = []
    if s["level"] != a.level or s["detail"] != a.detail:
        bad.append(f"settings {s['level']}, {s['detail']}; expected {a.level}, {a.detail}")
    if s["views"] == 0:
        bad.append("no view of a world was drawn")
    for opt, key in (("world", "world"), ("lod_cut", "lod"), ("range", "range"), ("radius", "radius"),
                     ("coarse", "coarse")):
        want = getattr(a, opt)
        if want is not None and s[key][1] != want:
            bad.append(f"{NAMES[key]}: at most {s[key][1]} a view, expected {want}")
    if a.all_parts and s["world"][1] != s["parts"][1]:
        bad.append(f"{s['world'][1]} of the world's {s['parts'][1]} parts let through, expected all")
    print(f"detail_check: {a.log}: {s['level']}, detail {s['detail']}: {s['views']} views of a world; parts let "
          f"through {s['world'][1]} of {s['parts'][1]}, nodes drawn {s['drawn'][0]:.1f} (most {s['drawn'][1]}), "
          f"past their last level {s['lod'][1]}, out of range {s['range'][1]}, past the fog's radius {s['radius'][1]}, "
          f"at a coarser level {s['coarse'][1]}; triangles {s['tris']:.1f} a task (most {s['tris_most']})")
    for b in bad:
        print("detail_check: FAIL: " + b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
