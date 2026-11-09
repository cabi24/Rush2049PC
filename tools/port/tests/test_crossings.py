#!/usr/bin/env python3
"""crossings.py's report on made-up dumps (port/ultra/crossings.c's format): every kind in its place, sites by name
with the paths that reach them and the data they point at, a guarded run's crossings not counted twice, the guard's
instructions, the PC layer's sites named through addr2line (a stand-in here) relative to the tree, and each check
failing when it should and only then.

    python3 tools/port/tests/test_crossings.py

Exit status 0 when every check passes. Stdlib only; no game run (the counting build's ctest crossings_paths does that).
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE.parent / "crossings.py"
sys.path.insert(0, str(HERE.parent))
import crossings  # noqa: E402

MENU = """\
# rush2049pc crossings: site KIND WHERE COUNT REGION=COUNT@FIRST[:DATUM]...
exe {exe}
site PC_N64P src/blob/forces1.c:71 10 game_arena=10@0x801116d0:gTune+0x0
site PC_N64P src/blob/FindChunk.c:60 5 heap=5@0x8017a840
site pc_n64_ptr rush2049pc+0x1234 7 heap=6@0x8017a840 phys=1@0x12340
site gfx.rom_list rush2049pc+0x2000 3 phys=3@0x1000
site gfx.native_list libc.so.6+0x99 1 game_arena=1@0x80120000:cull_box_vertices_a+0x10
"""
RACE = """\
exe {exe}
site PC_N64P src/blob/forces1.c:71 4 game_arena=4@0x801116d4:gTune+0x4
site PC_N64W src/blob/heap.c:12 2 heap=2@0x8017a000
"""
MENU_GUARD = """\
exe {exe}
site PC_N64P src/blob/forces1.c:71 10 game_arena=10@0x801116d0:gTune+0x0
guard arenas,count faults=14 neighbour=3 closes=5 lost=0
fault rush2049pc+0x3000 9 game_arena=9@0x8011ed00:start_overlay_loaded+0x0
fault rush2049pc+0x3100 3 boot_arena=3@0x80030000:gFoo+0x4
fault libc.so.6+0x1584c3,from=rush2049pc+0x4000 2 game_arena=2@0x80120000:gMemcpied+0x0
"""

failures = 0


def check(cond, what):
    global failures
    print(f"{'ok' if cond else 'FAIL'}: {what}")
    failures += not cond


def tool(*args, env=None):
    p = subprocess.run([sys.executable, str(TOOL)] + [str(a) for a in args], capture_output=True, text=True, env=env)
    return p.returncode, p.stdout


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        # a build folder whose cache names the tree, an "executable" there, and an addr2line that names each offset
        build = tmp / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text("CMAKE_HOME_DIRECTORY:INTERNAL=/src/tree\n")
        exe = build / "rush2049pc"
        exe.write_bytes(b"\x7fELF")
        dumps = tmp / "dumps"
        dumps.mkdir()
        (dumps / "menu.crossings").write_text(MENU.format(exe=exe))
        (dumps / "race.crossings").write_text(RACE.format(exe=exe))
        (dumps / "menu.guard.crossings").write_text(MENU_GUARD.format(exe=exe))
        bin_ = tmp / "bin"
        bin_.mkdir()
        a2l = bin_ / "addr2line"
        a2l.write_text("#!/bin/sh\n# made up: -f -e EXE then offsets\nshift 3\nfor a in \"$@\"; do\n"
                       "  echo \"fn_$a\"\n  echo \"/src/tree/port/ultra/x.c:${#a} (discriminator 2)\"\ndone\n")
        a2l.chmod(0o755)
        env = dict(os.environ, PATH=f"{bin_}{os.pathsep}{os.environ.get('PATH', '')}")
        # The stand-in addr2line is a shell script, which Windows doesn't run; the counting build it names the sites
        # of is Linux only (RUSH_COUNT_CROSSINGS), so on Windows the checks of the names it gives are left out.
        named = os.name != "nt"

        code, out = tool("--report", dumps, env=env)
        print(out)
        lines = out.splitlines()
        check(code == 0, "a report with no checks passes")
        check("crossings: 2 runs, 1 guarded" in out, "plain and guarded runs told apart")
        kinds = [l.split()[0] for l in lines if l.split() and l.split()[0] in crossings.KINDS
                 and len(l.split()) >= 3 and l.split()[1].isdigit()]
        check(kinds == crossings.KINDS, "every kind has its line, in crossings.c's order")
        n64p = next(l for l in lines if l.startswith("PC_N64P "))
        check(n64p.split()[1:3] == ["2", "19"], "PC_N64P: 2 sites, 19 crossings (the guarded run's not counted again)")
        check("game_arena 14, heap 5" in n64p, "the kind's regions, largest first")
        check(next(l for l in lines if l.startswith("PC_SLOT_N64 ")).split()[1:3] == ["0", "0"], "a kind with none: 0")
        i = lines.index(next(l for l in lines if l.startswith("PC_N64P: 2 sites")))
        check("src/blob/FindChunk.c:60" in lines[i + 1] and "src/blob/forces1.c:71" in lines[i + 2],
              "sites sorted by name")
        check("paths  2" in lines[i + 2] and "game_arena 14 (gTune)" in lines[i + 2],
              "a site's paths and the datum it points at, the offset dropped")
        if named:
            check("fn_0x1234 port/ultra/x.c:6" in out, "a PC layer site named by addr2line, relative to its tree")
        check("libc.so.6+0x99" in out, "a library's site as it is")
        check("14 faults at 3 instructions in 1 runs; 3 more" in out, "the guard's totals")
        if named:
            check("fn_0x3000 port/ultra/x.c:6" in out and "(start_overlay_loaded)" in out, "the guard's instructions")
            check("libc.so.6+0x1584c3 from fn_0x4000 port/ultra/x.c:6" in out and "(gMemcpied)" in out,
                  "a library's instruction named with the program's call of it")

        code, out2 = tool("--report", dumps, "--sort", "count", env=env)
        i = out2.splitlines().index(next(l for l in out2.splitlines() if l.startswith("PC_N64P: 2 sites")))
        check("forces1.c:71" in out2.splitlines()[i + 1], "--sort count: the largest first")
        code, out3 = tool("--report", dumps, env=env)
        check(out3 == out, "the same dumps give the same report")

        for args, fails, what in (
                (["--zero-kind", "PC_N64W"], True, "--zero-kind fails for a kind with crossings"),
                (["--zero-kind", "PC_SLOT_N64,pc_statll_link"], False, "--zero-kind passes for kinds with none"),
                (["--zero-region", "game_arena"], True, "--zero-region fails for a region crossed into"),
                (["--zero-region", "boot_arena"], False, "--zero-region passes for one no crossing reaches"),
                (["--zero-site", "FindChunk"], True, "--zero-site fails for a live site"),
                *([(["--zero-site", "fn_0x1234"], True, "--zero-site matches a PC layer site's name")] if named else []),
                (["--zero-site", "nowhere.c"], False, "--zero-site passes for no such site"),
                (["--no-faults"], True, "--no-faults fails when the guard found something"),
                (["--expect-faults"], False, "--expect-faults passes then")):
            code, out = tool("--report", dumps, *args, env=env)
            check((code != 0) == fails and ("crossings: FAIL" in out) == fails, what)
        code, out = tool("--report", dumps / "race.crossings", "--expect-faults", env=env)
        check(code != 0, "--expect-faults fails with no guarded run")

        base = tmp / "base.json"
        code, _ = tool("--report", dumps, "--write-baseline", base, "--json", tmp / "r.json", env=env)
        b = json.loads(base.read_text())["kinds"]
        check(b["PC_N64P"] == {"sites": 2, "crossings": 19, "regions": {"game_arena": 14, "heap": 5}},
              "--write-baseline: each kind's sites, crossings and regions")
        check(json.loads((tmp / "r.json").read_text())["guard"]["instructions"] == 3, "--json: the guard's numbers")
        code, _ = tool("--report", dumps, "--baseline", base, env=env)
        check(code == 0, "--baseline passes against itself")
        few = dumps / "few.crossings"
        few.write_text("site PC_N64P src/blob/forces1.c:71 3 game_arena=3@0x801116d0:gTune+0x0\n")
        code, _ = tool("--report", dumps, "--baseline", base, env=env)
        check(code == 0, "--baseline passes a few more crossings at the same sites (the frames around the exit)")
        few.write_text("site PC_N64P src/blob/forces1.c:71 1000 game_arena=1000@0x801116d0:gTune+0x0\n")
        code, out = tool("--report", dumps, "--baseline", base, env=env)
        check(code != 0 and "--baseline: PC_N64P" in out, "--baseline fails when a kind's crossings grow")
        few.unlink()
        (dumps / "more.crossings").write_text("site PC_N64W src/blob/heap.c:13 1 heap=1@0x8017a000\n")
        code, out = tool("--report", dumps, "--baseline", base, env=env)
        check(code != 0 and "--baseline: PC_N64W" in out, "--baseline fails when a kind grows")
    print(f"test_crossings: {'all passed' if not failures else f'{failures} failed'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
