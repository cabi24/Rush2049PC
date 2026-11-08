#!/usr/bin/env python3
"""tools/port/names_check.py on its fixture (tools/port/tests/names: made-up names), as ctest's names_check_tool
runs it:

    python3 tools/port/tests/test_names_check.py [-v]

Each kind of error and warning on a map written for it, against a copy of the fixture tree: the format, duplicates
across two maps, collisions (the C, the symbol tables, keywords, reserved and library names), addresses left, old
names missing, kinds the C contradicts, file and field rows, rows applied already, the C lexer and the classifier.
Stdlib only, Python 3.9+.
"""
import contextlib
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import names_check as nc  # noqa: E402
import rename  # noqa: E402

FIXTURE = HERE / "names" / "tree"
MAP = HERE / "names" / "map.tsv"
HEADER = "old\tnew\tkind\tsource\tconfidence\tnote\n"


class Base(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="names_check_test_"))
        cls.root = cls.tmp / "tree"
        shutil.copytree(FIXTURE, cls.root)
        cls.tree = nc.Tree(cls.root)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def check(self, *rows, text=None, name="m.tsv"):
        """(errors, warnings) of a map of these rows (each a tuple, or a whole line)."""
        p = self.tmp / name
        p.write_text(text if text is not None else HEADER + "".join(
            ("\t".join(r) if isinstance(r, tuple) else r) + "\n" for r in rows))
        _, errors, warnings, _ = nc.run_checks(self.root, [p], self.tree)
        return [m for _, m in errors], [m for _, m in warnings]

    def assertOne(self, msgs, part):
        self.assertTrue(any(part in m for m in msgs), f"{part!r} not in {msgs}")


class Format(Base):
    def test_fixture_map_is_clean(self):
        errors, warnings = self.check(text=MAP.read_text())
        self.assertEqual(errors, [])
        self.assertEqual(warnings, ["old func_80F00010 is also spelled func_80f00010 in the C: another identifier, "
                                    "which rename.py leaves (gen_stubs.py links it by address)"])

    def test_format(self):
        errors, _ = self.check("func_80F00010 spaces only", ("func_80F00010", "a_b", "func"),
                               ("func_80F00010", "a_b", "fn", "mat4", "maybe", "x"),
                               ("func_80F00010", "a b", "func", "code", "sure", ""),
                               ("OldThing.5AC", "x", "field", "code", "sure", "n"),
                               ("/abs/x.c", "../y.c", "file", "code", "sure", "n"))
        for part in ("no tab in the row", "3 fields, expected 6", "kind 'fn'", "source 'mat4'", "confidence 'maybe'",
                     "new name 'a b' is no C identifier", "no note", "Struct.0xOFFSET",
                     "old path '/abs/x.c'", "new path '../y.c'"):
            self.assertOne(errors, part)

    def test_header_and_comments(self):
        errors, _ = self.check(text="# a comment\n\n" + HEADER + "# another\n"
                               "helper_old_name\tthing_assist\tfunc\tcode\tsure\tok\n")
        self.assertEqual(errors, [])

    def test_runs_of_tabs(self):
        errors, _ = self.check(text=HEADER + "helper_old_name\t\tthing_assist\tfunc\t\tcode\tsure\tok, aligned\n")
        self.assertEqual(errors, [])


class Duplicates(Base):
    def test_across_maps(self):
        a, b = self.tmp / "a.tsv", self.tmp / "b.tsv"
        a.write_text(HEADER + "helper_old_name\tthing_assist\tfunc\tcode\tsure\tn\n")
        b.write_text(HEADER + "helper_old_name\tother_name\tfunc\tcode\tsure\tn\n"
                     "draft_old\tthing_assist\tfunc\tcode\tsure\tn\n")
        _, errors, _, _ = nc.run_checks(self.root, [a, b], self.tree)
        msgs = [m for _, m in errors]
        self.assertOne(msgs, "old helper_old_name is in another row too")
        self.assertOne(msgs, "new thing_assist is used by another row too")

    def test_other_spelling_is_the_same_old_name(self):
        errors, _ = self.check(("func_80F00010", "a_name", "func", "code", "sure", "n"),
                               ("func_80f00010", "b_name", "func", "code", "sure", "n"))
        self.assertOne(errors, "old func_80f00010 is in another row too")

    def test_field_names_per_struct(self):
        errors, _ = self.check(("OldThing.0x0", "total", "field", "code", "sure", "n"),
                               ("OldThing.0x4", "total", "field", "code", "sure", "n"),
                               ("OldThing.0x0", "other", "field", "code", "sure", "n"))
        self.assertOne(errors, "new total is used by another row too")
        self.assertOne(errors, "old OldThing.0x0 is in another row too")


class Collisions(Base):
    def test_names(self):
        errors, warnings = self.check(("func_80F00010", "zeta_keep", "func", "code", "sure", "in the C"),
                                      ("D_80F00100", "while", "data", "code", "sure", "keyword"),
                                      ("helper_old_name", "__thing", "func", "code", "sure", "reserved"),
                                      ("draft_old", "_draft", "func", "code", "sure", "file scope"),
                                      ("OldThing", "_Thing", "type", "code", "sure", "reserved"),
                                      name="c1.tsv")
        for part in ("new zeta_keep already exists in the C", "new while is a C keyword",
                     "new __thing is a reserved identifier", "new _draft starts with an underscore",
                     "new _Thing is a reserved identifier"):
            self.assertOne(errors, part)

    def test_libraries(self):
        errors, warnings = self.check(("func_80F00010", "memcpy", "func", "code", "sure", "libc"),
                                      ("D_80F00100", "SDL_thing", "data", "code", "sure", "sdl"),
                                      ("helper_old_name", "sinf", "func", "code", "sure", "libm"),
                                      ("draft_old", "near", "func", "code", "sure", "windows"),
                                      name="c2.tsv")
        self.assertOne(errors, "new memcpy is a name of the C library")
        self.assertOne(errors, "new SDL_thing is a name of the C library, POSIX, zlib or SDL")
        self.assertOne(errors, "new sinf is a name of the C library")
        self.assertOne(warnings, "new near is a macro of Windows' headers")

    def test_symbol_tables(self):
        errors, _ = self.check(("func_80F00010", "zeta_keep", "func", "code", "sure", "n"), name="c3.tsv")
        self.assertOne(errors, "already exists")

    def test_renamed_away_is_free(self):
        # a swap, or a chain: rename.py replaces every token in one pass
        errors, _ = self.check(("helper_old_name", "draft_old", "func", "code", "sure", "n"),
                               ("draft_old", "helper_old_name", "func", "code", "sure", "n"), name="c4.tsv")
        self.assertEqual(errors, [])


class Addresses(Base):
    def test_left_in_new_names(self):
        errors, _ = self.check(("func_80F00010", "car_800D0A34", "func", "code", "sure", "n"),
                               ("D_80F00100", "func_thing", "data", "code", "sure", "n"),
                               ("helper_old_name", "x_D_8012", "func", "code", "sure", "n"),
                               ("include/game/old_header.h", "include/game/h_80123456.h", "file", "code", "sure",
                                "n"), name="a.tsv")
        self.assertOne(errors, "new car_800D0A34 still has an address in it")
        self.assertOne(errors, "new func_thing still has an address in it")
        self.assertOne(errors, "new x_D_8012 still has an address in it")
        self.assertOne(errors, "new file include/game/h_80123456.h still has an address in its name")


class Existence(Base):
    def test_missing(self):
        errors, _ = self.check(("func_80F0FFFF", "a_name", "func", "code", "sure", "n"),
                               ("NoSuchStruct.0x4", "b_name", "field", "code", "sure", "n"),
                               ("src/nothing.c", "src/something.c", "file", "code", "sure", "n"),
                               ("include/game/old_header.h", "src/blob/groups/func_80F00010/group.c", "file", "code",
                                "sure", "onto a file"), name="e.tsv")
        self.assertOne(errors, "old func_80F0FFFF exists nowhere")
        self.assertOne(errors, "struct NoSuchStruct of NoSuchStruct.0x4 is no type of the C")
        self.assertOne(errors, "old file src/nothing.c doesn't exist")
        self.assertOne(errors, "new file src/blob/groups/func_80F00010/group.c exists already")

    def test_only_in_tables(self):
        _, warnings = self.check(("zeta_keep", "zeta_kept", "data", "code", "sure", "n"), name="t.tsv")
        self.assertEqual([w for w in warnings if "only in the symbol tables" in w], [])   # the header declares it

    def test_field_struct_by_type_row(self):
        errors, _ = self.check(("OldThing", "Thing", "type", "code", "sure", "n"),
                               ("Thing.0x0", "total", "field", "code", "sure", "by its new name"), name="f.tsv")
        self.assertEqual(errors, [])


class Kinds(Base):
    def test_contradictions(self):
        errors, warnings = self.check(("func_80F00010", "a_value", "data", "code", "sure", "a function"),
                                      ("D_80F00100", "b_call", "func", "code", "sure", "a variable"),
                                      ("helper_old_name", "CThing", "type", "code", "sure", "a function"),
                                      ("OldThing", "c_value", "data", "code", "sure", "a type"), name="k.tsv")
        self.assertOne(errors, "old func_80F00010 is defined as a function in the C, not data")
        self.assertOne(errors, "old D_80F00100 is a variable in the C, not a function")
        self.assertOne(errors, "old helper_old_name is no typedef name or struct, union or enum tag in the C")
        self.assertOne(errors, "old OldThing is a type in the C, not data")

    def test_style(self):
        _, warnings = self.check(("func_80F00010", "CountRead", "func", "code", "sure", "n"),
                                 ("OldThing", "thing_record", "type", "code", "sure", "n"),
                                 ("D_80F00100", "unk_table", "data", "code", "sure", "n"),
                                 ("helper_old_name", "pc_assist", "func", "code", "sure", "n"),
                                 ("draft_old", "DraftOld", "func", "mat3", "sure", "a MAT3 name: any style"),
                                 name="s.tsv")
        self.assertOne(warnings, "new CountRead: source code func names are lower_snake_case")
        self.assertOne(warnings, "new thing_record: source code type names are CamelCase")
        self.assertOne(warnings, "new unk_table uses unk")
        self.assertOne(warnings, "new pc_assist has the PC layer's prefix")
        self.assertFalse(any("DraftOld" in w for w in warnings))


class Applied(Base):
    def test_rows_applied_already(self):
        tmp = Path(tempfile.mkdtemp(prefix="names_check_applied_"))
        try:
            root = tmp / "tree"
            shutil.copytree(FIXTURE, root)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(rename.main(["--root", str(root), "--apply", str(MAP)]), 0)
            _, errors, warnings, applied = nc.run_checks(root, [MAP])
            self.assertEqual(errors, [])
            self.assertEqual(len(applied), 6)   # all but the field row
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class Lexer(Base):
    def test_classify(self):
        t = self.tree
        self.assertIn("func_80F00010", t.func_def)
        self.assertIn("helper_old_name", t.func_decl)
        self.assertIn("D_80F00100", t.data_decl)
        self.assertIn("OldThing", t.types)
        self.assertIn("D_80F00104", t.idents)      # in an asm string
        self.assertNotIn("calls", t.idents)        # in a plain string
        self.assertNotIn("x27C", nc.asm_string_names('__asm__(".set a, b + 0x27C");'))
        self.assertEqual(t.table_kind.get("draft_old"), "func")
        self.assertEqual(t.spellings[nc.norm("func_80F00010")], {"func_80F00010", "func_80f00010"})

    def test_tokens(self):
        kinds = [(k, v) for k, _, _, v in nc.c_tokens('a /* b */ "c d" // e\n#include "f.h"\n\'g\' 0x1F h')]
        self.assertEqual(kinds, [("id", "a"), ("bc", "/* b */"), ("str", '"c d"'), ("lc", "// e"),
                                 ("inc", '#include "f.h"'), ("chr", "'g'"), ("num", "0x1F"), ("id", "h")])

    def test_norm(self):
        self.assertEqual(nc.norm("func_8001fe58"), "func_8001FE58")
        self.assertEqual(nc.norm("CarSetup"), "CarSetup")


class Cli(Base):
    def test_exit_status(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(nc.main(["--root", str(self.root), str(MAP)]), 0)
            self.assertEqual(nc.main(["--root", str(self.root), "--strict", str(MAP)]), 1)
            self.assertEqual(nc.main(["--root", str(self.tmp / "nowhere")]), 0)   # no maps: nothing to check
        self.assertIn("names_check: 1 map, 7 rows (func 3, data 1, type 1, field 1, file 1): 0 errors, 1 warnings",
                      out.getvalue())
        self.assertIn("names_check: no maps in", out.getvalue())


if __name__ == "__main__":
    unittest.main()
