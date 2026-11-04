#!/usr/bin/env python3
"""The game at 60 on a host that can't keep 60 (docs/port/FRAMERATE.md 7.8), on the virtual clock.

    python3 tools/port/fps_slow.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 [--logs DIR]

RUSH_SIM_GFX_MS (port/ultra/rcp.c) keeps the RDP that long for each graphics task: at 20 ms, more than a retrace,
every frame of the game at 60 takes two retraces, as on a host too slow for 60. Along the synthetic solo race
(tools/port/synth/solo.press, one car that nothing on the game thread steers) three runs, each with --log-physics 1:

- at 60: the game draws a frame every retrace in the race, and the log says nothing of a slow host;
- at 60 with RUSH_SIM_GFX_MS=20: the log's line that the host can't keep 60 comes once, with the share it measured
  (about 0.50);
- at original with RUSH_SIM_GFX_MS=20: original's frames are two retraces anyway, and nothing is said. The game at 60
  keeps its speed on the slow host: its frames are the N64's, and the car's physics is the same as this run's at
  every model tick both reach, the race starting within a retrace of it. (Not against the first run: the solo race's
  car resurrects at tick 217, at the place the game thread last saw it pass, which the frames' timing decides:
  docs/port/FRAMERATE.md 7.4.)

Exit status: the number of checks that failed. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import fps_compare  # noqa: E402

_SLOW = re.compile(r"^rush2049pc: game\.frame_rate = 60: the game drew (\d\.\d\d) of its frames in (\d+) s of the race")
_RACE = 0x00400000


def run(exe, rom, logs, tag, fps, slow_ms, seconds):
    script = ROOT / "tools/port/synth/solo.press"
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--no-rom-check", "--headless", "--no-config",
           "--audio", "null", "--no-pak", "--fast", "--press-file", str(script), "--seconds", str(seconds),
           "--log-physics", "1", "--set", f"game.frame_rate={fps}"]
    env = dict(os.environ, RUSH_STATE_EXACT="1")
    env.pop("RUSH_SIM_GFX_MS", None)
    if slow_ms:
        env["RUSH_SIM_GFX_MS"] = str(slow_ms)
    env.setdefault("XDG_DATA_HOME", str(Path(logs).resolve() / "xdg"))
    log = Path(logs) / f"solo-{tag}.log"
    with open(log, "w") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, env=env, cwd=logs, timeout=600)
    text = log.read_text(errors="replace")
    states, phys, end = fps_compare.parse(text)
    slow = [m for m in (_SLOW.match(l) for l in text.splitlines()) if m]
    race = next((vi for s, vi, _ in states if int(s, 16) == _RACE), None)
    # the game's frames in the race: graphics tasks from the race's start to the end
    race_gfx = next((g for s, _, g in states if int(s, 16) == _RACE), None)
    share = None
    if race is not None and end and end[0] > race:
        share = (end[1] - race_gfx) / (end[0] - race)
    return {"rc": r.returncode, "phys": phys, "slow": slow, "race": race, "share": share}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/fps-slow"))
    ap.add_argument("--seconds", type=float, default=50)
    ap.add_argument("--slow-ms", type=float, default=20)
    a = ap.parse_args()
    os.makedirs(a.logs, exist_ok=True)
    fast = run(a.exe, a.rom, a.logs, "60", 60, 0, a.seconds)
    slow = run(a.exe, a.rom, a.logs, "60-slow", 60, a.slow_ms, a.seconds)
    orig = run(a.exe, a.rom, a.logs, "original-slow", "original", a.slow_ms, a.seconds)
    bad = 0

    def check(ok, what):
        nonlocal bad
        bad += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {what}")

    for name, r in (("60", fast), ("60, the RDP slowed", slow), ("original, the RDP slowed", orig)):
        check(r["rc"] == 0 and r["race"] is not None and r["share"] is not None,
              f"{name}: exit {r['rc']}, the race at retrace {r['race']}, "
              + (f"{r['share']:.2f} of its frames in the race" if r["share"] is not None else "no frames counted"))
    if bad:
        return bad
    check(fast["share"] > 0.95 and not fast["slow"], f"60: a frame a retrace ({fast['share']:.2f}), no line of a slow host")
    check(len(slow["slow"]) == 1 and float(slow["slow"][0].group(1)) < 0.9 and slow["share"] < 0.6,
          f"60 slowed: one line of a slow host ({', '.join(m.group(0)[12:] for m in slow['slow']) or 'none'}), "
          f"{slow['share']:.2f} of the frames")
    check(not orig["slow"], f"original slowed: nothing said ({orig['share']:.2f} of the frames)")
    ticks = sorted(t for t in set(orig["phys"]) & set(slow["phys"]) if orig["phys"][t][2] > 0)
    diff = next((t for t in ticks if orig["phys"][t][1:] != slow["phys"][t][1:]), None)
    check(len(ticks) > 1000 and diff is None,
          f"the game at 60 keeps its speed on the slow host: the car's physics the same as original's at {len(ticks)} "
          f"model ticks" + (f", first different at tick {diff}" if diff is not None else ""))
    check(abs(slow["race"] - orig["race"]) <= 1,
          f"the race starts at retrace {slow['race']} at 60, {orig['race']} at original, both slowed")
    print(f"fps_slow: {'ok' if not bad else f'{bad} failed'}")
    return bad


if __name__ == "__main__":
    sys.exit(main())
