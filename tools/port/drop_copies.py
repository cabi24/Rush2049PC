#!/usr/bin/env python3
"""Bring every game function down to one definition: the copies the linker never used, out of the C.

    python3 tools/port/drop_copies.py --weak-dir build/pc/weak [--check]
    python3 tools/port/drop_copies.py --statics LIST [--check]     # LIST: FILE STATIC GLOBAL rows

The N64 decompilation compiled many functions in more than one file: a group's files carried copies of their callees
as context for IDO's interprocedural optimizer, and some files carried stand-in callers (`__standin_*`, `caller_a`) that
only kept a callee out of line. The N64 build placed each function's bytes from the file its manifest named; the PC
build made every other copy weak (tools/port/gen_weak.py, a `#pragma weak` header per file), so the linker bound every
call to the one strong definition. With the N64 build gone (docs/port/PCONLY.md) this takes the weak copies out of the
C, so each function has its one definition, the one the program linked:

- each weak copy gen_weak listed (its headers in --weak-dir name the file and the functions) goes; where the file still
  names the function, the copy's own header stays in its place as a declaration (`T name(params);`, inline keywords
  dropped), so every call in the file sees what it saw;
- the stand-ins go everywhere, with their declarations, when nothing but their own definitions and declarations names
  them.

--statics does the same for file-static copies of a global function (`static void ZOID_Reset(void)` in InitWaves.c,
whose -O0 code is func_800A5B3C's): the definition becomes a declaration of the global, and the file's uses of the
name call the global. Which statics are copies was decided from the compiled code (the same instructions, relocations
and constants as the global's), not from this tool (docs/port/PCONLY.md 4).

A definition is found by reading the file's top level (comments, strings and preprocessor lines skipped); a copy whose
body holds a preprocessor line, or a name not found exactly once in its file, is reported and left alone. --check
lists what would change. Stdlib only.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STANDIN = re.compile(r"^(__standin_\w+|caller_[ab])$")
KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "__attribute__", "__asm__", "asm", "__typeof__",
            "typeof", "__extension__", "_Alignas", "__declspec"}
IDENT = re.compile(r"[A-Za-z_]\w*")


class Skip(Exception):
    pass


def scan(text):
    """The file's significant tokens as (start, end, kind) with kind 'id', 'pp' (a preprocessor line) or the
    punctuation character; comments and literals are left out (literals as kind 'lit')."""
    toks = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if c == "\n":
            line_start = True
            i += 1
            continue
        if c in " \t\r\f\v":
            i += 1
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        if c == "#" and line_start:
            j = i
            while True:   # to the end of the logical line, comments included
                k = text.find("\n", j)
                if k < 0:
                    k = n
                    break
                if text[k - 1] == "\\":
                    j = k + 1
                    continue
                # a block comment that runs past the line end continues the directive
                seg = text[i:k]
                if seg.count("/*") > seg.count("*/"):
                    j = text.find("*/", k) + 2
                    continue
                break
            toks.append((i, k, "pp"))
            i = k
            continue
        line_start = False
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            toks.append((i, j + 1, "lit"))
            i = j + 1
            continue
        m = IDENT.match(text, i)
        if m:
            toks.append((i, m.end(), "id"))
            i = m.end()
            continue
        if c.isdigit():
            j = i + 1
            while j < n and (text[j].isalnum() or text[j] in "._" or (text[j] in "+-" and text[j - 1] in "eEpP")):
                j += 1
            toks.append((i, j, "lit"))
            i = j
            continue
        toks.append((i, i + 1, c))
        i += 1
    return toks


def definitions(text):
    """[(name, start, brace, end, has_pp, knr_close)] of the file's top-level function definitions: start is the first
    token of the declaration, brace its body's '{', end just past the body's '}', has_pp whether a preprocessor line is
    inside, and for an old-style (K&R) definition knr_close the end of its identifier list's ')' (else None)."""
    toks = scan(text)
    out = []
    depth = paren = 0
    stmt = None    # index of the first token of the current top-level declaration
    stmts = []     # the top-level declarations since the last '}' or preprocessor line: (first, last) token indices
    k = 0
    while k < len(toks):
        s, e, kind = toks[k]
        if kind == "pp":
            if depth == 0 and paren == 0:
                stmt = None
                stmts = []
            k += 1
            continue
        if depth == 0 and paren == 0 and stmt is None:
            stmt = k
        if kind == "(":
            paren += 1
        elif kind == ")":
            paren -= 1
        elif kind == "{" and paren == 0:
            if depth == 0:
                decl = toks[stmt:k]
                name = function_name(text, decl)
                start, knr = stmt, None
                # old style, `T f(a, b) T a; T b; {`: the head is the last statement before the '{' that names a
                # function, `f(a, b)`, with the parameters' declarations after it
                for first, last in (reversed(stmts) if not decl else ()):
                    head = toks[first:last]
                    opens = [i for i, t in enumerate(head) if t[2] == "(" and i and head[i - 1][2] == "id"]
                    if not opens:
                        continue
                    o = opens[0]
                    c = next((i for i in range(o + 1, len(head)) if head[i][2] == ")"), None)
                    n = text[head[o - 1][0]:head[o - 1][1]]
                    if (c is not None and c < len(head) - 1 and all(t[2] in ("id", ",") for t in head[o + 1:c])
                            and n not in KEYWORDS):
                        name, start, knr = n, first, head[c][1]
                    break
                d, m = 0, k
                while m < len(toks):
                    if toks[m][2] == "{":
                        d += 1
                    elif toks[m][2] == "}":
                        d -= 1
                        if d == 0:
                            break
                    m += 1
                if m == len(toks):   # braces that only balance branch by branch (#if ... #else ... #endif)
                    raise Skip("its braces don't balance outside the preprocessor's branches")
                if name:
                    out.append((name, toks[start][0], s, toks[m][1], any(t[2] == "pp" for t in toks[k:m]), knr))
                    k = m + 1
                    stmt = None
                    stmts = []
                    continue
            depth += 1
        elif kind == "}" and paren == 0:
            depth -= 1
            if depth == 0:   # a struct's or an initializer's: what follows starts afresh
                stmt = None
                stmts = []
        elif kind == ";" and depth == 0 and paren == 0:
            stmts.append((stmt, k))
            stmt = None
        k += 1
    return out


def function_name(text, decl):
    """The defined function's name if the declaration tokens before a top-level '{' are a function's header."""
    if not decl or decl[-1][2] not in (")",):
        return None
    words = [text[s:e] for s, e, kind in decl if kind == "id"]
    if words and words[0] in ("struct", "union", "enum", "typedef"):
        if not any(t[2] == "(" for t in decl):
            return None
    paren = 0
    for i, (s, e, kind) in enumerate(decl):
        if kind == "=" and paren == 0:
            return None
        if kind == "(":
            if paren == 0 and i and decl[i - 1][2] == "id":
                w = text[decl[i - 1][0]:decl[i - 1][1]]
                if w not in KEYWORDS:
                    return w
            paren += 1
        elif kind == ")":
            paren -= 1
    return None


def code_names(text):
    """Identifiers in the code (not in comments, literals or preprocessor lines), with their counts."""
    out = {}
    for s, e, kind in scan(text):
        if kind == "id":
            w = text[s:e]
            out[w] = out.get(w, 0) + 1
    return out


def prototype(text, start, brace, knr=None):
    if knr is not None:   # an old-style definition declares no parameter types: `T f();`
        head = text[start:knr]
        head = head[:head.rindex("(")] + "()"
    else:
        head = text[start:brace].rstrip()
    head = re.sub(r"\b(?:__inline__|__inline|inline)\b[ \t]*", "", head)
    return head + ";"


def drop(text, names, standins):
    """The text without the definitions of names (declarations left where the file still names them) and of the
    stand-ins (no declarations); (new text, [names dropped], [problems])."""
    defs = definitions(text)
    by_name = {}
    for d in defs:
        by_name.setdefault(d[0], []).append(d)
    cut, problems, dropped = [], [], []
    for name in sorted(names | {n for n in by_name if n in standins}):
        found = by_name.get(name, [])
        if len(found) != 1:
            problems.append(f"{name}: {len(found)} definitions found")
            continue
        _, start, brace, end, has_pp, knr = found[0]
        if has_pp:
            problems.append(f"{name}: a preprocessor line in the body")
            continue
        cut.append((start, brace, end, name, knr))
    cut.sort()
    # first without any declarations, to see what the rest of the file still names
    pieces, pos = [], 0
    for start, brace, end, name, knr in cut:
        pieces.append(text[pos:start])
        pos = end
    pieces.append(text[pos:])
    still = code_names(" ".join(pieces))
    out, pos = [], 0
    for start, brace, end, name, knr in cut:
        if name not in standins and still.get(name, 0):
            out.append(text[pos:start])
            out.append(prototype(text, start, brace, knr))
            pos = end
            dropped.append(name)
            continue
        # gone whole: its lines, a block comment right above it that names it, and a blank line if one is left on
        # each side
        ls = text.rfind("\n", 0, start) + 1
        if text[ls:start].strip():
            ls = start
        else:
            above = text[:ls]
            m = re.search(r"(?:^|\n)([ \t]*/\*(?:(?!\*/).)*\*/[ \t]*\n)$", above, re.S)
            if m and re.search(r"\b" + re.escape(name) + r"\b", m.group(1)) and ls - len(m.group(1)) >= pos:
                ls -= len(m.group(1))
        le = end
        m = re.match(r"[ \t]*\n", text[end:])
        if m and ls < start + 1 and (ls == 0 or text[ls - 1] == "\n"):
            le = end + m.end()
            if (ls == 0 or text[ls - 2:ls] == "\n\n") and text[le:le + 1] == "\n":
                le += 1
        out.append(text[pos:ls])
        pos = max(le, ls)
        dropped.append(name)
    out.append(text[pos:])
    new = "".join(out)
    # declarations of a stand-in go too
    for name in standins & set(dropped):
        new = re.sub(r"(?m)^[ \t]*(?:extern[ \t]+)?[\w \t\*]*\b" + re.escape(name) + r"\s*\([^;{)]*\)\s*;[ \t]*\n", "",
                     new)
    return new, dropped, problems


def to_global(text, static, glob):
    """The text with its file-static function `static`, a copy of the global `glob`, gone: its header stays as a
    declaration of the global (`static` and inline keywords dropped, the name the global's), as do its other
    declarations, and every use of the name in the code names the global."""
    found = [x for x in definitions(text) if x[0] == static]
    if len(found) != 1:
        raise Skip(f"{static}: {len(found)} definitions found")
    _, start, brace, end, has_pp, knr = found[0]
    if knr is not None or not re.search(r"\bstatic\b", text[start:brace]):
        raise Skip(f"{static}: not a static definition with a prototype")
    head = re.sub(r"\b(?:static|__inline__|__inline|inline)\b[ \t]*", "", text[start:brace].rstrip())
    head = re.sub(r"\b" + re.escape(static) + r"\b", glob, head, count=1) + ";"
    # the code's other uses of the name (comments and strings left as they are), from the end
    out = text[:start] + head + text[end:]
    shift = len(head) - (end - start)
    uses = [(s, e) for s, e, kind in scan(text) if kind == "id" and text[s:e] == static and not start <= s < end]
    for s, e in reversed(uses):
        if s >= end:
            s, e = s + shift, e + shift
        out = out[:s] + glob + out[e:]
    # its own declarations stay, as the global's (without `static` and inline keywords)
    out = re.sub(r"(?m)^([ \t]*)static\b[ \t]*((?:(?:__inline__|__inline|inline)\b[ \t]*)?)([^;{}\n]*\b" + re.escape(glob)
                 + r"\s*\([^;{}]*\)\s*;)", lambda m: m.group(1) + m.group(3), out)
    return out


def weak_lists(weak_dir):
    """{source path: {names}} from gen_weak's headers."""
    out = {}
    for h in sorted(Path(weak_dir).glob("*.h")):
        text = h.read_text()
        names = set(re.findall(r"^#pragma weak (\S+)", text, re.M))
        if names:
            src = text.split(":", 1)[0].lstrip("/* ").strip()
            out[src] = names
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--weak-dir", help="gen_weak's headers (build/pc/weak)")
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--statics", type=Path, help="FILE STATIC GLOBAL rows: file-static copies of global functions, "
                                                   "whose code was found the same, to replace by the global")
    a = ap.parse_args()
    if a.statics:
        done = 0
        for line in a.statics.read_text().splitlines():
            f = line.split("#", 1)[0].split()
            if not f:
                continue
            path = a.root / f[0]
            try:
                new = to_global(path.read_text(errors="surrogateescape"), f[1], f[2])
            except Skip as e:
                print(f"left alone: {f[0]}: {e}", file=sys.stderr)
                continue
            done += 1
            if not a.check:
                path.write_text(new, errors="surrogateescape")
        print(f"drop_copies: {done} file-static copies replaced by their global")
        return 0
    lists = weak_lists(a.weak_dir)
    files = {}
    for d in ("src", "nonmatching"):
        for p in sorted((a.root / d).rglob("*.c")):
            files[p.relative_to(a.root).as_posix()] = p
    texts = {rel: p.read_text(errors="surrogateescape") for rel, p in files.items()}
    # stand-ins nothing else names (their definitions and declarations aside)
    defined = {}
    unread = {}
    for rel, t in texts.items():
        try:
            defs = definitions(t)
        except Skip as e:
            unread[rel] = str(e)
            continue
        for d in defs:
            if STANDIN.match(d[0]):
                defined.setdefault(d[0], set()).add(rel)
    standins = set()
    for name in defined:
        users = [rel for rel, t in texts.items() if re.search(r"\b" + re.escape(name) + r"\b", t)]
        calls = sum(len(re.findall(r"\b" + re.escape(name) + r"\s*\(", texts[r])) for r in users)
        decls = sum(len(re.findall(r"(?m)^[ \t]*(?:extern[ \t]+)?[\w \t\*]*\b" + re.escape(name) + r"\s*\([^;{)]*\)\s*[;{]",
                                   texts[r])) for r in users)
        if calls == decls:   # every mention with a '(' is a definition or a declaration
            standins.add(name)
        else:
            print(f"kept stand-in {name}: named elsewhere ({calls - decls} other uses)")
    total = files_changed = 0
    bad = []
    for rel in sorted(set(lists) | {r for r in texts if any(r in fs for fs in defined.values())}):
        if rel not in texts:
            bad.append(f"{rel}: no such file")
            continue
        names = lists.get(rel, set())
        if rel in unread:
            bad.append(f"{rel}: {unread[rel]} ({' '.join(sorted(names))})")
            continue
        new, dropped, problems = drop(texts[rel], names, standins)
        bad += [f"{rel}: {p}" for p in problems]
        if dropped:
            total += len(dropped)
            files_changed += 1
            if a.check:
                print(f"{rel}: {len(dropped)}: {' '.join(dropped)}")
            else:
                files[rel].write_text(new, errors="surrogateescape")
    for b in bad:
        print(f"left alone: {b}", file=sys.stderr)
    print(f"drop_copies: {total} definitions out of {files_changed} files ({len(standins)} stand-ins)"
          + (f"; {len(bad)} left alone" if bad else ""))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
