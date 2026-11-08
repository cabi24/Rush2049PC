#!/usr/bin/env python3
"""tools/port/names_check.py on its fixture (tools/port/tests/names: made-up names), as ctest's names_check_tool
runs it:

    python3 tools/port/tests/test_names_check.py [-v]

Each kind of error and warning on a map written for it, against a copy of the fixture tree (with a few files more:
a static function, an identity #define, a header no .c file includes, a struct member of a map's new name, a tool
that looks a label up): the format, duplicates across two maps and aliases made one, collisions (the C, where and
how; the symbol tables; keywords, reserved and library names, MAT3's reserved names), addresses left, old names
missing, kinds the C contradicts, file, folder and field rows, the drop list, rows applied already, the C lexer and
the classifier. Stdlib only, Python 3.9+.
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
DROP = HERE / "names" / "drop.tsv"
HEADER = "old\tnew\tkind\tsource\tconfidence\tnote\n"
EXTRA = {
    "src/blob/statics.c": "static s32 copy_of_reset(void) { return 1; }\ns32 user(void) { return copy_of_reset(); }\n",
    "src/blob/defines.c": "#define zlib_name helper_old_name\ns32 zl(void) { return zlib_name(1); }\n",
    "include/game/arcade.h": "void StartArcade(void);\n",
    "port/member.c": "struct S { int memberName; };\nint get(struct S s) { return s.memberName; }\n",
    "tools/port/uses.py": "LOOKUP = ('zeta_listed',)  # a tool that looks a label up\n",
    "src/blob/uses_label.c": "extern s32 zeta_used;\ns32 read_label(void) { return zeta_used; }\n",
}


class Base(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp(prefix="names_check_test_"))
        cls.root = cls.tmp / "tree"
        shutil.copytree(FIXTURE, cls.root)
        for rel, text in EXTRA.items():
            (cls.root / rel).parent.mkdir(parents=True, exist_ok=True)
            (cls.root / rel).write_text(text)
        (cls.root / "asm/us/ovl_a").mkdir(parents=True)
        (cls.root / "asm/us/ovl_a/symbols.json").write_text('{\n  "symbols": {\n    "ovl_label": "0x8038A400"\n  }\n}\n')
        table = cls.root / "asm/us/blob/symbols.json"
        table.write_text(table.read_text().replace('"zeta_keep"', '"zeta_listed": "0x80F00900",\n    "zeta_used": "0x80F00A00",\n    "zeta_keep"'))
        cls.tree = nc.Tree(cls.root)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def check(self, *rows, text=None, name="m.tsv", drops=None):
        """(errors, warnings) of a map of these rows (each a tuple, or a whole line), and of a drop list."""
        p = self.tmp / name
        p.write_text(text if text is not None else HEADER + "".join(
            ("\t".join(r) if isinstance(r, tuple) else r) + "\n" for r in rows))
        paths = [p]
        if drops is not None:
            d = self.tmp / "drop.tsv"
            d.write_text("name\ttable\tnote\n" + "".join("\t".join(r) + "\n" for r in drops))
            paths.append(d)
        _, errors, warnings, _, _, _ = nc.run_checks(self.root, paths)
        return [m for _, m in errors], [m for _, m in warnings]

    def assertOne(self, msgs, part):
        self.assertTrue(any(part in m for m in msgs), f"{part!r} not in {msgs}")


class Format(Base):
    def test_fixture_maps_are_clean(self):
        _, errors, warnings, _, drops, _ = nc.run_checks(self.root, [MAP, DROP])
        self.assertEqual(errors, [])
        self.assertEqual(len(drops), 1)
        self.assertEqual(sorted(m for _, m in warnings), [
            "new game_state: 2 rows merge the aliases D_80F00600, state_word_a (0x80F00600) into one symbol "
            f"({MAP}:12)",
            "old func_80F00010 is also spelled func_80f00010 in the C: another identifier, which rename.py leaves "
            "(gen_stubs.py links it by address)"])

    def test_format(self):
        errors, _ = self.check("func_80F00010 spaces only", ("func_80F00010", "a_b", "func"),
                               ("func_80F00010", "a_b", "fn", "mat4", "maybe", "x"),
                               ("func_80F00010", "a b", "func", "code", "sure", ""),
                               ("OldThing.5AC", "x", "field", "code", "sure", "n"),
                               ("OldThing.0x8", "a.b c", "field", "code", "sure", "n"),
                               ("/abs/x.c", "../y.c", "file", "code", "sure", "n"))
        for part in ("no tab in the row", "3 fields, expected 6", "kind 'fn'", "source 'mat4'", "confidence 'maybe'",
                     "new name 'a b' is no C identifier", "no note", "Struct.0xOFFSET", "'a.b c' is no member path",
                     "old path '/abs/x.c'", "new path '../y.c'"):
            self.assertOne(errors, part)

    def test_member_paths(self):
        errors, _ = self.check(("OldThing.0x0", "visuals[14].func", "field", "mat3", "sure", "an array member"),
                               ("OldThing.0x4", "uvs[3][1]", "field", "mat3", "sure", "two indices"))
        self.assertEqual(errors, [])

    def test_header_and_comments(self):
        errors, _ = self.check(text="# a comment\n# the namer's notes: old\tnew\tfunc\n\n" + HEADER + "# another\n"
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
        _, errors, _, _, _, _ = nc.run_checks(self.root, [a, b])
        msgs = [m for _, m in errors]
        self.assertOne(msgs, "old helper_old_name is in another row too")
        self.assertOne(msgs, "new thing_assist is used by another row too")
        self.assertOne(msgs, "and the old names are no aliases at one address")

    def test_aliases_made_one(self):
        errors, warnings = self.check(("D_80F00600", "game_state", "data", "code", "sure", "n"),
                                      ("state_word_a", "game_state", "data", "code", "sure", "its alias"))
        self.assertEqual(errors, [])
        self.assertOne(warnings, "2 rows merge the aliases D_80F00600, state_word_a (0x80F00600) into one symbol")

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
                                      ("OldThing", "_Thing", "type", "code", "sure", "reserved"))
        for part in ("new zeta_keep already exists: used in include/game/old_header.h", "new while is a C keyword",
                     "new __thing is a reserved identifier", "new _draft starts with an underscore",
                     "new _Thing is a reserved identifier"):
            self.assertOne(errors, part)

    def test_mat3_reserved_names_warn(self):
        errors, warnings = self.check(("helper_old_name", "_PS_FlushFile", "func", "mat3", "sure", "MAT3 verbatim"),
                                      ("draft_old", "_tr_init", "func", "mat3", "sure", "zlib verbatim"))
        self.assertEqual(errors, [])
        self.assertOne(warnings, "new _PS_FlushFile is a reserved identifier")
        self.assertOne(warnings, "new _tr_init starts with an underscore")

    def test_libraries(self):
        errors, warnings = self.check(("func_80F00010", "memcpy", "func", "mat3", "sure", "libc"),
                                      ("D_80F00100", "SDL_thing", "data", "code", "sure", "sdl"),
                                      ("helper_old_name", "sinf", "func", "code", "sure", "libm"),
                                      ("draft_old", "near", "func", "code", "sure", "windows"))
        self.assertOne(errors, "new memcpy is a name of the C library")   # an error for MAT3 names too
        self.assertOne(errors, "new SDL_thing is a name of the C library, POSIX, zlib or SDL")
        self.assertOne(errors, "new sinf is a name of the C library")
        self.assertOne(warnings, "new near is a macro of Windows' headers")

    def test_where_and_how(self):
        errors, warnings = self.check(("func_80F00010", "copy_of_reset", "func", "mat3", "sure", "a static copy"),
                                      ("helper_old_name", "zlib_name", "func", "mat3", "sure", "#defined to it"),
                                      ("draft_old", "StartArcade", "func", "mat3", "sure", "an arcade header"),
                                      ("D_80F00100", "memberName", "data", "code", "sure", "a member elsewhere"))
        self.assertOne(errors, "new copy_of_reset already exists: a static function in src/blob/statics.c")
        self.assertOne(warnings, "new zlib_name is in the C only as #define zlib_name helper_old_name in "
                                 "src/blob/defines.c")
        self.assertOne(warnings, "new StartArcade is in the C only as include/game/arcade.h, which no .c file "
                                 "includes")
        self.assertOne(warnings, "new memberName is in the C only as a struct member in port/member.c")
        self.assertEqual(len(errors), 1, errors)

    def test_type_named_after_another_files_type(self):
        errors, warnings = self.check(("OldThing", "Win", "type", "mat3", "sure", "the record shadow.c calls Win"))
        self.assertEqual(errors, [])
        self.assertOne(warnings, "new Win is a type the C has already (src/blob/shadow.c)")

    def test_alias_in_the_tables(self):
        errors, warnings = self.check(("D_80F00600", "state_word_a", "data", "code", "sure", "its existing alias"))
        self.assertEqual(errors, [])
        self.assertOne(warnings, "new state_word_a is already a name of D_80F00600's address 0x80F00600")

    def test_renamed_away_is_free(self):
        # a swap, or a chain: rename.py replaces every token in one pass
        errors, _ = self.check(("helper_old_name", "draft_old", "func", "code", "sure", "n"),
                               ("draft_old", "helper_old_name", "func", "code", "sure", "n"))
        self.assertEqual(errors, [])


class Addresses(Base):
    def test_left_in_new_names(self):
        errors, _ = self.check(("func_80F00010", "car_800D0A34", "func", "code", "sure", "n"),
                               ("D_80F00100", "func_thing", "data", "code", "sure", "n"),
                               ("helper_old_name", "x_D_8012", "func", "code", "sure", "n"),
                               ("include/game/old_header.h", "include/game/h_80123456.h", "file", "code", "sure",
                                "n"))
        self.assertOne(errors, "new car_800D0A34 still has an address in it")
        self.assertOne(errors, "new func_thing still has an address in it")
        self.assertOne(errors, "new x_D_8012 still has an address in it")
        self.assertOne(errors, "new include/game/h_80123456.h still has an address in its name")


class Existence(Base):
    def test_missing(self):
        errors, warnings = self.check(("func_80F0FFFF", "a_name", "func", "code", "sure", "n"),
                                      ("NoSuchStruct.0x4", "b_name", "field", "code", "sure", "n"),
                                      ("src/nothing.c", "src/something.c", "file", "code", "sure", "n"),
                                      ("include/game/old_header.h", "src/blob/groups/func_80F00010/group.c", "file",
                                       "code", "sure", "onto a file"))
        self.assertOne(errors, "old func_80F0FFFF exists nowhere")
        self.assertOne(warnings, "struct NoSuchStruct of NoSuchStruct.0x4 is no type of the C")
        self.assertOne(errors, "old file or folder src/nothing.c doesn't exist")
        self.assertOne(errors, "new file or folder src/blob/groups/func_80F00010/group.c exists already")

    def test_folders(self):
        errors, _ = self.check(("src/blob/groups/old_group", "src/blob/groups/new_group", "file", "code", "sure",
                                "a folder"))
        self.assertEqual(errors, [])

    def test_field_struct_by_type_row(self):
        errors, warnings = self.check(("OldThing", "Thing", "type", "code", "sure", "n"),
                                      ("Thing.0x0", "total", "field", "code", "sure", "by its new name"))
        self.assertEqual(errors, [])
        self.assertEqual([w for w in warnings if "no type" in w], [])


class Kinds(Base):
    def test_contradictions(self):
        errors, warnings = self.check(("func_80F00010", "a_value", "data", "code", "sure", "a function"),
                                      ("D_80F00100", "b_call", "func", "code", "sure", "a variable"),
                                      ("helper_old_name", "CThing", "type", "code", "sure", "a function"),
                                      ("OldThing", "c_value", "data", "code", "sure", "a type"))
        self.assertOne(errors, "old func_80F00010 is defined as a function in the C, not data")
        self.assertOne(errors, "old D_80F00100 is a variable in the C, not a function")
        self.assertOne(errors, "old helper_old_name is no typedef name or struct, union or enum tag in the C")
        self.assertOne(errors, "old OldThing is a type in the C, not data")

    def test_style(self):
        _, warnings = self.check(("func_80F00010", "CountRead", "func", "code", "sure", "n"),
                                 ("OldThing", "thing_record", "type", "code", "sure", "n"),
                                 ("D_80F00100", "unk_table", "data", "code", "sure", "n"),
                                 ("helper_old_name", "pc_assist", "func", "code", "sure", "n"),
                                 ("draft_old", "DraftOld", "func", "mat3", "sure", "a MAT3 name: any style"))
        self.assertOne(warnings, "new CountRead: source code func names are lower_snake_case")
        self.assertOne(warnings, "new thing_record: source code type names are CamelCase")
        self.assertOne(warnings, "new unk_table uses unk")
        self.assertOne(warnings, "new pc_assist has the PC layer's prefix")
        self.assertFalse(any("DraftOld" in w for w in warnings))


class Drops(Base):
    def test_drop_list(self):
        errors, warnings = self.check(drops=[("zeta_unused", "blob", "nothing uses it"),
                                      ("zeta_keep", "ovl_b", "a table the tree no longer has"),
                                      ("zeta_keep", "blob", "a header declares it"),
                                      ("zeta_used", "blob", "a function reads it"),
                                      ("zeta_listed", "*", "a tool looks it up"),
                                      ("D_80F00100", "*", "a map renames it"),
                                      ("zeta_unused", "ovl_a", "not in that table"),
                                      ("never_there", "blob", "in no table: dropped already"),
                                      ("zeta_unused", "ovl_c", "no such table")],
                               text=MAP.read_text())
        self.assertOne(errors, "zeta_used is still used in the C: src/blob/uses_label.c")
        self.assertOne(warnings, "1 labels to drop are still declared (not used) in the C, e.g. zeta_keep")
        self.assertOne(warnings, "1 rows drop labels from tables the tree no longer has (ovl_b): nothing to do")
        self.assertOne(errors, "zeta_listed is still used by tools/port/uses.py")
        self.assertOne(errors, "D_80F00100 is also renamed by a map")
        self.assertOne(errors, "zeta_unused is not in ovl_a's table (it is in blob)")
        self.assertOne(errors, "table 'ovl_c' is none of blob, boot_tail, ovl_a, ovl_b, *")
        self.assertFalse(any("never_there" in e or "zeta_unused is still" in e for e in errors), errors)
        self.assertEqual(len(errors), 6, errors)   # D_80F00100: renamed by a map, and used


class Applied(Base):
    def test_rows_applied_already(self):
        tmp = Path(tempfile.mkdtemp(prefix="names_check_applied_"))
        try:
            root = tmp / "tree"
            shutil.copytree(FIXTURE, root)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(rename.main(["--root", str(root), "--apply", str(MAP), str(DROP)]), 0)
            _, errors, warnings, applied, drops, dropped = nc.run_checks(root, [MAP, DROP])
            self.assertEqual(errors, [])
            self.assertEqual(len(applied), 10)   # all but the field row: targetInfo stays only as members, locals
            self.assertEqual(len(dropped), 1)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class Lexer(Base):
    def test_classify(self):
        t = self.tree
        self.assertIn("func_80F00010", t.func_def)
        self.assertIn("helper_old_name", t.func_decl)
        self.assertIn("D_80F00100", t.data_decl)
        self.assertIn("OldThing", t.types)
        self.assertIn("copy_of_reset", t.func_static)
        self.assertIn("D_80F00104", t.idents)      # in an asm string
        self.assertNotIn("calls", t.idents)        # in a plain string
        self.assertNotIn("memberName", t.nonmember)
        self.assertNotIn("arcade.h", t.included)
        self.assertIn("old_header.h", t.included)
        self.assertNotIn("x27C", nc.asm_string_names('__asm__(".set a, b + 0x27C");'))
        self.assertEqual(t.table_kind.get("draft_old"), "func")
        self.assertEqual(t.spellings[nc.norm("func_80F00010")], {"func_80F00010", "func_80f00010"})
        self.assertTrue(t.aliases("D_80F00600", "state_word_a"))
        self.assertFalse(t.aliases("D_80F00600", "zeta_keep"))

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
            self.assertEqual(nc.main(["--root", str(self.root), str(MAP), str(DROP)]), 0)
            self.assertEqual(nc.main(["--root", str(self.root), "--strict", str(MAP)]), 1)
            self.assertEqual(nc.main(["--root", str(self.tmp / "nowhere")]), 0)   # no maps: nothing to check
        self.assertIn("names_check: 1 map, 11 rows (func 3, data 4, type 1, field 1, file 2), 1 labels to drop: "
                      "0 errors, 2 warnings", out.getvalue())
        self.assertIn("names_check: no maps in", out.getvalue())


if __name__ == "__main__":
    unittest.main()
