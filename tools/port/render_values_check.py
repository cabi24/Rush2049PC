#!/usr/bin/env python3
"""Render and per-PC values that reach the game C (docs/port/LOCKSTEP.md 6, NETPLAY.md risk 13).

    python3 tools/port/render_values_check.py [--list]

In lockstep both PCs run the same game; a value the game C takes from the PC layer that one PC's window, renderer,
audio device, input or settings file decides can make the two games differ. This lists every place the game C
(src/, nonmatching/, include/) names such a value, comments left out:

  - a function of the PC layer's render or per-PC parts: pc_gfx_*, pc_audio_*, pc_input_*, pc_menu_*, pc_app_*,
    pc_cfg*, pc_settings_*, and the detail settings' readers (pc_detail, pc_draw_distance, pc_fog, pc_draw_reach,
    pc_lod_distance, pc_detail_players, pc_draw_range_scale, pc_texture_lod);
  - a macro of port/include's headers whose definition calls one (PC_DRAW_REACH...), found here by reading them.

Each (file, name) must be in tools/port/render_values_allowed.txt, with what makes it safe: a setting a session must
match (NETPLAY.md 1.5), a session rule (--session-rules), a test hook. A new one fails (exit 1) until it is read and
listed; one listed that is gone is said, so the list stays true. --list prints what is found, as the file's lines.
Stdlib only, Python 3.9+.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ALLOWED = ROOT / "tools/port/render_values_allowed.txt"
FUNCS = re.compile(r"\b(pc_gfx_\w+|pc_audio_\w+|pc_input_\w+|pc_menu_\w+|pc_app_\w+|pc_cfg\w*|pc_settings_\w+|"
                   r"pc_detail|pc_draw_distance|pc_fog|pc_draw_reach|pc_lod_distance|pc_detail_players|"
                   r"pc_draw_range_scale|pc_texture_lod)\b")
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def strip(text):
    """The code without its comments (string literals are left: none names these)."""
    return _COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), text)


def header_macros():
    """port/include's macros whose definitions call a watched function: {macro: function}."""
    out = {}
    for h in sorted((ROOT / "port/include").glob("*.h")):
        code = strip(h.read_text(errors="replace"))
        for m in re.finditer(r"^\s*#\s*define\s+(\w+)(\([^)]*\))?\s+(.*)$", code, re.M):
            f = FUNCS.search(m.group(3))
            if f:
                out[m.group(1)] = f.group(1)
    return out


def found():
    macros = header_macros()
    names = re.compile(r"\b(" + "|".join(sorted(map(re.escape, macros))) + r")\b") if macros else None
    hits = {}
    for top in ("src", "nonmatching", "include"):
        for p in sorted((ROOT / top).rglob("*")):
            if p.suffix not in (".c", ".h") or not p.is_file():
                continue
            code = strip(p.read_text(errors="replace"))
            rel = p.relative_to(ROOT).as_posix()   # forward slashes on Windows too, as the allowlist spells them
            for m in FUNCS.finditer(code):
                hits.setdefault((rel, m.group(1)), code.count("\n", 0, m.start()) + 1)
            if names:
                for m in names.finditer(code):
                    hits.setdefault((rel, m.group(1)), code.count("\n", 0, m.start()) + 1)
    return hits, macros


def allowed():
    out = {}
    for line in ALLOWED.read_text().splitlines():
        body = line.split("#", 1)[0].split()
        if len(body) >= 2:
            out[(body[0], body[1])] = line.split("#", 1)[1].strip() if "#" in line else ""
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--list", action="store_true", help="print what is found, as the allow list's lines")
    a = ap.parse_args()
    hits, macros = found()
    if a.list:
        for (f, n), line in sorted(hits.items()):
            print(f"{f} {n}   # line {line}{f'; calls {macros[n]}' if n in macros else ''}")
        return 0
    ok = allowed()
    new = sorted(k for k in hits if k not in ok)
    gone = sorted(k for k in ok if k not in hits)
    for f, n in new:
        print(f"render_values_check: NEW {f}:{hits[(f, n)]}: {n}{f' (calls {macros[n]})' if n in macros else ''}: a "
              f"render or per-PC value in the game C; read it, and list it in {ALLOWED.name} with what keeps two PCs "
              f"alike, or keep it out")
    for f, n in gone:
        print(f"render_values_check: {f}: {n} is listed but no longer there (drop it from {ALLOWED.name})")
    print(f"render_values_check: {len(hits)} uses of render or per-PC values in the game C, {len(new)} not listed; "
          f"macros that reach them: {', '.join(sorted(macros)) or 'none'}")
    return 1 if new else 0


if __name__ == "__main__":
    sys.exit(main())
