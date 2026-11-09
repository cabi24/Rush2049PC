#!/usr/bin/env python3
"""One spelling per constant: each meaning one macro name, defined once, in the header it belongs to.

    python3 tools/port/one_spelling.py --dry-run          what would change, per file, and every refusal
    python3 tools/port/one_spelling.py --apply            change the tree
    python3 tools/port/one_spelling.py --census           the macro names still #defined in more than one place
    python3 tools/port/one_spelling.py --check            exit 1 if --apply would change a file (ctest one_spelling)
    options: [--root DIR] [--tables DIR] [--only PASS[,PASS...]] (passes: spell, inline, dedupe, literals) [--names REGEX] [-v]

Wave 2b's area agents each named the constants of their own files, so a meaning was often defined in several files,
sometimes under several names (gGameState's bits as GAME_STATE_*, GS_* and GS_RACE_*). The shared definitions are
in the subsystem headers (include/game/*.h, the boot segment's src/rom/rom_tu.h and the sound library's header);
this tool brings the files to them. It is re-runnable: run it again after merging work that still spells a constant
the old way or keeps a file's own copy, and it changes only what is left. Three passes, in this order:

1. spell (tools/port/consts/spellings.tsv: `old  new  old_value  note`): another name for a meaning becomes the
   meaning's name, as a whole identifier token, in code and comments, in src/, include/, nonmatching/ and the
   area pages (docs/code/*.md but README.md). A file's own `#define old ...` becomes `#define
   new ...` (pass 3 then drops it as a copy). In the docs only a name with one row, an underscore (or MAT3's kName
   form) and no inlined namesake is renamed. The rename is checked per C file: the value `old` has there (the
   file's own definition, else the table's old_value, which was the value of the header definition the old name
   had) must be the value `new` has in the file's headers, or the file is left and reported. An old name may have
   rows for several values (a name two files gave two meanings): the row whose value is the file's is used.
2. inline (tools/port/consts/inline.tsv: `name  body_regex  comment_name  note`): a file's own abbreviation of a
   shared conversion (`#define N64W(x) PC_N64W(x)`, `#define N64PTR(T, a) ((T)PC_N64P(a))`) or an alias of a PC
   name (`#define LINK_SLOTS PC_MAX_LINKS`) goes, and each use is replaced by the definition's body with the use's
   arguments, token for token what the preprocessor made of it, so the code is the same (`N64PTR(Foo *, p)` becomes
   `((Foo *)PC_N64P(p))`; a body that ends its statement loses its `;` before the use's own). Only a definition whose body
   matches the row's regex is inlined (the typed `_Generic` N64P of a file's own N64-address fields is a converter
   of its own and stays); a definition in a header is inlined in every file that includes the header. In comments
   the name becomes comment_name (`-` leaves comments alone).
3. dedupe: a file's own `#define NAME body` goes when a header the file includes (the include closure: quoted
   includes from the file's folder, include/ and include/PR, and the prelude every game file gets) defines NAME
   (or declares NAME as an enumerator) with the same value: integer expressions are evaluated with the macros and
   enumerators the file sees (and a literal's type counted: 8 and 8U differ), anything else compared token for
   token. A copy with another value is reported, never dropped (two meanings under one name, or a bug). A copy
   inside a conditional block (other than the header guard), or before the file's first #include, is left. The
   files are the C files and the headers other than the homes (include/game, libultra's include/PR): the boot
   segment's rom_tu.h loses what the libultra headers it includes define.

4. literals (tools/port/consts/literals.tsv: `pattern  names  note`): a number where the code tests or sets a datum
   whose values have names (`gGameState & 0x100`) becomes the name the file's headers give that value (GS_CAR), or
   its bits' names ((GS_RACING | GS_DEMO)) when each bit has one; a number with an unnamed bit stays. pattern is a
   regex whose group `lit` is the number; names a regex of the names that may stand for it. A trailing comment that
   only named the values (`/* GS_RACING; kPractice */`) goes once the line's code says them.

Removing lines: a definition's continuation lines (a backslash, or a comment that goes on below it) go with it; an
#undef of the name just above it goes too; a comment block directly above a run of definitions that all went
(up to a blank line) goes with them, unless it starts the file; blank lines left doubled are made one.

Proof that a pass changes no code: tools/port/objsame.py against a build of the tree before it (every unit `same`).
--census prints, for each macro name #defined in more than one place under src/, include/ and nonmatching/, its
places, values and whether the values agree: what is left for a hand decision. Stdlib only, Python 3.9+.
"""
import argparse
import collections
import fnmatch
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONSTS = Path(__file__).resolve().parent / "consts"
C_DIRS = ("src", "include", "nonmatching")
C_SUFFIXES = (".c", ".h", ".inc")
DOC_GLOBS = ("docs/code/*.md",)
DOC_EXCLUDE = ("docs/code/README.md",)
INCLUDE_DIRS = ("include", "include/PR")
PRELUDE = "port/include/pc_prelude.h"   # every game unit gets it (CMakeLists.txt: -include)

IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
DEFINE = re.compile(r"^(\s*#\s*define\s+)([A-Za-z_][A-Za-z0-9_]*)(\([^)]*\))?(.*)$")
UNDEF = re.compile(r"^\s*#\s*undef\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:/\*.*\*/\s*)?$")
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
COND_OPEN = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
COND_ELSE = re.compile(r"^\s*#\s*(else|elif)\b")
COND_CLOSE = re.compile(r"^\s*#\s*endif\b")


# --------------------------------------------------------------------------- C text: code, comments, strings


def segments(text):
    """Split C text into (kind, text) runs: kind is 'code', 'comment', 'string' or 'char'."""
    out = []
    i, n, start = 0, len(text), 0
    kind = "code"
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] in "*/":
            if i > start:
                out.append(("code", text[start:i]))
            if text[i + 1] == "*":
                j = text.find("*/", i + 2)
                j = n if j < 0 else j + 2
            else:
                j = text.find("\n", i + 2)
                j = n if j < 0 else j
            out.append(("comment", text[i:j]))
            i = start = j
            continue
        if c in "\"'":
            if i > start:
                out.append(("code", text[start:i]))
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(("string" if c == '"' else "char", text[i:j]))
            i = start = j
            continue
        i += 1
    if start < n:
        out.append((kind, text[start:]))
    return out


def strip_comments(s):
    """A one-line body without its comments (one left open, to go on below, included)."""
    s = re.sub(r"/\*.*?\*/", " ", s)
    return re.sub(r"(/\*|//).*", "", s).strip()


def sub_tokens(text, mapping, in_comments=None):
    """Rename whole identifier tokens of C text: in code by mapping, in comments by in_comments (None: same)."""
    if in_comments is None:
        in_comments = mapping
    out = []
    for kind, t in segments(text):
        m = mapping if kind == "code" else in_comments if kind == "comment" else None
        if m:
            t = IDENT.sub(lambda g: m.get(g.group(0), g.group(0)), t)
        out.append(t)
    return "".join(out)


# --------------------------------------------------------------------------- definitions in a file


class Define:
    """One #define: its lines [first, last] (continuations and a trailing comment block included)."""

    def __init__(self, name, params, body, first, last, depth, before_include):
        self.name, self.params, self.body = name, params, body
        self.first, self.last, self.depth, self.before_include = first, last, depth, before_include

    @property
    def value(self):
        return norm(self.body)


def norm(body):
    """A body's tokens, comments gone, whitespace one space, outer parentheses that wrap it all removed."""
    v = re.sub(r"\s+", " ", strip_comments(body)).strip()
    while v.startswith("(") and v.endswith(")") and balanced(v[1:-1]):
        v = v[1:-1].strip()
    return v


def balanced(s):
    d = 0
    for ch in s:
        d += ch == "("
        d -= ch == ")"
        if d < 0:
            return False
    return d == 0


def parse_defines(lines, is_header):
    """Every #define of a file, with its line span, its conditional depth and whether it comes before an #include."""
    defs = []
    depth = 0
    guard = None
    seen_include = False
    i = 0
    first_cond = True
    while i < len(lines):
        line = lines[i]
        if INCLUDE.match(line):
            seen_include = True
        m = COND_OPEN.match(line)
        if m:
            if is_header and first_cond and m.group(1) == "ifndef":
                guard = line.split()[-1] if len(line.split()) > 1 else None
                first_cond = False
                depth += 0   # the header guard does not count
                i += 1
                continue
            first_cond = False
            depth += 1
            i += 1
            continue
        if COND_CLOSE.match(line):
            depth = max(depth - 1, 0) if depth else 0
            i += 1
            continue
        m = DEFINE.match(line)
        if m:
            name, params, rest = m.group(2), m.group(3), m.group(4)
            j = i
            body = rest
            while body.rstrip().endswith("\\") and j + 1 < len(lines):
                j += 1
                body = body.rstrip()[:-1] + " " + lines[j]
            # a trailing block comment that goes on below the definition
            tail = re.sub(r"/\*.*?\*/", "", body)
            if "/*" in tail:
                while j + 1 < len(lines) and "*/" not in lines[j]:
                    j += 1
                if "*/" not in lines[j] and j + 1 < len(lines):
                    j += 1
            if name != guard:
                defs.append(Define(name, params, body, i, j, depth, not seen_include))
            i = j + 1
            continue
        i += 1
    return defs


# --------------------------------------------------------------------------- values


INT_LIT = re.compile(r"^(0[xX][0-9A-Fa-f]+|\d+)([uUlL]*)$")
FLOAT_LIT = re.compile(r"^(\d+\.\d*([eE][-+]?\d+)?|\.\d+([eE][-+]?\d+)?|\d+[eE][-+]?\d+)([fFlL]?)$")
TOKEN = re.compile(r"0[xX][0-9A-Fa-f]+[uUlL]*|\d+\.\d*(?:[eE][-+]?\d+)?[fFlL]?|\.\d+(?:[eE][-+]?\d+)?[fFlL]?|"
                   r"\d+[eE][-+]?\d+[fFlL]?|\d+[uUlL]*|[A-Za-z_]\w*|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^~!()<>?:]")


class Unknown(Exception):
    pass


def value_of(body, env, seen=()):
    """The value of an object-like macro body: ('int', value, unsigned, long) or ('float', value, suffix), or the
    normalized token string when it is not an arithmetic constant."""
    text = norm(body)
    try:
        return evaluate(text, env, seen)
    except Unknown:
        return ("text", re.sub(r"\s+", "", text))


def evaluate(text, env, seen):
    toks = TOKEN.findall(text)
    if "".join(toks) != re.sub(r"\s+", "", text):
        raise Unknown(text)
    py = []
    flags = {"unsigned": False, "long": False, "float": None}
    for t in toks:
        m = INT_LIT.match(t)
        if m:
            digits, suf = m.groups()
            v = int(digits, 0) if not (len(digits) > 1 and digits[0] == "0" and digits[1] not in "xX") else int(digits, 8)
            u = "u" in suf.lower() or (digits.lower().startswith("0x") and 0x7FFFFFFF < v <= 0xFFFFFFFF)
            flags["unsigned"] |= u
            flags["long"] |= "l" in suf.lower() or v > 0xFFFFFFFF
            py.append(str(v))
            continue
        m = FLOAT_LIT.match(t)
        if m:
            suf = m.group(4).lower()
            if flags["float"] not in (None, suf):
                raise Unknown(text)
            flags["float"] = suf
            py.append(repr(float(m.group(1))))
            continue
        if re.match(r"[A-Za-z_]", t):
            if t in seen or t not in env:
                raise Unknown(text)
            sub = env[t]
            if callable(sub):
                raise Unknown(text)
            v = value_of(sub, env, seen + (t,)) if isinstance(sub, str) else sub
            if v[0] == "int":
                flags["unsigned"] |= v[2]
                flags["long"] |= v[3]
                py.append(f"({v[1]})")
            elif v[0] == "float":
                if flags["float"] not in (None, v[2]):
                    raise Unknown(text)
                flags["float"] = v[2]
                py.append(f"({v[1]!r})")
            else:
                raise Unknown(text)
            continue
        py.append({"&&": " and ", "||": " or ", "!": " not ", "/": "//"}.get(t, t))
    expr = "".join(py)
    if "?" in expr or ":" in expr:
        raise Unknown(text)
    if flags["float"] is not None:
        expr = expr.replace("//", "/")
    try:
        v = eval(expr, {"__builtins__": {}}, {})   # noqa: S307 - digits and operators only
    except Exception:
        raise Unknown(text)
    if flags["float"] is not None:
        return ("float", float(v), flags["float"])
    if isinstance(v, bool):
        v = int(v)
    if not isinstance(v, int):
        raise Unknown(text)
    if flags["unsigned"]:
        v &= 0xFFFFFFFFFFFFFFFF if flags["long"] else 0xFFFFFFFF
    return ("int", v, flags["unsigned"], flags["long"])


ENUM = re.compile(r"\benum\s*(\w+)?\s*\{([^{}]*)\}", re.S)


def enumerators(text):
    """name -> value of every enumerator in text whose value is a plain constant or follows one."""
    out = {}
    for m in ENUM.finditer(strip_block_comments(text)):
        nxt = 0
        for part in m.group(2).split(","):
            part = part.strip()
            if not part:
                continue
            if "=" in part:
                name, expr = (x.strip() for x in part.split("=", 1))
                try:
                    v = evaluate(norm(expr), {**out}, ())
                except Unknown:
                    nxt = None
                    continue
                if v[0] != "int":
                    nxt = None
                    continue
                out[name] = ("int", v[1], False, False)
                nxt = v[1] + 1
            elif nxt is not None and IDENT.fullmatch(part):
                out[part] = ("int", nxt, False, False)
                nxt += 1
    return out


def strip_block_comments(text):
    return re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", " ", text, flags=re.S))


# --------------------------------------------------------------------------- the tree


class Tree:
    def __init__(self, root):
        self.root = Path(root)
        self.text = {}
        for top in C_DIRS:
            for dirpath, dirnames, filenames in os.walk(self.root / top):
                dirnames.sort()
                for f in sorted(filenames):
                    if f.endswith(C_SUFFIXES):
                        p = Path(dirpath) / f
                        self.text[p.relative_to(self.root).as_posix()] = p.read_text(errors="surrogateescape")
        prelude = self.root / PRELUDE
        self.text[PRELUDE] = prelude.read_text(errors="surrogateescape")
        self._closure = {}
        self._defs = {}

    def resolve(self, rel, inc):
        here = Path(rel).parent
        for base in [here] + [Path(d) for d in INCLUDE_DIRS] + [Path("port/include")]:
            cand = os.path.normpath((base / inc).as_posix())
            if cand in self.text:
                return cand
            if (self.root / cand).is_file() and cand.startswith("port/"):
                self.text[cand] = (self.root / cand).read_text(errors="surrogateescape")
                return cand
        return None

    def includes(self, rel):
        out = []
        for line in self.text[rel].split("\n"):
            m = INCLUDE.match(line)
            if m:
                r = self.resolve(rel, m.group(1))
                if r:
                    out.append(r)
        return out

    def closure(self, rel):
        """The headers a file sees, in order (the prelude first for a game file), itself not included."""
        if rel in self._closure:
            return self._closure[rel]
        order, seen = [], {rel}
        start = [PRELUDE] if rel.endswith(".c") and not rel.startswith("port/") else []

        def walk(f):
            for h in (start if f == rel else []) + self.includes(f):
                if h not in seen:
                    seen.add(h)
                    walk(h)
                    order.append(h)
        walk(rel)
        self._closure[rel] = order
        return order

    def defines(self, rel):
        if rel not in self._defs:
            self._defs[rel] = parse_defines(self.text[rel].split("\n"), not rel.endswith(".c"))
        return self._defs[rel]

    def env(self, rel, upto=None):
        """Macros (name -> body, or a marker for function-like) and enumerators a file sees: its headers', then its
        own up to line `upto`."""
        env = {}
        for h in self.closure(rel):
            env.update(enumerators(self.text[h]))
            for d in self.defines(h):
                env[d.name] = d.body if not d.params else (lambda: None)
        env.update(enumerators(self.text[rel]))
        for d in self.defines(rel):
            if upto is not None and d.first >= upto:
                break
            env[d.name] = d.body if not d.params else (lambda: None)
        return env

    def header_defs(self, rel):
        """name -> (header, Define or enumerator value) for the definitions a file's headers make."""
        out = {}
        for h in self.closure(rel):
            for name, v in enumerators(self.text[h]).items():
                out.setdefault(name, (h, v))
            for d in self.defines(h):
                if d.depth == 0:
                    out.setdefault(d.name, (h, d))
        return out


# --------------------------------------------------------------------------- tables


def read_tsv(path, ncols):
    rows = []
    if not path.exists():
        return rows
    for ln, line in enumerate(path.read_text().split("\n"), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cols = line.split("\t")
        if cols[0] in ("old", "name", "pattern"):   # a header row
            continue
        cols += [""] * (ncols - len(cols))
        rows.append((ln, [c.strip() for c in cols[:ncols]]))
    return rows


# --------------------------------------------------------------------------- line removal


def remove_spans(lines, spans):
    """Remove the line spans (first, last); a comment block directly above a run of definitions that all go goes
    too (not the file's opening comment), and blank lines left doubled become one."""
    gone = set()
    for a, b in spans:
        gone.update(range(a, b + 1))
    if not gone:
        return lines

    def is_comment_only(k):
        s = lines[k].strip()
        return s.startswith("/*") or s.startswith("*") or s.startswith("//") or s.endswith("*/") and "/*" not in s \
            and not DEFINE.match(lines[k])

    def is_define_line(k):
        return k in gone or DEFINE.match(lines[k]) is not None

    # each run: consecutive lines that are definitions (or their spans) or comment-only lines
    k = 0
    n = len(lines)
    extra = set()
    while k < n:
        if k in gone:
            # find the run around k
            a = k
            while a - 1 >= 0 and lines[a - 1].strip() and (is_define_line(a - 1) or is_comment_only(a - 1)):
                a -= 1
            b = k
            while b + 1 < n and lines[b + 1].strip() and (is_define_line(b + 1) or is_comment_only(b + 1)):
                b += 1
            defs_in_run = [x for x in range(a, b + 1) if DEFINE.match(lines[x])]
            if defs_in_run and all(x in gone for x in defs_in_run):
                ends_clean = b + 1 >= n or not lines[b + 1].strip()
                starts_clean = a == 0 or not lines[a - 1].strip()
                if ends_clean and starts_clean and not (a == 0 and lines[0].lstrip().startswith("/*")):
                    extra.update(range(a, b + 1))
            k = b + 1
            continue
        k += 1
    # each comment block directly above a group of definitions (the definitions up to the next comment, blank line
    # or code) that all go, when the group ends at a blank line, another comment or the end
    starts = {d.first: d.last for d in parse_defines(lines, False)}
    in_span = {x for a, b in starts.items() for x in range(a, b + 1)}
    k = 0
    while k < n:
        if k not in in_span and lines[k].strip() and is_comment_only(k):
            a = k
            while k + 1 < n and k + 1 not in in_span and lines[k + 1].strip() and is_comment_only(k + 1):
                k += 1
            b = k
            j, group = b + 1, []
            while j < n and j in starts:
                group.append(j)
                j = starts[j] + 1
            ends_clean = j >= n or not lines[j].strip() or (j not in in_span and is_comment_only(j))
            if group and all(g in gone for g in group) and ends_clean and a > 0:
                extra.update(range(a, b + 1))
            k = max(j, b + 1)
            continue
        k += 1
    gone |= extra
    out = [l for i, l in enumerate(lines) if i not in gone]
    res = []
    for l in out:
        if not l.strip() and res and not res[-1].strip():
            continue
        res.append(l)
    return res


# --------------------------------------------------------------------------- the passes


def split_args(text, i):
    """text[i] is '(': the arguments up to the matching ')' and the index after it, or None."""
    depth, args, start, j = 0, [], i + 1, i
    n = len(text)
    while j < n:
        c = text[j]
        if c in "\"'":
            q = c
            j += 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                args.append(text[start:j])
                return [a.strip() for a in args], j + 1
        elif c == "," and depth == 1:
            args.append(text[start:j])
            start = j + 1
        j += 1
    return None


def expand_uses(text, name, params, body):
    """Replace each code use `name(args)` by body with the parameters substituted; comments and strings left."""
    out = []
    pat = re.compile(r"\b" + re.escape(name) + r"\s*\(")
    count = 0
    # work on the code only, keeping comments and strings
    segs = segments(text)
    # join code segments with placeholders for the others, so a call spanning a comment still parses
    holders = []
    joined = []
    for kind, t in segs:
        if kind == "code":
            joined.append(t)
        else:
            holders.append(t)
            joined.append(f"\x00{len(holders) - 1}\x00")
    code = "".join(joined)
    i = 0
    while True:
        m = pat.search(code, i)
        if not m:
            out.append(code[i:])
            break
        # not a definition of the name, not a member
        line_start = code.rfind("\n", 0, m.start()) + 1
        line_end = code.find("\n", m.start())
        dm = DEFINE.match(code[line_start:len(code) if line_end < 0 else line_end])
        if dm and dm.group(2) == name and m.start() - line_start == dm.start(2):
            out.append(code[i:m.end()])   # the definition itself
            i = m.end()
            continue
        prev = code[:m.start()].rstrip()
        if prev.endswith(".") or prev.endswith("->"):
            out.append(code[i:m.end()])
            i = m.end()
            continue
        r = split_args(code, m.end() - 1)
        if r is None:
            out.append(code[i:m.end()])
            i = m.end()
            continue
        args, after = r
        if params == [] and args == [""]:
            args = []
        if len(args) != len(params):
            raise ValueError(f"{name}: {len(args)} arguments for {len(params)}")
        sub = dict(zip(params, args))
        exp = IDENT.sub(lambda g: sub.get(g.group(0), g.group(0)), body)
        if exp.rstrip().endswith(";") and code[after:].lstrip(" \t").startswith(";"):
            exp = exp.rstrip()[:-1]   # `NAME();` of a body that ends its own statement: one semicolon
        out.append(code[i:m.start()])
        out.append(exp)
        count += 1
        i = after
    code = "".join(out)
    text = re.sub("\x00(\\d+)\x00", lambda g: holders[int(g.group(1))], code)
    return text, count


class Run:
    def __init__(self, root, verbose, names=None, consts=CONSTS):
        self.consts = Path(consts)
        self.names = re.compile(names) if names else None
        self.tree = Tree(root)
        self.root = Path(root)
        self.verbose = verbose
        self.changes = collections.Counter()
        self.refusals = []
        self.edited = set()

    def wanted(self, name):
        return self.names is None or self.names.fullmatch(name) is not None

    def say(self, msg):
        self.refusals.append(msg)

    def write(self, rel, text):
        if self.tree.text.get(rel) != text:
            self.tree.text[rel] = text
            self.tree._defs.pop(rel, None)
            self.tree._closure.clear()
            self.edited.add(rel)

    # ---- pass 1
    def spell(self):
        rows = read_tsv(self.consts / "spellings.tsv", 4)
        by_old = collections.defaultdict(list)
        for ln, (old, new, old_value, note) in rows:
            if self.wanted(old) or self.wanted(new):
                by_old[old].append((new, old_value, ln))
        if not by_old:
            return
        olds = set(by_old)
        inlined = {r[1][0] for r in read_tsv(self.consts / "inline.tsv", 4)}
        for rel in sorted(self.tree.text):
            if rel == PRELUDE:
                continue
            text = self.tree.text[rel]
            used = olds & set(IDENT.findall(text))
            if not used:
                continue
            env = self.tree.env(rel)
            hdr = self.tree.header_defs(rel)
            own = {d.name: d for d in self.tree.defines(rel)}
            mapping = {}
            for old in sorted(used):
                if old in own:
                    have = value_of(own[old].body, env)
                elif old in inlined:
                    continue   # a spelling with another meaning elsewhere (inline.tsv): only where it is defined
                else:
                    cands = {r[1] for r in by_old[old]}
                    if len(cands) != 1:
                        self.say(f"{rel}: {old} has rows for several values and no definition here: left")
                        continue
                    have = value_of(next(iter(cands)), env)
                row = None
                for new, old_value, ln in by_old[old]:
                    if value_of(old_value, env) == have or len(by_old[old]) == 1:
                        row = (new, old_value, ln)
                        break
                if row is None:
                    self.say(f"{rel}: {old} is {have} here; no row of spellings.tsv has that value: left")
                    continue
                new = row[0]
                if new in own and old not in own:
                    want = value_of(own[new].body, env)
                elif new in hdr:
                    h, d = hdr[new]
                    want = d if isinstance(d, tuple) else value_of(d.body, env)
                elif new in own:
                    want = value_of(own[new].body, env)
                else:
                    want = None
                if want is not None and want != have:
                    self.say(f"{rel}: {old} is {have} here but {new} is {want}: left")
                    continue
                if want is None and not rel.startswith("include/game/") and old not in own:
                    self.say(f"{rel}: {new} (for {old}) is defined nowhere it can see: left")
                    continue
                mapping[old] = new
            if mapping:
                self.write(rel, sub_tokens(text, mapping))
                for o in mapping:
                    self.changes[f"spell {o} -> {mapping[o]}"] += 1
        # the docs: the names as words
        # in prose only a name that can't be a word or another meaning's: one row, an underscore or MAT3's kName,
        # and no inlined abbreviation of the same spelling (a file's MAX_LINKS was LINK_OWNER_HUMAN, another's not)
        inlined = {r[1][0] for r in read_tsv(self.consts / "inline.tsv", 4)}
        simple = {}
        for old, rs in by_old.items():
            if len(rs) == 1 and old not in inlined and ("_" in old or re.fullmatch(r"k[A-Z]\w+", old)):
                simple[old] = rs[0][0]
        for pattern in DOC_GLOBS:
            for p in sorted(self.root.glob(pattern)):
                rel = p.relative_to(self.root).as_posix()
                if rel in DOC_EXCLUDE:
                    continue
                t = p.read_text()
                nt = IDENT.sub(lambda g: simple.get(g.group(0), g.group(0)), t)
                if nt != t:
                    self.tree.text[rel] = nt
                    self.edited.add(rel)
                    self.changes["spell (docs)"] += 1

    # ---- pass 2
    def inline(self):
        rows = read_tsv(self.consts / "inline.tsv", 4)
        if not rows:
            return
        for ln, (name, body_re, comment_name, note) in rows:
            if not self.wanted(name):
                continue
            rx = re.compile(body_re)
            # definitions to inline: file -> (params, body), and the files each serves
            for rel in sorted(self.tree.text):
                if rel == PRELUDE:
                    continue
                for d in self.tree.defines(rel):
                    if d.name != name:
                        continue
                    body = strip_comments(d.body).strip()
                    if not rx.fullmatch(body):
                        if self.verbose:
                            self.say(f"{rel}:{d.first + 1}: {name} is `{body}`, not an abbreviation: kept")
                        continue
                    if d.depth:
                        self.say(f"{rel}:{d.first + 1}: {name} inside a conditional block: kept")
                        continue
                    if d.params is None and not IDENT.fullmatch(body):
                        self.say(f"{rel}:{d.first + 1}: {name} is `{body}`: only an alias of a name is inlined: kept")
                        continue
                    params = None if d.params is None else [p.strip() for p in d.params[1:-1].split(",") if p.strip()]
                    users = [rel] if rel.endswith(".c") else [
                        f for f in sorted(self.tree.text) if f != rel and rel in self.tree.closure(f)
                        and not any(x.name == name for x in self.tree.defines(f))]
                    if not rel.endswith(".c"):
                        users = [rel] + users
                    for f in users:
                        t = self.tree.text[f]
                        try:
                            if params is None:   # an alias of another name: the name's tokens in code
                                nt = sub_tokens(t, {name: body}, {})   # its definition becomes `body body`
                                k = t.count(name)
                            else:
                                nt, k = expand_uses(t, name, params, body)
                        except ValueError as e:
                            self.say(f"{f}: {e}: left")
                            continue
                        if comment_name and comment_name != "-":
                            nt = sub_tokens(nt, {}, {name: comment_name})
                        if nt != t:
                            self.write(f, nt)
                            self.changes[f"inline {name}"] += k
                    # the definition itself
                    lines = self.tree.text[rel].split("\n")
                    dd = [x for x in parse_defines(lines, not rel.endswith(".c"))
                          if (x.name == name or (params is None and x.name == body))
                          and (x.params is None) == (params is None)]
                    spans = [(x.first, x.last) for x in dd if rx.fullmatch(strip_comments(x.body).strip())]
                    spans += [(k, k) for x in dd for k in [x.first - 1]
                              if k >= 0 and UNDEF.match(lines[k]) and UNDEF.match(lines[k]).group(1) == name]
                    self.write(rel, "\n".join(remove_spans(lines, spans)))
                    break

    # ---- pass 3
    def dedupe(self):
        for rel in sorted(self.tree.text):
            # a file's copies of what its includes define; not the canonical homes (include/game, libultra's
            # include/PR: their duplicates of each other are a hand decision, --census) or the PC layer
            if rel.startswith(("port/", "include/game/", "include/PR/")) or rel == PRELUDE:
                continue
            defs = [d for d in self.tree.defines(rel) if self.wanted(d.name)]
            if not defs:
                continue
            hdr = self.tree.header_defs(rel)
            if not hdr:
                continue
            lines = self.tree.text[rel].split("\n")
            spans = []
            for d in defs:
                if d.name not in hdr:
                    continue
                h, hd = hdr[d.name]
                if d.depth or d.before_include:
                    self.say(f"{rel}:{d.first + 1}: {d.name} (also in {h}) is inside a conditional block or before "
                             f"the includes: left")
                    continue
                if prior_undef(lines, d):
                    # an override on purpose: the file undefines the header's first
                    if isinstance(hd, Define) and same_def(self.tree, rel, d, h, hd):
                        spans.append((d.first - 1, d.first - 1))
                        spans.append((d.first, d.last))
                        self.changes["dedupe (with its #undef)"] += 1
                    continue
                if isinstance(hd, tuple):   # an enumerator
                    if d.params is None and value_of(d.body, self.tree.env(rel, d.first)) == hd:
                        spans.append((d.first, d.last))
                        self.changes[f"dedupe {d.name}"] += 1
                    else:
                        self.say(f"{rel}:{d.first + 1}: {d.name} is `{norm(d.body)}`, the enumerator in {h} is "
                                 f"{hd[1]}: left")
                    continue
                if same_def(self.tree, rel, d, h, hd):
                    spans.append((d.first, d.last))
                    self.changes[f"dedupe {d.name}"] += 1
                else:
                    self.say(f"{rel}:{d.first + 1}: {d.name}{d.params or ''} is `{norm(d.body)}`, {h} has "
                             f"`{norm(hd.body)}`: left")
            if spans:
                self.write(rel, "\n".join(remove_spans(lines, spans)))

    # ---- pass 4
    def literals(self):
        """A number where the code tests or sets a datum whose values have names (literals.tsv) becomes the name."""
        rows = read_tsv(self.consts / "literals.tsv", 3)
        if not rows:
            return
        said = re.compile("|".join(f"(?:{r[1][1]})" for r in rows))   # every row's names, for the comments
        for ln, (pattern, names_re, note) in rows:
            rx = re.compile(pattern)
            nx = re.compile(names_re)
            for rel in sorted(self.tree.text):
                if not rel.endswith(".c") or not rel.startswith(("src/", "nonmatching/")):
                    continue
                text = self.tree.text[rel]
                if not rx.search(text):
                    nt = drop_said_comments(text, said)
                    if nt != text:
                        self.write(rel, nt)
                        self.changes[f"literal comments ({note})"] += 1
                    continue
                env = self.tree.env(rel)
                named = {}
                for name, (h, d) in self.tree.header_defs(rel).items():
                    if not nx.fullmatch(name):
                        continue
                    v = d if isinstance(d, tuple) else value_of(d.body, env)
                    if v[0] == "int" and not v[2]:
                        named.setdefault(v[1], name)
                out = []
                count = 0
                for kind, seg in segments(text):
                    if kind != "code":
                        out.append(seg)
                        continue

                    def put(m):
                        nonlocal count
                        lit = m.group("lit")
                        v = int(lit, 0)
                        name = spell_value(v, named)
                        if name is None:
                            return m.group(0)
                        count += 1
                        a, b = m.span("lit")
                        return m.group(0)[:a - m.start()] + name + m.group(0)[b - m.start():]
                    out.append(rx.sub(put, seg))
                nt = drop_said_comments("".join(out), said)
                if nt != text:
                    self.write(rel, nt)
                    self.changes[f"literal ({note})"] += count

    def save(self):
        for rel in sorted(self.edited):
            (self.root / rel).write_text(self.tree.text[rel], errors="surrogateescape")


SAID = re.compile(r"[ \t]*/\*\s*([A-Za-z_][A-Za-z0-9_]*(?:\s*[,;|]\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*\*/[ \t]*$")


def drop_said_comments(text, nx):
    """A line's trailing comment that only names values (`/* GS_RACING | GS_DEMO; kPractice */`, left to explain the
    numbers) goes once the line's code says each of them."""
    out = []
    for line in text.split("\n"):
        m = SAID.search(line)
        if m:
            names = re.findall(r"[A-Za-z_][A-Za-z0-9_]*", m.group(1))
            code = line[:m.start()]
            if code.strip() and all(nx.fullmatch(n) for n in names) and \
                    all(re.search(r"\b" + n + r"\b", code) for n in names):
                line = code.rstrip()
        out.append(line)
    return "\n".join(out)


def spell_value(v, named):
    """The name of value v, or of each of its bits ((A | B)) when every bit has one; None if some bit has none."""
    if v in named:
        return named[v]
    bits = [1 << i for i in range(32) if v >> i & 1]
    if len(bits) < 2 or any(b not in named for b in bits):
        return None
    return "(" + " | ".join(named[b] for b in reversed(bits)) + ")"


def prior_undef(lines, d):
    k = d.first - 1
    return k >= 0 and UNDEF.match(lines[k]) is not None and UNDEF.match(lines[k]).group(1) == d.name


def same_def(tree, rel, d, h, hd):
    if (d.params is None) != (hd.params is None):
        return False
    if d.params is not None:
        pa = [p.strip() for p in d.params[1:-1].split(",")]
        pb = [p.strip() for p in hd.params[1:-1].split(",")]
        if len(pa) != len(pb):
            return False
        ren = dict(zip(pa, pb))
        a = IDENT.sub(lambda g: ren.get(g.group(0), g.group(0)), norm(d.body))
        return re.sub(r"\s+", "", a) == re.sub(r"\s+", "", norm(hd.body))
    env_here = tree.env(rel, d.first)
    env_there = tree.env(h)
    return value_of(d.body, env_here) == value_of(hd.body, env_there)


# --------------------------------------------------------------------------- census


def census(root):
    tree = Tree(root)
    places = collections.defaultdict(list)
    for rel in sorted(tree.text):
        if rel == PRELUDE:
            continue
        for d in tree.defines(rel):
            if d.name[0].isupper():
                places[d.name].append((rel, d.first + 1, (d.params or "") + norm(d.body)))
    dup = {n: p for n, p in places.items() if len(p) > 1}
    for n in sorted(dup, key=lambda n: (-len(dup[n]), n)):
        vals = {v for _, _, v in dup[n]}
        print(f"{n}\t{len(dup[n])}\t{'same' if len(vals) == 1 else 'differ'}")
        for rel, ln, v in dup[n]:
            print(f"    {rel}:{ln}\t{v}")
    print(f"# {len(dup)} names defined in more than one place ({sum(len(p) for p in dup.values())} definitions)",
          file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dry-run", action="store_true")
    g.add_argument("--apply", action="store_true")
    g.add_argument("--census", action="store_true")
    g.add_argument("--check", action="store_true", help="exit 1 if the passes would change a file")
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--tables", default=str(CONSTS), help="the tables' folder (default tools/port/consts)")
    ap.add_argument("--only", default="spell,inline,dedupe,literals")
    ap.add_argument("--names", help="only the names this regex matches (whole name)")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    if a.census:
        census(a.root)
        return 0
    run = Run(a.root, a.verbose, a.names, a.tables)
    passes = [p.strip() for p in a.only.split(",") if p.strip()]
    for p in ("spell", "inline", "dedupe", "literals"):
        if p in passes:
            getattr(run, p)()
    for k, v in sorted(run.changes.items()):
        print(f"{v:6d}  {k}")
    print(f"{len(run.edited)} files {'changed' if a.apply else 'would change'}")
    for r in run.refusals:
        print(f"left: {r}")
    if a.apply:
        run.save()
    else:
        for rel in sorted(run.edited):
            print(f"  {rel}")
    if a.check and run.edited:
        print("one_spelling: the tree has constants the tables would change: run --apply", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
