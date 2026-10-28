#!/usr/bin/env python3
"""Functions defined in several files with no manifest owner must be the same function (docs/port/QA.md 2).

    python3 tools/port/weak_copies_check.py [--build build/pc]

gen_weak.py makes every copy of a function but its owner's weak; where the manifest names no owner it keeps the first
copy, and every caller in the program reaches that one. That is right only if the copies are the same function. The
start screen's InitEgg (the main menu's cheat code, D_803B41D4) and eggs.c's (cheat i of D_803B3460) were not: the
main menu got eggs.c's and its code could never be entered. This reads gen_weak's report (weak/report.txt), takes
each such function's body from each file's source (comments and spacing ignored) and lists those whose bodies
differ. Exit status 1 if one is not in REVIEWED. Stdlib only.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SECTION = "Defined in several files, no manifest owner"
# Copies read by hand that may differ: name -> why.
REVIEWED = {
    "hs_font_set": "the same calls: one spells the unlock as a helper, the other as osSendMesg(&D_801461D0, 0, 0)",
    "__standin_a": "a stand-in caller (keeps a callee out of line under -O3); nothing calls it",
    "__standin_b": "a stand-in caller; nothing calls it",
    "caller_a": "a stand-in caller (audio_pitch_adjust, draw_number); nothing calls it",
    "caller_b": "a stand-in caller (audio_pitch_adjust, draw_number); nothing calls it",
}


def duplicates(report):
    """[(name, [files])] of the report's section on copies with no manifest owner."""
    out, on = [], False
    for line in report.splitlines():
        if line.startswith(SECTION):
            on = True
        elif on and line.startswith("  ") and ": " in line:
            name, files = line.strip().split(": ", 1)
            out.append((name, files.split(", ")))
        elif on:
            break
    return out


def body(text, name):
    """The body of `name`'s definition in a C file's text, comments and spacing dropped; None if it has none."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    m = re.search(r"(^|\n)[^\n;{}#]*\b" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", text)
    if not m:
        return None
    i, depth = m.end(), 1
    while depth and i < len(text):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return re.sub(r"\s+", " ", text[m.end():i]).strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build", type=Path, default=ROOT / "build/pc")
    ap.add_argument("--root", type=Path, default=ROOT)
    a = ap.parse_args()
    report = a.build / "weak/report.txt"
    if not report.exists():
        sys.exit(f"weak_copies_check: no {report} (build the program first)")
    bad = 0
    dups = duplicates(report.read_text())
    for name, files in dups:
        bodies = {}
        for f in files:
            bodies.setdefault(body((a.root / f).read_text(errors="replace"), name), []).append(f)
        if len(bodies) == 1:
            continue
        if name in REVIEWED:
            print(f"reviewed {name}: {REVIEWED[name]}")
            continue
        bad += 1
        print(f"FAIL {name}: the copies differ, and the program keeps one of them for every caller:")
        for b, fs in bodies.items():
            print(f"    {', '.join(fs)}: {(b or '<no definition found>')[:160]}")
    print(f"weak_copies_check: {len(dups)} functions with copies and no owner, {bad} whose copies differ")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
