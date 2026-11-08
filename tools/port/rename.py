#!/usr/bin/env python3
"""Apply the name maps (tools/port/names/*.tsv, docs/port/PCONLY.md 3) to the whole tree.

    python3 tools/port/rename.py --dry-run [--report FILE]     what would change, per map row and per file
    python3 tools/port/rename.py --apply                       change the tree (git mv for files)
    python3 tools/port/rename.py --check                       exit 1 if an old name is still anywhere
    options: [MAP ...] (default: every tools/port/names/*.tsv) [--root DIR] [--exclude GLOB]... [--strings all]
             [--no-follow-files] [--no-names-check] [-v]

One pass reads every file of the tree (git's: tracked, and untracked but not ignored), splits it into identifier
tokens and looks each one up in the maps, so the cost doesn't grow with the number of rows. A name is matched as a
whole identifier token, never inside a longer one, in the spelling the map gives: another spelling of an address
name's hex digits (func_800a3424 for func_800A3424) is another C identifier, often declared with another prototype in
the same file, so it is reported and left (gen_stubs.py still links it to the renamed symbol by its address, and
--check finds it). Where a name lives, and what happens there:

- C (.c, .h, .inc anywhere): identifiers in the code; comments (whole words, but see paths below); string literals
  only where the string names a symbol: the strings of asm statements and of alias/weakref attributes (linker
  symbols: `.set D_x, gScheduler + 0x40`), and a literal that is exactly an old name (lookup tables and labels:
  {"func_800D6348", func_800D6348}, PC_SPIN("func_800E7038")). Any other string that mentions a name is reported
  and left (it is output; --strings all renames those too). `#include` paths follow file renames.
- The data loader's symbol tables (asm/us/*/symbols.json): their keys, the JSON rewritten in its own style.
- symbol_addrs.us.txt and every other text file (the tools' Python, the press scripts, port/data/*.txt such as
  extents.txt and decoupled.txt, the sanitizer suppressions, the linker scripts, CMake, the manifest and layout
  JSON, the docs' Markdown): whole identifier tokens.
- An address name's address: the tools take the address of a D_/func_XXXXXXXX name from its spelling when no
  table has it (n64syms.address_of, kind_of). For each renamed address name that symbol_addrs.us.txt doesn't define,
  rename.py adds `new = 0xADDRESS; // type:func|data` there, the kind its old spelling implied, so gen_data.py,
  gen_stubs.py and synth_rom.py resolve the new name exactly as the old one.
- Paths. A token inside a path (a run like src/blob/func_800A1234.c, or a tracked folder such as
  src/blob/groups/heap_release) is part of a file's name, not the symbol's: it changes only with the file. A file row
  moves its file (git mv) and rewrites every reference to its path, by its full path or any trailing part of it
  (blob/x.c, x.c), #include lines included; a reference whose trailing part matches several files that don't all
  get the same new name is reported and left. A func, data or type row whose old name is the stem of a file under
  src/ or nonmatching/ (src/ovl_a/func_8038F454.c) moves that file to the new stem as well (an implied file row;
  --no-follow-files turns that off), so a CMake list of stems (nonmatching/game/${f}.c) stays true. In CMake files
  an explicit file row's bare stem is renamed too.
- SHA256SUMS files list the checksums of the files next to them: a line for a file rename.py changes is updated.

Left, and said so per row: field rows (Struct.0xOFFSET: renaming a field needs the struct's layout, wave 2's hand
edit), strings that mention a name (above), paths into files that don't move (the N64 assembly, folders named
after a function), derived identifiers that contain an old address name (func_800D6E00_1, __standin_X) and names
built by token pasting (a ## b) that make an old name.

Never touched: the maps themselves, these tools' sources and their tests' fixtures, docs/port/PCONLY.md (its
examples) and the N64 assembly (asm/**/*.s: checksummed, the matching build's), plus --exclude globs.

--dry-run and --apply run names_check.py's checks on the maps first and stop on an error (--no-names-check skips
that). --apply twice changes nothing the second time. --check lists every old name still in the tree (all contexts,
strings and paths included) and every file row whose old file still exists. Stdlib only, Python 3.9+.
"""
import argparse
import collections
import fnmatch
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import names_check as nc  # noqa: E402

ROOT = nc.ROOT
DEFAULT_EXCLUDES = (
    "tools/port/names/*",             # the maps
    "tools/port/tests/names/*",       # the fixtures of these tools' tests (made-up names)
    "tools/port/rename.py", "tools/port/names_check.py", "tools/port/pconly_gate.sh",
    "tools/port/tests/test_rename.py", "tools/port/tests/test_names_check.py",
    "docs/port/PCONLY.md",            # the map format's examples
    "asm/*.s",                        # the N64 build's assembly (fnmatch's * crosses folders)
    "*SHA256SUMS",                    # updated by checksum, below
)
SYMBOL_FILE = "symbol_addrs.us.txt"
SYMBOL_TABLES = re.compile(r"^asm/us/[^/]+/symbols\.json$")
C_EXTS = nc.C_EXTS
# The extensions that make a token followed by `.ext` a file name.
PATH_EXTS = {"c", "h", "s", "inc", "o", "obj", "json", "txt", "md", "py", "ld", "press", "cmake", "png", "log", "sh",
             "yml", "yaml", "supp", "tsv", "wav", "z64", "bin", "hex", "mk", "rc", "bat", "d", "a", "elf", "map"}
PATH_CH = re.compile(r"[A-Za-z0-9_./\-]")
ASM_CONTEXT = {"asm", "__asm__", "__asm", "alias", "__alias__", "weakref", "__weakref__"}
ASM_QUALIFIERS = {"volatile", "__volatile__", "__volatile", "goto", "inline", "__inline__"}
ADDR_SEARCH = re.compile(r"(?:D|func|sym|jtbl)_[0-9A-Fa-f]{8}")
PASTE_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)((?:\\\r?\n|[^\n])*)", re.M)


def category_of(path):
    """The kind of file a path is, for the report."""
    p = Path(path)
    if p.suffix in C_EXTS:
        return "c"
    if SYMBOL_TABLES.match(path):
        return "symbol table"
    if p.suffix == ".md":
        return "docs"
    if p.suffix == ".py":
        return "tools"
    if p.name == "CMakeLists.txt" or p.suffix == ".cmake":
        return "cmake"
    return {".json": "json", ".supp": "sanitizer", ".ld": "linker script", ".press": "press"}.get(p.suffix, "text")


class Plan:
    """What the maps change in the tree: built once, then printed (--dry-run) or applied (--apply)."""

    def __init__(self, root, rows, files, follow_files=True, strings_all=False):
        self.root = Path(root)
        self.rows = rows
        self.files = files
        self.strings_all = strings_all
        self.errors = []
        self.ident = {nc.norm(r.old): r for r in rows if r.kind in nc.IDENT_KINDS}
        self.moves = {}            # old path -> (new path, row, implied)
        for r in rows:
            if r.kind == "file":
                self.moves[r.old.replace("\\", "/")] = (r.new.replace("\\", "/"), r, False)
        tracked = set(files)
        if follow_files:
            for f in files:
                if not (f.startswith(("src/", "nonmatching/")) and f.endswith((".c", ".h"))):
                    continue
                r = self.ident.get(nc.norm(Path(f).stem))
                if r and r.old == Path(f).stem and f not in self.moves:
                    self.moves[f] = (str(Path(f).with_name(r.new + Path(f).suffix)).replace("\\", "/"), r, True)
        targets = collections.Counter(new for new, _, _ in self.moves.values())
        for old, (new, r, implied) in sorted(self.moves.items()):
            if targets[new] > 1:
                self.errors.append(f"{r.where()}: {old} and another file would both become {new}")
            elif new in tracked and new not in self.moves:
                self.errors.append(f"{r.where()}: {old} would become {new}, which exists")
        # The tree's paths (files and folders) by every trailing part: a path in a text names one of these.
        self.suffixes = collections.defaultdict(set)   # trailing part of a file's path -> {path}
        self.known = set()                              # trailing parts of files' and folders' paths
        for f in files:
            parts = f.split("/")
            for j in range(1, len(parts) + 1):
                for i in range(j):
                    s = "/".join(parts[i:j])
                    self.known.add(s)
                    if j == len(parts):
                        self.suffixes[s].add(f)
        self.moved_stems = {Path(old).stem for old in self.moves}
        self.explicit_stems = {Path(old).stem: (Path(new).stem, r) for old, (new, r, implied) in self.moves.items()
                               if not implied}
        self.edits = {}            # path -> (text, [(start, end, new text, row, category)])
        self.counts = collections.defaultdict(collections.Counter)   # id(row) -> {category: changes}
        self.row_files = collections.defaultdict(set)                # id(row) -> {path}
        self.left = collections.defaultdict(list)   # path -> [(line, category, token, row, excerpt)]
        self.symfile_add = []      # (row, address, kind)
        self.paste_macros = {}     # macro -> (params, body, path, line)
        self.pastes = []           # (path, line, macro, what, row)
        self.derived = []          # (path, line, token, row)

    # ------------------------------------------------------------------------------------------------ helpers
    def row_for(self, tok):
        """The row that renames this token: its old name in the same spelling."""
        r = self.ident.get(nc.norm(tok))
        return r if r is not None and r.old == tok else None

    def variant_of(self, tok):
        """The row whose old name this token spells otherwise (func_800a3424 for func_800A3424), or None."""
        r = self.ident.get(nc.norm(tok))
        return r if r is not None and r.old != tok else None

    def interesting(self, tok):
        """Is the token an old name, a moved file's stem, or an identifier with an old address name inside?"""
        if tok in self.moved_stems or nc.norm(tok) in self.ident:
            return True
        return len(tok) > 10 and any(nc.norm(m) in self.ident for m in ADDR_SEARCH.findall(tok))

    def edit(self, f, edits, s, e, new, row, cat):
        edits.append((s, e, new, row, cat))
        if row is not None:
            self.counts[id(row)][cat] += 1
            self.row_files[id(row)].add(f)

    @staticmethod
    def line_of(text, pos):
        return text.count("\n", 0, pos) + 1

    def leave(self, f, text, pos, category, tok, row):
        start = text.rfind("\n", 0, pos) + 1
        end = text.find("\n", pos)
        excerpt = text[start:end if end >= 0 else len(text)].strip()
        self.left[f].append((self.line_of(text, pos), category, tok, row, excerpt[:160]))

    def path_run(self, text, s, e, lo, hi):
        """(a, b) of the path-like run of characters around text[s:e] inside [lo, hi), trimmed of sentence
        punctuation, or None when the token stands alone."""
        a, b = s, e
        while a > lo and PATH_CH.match(text[a - 1]):
            a -= 1
        while b < hi and PATH_CH.match(text[b]):
            b += 1
        while b > e and text[b - 1] in ".-/":
            b -= 1
        while a < s and text[a] in "./-":
            a += 1
        run = text[a:b]
        if (a, b) == (s, e) or ("/" not in run and "." not in run):
            return None
        return a, b

    def is_path(self, run, rel):
        """Does the run name a file or folder? A tracked path's trailing part, a name with a file extension, or a
        run whose part up to the end of the token's component (at offset rel) is a tracked folder. Not
        D_80118E28/2C or gCar.speed."""
        last = run.rsplit("/", 1)[-1]
        if run in self.known or ("." in last and last.rsplit(".", 1)[1].lower() in PATH_EXTS):
            return True
        cut = run.find("/", rel)
        return cut > 0 and run[:cut] in self.known

    def move_for_run(self, run):
        """(new text, row) for a run that names a moved file by a trailing part of its path; None if it names
        none; 'ambiguous' if it could name files that don't all move alike."""
        parts = run.split("/")
        k, base = len(parts), parts[-1]
        if Path(base).stem not in self.moved_stems:
            return None
        cands = []
        for old in self.moves:
            op = old.split("/")
            n = min(k, len(op))
            if op[-1] == base and op[-n:] == parts[-n:]:
                cands.append(old)
        if not cands:
            return None
        if any(f not in self.moves for f in self.suffixes.get(run, ())):
            return "ambiguous"
        news = set()
        for old in cands:
            new, op = self.moves[old][0].split("/"), old.split("/")
            n = min(k, len(op))
            news.add("/".join(parts[:k - n] + new[len(new) - n:] if n <= len(new) else new))
        if len(news) != 1:
            return "ambiguous"
        return news.pop(), self.moves[sorted(cands)[0]][1]

    def stem_row(self, stem):
        for old, (new, r, implied) in self.moves.items():
            if Path(old).stem == stem:
                return r
        return None

    def note_derived(self, f, text, pos, tok):
        for m in ADDR_SEARCH.finditer(tok):
            row = self.row_for(m.group())
            if row and m.group() != tok:
                self.derived.append((f, self.line_of(text, pos), tok, row))

    # ------------------------------------------------------------------------------------------------ scanning
    def scan(self):
        for f in self.files:
            text = nc.read_text(self.root / f)
            if text is None:
                continue
            if f.endswith(C_EXTS) and "##" in text:
                for m in PASTE_DEFINE.finditer(text):
                    if "##" in m.group(3):
                        params = [p.strip() for p in m.group(2).split(",")]
                        self.paste_macros.setdefault(m.group(1), (params, m.group(3), f,
                                                                  self.line_of(text, m.start())))
            if not any(self.interesting(t) for t in set(nc.IDENT.findall(text))):
                continue
            edits = []
            if f.endswith(C_EXTS):
                self.scan_c(f, text, edits)
            elif SYMBOL_TABLES.match(f):
                self.scan_symbol_table(f, text, edits)
            else:
                cat = category_of(f)
                self.scan_text(f, text, 0, len(text), edits, cat, cmake=cat == "cmake")
            if edits:
                self.edits[f] = (text, edits)
        self.scan_pastes()
        self.plan_symbol_file()

    def scan_text(self, f, text, lo, hi, edits, category, cmake=False, strings=None, path_only=False):
        """The identifier tokens of text[lo:hi]: an old name is renamed, or left with its file when it is part of a
        path. strings: None for text, else 'asm' (every name renamed), 'name' (the literal is one name) or
        'mention' (left, unless --strings all). path_only: only paths change (#include)."""
        covered_to = lo
        for m in nc.IDENT.finditer(text, lo, hi):
            tok, s, e = m.group(), m.start(), m.end()
            if s < covered_to:
                continue
            row = self.row_for(tok)
            if not row and tok not in self.moved_stems:
                variant = self.variant_of(tok)
                if variant and not path_only:
                    self.leave(f, text, s, "other spelling", tok, variant)
                elif not path_only and len(tok) > 10 and ADDR_SEARCH.search(tok):
                    self.note_derived(f, text, s, tok)
                continue
            pr = self.path_run(text, s, e, lo, hi)
            if pr:
                a, b = pr
                run = text[a:b]
                if self.is_path(run, e - a):
                    mv = self.move_for_run(run)
                    if mv and mv != "ambiguous":
                        new, mrow = mv
                        if new != run:
                            self.edit(f, edits, a, b, new, mrow, "path")
                    elif row or tok in self.explicit_stems:
                        self.leave(f, text, s, "path (ambiguous)" if mv else "path", tok, row or self.stem_row(tok))
                    covered_to = b
                    continue
            if path_only:
                continue
            if not row:
                if cmake and tok in self.explicit_stems:
                    new, srow = self.explicit_stems[tok]
                    self.edit(f, edits, s, e, new, srow, "cmake stem")
                continue
            if strings == "mention" and not self.strings_all:
                self.leave(f, text, s, "string", tok, row)
                continue
            self.edit(f, edits, s, e, row.new, row, category if strings is None else f"string ({strings})")

    def scan_c(self, f, text, edits):
        depth = 0
        asm_ends = []       # the paren depths that end an asm( or alias( context
        pending_asm = False
        for k, s, e, v in nc.c_tokens(text):
            if k == "p":
                if v == "(":
                    depth += 1
                    if pending_asm:
                        asm_ends.append(depth)
                elif v == ")":
                    if asm_ends and asm_ends[-1] == depth:
                        asm_ends.pop()
                    depth = max(0, depth - 1)
                elif v in (";", "{", "}"):
                    asm_ends.clear()
                    depth = 0
                pending_asm = False
                continue
            if k == "id":
                pending_asm = v in ASM_CONTEXT or (pending_asm and v in ASM_QUALIFIERS)
                row = self.row_for(v)
                if row:
                    self.edit(f, edits, s, e, row.new, row, "code")
                elif self.variant_of(v):
                    self.leave(f, text, s, "other spelling", v, self.variant_of(v))
                elif len(v) > 10 and ADDR_SEARCH.search(v):
                    self.note_derived(f, text, s, v)
                continue
            if k in ("bc", "lc"):
                self.scan_text(f, text, s, e, edits, "comment")
            elif k == "inc":
                q = max(v.find('"'), v.find("<"))
                self.scan_text(f, text, s + q + 1, e - 1, edits, "include", path_only=True)
            elif k == "str":
                q = v.find('"')
                body_s = s + q + 1
                body_e = e - 1 if (e - body_s) > 0 and v.endswith('"') else e
                body = text[body_s:body_e]
                if asm_ends:
                    kind = "asm"
                elif nc.IDENT_FULL.match(body) and self.row_for(body):
                    kind = "name"
                else:
                    kind = "mention"
                self.scan_text(f, text, body_s, body_e, edits, "c", strings=kind)
            pending_asm = pending_asm and k in ("bc", "lc")

    def scan_pastes(self):
        """Macros that paste (##) identifiers: every invocation in the tree that makes an old name, and every
        macro with a pasted fragment that starts or ends old names (whatever its invocations)."""
        if not self.paste_macros:
            return
        call = re.compile(r"\b(" + "|".join(map(re.escape, sorted(self.paste_macros))) + r")\s*\(")
        for f in self.files:
            if not f.endswith(C_EXTS):
                continue
            text = nc.read_text(self.root / f)
            if not text:
                continue
            for m in call.finditer(text):
                params, body, df, dl = self.paste_macros[m.group(1)]
                args = split_args(text, m.end())
                if args is None or len(args) != len(params):
                    continue
                sub = dict(zip(params, args))
                for pm in re.finditer(r"\w+(?:\s*##\s*\w+)+", body):
                    name = "".join(sub.get(x, x) for x in re.split(r"\s*##\s*", pm.group()))
                    row = self.row_for(name) if nc.IDENT_FULL.match(name) else None
                    if row:
                        self.pastes.append((f, self.line_of(text, m.start()), m.group(1), name, row))
        olds = sorted(r.old for r in self.ident.values())
        for name, (params, body, df, dl) in sorted(self.paste_macros.items()):
            for pm in re.finditer(r"(\w+)\s*##\s*(\w+)", body):
                for lit, at_start in ((pm.group(1), True), (pm.group(2), False)):
                    if lit in params or len(lit) < 2:
                        continue
                    hits = [o for o in olds if o != lit and (o.startswith(lit) if at_start else o.endswith(lit))]
                    if hits:
                        self.pastes.append((df, dl, name, f"pastes {lit}{'##' if at_start else ''}: could make "
                                            f"{len(hits)} old names ({hits[0]}...)", self.row_for(hits[0])))

    def scan_symbol_table(self, f, text, edits):
        """asm/us/<image>/symbols.json: its keys renamed, rewritten in the table's own style (sorted keys, two
        spaces) when it has it, else as text."""
        try:
            d = json.loads(text)
            syms = d["symbols"]
            styled = json.dumps(d, indent=2, sort_keys=True) + "\n" == text
        except (ValueError, KeyError, TypeError):
            styled = False
        if not styled:
            self.scan_text(f, text, 0, len(text), edits, "symbol table")
            return
        new_syms, changed = {}, []
        for k, v in syms.items():
            row = self.row_for(k)
            nk = row.new if row else k
            if row:
                changed.append(row)
            if nk in new_syms and new_syms[nk] != v:
                self.errors.append(f"{f}: {k} would become {nk}, which the table has at {new_syms[nk]}, not {v}")
            new_syms.setdefault(nk, v)
        if changed:
            d["symbols"] = new_syms
            self.edit(f, edits, 0, len(text), json.dumps(d, indent=2, sort_keys=True) + "\n", None, "symbol table")
            for row in changed:
                self.counts[id(row)]["symbol table"] += 1
                self.row_files[id(row)].add(f)

    def plan_symbol_file(self):
        """The renamed address names symbol_addrs.us.txt doesn't define, with the address and the kind the tools
        took from the old spelling (n64syms.address_of, kind_of)."""
        p = self.root / SYMBOL_FILE
        if not p.is_file():
            return
        defined = set()
        for line in p.read_text(errors="replace").splitlines():
            m = re.match(r"^\s*([A-Za-z_]\w*)\s*=", line)
            if m:
                defined.add(m.group(1))
        for key, row in sorted(self.ident.items()):
            addr = nc.address_of_name(row.old)
            if addr is None or row.kind == "type" or row.old in defined or row.new in defined:
                continue
            self.symfile_add.append((row, addr, "func" if row.old.startswith("func_") else "data"))

    # ------------------------------------------------------------------------------------------------ results
    def new_texts(self):
        """path -> new text for every file the edits change."""
        out = {}
        for f, (text, edits) in self.edits.items():
            buf, pos = [], 0
            for s, e, new in sorted({(s, e, new) for s, e, new, row, cat in edits}):
                if s < pos:
                    self.errors.append(f"{f}: overlapping edits at line {self.line_of(text, s)}")
                    continue
                buf += [text[pos:s], new]
                pos = e
            buf.append(text[pos:])
            new_text = "".join(buf)
            if new_text != text:
                out[f] = new_text
        return out


def split_args(text, pos):
    """The arguments of a macro invocation whose '(' ends just before pos, or None if it doesn't close soon."""
    depth, args, cur = 1, [], []
    for i in range(pos, min(len(text), pos + 4000)):
        c = text[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append("".join(cur).strip())
                return args
        elif c == "," and depth == 1:
            args.append("".join(cur).strip())
            cur = []
            continue
        cur.append(c)
    return None


def scope_files(root, extra_excludes=()):
    """The files rename.py reads: the tree's, less the excluded."""
    globs = list(DEFAULT_EXCLUDES) + list(extra_excludes)
    return [f for f in nc.tracked_files(root) if not any(fnmatch.fnmatchcase(f, g) for g in globs)]


def update_checksums(root, changed):
    """Rewrite the SHA256SUMS lines of the changed files next to them; the SHA256SUMS files rewritten."""
    by_dir = collections.defaultdict(dict)
    for f in changed:
        by_dir[str(Path(f).parent)][Path(f).name] = f
    out = []
    for d, names in sorted(by_dir.items()):
        sums = Path(root) / d / "SHA256SUMS"
        if not sums.is_file():
            continue
        lines = sums.read_text().splitlines()
        new_lines = []
        for line in lines:
            parts = line.split(None, 1)
            if len(parts) == 2 and parts[1].lstrip("*") in names:
                digest = hashlib.sha256((Path(root) / names[parts[1].lstrip("*")]).read_bytes()).hexdigest()
                line = f"{digest}  {parts[1]}"
            new_lines.append(line)
        if new_lines != lines:
            sums.write_text("\n".join(new_lines) + "\n")
            out.append(str(sums.relative_to(root)))
    return out


def git_mv(root, old, new):
    (Path(root) / new).parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "-C", str(root), "mv", old, new], check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError):
        (Path(root) / old).rename(Path(root) / new)


# ---------------------------------------------------------------------------------------------------- the modes

def describe(plan, verbose=False, report=None):
    """The dry run's report: per row, per file, and what is left; the report file gets every change."""
    kinds = collections.Counter(r.kind for r in plan.rows)
    implied = [(o, n, r) for o, (n, r, imp) in sorted(plan.moves.items()) if imp]
    sym = {id(r): (a, k) for r, a, k in plan.symfile_add}
    left_by_row = collections.defaultdict(collections.Counter)
    for f, items in plan.left.items():
        for ln, cat, tok, row, ex in items:
            if row is not None:
                left_by_row[id(row)][cat] += 1
    out = [f"rename.py: {len(plan.rows)} rows ({', '.join(f'{k} {kinds[k]}' for k in nc.KINDS if kinds[k])}), "
           f"{len(implied)} implied file moves; {len(plan.edits)} files change", "",
           "per row (old -> new [kind]: changes by place; what else; what is left):"]
    for r in plan.rows:
        if r.kind == "field":
            out.append(f"  {r.old} -> {r.new} [field]: LEFT for wave 2: a field is renamed by hand, in the struct "
                       f"and its users (it needs the struct's layout)  ({r.where()})")
            continue
        c = plan.counts.get(id(r), {})
        extra = []
        if r.kind == "file":
            extra.append(f"git mv {r.old} {r.new}")
        if id(r) in sym:
            a, k = sym[id(r)]
            extra.append(f"{SYMBOL_FILE} += {r.new} = 0x{a:08X} type:{k}")
        extra += [f"moves {o} -> {n}" for o, n, ir in implied if ir is r]
        lft = left_by_row.get(id(r), {})
        out.append(f"  {r.old} -> {r.new} [{r.kind}]: {sum(c.values())} in {len(plan.row_files.get(id(r), ()))} files"
                   + (f" ({', '.join(f'{k} {v}' for k, v in sorted(c.items()))})" if c else "")
                   + "".join(f"; {x}" for x in extra)
                   + (f"; left: {', '.join(f'{k} {v}' for k, v in sorted(lft.items()))}" if lft else ""))
    out += ["", "per file:"]
    for f in sorted(plan.edits):
        text, edits = plan.edits[f]
        c = collections.Counter(row.old for s, e, n, row, cat in edits if row is not None)
        if f.endswith("symbols.json"):
            c = collections.Counter({r.old: 1 for r in plan.rows if f in plan.row_files.get(id(r), ())
                                     and plan.counts[id(r)].get("symbol table")})
        out.append(f"  {f}: " + ", ".join(f"{o} x{k}" for o, k in sorted(c.items())))
    left_items = [(f, x) for f in sorted(plan.left) for x in plan.left[f]]
    out += ["", f"left for hand edits ({len(left_items)}):"]
    shown = left_items if verbose else left_items[:40]
    out += [f"  {f}:{ln}: {cat}: {tok}: {ex}" for f, (ln, cat, tok, row, ex) in shown]
    if len(shown) < len(left_items):
        out.append(f"  ... {len(left_items) - len(shown)} more (-v, or --report FILE)")
    if plan.pastes:
        out += ["", "token pasting (##) that makes or could make an old name (by hand):"]
        out += [f"  {f}:{ln}: {macro}: {what}" for f, ln, macro, what, row in plan.pastes]
    if plan.derived:
        out += ["", f"derived identifiers that contain an old address name ({len(plan.derived)}, left):"]
        out += [f"  {f}:{ln}: {tok} ({row.old} -> {row.new})"
                for f, ln, tok, row in (plan.derived if verbose else plan.derived[:20])]
        if not verbose and len(plan.derived) > 20:
            out.append(f"  ... {len(plan.derived) - 20} more")
    if report:
        lines = ["# rename.py: every change and everything left", ""]
        for f in sorted(plan.edits):
            text, edits = plan.edits[f]
            for s, e, new, row, cat in sorted(edits, key=lambda x: x[0]):
                if row is not None:
                    lines.append(f"{f}:{plan.line_of(text, s)}: {cat}: {text[s:e]} -> {new}")
                else:
                    lines.append(f"{f}: {cat}: rewritten")
        for row, a, k in plan.symfile_add:
            lines.append(f"{SYMBOL_FILE}: added: {row.new} = 0x{a:08X}; // type:{k}")
        for o, (n, r, imp) in sorted(plan.moves.items()):
            lines.append(f"{o}: moved to {n}" + (" (implied by its function's row)" if imp else ""))
        lines += [f"{f}:{ln}: LEFT {cat}: {tok}: {ex}" for f, (ln, cat, tok, row, ex) in left_items]
        lines += [f"{f}:{ln}: LEFT token pasting: {macro}: {what}" for f, ln, macro, what, row in plan.pastes]
        lines += [f"{f}:{ln}: LEFT derived identifier: {tok}" for f, ln, tok, row in plan.derived]
        Path(report).write_text("\n".join(lines) + "\n")
        out += ["", f"every change: {report}"]
    return "\n".join(out)


def apply(plan, texts):
    """Write the new texts, add the symbol file's names, move the files, update the checksums."""
    for f, t in texts.items():
        (plan.root / f).write_text(t, encoding="utf-8")
    changed = set(texts)
    if plan.symfile_add:
        p = plan.root / SYMBOL_FILE
        text = p.read_text()
        lines = [] if text.endswith("\n") else [""]
        header = "// Names from the name maps (docs/port/PCONLY.md 3, tools/port/rename.py):"
        if header not in text:
            lines += ["", header, "// the N64 address and the kind their old name spelled, which the tools read from it."]
        lines += [f"{row.new} = 0x{addr:08X}; // type:{kind}" for row, addr, kind in plan.symfile_add]
        p.write_text(text + "\n".join(lines) + "\n")
        changed.add(SYMBOL_FILE)
    moved = []
    for old, (new, row, implied) in sorted(plan.moves.items()):
        if (plan.root / old).exists() and not (plan.root / new).exists():
            git_mv(plan.root, old, new)
            moved.append((old, new))
    m = dict(moved)
    sums = update_checksums(plan.root, [m.get(f, f) for f in changed])
    return moved, sums


def check(root, rows, files):
    """Every place an old name (or an old file) is still in the tree: [(path, line, token, row)]."""
    ident = {nc.norm(r.old): r for r in rows if r.kind in nc.IDENT_KINDS}
    hits = []
    for f in files:
        text = nc.read_text(Path(root) / f)
        if text is None:
            continue
        for m in nc.IDENT.finditer(text):
            r = ident.get(nc.norm(m.group()))
            if r:
                hits.append((f, text.count("\n", 0, m.start()) + 1, m.group(), r))
    for r in rows:
        if r.kind == "file" and (Path(root) / r.old).exists():
            hits.append((r.old, 0, "(the old file is still there)", r))
    return hits


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("maps", nargs="*", help="the maps (default: every tools/port/names/*.tsv)")
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--dry-run", action="store_true", help="say what would change")
    mode.add_argument("--apply", action="store_true", help="change the tree")
    mode.add_argument("--check", action="store_true", help="exit 1 if an old name is still in the tree")
    ap.add_argument("--root", type=Path, default=ROOT, help="the top of the tree (default: this checkout)")
    ap.add_argument("--exclude", action="append", default=[], metavar="GLOB", help="leave these files alone")
    ap.add_argument("--strings", choices=("names", "all"), default="names",
                    help="all: rename names in any C string literal, not only the symbol-name strings")
    ap.add_argument("--no-follow-files", action="store_true",
                    help="don't move a file named after a renamed function (src/ovl_a/func_8038F454.c)")
    ap.add_argument("--no-names-check", action="store_true", help="don't run names_check.py's checks first")
    ap.add_argument("--report", type=Path, help="write every change and everything left to this file")
    ap.add_argument("-v", "--verbose", action="store_true", help="list everything left, not the first 40")
    a = ap.parse_args(argv)
    root = a.root.resolve()
    paths = nc.map_paths(root, a.maps)
    if not paths:
        print(f"rename.py: no maps in {root / nc.MAPS_DIR}")
        return 0
    files = scope_files(root, a.exclude)
    if a.check:
        rows, errs = nc.read_maps(paths)
        for where, msg in errs:
            print(f"{where}: error: {msg}")
        hits = check(root, rows, files)
        for f, ln, tok, r in hits:
            print(f"{f}:{ln}: {tok} ({r.kind} {r.old} -> {r.new}, {r.where()})")
        nfield = sum(1 for r in rows if r.kind == "field")
        print(f"rename.py --check: {len(hits)} places still have an old name"
              + (f"; {nfield} field rows not checked (renamed by hand)" if nfield else ""))
        return 1 if hits or errs else 0
    if a.no_names_check:
        rows, errs = nc.read_maps(paths)
        errs += [(r.where(), m) for r in rows for m in nc.check_format(r)]
    else:
        rows, errs, _ = nc.run_checks(root, paths)
    if errs:
        for where, msg in errs:
            print(f"{where}: error: {msg}")
        print(f"rename.py: the maps have {len(errs)} errors (names_check.py); nothing done")
        return 1
    plan = Plan(root, rows, files, follow_files=not a.no_follow_files, strings_all=a.strings == "all")
    if not plan.errors:
        plan.scan()
    texts = plan.new_texts() if not plan.errors else {}
    if plan.errors:
        for e in plan.errors:
            print(f"rename.py: error: {e}")
        print("rename.py: nothing done")
        return 1
    if a.dry_run:
        print(describe(plan, a.verbose, a.report))
        return 0
    moved, sums = apply(plan, texts)
    if a.report:
        describe(plan, a.verbose, a.report)
    left = sum(len(v) for v in plan.left.values())
    print(f"rename.py: {len(texts)} files changed, {len(moved)} moved"
          + (f", {len(plan.symfile_add)} names added to {SYMBOL_FILE}" if plan.symfile_add else "")
          + (f", checksums updated in {', '.join(sums)}" if sums else "")
          + f"; left for hand edits: {left} places, {len(plan.pastes)} pasted names, {len(plan.derived)} derived "
          f"identifiers, {sum(1 for r in rows if r.kind == 'field')} field rows (--dry-run lists them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
