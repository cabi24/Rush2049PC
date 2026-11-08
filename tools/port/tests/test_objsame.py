#!/usr/bin/env python3
"""objsame.py on objects compiled for the test: what must be `same` (declarations, local and static names, comments,
moved lines) and what must not (code, data, a string, a reference to another symbol, a moved __LINE__).

    python3 tools/port/tests/test_objsame.py [--cc COMPILER]

Exit status 0 when every case comes out as it should. Stdlib and a C compiler (ELF objects)."""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
OBJSAME = HERE.parent / "objsame.py"

BASE = """\
typedef int s32;
typedef struct { s32 a; float b; } Pair;
extern s32 gCount;
extern float gTable[];
extern Pair gPair;
static int helper(int x) { return x * 3; }
int lookup(int i)
{
    int sum = helper(i) + gCount;   /* a comment */
    return sum + (int)gTable[i] + gPair.a;
}
const char *name(void) { return "name"; }
int gDefined = 5;
static int counter;
int bump(void) { return ++counter; }
"""

# variant -> (source, expected verdict)
CASES = {
    # declarations another way, a local and a static renamed, comments, blank lines: the same code
    "decls": ("""\


/* the file's own declarations, spelled another way */
typedef signed int S32;
typedef struct PairTag { S32 a; float b; } Pair2;
extern int gCount;
extern float gTable[64];
extern Pair2 gPair;
extern int gUnused;
void unused_function(int);
static int triple(int value) { return value * 3; }
int lookup(int index)
{
    int total = triple(index) + gCount;
    return total + (int)gTable[index] + gPair.a;
}
const char *name(void) { return "name"; }
int gDefined = 5;
static int calls;
int bump(void) { return ++calls; }
""", "same"),
    "code": (BASE.replace("x * 3", "x * 4"), "DIFFERS"),
    "data": (BASE.replace("gDefined = 5", "gDefined = 6"), "DIFFERS"),
    "string": (BASE.replace('"name"', '"nam2"'), "DIFFERS"),
    "symbol": (BASE.replace("extern s32 gCount;", "extern s32 gCount, gOther;").replace("+ gCount;", "+ gOther;"),
               "DIFFERS"),
    "export": (BASE.replace("int gDefined = 5;", "static int gDefined = 5;"), "DIFFERS"),
    "type": (BASE.replace("typedef int s32;", "typedef unsigned s32;").replace("return sum +",
                                                                              "return (sum > 3u) +"), "DIFFERS"),
}
LINE_BASE = "int line(void) { return __LINE__; }\n"


def compile_c(cc, src, out):
    c = out.with_suffix(".c")
    c.write_text(src)
    subprocess.run([cc, "-std=gnu89", "-g", "-O0", "-fPIC", "-w", "-c", str(c), "-o", str(out)], check=True)


def objsame(*args):
    r = subprocess.run([sys.executable, str(OBJSAME), *map(str, args)], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"))
    a = ap.parse_args()
    if not shutil.which(a.cc) and not Path(a.cc).exists():
        print(f"test_objsame: no compiler {a.cc}")
        return 1
    bad = []
    with tempfile.TemporaryDirectory() as td:
        t = Path(td)
        compile_c(a.cc, BASE, t / "base.o")
        for name, (src, want) in CASES.items():
            compile_c(a.cc, src, t / f"{name}.o")
            rc, out = objsame(t / "base.o", t / f"{name}.o")
            got = "same" if rc == 0 and out.startswith("same") else "DIFFERS" if rc == 1 else f"error {rc}"
            print(f"test_objsame: {name:8s} {got:8s} (want {want})")
            if got != want:
                bad.append(name)
                print(out)
        # __LINE__: a moved line is a code change, which objsame shows (the checking builds' PC_N64W uses it)
        compile_c(a.cc, LINE_BASE, t / "line1.o")
        compile_c(a.cc, "\n\n" + LINE_BASE, t / "line2.o")
        rc, out = objsame(t / "line1.o", t / "line2.o")
        print(f"test_objsame: __LINE__ {'DIFFERS' if rc == 1 else 'same'} (want DIFFERS)")
        if rc != 1:
            bad.append("__LINE__")
        # two build directories: units matched by source path, --explained, a unit in one build only
        for b in ("A", "B"):
            objs = []
            for unit, var in (("src/x.c", "base"), ("src/y.c", "base" if b == "A" else "code"), ("src/z.c", "base")):
                d = t / b / "CMakeFiles" / "game.dir" / Path(unit).parent
                d.mkdir(parents=True, exist_ok=True)
                o = d / (Path(unit).name + ".o")
                shutil.copy(t / f"{var}.o", o)
                objs.append(str(o))
            if b == "B":
                d = t / b / "CMakeFiles" / "game.dir" / "src"
                shutil.copy(t / "base.o", d / "w.c.o")
                objs.append(str(d / "w.c.o"))
            (t / b / "game_objects.txt").write_text("\n".join(objs) + "\n")
        rc, out = objsame(t / "A", t / "B", "-q")
        ok = rc == 1 and "src/y.c" in out and "DIFFERS" in out and "src/w.c" in out and "only in B" in out
        print(f"test_objsame: builds   {'ok' if ok else 'WRONG'}")
        if not ok:
            bad.append("builds")
            print(out)
        (t / "explained.txt").write_text("# unit  why\nsrc/y.c  the test's code change\n")
        rc, out = objsame(t / "A", t / "B", "--explained", t / "explained.txt", "src/x.c", "src/y.c", "src/z.c")
        ok = rc == 0 and "differs, explained" in out and "2 same" in out
        print(f"test_objsame: explained {'ok' if ok else 'WRONG'}")
        if not ok:
            bad.append("explained")
            print(out)
    print(f"test_objsame: {'PASS' if not bad else 'FAIL: ' + ', '.join(bad)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
