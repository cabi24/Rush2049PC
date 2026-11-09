#!/usr/bin/env python3
"""ctest synth_compare_expect: synth_compare.py's verdicts on made-up runs (no program, no cartridge): two builds that
agree pass, a run that doesn't end as the script's `# expect:` line says MISSES, whatever the traces' agreement
(docs/port/QA.md, "Integration QA": battle2p.press on the retail cartridge, a practice race run the same in every
run). Stdlib only."""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_compare as sc  # noqa: E402

LOG = """rush2049pc: game state 0x00000080 (was 0x00000040) at vi 1022, after 991 graphics tasks: players 2, windows 1, cars 0, mode {mode}, track 0, pause 0; car 0 at 0 0 0, speed 0, steer 0.00
rush2049pc: game state 0x00400000 (was 0x00200000) at vi 1615, after 1584 graphics tasks: players 2, windows 2, cars 2, mode {mode}, track {track}, pause 0; car 0 at 1 2 3, speed 0, steer 0.00
rush2049pc: state reached: t=36.0s vi=2157 gfx_tasks=2129 audio_tasks=2154 state=0x00400000: players 2, windows 2, cars 2, mode {mode}, track {track}, pause 0; car 0 at 1 2 3, speed 0, steer 0.00
"""
EXPECT = "state=0x00400000: players 2, windows 2, cars 2, mode 6, track 6"


class Done:
    def __init__(self, value):
        self.value = value

    def result(self):
        return self.value


def runs(mode, track, n=2):
    t = sc.trace(LOG.format(mode=mode, track=track))
    return [(k, Done((t, 0, 0.1))) for k in (0, 1) for _ in range(n)]


fails = 0


def check(ok, what):
    global fails
    print(f"synth_compare_expect: {'ok  ' if ok else 'FAIL'} {what}")
    fails += not ok


check(sc.judge("battle", runs(6, 6), EXPECT) == 0, "a battle in every run, the expect met: passes")
check(sc.judge("battle", runs(1, 0), EXPECT) == 1, "a practice race in every run (the same trace): MISSES")
check(sc.judge("battle", runs(1, 0), "") == 0, "no expect line: the agreement alone")
check(sc.reaches(sc.trace(LOG.format(mode=6, track=6)), "players 2"), "an expect of part of the line")
check(not sc.reaches(sc.trace(LOG.format(mode=6, track=6).splitlines()[0]), EXPECT), "no end line: missed")
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / "x.press"
    p.write_text("# A script\n# until: 0x400000:600\n# expect: " + EXPECT + "\n5 A\n")
    check(sc.script_expect(p) == EXPECT, "a script's expect line read")
    p.write_text("# until: 0x400000:600\n5 A\n")
    check(sc.script_expect(p) == "", "a script without one")
for name in ("battle2p", "battlequit"):   # the retail cartridge's battle scripts say what they reach
    check("mode 6" in sc.script_expect(Path(__file__).resolve().parent / "retail" / f"{name}.press"),
          f"tools/port/retail/{name}.press expects a battle (mode 6)")
GLIBC = ("rush2049pc: state reached: t=36.0s vi=2157 gfx_tasks=2129 audio_tasks=2154 state=0x00400000: players 1, "
         "windows 1, cars 6, mode 1, track 0, pause 0; car 0 at 0x0p+0 -0x1.8p+3 0x1.0000000000001p+0, speed -nan, "
         "steer 0x0.0000000000001p-1022")
UCRT = GLIBC.replace("0x0p+0", "0x0.0000000000000p+0").replace("0x1.8p+3", "0x1.8000000000000p+3") \
    .replace("-nan", "-nan(ind)")
check(sc.trace(GLIBC) == sc.trace(UCRT), "glibc's and the UCRT's spellings of one exact float: the same trace")
check(sc.trace(GLIBC) != sc.trace(GLIBC.replace("-0x1.8p+3", "-0x1.8000000000001p+3")),
      "exact floats a bit apart: different traces")
check(sc.trace(GLIBC) != sc.trace(GLIBC.replace("-0x1.8p+3", "0x1.8p+3")), "the sign kept")
check(sc.trace(GLIBC)[0][2] == "0x00400000", "the state word (hex without p) left as it is")
# The loader's summary: its symbol count left out; with --data-moved its pointer counts by class too, the rest kept.
DATA = ("pc_data: 418 symbols, pointers: 0 own, 0 C data, 306 arena (288 to undescribed data, 0 to text in words), "
        "16 functions, 0 heap (kept), 3198 null, 1 raw, 0 unresolved, 0 to functions with no C (0 inexact, 0 ambiguous); "
        "0 address-like words (0 to host storage or code); 5 tables only pointers reach (8 elements)")
MOVED = DATA.replace("418 symbols", "419 symbols").replace("0 own", "13 own").replace("306 arena", "297 arena") \
    .replace("3198 null", "3218 null")
check(sc.trace(DATA) == sc.trace(DATA.replace("418 symbols", "419 symbols")), "the symbol count: not compared")
check(sc.trace(DATA) != sc.trace(MOVED), "pointers moved between classes: different traces")
sc.DATA_MOVED = True
check(sc.trace(DATA) == sc.trace(MOVED), "--data-moved: pointers moved between classes, the same trace")
check(sc.trace(DATA) != sc.trace(DATA.replace("0 unresolved", "2 unresolved")), "--data-moved: unresolved kept")
check(sc.trace(DATA) != sc.trace(DATA.replace("288 to undescribed", "287 to undescribed")),
      "--data-moved: undescribed targets kept")
check(sc.trace(DATA) != sc.trace(DATA.replace("0 address-like", "4 address-like")), "--data-moved: address words kept")
sc.DATA_MOVED = False
print(f"synth_compare_expect: {'ok' if not fails else f'{fails} failed'}")
sys.exit(1 if fails else 0)
