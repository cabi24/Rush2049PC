#!/usr/bin/env python3
"""Apply the name maps (tools/port/names/*.tsv, docs/port/PCONLY.md 3) and the drop list (tools/port/names/drop.tsv)
to the whole tree.

    python3 tools/port/rename.py --dry-run [--report FILE]     what would change, per map row and per file
    python3 tools/port/rename.py --apply                       change the tree (git mv for files and folders)
    python3 tools/port/rename.py --check                       exit 1 if an old name is still anywhere
    options: [MAP ...] (default: every tools/port/names/*.tsv; a file named drop.tsv is a drop list) [--root DIR]
             [--exclude GLOB]... [--strings all] [--no-follow-files] [--no-names-check] [-v]

One pass reads every file of the tree (git's: tracked, and untracked but not ignored), splits it into identifier
tokens and looks each one up in the maps, so the cost doesn't grow with the number of rows. All the rows are one
simultaneous substitution, never one rename after another: a row's new name may be another row's old name (a chain,
func_800DE858 to no_catchup while no_catchup becomes pak_insert_file, or a swap), files included (a chain of moves
waits for its next link, a cycle goes through a temporary name). The drop list goes first: its labels leave the
tables before the maps give their names to other addresses. A name is matched as a
whole identifier token, never inside a longer one, in the spelling the map gives: another spelling of an address
name's hex digits (func_800a3424 for func_800A3424) is another C identifier, often declared with another prototype in
the same file, so it is reported and left (gen_stubs.py still links it to the renamed symbol by its address, and
--check finds it). Where a name lives, and what happens there:

- C (.c, .h, .inc anywhere): the identifiers of the code that name the global symbol; comments (whole words, but see
  paths below); string literals only where the string names a symbol: the strings of asm statements and of
  alias/weakref attributes (linker symbols: `.set D_x, gScheduler + 0x40`), and a literal that is exactly an old
  name (lookup tables and labels: {"func_800D6348", func_800D6348}, PC_SPIN("func_800E7038")). Any other string that
  mentions a name is reported and left (it is output; --strings all renames those too). `#include` paths follow
  file renames. Not the symbol, so left and reported: a struct member of the same spelling (after . or ->, a
  designated initialiser, a declaration in a struct or union body: port/gfx's Windows `targetInfo`), a macro's
  parameter, a function's parameter or a local variable of the same spelling and the uses its scope gives it (the
  new name would capture them, or they would keep the old one).
- The data loader's symbol tables (asm/us/*/symbols.json): their keys, the JSON rewritten in its own style; the
  labels drop.tsv lists are removed (refused, with the maps' other errors, while code still uses one that no symbol
  file keeps), and so are its symbol_addrs rows' lines of symbol_addrs.us.txt (a stale gNumPlayers = 0x80166004
  whose name a map gives to another address).
- symbol_addrs.us.txt and every other text file (the tools' Python, the press scripts, port/data/*.txt such as
  extents.txt and decoupled.txt, the sanitizer suppressions, the linker scripts, CMake, the manifest and layout
  JSON, the docs' Markdown): whole identifier tokens (not after . or ->: reached.txt's Tag.field is a member).
- An address name's address: the tools take the address of a D_/func_XXXXXXXX name from its spelling when no
  table has it (n64syms.address_of, kind_of). For each renamed address name that symbol_addrs.us.txt doesn't define,
  rename.py adds `new = 0xADDRESS; // type:func|data` there, the kind its old spelling implied, so gen_data.py,
  gen_stubs.py and synth_rom.py resolve the new name exactly as the old one.
- Aliases: rows that give two names of one address (D_801174B4 and state_word_a in the tables) one new name make one
  symbol: one key in each table, one line in symbol_addrs.us.txt, and in a C file one declaration where the rename
  made two of the same text (the second is dropped); two that differ (s32 against u32) are reported, not picked, and
  so is an alias the rename made of a name to itself (lib_1050_sdk.c's PC_ALIAS("D_8002E8E8", "gScheduler") becomes
  `.set gScheduler, gScheduler`, which doesn't assemble: the lead deletes those lines).
- Paths. A token inside a path (a run like src/blob/func_800A1234.c, or a tracked folder such as
  src/blob/groups/heap_release) is part of a file's name, not the symbol's: it changes only with the file. A file row
  moves its file (git mv) and rewrites every reference to its path, by its full path or any trailing part of it
  (blob/x.c, x.c), #include lines included; a reference whose trailing part matches several files that don't all
  get the same new name is reported and left. A file row may name a folder (src/blob/groups/carcolor): its files
  move, and references to the folder and to paths in it follow. A func, data or type row whose old name is the stem
  of a file under src/ or nonmatching/ (src/ovl_a/func_8038F454.c) moves that file to the new stem as well (an
  implied file row; --no-follow-files turns that off), so a CMake list of stems (nonmatching/game/${f}.c) stays
  true. In CMake files an explicit file row's bare stem is renamed too.
- SHA256SUMS files list the checksums of the files next to them: a line for a file rename.py changes is updated.

Left, and said so per row: field rows (Struct.0xOFFSET: renaming a field needs the struct's layout, wave 2's hand
edit), strings that mention a name, members, parameters and locals of the same spelling (above), paths into files
that don't move (the N64 assembly, folders named after a function), derived identifiers that contain an old address
name (func_800D6E00_1, __standin_X, include guards) and names built by token pasting (a ## b) that make an old name.

Never touched: the maps themselves, these tools' sources and their tests' fixtures, docs/port/PCONLY.md (its
examples) and the N64 assembly (asm/**/*.s: checksummed, the matching build's), plus --exclude globs.

--dry-run and --apply run names_check.py's checks on the maps and the drop list first and stop on an error
(--no-names-check skips the maps' checks, not the drop list's; --force applies in spite of them, for the collisions
the hand edits fix after; a row's format error still stops it). --apply writes the rows it applied to
tools/port/names/applied.tsv, the ledger names_check.py and a second --apply read (by names alone a chain's rows
look undone after it), so --apply twice changes nothing the second time. --check lists every place an old name
still names its symbol (code, comments, strings, paths, other spellings, derived and pasted names), every file or
folder still to move and every dropped label still in a table; a chain's middle rows (old names that are other
rows' new names) can't be checked by name. Stdlib only, Python 3.9+.
"""
import argparse
import collections
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import names_check as nc  # noqa: E402

ROOT = nc.ROOT
DEFAULT_EXCLUDES = (
    "tools/port/names/*",             # the maps and the drop list
    "tools/port/tests/names/*",       # the fixtures of these tools' tests (made-up names)
    "tools/port/rename.py", "tools/port/names_check.py", "tools/port/pconly_gate.sh",
    "tools/port/tests/test_rename.py", "tools/port/tests/test_names_check.py",
    "docs/port/PCONLY.md",            # the map format's examples
    "asm/*.s",                        # the N64 build's assembly (fnmatch's * crosses folders)
    "*SHA256SUMS",                    # updated by checksum, below
)
SYMBOL_FILE = "symbol_addrs.us.txt"
SYMBOL_TABLES = re.compile(r"^asm/us/([^/]+)/symbols\.json$")
C_EXTS = nc.C_EXTS
# The extensions that make a token followed by `.ext` a file name (not .a, .d or .o: one-letter struct members).
PATH_EXTS = {"c", "h", "s", "inc", "json", "txt", "md", "py", "ld", "press", "cmake", "png", "log", "sh", "yml",
             "yaml", "supp", "tsv", "wav", "z64", "hex", "mk", "rc", "bat"}
PATH_CH = re.compile(r"[A-Za-z0-9_./\-]")
ASM_CONTEXT = {"asm", "__asm__", "__asm", "alias", "__alias__", "weakref", "__weakref__"}
ASM_QUALIFIERS = {"volatile", "__volatile__", "__volatile", "goto", "inline", "__inline__"}
ADDR_SEARCH = re.compile(r"(?:D|func|sym|jtbl)_[0-9A-Fa-f]{8}")
PASTE_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)((?:\\\r?\n|[^\n])*)", re.M)
# What a declaration statement in a function body may start with (besides the tree's typedef names).
C_TYPE_WORDS = {"void", "char", "short", "int", "long", "float", "double", "signed", "unsigned", "struct", "union",
                "enum", "_Bool", "bool", "s8", "u8", "s16", "u16", "s32", "u32", "s64", "u64", "f32", "f64",
                "int8_t", "uint8_t", "int16_t", "uint16_t", "int32_t", "uint32_t", "int64_t", "uint64_t", "intptr_t",
                "uintptr_t", "size_t"}
DECL_QUALIFIERS = ("static", "register", "const", "volatile", "auto")
# Identifiers that stand before another one without making it a declared name (struct OldThing: a tag; const T: a
# type follows).
NOT_A_TYPE = {"return", "sizeof", "case", "goto", "else", "do", "typedef", "extern", "if", "while", "for", "switch",
              "break", "continue", "default", "struct", "union", "enum", "const", "volatile"}


# A preprocessor line, with its continuation lines; group 1 is the directive.
PP_LINE = re.compile(r"^[ \t]*#[ \t]*(\w*)(?:\\\r?\n|[^\n])*", re.M)


class CState:
    """Where scan_c is in a C file's code: the open braces ([kind, {old names declared locally}], kind struct,
    enum, init, func or block), the old names among the last file-scope declarator's parameters, the statement's
    tokens so far, the paren depth and the asm( contexts open."""
    __slots__ = ("scopes", "params", "stmt", "depth", "asm_ends", "pending_asm")

    def __init__(self):
        self.scopes, self.params, self.stmt, self.depth, self.asm_ends, self.pending_asm = [], set(), [], 0, [], False

    def copy(self):
        c = CState()
        c.scopes = [[kind, set(names)] for kind, names in self.scopes]
        c.params, c.stmt, c.depth = set(self.params), list(self.stmt), self.depth
        c.asm_ends, c.pending_asm = list(self.asm_ends), self.pending_asm
        return c


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


def after_member_op(text, s, lo):
    """Does text[s:] follow a member operator: `.` (not `..`) or `->`?"""
    if s - 1 >= lo and text[s - 1] == "." and not (s - 2 >= lo and text[s - 2] == "."):
        return True
    return s - 2 >= lo and text[s - 2:s] == "->"


def function_macro_params(text):
    """[(start, end, {parameter})] of each function-like #define of a C file, in order."""
    if "define" not in text:
        return []
    return [(m.start(), m.end(), {p.strip() for p in m.group(2).split(",")}) for m in PASTE_DEFINE.finditer(text)]


def line_span(text, pos):
    a = text.rfind("\n", 0, pos) + 1
    b = text.find("\n", pos)
    return a, (b if b >= 0 else len(text))


class Plan:
    """What the maps change in the tree: built once, then printed (--dry-run) or applied (--apply)."""

    def __init__(self, root, rows, files, follow_files=True, strings_all=False, types=(), drops=()):
        self.root = Path(root)
        self.rows = rows
        self.files = files
        self.strings_all = strings_all
        self.types = set(types)
        self.errors = []
        self.ident = {nc.norm(r.old): r for r in rows if r.kind in nc.IDENT_KINDS}
        by_new = collections.defaultdict(list)
        for r in rows:
            if r.kind in ("func", "data"):
                by_new[r.new].append(r)
        self.merged = {new: rs for new, rs in by_new.items() if len(rs) > 1}   # aliases made one name
        self.drops = list(drops)
        # drop.tsv's rows for symbol_addrs.us.txt: lines that go before the rename (a stale gNumPlayers = 0x80166004
        # whose name a map gives to another address)
        self.symfile_drops = {d.name for d in self.drops if d.table == nc.SYMBOL_FILE_TABLE}
        self.plan_moves(follow_files)
        # The tree's paths by every trailing part: a path in a text names one of these.
        self.suffixes = collections.defaultdict(set)       # trailing part of a file's path -> {path}
        self.dir_suffixes = collections.defaultdict(set)   # trailing part of a folder's path -> {folder}
        self.known = set()                                  # trailing parts of files' and folders' paths
        for f in files:
            parts = f.split("/")
            for j in range(1, len(parts) + 1):
                for i in range(j):
                    s = "/".join(parts[i:j])
                    self.known.add(s)
                    if j == len(parts):
                        self.suffixes[s].add(f)
                    else:
                        self.dir_suffixes[s].add("/".join(parts[:j]))
        self.moved_stems = {Path(old).stem for old in self.moves}
        self.moves_by_base = collections.defaultdict(list)   # a moved file's name -> [its old path]
        for old in self.moves:
            self.moves_by_base[old.rsplit("/", 1)[-1]].append(old)
        self.dirs_by_base = collections.defaultdict(list)    # a moved folder's name -> [its old path]
        for old in self.dir_moves:
            self.dirs_by_base[old.rsplit("/", 1)[-1]].append(old)
        self.explicit_stems = {Path(old).stem: (Path(new).stem, r) for old, (new, r, how) in self.moves.items()
                               if how == "file"}
        self.edits = {}            # path -> (text, [(start, end, new text, row, category)])
        self.counts = collections.defaultdict(collections.Counter)   # id(row) -> {category: changes}
        self.row_files = collections.defaultdict(set)                # id(row) -> {path}
        self.left = collections.defaultdict(list)   # path -> [(line, category, token, row, excerpt)]
        self.symfile_add = []      # (row, address, kind)
        self.paste_macros = {}     # macro -> (params, body, path, line)
        self.pastes = []           # (path, line, macro, what, row)
        self.derived = []          # (path, line, token, row)
        self.merges = []           # (path, line, what): declarations the alias merges dropped or left
        self.dropped = collections.defaultdict(list)   # label -> [table it leaves]

    def plan_moves(self, follow_files):
        """self.moves: old path -> (new path, row, how) for every file that moves: `file` (its own row), `folder`
        (its folder's row, and its stem's if that is renamed too) or `implied` (its stem is a renamed function's);
        self.dir_moves: old folder -> (new folder, row)."""
        tracked = set(self.files)
        explicit, self.dir_moves, self.moves = {}, {}, {}
        for r in self.rows:
            if r.kind != "file":
                continue
            old, new = r.old.replace("\\", "/").rstrip("/"), r.new.replace("\\", "/").rstrip("/")
            if old in tracked:
                explicit[old] = (new, r)
            elif any(f.startswith(old + "/") for f in self.files):
                self.dir_moves[old] = (new, r)
            # else moved already (names_check tells a missing one)
        for f in self.files:
            if f in explicit:
                self.moves[f] = (explicit[f][0], explicit[f][1], "file")
                continue
            new, row, how = f, None, None
            d = max((d for d in self.dir_moves if f.startswith(d + "/")), key=len, default=None)
            if d:
                new, row, how = self.dir_moves[d][0] + f[len(d):], self.dir_moves[d][1], "folder"
            stem = Path(f).stem
            r = self.ident.get(nc.norm(stem))
            if follow_files and r and r.old == stem and f.startswith(("src/", "nonmatching/")) \
                    and f.endswith((".c", ".h")):
                new = new.rsplit("/", 1)[0] + "/" + r.new + Path(f).suffix
                row, how = row or r, how or "implied"
            if new != f:
                self.moves[f] = (new, row, how)
        targets = collections.Counter(new for new, _, _ in self.moves.values())
        for old, (new, r, how) in sorted(self.moves.items()):
            if targets[new] > 1:
                self.errors.append(f"{r.where()}: {old} and another file would both become {new}")
            elif new in tracked and new not in self.moves:
                self.errors.append(f"{r.where()}: {old} would become {new}, which exists")

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
        """Is the token an old name, a moved file's or folder's name, or an identifier with an old address name
        inside?"""
        if tok in self.moved_stems or tok in self.dirs_by_base or nc.norm(tok) in self.ident:
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
        a, b = line_span(text, pos)
        self.left[f].append((self.line_of(text, pos), category, tok, row, text[a:b].strip()[:160]))

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
        """(new text, row) for a run that names a moved file by a trailing part of its path, or a moved folder or
        a path inside one; None if it names none; 'ambiguous' if it could name things that don't all move alike."""
        parts = run.split("/")
        k, base = len(parts), parts[-1]
        cands = []
        for old in self.moves_by_base.get(base, ()):
            op = old.split("/")
            n = min(k, len(op))
            if op[-n:] == parts[-n:]:
                cands.append(old)
        if cands:
            if any(f not in self.moves for f in self.suffixes.get(run, ())):
                return "ambiguous"
            news = set()
            for old in cands:
                new, op = self.moves[old][0].split("/"), old.split("/")
                n = min(k, len(op))
                news.add("/".join(parts[:k - n] + (new[len(new) - n:] if n <= len(new) else new)))
            if len(news) != 1:
                return "ambiguous"
            return news.pop(), self.moves[sorted(cands)[0]][1]
        for i, comp in enumerate(parts):   # a moved folder, or a path inside one
            for old in self.dirs_by_base.get(comp, ()):
                od = old.split("/")
                n = min(i + 1, len(od))
                if od[-n:] != parts[i + 1 - n:i + 1]:
                    continue
                if any(d not in self.dir_moves for d in self.dir_suffixes.get("/".join(parts[i + 1 - n:i + 1]), ())):
                    return "ambiguous"
                nd = self.dir_moves[old][0].split("/")
                return "/".join(parts[:i + 1 - n] + (nd[len(nd) - n:] if n <= len(nd) else nd)
                                + parts[i + 1:]), self.dir_moves[old][1]
        return None

    def stem_row(self, stem):
        return self.explicit_stems.get(stem, (None, None))[1]

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
            edits = []
            if SYMBOL_TABLES.match(f):
                self.scan_symbol_table(f, text, edits)
            elif f == SYMBOL_FILE and self.symfile_drops:
                self.scan_text(f, text, 0, len(text), edits, category_of(f))
                self.edits[f] = (text, edits)   # its dropped lines go in new_texts, edits or none
                continue
            elif not any(self.interesting(t) for t in set(nc.IDENT.findall(text))):
                continue
            elif f.endswith(C_EXTS):
                self.scan_c(f, text, edits)
            else:
                cat = category_of(f)
                self.scan_text(f, text, 0, len(text), edits, cat, cmake=cat == "cmake")
            if edits:
                self.edits[f] = (text, edits)
        self.scan_pastes()
        self.plan_symbol_file()

    def scan_text(self, f, text, lo, hi, edits, category, cmake=False, strings=None, path_only=False):
        """The identifier tokens of text[lo:hi]: an old name is renamed, or left with its file when it is part of a
        path, or left as a struct member when it follows `.` or `->` (gCar.speed, reached.txt's Tag.field).
        strings: None for text, else 'asm' (every name renamed), 'name' (the literal is one name) or 'mention'
        (left, unless --strings all). path_only: only paths change (#include)."""
        covered_to = lo
        for m in nc.IDENT.finditer(text, lo, hi):
            tok, s, e = m.group(), m.start(), m.end()
            if s < covered_to:
                continue
            row = self.row_for(tok)
            if not row and tok not in self.moved_stems and tok not in self.dirs_by_base:
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
            if strings in (None, "mention") and after_member_op(text, s, lo):
                self.leave(f, text, s, "struct member", tok, row)
                continue
            self.edit(f, edits, s, e, row.new, row, category if strings is None else f"string ({strings})")

    def scan_c(self, f, text, edits):
        """A C file's tokens. In the code an old name is renamed where it names the global symbol, and left (and
        reported) where it names something else of the same spelling: a struct member (after . or ->, a
        designated initialiser, a declaration in a struct or union body), a macro's parameter, a parameter or a
        local variable that shadows the global, and the uses that local's scope gives it. Preprocessor lines
        don't move the scopes: each #else or #elif branch starts where its #if did, and #endif goes on from the end
        of the first branch (an #ifdef TARGET_PC and its #else may each open a struct)."""
        macro_params = function_macro_params(text)   # [(start, end, {parameter})] of each function-like #define
        pp_lines = [(m.start(), m.end(), m.group(1)) for m in PP_LINE.finditer(text)] if "#" in text else []
        mp_i = pp_i = 0
        pp_seen = -1
        toks = nc.c_tokens(text)
        code = [i for i, t in enumerate(toks) if t[0] in ("id", "p", "num", "str", "chr")]
        st = CState()
        conds = []           # per open #if: [the state at the #if, the state at the end of its first branch]
        ci = 0               # index into code of the token being read

        def tok(j):
            return toks[code[j]][3] if 0 <= j < len(code) else ""

        comments = []        # the comments' edits: kept unless the file's code uses that name only otherwise
        global_use = set()   # rows the code renames in this file
        other_use = set()    # rows the code leaves in this file (members, parameters, locals)
        for k, s, e, v in toks:
            if k in ("bc", "lc"):
                self.scan_text(f, text, s, e, comments, "comment")
                continue
            if k == "inc":
                q = max(v.find('"'), v.find("<"))
                self.scan_text(f, text, s + q + 1, e - 1, edits, "include", path_only=True)
                continue
            prev, prev2, nxt = tok(ci - 1), tok(ci - 2), tok(ci + 1)
            prev_is_id = ci >= 1 and toks[code[ci - 1]][0] == "id"
            ci += 1
            while pp_i < len(pp_lines) and pp_lines[pp_i][1] <= s:
                pp_i += 1
            directive = pp_i < len(pp_lines) and pp_lines[pp_i][0] <= s
            if directive and pp_seen != pp_i:   # the line's first token: #
                pp_seen = pp_i
                name = pp_lines[pp_i][2]
                if name in ("if", "ifdef", "ifndef"):
                    conds.append([st.copy(), None])
                elif name in ("elif", "else") and conds:
                    if conds[-1][1] is None:
                        conds[-1][1] = st.copy()
                    st = conds[-1][0].copy()
                elif name == "endif" and conds:
                    at_if, first = conds.pop()
                    if first is not None:
                        st = first
                continue
            if k == "p":
                if directive:
                    continue
                if v == "(":
                    st.depth += 1
                    if st.pending_asm:
                        st.asm_ends.append(st.depth)
                elif v == ")":
                    if st.asm_ends and st.asm_ends[-1] == st.depth:
                        st.asm_ends.pop()
                    st.depth = max(0, st.depth - 1)
                elif v == "{":
                    top = st.scopes[-1][0] if st.scopes else "file"
                    if prev == "=" or (prev in (",", "{") and top == "init"):
                        kind = "init"
                    elif prev != ")" and ("struct" in st.stmt or "union" in st.stmt):
                        kind = "struct"
                    elif prev != ")" and "enum" in st.stmt:
                        kind = "enum"
                    elif not st.scopes and prev == ")":
                        kind = "func"
                    else:
                        kind = "block"
                    st.scopes.append([kind, st.params if kind == "func" else set()])
                    st.params = set()
                    st.stmt = []
                elif v == "}":
                    if st.scopes:
                        st.scopes.pop()
                    st.stmt = []
                elif v == ";":
                    st.stmt = []
                    if not st.scopes:
                        st.params = set()
                if v in (";", "{", "}"):
                    st.asm_ends.clear()
                    st.depth = 0
                else:
                    st.stmt.append(v)
                st.pending_asm = False
                continue
            if k == "id":
                if not directive:
                    st.pending_asm = v in ASM_CONTEXT or (st.pending_asm and v in ASM_QUALIFIERS)
                    st.stmt.append(v)
                row = self.row_for(v)
                if not row:
                    if self.variant_of(v):
                        self.leave(f, text, s, "other spelling", v, self.variant_of(v))
                    elif len(v) > 10 and ADDR_SEARCH.search(v):
                        self.note_derived(f, text, s, v)
                    continue
                while mp_i < len(macro_params) and macro_params[mp_i][1] <= s:
                    mp_i += 1
                in_macro = mp_i < len(macro_params) and macro_params[mp_i][0] <= s
                top = st.scopes[-1][0] if st.scopes else "file"
                typeish = prev_is_id and prev not in NOT_A_TYPE
                fptr = prev == "*" and prev2 == "("
                other_use.add(id(row))
                if in_macro and v in macro_params[mp_i][2]:
                    self.leave(f, text, s, "macro parameter", v, row)
                elif prev in (".", "->"):
                    self.leave(f, text, s, "struct member", v, row)
                elif directive:   # a macro's body or a condition: the global
                    other_use.discard(id(row))
                    global_use.add(id(row))
                    self.edit(f, edits, s, e, row.new, row, "code")
                elif (top == "struct" and st.depth == 0 and nxt in (";", ",", "[", ":")) or (
                        top == "struct" and fptr and nxt == ")"):
                    self.leave(f, text, s, "struct member", v, row)
                elif not st.scopes and st.depth >= 1 and nxt in (",", ")", "[") and (typeish or prev == "*") \
                        and not (fptr and st.depth == 1):
                    st.params.add(v)   # a parameter: shadows the global in a definition's body
                    self.leave(f, text, s, "parameter of the same name", v, row)
                elif top in ("func", "block") and "extern" not in st.stmt and self.declares(
                        st.stmt, prev, nxt, typeish, fptr, st.depth):
                    st.scopes[-1][1].add(v)
                    self.leave(f, text, s, "local of the same name", v, row)
                elif any(v in sc[1] for sc in st.scopes):
                    self.leave(f, text, s, "use of a local of the same name", v, row)
                else:
                    other_use.discard(id(row))
                    global_use.add(id(row))
                    self.edit(f, edits, s, e, row.new, row, "code")
                continue
            if not directive:
                st.pending_asm = False
                st.stmt.append("0")
            if k == "str":
                q = v.find('"')
                body_s = s + q + 1
                body_e = e - 1 if (e - body_s) > 0 and v.endswith('"') else e
                body = text[body_s:body_e]
                if st.asm_ends and not directive:
                    kind = "asm"
                elif nc.IDENT_FULL.match(body) and self.row_for(body):
                    kind = "name"
                else:
                    kind = "mention"
                self.scan_text(f, text, body_s, body_e, edits, "c", strings=kind)
        # A file whose code has a name only as a member, parameter or local: its comments mean that too (port/gfx's
        # "DISPLAYCONFIG_PATH_INFO has a targetInfo"), so they are left and reported.
        for s, e, new, row, cat in comments:
            if row is not None and cat == "comment" and id(row) in other_use and id(row) not in global_use:
                self.counts[id(row)][cat] -= 1
                self.leave(f, text, s, "comment in a file that has the name only as another thing", text[s:e], row)
            else:
                edits.append((s, e, new, row, cat))

    def declares(self, stmt, prev, nxt, typeish, fptr, depth):
        """Does the statement being read declare the identifier just read (a local, in a function body)? The
        statement starts with a type (a C type keyword or a typedef name of the tree, after static, const...); the
        identifier follows a type, a * or a comma of the declarator list, outside an initialiser, and is followed
        by ; , = or [ (or is (*name))."""
        first = next((x for x in stmt if x not in DECL_QUALIFIERS), "")
        if first not in C_TYPE_WORDS and first not in self.types:
            return False
        if fptr and nxt == ")" and depth == 1:
            return True
        last_comma = max((i for i, x in enumerate(stmt[:-1]) if x == ","), default=-1)
        if "=" in stmt[last_comma + 1:-1]:
            return False   # in an initialiser: a use
        return depth == 0 and nxt in (";", ",", "=", "[") and (typeish or prev in ("*", ","))

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
        """asm/us/<table>/symbols.json: its keys renamed (two names of one address made one: one key), the labels
        drop.tsv lists for it removed; rewritten in the table's own style (sorted keys, two spaces), or edited as
        text when it has another (no drops then)."""
        table = SYMBOL_TABLES.match(f).group(1)
        drop = {d.name for d in self.drops if d.table in (table, "*")}
        try:
            d = json.loads(text)
            syms = d["symbols"]
            styled = json.dumps(d, indent=2, sort_keys=True) + "\n" == text
        except (ValueError, KeyError, TypeError):
            styled = False
        if not styled:
            if drop & set(nc.IDENT.findall(text)):
                self.errors.append(f"{f}: not in the tables' JSON style, so its labels can't be dropped")
            self.scan_text(f, text, 0, len(text), edits, "symbol table")
            return
        new_syms, changed = {}, []
        for k, v in syms.items():
            if k in drop:
                self.dropped[k].append(table)
                continue
            row = self.row_for(k)
            nk = row.new if row else k
            if row:
                changed.append(row)
            if nk in new_syms and new_syms[nk] != v:
                self.errors.append(f"{f}: {k} would become {nk}, which the table has at {new_syms[nk]}, not {v}")
            new_syms.setdefault(nk, v)
        if changed or len(new_syms) != len(syms) or any(k in drop for k in syms):
            d["symbols"] = new_syms
            self.edit(f, edits, 0, len(text), json.dumps(d, indent=2, sort_keys=True) + "\n", None, "symbol table")
            for row in changed:
                self.counts[id(row)]["symbol table"] += 1
                self.row_files[id(row)].add(f)

    def plan_symbol_file(self):
        """The renamed address names symbol_addrs.us.txt won't define after the rename, with the address and the
        kind the tools took from the old spelling (n64syms.address_of, kind_of)."""
        p = self.root / SYMBOL_FILE
        if not p.is_file():
            return
        defined = set()
        for line in nc.read_text(p).splitlines():
            m = re.match(r"^\s*([A-Za-z_]\w*)\s*=", line)
            if m:
                r = self.row_for(m.group(1))
                if m.group(1) not in self.symfile_drops:
                    defined.add(r.new if r else m.group(1))
        for key, row in sorted(self.ident.items()):
            addr = nc.address_of_name(row.old)
            if addr is None or row.kind == "type" or row.new in defined:
                continue
            defined.add(row.new)
            self.symfile_add.append((row, addr, "func" if row.old.startswith("func_") else "data"))

    # ------------------------------------------------------------------------------------------------ results
    def new_texts(self):
        """path -> new text for every file the edits change; for the merged aliases, one declaration per file and
        one line in the symbol file."""
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
            if f == SYMBOL_FILE and self.symfile_drops:
                new_text = self.drop_symbol_lines(new_text)
            if self.merged and f.endswith(C_EXTS):
                new_text = self.merge_declarations(f, new_text)
            elif self.merged and f == SYMBOL_FILE:
                new_text = self.merge_symbol_lines(f, new_text)
            if f.endswith(C_EXTS):
                self.find_self_aliases(f, text, new_text)
            if new_text != text:
                out[f] = new_text
        return out

    def drop_symbol_lines(self, text):
        """symbol_addrs.us.txt without the lines of the names drop.tsv lists for it."""
        out = []
        for line in text.split("\n"):
            m = re.match(r"^\s*([A-Za-z_]\w*)\s*=", line)
            if m and m.group(1) in self.symfile_drops:
                self.dropped[m.group(1)].append(nc.SYMBOL_FILE_TABLE)
                continue
            out.append(line)
        return "\n".join(out)

    # an alias and what it names: PC_ALIAS("X", "Y + 4"), .set X, Y (in an asm string), X __attribute__((alias("Y")))
    SELF_ALIAS = (re.compile(r'PC_ALIAS\(\s*"(\w+)"\s*,\s*"\s*([^"]*?)\s*"\s*\)'),
                  re.compile(r'\.set\s+(\w+)\s*,\s*([^"\\]*?)\s*(?:\\n|")'),
                  re.compile(r'\b(\w+)\s*(?:\[[^\]]*\]\s*)*__attribute__\s*\(\(\s*alias\s*\(\s*"(\w+)"'))

    def find_self_aliases(self, f, old_text, new_text):
        """Report the aliases the rename made of a name to itself (.set X, X in an asm string, PC_ALIAS("X", "X"),
        X __attribute__((alias("X")))): they don't assemble; the lead deletes them (NAMING.md 7)."""
        if "alias" not in new_text.lower() and ".set" not in new_text:
            return

        def selfs(text):
            return {(m.group(0), text.count("\n", 0, m.start()) + 1) for rx in self.SELF_ALIAS
                    for m in rx.finditer(text) if m.group(1) == m.group(2)}
        before = {s for s, ln in selfs(old_text)}
        for s, ln in sorted(selfs(new_text), key=lambda x: x[1]):
            if s not in before:
                self.merges.append((f, ln, f"`{s}` names itself after the rename: it won't assemble, delete it by "
                                           f"hand"))

        def defined(text):   # the symbols the file's asm aliases define, with their lines
            out = collections.defaultdict(list)
            for rx in self.SELF_ALIAS[:2]:
                for m in rx.finditer(text):
                    if m.group(1) != m.group(2):
                        out[m.group(1)].append(text.count("\n", 0, m.start()) + 1)
            return out
        was = defined(old_text)
        for name, lines in sorted(defined(new_text).items()):
            if len(lines) > 1 and len(was.get(name, ())) < len(lines):
                self.merges.append((f, lines[1], f"{name} is defined twice by asm aliases after the rename (lines "
                                                 f"{', '.join(map(str, lines))}): delete the second by hand"))

    def merge_declarations(self, f, text):
        """Two names of one symbol made one: where a C file now declares it twice on lines of the same text (and
        indent), the second goes; where two declarations differ, both stay and the difference is reported."""
        names = [n for n in self.merged if n in text]
        if not names:
            return text
        lines = text.split("\n")
        drop = set()
        for name in names:
            # a type (words and stars) before the name: `extern s32 x[4];`, `void x(s16 a);`, never `x(1);`
            decl = re.compile(r"^([ \t]*)(?:extern[ \t]+)?((?:[A-Za-z_]\w*[ \t*]+)+" + re.escape(name)
                              + r"\b[^;{}=\n]*);[ \t]*(?:/\*.*?\*/|//.*)?[ \t]*$")
            seen = {}   # indent -> [(line, normalized declaration)]
            for i, line in enumerate(lines):
                m = decl.match(line) if name in line else None
                if not m or m.group(2).split()[0] in NOT_A_TYPE:
                    continue
                key = re.sub(r"\s+", " ", m.group(2)).strip()
                same = [j for j, k in seen.get(m.group(1), []) if k == key]
                if same:
                    drop.add(i)
                    self.merges.append((f, i + 1, f"dropped a second declaration of {name} (line {same[0] + 1})"))
                else:
                    for j, k in seen.get(m.group(1), []):
                        self.merges.append((f, i + 1, f"declarations of {name} differ: line {j + 1} `{k}`, "
                                                      f"line {i + 1} `{key}`: by hand"))
                    seen.setdefault(m.group(1), []).append((i, key))
        return "\n".join(x for i, x in enumerate(lines) if i not in drop)

    def merge_symbol_lines(self, f, text):
        """The symbol file's second line for one merged name goes (its first stays); another address is reported."""
        lines, out, first = text.split("\n"), [], {}
        for i, line in enumerate(lines):
            m = re.match(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", line)
            if m and m.group(1) in self.merged:
                if m.group(1) in first:
                    if int(first[m.group(1)][1], 16) != int(m.group(2), 16):
                        self.merges.append((f, i + 1, f"{m.group(1)} at {m.group(2)} and at {first[m.group(1)][1]} "
                                                      f"(line {first[m.group(1)][0]}): by hand"))
                        out.append(line)
                    else:
                        self.merges.append((f, i + 1, f"dropped a second line for {m.group(1)}"))
                    continue
                first[m.group(1)] = (i + 1, m.group(2))
            out.append(line)
        return "\n".join(out)


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


def write_text(path, text):
    """Write text as it is: no newline translation (Windows' text mode would write CRLF)."""
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


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
            write_text(sums, "\n".join(new_lines) + "\n")
            out.append(str(sums.relative_to(root)))
    return out


def git_mv(root, old, new):
    (Path(root) / new).parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "-C", str(root), "mv", old, new], check=True, capture_output=True)
    except (OSError, subprocess.CalledProcessError):
        (Path(root) / old).rename(Path(root) / new)


def move_all(root, moves):
    """Move every old path to its new one, as one simultaneous rename: a file whose new path is another moving
    file's old one waits for it (a chain), and a cycle goes through a temporary name. [(old, new)] moved."""
    pending, moved, n = dict(moves), [], 0
    first_name = {}
    while pending:
        progress = False
        for old, new in sorted(pending.items()):
            if not (Path(root) / new).exists():
                git_mv(root, old, new)
                moved.append((first_name.pop(old, old), new))
                del pending[old]
                progress = True
        if not progress:   # a cycle (a swap): one of them to a temporary name first
            old, new = sorted(pending.items())[0]
            n += 1
            tmp = f"{old}.rename-{n}"
            git_mv(root, old, tmp)
            first_name[tmp] = first_name.pop(old, old)
            del pending[old]
            pending[tmp] = new
    return moved


def write_ledger(root, rows):
    """Add the rows applied to tools/port/names/applied.tsv (kind, old, new): with it names_check.py and a second
    --apply know them applied, which a chain's names alone can't tell."""
    p = Path(root) / nc.MAPS_DIR / nc.APPLIED_FILE
    p.parent.mkdir(parents=True, exist_ok=True)
    have = nc.read_ledger(root)
    new = sorted({nc.row_key(r) for r in rows if r.kind != "field"} - have)
    if not new:
        return 0
    text = nc.read_text(p) if p.is_file() else (
        "# The rows tools/port/rename.py applied (kind, old, new), written by --apply: names_check.py and a second\n"
        "# --apply read them as applied (a chain, A to B and B to C, can't be told by its names).\n")
    write_text(p, text + "".join("\t".join(k) + "\n" for k in new))
    return len(new)


def remove_empty_dirs(root, paths):
    """Remove the folders the moves left empty, up to the top of the tree."""
    root = Path(root).resolve()
    for p in sorted({(root / x).parent for x in paths}, key=lambda d: -len(d.parts)):
        d = p
        while d != root and d.is_dir() and not any(d.iterdir()):
            d.rmdir()
            d = d.parent


# ---------------------------------------------------------------------------------------------------- the modes

def describe(plan, verbose=False, report=None):
    """The dry run's report: per row, per file, and what is left; the report file gets every change."""
    kinds = collections.Counter(r.kind for r in plan.rows)
    moves_of = collections.defaultdict(list)
    for o, (n, r, how) in sorted(plan.moves.items()):
        moves_of[id(r)].append((o, n, how))
    sym = {id(r): (a, k) for r, a, k in plan.symfile_add}
    left_by_row = collections.defaultdict(collections.Counter)
    for f, items in plan.left.items():
        for ln, cat, tok, row, ex in items:
            if row is not None:
                left_by_row[id(row)][cat] += 1
    implied = sum(1 for n, r, how in plan.moves.values() if how == "implied")
    out = [f"rename.py: {len(plan.rows)} rows ({', '.join(f'{k} {kinds[k]}' for k in nc.KINDS if kinds[k])}), "
           f"{implied} implied file moves" + (f", {len(plan.drops)} labels to drop" if plan.drops else "")
           + f"; {len(plan.edits)} files change", "",
           "per row (old -> new [kind]: changes by place; what else; what is left):"]
    for r in plan.rows:
        if r.kind == "field":
            out.append(f"  {r.old} -> {r.new} [field]: LEFT for wave 2: a field is renamed by hand, in the struct "
                       f"and its users (it needs the struct's layout)  ({r.where()})")
            continue
        c = plan.counts.get(id(r), {})
        extra = []
        if id(r) in sym:
            a, k = sym[id(r)]
            extra.append(f"{SYMBOL_FILE} += {r.new} = 0x{a:08X} type:{k}")
        moves = moves_of.get(id(r), [])
        if r.kind == "file" and len(moves) > 1:
            extra.append(f"moves the folder {r.old} -> {r.new} ({len(moves)} files)")
        else:
            extra += [f"moves {o} -> {n}" for o, n, how in moves]
        if r.new in plan.merged and plan.merged[r.new][0] is r:
            extra.append(f"one symbol with {', '.join(x.old for x in plan.merged[r.new][1:])}")
        lft = left_by_row.get(id(r), {})
        out.append(f"  {r.old} -> {r.new} [{r.kind}]: {sum(c.values())} in {len(plan.row_files.get(id(r), ()))} files"
                   + (f" ({', '.join(f'{k} {v}' for k, v in sorted(c.items()) if v)})" if any(c.values()) else "")
                   + "".join(f"; {x}" for x in extra)
                   + (f"; left: {', '.join(f'{k} {v}' for k, v in sorted(lft.items()))}" if lft else ""))
    if plan.drops:
        out += ["", "dropped labels (drop.tsv):"]
        for d in plan.drops:
            tables = plan.dropped.get(d.name, [])
            out.append(f"  {d.name} [{d.table}]: " + (f"leaves {', '.join(tables)}" if tables else "in no table"))
    out += ["", "per file:"]
    for f in sorted(plan.edits):
        text, edits = plan.edits[f]
        c = collections.Counter(row.old for s, e, n, row, cat in edits if row is not None)
        if SYMBOL_TABLES.match(f):
            c = collections.Counter({r.old: 1 for r in plan.rows if f in plan.row_files.get(id(r), ())
                                     and plan.counts[id(r)].get("symbol table")})
            dropped = [n for n, ts in plan.dropped.items() if SYMBOL_TABLES.match(f).group(1) in ts]
            if dropped:
                c["(dropped labels)"] = len(dropped)
        out.append(f"  {f}: " + ", ".join(f"{o} x{k}" for o, k in sorted(c.items())))
    left_items = [(f, x) for f in sorted(plan.left) for x in plan.left[f]]
    out += ["", f"left for hand edits ({len(left_items)}):"]
    shown = left_items if verbose else left_items[:40]
    out += [f"  {f}:{ln}: {cat}: {tok}: {ex}" for f, (ln, cat, tok, row, ex) in shown]
    if len(shown) < len(left_items):
        out.append(f"  ... {len(left_items) - len(shown)} more (-v, or --report FILE)")
    if plan.merges:
        out += ["", "aliases made one symbol (and names an alias gave itself):"]
        differ = collections.defaultdict(list)
        for f, ln, what in plan.merges:
            m = re.match(r"declarations of (\w+) differ", what)
            if m:
                differ[m.group(1)].append((f, ln, what))
            else:
                out.append(f"  {f}:{ln}: {what}")
        for name, items in sorted(differ.items(), key=lambda x: -len(x[1])):
            f, ln, what = items[0]
            out.append(f"  declarations of {name} differ in {len(items)} places (by hand), e.g. {f}:{ln}: "
                       f"{what.split(': ', 1)[1]}")
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
        for o, (n, r, how) in sorted(plan.moves.items()):
            lines.append(f"{o}: moved to {n}" + {"implied": " (implied by its function's row)",
                                                  "folder": " (its folder's row)"}.get(how, ""))
        for name, tables in sorted(plan.dropped.items()):
            lines.append(f"{name}: dropped from {', '.join(tables)}")
        lines += [f"{f}:{ln}: {what}" for f, ln, what in plan.merges]
        lines += [f"{f}:{ln}: LEFT {cat}: {tok}: {ex}" for f, (ln, cat, tok, row, ex) in left_items]
        lines += [f"{f}:{ln}: LEFT token pasting: {macro}: {what}" for f, ln, macro, what, row in plan.pastes]
        lines += [f"{f}:{ln}: LEFT derived identifier: {tok}" for f, ln, tok, row in plan.derived]
        Path(report).write_text("\n".join(lines) + "\n")
        out += ["", f"every change: {report}"]
    return "\n".join(out)


def apply(plan, texts):
    """Write the new texts, add the symbol file's names, move the files, update the checksums."""
    for f, t in texts.items():
        write_text(plan.root / f, t)
    changed = set(texts)
    if plan.symfile_add:
        p = plan.root / SYMBOL_FILE
        text = nc.read_text(p)
        lines = [] if text.endswith("\n") else [""]
        header = "// Names from the name maps (docs/port/PCONLY.md 3, tools/port/rename.py):"
        if header not in text:
            lines += ["", header, "// the N64 address and the kind their old name spelled, which the tools read from it."]
        lines += [f"{row.new} = 0x{addr:08X}; // type:{kind}" for row, addr, kind in plan.symfile_add]
        write_text(p, text + "\n".join(lines) + "\n")
        changed.add(SYMBOL_FILE)
    moved = move_all(plan.root, {o: n for o, (n, row, how) in plan.moves.items() if (plan.root / o).exists()})
    remove_empty_dirs(plan.root, [o for o, n in moved])
    m = dict(moved)
    sums = update_checksums(plan.root, [m.get(f, f) for f in changed])
    return moved, sums


# What rename.py leaves because it is another thing of the old name's spelling, not the symbol: --check lists it
# apart and doesn't count it.
OTHER_THINGS = {"struct member", "macro parameter", "parameter of the same name", "local of the same name",
                "use of a local of the same name", "comment in a file that has the name only as another thing"}


def check(plan):
    """(hits, others) after plan.scan(): every place the old names still name their symbols (code, comments,
    strings, paths, other spellings, derived and pasted names), every file or folder still to move, every dropped
    label still in a table, every address still to add: [(path, line, token, what)]; and the places of the old
    names' spellings that are other things (members, parameters, locals), which stay."""
    hits, others = [], []
    for f, (text, edits) in sorted(plan.edits.items()):
        for s, e, new, row, cat in sorted(edits, key=lambda x: x[0]):
            if row is not None:
                hits.append((f, plan.line_of(text, s), text[s:e] if e - s < 80 else row.old, f"{cat}: {row.old}"))
    for f, items in sorted(plan.left.items()):
        for ln, cat, tok, row, ex in items:
            (others if cat in OTHER_THINGS else hits).append((f, ln, tok, cat))
    hits += [(f, ln, what, f"token pasting in {macro}") for f, ln, macro, what, row in plan.pastes
             if nc.IDENT_FULL.match(what)]   # an invocation that makes the name, not a macro that could
    hits += [(f, ln, tok, "derived identifier") for f, ln, tok, row in plan.derived]
    hits += [(old, 0, "", f"to move to {new}") for old, (new, row, how) in sorted(plan.moves.items())]
    hits += [(f"asm/us/{t}/symbols.json", 0, name, "a dropped label") for name, ts in sorted(plan.dropped.items())
             for t in ts]
    hits += [(SYMBOL_FILE, 0, row.new, "its address to add") for row, a, k in plan.symfile_add]
    return hits, others


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("maps", nargs="*", help="the maps and drop list (default: every tools/port/names/*.tsv)")
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
    ap.add_argument("--no-names-check", action="store_true", help="don't run names_check.py's checks of the maps")
    ap.add_argument("--force", action="store_true",
                    help="apply in spite of names_check.py's errors (the collisions left for hand edits); the "
                         "rows' format errors and conflicts in the tables still stop it")
    ap.add_argument("--report", type=Path, help="write every change and everything left to this file")
    ap.add_argument("-v", "--verbose", action="store_true", help="list everything left, not the first 40")
    a = ap.parse_args(argv)
    root = a.root.resolve()
    paths = nc.map_paths(root, a.maps)
    if not paths:
        print(f"rename.py: no maps in {root / nc.MAPS_DIR}")
        return 0
    maps, drop_files = nc.split_paths(paths)
    files = scope_files(root, a.exclude)
    rows, format_errs = nc.read_maps(maps)
    drops, drop_errs = nc.read_drops(drop_files)
    format_errs += drop_errs + [(r.where(), m) for r in rows for m in nc.check_format(r)]
    rows = [r for r in rows if not nc.check_format(r)]
    ledger = nc.read_ledger(root)
    tree = nc.Tree(root, watch={r.new for r in rows if r.kind in nc.IDENT_KINDS} | {d.name for d in drops})
    if a.check:
        for where, msg in format_errs:
            print(f"{where}: error: {msg}")
        # A chain's middle (B in A to B, B to C) is a new name after the rename: by its name it can't be checked.
        news = {r.new for r in rows if r.kind in nc.IDENT_KINDS}
        middles = [r for r in rows if r.kind in nc.IDENT_KINDS and r.old in news]
        plan = Plan(root, [r for r in rows if r not in middles], files, follow_files=not a.no_follow_files,
                    strings_all=a.strings == "all", types=tree.types, drops=drops)
        if not plan.errors:
            plan.scan()
        hits, others = check(plan)
        for f, ln, tok, what in hits:
            print(f"{f}:{ln}: {tok} ({what})")
        if a.verbose:
            for f, ln, tok, what in others:
                print(f"{f}:{ln}: {tok} ({what}: another thing, it stays)")
        for e in plan.errors:
            print(f"rename.py: error: {e}")
        nfield = sum(1 for r in rows if r.kind == "field")
        print(f"rename.py --check: {len(hits)} places still have an old name"
              + (f"; {len(others)} places of the old spellings are other things (members, parameters, locals: -v)"
                 if others else "")
              + (f"; {len(middles)} rows a chain's middle (their old name is another row's new one), not checked"
                 if middles else "")
              + (f"; {nfield} field rows not checked (renamed by hand)" if nfield else ""))
        return 1 if hits or format_errs or plan.errors else 0
    if format_errs:
        for where, msg in format_errs:
            print(f"{where}: error: {msg}")
        print(f"rename.py: the maps have {len(format_errs)} format errors; nothing done")
        return 1
    errs, applied = [], []
    if not a.no_names_check or drops:
        _, errs, _, applied, _, _ = nc.run_checks(root, maps + drop_files, tree)
        if a.no_names_check:   # the drop list's checks all the same
            errs = [(w, m) for w, m in errs if w.rsplit(":", 1)[0] in {str(p) for p in drop_files}]
    if applied:
        print(f"rename.py: {len(applied)} rows applied already")
    if errs:
        for where, msg in errs:
            print(f"{where}: error: {msg}")
        if not a.force:
            print(f"rename.py: the maps have {len(errs)} errors (names_check.py); nothing done (--force applies "
                  f"them all the same)")
            return 1
        print(f"rename.py: the maps have {len(errs)} errors (names_check.py); --force: applied all the same")
    rows = [r for r in rows if nc.row_key(r) not in ledger]   # the ledger's rows were applied: not again
    plan = Plan(root, rows, files, follow_files=not a.no_follow_files, strings_all=a.strings == "all",
                types=tree.types, drops=drops)
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
    logged = write_ledger(root, rows) if (texts or moved or plan.dropped) else 0
    if a.report:
        describe(plan, a.verbose, a.report)
    left = sum(len(v) for v in plan.left.values())
    print(f"rename.py: {len(texts)} files changed, {len(moved)} moved"
          + (f", {len(plan.symfile_add)} names added to {SYMBOL_FILE}" if plan.symfile_add else "")
          + (f", {len(plan.dropped)} labels dropped" if plan.dropped else "")
          + (f", checksums updated in {', '.join(sums)}" if sums else "")
          + (f", {logged} rows in {nc.MAPS_DIR / nc.APPLIED_FILE}" if logged else "")
          + f"; left for hand edits: {left} places, {len(plan.merges)} alias notes, {len(plan.pastes)} pasted names, "
          f"{len(plan.derived)} derived identifiers, {sum(1 for r in rows if r.kind == 'field')} field rows "
          f"(--dry-run lists them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
