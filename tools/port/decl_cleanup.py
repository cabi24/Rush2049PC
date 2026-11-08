#!/usr/bin/env python3
"""Declarations the name maps left contradicting each other, removed (docs/port/PCONLY.md 3, wave 2's hand edits after
tools/port/rename.py --apply).

    python3 tools/port/decl_cleanup.py [--apply] [-v] [--root DIR] [PATH ...]   (default: src include port nonmatching)

Three kinds of line go, in C files (.c, .h, .inc):

- an identity macro, `#define X X` or `#define f(a, b) f(a, b)`: a `#define NEW OLD` the rename made of an alias the
  file kept for an address name (`#define gNumPlayers D_8014A108`), which now names the symbol itself;
- a file-scope data declaration of a name the same file declares or defines as a function (`extern s32
  AnimateNewBlastRing;` beside `void AnimateNewBlastRing(s32 *arg0, s16 arg1);`): the m2c context preambles declared
  a function's address spelling (D_80090B68) as data and the function spelling (func_80090B68) as a function; the
  maps give both one name, and C allows a name only one of the two kinds in a file. A data declaration whose name the
  file declares as nothing else stays;
- m2c's guess `extern s32 X;` of a datum the same file declares with its type in another file-scope declaration
  (`extern u8 rspbootTextStart[], rspbootTextEnd[];`): the preambles declared the address spelling (D_80028xxx) with
  the guess and the name with the type; the maps give both one name, and two types for it don't compile.

Only whole lines go, and only lines that are nothing but that declaration (a comment after it is allowed); comments
and strings are never read as code. Without --apply it says what it would remove (-v: every line). Stdlib only,
Python 3.9+.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
C_EXTS = (".c", ".h", ".inc")
DEFAULT_DIRS = ("src", "include", "port", "nonmatching")

TRAIL = r"[ \t]*(?:/\*(?:[^*]|\*(?!/))*\*/[ \t]*|//[^\n]*)?$"
IDENTITY = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+\1" + TRAIL)
IDENTITY_FN = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)[ \t]+\1[ \t]*\(([^)]*)\)" + TRAIL)
QUALIFIERS = r"(?:(?:extern|static|inline|const|volatile|signed|unsigned|struct|union|enum)[ \t]+)*"
# `extern T name;`, `T *name[N];`: one declarator, nothing else on the line (matched on the line with comments
# blanked)
DATA_DECL = re.compile(r"^[ \t]*" + QUALIFIERS + r"\w+(?:[ \t]+|[ \t]*\*[ \t*]*)(\w+)[ \t]*(?:\[[^\]\n]*\][ \t]*)*;"
                       r"[ \t]*$")
# a function's name at file scope: `T name(` at the start of a line (a prototype or a definition)
FUNC_DECL = re.compile(r"^[ \t]*" + QUALIFIERS + r"\w+(?:[ \t]+|[ \t]*\*[ \t*]*)(\w+)[ \t]*\(", re.M)
NOT_TYPES = {"return", "else", "case", "goto", "sizeof", "do", "if", "while", "for", "switch", "typedef"}
# m2c's declaration of a datum it knew no type for
GUESS = re.compile(r"^[ \t]*extern[ \t]+s32[ \t]+(\w+)[ \t]*;[ \t]*$")
# a declarator's name in an extern declaration: the identifier before [ , ; = or the end of the declarators
DECLARATOR = re.compile(r"(\w+)[ \t]*(?=\[|,|;|=)")
BASE_TYPE = re.compile(r"^[ \t]*extern[ \t]+(?:(?:const|volatile)[ \t]+)*((?:signed[ \t]+)?\w+)")
S32_TYPES = {"s32", "int", "signed int", "int32_t", "signed"}


def scan(text):
    """(the brace depth at the start of each line, the text with comments and the insides of strings and character
    constants blanked, its newlines kept). Preprocessor lines count no braces."""
    depths, depth, i, n = [0], 0, 0, len(text)
    out = []
    line_start = True
    while i < n:
        c = text[i]
        if c == "\n":
            depths.append(depth)
            out.append(c)
            line_start = True
            i += 1
            continue
        if line_start and c in " \t":
            out.append(c)
            i += 1
            continue
        if line_start and c == "#":   # a directive: to its end, with continuations
            while i < n and text[i] != "\n":
                if text[i] == "\\" and i + 1 < n and text[i + 1] == "\n":
                    depths.append(depth)
                    out.append("\\\n")
                    i += 2
                    continue
                out.append(text[i])
                i += 1
            continue
        line_start = False
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            k = text.count("\n", i, j)
            depths.extend([depth] * k)
            out.append(" " + "\n" * k)
            i = j
        elif text.startswith("//", i):
            while i < n and text[i] != "\n":
                i += 1
            out.append(" ")
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " + c)
            i = j + 1 if j < n and text[j] == c else j   # an unterminated one ends at its line's end
        else:
            if c == "{":
                depth += 1
            elif c == "}":
                depth = max(0, depth - 1)
            out.append(c)
            i += 1
    return depths, "".join(out)


def clean(text):
    """(the new text, [(line number, kind, line)] removed)."""
    lines = text.split("\n")
    depths, code = scan(text)
    code_lines = code.split("\n")
    assert len(code_lines) == len(lines) == len(depths)
    funcs = set()
    for m in FUNC_DECL.finditer(code):
        line = code.count("\n", 0, m.start())
        if depths[line] == 0 and m.group(1) not in NOT_TYPES:
            funcs.add(m.group(1))
    # the names other file-scope extern declarations give another type than s32: `extern u8 a[], b[];` (a plain
    # `extern s32 a, b;` or `extern int a;` agrees with the guess, which stays)
    typed = set()
    for no, c in enumerate(code_lines):
        if depths[no] == 0 and c.lstrip().startswith("extern") and not GUESS.match(c):
            decl = c.split("(")[0] if "(" in c.split("[")[0] else c
            base = BASE_TYPE.match(decl)
            for m in DECLARATOR.finditer(decl):
                plain = decl[m.end():].lstrip()[:1] != "[" and not decl[:m.start()].rstrip().endswith("*")
                if not (plain and base and base.group(1) in S32_TYPES):
                    typed.add(m.group(1))
    removed, keep = [], []
    for no, line in enumerate(lines):
        if not code_lines[no].strip():   # blank, or in a comment
            keep.append(line)
            continue
        m = IDENTITY.match(line)
        f = IDENTITY_FN.match(line)
        d = DATA_DECL.match(code_lines[no]) if depths[no] == 0 else None
        if m and code_lines[no].lstrip().startswith("#"):
            removed.append((no + 1, "identity macro", line))
            continue
        if f and code_lines[no].lstrip().startswith("#") and \
                [a.strip() for a in f.group(2).split(",")] == [a.strip() for a in f.group(3).split(",")]:
            removed.append((no + 1, "identity macro", line))
            continue
        if d and d.group(1) in funcs and not code_lines[no].lstrip().startswith(("typedef", "return")):
            removed.append((no + 1, "data declaration of a function", line))
            continue
        g = GUESS.match(code_lines[no]) if depths[no] == 0 else None
        if g and g.group(1) in typed:
            removed.append((no + 1, "m2c guess beside a typed declaration", line))
            continue
        keep.append(line)
    return "\n".join(keep), removed


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("-v", action="store_true", help="every line removed")
    args = ap.parse_args()
    root = Path(args.root)
    paths = [root / p for p in (args.paths or DEFAULT_DIRS)]
    files = []
    for p in paths:
        files += [p] if p.is_file() else sorted(q for q in p.rglob("*") if q.suffix in C_EXTS and q.is_file())
    totals = {"identity macro": 0, "data declaration of a function": 0, "m2c guess beside a typed declaration": 0}
    changed = 0
    for f in files:
        text = f.read_text(encoding="utf-8", errors="surrogateescape")
        new, removed = clean(text)
        if not removed:
            continue
        changed += 1
        for no, kind, line in removed:
            totals[kind] += 1
            if args.v:
                print(f"{f.relative_to(root)}:{no}: {kind}: {line.strip()}")
        if args.apply:
            f.write_text(new, encoding="utf-8", errors="surrogateescape")
    verb = "removed" if args.apply else "would remove"
    print(f"decl_cleanup: {verb} {totals['identity macro']} identity macros, "
          f"{totals['data declaration of a function']} data declarations of functions and "
          f"{totals['m2c guess beside a typed declaration']} m2c guesses beside a typed declaration in {changed} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
