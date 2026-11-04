#!/usr/bin/env python3
"""Compare the game at game.frame_rate original and at 60 along press scripts (docs/port/FRAMERATE.md 7).

    python3 tools/port/fps_compare.py --exe build/pc/rush2049pc [--rom ROM] [--logs DIR] [-j N] [NAME ...]
    python3 tools/port/fps_compare.py --exe EXE --rom ROM --script PRESS --seconds S [--logs DIR]

Each path (tools/port/synth/NAME.press, as tools/port/synth_paths.py runs it, always on the virtual clock, --fast,
and to its # seconds: rather than its # until:, whose count of graphics tasks is twice as quick at 60) runs once at
original and once at 60, with --log-physics 1 and RUSH_STATE_EXACT: every retrace, the model thread's tick and time
and each car's position and velocity, its lap and its place, as exact floats. The car physics runs per retrace in the
model thread, apart from the frames (FRAMERATE.md 1), so at the same model tick the two runs must agree:

- the physics: every model tick both runs reach, every car's position, velocity, lap and place, to the bit;
- the states: each state the game passes through, in the same order, and the retrace it comes at, which may differ:
  the game decides a change at a frame, 60 a second against 30, so a press or a timer can be seen a retrace sooner
  (the difference in retraces is reported; --max-shift bounds it);
- the frames: at 60 about one a retrace, at original one every two.

The model tick counts the race's retraces from its start (negative in the countdown), so a race that starts at
another retrace still compares tick for tick; inputs the script changes at a fixed time then reach the cars at
another tick, which is reported as where the physics first differs, with the input's change at that tick. A path
compares the physics only from the first tick both runs reach to the last; a run with no race compares its states.

Exit status: the number of paths whose physics differ, whose states differ in kind or order, or that shift a state
by more than --max-shift retraces. Stdlib only, Python 3.9+.
"""
import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_paths  # noqa: E402

_STATE = re.compile(r"^rush2049pc: game state (0x[0-9A-F]{8}) \(was 0x[0-9A-F]{8}\) at vi (\d+), after (\d+) graphics "
                    r"tasks")
_PHYS = re.compile(r"^rush2049pc: physics vi=(\d+): tick (-?\d+) time (\S+) cars (\d+)(.*)$")
_END = re.compile(r"^rush2049pc: (?:time reached:|state reached:) t=\S+ vi=(\d+) gfx_tasks=(\d+)")


def parse(text):
    states, phys, end = [], {}, None
    for line in text.splitlines():
        m = _PHYS.match(line)
        if m:
            vi, tick = int(m.group(1)), int(m.group(2))
            # The tick moves on each retrace while the race runs; the first retrace at a tick is where it was set
            # (the model thread steps once per retrace, after the loop has read the line).
            phys.setdefault(tick, (vi, m.group(3), int(m.group(4)), m.group(5)))
            continue
        m = _STATE.match(line)
        if m:
            states.append((m.group(1), int(m.group(2)), int(m.group(3))))
            continue
        m = _END.match(line)
        if m:
            end = (int(m.group(1)), int(m.group(2)))
    return states, phys, end


def run(exe, rom, script, seconds, extra, logs, tag):
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--headless", "--no-config", "--audio", "null",
           "--no-pak", "--fast", "--press-file", str(Path(script).resolve()), "--seconds", str(seconds),
           "--log-physics", "1", "--set", f"game.frame_rate={tag}"] + extra
    if "synth" in Path(rom).name:
        cmd.insert(3, "--no-rom-check")
    env = dict(os.environ, RUSH_STATE_EXACT="1")
    env.setdefault("XDG_DATA_HOME", str(Path(logs).resolve() / "xdg"))
    log = Path(logs) / f"{Path(script).stem}-{tag}.log"
    with open(log, "w") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, env=env, cwd=logs, timeout=1800)
    return r.returncode, log.read_text(errors="replace")


def compare(name, a, b, max_shift):
    """a: original's (rc, text), b: 60's. Returns (ok, lines)."""
    out, ok = [], True
    (rca, ta), (rcb, tb) = a, b
    sa, pa, ea = parse(ta)
    sb, pb, eb = parse(tb)
    if rca or rcb:
        out.append(f"exit codes {rca} and {rcb}")
        ok = False
    # States: the same sequence, at retraces that may move.
    if [s for s, _, _ in sa] != [s for s, _, _ in sb]:
        ok = False
        out.append("states differ: " + " ".join(s for s, _, _ in sa) + " / " + " ".join(s for s, _, _ in sb))
    else:
        shifts = [vb - va for (_, va, _), (_, vb, _) in zip(sa, sb)]
        worst = max((abs(x) for x in shifts), default=0)
        out.append(f"{len(sa)} states, the same order; retraces at 60 minus original: "
                   + " ".join(f"{s[0][2:].lstrip('0') or '0'}:{d:+d}" for s, d in zip(sa, shifts)))
        if worst > max_shift:
            ok = False
            out.append(f"a state moved {worst} retraces (more than {max_shift})")
    # Frames.
    if ea and eb:
        out.append(f"graphics tasks: {ea[1]} in {ea[0]} retraces at original, {eb[1]} in {eb[0]} at 60")
    # Physics, tick by tick.
    ticks = sorted(set(pa) & set(pb))
    racing = [t for t in ticks if pa[t][2] > 0]
    if not racing:
        out.append("no race: physics not compared")
        return ok, out
    first_diff = None
    for t in racing:
        if pa[t][1:] != pb[t][1:]:
            first_diff = t
            break
    vi_a, vi_b = pa[racing[0]][0], pb[racing[0]][0]
    if first_diff is None:
        out.append(f"physics: the same at every one of {len(racing)} model ticks ({racing[0]} to {racing[-1]}), "
                   f"{pa[racing[0]][2]} cars; tick {racing[0]} at retrace {vi_a} at original, {vi_b} at 60")
    else:
        ok = False
        da, db = pa[first_diff], pb[first_diff]
        out.append(f"physics DIFFER from model tick {first_diff} (retrace {da[0]} at original, {db[0]} at 60), "
                   f"{len([t for t in racing if t < first_diff])} ticks the same before it")
        out.append("  original: " + da[3][:300])
        out.append("  60:       " + db[3][:300])
    return ok, out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/fps"))
    ap.add_argument("--script", help="one press script of any path (the retail cartridge: --rom)")
    ap.add_argument("--seconds", type=float, help="with --script: how long to run")
    ap.add_argument("--max-shift", type=int, default=30, help="the most retraces a state may move (default 30)")
    ap.add_argument("--arg", action="append", default=[], help="another option for both runs (repeatable)")
    ap.add_argument("-j", type=int, default=2)
    ap.add_argument("names", nargs="*")
    args = ap.parse_args()
    os.makedirs(args.logs, exist_ok=True)
    jobs = []
    if args.script:
        jobs.append((Path(args.script).stem, args.script, args.seconds or 60, []))
    else:
        names = args.names or sorted(p.stem for p in synth_paths.PRESS_DIR.glob("*.press"))
        for n in names:
            spec = synth_paths.read_script(synth_paths.PRESS_DIR / f"{n}.press")
            extra, skip = [], False
            for a in spec["args"]:   # no screenshots: they are taken at a count of graphics tasks
                if skip:
                    skip = False
                    continue
                if a in ("--screenshot-at", "--screenshot"):
                    skip = True
                    continue
                extra.append(a)
            jobs.append((n, synth_paths.PRESS_DIR / f"{n}.press", float(spec["seconds"]), extra))
    bad = 0
    with concurrent.futures.ThreadPoolExecutor(args.j) as ex:
        futs = {}
        for n, script, secs, extra in jobs:
            futs[n] = (ex.submit(run, args.exe, args.rom, script, secs, extra + args.arg, args.logs, "original"),
                       ex.submit(run, args.exe, args.rom, script, secs, extra + args.arg, args.logs, "60"))
        for n, _, _, _ in jobs:
            ok, lines = compare(n, futs[n][0].result(), futs[n][1].result(), args.max_shift)
            bad += not ok
            print(f"{'ok' if ok else 'FAIL'}  {n}")
            for l in lines:
                print(f"      {l}")
    print(f"fps_compare: {len(jobs) - bad} of {len(jobs)} paths agree")
    return bad


if __name__ == "__main__":
    sys.exit(main())
