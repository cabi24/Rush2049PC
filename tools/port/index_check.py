#!/usr/bin/env python3
"""Find initialized game data that code indexes past its declaration (docs/port/SYNTH.md 3, port bug 8).

    python3 tools/port/index_check.py [--build build/pc]

The data loader (port/data) converts each symbol's bytes by the element kinds of its declaration, over the extent
the declarations and port/data/extents.txt give it, and copies the rest of the image as it is. m2c's code often
declares a table as one scalar and indexes it, `(&D_80110DD8)[type]` or `((T *) &D_x)[i]`: on the PC the first
element is converted and the others keep the ROM's byte order. This lists every symbol of an image's initialized
data that the C indexes that way whose extent in the build's data report (pc-data/data_symbols.txt) is one element
while bytes no symbol covers follow it: those are the elements that would read back byte-swapped. Exit status 1 if
there is one not in REVIEWED.
"""
import argparse
import bisect
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import n64syms  # noqa: E402

ROOT = n64syms.ROOT
INDEXED = re.compile(r"\(\(?[A-Za-z0-9_ ]*\*?\)? *&(D_[0-9A-F]{8})\)\s*\[|\(\s*&(D_[0-9A-F]{8})\s*\+"
                     # m2c's byte offsets: (((s8 *) (&D_x)) + (i * 0x18 + ...)), ((T *) (&D_x))[i]
                     r"|\(\s*\([A-Za-z0-9_ ]*\*\s*\)\s*\(\s*&(D_[0-9A-F]{8})\s*\)\s*\)\s*[+\[]")
WIDTH = {"u8": 1, "u16": 2, "u32": 4, "f32": 4, "f64": 8, "u64": 8, "ptr": 4}
# Symbols checked by hand that need nothing: name -> why.
REVIEWED = {
    "D_80110EC0": "+4 of the gear ratios D_80110EBC, f32[8][6] (port/data/extents.txt); the gear is at most 4 "
                  "(func_800E313C), so the last element read is +184, and the 4 bytes after the table are not read "
                  "through it",
}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--build", default=str(ROOT / "build/pc"))
    args = ap.parse_args()
    report = Path(args.build) / "pc-data/data_symbols.txt"
    rows = []
    for line in report.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split("\t")
        if len(f) < 7:
            continue
        rows.append((int(f[0], 16), f[1], int(f[2]), f[4].split(","), f[5], f[6].split()))
    rows.sort()
    starts = [r[0] for r in rows]
    by_name = {n: r for r in rows for n in r[5]}

    users = {}
    for path in sorted(list((ROOT / "src").rglob("*.c")) + list((ROOT / "nonmatching").rglob("*.c"))):
        text = path.read_text(errors="replace")
        for m in INDEXED.finditer(text):
            users.setdefault(m.group(1) or m.group(2) or m.group(3), set()).add(str(path.relative_to(ROOT)))

    found = []
    for name, files in sorted(users.items()):
        addr = int(name[2:], 16)
        init = next(((lo, hi) for _, lo, hi, _ in n64syms.ARENAS if lo <= addr < hi), None)
        if init is None or name not in by_name:
            continue                  # BSS (written at run time) or nothing the loader converts
        a, storage, size, kinds, decl, _ = by_name[name]
        elem = max(WIDTH.get(k, 1) for k in kinds)
        if size > elem or a != addr or storage.startswith("in "):
            continue                  # a table already, or inside one
        j = bisect.bisect_right(starts, addr)
        nxt = starts[j] if j < len(starts) else init[1]
        covered = max(r[0] + r[2] for r in rows[:j])   # a table that starts before it may run past it
        uncovered = min(nxt, init[1]) - max(addr + size, covered)
        if uncovered <= 0:
            continue
        found.append((name, decl, size, uncovered, sorted(files)))

    bad = 0
    for name, decl, size, uncovered, files in found:
        tag = "reviewed" if name in REVIEWED else "UNCONVERTED"
        bad += name not in REVIEWED
        print(f"index_check: {tag:11} {name} ({decl}, {size} B) indexed in {', '.join(files)}: the next "
              f"{uncovered} B are in no symbol{': ' + REVIEWED[name] if name in REVIEWED else ''}")
    print(f"index_check: {len(users)} symbols indexed past a scalar, {len(found)} with bytes no symbol covers after "
          f"them, {bad} not reviewed")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
