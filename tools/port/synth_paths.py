#!/usr/bin/env python3
"""Run the game on the synthetic cartridge along each path a press script describes (docs/port/SYNTH.md 2).

    python3 tools/port/synth_paths.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
                                      [--logs DIR] [NAME ...]

Each tools/port/synth/NAME.press is a --press-file script whose leading comments say what the run must reach:

    # until: 0x400000:600      --until-state: the state, and the graphics tasks to run there
    # seconds: 90              --seconds (game time with --fast)
    # expect: players 2, windows 2, cars 6, mode 0, track 0
    #                          what the state log's last line ("state reached: ... state=S: <info>") must say
    # visits: 0x2000 0x200     states the run must pass through on the way (the state log has each)
    # realtime                 run on the host's clock (the default is --fast, the virtual clock)
    # args: --screenshot-at 900 --screenshot race.png
    #                          more options (a path is relative to the logs directory, where the run happens;
    #                          a .png or .mpk named there is deleted first, so each run starts afresh)
    # check: tools/port/synth_shot.py race.png 1
    #                          then this command, in the logs directory (tools/ is the repository's)

A run passes if the program exits 0 (it reached the state in time and nothing faulted), its last state line matches,
it passed through the states visits names, and the check, if any, exits 0. Every run's log goes to DIR/NAME.log (default build/synth/paths). Exit status: the
number of paths that failed.
"""
import argparse
import os
import re
import shlex
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRESS_DIR = ROOT / "tools/port/synth"


def read_script(path):
    spec = {"until": None, "seconds": "60", "expect": "", "realtime": False, "args": [], "check": None, "visits": []}
    for line in path.read_text().splitlines():
        m = re.match(r"#\s*(until|seconds|expect|args|check|visits)\s*:\s*(.*)$", line)
        if m:
            key, value = m.group(1), m.group(2).strip()
            spec[key] = shlex.split(value) if key in ("args", "check", "visits") else value
        elif re.match(r"#\s*realtime\s*$", line):
            spec["realtime"] = True
    if not spec["until"]:
        raise SystemExit(f"synth_paths: {path} has no '# until:' line")
    return spec


def run(name, exe, rom, logs):
    spec = read_script(PRESS_DIR / f"{name}.press")
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--no-rom-check", "--headless", "--no-config",
           "--audio", "null", "--no-pak", "--press-file", str(PRESS_DIR / f"{name}.press"),
           "--until-state", spec["until"], "--seconds", spec["seconds"]]
    if not spec["realtime"]:
        cmd.append("--fast")
    cmd += spec["args"]
    for a in spec["args"]:              # what the run writes starts afresh: a screenshot, a controller pak
        if a.endswith((".png", ".mpk")) and (Path(logs) / a).exists():
            (Path(logs) / a).unlink()
    env = dict(os.environ, RUSH_TRACE_WAVES="1")
    t0 = time.time()
    try:
        p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        out, code = p.stdout.decode(errors="replace"), p.returncode
    except subprocess.TimeoutExpired as e:
        out, code = (e.stdout or b"").decode(errors="replace") + "\nsynth_paths: timed out\n", -1
    took = time.time() - t0
    (Path(logs) / f"{name}.log").write_text(out, encoding="utf-8")
    reached = [l for l in out.splitlines() if "state reached:" in l]
    last = reached[-1] if reached else ""
    why = []
    if code != 0:
        why.append(f"exit {code}")
    if not last:
        why.append("the state was not reached")
    elif spec["expect"] and spec["expect"] not in last:
        why.append(f"expected '{spec['expect']}'")
    for state in spec["visits"]:
        if f"game state 0x{int(state, 0):08X} " not in out:
            why.append(f"never in state {state}")
    if not why and spec["check"]:
        c = spec["check"]
        if c and c[0].startswith("tools/"):
            c = [sys.executable, str(ROOT / c[0])] + c[1:]
        r = subprocess.run(c, cwd=logs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out_c = r.stdout.decode(errors="replace").strip()
        if out_c:
            print("\n".join(f"  {l}" for l in out_c.splitlines()))
        if r.returncode != 0:
            why.append(f"check {' '.join(spec['check'])} failed")
    mode = "real time" if spec["realtime"] else "fast"
    print(f"synth_paths: {name:10} {'ok  ' if not why else 'FAIL'} {took:5.1f} s ({mode}) "
          f"{last.split('state reached: ', 1)[-1] if last else ''}{'; ' + ', '.join(why) if why else ''}", flush=True)
    if why:
        tail = [l for l in out.splitlines() if not re.search(r"wave .* loaded|wad .* converted", l)][-40:]
        print("\n".join(f"  | {l}" for l in tail))
    return not why


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/paths"))
    ap.add_argument("names", nargs="*", help="the paths to run (default: every tools/port/synth/*.press)")
    args = ap.parse_args()
    names = args.names or sorted(p.stem for p in PRESS_DIR.glob("*.press"))
    Path(args.logs).mkdir(parents=True, exist_ok=True)
    failed = sum(not run(n, args.exe, args.rom, args.logs) for n in names)
    if len(names) > 1:
        print(f"synth_paths: {len(names) - failed} of {len(names)} paths reached")
    sys.exit(failed)


if __name__ == "__main__":
    main()
