#!/usr/bin/env python3
"""Make game C read as the PC compiler sees it: TARGET_PC's conditionals resolved, the neutrality macros unwrapped.

    python3 tools/port/flatten_pc.py [--check] [FILE...]      # default: every .c/.h under src/, include/, nonmatching/

Until 2026-11-08 the game C also built the N64 ROM, so the PC's code sat in `#ifdef TARGET_PC` blocks (the N64's in
the `#else`, or under `#ifndef TARGET_PC`), and statements one build had and the other didn't were wrapped in macros
that kept the function's lines as IDO compiled them: PC_SPIN, PC_IDLE, PC_ONLY and N64_ONLY (port/include/pc_prelude.h).
With the N64 build gone (docs/port/PCONLY.md) this rewrites each file as if TARGET_PC were defined:

1. `#ifdef TARGET_PC` (and `#if defined(TARGET_PC)`) keeps its first branch, `#ifndef TARGET_PC` its `#else`; the
   directives go, and so does every other branch. A directive's trailing comment that explains the PC side is kept as a
   comment line (`#ifdef TARGET_PC /* PC: pointer-sized ... */`). `#if defined(TARGET_PC) && X` becomes `#if X`.
2. The macros become what they expand to on the PC: `PC_SPIN(s)` -> `pc_os_spin_yield(s);`, `PC_IDLE()` ->
   `pc_os_idle();`, `PC_ONLY(stmt)` -> `stmt`, and `N64_ONLY(stmt)` goes.

The preprocessor's output for the PC is the same token for token before and after (only line numbers move), which is
how the result was checked (docs/port/PCONLY.md 4). A file whose conditionals don't parse (an `#elif` in a TARGET_PC
group, a comment that runs on past a directive) is reported and left alone. --check lists what would change.
Stdlib only.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRECTIVE = re.compile(r"^(\s*)#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)$")
PC_IF = re.compile(r"^\s*defined\s*\(\s*TARGET_PC\s*\)\s*$|^\s*defined\s+TARGET_PC\s*$")
PC_AND = re.compile(r"^\s*defined\s*\(\s*TARGET_PC\s*\)\s*&&\s*(.+)$")


class Unresolved(Exception):
    pass


def split_comment(rest):
    """(condition, trailing comment or '') of a directive's text; the comment must close on the line."""
    m = re.search(r"/[*/]", rest)
    if not m:
        return rest.strip(), ""
    cond, comment = rest[:m.start()].strip(), rest[m.start():].strip()
    if comment.startswith("/*") and not comment.endswith("*/"):
        raise Unresolved("a comment runs on past a directive")
    return cond, comment


def comment_state(line, in_comment):
    """Whether a block comment is open at the end of the line (strings and char literals skipped)."""
    i, n = 0, len(line)
    while i < n:
        if in_comment:
            j = line.find("*/", i)
            if j < 0:
                return True
            in_comment, i = False, j + 2
            continue
        c = line[i]
        if c == "/" and line.startswith("/*", i):
            in_comment, i = True, i + 2
        elif c == "/" and line.startswith("//", i):
            return False
        elif c in "\"'":
            j = i + 1
            while j < n and line[j] != c:
                j += 2 if line[j] == "\\" else 1
            i = j + 1
        else:
            i += 1
    return in_comment


def resolve(text):
    """The text with TARGET_PC's conditionals resolved; (new text, number of TARGET_PC groups resolved)."""
    lines = text.split("\n")
    out = []
    # One frame per open conditional: [kind, keep, outer] where kind is 'pc' (resolved here) or 'other' (passed
    # through, with its directives), keep says whether the current branch of a 'pc' frame is kept, and outer whether
    # the text around the conditional is.
    stack = []
    groups = 0
    in_comment = False

    def emitting():
        return all(f[1] for f in stack if f[0] == "pc") and all(f[2] for f in stack if f[0] == "other")

    i = 0
    while i < len(lines):
        line = lines[i]
        # a directive continued with backslashes is one logical line
        logical, j = line, i
        while logical.endswith("\\") and j + 1 < len(lines):
            j += 1
            logical = logical[:-1] + " " + lines[j].strip()
        m = None if in_comment else DIRECTIVE.match(logical)
        if not m:
            if emitting():
                out.append(line)
            in_comment = comment_state(line, in_comment)
            i += 1
            continue
        indent, d, rest = m.group(1), m.group(2), m.group(3)
        raw = lines[i:j + 1]
        i = j + 1
        if d in ("ifdef", "ifndef", "if"):
            cond, comment = split_comment(rest)
            pc = (d == "ifdef" and cond == "TARGET_PC") or (d == "if" and PC_IF.match(cond))
            npc = d == "ifndef" and cond == "TARGET_PC"
            if "TARGET_PC" in cond and not (pc or npc):
                am = PC_AND.match(cond)
                if d == "if" and am and "TARGET_PC" not in am.group(1):
                    if emitting():
                        out.append(f"{indent}#if {am.group(1).strip()}" + (f" {comment}" if comment else ""))
                    stack.append(["other", True, emitting()])
                    groups += 1
                    continue
                raise Unresolved(f"a condition on TARGET_PC this doesn't resolve: {logical.strip()}")
            if pc or npc:
                outer = emitting()
                stack.append(["pc", pc, outer])
                groups += 1
                if outer and pc and comment:
                    out.append(f"{indent}{comment}")
            else:
                outer = emitting()
                stack.append(["other", True, outer])
                if outer:
                    out.extend(raw)
            continue
        if not stack:
            raise Unresolved(f"#{d} without #if")
        top = stack[-1]
        if d == "elif":
            if top[0] == "pc":
                raise Unresolved("#elif in a TARGET_PC group")
            if "TARGET_PC" in rest:
                raise Unresolved("#elif on TARGET_PC")
            if all(f[1] for f in stack[:-1] if f[0] == "pc") and top[2]:
                out.extend(raw)
        elif d == "else":
            if top[0] == "pc":
                top[1] = not top[1]
                _, comment = split_comment(rest)
                if top[1] and top[2] and comment and "TARGET_PC" not in comment and "N64" not in comment:
                    out.append(f"{indent}{comment}")
            elif top[2]:
                out.extend(raw)
        else:   # endif
            stack.pop()
            if top[0] == "other" and top[2]:
                out.extend(raw)
    if stack:
        raise Unresolved("an #if without #endif")
    return "\n".join(out), groups


def tidy_blank_runs(old, new):
    """Where resolving left more blank lines in a row than the original ever has, keep as many as it does."""
    runs = [len(m.group(0)) for m in re.finditer(r"\n{3,}", old)]
    longest = max(runs) if runs else 2
    return re.sub(r"\n{%d,}" % (longest + 1), "\n" * longest, new)


def macro_args(text, i):
    """The end of a macro call's parenthesised argument starting at text[i] == '(' (strings and comments skipped)."""
    depth, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j
        elif text.startswith("/*", i):
            i = text.index("*/", i) - 1 + 2
        i += 1
    raise Unresolved("unbalanced macro call")


MACRO = re.compile(r"\b(PC_SPIN|PC_IDLE|PC_ONLY|N64_ONLY)\s*\(")
DEFINE = re.compile(r"^\s*#\s*define\s+(PC_SPIN|PC_IDLE|PC_ONLY|N64_ONLY)\b")


def unwrap(text):
    """The neutrality macros replaced by their PC expansion; (new text, count)."""
    out, pos, count = [], 0, 0
    if any(DEFINE.match(line) for line in text.split("\n")):
        raise Unresolved("defines a neutrality macro itself")
    for m in MACRO.finditer(text):
        if m.start() < pos:
            continue
        # skip mentions inside comments
        line_start = text.rfind("\n", 0, m.start()) + 1
        before = text[:m.start()]
        if before.rfind("/*") > before.rfind("*/") or "//" in text[line_start:m.start()]:
            continue
        name = m.group(1)
        open_paren = m.end() - 1
        close = macro_args(text, open_paren)
        arg = text[open_paren + 1:close]
        end = close + 1
        if name == "PC_SPIN":
            rep = f"pc_os_spin_yield({arg});"
        elif name == "PC_IDLE":
            if arg.strip():
                raise Unresolved("PC_IDLE with an argument")
            rep = "pc_os_idle();"
        elif name == "PC_ONLY":
            rep = arg
        else:   # N64_ONLY: the call goes, and its line if nothing else is on it
            rep = ""
            while end < len(text) and text[end] == " ":
                end += 1
            if not text[line_start:m.start()].strip() and (end == len(text) or text[end] == "\n"):
                out.append(text[pos:line_start])
                pos = end + 1
                count += 1
                continue
        out.append(text[pos:m.start()])
        out.append(rep)
        pos = end
        count += 1
    out.append(text[pos:])
    return "".join(out), count


def files_of(args):
    if args:
        return [Path(a) for a in args]
    out = []
    for d in ("src", "include", "nonmatching"):
        out += sorted(p for p in (ROOT / d).rglob("*") if p.suffix in (".c", ".h"))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="list what would change, change nothing")
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()
    changed = groups = macros = 0
    failed = []
    for p in files_of(a.files):
        old = p.read_text(errors="surrogateescape")
        if "TARGET_PC" not in old and not MACRO.search(old):
            continue
        try:
            new, g = resolve(old) if "TARGET_PC" in old else (old, 0)
            new = tidy_blank_runs(old, new)
            new, n = unwrap(new)
        except Unresolved as e:
            failed.append(f"{p}: {e}")
            continue
        if new != old:
            changed += 1
            groups += g
            macros += n
            if a.check:
                print(f"{p}: {g} TARGET_PC groups, {n} macros")
            else:
                p.write_text(new, errors="surrogateescape")
    for f in failed:
        print(f"left alone: {f}", file=sys.stderr)
    print(f"flatten_pc: {changed} files, {groups} TARGET_PC groups resolved, {macros} macro uses unwrapped"
          + (f", {len(failed)} files left alone" if failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
