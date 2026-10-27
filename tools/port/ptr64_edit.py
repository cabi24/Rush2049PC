#!/usr/bin/env python3
"""Mechanical 64-bit edits to the game C (docs/port/POINTERS.md). Every edit keeps the N64 view identical;
check with tools/port/n64_neutral.py.

    python3 tools/port/ptr64_edit.py guard           # local `typedef u32 uintptr_t;` -> under #ifndef TARGET_PC
    python3 tools/port/ptr64_edit.py need FILE...    # give FILE the N64 uintptr_t/intptr_t typedefs
    python3 tools/port/ptr64_edit.py gfx             # Gfx/Gwords w1 -> uintptr_t (host-built lists)
    python3 tools/port/ptr64_edit.py gbi             # address-carrying GBI macros store w1 via uintptr_t
    python3 tools/port/ptr64_edit.py casts [FILE...] # (u32)ptr -> (uintptr_t)ptr at each warning (build/pc)

guard: the PC prelude (port/include/pc_prelude.h) defines uintptr_t/intptr_t pointer-sized; a file's own u32/s32
typedefs of them would clash, so they are wrapped in #ifndef TARGET_PC. The N64 compiler sees them unchanged.

need: files that use uintptr_t/intptr_t but don't declare them get, right after their last basic integer
typedef, `#ifndef TARGET_PC / typedef u32 uintptr_t; / typedef s32 intptr_t; / #endif` (u32/s32 or their
spelled-out types, whichever the file has).

gfx: in every local Gfx and Gwords typedef, `u32 w1;` becomes `uintptr_t w1;` (on the N64 a u32 typedef, so the
same type), giving the PC layout {u32 w0; uintptr_t w1;} of PcGfxWords (port/include/pc_port.h). One-line forms
(`u32 w0, w1;`) get an #ifdef TARGET_PC copy of the line. Files listed in N64_FORM_GFX keep 8-byte commands:
they walk display lists loaded from the ROM, which stay in N64 form.

gbi: the local GBI macros whose w1 is an address (gSetImage, gDma1p, gDma2p, gSPVertex, gMoveWd for
gSPSegment, gDma1p_) cast it with (uintptr_t) instead of (unsigned int)/(u32), so host pointers aren't cut to
32 bits. Macros whose w1 is a value (other modes, combine, colors, geometry mode) keep their 32-bit casts:
for those the upper half of w1 is zero.

casts: compiles FILEs (default: all game files) with the PC build's flags (build/pc/compile_commands.json) and,
at each -Wpointer-to-int-cast, rewrites a (u32)/(unsigned int)/(s32)/(int) cast to (uintptr_t)/(intptr_t),
keeping the signedness. This only stops the cast truncating; where the value then lands in 32-bit storage
the storage is widened by hand (docs/port/POINTERS.md lists how those were found).
Stdlib only, Python 3.9+.
"""
import json
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = ("src/blob", "src/ovl_a", "src/ovl_b")
# Their local Gfx describes ROM display lists (N64 form, big-endian on PC): not widened.
N64_FORM_GFX = {"src/blob/entity_cull_check.c"}
# The 32-bit N64 world (docs/port/ASSETS.md): the game heap (allocator internals and its u32-address API:
# free, purge, size, handles) and the loaders that relocate ROM records in place. On PC the heap sits in the
# RDRAM window at host 0x80000000, so (u32) of those addresses is exact; their casts stay as they are.
N64_WORLD = {
    "src/blob/func_80095EF4.c", "src/blob/resource_alloc_init.c", "src/blob/MBSetupWad.c",
    "src/blob/groups/audio_heap/group.c", "src/blob/groups/counter_guard/group.c",
    "src/blob/groups/state/ctx_alloc_at.c", "src/blob/groups/state/ctx_heap_group.c", "src/blob/groups/state/state.c",
    "src/blob/groups/score_insert/alloc_at.c", "src/blob/groups/score_insert/group.c",
    "src/blob/groups/score_insert/car_damage_visual.c", "src/blob/groups/score_insert/deflate_mem.c",
    "src/blob/groups/score_insert/func_800CB748.c",
    "src/blob/groups/heap_release/alloc_at.c", "src/blob/groups/heap_release/group.c",
    "src/blob/groups/heap_release/car_damage_visual.c", "src/blob/groups/heap_release/deflate_mem.c",
    "src/blob/groups/heap_release/initsystem.c",
    "src/blob/groups/carcolor/ctx_alloc_at.c", "src/blob/groups/carcolor/ctx_group.c",
    "src/blob/groups/carcolor/ctx_car_damage_visual.c", "src/blob/groups/carcolor/ctx_deflate_mem.c",
    "src/blob/groups/carcolor/ctx_initsystem.c", "src/blob/groups/carcolor/carcolor.c",
    "src/blob/groups/paksys/ctx_heap.c", "src/blob/groups/paksys/group.c",
    "src/blob/groups/func_800E7A98/heap_release_context.c", "src/blob/groups/func_800E7A98/func_800E7A98.c",
    "src/blob/groups/menu_save_options/ctx_slot.c", "src/blob/groups/menu_save_options/ctx_newghost.c",
    "src/blob/groups/menu_save_options/hud_setup.c", "src/blob/groups/menu_save_options/save.c",
    "src/blob/groups/state_audio/group.c",
    "src/blob/groups/audio_frame_sync/waves.c", "src/blob/groups/slot_sound/func_800A4E58.c",
    "src/blob/groups/audio_init_sys/ctx_func_800A4E58.c", "src/blob/groups/loadtrack/loadtrack.c",
    "src/blob/groups/audio_init_sys/car_shadow_render.c",
}
GUARD_OLD = "typedef u32 uintptr_t;\ntypedef s32 intptr_t;\n"
GUARD_NEW = "#ifndef TARGET_PC\ntypedef u32 uintptr_t;\ntypedef s32 intptr_t;\n#endif\n"
_HAS_PTR_TYPEDEF = re.compile(r"^typedef\s+\w+(\s+\w+)?\s+uintptr_t\s*;", re.M)


def game_files():
    for d in GAME:
        for p in sorted((ROOT / d).rglob("*.c")):
            if p.name not in ("overlay_loader.c", "ctx_overlay_loader.c"):   # the overlay-loader work's
                yield p


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


_CASTS = {"u32": "uintptr_t", "unsigned int": "uintptr_t", "unsigned": "uintptr_t", "U32": "uintptr_t",
          "s32": "intptr_t", "int": "intptr_t", "signed int": "intptr_t", "S32": "intptr_t"}
_CAST_AT = re.compile(r"\(\s*(u32|unsigned int|unsigned|U32|s32|int|signed int|S32)\s*\)")
_WARN = re.compile(r"^(\S+?):(\d+):(\d+): warning: cast from pointer to integer of different size")


def pointer_casts(files, build):
    """file -> sorted {(line, col)} of -Wpointer-to-int-cast warnings, from the PC build's compile commands."""
    cc = json.loads((build / "compile_commands.json").read_text())
    want = {str(Path(f).resolve()) for f in files}
    sites = {}
    for e in cc:
        f = str(Path(e["file"]).resolve())
        if f not in want:
            continue
        args = e["arguments"] if "arguments" in e else shlex.split(e["command"])
        out, skip = [], False
        for x in args:
            if skip:
                skip = False
                continue
            if x in ("-o", "-MF", "-MT", "-MQ"):
                skip = True
                continue
            if x not in ("-MD", "-MMD"):
                out.append("-fsyntax-only" if x == "-c" else x)
        r = subprocess.run(out, cwd=e["directory"], capture_output=True, text=True)
        for line in r.stderr.splitlines():
            m = _WARN.match(line)
            if m:
                src = str(Path(e["directory"], m.group(1)).resolve())
                sites.setdefault(src, set()).add((int(m.group(2)), int(m.group(3))))
    return sites


def casts(files, build):
    """At each pointer-to-int warning, a 32-bit integer cast becomes the pointer-sized one of the same
    signedness (u32 -> uintptr_t, s32 -> intptr_t: the same types on the N64)."""
    sites = pointer_casts(files, build)
    left = []
    for src, locs in sorted(sites.items()):
        p = Path(src)
        lines = p.read_text().split("\n")
        changed = False
        for ln, col in sorted(locs, key=lambda x: (x[0], -x[1])):   # right to left on a line
            text = lines[ln - 1]
            m = _CAST_AT.match(text, col - 1)
            if not m:
                left.append(f"{rel(p)}:{ln}:{col}: {text.strip()[:100]}")
                continue
            lines[ln - 1] = text[:m.start()] + "(" + _CASTS[m.group(1)] + ")" + text[m.end():]
            changed = True
        if changed:
            p.write_text(ensure_ptr_typedefs("\n".join(lines)))
            print(f"casts: {rel(p)}: {len(locs)}")
    for l in left:
        print("casts: left", l)


GBI_ADDR = {"gSetImage", "gDma1p", "gDma2p", "gSPVertex", "gMoveWd", "gDma1p_"}
_W1_CAST = re.compile(r"(w1\s*=\s*)\((?:unsigned int|u32)\)\(")


def gbi():
    changed = 0
    for p in game_files():
        lines = p.read_text().split("\n")
        old = list(lines)
        i = 0
        while i < len(lines):
            m = re.match(r"#define\s+(\w+)\(", lines[i])
            while m and m.group(1) in GBI_ADDR:
                lines[i] = _W1_CAST.sub(r"\1(uintptr_t)(", lines[i])
                if not lines[i].rstrip().endswith("\\"):
                    break
                i += 1
            i += 1
        if lines != old:
            p.write_text(ensure_ptr_typedefs("\n".join(lines)))
            changed += 1
    print(f"gbi: {changed} files")


def main(argv):
    if not argv or argv[0] not in ("guard", "need", "gfx", "gbi", "casts"):
        sys.exit(__doc__)
    if argv[0] == "guard":
        guard()
    elif argv[0] == "need":
        need(argv[1:])
    elif argv[0] == "gbi":
        gbi()
    elif argv[0] == "casts":
        casts(argv[1:] or [str(p) for p in game_files() if rel(p) not in N64_WORLD], ROOT / "build" / "pc")
    else:
        gfx()


if __name__ == "__main__":
    main(sys.argv[1:])
