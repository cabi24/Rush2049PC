#!/usr/bin/env python3
"""Compare two builds of the game along the press scripts' paths, on the virtual clock (docs/port/OPTIMIZED.md).

    python3 tools/port/synth_compare.py --exe build/pc/rush2049pc --exe build/rel/rush2049pc \
        [--rom build/synth/rush2049_synth.z64] [--runs 2] [--status 1] [--logs DIR] [NAME ...]
    python3 tools/port/synth_compare.py --exe A --exe B --rom ROM --script PRESS --until MASK:N --seconds S

Each path (tools/port/synth/NAME.press, as tools/port/synth_paths.py runs it, but always with --fast) runs --runs
times with each executable, with three settings:

- RUSH_FAST_STRICT: the virtual clock's strict mode (docs/port/ULTRA.md 1), where the host main thread runs the RSP
  tasks, posts events and reads the game's state only while every N64 thread waits. Plain --fast (--loose) lands
  those wherever a computing thread happens to be, so it keeps a run only to within a graphics task, and the
  physics amplify that: two runs of one build drift apart. Strict, a run depends on the N64 clock alone.
- RUSH_STATE_EXACT: the state log gives the first car's position and speed as exact floats (--coarse: whole feet,
  for a build without it).
- --status: the log gives them every --status seconds of game time too.

A run's trace is the loader's summary line, its state changes (the state, the retrace it happened at, the players,
windows, cars, mode, track, pause state, the car) and its status lines (the same at a fixed retrace), without the
graphics-task counts. Runs of one executable give the baseline. The verdict per path:

    same      every run of both executables has one trace
    varies    the executables' traces differ, but a run of one is a run of the other, or they differ no earlier
              than one executable's runs among themselves (the first place each differs is shown)
    DIFFERS   otherwise

--script, --until and --seconds run one press script of any path instead (the retail cartridge: --rom, no
--no-rom-check then, docs/port/BRINGUP.md); the logs stay in --logs. Exit status: the number of paths that DIFFER,
or with --same (CI, strict) that aren't the same. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_paths  # noqa: E402

_STATE = re.compile(r"^rush2049pc: game state (0x[0-9A-F]{8}) \(was 0x[0-9A-F]{8}\) at vi (\d+), "
                    r"after \d+ graphics tasks(?:: (.*))?$")
EXACT = True   # RUSH_STATE_EXACT: positions and speeds as exact floats (--coarse: whole feet)
STRICT = True  # RUSH_FAST_STRICT: the main thread acts only while every N64 thread waits (--loose: plain --fast)
SAME = False   # --same: only "same" passes
_STATUS = re.compile(r"^rush2049pc: (status|state reached:|time reached:) t=\S+ vi=(\d+) gfx_tasks=\d+ "
                     r"audio_tasks=\d+(?: state=(0x[0-9A-F]{8}))?(?:: (.*))?$")


def trace(text):
    """The comparable events of one run's log: ('state', vi, state, info) and ('status', vi, state, info). The
    end line ("state reached:", after a number of graphics tasks) keeps only its state and info."""
    out = []
    for line in text.splitlines():
        if line.startswith("pc_data: "):   # the loader's summary: what it filled and how pointers resolved
            out.append(("data", None, "", line[9:].strip()))
            continue
        m = _STATE.match(line)
        if m:
            out.append(("state", int(m.group(2)), m.group(1), m.group(3) or ""))
            continue
        m = _STATUS.match(line)
        if m:
            kind = "status" if m.group(1) == "status" else "end"
            out.append((kind, int(m.group(2)) if kind == "status" else None, m.group(3) or "", m.group(4) or ""))
        elif "Segmentation fault" in line or "rush2049pc: fault" in line:
            out.append(("fault", None, "", line.strip()))
    return tuple(out)


def first_difference(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i, x, y
    if len(a) != len(b):
        i = min(len(a), len(b))
        return i, a[i] if i < len(a) else None, b[i] if i < len(b) else None
    return None


def show(e):
    if e is None:
        return "(nothing: the run ended)"
    kind, vi, state, info = e
    return f"{kind} vi {vi} {state}: {info}" if vi is not None else f"{kind} {state}: {info}"


def run_one(exe, rom, cmd_tail, logs, tag, retail):
    env = dict(os.environ)
    if EXACT:
        env["RUSH_STATE_EXACT"] = "1"
    if STRICT:
        env["RUSH_FAST_STRICT"] = "1"
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve())] + ([] if retail else ["--no-rom-check"]) + \
        ["--headless", "--no-config", "--audio", "null", "--no-pak", "--fast"] + cmd_tail
    with tempfile.TemporaryDirectory(dir=logs) as cwd:   # screenshots and paks start afresh, and don't collide
        t0 = time.time()
        p = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        took = time.time() - t0
    text = p.stdout.decode(errors="replace")
    (Path(logs) / f"{tag}.log").write_text(text, encoding="utf-8")
    return trace(text), p.returncode, took


def compare(name, exes, rom, cmd_tail, runs, logs, retail):
    traces = []   # per executable: [trace, ...]
    times = []
    for k, exe in enumerate(exes):
        ts, tt = [], []
        for r in range(runs):
            t, code, took = run_one(exe, rom, cmd_tail, logs, f"{name}.{k}.{r}", retail)
            ts.append(t + ((("exit", None, "", str(code)),) if code else ()))
            tt.append(took)
        traces.append(ts)
        times.append(min(tt))
    sets = [set(ts) for ts in traces]
    events = len(traces[0][0])
    timing = ", ".join(f"{t:.2f} s" for t in times)
    if len(sets[0]) == 1 and sets[0] == sets[1]:
        print(f"synth_compare: {name:10} same     {events} events ({timing})", flush=True)
        return 0

    def first(pairs):   # the earliest event at which two traces of the pairs differ
        return min((first_difference(x, y) or (len(x),))[0] for x, y in pairs) if pairs else None
    within = first([(x, y) for s in sets for x in s for y in s if x != y])
    across = first([(x, y) for x in sets[0] for y in sets[1]])
    overlap = bool(sets[0] & sets[1])
    # The builds agree if a run of one is a run of the other, or if they start to differ no earlier than one build's
    # own runs do (the virtual clock keeps a graphics task's tolerance, docs/port/ULTRA.md 1).
    verdict = "varies " if overlap or (within is not None and across >= within) else "DIFFERS"
    print(f"synth_compare: {name:10} {verdict}  {events} events ({timing}); runs give {len(sets[0])} and "
          f"{len(sets[1])} traces, {'some shared' if overlap else 'none shared'}", flush=True)
    for label, a, b in (("A vs B", traces[0][0], traces[1][0]),
                        ("A runs", traces[0][0], next((t for t in traces[0] if t != traces[0][0]), None)),
                        ("B runs", traces[1][0], next((t for t in traces[1] if t != traces[1][0]), None))):
        if b is None:
            continue
        d = first_difference(a, b)
        if d:
            i, x, y = d
            print(f"  {label}: event {i}\n    {show(x)}\n    {show(y)}")
    return 0 if verdict != "DIFFERS" and not SAME else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", action="append", required=True, help="an executable (give two)")
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--runs", type=int, default=2, help="runs of each path per executable")
    ap.add_argument("--status", default="1", help="whole seconds of game time between status lines")
    ap.add_argument("--logs", default=str(ROOT / "build/synth/compare"))
    ap.add_argument("--script", help="a press script to run instead of the synthetic paths")
    ap.add_argument("--until", help="with --script: --until-state")
    ap.add_argument("--seconds", default="90", help="with --script: --seconds")
    ap.add_argument("--coarse", action="store_true",
                    help="positions in whole feet, as builds without RUSH_STATE_EXACT print them")
    ap.add_argument("--same", action="store_true", help="a path whose runs aren't all the same fails")
    ap.add_argument("--loose", action="store_true",
                    help="plain --fast (without RUSH_FAST_STRICT), as synth_paths.py runs: within a graphics task")
    ap.add_argument("names", nargs="*", help="the paths (default: every tools/port/synth/*.press)")
    a = ap.parse_args()
    if len(a.exe) != 2:
        ap.error("give --exe twice")
    global EXACT, STRICT, SAME
    EXACT, STRICT, SAME = not a.coarse, not a.loose, a.same
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    differ = 0
    if a.script:
        if not a.until:
            ap.error("--script needs --until")
        tail = ["--press-file", str(Path(a.script).resolve()), "--until-state", a.until, "--seconds", a.seconds,
                "--status", a.status]
        differ += compare(Path(a.script).stem, a.exe, a.rom, tail, a.runs, a.logs, retail=True)
    else:
        names = a.names or sorted(p.stem for p in synth_paths.PRESS_DIR.glob("*.press"))
        for name in names:
            press = synth_paths.PRESS_DIR / f"{name}.press"
            spec = synth_paths.read_script(press)
            tail = ["--press-file", str(press), "--until-state", spec["until"], "--seconds", spec["seconds"],
                    "--status", a.status] + spec["args"]
            differ += compare(name, a.exe, a.rom, tail, a.runs, a.logs, retail=False)
        if len(names) > 1:
            print(f"synth_compare: {len(names) - differ} of {len(names)} paths agree")
    sys.exit(differ)


if __name__ == "__main__":
    main()
