#!/usr/bin/env python3
"""Checks of the mods tests' runs (docs/port/MODS.md 8; tools/port/synth/mods_*.press).

    python3 tools/port/mods_check.py shot PNG --rgb R,G,B [--min N] [--max N]   pixels of that colour (within 8)
    python3 tools/port/mods_check.py same A B                                  two files byte for byte
    python3 tools/port/mods_check.py differ A B                                two pictures differ
    python3 tools/port/mods_check.py log LOG [--has TEXT ...] [--lacks TEXT ...] [--count TEXT=N ...]

Exit status 1 on a failed check. Stdlib only.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_shot  # noqa: E402


def near(px, rgb, tol=8):
    return all(abs(a - b) <= tol for a, b in zip(px, rgb))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("shot")
    p.add_argument("png")
    p.add_argument("--rgb", required=True)
    p.add_argument("--min", type=int, default=1)
    p.add_argument("--max", type=int)
    for name in ("same", "differ"):
        p = sub.add_parser(name)
        p.add_argument("a")
        p.add_argument("b")
    p = sub.add_parser("log")
    p.add_argument("log")
    p.add_argument("--has", action="append", default=[])
    p.add_argument("--lacks", action="append", default=[])
    p.add_argument("--count", action="append", default=[])
    a = ap.parse_args()
    bad = []
    if a.cmd == "shot":
        rgb = tuple(int(v) for v in a.rgb.split(","))
        w, h, rows = synth_shot.read_png(a.png)
        n = sum(near(px, rgb) for row in rows for px in row)
        print(f"mods_check: {a.png}: {n} of {w * h} pixels are {a.rgb}")
        if n < a.min or (a.max is not None and n > a.max):
            bad.append(f"{n} pixels of {a.rgb}, not {a.min}{f' to {a.max}' if a.max is not None else ' or more'}")
    elif a.cmd in ("same", "differ"):
        same = Path(a.a).read_bytes() == Path(a.b).read_bytes()
        print(f"mods_check: {a.a} and {a.b} are {'the same' if same else 'different'}")
        if same != (a.cmd == "same"):
            bad.append("expected " + ("the same" if a.cmd == "same" else "a difference"))
    else:
        text = Path(a.log).read_text(encoding="utf-8", errors="replace")
        for t in a.has:
            if t not in text:
                bad.append(f"no '{t}'")
        for t in a.lacks:
            if t in text:
                bad.append(f"'{t}' is there")
        for c in a.count:
            t, n = c.rsplit("=", 1)
            if text.count(t) != int(n):
                bad.append(f"'{t}' {text.count(t)} times, not {n}")
        print(f"mods_check: {a.log}: {'ok' if not bad else '; '.join(bad)}")
    if bad:
        print("mods_check: FAIL: " + "; ".join(bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
