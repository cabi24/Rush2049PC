#!/usr/bin/env python3
"""n64_guard.py's reading of a guarded run's log (port/ultra/memory.c's lines, misc.c's fault handler): a clean run
passes; a run without the guard, without the heap's line, or with a guard's note of an N64 address or of a heap
pointer cut to 32 bits fails, with that line; --check-logs reads a folder's guarded logs (NAME.1.0.log) only.

    python3 tools/port/tests/test_n64_guard.py

Exit status 0 when every check passes. Stdlib only; no game run (ctest n64_guard_paths does that).
"""
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOL = HERE.parent / "n64_guard.py"
sys.path.insert(0, str(HERE.parent))
import n64_guard  # noqa: E402

ON = ("rush2049pc: N64 guard (RUSH2049_N64_GUARD): 0x80000000-0xBFFFFFFF reserved; an N64 address used as a host "
      "pointer faults there (docs/port/NATIVE.md 17)\n"
      "rush2049pc: N64 guard: the game heap's block at 0x5540000000a640; a pointer into it cut to 32 bits faults\n")
RUN = "rush2049pc: game state 0x00000040 (was 0x00000001) at vi 120, after 60 graphics tasks: players 1\n"
NOTE = ("rush2049pc: N64 guard: 0x8003803C, an N64 address (KSEG0: RDRAM), the N64's gWaveSizes+0x10, used as a host "
        "pointer: nothing is there on the PC (RUSH2049_N64_GUARD, docs/port/NATIVE.md 17)\n")
CUT = ("rush2049pc: N64 guard: 0x4001A6C0, a pointer into the game heap cut to 32 bits (heap+0x10080) "
       "(RUSH2049_N64_GUARD, docs/port/NATIVE.md 17)\n")

failures = 0


def check(cond, what):
    global failures
    if not cond:
        failures += 1
        print(f"test_n64_guard: FAIL: {what}")


check(n64_guard.check_log(ON + RUN) == [], "a clean guarded run passes")
check(len(n64_guard.check_log(RUN)) == 2, "a run without the guard: no guard line, no heap line")
bad = n64_guard.check_log(ON + RUN + NOTE)
check(len(bad) == 1 and "gWaveSizes" in bad[0], "an N64 address's note fails the run, with its line")
bad = n64_guard.check_log(ON + CUT)
check(len(bad) == 1 and "cut to 32 bits" in bad[0], "a cut heap pointer's note fails the run")
check(n64_guard.check_log(ON.replace("0x5540000000a640", "0x7f12a640")) == [], "the heap's line, any address")

with tempfile.TemporaryDirectory() as d:
    Path(d, "drive.0.0.log").write_text(RUN)            # the plain run: not checked
    Path(d, "drive.1.0.log").write_text(ON + RUN)
    Path(d, "race2p.1.0.log").write_text(ON + RUN + NOTE)
    res = n64_guard.check_logs(d)
    check(sorted(res) == ["drive", "race2p"] and res["drive"] == [] and len(res["race2p"]) == 1,
          "check_logs reads the guarded logs only")
    p = subprocess.run([sys.executable, str(TOOL), "--check-logs", d], stdout=subprocess.PIPE, text=True)
    check(p.returncode == 1 and "1 of 2 guarded runs clean" in p.stdout, "--check-logs fails a folder with a note")
    Path(d, "race2p.1.0.log").write_text(ON + RUN)
    p = subprocess.run([sys.executable, str(TOOL), "--check-logs", d], stdout=subprocess.PIPE, text=True)
    check(p.returncode == 0 and "2 of 2 guarded runs clean" in p.stdout, "--check-logs passes clean logs")

print(f"test_n64_guard: {'FAILED' if failures else 'ok'}")
sys.exit(1 if failures else 0)
