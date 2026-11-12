#!/usr/bin/env python3
"""The N64 guard's check: nothing the game runs reaches an N64 address (docs/port/NATIVE.md 17).

    python3 tools/port/n64_guard.py [--exe build/pc/rush2049pc] [--rom ROM] [-j 8] [--logs DIR] [NAME ...]
    python3 tools/port/n64_guard.py --exe EXE --rom ROM --script PRESS --until STATE[:TASKS] [--seconds N]
    python3 tools/port/n64_guard.py --check-logs DIR

Nothing of the game is at an N64 address since step 12: its data are host storage, its heap a block of PC memory,
its display lists hold host pointers. RUSH2049_N64_GUARD=1 makes sure: the program reserves the N64's CPU address
range, 0x80000000-0xBFFFFFFF, with no access, so a 32-bit N64 address that anything still takes for a host pointer
faults (the fault handler's note says which address it was, and which datum the N64 had there), and it maps the game
heap's block where a pointer into it cut to 32 bits faults too (port/ultra/memory.c).

This runs each synthetic path (the default: all of them but the ones on the host's clock), or a press script on a
cartridge of your own (--script, as synth_compare.py takes one: the lead's retail check), twice with the same
program, the second time with the guard: tools/port/synth_compare.py --env-b RUSH2049_N64_GUARD=1 --same --runs 1.
The check passes when every guarded run's trace is its plain run's (a fault would end it early, with a fault line),
every guarded log says the guard was on and where the heap's block went, and no guarded log has a guard's note.
--check-logs reads the logs of runs already made (NAME.1.0.log in DIR) and checks the last two. The logs stay in
--logs (default build/synth/n64-guard). Exit status 0 when it holds. Stdlib only, Python 3.9+.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ON = "rush2049pc: N64 guard (RUSH2049_N64_GUARD):"
HEAP = re.compile(r"^rush2049pc: N64 guard: the game heap's block at (\S+); (.*)$", re.M)
NOTE = re.compile(r"^rush2049pc: N64 guard: 0x[0-9A-Fa-f]+, .*$", re.M)


def check_log(text):
    """The problems a guarded run's log shows: the guard not on, no heap line, or a note of an N64 address used (each
    note's line). [] when it held."""
    bad = []
    if ON not in text:
        bad.append("the guard was not on (no 'N64 guard (RUSH2049_N64_GUARD)' line)")
    if not HEAP.search(text):
        bad.append("no line for the game heap's block")
    bad += [m.group(0) for m in NOTE.finditer(text)]
    return bad


def check_logs(folder):
    """Every guarded log in folder (NAME.1.0.log, synth_compare.py's second program's first run): {name: problems}."""
    out = {}
    for log in sorted(Path(folder).glob("*.1.0.log")):
        out[log.name[:-len(".1.0.log")]] = check_log(log.read_text(encoding="utf-8", errors="replace"))
    return out


def report(results):
    bad = 0
    for name, problems in results.items():
        if problems:
            bad += 1
            for p in problems:
                print(f"n64_guard: {name}: {p}")
    print(f"n64_guard: {len(results) - bad} of {len(results)} guarded runs clean")
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/n64-guard"))
    ap.add_argument("-j", "--jobs", type=int, default=1)
    ap.add_argument("--script", help="a press script to run instead of the synthetic paths (synth_compare.py's)")
    ap.add_argument("--until", help="with --script: --until-state")
    ap.add_argument("--seconds", default="90", help="with --script: --seconds")
    ap.add_argument("--check-logs", metavar="DIR", help="only check the guarded logs already in DIR")
    ap.add_argument("names", nargs="*")
    a = ap.parse_args()
    if a.check_logs:
        sys.exit(1 if report(check_logs(a.check_logs)) else 0)
    for old in Path(a.logs).glob("*.log"):   # this run's logs only
        old.unlink()
    cmd = [sys.executable, str(ROOT / "tools/port/synth_compare.py"), "--exe", a.exe, "--exe", a.exe, "--rom", a.rom,
           "--env-b", "RUSH2049_N64_GUARD=1", "--same", "--runs", "1", "--logs", a.logs, "-j", str(a.jobs)]
    if a.script:
        cmd += ["--script", a.script, "--until", a.until or "", "--seconds", a.seconds]
    cmd += a.names
    same = subprocess.run(cmd).returncode
    bad = report(check_logs(a.logs))
    if same:
        print("n64_guard: a guarded run's trace isn't its plain run's (synth_compare.py above)")
    sys.exit(1 if same or bad else 0)


if __name__ == "__main__":
    main()
