#!/usr/bin/env python3
"""Mechanical 64-bit edits to the game C (docs/port/POINTERS.md). Every edit keeps the N64 view identical;
check with tools/port/n64_neutral.py.

    python3 tools/port/ptr64_edit.py guard           # local `typedef u32 uintptr_t;` -> under #ifndef TARGET_PC
    python3 tools/port/ptr64_edit.py need FILE...    # give FILE the N64 uintptr_t/intptr_t typedefs
    python3 tools/port/ptr64_edit.py gfx             # Gfx/Gwords w1 -> uintptr_t (host-built lists)

guard: the PC prelude (port/include/pc_prelude.h) defines uintptr_t/intptr_t pointer-sized; a file's own u32/s32
typedefs of them would clash, so they are wrapped in #ifndef TARGET_PC. The N64 compiler sees them unchanged.

need: files that use uintptr_t/intptr_t but don't declare them get, right after their last basic integer
typedef, `#ifndef TARGET_PC / typedef u32 uintptr_t; / typedef s32 intptr_t; / #endif` (u32/s32 or their
spelled-out types, whichever the file has).

gfx: in every local Gfx and Gwords typedef, `u32 w1;` becomes `uintptr_t w1;` (on the N64 a u32 typedef, so the
same type), giving the PC layout {u32 w0; uintptr_t w1;} of PcGfxWords (port/include/pc_port.h). One-line forms
(`u32 w0, w1;`) get an #ifdef TARGET_PC copy of the line. Files listed in N64_FORM_GFX keep 8-byte commands:
they walk display lists loaded from the ROM, which stay in N64 form.
Stdlib only, Python 3.9+.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = ("src/blob", "src/ovl_a", "src/ovl_b")
# Their local Gfx describes ROM display lists (N64 form, big-endian on PC): not widened.
N64_FORM_GFX = {"src/blob/entity_cull_check.c"}
GUARD_OLD = "typedef u32 uintptr_t;\ntypedef s32 intptr_t;\n"
GUARD_NEW = "#ifndef TARGET_PC\ntypedef u32 uintptr_t;\ntypedef s32 intptr_t;\n#endif\n"
_HAS_PTR_TYPEDEF = re.compile(r"^typedef\s+\w+(\s+\w+)?\s+uintptr_t\s*;", re.M)


def game_files():
    for d in GAME:
        yield from sorted((ROOT / d).rglob("*.c"))


def rel(p):
    return str(p.relative_to(ROOT))


def guard():
    n = 0
    for p in game_files():
        t = p.read_text()
        if GUARD_OLD in t and GUARD_NEW not in t:
            p.write_text(t.replace(GUARD_OLD, GUARD_NEW))
            n += 1
    print(f"guard: {n} files")


def ensure_ptr_typedefs(t):
    """The N64 uintptr_t/intptr_t typedefs, guarded, after the file's basic integer typedefs."""
    if _HAS_PTR_TYPEDEF.search(t):
        return t
    lines = t.split("\n")
    u32 = s32 = None
    last = None
    for i, l in enumerate(lines):
        m = re.match(r"^typedef\s+((?:signed|unsigned)?\s*(?:int|char|short|long(?:\s+long)?))\s+(\w+)\s*;\s*$", l)
        if m:
            last = i
            if m.group(2) == "u32":
                u32 = "u32"
            elif m.group(2) == "s32":
                s32 = "s32"
        elif re.match(r"^typedef .*\bu32;", l) and "struct" not in l:
            last, u32 = i, "u32"   # several typedefs on one line
            if re.search(r"\bs32;", l):
                s32 = "s32"
        if l.startswith(("typedef struct", "typedef union", "struct", "extern", "void ", "static ")) and last is not None:
            break
    if last is None:
        raise SystemExit("no integer typedefs to anchor the uintptr_t block")
    block = ["#ifndef TARGET_PC", f"typedef {u32 or 'unsigned int'} uintptr_t;",
             f"typedef {s32 or 'int'} intptr_t;", "#endif"]
    return "\n".join(lines[:last + 1] + block + lines[last + 1:])


def need(files):
    for f in files:
        p = Path(f).resolve()
        t = p.read_text()
        new = ensure_ptr_typedefs(t)
        if new != t:
            p.write_text(new)
            print(f"need: {rel(p)}")


_GFX_UNION = re.compile(r"(typedef union\s*\w*\s*\{\s*struct\s*\{\s*u32 w0;\s*)u32( w1;\s*\}\s*words;[^{}]*\}\s*Gfx;)")
_GFX_STRUCT = re.compile(r"(typedef struct\s*\w*\s*\{\s*u32 w0;\s*)u32( w1;\s*\}\s*(?:Gfx|Gwords);)")
_GFX_ONELINE = re.compile(r"^(typedef (?:union|struct)[^\n]*\bu32 w0, ?w1;[^\n]*\b(?:Gfx|Gwords);)$", re.M)


def gfx():
    changed = 0
    for p in game_files():
        r = rel(p)
        if r in N64_FORM_GFX:
            continue
        t = p.read_text()
        new = _GFX_UNION.sub(r"\1uintptr_t\2", t)
        new = _GFX_STRUCT.sub(r"\1uintptr_t\2", new)

        def oneline(m):
            pc = re.sub(r"u32 w0, ?w1;", "u32 w0; uintptr_t w1;", m.group(1))
            return f"#ifdef TARGET_PC\n{pc}\n#else\n{m.group(1)}\n#endif"
        new = _GFX_ONELINE.sub(oneline, new)
        if new != t:
            new = ensure_ptr_typedefs(new)
            p.write_text(new)
            changed += 1
    print(f"gfx: {changed} files")


def main(argv):
    if not argv or argv[0] not in ("guard", "need", "gfx"):
        sys.exit(__doc__)
    if argv[0] == "guard":
        guard()
    elif argv[0] == "need":
        need(argv[1:])
    else:
        gfx()


if __name__ == "__main__":
    main(sys.argv[1:])
