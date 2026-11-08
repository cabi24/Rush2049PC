#!/usr/bin/env python3
"""tools/port/rename.py on its fixture (tools/port/tests/names: tree/, map.tsv and drop.tsv, made-up names), as
ctest's rename_tool runs it:

    python3 tools/port/tests/test_rename.py [-v]

Each test copies the fixture tree to a temporary folder and runs rename.py there: every place a name lives (the C's
code, comments and symbol-name strings, asm and alias strings, #include lines, the symbol tables and their checksums,
symbol_addrs.us.txt and the address an old name spelled, the port's C, a tool, a suppression, extents.txt, CMake's
lists of stems and of paths, the layout, the docs), a folder row, two aliases made one symbol, a dropped label, and
what is left and reported (struct members, a macro's parameter, a parameter and a local of the same spelling,
another spelling, a string that only mentions a name, a folder named after a function, token pasting, derived names,
field rows); --check; a second --apply that changes nothing; git mv in a git tree; the refusals; and a map of 5,000
rows over a tree of 400 files. Stdlib only, Python 3.9+.
"""
import contextlib
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import rename  # noqa: E402

FIXTURE = HERE / "names" / "tree"
MAP = HERE / "names" / "map.tsv"
DROP = HERE / "names" / "drop.tsv"
HEADER = "old\tnew\tkind\tsource\tconfidence\tnote\n"


def run(*args):
    """(exit status, output) of rename.py's main."""
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        status = rename.main([str(a) for a in args])
    return status, out.getvalue()


def snapshot(root):
    return {str(p.relative_to(root)): p.read_bytes() for p in sorted(Path(root).rglob("*"))
            if p.is_file() and ".git" not in p.parts}


class Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="rename_test_"))
        self.root = self.tmp / "tree"
        shutil.copytree(FIXTURE, self.root)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def text(self, rel):
        return (self.root / rel).read_text()

    def apply(self, *maps):
        status, out = run("--root", self.root, "--apply", *(maps or (MAP, DROP)))
        self.assertEqual(status, 0, out)
        return out

    def write_map(self, rows, name="m.tsv"):
        p = self.tmp / name
        p.write_text(HEADER + "".join("\t".join(r) + "\n" for r in rows))
        return p


class DryRun(Fixture):
    def test_reports_without_changing(self):
        before = snapshot(self.root)
        report = self.tmp / "report.txt"
        status, out = run("--root", self.root, "--dry-run", "--report", report, MAP, DROP)
        self.assertEqual(status, 0, out)
        self.assertEqual(snapshot(self.root), before, "a dry run changed the tree")
        for want in ("11 rows (func 3, data 4, type 1, field 1, file 2), 2 implied file moves, 1 labels to drop",
                     "func_80F00010 -> thing_count_read [func]",
                     "moves src/blob/func_80F00010.c -> src/blob/thing_count_read.c",
                     "symbol_addrs.us.txt += thing_table = 0x80F00100 type:data",
                     "OldThing.0x0 -> total [field]: LEFT for wave 2",
                     "moves the folder src/blob/groups/old_group -> src/blob/groups/new_group (2 files)",
                     "D_80F00600 -> game_state [data]", "one symbol with state_word_a",
                     "zeta_unused [blob]: leaves blob",
                     "other spelling: func_80f00010", "string: func_80F00010",
                     "path: func_80F00010: src/blob/groups/func_80F00010/group.c",
                     "PASTE: func_80F00010", "func_80F00010_1 (func_80F00010 -> thing_count_read)",
                     "src/blob/alias_a.c:3: dropped a second declaration of game_state (line 2)",
                     "declarations of game_state differ in 1 places (by hand), e.g. src/blob/alias_b.c:3: "
                     "line 2 `s32 game_state`, line 3 `u32 game_state`: by hand"):
            self.assertIn(want, out)
        lines = report.read_text()
        self.assertIn("port/main.c:6: string (name): D_80F00100 -> thing_table", lines)
        self.assertIn("src/blob/func_80F00010.c: moved to src/blob/thing_count_read.c (implied", lines)
        self.assertIn("src/blob/groups/old_group/member.c: moved to src/blob/groups/new_group/member.c (its folder's",
                      lines)
        self.assertIn("zeta_unused: dropped from blob", lines)


class Apply(Fixture):
    def test_every_place(self):
        out = self.apply()
        self.assertIn("19 files changed, 5 moved, 2 names added to symbol_addrs.us.txt, 1 labels dropped", out)
        # the function's file follows its name; the header moves; the draft CMake lists by stem follows its name
        self.assertFalse((self.root / "src/blob/func_80F00010.c").exists())
        self.assertFalse((self.root / "include/game/old_header.h").exists())
        self.assertTrue((self.root / "nonmatching/game/thing_draft.c").exists())
        c = self.text("src/blob/thing_count_read.c")
        for want in ('/* thing_count_read.c: the fixture\'s function (src/blob/thing_count_read.c).',
                     '#include "game/thing.h"', "#include <game/thing.h>",
                     "extern s32 thing_table[4];",
                     "extern void func_80f00010(s32 a, s32 b);",           # another spelling: left
                     "extern s32 func_80F00010_1;",                         # derived: left
                     "#define PASTE(x) func_80F000##x",                     # pasting: left
                     '{ "thing_count_read", "thing_table", "calls func_80F00010 here" }',   # the mention: left
                     '.set D_80F00104, thing_table + 4"',                   # asm string
                     '__attribute__((alias("thing_table")))',
                     "/* Thing.count is read below; see thing_table/2C. */",
                     "s32 thing_count_read(Thing *thing)", 'PC_SPIN("thing_count_read")',
                     "thing->count + thing_table[0] + thing_assist(PASTE(10))"):
            self.assertIn(want, c)
        h = self.text("include/game/thing.h")
        self.assertIn("typedef struct Thing {", h)
        self.assertIn("} Thing;", h)
        self.assertIn("s32 thing_count_read(Thing *thing);", h)
        self.assertIn("#ifndef OLD_HEADER_H", h)   # an include guard is derived text: left
        group = self.text("src/blob/groups/func_80F00010/group.c")   # the folder keeps its name
        self.assertIn("return thing_count_read(0);", group)
        cmake = self.text("CMakeLists.txt")
        self.assertEqual(cmake.splitlines()[1], "foreach(f thing_draft other_draft)")
        self.assertIn("add_executable(fixture src/blob/thing_count_read.c include/game/thing.h "
                      "src/blob/groups/new_group/member.c)", cmake)
        doc = self.text("docs/port/THING.md")
        self.assertIn("`thing_count_read` (src/blob/thing_count_read.c) reads thing_table/2C and Thing.count", doc)
        self.assertIn("src/blob/groups/func_80F00010/group.c. See thing_count_read.c and blob/thing_count_read.c",
                      doc)
        self.assertIn("include/game/thing.h.", doc)
        self.assertIn("The folder src/blob/groups/new_group holds member.c (groups/new_group/member.c).", doc)
        self.assertIn('{"thing_table", thing_table}', self.text("port/main.c"))
        self.assertIn('NAMES = ("thing_table", "thing_assist")', self.text("tools/port/gen_thing.py"))
        self.assertIn("bounds:^thing_count_read$", self.text("tools/port/sanitize/ubsan.supp"))
        self.assertIn("thing_table 0x10", self.text("port/data/extents.txt"))
        self.assertEqual([e["target_id"] for e in json.loads(self.text("layout/game.json"))["regions"][0]["entries"]],
                         ["thing_count_read", "thing_draft"])
        # the symbol table: keys renamed, the aliases one key, the dropped label gone, in its own style; its
        # checksum line follows, and the assembly's
        table = self.text("asm/us/blob/symbols.json")
        d = json.loads(table)
        self.assertEqual(d["symbols"], {"gTargetInfo": "0x80F00800", "game_state": "0x80F00600",
                                        "thing_assist": "0x80F00200", "thing_count_read": "0x80F00010",
                                        "thing_table": "0x80F00100", "zeta_keep": "0x80F00300"})
        self.assertEqual(table, json.dumps(d, indent=2, sort_keys=True) + "\n")
        sums = dict(reversed(line.split("  ")) for line in self.text("asm/us/blob/SHA256SUMS").splitlines())
        self.assertEqual(sums["symbols.json"], hashlib.sha256(table.encode()).hexdigest())
        region = self.text("asm/us/blob/region.s")   # assembly labels are names too; its checksum follows
        self.assertIn(".globl thing_count_read", region)
        self.assertEqual(sums["region.s"], hashlib.sha256(region.encode()).hexdigest())
        # symbol_addrs.us.txt: named entries renamed in place; the address names gain their address and kind
        syms = self.text("symbol_addrs.us.txt")
        self.assertIn("thing_assist = 0x80F00200; // type:func a named function", syms)
        self.assertIn("thing_draft = 0x80F00400; // type:func", syms)
        self.assertIn("thing_table = 0x80F00100; // type:data", syms)
        self.assertIn("thing_count_read = 0x80F00010; // type:func", syms)
        self.assertEqual(syms.count("game_state = 0x80F00600;"), 1)   # the alias' line, renamed; none added
        self.assertNotIn("D_80F00100", syms)

    def test_other_things_of_the_same_spelling(self):
        """Struct members, a macro's parameter, a function's parameter, a local and its uses stay; the globals
        are renamed. port/gfx's Windows member, and the comment that names it, stay."""
        self.apply()
        s = self.text("src/blob/shadow.c")
        for want in ("extern s32 gTargetInfo;", "extern s32 thing_table[4];",
                     "    s32 targetInfo;                /* a member */",
                     "    s32 (*D_80F00100)(void);       /* a function pointer member */",
                     "#define USE(D_80F00100) ((D_80F00100) + gTargetInfo)",
                     "static Win gWin = { .targetInfo = 1 };",
                     "return w->targetInfo + v.targetInfo + gTargetInfo;",
                     "s32 shadow_param(s32 D_80F00100)\n{\n    return D_80F00100 + 1;",
                     "    s32 targetInfo = 2;\n    {\n        targetInfo++;\n    }\n    return targetInfo + USE(3);",
                     "s32 sum = gTargetInfo + thing_table[1];"):
            self.assertIn(want, s)
        self.assertEqual(self.text("port/gfx/gfx_display.c"), (FIXTURE / "port/gfx/gfx_display.c").read_text())

    def test_aliases_made_one(self):
        self.apply()
        self.assertEqual(self.text("src/blob/alias_a.c").count("extern s32 game_state;"), 1)
        self.assertIn("return game_state + game_state;", self.text("src/blob/alias_a.c"))
        b = self.text("src/blob/alias_b.c")   # the two types: both stay, for a hand edit
        self.assertIn("extern s32 game_state;\nextern u32 game_state;", b)

    def test_folder(self):
        self.apply()
        self.assertFalse((self.root / "src/blob/groups/old_group").exists())
        self.assertTrue((self.root / "src/blob/groups/new_group/member.c").exists())
        self.assertIn('"dir": "src/blob/groups/new_group"', self.text("src/blob/groups/new_group/group.json"))

    def test_file_row_in_a_moved_folder(self):
        """A file row names the folders as they were: a folder row's move takes the file along."""
        m = self.write_map([("src/blob/groups/old_group", "src/blob/groups/new_group", "file", "code", "sure", "x"),
                            ("src/blob/groups/old_group/member.c", "src/blob/groups/old_group/renamed.c", "file",
                             "code", "sure", "a file of the folder")])
        self.apply(m)
        self.assertTrue((self.root / "src/blob/groups/new_group/renamed.c").exists())
        self.assertFalse((self.root / "src/blob/groups/old_group").exists())
        self.assertIn("src/blob/groups/new_group/renamed.c", self.text("CMakeLists.txt"))

    def test_again_changes_nothing(self):
        self.apply()
        after = snapshot(self.root)
        out = self.apply()
        self.assertIn("10 rows applied already", out)
        self.assertIn("0 files changed, 0 moved", out)
        self.assertEqual(snapshot(self.root), after)

    def test_check(self):
        status, out = run("--root", self.root, "--check", MAP, DROP)
        self.assertEqual(status, 1)
        self.apply()
        status, out = run("--root", self.root, "--check", "-v", MAP, DROP)
        self.assertEqual(status, 1, out)   # what is left for hand edits
        for want in ("src/blob/thing_count_read.c:6: func_80f00010 (other spelling)",
                     "src/blob/thing_count_read.c:10: func_80F00010 (string)",
                     "docs/port/THING.md:4: func_80F00010 (path)",
                     "src/blob/thing_count_read.c:18: func_80F00010 (token pasting in PASTE)",
                     "src/blob/thing_count_read.c:7: func_80F00010_1 (derived identifier)",
                     "src/blob/shadow.c:27: targetInfo (local of the same name: another thing, it stays)",
                     "rename.py --check: 5 places still have an old name; 16 places of the old spellings are other "
                     "things"):
            self.assertIn(want, out)

    def test_check_clean(self):
        clean = self.write_map([("helper_old_name", "thing_assist", "func", "code", "sure", "a named function")],
                               "clean.tsv")
        self.apply(clean, DROP)
        status, out = run("--root", self.root, "--check", clean, DROP)
        self.assertEqual(status, 0, out)
        self.assertIn("0 places still have an old name", out)

    def test_git_mv(self):
        if not shutil.which("git"):
            self.skipTest("no git")
        g = ["git", "-C", str(self.root), "-c", "user.name=t", "-c", "user.email=t@example.invalid"]
        subprocess.run(g + ["init", "-q"], check=True)
        subprocess.run(g + ["add", "-A"], check=True)
        subprocess.run(g + ["commit", "-q", "-m", "fixture"], check=True)
        self.apply()
        status = subprocess.run(g + ["status", "--porcelain"], capture_output=True, text=True, check=True).stdout
        moves = {line[3:] for line in status.splitlines() if line.startswith("R")}   # RM: moved, then edited
        for want in ("include/game/old_header.h -> include/game/thing.h",
                     "src/blob/func_80F00010.c -> src/blob/thing_count_read.c",
                     "nonmatching/game/draft_old.c -> nonmatching/game/thing_draft.c",
                     "src/blob/groups/old_group/member.c -> src/blob/groups/new_group/member.c"):
            self.assertIn(want, moves)


class Refusals(Fixture):
    def test_bad_map_changes_nothing(self):
        before = snapshot(self.root)
        m = self.write_map([("func_80F00010", "zeta_keep", "func", "code", "sure", "collides")])
        status, out = run("--root", self.root, "--apply", m)
        self.assertEqual(status, 1)
        self.assertIn("new zeta_keep already exists", out)
        self.assertEqual(snapshot(self.root), before)

    def test_drop_of_a_used_label(self):
        with open(self.root / "src/blob/alias_a.c", "a") as f:
            f.write("\ns32 read_keep(void)\n{\n    return zeta_keep;\n}\n")
        before = snapshot(self.root)
        d = self.tmp / "drop.tsv"
        d.write_text("name\ttable\tnote\nzeta_keep\tblob\ta function reads it\n")
        status, out = run("--root", self.root, "--apply", MAP, d)
        self.assertEqual(status, 1)
        self.assertIn("zeta_keep is still used in the C: src/blob/alias_a.c", out)
        self.assertEqual(snapshot(self.root), before)

    def test_drop_of_a_declared_label(self):
        d = self.tmp / "drop.tsv"
        d.write_text("name\ttable\tnote\nzeta_keep\tblob\tthe header only declares it\n")
        self.apply(MAP, d)
        self.assertNotIn("zeta_keep", self.text("asm/us/blob/symbols.json"))
        self.assertIn("extern s32 zeta_keep;", self.text("include/game/thing.h"))   # the declaration: by hand

    def test_move_onto_a_file(self):
        before = snapshot(self.root)
        m = self.write_map([("include/game/old_header.h", "src/blob/func_80F00010.c", "file", "code", "sure",
                             "onto a file")])
        status, out = run("--root", self.root, "--apply", "--no-names-check", m)
        self.assertEqual(status, 1, out)
        self.assertIn("which exists", out)
        self.assertEqual(snapshot(self.root), before)

    def test_no_follow_files(self):
        m = self.write_map([("func_80F00010", "thing_count_read", "func", "code", "sure", "x")])
        status, out = run("--root", self.root, "--apply", "--no-follow-files", m)
        self.assertEqual(status, 0, out)
        self.assertTrue((self.root / "src/blob/func_80F00010.c").exists())
        self.assertIn("add_executable(fixture src/blob/func_80F00010.c", self.text("CMakeLists.txt"))

    def test_strings_all(self):
        m = self.write_map([("func_80F00010", "thing_count_read", "func", "code", "sure", "x")])
        status, out = run("--root", self.root, "--apply", "--strings", "all", m)
        self.assertEqual(status, 0, out)
        self.assertIn('"calls thing_count_read here"', self.text("src/blob/thing_count_read.c"))


class Chains(Fixture):
    """All rows are one simultaneous substitution: a row's new name may be another row's old name."""

    def setUp(self):
        super().setUp()
        (self.root / "src/blob/draft_old.c").write_text("s32 draft_old(void);\n")
        (self.root / "src/blob/helper_old_name.c").write_text("s32 helper_old_name(s32 x) { return x; }\n")

    def test_chain(self):
        m = self.write_map([("draft_old", "thing_draft", "func", "code", "sure", "B to C"),
                            ("helper_old_name", "draft_old", "func", "code", "sure", "A to B")])
        out = self.apply(m)
        self.assertIn("rows in tools/port/names/applied.tsv", out)
        self.assertEqual(self.text("nonmatching/game/thing_draft.c").splitlines()[1:3],
                         ["s32 thing_draft(void)", "{"])
        self.assertIn("return draft_old(1);", self.text("nonmatching/game/thing_draft.c"))
        # the file chain in one folder: draft_old.c became thing_draft.c before helper_old_name.c became draft_old.c
        self.assertEqual(self.text("src/blob/thing_draft.c"), "s32 thing_draft(void);\n")
        self.assertEqual(self.text("src/blob/draft_old.c"), "s32 draft_old(s32 x) { return x; }\n")
        self.assertFalse((self.root / "src/blob/helper_old_name.c").exists())
        syms = self.text("symbol_addrs.us.txt")
        self.assertIn("draft_old = 0x80F00200; // type:func a named function", syms)
        self.assertIn("thing_draft = 0x80F00400; // type:func", syms)
        after = snapshot(self.root)
        out = self.apply(m)   # the ledger: applied, not again (by the names alone the chain looks undone)
        self.assertIn("2 rows applied already", out)
        self.assertEqual(snapshot(self.root), after)
        status, out = run("--root", self.root, "--check", m)
        self.assertEqual(status, 0, out)
        self.assertIn("1 rows a chain's middle", out)

    def test_swap(self):
        m = self.write_map([("draft_old", "helper_old_name", "func", "code", "sure", "a swap"),
                            ("helper_old_name", "draft_old", "func", "code", "sure", "a swap")])
        self.apply(m)
        self.assertEqual(self.text("src/blob/helper_old_name.c"), "s32 helper_old_name(void);\n")
        self.assertEqual(self.text("src/blob/draft_old.c"), "s32 draft_old(s32 x) { return x; }\n")
        self.assertEqual([p.name for p in (self.root / "src/blob").glob("*.rename-*")], [])


class Tables(Fixture):
    def test_symbol_file_drop(self):
        with open(self.root / "symbol_addrs.us.txt", "a") as f:
            f.write("stale_name = 0x80F00B00; // a stale guess\n")
        m = self.write_map([("D_80F00100", "stale_name", "data", "code", "sure", "the name, at its true address")])
        status, out = run("--root", self.root, "--dry-run", m)
        self.assertEqual(status, 1)
        self.assertIn("new stale_name already exists: named in the tables (symbol file) at 0x80F00B00", out)
        d = self.tmp / "drop.tsv"
        d.write_text("name\ttable\tnote\nstale_name\tsymbol_addrs\ta stale guess: the map gives the name\n")
        self.apply(m, d)
        syms = self.text("symbol_addrs.us.txt")
        self.assertNotIn("0x80F00B00", syms)
        self.assertIn("stale_name = 0x80F00100; // type:data", syms)

    def test_dropped_name_given_again(self):
        """drop.tsv takes a stale label out; a map gives its name to another address; a second --apply must not
        drop the new entry (the ledger keeps the drop)."""
        m = self.write_map([("D_80F00100", "zeta_unused", "data", "code", "sure", "the name, at its true address")])
        self.apply(m, DROP)
        table = json.loads(self.text("asm/us/blob/symbols.json"))["symbols"]
        self.assertEqual(table["zeta_unused"], "0x80F00100")
        self.assertIn("drop\tzeta_unused\tblob", self.text("tools/port/names/applied.tsv"))
        after = snapshot(self.root)
        out = self.apply(m, DROP)
        self.assertIn("0 files changed, 0 moved", out)
        self.assertEqual(snapshot(self.root), after)
        status, out = run("--root", self.root, "--check", m, DROP)
        self.assertEqual(status, 0, out)

    def test_self_alias(self):
        (self.root / "src/rom").mkdir(parents=True)
        (self.root / "src/rom/sdk.c").write_text(
            '#define PC_ALIAS(name, target) ".globl " name "\\n.set " name ", " target "\\n"\n'
            '__asm__(PC_ALIAS("D_80F00600", "state_word_a")\n'
            '        ".set D_80F00600, state_word_a\\n");\n')
        status, out = run("--root", self.root, "--dry-run", MAP)
        self.assertEqual(status, 0, out)
        self.assertIn('src/rom/sdk.c:2: `PC_ALIAS("game_state", "game_state")` names itself after the rename', out)
        self.assertIn("src/rom/sdk.c:3: `.set game_state, game_state\\n` names itself", out)


class Scale(unittest.TestCase):
    """5,000 rows over 400 files: one pass over the tree, so the rows cost dictionary lookups, not greps."""

    def test_5000_rows(self):
        tmp = Path(tempfile.mkdtemp(prefix="rename_scale_"))
        try:
            root = tmp / "tree"
            n, files = 5000, 400
            names = [f"D_80A{i:05X}" if i % 2 else f"func_80B{i:05X}" for i in range(n)]
            syms = {}
            for f in range(files):
                pick = [names[(f * 37 + k * 101) % n] for k in range(60)]
                body = [f"/* src/blob/f{f}.c: uses {pick[0]} and {pick[1]} */"]
                body += [f"extern int {x}[4];" if x.startswith("D_") else f"int {x}(int a);" for x in pick]
                body += [f"int f{f}(void) {{ return {pick[2]}(1) + {pick[3]}[0]; }}"]
                (root / "src/blob").mkdir(parents=True, exist_ok=True)
                (root / f"src/blob/f{f}.c").write_text("\n".join(body) + "\n")
            for i, x in enumerate(names):
                syms[x] = f"0x{0x80A00000 + i * 4:08X}"
            (root / "asm/us/blob").mkdir(parents=True)
            (root / "asm/us/blob/symbols.json").write_text(
                json.dumps({"symbols": syms}, indent=2, sort_keys=True) + "\n")
            (root / "symbol_addrs.us.txt").write_text("// scale\n")
            (root / "docs").mkdir()
            (root / "docs/NAMES.md").write_text(" ".join(names) + "\n")
            rows = ["old\tnew\tkind\tsource\tconfidence\tnote"]
            rows += [f"{x}\tname_{i:04d}_{'datum' if x.startswith('D_') else 'call'}\t"
                     f"{'data' if x.startswith('D_') else 'func'}\tcode\tlikely\tscale" for i, x in enumerate(names)]
            m = tmp / "scale.tsv"
            m.write_text("\n".join(rows) + "\n")
            t = time.monotonic()
            status, out = run("--root", root, "--apply", m)
            took = time.monotonic() - t
            self.assertEqual(status, 0, out[-2000:])
            self.assertIn(f"{n} names added to symbol_addrs.us.txt", out)
            status, out = run("--root", root, "--check", m)
            self.assertEqual(status, 0, out[-2000:])
            print(f"\n  5,000 rows over {files} files: --apply {took:.1f} s", file=sys.stderr)
            self.assertLess(took, float(os.environ.get("RENAME_SCALE_SECONDS", "240")))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
