#!/usr/bin/env python3
"""The evenness of what a display shows under the vsync model (docs/port/FRAMERATE.md 7.9): the synthetic race in
real time under RUSH_SIM_VSYNC_HZ at each display rate, game.frame_rate and video.frame_rate given, the waits
precise, the platform giving the display's rate (RUSH_SIM_DISPLAY_TIMING=rate), counted from --after seconds (the
race). Prints per run the vblanks with no new picture, how long each new picture was held and its spread, and the
shown moment's step off the vblank's period (rms): how evenly a camera moving at a steady speed steps on screen.

  vsync_even.py --exe build/pc/rush2049pc --rom synth.z64 --hz 60,144 --game original,60 --rate original,display
  --check HZ:GAME:RATE:MAX_RMS_MS  fails when that run's step is off by more (repeatable)"""
import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def run(args, hz, game, rate):
    logs = Path(args.logs) / f"{hz}-{game}-{rate}"
    logs.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", RUSH_SIM_VSYNC_HZ=str(hz),
               RUSH_SIM_DISPLAY_HZ=str(int(round(float(hz)))), RUSH_SIM_DISPLAY_TIMING="rate",
               RUSH_SIM_EVEN_AFTER_S=str(args.after))
    env.pop("RUSH_SIM_TIMER_STEP_US", None)
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--no-pak", "--press-file", str(ROOT / "tools/port/synth/race.press"),
           "--seconds", str(args.seconds), "--audio", "null", "--set", f"game.frame_rate={game}", "--set", f"video.frame_rate={rate}"]
    p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    (logs / "run.log").write_text(out)
    v = re.search(r"simulated vsync at \S+ Hz, [^:]*: (\d+) vblanks, (\d+) showed no new frame", out)
    e = re.search(r"evenness: the shown moment's step off the vblank's ([\d.]+) ms by ([\d.]+) ms rms over (\d+) "
                  r"vblanks; (\d+) new pictures, held ([\d.]+) ms each, spread ([\d.]+) ms", out)
    a = re.search(r"^gfx: video.frame_rate auto: (.*)$", out, re.M)
    d = re.search(r"^gfx: frame rate \d+: (\d+) frames shown, (\d+) drawn between two", out, re.M)
    if not v or not e:
        return (hz, game, rate, None, out[-600:])
    steps, pictures = int(e[3]), int(e[4])
    return (hz, game, rate, dict(stale=1 - pictures / steps, held=float(e[5]), spread=float(e[6]), rms=float(e[2]),
                                 vblanks=steps, between=int(d[2]) if d else 0, auto=a[1] if a else ""), "")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--hz", default="60,75,120,144,165,240")
    ap.add_argument("--game", default="original,60")
    ap.add_argument("--rate", default="original,display")
    ap.add_argument("--seconds", type=float, default=20)
    ap.add_argument("--after", type=float, default=10)
    ap.add_argument("-j", "--jobs", type=int, default=3)
    ap.add_argument("--logs", default=str(ROOT / "build/synth/even"))
    ap.add_argument("--check", action="append", default=[])
    args = ap.parse_args()
    if os.name == "nt":
        print("vsync_even: the vsync model is Linux's: skipped")
        return 77
    jobs = [(hz, g, r) for hz in args.hz.split(",") for g in args.game.split(",") for r in args.rate.split(",")]
    with ThreadPoolExecutor(args.jobs) as ex:
        res = list(ex.map(lambda j: run(args, *j), jobs))
    ok = True
    got = {}
    print("display  game      video     no new picture  held ms (spread)  step off rms ms  between  auto")
    for hz, g, r, m, tail in res:
        if m is None:
            print(f"{hz:7} {g:9} {r:9} no evenness line:\n{tail}")
            ok = False
            continue
        got[(hz, g, r)] = m
        print(f"{hz:7} {g:9} {r:9} {m['stale']:14.3f}  {m['held']:7.2f} ({m['spread']:5.2f})   {m['rms']:15.2f}  "
              f"{m['between']:7}  {m['auto']}")
    for c in args.check:
        hz, g, r, lim = c.split(":")
        m = got.get((hz, g, r))
        if m is None or m["rms"] > float(lim):
            print(f"vsync_even: {hz} Hz, game {g}, video {r}: step off by "
                  f"{m['rms'] if m else 'nothing'} ms rms, more than {lim}")
            ok = False
    print("vsync_even: OK" if ok else "vsync_even: FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
