#!/usr/bin/env python3
"""Prove that source edits leave the N64 build's view of the code unchanged.

    python3 tools/port/n64_neutral.py [--base REV] [FILE ...]     # default: every src/ and include/ file
                                                                  # changed since REV (default HEAD)

The matching N64 build (IDO, x86-64 only) can't run on every host, so PC-port edits to shared game C are
checked here instead. Each file is preprocessed the way the N64 build preprocesses it (no TARGET_PC; the
Makefile's -I/-D for src/rom, none for the game and overlay files; IDO-like predefines) with the host's
`gcc -E -undef -nostdinc`, at REV and in the working tree, and the two token streams are compared:

1. Tokens. Equal after normalization: runs of basic type keywords get one spelling (`unsigned int` ==
   `unsigned`, `signed int` == `int`, `long int` == `long`), and names of simple file-scope typedefs
   (`typedef u32 uintptr_t;`) are replaced by the type they name, from the point of their declaration. So
   `(u32)p` -> `(uintptr_t)p` passes where `uintptr_t` is a u32 typedef the N64 compiler sees, and
   `long` -> `s32` fails. The simple typedef declarations themselves are compared as a table: a name in both
   must name the same type, and a name may not be declared twice where it was not before.
2. Lines. IDO's assembler breaks scheduling ties by source line (docs/COMPILER_SETTINGS.md), so the lines
   tokens sit on matter too. "exact": every token on the same line. "shifted": tokens on the same line before
   are on the same line after, order is kept, and each function body moved by one constant (lines added
   above or between functions only, never inside one). Anything else fails.
3. Text. Added conditional directives must test TARGET_PC (so no other macro decides what IDO sees), and
   no `//` comments are added to game files (IDO compiles most of them without -Xcpluscomm).

A changed header under include/ or src/rom/ is checked through every src/rom file (the only C that
includes headers). Exit status 1 if any file fails. Stdlib only, Python 3.9+.
"""
import argparse
import io
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME_DIRS = ("src/blob/", "src/ovl_a/", "src/ovl_b/")
ROM_FLAGS = ["-Iinclude", "-Iinclude/PR", "-D_LANGUAGE_C"]
# What IDO's cc predefines for a big-endian o32 MIPS target (the game C tests none of these; they are here so
# a header that does sees the N64 values).
IDO_DEFINES = ["-D__mips=1", "-Dmips=1", "-D_MIPS_ISA=2", "-D_MIPS_SIM=1", "-D_MIPS_SZINT=32",
               "-D_MIPS_SZLONG=32", "-D_MIPS_SZPTR=32", "-D_MIPSEB=1", "-DMIPSEB=1", "-D__sgi=1", "-Dsgi=1",
               "-D__unix=1", "-Dunix=1", "-D_LANGUAGE_C=1", "-DLANGUAGE_C=1", "-D_SVR4_SOURCE=1",
               "-D_SGI_SOURCE=1", "-D_SYSTYPE_SVR4=1", "-D__host_mips=1"]

_TOKEN = re.compile(r"""
    (?P<ws>[ \t\r\f\v]+)
  | (?P<nl>\n)
  | (?P<str>L?"(?:\\.|[^"\\\n])*")
  | (?P<chr>L?'(?:\\.|[^'\\\n])*')
  | (?P<num>\.?[0-9](?:[eEpP][+-]|[A-Za-z0-9_.])*)
  | (?P<id>[A-Za-z_$][A-Za-z0-9_$]*)
  | (?P<punct>\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=|\#\#|.)
""", re.X | re.S)
_LINEMARKER = re.compile(r'^#\s*(?:line\s+)?(\d+)\s+"((?:\\.|[^"\\])*)"')
BASIC = {"signed", "unsigned", "char", "short", "int", "long", "float", "double", "void", "_Bool"}
QUALS = {"const", "volatile"}


def canon_basic(words):
    """One spelling for a run of basic type keywords (None if not a valid type)."""
    w = list(words)
    sign = "u" if "unsigned" in w else ""
    w = [x for x in w if x not in ("signed", "unsigned")]
    n_long = w.count("long")
    rest = sorted(x for x in w if x not in ("long", "int"))
    if rest == ["char"] and n_long == 0:
        if "int" in w:
            return None
        if sign:
            return "$uchar"
        return "$schar" if "signed" in words else "$char"
    if rest == ["short"] and n_long == 0:
        return f"${sign}short"
    if rest == ["double"] and n_long <= 1 and "int" not in w and not sign:
        return "$ldouble" if n_long else "$double"
    if rest in (["float"], ["void"], ["_Bool"]) and n_long == 0 and "int" not in w and not sign:
        return "$" + rest[0]
    if rest == []:
        if n_long == 0:
            return f"${sign}int"
        if n_long == 1:
            return f"${sign}long"
        if n_long == 2:
            return f"${sign}llong"
    return None


class Tok:
    __slots__ = ("text", "file", "line")

    def __init__(self, text, file, line):
        self.text, self.file, self.line = text, file, line


def lex(pre, rel_of):
    """Tokens of `gcc -E` output with the (file, line) each came from."""
    toks = []
    file, line = "?", 1
    for raw in pre.split("\n"):
        m = _LINEMARKER.match(raw)
        if m:
            file, line = rel_of(m.group(2)), int(m.group(1))
            continue
        if raw.lstrip().startswith("#"):   # #pragma and friends: keep as one token
            toks.append(Tok(raw.strip(), file, line))
            line += 1
            continue
        for m in _TOKEN.finditer(raw):
            kind = m.lastgroup
            if kind in ("ws", "nl"):
                continue
            toks.append(Tok(m.group(0), file, line))
        line += 1
    return toks


def normalize(toks):
    """Basic-type runs canonicalized, simple typedefs expanded; returns (tokens, typedef table)."""
    out = []
    i, n = 0, len(toks)
    while i < n:   # pass 1: basic type keyword runs
        if toks[i].text in BASIC:
            j = i
            while j < n and toks[j].text in BASIC:
                j += 1
            c = canon_basic([t.text for t in toks[i:j]])
            if c is None:
                out.extend(toks[i:j])
            else:
                out.append(Tok(c, toks[i].file, toks[i].line))
            i = j
        else:
            out.append(toks[i])
            i += 1
    toks = out
    aliases = {}      # name -> canonical token list
    decls = {}        # name -> [canonical, ...] in declaration order
    res = []
    depth = 0
    i, n = 0, len(toks)
    while i < n:
        t = toks[i]
        if depth == 0 and t.text == "typedef":
            parsed = simple_typedef(toks, i, aliases)
            if parsed:
                end, names, canon = parsed
                for name in names:
                    decls.setdefault(name, []).append(" ".join(canon))
                    aliases[name] = canon
                i = end
                continue
        if t.text == "{":
            depth += 1
        elif t.text == "}":
            depth -= 1
        prev = res[-1].text if res else ""
        if t.text in aliases and prev not in (".", "->", "struct", "union", "enum", "goto"):
            res.extend(Tok(c, t.file, t.line) for c in aliases[t.text])
        else:
            res.append(t)
        i += 1
    return res, decls


def simple_typedef(toks, i, aliases):
    """`typedef [quals] TYPE [quals] NAME {, NAME} ;` where TYPE is a canonical basic type or an alias."""
    j = i + 1
    quals = []
    base = None
    n = len(toks)
    while j < n:
        x = toks[j].text
        if x in QUALS:
            quals.append(x)
        elif base is None and x.startswith("$"):
            base = [x]
        elif base is None and x in aliases:
            base = list(aliases[x])
        else:
            break
        j += 1
    if base is None:
        return None
    names = []
    while j < n and re.match(r"^[A-Za-z_]\w*$", toks[j].text) and toks[j].text not in QUALS:
        names.append(toks[j].text)
        j += 1
        if j < n and toks[j].text == ",":
            j += 1
            continue
        break
    if not names or j >= n or toks[j].text != ";":
        return None
    canon = sorted(set(quals) | {q for q in base if q in QUALS}) + [b for b in base if b not in QUALS]
    return j + 1, names, canon


def function_bodies(toks):
    """(start, end) index ranges of depth-0 brace blocks that follow `)` (function definitions)."""
    spans, depth, start = [], 0, None
    for k, t in enumerate(toks):
        if t.text == "{":
            if depth == 0 and k and toks[k - 1].text == ")":
                start = k
            depth += 1
        elif t.text == "}":
            depth -= 1
            if depth == 0 and start is not None:
                spans.append((start, k))
                start = None
    return spans


def preprocess(path, cwd, flags):
    cmd = ["gcc", "-E", "-undef", "-nostdinc", "-std=gnu89", "-x", "c", *IDO_DEFINES, *flags, str(path)]
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip().splitlines()[-1] if r.stderr.strip() else "cpp failed")
    return r.stdout


def git(*args, binary=False):
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.decode(errors="replace").strip())
    return r.stdout if binary else r.stdout.decode(errors="replace")


def compare(rel, base_root, flags):
    """Compare one .c file at base (under base_root) and in the working tree. Returns (ok, verdict, detail)."""
    def rel_of(root):
        root = str(root).rstrip("/") + "/"
        return lambda f: f[len(root):] if f.startswith(root) else f
    base_path = base_root / rel
    if not base_path.exists():
        return True, "new file", "not at base: nothing for IDO to compare"
    if not (ROOT / rel).exists():
        return True, "deleted", ""
    try:
        a = lex(preprocess(base_path, base_root, flags), rel_of(base_root))
        b = lex(preprocess(ROOT / rel, ROOT, flags), rel_of(ROOT))
    except RuntimeError as e:
        return False, "FAIL", f"preprocessor: {e}"
    na, da = normalize(a)
    nb, db = normalize(b)
    for name, canons in db.items():
        before = da.get(name, [])
        if len(canons) > max(1, len(before)):
            return False, "FAIL", f"typedef {name} declared {len(canons)} times"
        if before and before[-1] != canons[-1]:
            return False, "FAIL", f"typedef {name}: {before[-1]} -> {canons[-1]}"
    new_aliases = sorted(set(db) - set(da))
    ta, tb = [t.text for t in na], [t.text for t in nb]
    if ta != tb:
        k = next((k for k in range(min(len(ta), len(tb))) if ta[k] != tb[k]), min(len(ta), len(tb)))
        ctx = lambda ts, k: " ".join(t.text for t in ts[max(0, k - 6):k + 6])  # noqa: E731
        wa = f"{na[k].file}:{na[k].line}" if k < len(na) else "end"
        wb = f"{nb[k].file}:{nb[k].line}" if k < len(nb) else "end"
        return False, "FAIL", f"tokens differ at base {wa} / new {wb}\n    base: {ctx(na, k)}\n    new:  {ctx(nb, k)}"
    # lines
    if all(x.file == y.file and x.line == y.line for x, y in zip(na, nb)):
        return True, "exact", ("new typedef names: " + ", ".join(new_aliases)) if new_aliases else ""
    for k in range(len(na)):
        if na[k].file != nb[k].file:
            return False, "FAIL", f"token moved between files at {nb[k].file}:{nb[k].line}"
        if k and na[k].file == na[k - 1].file:
            sa = (na[k].line > na[k - 1].line) - (na[k].line < na[k - 1].line)
            sb = (nb[k].line > nb[k - 1].line) - (nb[k].line < nb[k - 1].line)
            if sa != sb:
                return False, "FAIL", (f"line structure changed near base {na[k].file}:{na[k].line} / "
                                       f"new {nb[k].line}")
    deltas = set()
    for s, e in function_bodies(na):
        d = {nb[k].line - na[k].line for k in range(s, e + 1)}
        if len(d) != 1:
            return False, "FAIL", f"lines inside a function moved (base {na[s].file}:{na[s].line}): {sorted(d)}"
        deltas |= d
    tail = ("; new typedef names: " + ", ".join(new_aliases)) if new_aliases else ""
    if deltas <= {0}:
        return True, "code-exact", "declarations outside functions moved; every function on its lines" + tail
    return True, "shifted", "function bodies shifted by " + ", ".join(f"{d:+d}" for d in sorted(deltas)) + tail


def text_checks(rel, base_text, new_text):
    """Added conditionals must test TARGET_PC; no new // comments in game files."""
    problems = []
    base_lines = set(base_text.splitlines()) if base_text is not None else set()
    for line in new_text.splitlines():
        if line in base_lines:
            continue
        s = line.strip()
        if re.match(r"#\s*(if|ifdef|ifndef|elif)\b", s) and "TARGET_PC" not in s \
                and not re.match(r"#\s*ifndef\s+\w+_H\b", s):
            problems.append(f"added conditional without TARGET_PC: {s}")
        if rel.startswith(GAME_DIRS) and re.search(r'^(?:[^"/]|"(?:\\.|[^"\\])*"|/(?![/*]))*//', line):
            problems.append(f"added // comment (IDO without -Xcpluscomm): {s[:80]}")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", default="HEAD", help="git revision to compare against (default HEAD)")
    ap.add_argument("-q", "--quiet", action="store_true", help="print failures and the summary only")
    ap.add_argument("files", nargs="*", type=Path)
    a = ap.parse_args()
    if a.files:
        files = [str(Path(f).resolve().relative_to(ROOT)) for f in a.files]
    else:
        out = git("diff", "--name-only", a.base, "--", "src", "include")
        files = [f for f in out.split() if f.endswith((".c", ".h"))]
    files = sorted(set(files))
    if not files:
        print("n64_neutral: no changed files")
        return 0
    tmp = Path(tempfile.mkdtemp(prefix="n64neutral-"))
    try:
        # base tree: every header src/rom can include, and each changed file
        tar = git("archive", "--format=tar", a.base, "include", "src/rom", binary=True)
        with tarfile.open(fileobj=io.BytesIO(tar)) as tf:
            tf.extractall(tmp)
        targets = []
        headers = [f for f in files if f.endswith(".h")]
        for f in files:
            if f.endswith(".c"):
                targets.append(f)
                try:
                    data = git("show", f"{a.base}:{f}", binary=True)
                except RuntimeError:
                    continue
                (tmp / f).parent.mkdir(parents=True, exist_ok=True)
                (tmp / f).write_bytes(data)
        if headers:
            targets += sorted(str(p.relative_to(ROOT)) for p in (ROOT / "src/rom").glob("*.c"))
        targets = sorted(set(targets))
        counts = {"exact": 0, "code-exact": 0, "shifted": 0, "new file": 0, "deleted": 0, "FAIL": 0}
        failed = []
        for f in targets:
            flags = ROM_FLAGS if f.startswith("src/rom/") else []
            ok, verdict, detail = compare(f, tmp, flags)
            probs = []
            if f in files and (ROOT / f).exists():
                base_file = tmp / f
                probs = text_checks(f, base_file.read_text(errors="replace") if base_file.exists() else None,
                                    (ROOT / f).read_text(errors="replace"))
            if probs:
                ok, verdict = False, "FAIL"
                detail = "; ".join(probs + ([detail] if detail else []))
            counts[verdict] += 1
            if not ok:
                failed.append(f)
            if not ok or not a.quiet:
                print(f"{verdict:8} {f}" + (f"  ({detail})" if detail else ""))
        for h in headers:
            probs = text_checks(h, (tmp / h).read_text(errors="replace") if (tmp / h).exists() else None,
                                (ROOT / h).read_text(errors="replace") if (ROOT / h).exists() else "")
            for p in probs:
                print(f"FAIL     {h}  ({p})")
                failed.append(h)
        summary = ", ".join(f"{v} {k}" for k, v in counts.items() if v)
        print(f"n64_neutral: {len(targets)} files vs {a.base}: {summary}"
              + (f"; headers checked through src/rom: {', '.join(headers)}" if headers else ""))
        return 1 if failed else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
