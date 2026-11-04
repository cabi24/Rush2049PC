#!/usr/bin/env python3
"""The game's frames at game.frame_rate 60 against original: one model tick a frame, and the camera's steps even
(docs/port/FRAMERATE.md 7.6).

    python3 tools/port/fps_even.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 [--logs DIR]
                                   [--script PRESS --seconds S] [NAME ...]

Each path (tools/port/synth/NAME.press, default drive and race2p) runs at 60 and at original on the virtual clock with
--log-frames: at each of the game's frames its retraces, the model tick, the camera's eye and car 0's drawn
position as the last frame drew them. In the race (the model tick past 0):

- the model ticks a frame: exactly its retraces (1 at 60, 2 at original), every frame: a frame that saw none or two
  at 60 would show the car still and then twice as far, a camera moving at 30 a second;
- the eye's steps: its step a frame divided by the frame's model ticks, between two frames whose eye moves; a step
  under a third or over three times the mean of its neighbours' is uneven (a camera moving at 30 a second at 60:
  every other step near 0 and the next twice). At 60 no larger a share of them than at original (plus half a
  percent), whose frames see the same motion two ticks at a time. A step over 50 ft (a cut, a resurrection) is left
  out.

--script runs one press script of any path (the retail cartridge: --rom). Exit status: the number of runs that fail.
Stdlib only, Python 3.9+.
"""
import argparse
import math
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_paths  # noqa: E402

_FRAME = re.compile(r"^rush2049pc: frame vi=(\d+): retraces (\d+) tick (-?\d+) eye (\S+) (\S+) (\S+) car (\S+) (\S+) (\S+)"
                    r" view (\S+) (\S+) (\S+)")


def run(exe, rom, script, seconds, logs, name, fps):
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--headless", "--no-config", "--audio", "null",
           "--no-pak", "--fast", "--press-file", str(Path(script).resolve()), "--seconds", str(seconds),
           "--log-frames", "--set", f"game.frame_rate={fps}"]
    if "synth" in Path(rom).name:
        cmd.insert(3, "--no-rom-check")
    env = dict(os.environ, RUSH_STATE_EXACT="1")
    env.setdefault("XDG_DATA_HOME", str(Path(logs).resolve() / "xdg"))
    log = Path(logs) / f"{name}-{fps}.log"
    with open(log, "w") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, env=env, cwd=logs, timeout=1800)
    frames = []
    for line in log.read_text(errors="replace").splitlines():
        m = _FRAME.match(line)
        if m:
            v = [float.fromhex(x) for x in m.groups()[3:]]
            frames.append((int(m.group(2)), int(m.group(3)), v[:3], v[3:6], v[6:9]))
    return r.returncode, frames


def check(frames, fps):
    """(ok, line): the race's frames (each with its successor) as the docstring says."""
    race = [(a, b) for a, b in zip(frames, frames[1:]) if a[1] > 0]
    if len(race) < 200:
        return False, f"{len(race)} race frames: too few", 1.0
    bad_ticks = [(a, b) for a, b in race if b[1] - a[1] != b[0]]
    want = 1 if fps == "60" else 2
    wrong_len = sum(1 for _, b in race if b[0] != want)
    # the eye's step a model tick, frame by frame; a cut or a resurrection (a step over 50 ft) breaks the run
    steps = []
    for a, b in race:
        d = b[1] - a[1]
        eye, car = math.dist(a[2], b[2]), math.dist(a[3], b[3])
        steps.append(None if d <= 0 or eye > 50 or car > 50 else eye / d)
    turns = []   # the view's turn a tick, in degrees (the pan)
    for a, b in race:
        d = b[1] - a[1]
        dot = sum(x * y for x, y in zip(a[4], b[4])) / ((math.hypot(*a[4]) * math.hypot(*b[4])) or 1.0)
        ang = math.degrees(math.acos(max(-1.0, min(1.0, dot))))
        turns.append(None if d <= 0 or ang > 20 else ang / d)

    def unevenness(seq, floor):
        moving = uneven = 0
        for p, q, r in zip(seq, seq[1:], seq[2:]):
            if p is None or q is None or r is None or p < floor or r < floor:
                continue
            moving += 1
            m = (p + r) / 2
            uneven += q < m / 3 or q > m * 3   # (nearly) still, or caught up, between two moving
        return moving, uneven
    moving, uneven = unevenness(steps, 0.005)
    panning, jerky = unevenness(turns, 0.02)
    ok = not bad_ticks and moving >= 100
    line = (f"{len(race)} race frames, {wrong_len} not {want} retraces long; model ticks a frame other than its "
            f"retraces: {len(bad_ticks)}; the eye moving in {moving} frames, {uneven} of them uneven (a step under a "
            f"third or over three times its neighbours' a tick); the view turning in {panning}, {jerky} uneven")
    if bad_ticks:
        a, b = bad_ticks[0]
        line += f"; first: tick {a[1]} to {b[1]} in a frame of {b[0]} retraces"
    return ok, line, ((uneven + jerky) / (moving + panning) if moving + panning else 1.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/fps-even"))
    ap.add_argument("--script")
    ap.add_argument("--seconds", type=float, default=60)
    ap.add_argument("names", nargs="*")
    a = ap.parse_args()
    os.makedirs(a.logs, exist_ok=True)
    jobs = []
    if a.script:
        jobs.append((Path(a.script).stem, a.script, a.seconds))
    else:
        for n in a.names or ["drive", "race2p"]:
            spec = synth_paths.read_script(synth_paths.PRESS_DIR / f"{n}.press")
            jobs.append((n, synth_paths.PRESS_DIR / f"{n}.press", float(spec["seconds"])))
    bad = 0
    for name, script, seconds in jobs:
        share = {}
        for fps in ("original", "60"):
            rc, frames = run(a.exe, a.rom, script, seconds, a.logs, name, fps)
            ok, line, share[fps] = check(frames, fps)
            if fps == "60" and ok:   # no more uneven steps than original's, which has the same motion
                ok = share["60"] <= share["original"] + 0.005
                line += f"; {share['60']:.1%} uneven at 60 against {share['original']:.1%} at original"
            ok = ok and rc == 0
            bad += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {name} at {fps}: exit {rc}; {line}")
    print(f"fps_even: {'ok' if not bad else f'{bad} failed'}")
    return bad


if __name__ == "__main__":
    sys.exit(main())
