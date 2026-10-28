#!/usr/bin/env python3
"""A long run in real time in the configuration a player has, with its memory sampled (docs/port/QA.md "Integration
soak").

    python3 tools/port/soak.py --exe build/opt/rush2049pc --rom ROM [--no-rom-check] --press-file SCRIPT
                               --seconds N --out DIR [--name NAME] [--size 2880x1620] [--until-state M:N]
                               [--set S.K=V ...] [--env K=V ...] [-- more program options]
    python3 tools/port/soak.py --exe ... --rom SYNTH --no-rom-check --path NAME --out DIR
                               one of tools/port/synth/NAME.press with its own targets (# until, seconds, expect,
                               visits, args), checked as synth_paths.py checks them, but in real time like this

The run: headless at --size (the window's pixels, which video.scale = native and video.widescreen = auto follow),
the settings' defaults (--no-config) plus video.textures=upscaled and video.frame_rate=120 unless --set says
otherwise, SDL's disk audio driver at 48 kHz (consumed in real time, into DIR/NAME.raw, deleted at the end; SDL
2.30's driver refuses /dev/null), and the timing of a Windows desktop: RUSH_SIM_TIMER_STEP_US=15625 (winpthreads'
timed waits) and RUSH_SIM_VSYNC_HZ=60 (a vsync swap). The settings
folder, the data folder (the log, the texture cache) and the controller pak are in DIR, so nothing of the user's is
read or written. The program's resident memory is read every --every seconds (default 10) into DIR/NAME.rss.

At the end it prints the exit lines (retraces, audio, graphics), the memory (first sample after --warmup seconds,
last, most, and its growth over the second half of the run in MB a minute), and the log's most repeated lines with
the numbers in them taken out (repeated warnings would fill rush2049pc.log over an hour). Exit status: the
program's. Stdlib only, Python 3.9+, Linux (it reads /proc).
"""
import argparse
import collections
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

DEFAULT_SETS = ["video.textures=upscaled", "video.frame_rate=120"]


def rss_kb(pid):
    try:
        with open(f"/proc/{pid}/status") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except OSError:
        return None
    return None


def threads(pid):
    try:
        return len(os.listdir(f"/proc/{pid}/task"))
    except OSError:
        return 0


def growth(samples, warmup):
    """MB a minute over the second half of the samples after the warm-up (least squares)."""
    pts = [(t, kb) for t, kb, _ in samples if t >= warmup]
    pts = pts[len(pts) // 2:]
    if len(pts) < 3:
        return 0.0
    n = len(pts)
    mt = sum(t for t, _ in pts) / n
    mk = sum(k for _, k in pts) / n
    var = sum((t - mt) ** 2 for t, _ in pts)
    if var == 0:
        return 0.0
    slope = sum((t - mt) * (k - mk) for t, k in pts) / var  # kB a second
    return slope * 60 / 1024


def noise(text, top):
    """The most repeated lines, numbers and hex taken out."""
    norm = collections.Counter()
    for line in text.splitlines():
        if not line.strip() or line.startswith("state "):
            continue
        key = re.sub(r"0x[0-9a-fA-F]+|[-+]?\d+(\.\d+)?", "#", line.strip())
        norm[key] += 1
    return norm.most_common(top)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--no-rom-check", action="store_true")
    ap.add_argument("--press-file")
    ap.add_argument("--path", help="a synthetic path (tools/port/synth/NAME.press) with its own targets")
    ap.add_argument("--seconds", type=float)
    ap.add_argument("--until-state")
    ap.add_argument("--out", required=True)
    ap.add_argument("--name", default="soak")
    ap.add_argument("--size", default="2880x1620")
    ap.add_argument("--set", action="append", default=[], help="a setting for the run (over the defaults above)")
    ap.add_argument("--env", action="append", default=[], help="K=V in the program's environment")
    ap.add_argument("--every", type=float, default=10)
    ap.add_argument("--warmup", type=float, default=60, help="seconds before memory growth counts")
    ap.add_argument("--no-sim", action="store_true", help="the host's own timing, no Windows model")
    ap.add_argument("--pak", help="a controller pak image (default DIR/NAME.mpk, made fresh)")
    ap.add_argument("--top", type=int, default=8)
    ap.add_argument("rest", nargs="*", help="more program options, after --")
    args = ap.parse_args()
    spec = None
    if args.path:
        from synth_paths import PRESS_DIR, read_script
        spec = read_script(PRESS_DIR / f"{args.path}.press")
        args.press_file = str(PRESS_DIR / f"{args.path}.press")
        args.until_state = args.until_state or spec["until"]
        args.seconds = args.seconds or float(spec["seconds"])
        args.rest = spec["args"] + args.rest
        if args.name == "soak":
            args.name = args.path
    if not args.seconds:
        ap.error("--seconds is needed")

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    data = out / f"{args.name}.data"
    conf = out / f"{args.name}.config"
    data.mkdir(exist_ok=True)
    conf.mkdir(exist_ok=True)
    pak = Path(args.pak).resolve() if args.pak else out / f"{args.name}.mpk"
    if not args.pak and pak.exists():
        pak.unlink()

    env = dict(os.environ, SDL_AUDIODRIVER="disk", SDL_DISKAUDIOFILE=str(out / f"{args.name}.raw"),
               RUSH2049_AUDIO_RATE="48000", XDG_DATA_HOME=str(data), XDG_CONFIG_HOME=str(conf))
    if not args.no_sim:
        env.update(RUSH_SIM_TIMER_STEP_US="15625", RUSH_SIM_VSYNC_HZ="60")
    for kv in args.env:
        k, _, v = kv.partition("=")
        env[k] = v

    sets = dict(s.split("=", 1) for s in DEFAULT_SETS)
    for s in args.set:
        k, _, v = s.partition("=")
        sets[k] = v
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--headless", "--no-config",
           "--size", args.size, "--audio", "sdl", "--pak", str(pak), "--seconds", str(args.seconds), "--status", "10"]
    if args.no_rom_check:
        cmd.append("--no-rom-check")
    if args.press_file:
        cmd += ["--press-file", str(Path(args.press_file).resolve())]
    if args.until_state:
        cmd += ["--until-state", args.until_state]
    for k, v in sets.items():
        cmd += ["--set", f"{k}={v}"]
    cmd += args.rest

    log_path = out / f"{args.name}.out"
    rss_path = out / f"{args.name}.rss"
    samples = []
    t0 = time.monotonic()
    with open(log_path, "wb") as log:
        p = subprocess.Popen(cmd, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
        next_t = 0.0
        while p.poll() is None:
            t = time.monotonic() - t0
            if t >= next_t:
                kb = rss_kb(p.pid)
                if kb is not None:
                    samples.append((t, kb, threads(p.pid)))
                next_t += args.every
            time.sleep(0.25)
    wall = time.monotonic() - t0
    (out / f"{args.name}.raw").unlink(missing_ok=True)   # what the disk driver wrote: 384 kB a second
    rss_path.write_text("".join(f"{t:.1f} {kb} {n}\n" for t, kb, n in samples))

    text = log_path.read_text(errors="replace")
    print(f"soak {args.name}: exit {p.returncode} after {wall:.0f} s; {' '.join(cmd[1:])}")
    for line in text.splitlines():
        if re.match(r"(rush2049pc: \d+ retraces|audio: \w+ at |gfx: .*(frames|upscaled|textures|shown)|"
                    r"rush2049pc: .*(fault|signal|hang|stall)|.*(SIGSEGV|Assertion|abort|GL error))", line):
            print("  " + line)
    if samples:
        warm = [s for s in samples if s[0] >= args.warmup] or samples
        print(f"  memory: {warm[0][1] / 1024:.0f} MB at {warm[0][0]:.0f} s, {samples[-1][1] / 1024:.0f} MB at the "
              f"end, most {max(s[1] for s in samples) / 1024:.0f} MB; second half {growth(samples, args.warmup):+.2f} "
              f"MB a minute; threads {samples[-1][2]}")
    logs = list(data.rglob("rush2049pc.log"))
    body = logs[0].read_text(errors="replace") if logs else text
    print(f"  log: {len(body.splitlines())} lines, {len(body)} bytes"
          f"{' (' + str(logs[0]) + ')' if logs else ' (the output)'}; most repeated:")
    for line, n in noise(body, args.top):
        print(f"  {n:6d}  {line[:150]}")
    if spec:   # synth_paths.py's checks
        reached = [l for l in text.splitlines() if "state reached:" in l]
        why = [] if p.returncode == 0 else [f"exit {p.returncode}"]
        if not reached:
            why.append("the state was not reached")
        elif spec["expect"] and spec["expect"] not in reached[-1]:
            why.append(f"expected '{spec['expect']}'")
        why += [f"never in state {v}" for v in spec["visits"] if f"game state 0x{int(v, 0):08X} " not in text]
        print(f"  path {args.path}: {'ok' if not why else 'FAIL: ' + ', '.join(why)}"
              f"{'; ' + reached[-1].split('state reached: ', 1)[-1] if reached else ''}")
        return 1 if why else 0
    return p.returncode


if __name__ == "__main__":
    sys.exit(main())
