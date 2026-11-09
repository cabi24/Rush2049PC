#!/usr/bin/env python3
"""to_headers.py on a made-up tree: a file with an m2c-style preamble becomes `#include "game/game.h"` and what is
its own (a view of a datum, a bounded array as a view, its own struct, a declaration the headers lack), compiles to
the same object (objsame.py), and a second run changes nothing.

    python3 tools/port/tests/test_to_headers.py [--cc COMPILER]

Stdlib, binutils (readelf) and a C compiler (ELF objects)."""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
REPO = TOOLS.parents[1]

HEADER = """\
#ifndef GAME_GAME_H
#define GAME_GAME_H
#ifdef GAME_DECLS_EXTENTS
#define EXTENT(n) n
#else
#define EXTENT(n)
#endif
#include "game/view.h"
typedef signed int s32;
typedef unsigned char u8;
typedef float f32;
#define NULL ((void *)0)
typedef struct Pair { s32 a; f32 b; } Pair;
extern s32 gCount;
extern Pair gPairs[EXTENT(4)];
extern u8 gBytes[EXTENT(8)];
extern Pair *gCurrent;
s32 add(s32 x, s32 y);
#endif
"""
DECLS = '#define GAME_DECLS_EXTENTS\n#include "game/game.h"\n'
SOURCE = """\
/* flags: -g0 -O2 -mips2 */
typedef signed int s32;
typedef unsigned char u8;
typedef float f32;
typedef double f64;
#define NULL ((void *)0)
typedef struct Pair { s32 a; f32 b; } Pair;
typedef struct { u8 pad[4]; f32 b; } MyPair;   /* this file's view */
extern s32 gCount;
extern MyPair gPairs[];
extern u8 gBytes[8];
extern Pair *gCurrent;
extern s32 gUnused;
extern f64 gUnusedToo;
s32 add(s32, s32);   /* the headers' prototype: dropped, the call compiles alike */
s32 sub(s32, s32);
void unused_function(void);

/* the file's own code */
s32 use(void)
{
    return add(gCount, 1) + (s32)gPairs[1].b + gBytes[2] + sub(1, 2) + gCurrent->a + (NULL == 0);
}
"""


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"))
    a = ap.parse_args()
    bad = []
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        (root / "include/game").mkdir(parents=True)
        (root / "include/game/game.h").write_text(HEADER)
        shutil.copy(REPO / "include/game/view.h", root / "include/game/view.h")
        (root / "port/data").mkdir(parents=True)
        (root / "port/data/game_decls.c").write_text(DECLS)
        (root / "src").mkdir()
        (root / "src/a.c").write_text(SOURCE)
        build = root / "build"
        obj = build / "CMakeFiles/game.dir/src/a.c.o"
        decls = build / "CMakeFiles/game_decls.dir/port/data/game_decls.c.o"
        for p in (obj, decls):
            p.parent.mkdir(parents=True, exist_ok=True)
        flags = ["-std=gnu89", "-g", "-O0", "-fPIC", "-w", "-I", str(root / "include")]

        def cc(src, out, extra=()):
            r = run([a.cc, *flags, *extra, "-c", str(src), "-o", str(out)])
            if r.returncode:
                print(r.stderr)
            return r.returncode == 0

        if not (cc(root / "src/a.c", obj) and cc(root / "port/data/game_decls.c", decls,
                                                    ["-fno-eliminate-unused-debug-symbols",
                                                     "-fno-eliminate-unused-debug-types"])):
            print("test_to_headers: the fixture doesn't compile")
            return 1
        (build / "types_objects.txt").write_text(f"{obj}\n")
        before = root / "before.o"
        shutil.copy(obj, before)
        tool = [sys.executable, str(TOOLS / "to_headers.py"), "--build", str(build), "--root", str(root), "-j", "1"]
        r = run([*tool, "--apply"])
        print(r.stdout.strip())
        text = (root / "src/a.c").read_text()
        print(text)
        expect = {
            "includes the headers": text.startswith('#include "game/game.h"'),
            "the flags line goes": "flags:" not in text,
            "a view of gPairs": "VIEW(MyPair[], gPairs);" in text and "#define gPairs gPairs_view" in text,
            "a bounded array is a view": "VIEW(u8[8], gBytes);" in text,
            "its own struct stays": "MyPair;" in text,
            "the headers' types go": "typedef signed int s32" not in text and "struct Pair {" not in text,
            "the headers' data go": "gCount;" not in text and "gCurrent;" not in text,
            "unused declarations go": "gUnused" not in text and "unused_function" not in text,
            "the headers' function goes": "s32 add(" not in text,
            "what the headers lack stays": "s32 sub(s32, s32);" in text,
            "a macro the headers define alike goes": "#define NULL" not in text,
            "the code stays": "/* the file's own code */" in text and "gCurrent->a" in text,
        }
        for k, ok in expect.items():
            print(f"test_to_headers: {k:40s} {'ok' if ok else 'WRONG'}")
            if not ok:
                bad.append(k)
        if not cc(root / "src/a.c", obj):
            bad.append("compiles")
        else:
            r = run([sys.executable, str(TOOLS / "objsame.py"), str(before), str(obj)])
            print(f"test_to_headers: objsame {r.stdout.strip().splitlines()[0]}")
            if r.returncode:
                bad.append("objsame")
                print(r.stdout)
            r = run([*tool, "--check"])
            print(f"test_to_headers: second run: {r.stdout.strip().splitlines()[-1]}")
            if r.returncode:
                bad.append("idempotent")
    print(f"test_to_headers: {'PASS' if not bad else 'FAIL: ' + ', '.join(bad)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
