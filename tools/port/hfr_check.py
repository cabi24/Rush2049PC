#!/usr/bin/env python3
"""The game's timing with a high frame rate (video.frame_rate, docs/port/GFX.md "High frame rate"): the same.

    python3 tools/port/hfr_check.py [--exe build/pc/rush2049pc] [--rom build/synth/rush2049_synth.z64]
                                    [--rate 120] [--rom-check] [--require-drawn] [--press FILE ...] [NAME ...]

Runs each path twice on the virtual clock (--fast), with --frame-rate original and with --frame-rate RATE, and
compares what the game did against N64 time: every change of its state word (with the retrace, the graphics tasks,
the players, cars, mode and track, and the first car's position and speed), a status line every second of N64 time
(the retrace, the graphics and audio tasks so far, the same game values), and where it stopped. They must be the
same line for line. With --require-drawn the run with the setting on must also have drawn frames between the
game's, so the interpolator ran (a path whose frames have no 3D, like the synthetic battle arena, draws none). The
paths are tools/port/synth/NAME.press (their '# until:', '# seconds:' and '# args:' lines, as synth_paths.py reads
them), or --press files of the same form; the default is the synthetic cartridge's race2p, drive and demo. Exit
status: the number of paths that differ. Logs: build/synth/hfr/.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_paths import PRESS_DIR, ROOT, read_script  # noqa: E402

KEEP = re.compile(r"rush2049pc: (game state |status |state reached:|time reached:)")


def run(exe, rom, press, spec, rate, rom_check, log):
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--headless", "--no-config", "--audio",
           "null", "--no-pak", "--fast", "--press-file", str(press), "--until-state", spec["until"], "--seconds",
           spec["seconds"], "--status", "1", "--frame-rate", rate]
    if not rom_check:
        cmd.append("--no-rom-check")
    cmd += spec["args"]
    p = subprocess.run(cmd, cwd=log.parent, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    log.write_text(out, encoding="utf-8")
    drawn = 0
    m = re.search(r"gfx: frame rate \d+: \d+ frames shown, (\d+) drawn between two", out)
    if m:
        drawn = int(m.group(1))
    return p.returncode, [l for l in out.splitlines() if KEEP.match(l)], drawn


def strip_screenshot_args(args):   # screenshots aren't the point here
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ("--screenshot-at", "--screenshot"):
            skip = True
            continue
        out.append(a)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--rate", default="120")
    ap.add_argument("--rom-check", action="store_true", help="check the ROM's SHA-1 (a retail cartridge)")
    ap.add_argument("--require-drawn", action="store_true", help="fail a path that drew no frame between two")
    ap.add_argument("--press", nargs="*", default=[], help="press files with '# until:' lines, besides the names")
    ap.add_argument("--logs", default=str(ROOT / "build/synth/hfr"))
    ap.add_argument("names", nargs="*", help="tools/port/synth paths (default: race2p drive demo)")
    args = ap.parse_args()
    paths = [PRESS_DIR / f"{n}.press" for n in args.names]
    paths += [Path(p).resolve() for p in args.press]
    if not paths:
        paths = [PRESS_DIR / f"{n}.press" for n in ("race2p", "drive", "demo")]
    logs = Path(args.logs)
    logs.mkdir(parents=True, exist_ok=True)
    failed = 0
    for press in paths:
        spec = read_script(press)
        spec["args"] = strip_screenshot_args(spec["args"])
        name = press.stem
        code0, off, _ = run(args.exe, args.rom, press, spec, "original", args.rom_check, logs / f"{name}-original.log")
        code1, on, drawn = run(args.exe, args.rom, press, spec, args.rate, args.rom_check,
                               logs / f"{name}-{args.rate}.log")
        why = []
        if code0 != 0 or code1 != 0:
            why.append(f"exit {code0} and {code1}")
        if not off:
            why.append("no state lines")
        if off != on:
            for i, (a, b) in enumerate(zip(off + [""] * len(on), on + [""] * len(off))):
                if a != b:
                    why.append(f"line {i + 1} differs:\n    original: {a}\n    {args.rate:>8}: {b}")
                    break
        if drawn == 0 and args.require_drawn:
            why.append(f"no frame drawn between two at {args.rate}")
        last = off[-1].split(": ", 1)[-1] if off else ""
        print(f"hfr_check: {name:10} {'same' if not why else 'DIFFERS'}: {len(off)} lines, {drawn} frames drawn "
              f"between two at {args.rate}; {last}", flush=True)
        for w in why:
            print(f"  {w}")
        failed += bool(why)
    sys.exit(failed)


if __name__ == "__main__":
    main()
