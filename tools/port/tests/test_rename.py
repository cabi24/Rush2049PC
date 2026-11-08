#!/usr/bin/env python3
"""tools/port/rename.py on its fixture (tools/port/tests/names/tree and map.tsv: made-up names), as ctest's
rename_tool runs it:

    python3 tools/port/tests/test_rename.py [-v]

Each test copies the fixture tree to a temporary folder and runs rename.py there: every place a name lives (the C's
code, comments and symbol-name strings, asm and alias strings, #include lines, the symbol tables and their checksums,
symbol_addrs.us.txt and the address an old name spelled, the port's C, a tool, a suppression, extents.txt, CMake's
lists of stems and of paths, the layout, the docs), what is left and reported (another spelling, a string that only
mentions a name, a folder named after a function, token pasting, derived names, field rows), --check, a second
--apply that changes nothing, git mv in a git tree, the refusals, and a map of 5,000 rows over a tree of 400 files.
Stdlib only, Python 3.9+.
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

    def apply(self, map_path=MAP):
        status, out = run("--root", self.root, "--apply", map_path)
        self.assertEqual(status, 0, out)
        return out


class DryRun(Fixture):
    def test_reports_without_changing(self):
        before = snapshot(self.root)
        report = self.tmp / "report.txt"
        status, out = run("--root", self.root, "--dry-run", "--report", report, MAP)
        self.assertEqual(status, 0, out)
        self.assertEqual(snapshot(self.root), before, "a dry run changed the tree")
        self.assertIn("7 rows (func 3, data 1, type 1, field 1, file 1), 2 implied file moves", out)
        self.assertIn("func_80F00010 -> thing_count_read [func]", out)
        self.assertIn("moves src/blob/func_80F00010.c -> src/blob/thing_count_read.c", out)
        self.assertIn("symbol_addrs.us.txt += thing_table = 0x80F00100 type:data", out)
        self.assertIn("OldThing.0x0 -> total [field]: LEFT for wave 2", out)
        self.assertIn("other spelling: func_80f00010", out)
        self.assertIn("string: func_80F00010", out)
        self.assertIn("path: func_80F00010: src/blob/groups/func_80F00010/group.c", out)
        self.assertIn("PASTE: func_80F00010", out)
        self.assertIn("func_80F00010_1 (func_80F00010 -> thing_count_read)", out)
        lines = report.read_text()
        self.assertIn("port/main.c:6: string (name): D_80F00100 -> thing_table", lines)
        self.assertIn("src/blob/func_80F00010.c: moved to src/blob/thing_count_read.c (implied", lines)


class Apply(Fixture):
    def test_every_place(self):
        out = self.apply()
        self.assertIn("13 files changed, 3 moved, 2 names added to symbol_addrs.us.txt", out)
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
        self.assertEqual(self.text("CMakeLists.txt").splitlines()[1], "foreach(f thing_draft other_draft)")
        self.assertIn("add_executable(fixture src/blob/thing_count_read.c include/game/thing.h)",
                      self.text("CMakeLists.txt"))
        doc = self.text("docs/port/THING.md")
        self.assertIn("`thing_count_read` (src/blob/thing_count_read.c) reads thing_table/2C and Thing.count", doc)
        self.assertIn("src/blob/groups/func_80F00010/group.c. See thing_count_read.c and blob/thing_count_read.c",
                      doc)
        self.assertIn("include/game/thing.h.", doc)
        self.assertIn('{"thing_table", thing_table}', self.text("port/main.c"))
        self.assertIn('NAMES = ("thing_table", "thing_assist")', self.text("tools/port/gen_thing.py"))
        self.assertIn("bounds:^thing_count_read$", self.text("tools/port/sanitize/ubsan.supp"))
        self.assertIn("thing_table 0x10", self.text("port/data/extents.txt"))
        self.assertEqual([e["target_id"] for e in json.loads(self.text("layout/game.json"))["regions"][0]["entries"]],
                         ["thing_count_read", "thing_draft"])
        # the symbol table: keys renamed, in its own style; its checksum line follows; the assembly is left
        table = self.text("asm/us/blob/symbols.json")
        d = json.loads(table)
        self.assertEqual(d["symbols"], {"thing_assist": "0x80F00200", "thing_count_read": "0x80F00010",
                                        "thing_table": "0x80F00100", "zeta_keep": "0x80F00300"})
        self.assertEqual(table, json.dumps(d, indent=2, sort_keys=True) + "\n")
        sums = dict(reversed(line.split("  ")) for line in self.text("asm/us/blob/SHA256SUMS").splitlines())
        self.assertEqual(sums["symbols.json"], hashlib.sha256(table.encode()).hexdigest())
        self.assertEqual(self.text("asm/us/blob/region.s"), (FIXTURE / "asm/us/blob/region.s").read_text())
        # symbol_addrs.us.txt: named entries renamed in place; the address names gain their address and kind
        syms = self.text("symbol_addrs.us.txt")
        self.assertIn("thing_assist = 0x80F00200; // type:func a named function", syms)
        self.assertIn("thing_draft = 0x80F00400; // type:func", syms)
        self.assertIn("thing_table = 0x80F00100; // type:data", syms)
        self.assertIn("thing_count_read = 0x80F00010; // type:func", syms)
        self.assertNotIn("D_80F00100", syms)

    def test_again_changes_nothing(self):
        self.apply()
        after = snapshot(self.root)
        out = self.apply()
        self.assertIn("6 rows applied already", out)
        self.assertIn("0 files changed, 0 moved", out)
        self.assertEqual(snapshot(self.root), after)

    def test_check(self):
        status, out = run("--root", self.root, "--check", MAP)
        self.assertEqual(status, 1)
        self.apply()
        status, out = run("--root", self.root, "--check", MAP)
        self.assertEqual(status, 1, out)   # what is left for hand edits
        self.assertIn("src/blob/thing_count_read.c:6: func_80f00010", out)
        self.assertIn("src/blob/thing_count_read.c:10: func_80F00010", out)
        self.assertIn("docs/port/THING.md:4: func_80F00010", out)
        self.assertIn("rename.py --check: 3 places still have an old name; 1 field rows not checked", out)

    def test_check_clean(self):
        clean = self.tmp / "clean.tsv"
        clean.write_text("old\tnew\tkind\tsource\tconfidence\tnote\n"
                         "helper_old_name\tthing_assist\tfunc\tcode\tsure\ta named function\n")
        self.apply(clean)
        status, out = run("--root", self.root, "--check", clean)
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
        self.assertIn("include/game/old_header.h -> include/game/thing.h", moves)
        self.assertIn("src/blob/func_80F00010.c -> src/blob/thing_count_read.c", moves)
        self.assertIn("nonmatching/game/draft_old.c -> nonmatching/game/thing_draft.c", moves)


class Refusals(Fixture):
    def write_map(self, rows):
        p = self.tmp / "m.tsv"
        p.write_text("old\tnew\tkind\tsource\tconfidence\tnote\n" + "".join("\t".join(r) + "\n" for r in rows))
        return p

    def test_bad_map_changes_nothing(self):
        before = snapshot(self.root)
        m = self.write_map([("func_80F00010", "zeta_keep", "func", "code", "sure", "collides")])
        status, out = run("--root", self.root, "--apply", m)
        self.assertEqual(status, 1)
        self.assertIn("new zeta_keep already exists", out)
        self.assertEqual(snapshot(self.root), before)

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
