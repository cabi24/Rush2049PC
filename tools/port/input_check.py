#!/usr/bin/env python3
"""Live input through SDL drives the game as scripted input does (docs/port/FRONTEND.md "Testing live input").

    python3 tools/port/input_check.py [--exe build/pc/rush2049pc] [--rom build/synth/rush2049_synth.z64]
                                      [--rom-check] [--press FILE] [--logs DIR]

The program runs headless (live input then pumps SDL's events itself; --window for a window, which needs
SDL_VIDEODRIVER=offscreen without a display) on the virtual clock (--fast), with a status line every second of N64
time: the game's state, the first car's position, speed and steering input. A path
with STICKLEFT and STICKRIGHT (default: the synthetic cartridge's drive) runs:

- with its presses straight into the pads: the reference;
- with --press-via controller (an SDL virtual GameController), joystick (a virtual joystick without a mapping: the
  generic one), keyboard (key events), and controller:2 (the controller plugged in 2 s after the start): each must
  give the reference line for line, and name the controller and its player in the log;
- without its stick lines: its steering input must stay 0, and with --expect-turn the car must go elsewhere (the
  synthetic cartridge's car physics don't turn the car, so there the steering input is the check; a retail run
  shows both);

and in the reference the steering input is nonzero while the stick is pushed, one way for right and the other for
left. Then race2p with two virtual controllers must give its reference, the second on player 2. The lines are
compared without their audio task counts, which can differ by one between runs in a window. Exit status: the
number of failed checks; 77 (skipped) when the program has no virtual joysticks (SDL before 2.24) or no SDL2.
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synth_paths import PRESS_DIR, ROOT, read_script  # noqa: E402

KEEP = re.compile(r"rush2049pc: (game state |status |state reached:|time reached:)")
STATUS = re.compile(r"rush2049pc: status t=([0-9.]+)s .*steer (\S+)$")
SKIP = 77


WINDOW = ["--headless"]


def run(exe, rom, press, spec, via, rom_check, log, more=()):
    """spec["until"] None: run for spec["seconds"] of N64 time."""
    stop = ["--until-state", spec["until"]] if spec["until"] else []
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), *WINDOW, "--no-config", "--no-log",
           "--audio", "null", "--no-pak", "--fast", "--press-file", str(press), *stop, "--seconds", spec["seconds"],
           "--status", "1", "--set", "video.vsync=off", *more]
    if via:
        cmd += ["--press-via", via]
    if not rom_check:
        cmd.append("--no-rom-check")
    env = dict(os.environ, RUSH_STATE_EXACT="1")
    p = subprocess.run(cmd, cwd=log.parent, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    log.write_text(out, encoding="utf-8")
    return p.returncode, out, [re.sub(r" audio_tasks=\d+", "", l) for l in out.splitlines() if KEEP.match(l)]


def stick_windows(press):
    """(start, end, sign) of each STICKLEFT/STICKRIGHT line: +1 for right."""
    out = []
    for line in press.read_text().splitlines():
        f = line.split("#", 1)[0].split()
        if len(f) >= 2 and f[1].split("@")[0] in ("STICKLEFT", "STICKRIGHT"):
            t = float(f[0])
            out.append((t, t + (float(f[2]) if len(f) > 2 else 0.2), 1 if f[1].startswith("STICKRIGHT") else -1))
    return out


def steering(lines):
    return [(float(m.group(1)), float.fromhex(m.group(2))) for m in map(STATUS.match, lines) if m]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--rom-check", action="store_true", help="check the ROM's SHA-1 (a retail cartridge)")
    ap.add_argument("--press", help="a press file with '# until:' and '# seconds:' lines (default: drive)")
    ap.add_argument("--no-two", action="store_true", help="skip the two-player run (race2p)")
    ap.add_argument("--expect-turn", action="store_true", help="the car must go elsewhere without the stick")
    ap.add_argument("--window", action="store_true", help="run in a window, as a player does")
    ap.add_argument("--logs", default=str(ROOT / "build/synth/input"))
    args = ap.parse_args()
    if args.window:
        WINDOW[:] = ["--window"]
    logs = Path(args.logs).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    press = Path(args.press).resolve() if args.press else PRESS_DIR / "drive.press"
    spec = read_script(press)
    windows = stick_windows(press)
    if windows:   # only to just past the last push of the stick
        spec["until"], spec["seconds"] = None, f"{max(t1 for _, t1, _ in windows) + 3:g}"
    fails = []

    def check(ok, what):
        print(f"input_check: {'ok  ' if ok else 'FAIL'} {what}", flush=True)
        if not ok:
            fails.append(what)

    code, out, ref = run(args.exe, args.rom, press, spec, None, args.rom_check, logs / "pads.log")
    check(code == 0 and ref, f"{press.name}: the scripted run (exit {code})")
    for via in ("controller", "joystick", "keyboard", "controller:2"):
        name = via.replace(":", "_")
        code, out, lines = run(args.exe, args.rom, press, spec, via, args.rom_check, logs / f"{name}.log")
        if "needs SDL 2.24" in out or "--press-via needs SDL2" in out:
            print(f"input_check: skipped: {'no virtual joysticks' if 'SDL 2.24' in out else 'no SDL2'}")
            return SKIP
        same = code == 0 and lines == ref
        if not same and code == 0:
            first = next((i for i, (a, b) in enumerate(zip(lines, ref)) if a != b), min(len(lines), len(ref)))
            print(f"  first difference, line {first + 1}:\n  - {ref[first] if first < len(ref) else '(none)'}\n"
                  f"  + {lines[first] if first < len(lines) else '(none)'}")
        check(same, f"--press-via {via}: the same run as the scripted one (exit {code})")
        if via != "keyboard":
            check(re.search(r"controller 'rush2049pc virtual \w+ 1' \(.*\) -> player 1", out) is not None,
                  f"--press-via {via}: the log names the controller and its player")
    steer = steering(ref)
    check(bool(windows), f"{press.name} pushes the stick")
    for t0, t1, sign in windows:
        inside = [s for t, s in steer if t0 < t < t1]
        check(inside and all(s * sign > 0 for s in inside),
              f"steering input {'right' if sign > 0 else 'left'} at {t0:g}-{t1:g} s: {inside}")
    if any(s > 0 for _, _, s in windows) and any(s < 0 for _, _, s in windows):
        right = [s for t, s in steer for t0, t1, w in windows if w > 0 and t0 < t < t1]
        left = [s for t, s in steer for t0, t1, w in windows if w < 0 and t0 < t < t1]
        check(right and left and right[0] * left[0] < 0, "right and left steer opposite ways")
    flat = logs / f"{press.stem}-nosteer.press"
    flat.write_text("\n".join(l for l in press.read_text().splitlines() if "STICK" not in l.split("#", 1)[0]) + "\n")
    code, out, lines = run(args.exe, args.rom, flat, spec, None, args.rom_check, logs / "nosteer.log")
    pos = re.compile(r"car 0 at (\S+ \S+ \S+)")
    moved = [pos.search(a).group(1) != pos.search(b).group(1) for a, b in zip(lines, ref) if pos.search(a)]
    check(code == 0, f"without the stick (exit {code}); the car {'goes elsewhere' if any(moved) else 'goes the same way'}")
    if args.expect_turn:
        check(any(moved), "without the stick the car goes elsewhere")
    check(all(s == 0 for _, s in steering(lines)), "without the stick its steering input stays 0")

    if not args.no_two and not args.press:
        two = PRESS_DIR / "race2p.press"
        spec2 = read_script(two)
        spec2["until"] = spec2["until"].split(":")[0] + ":60"   # 2 s into the race: both players are in it
        code, out, ref2 = run(args.exe, args.rom, two, spec2, None, args.rom_check, logs / "race2p-pads.log")
        check(code == 0 and ref2, f"race2p: the scripted run (exit {code})")
        code, out, lines = run(args.exe, args.rom, two, spec2, "controller", args.rom_check,
                               logs / "race2p-controller.log")
        check(code == 0 and lines == ref2, "race2p, two virtual controllers: the same run as the scripted one")
        # (SDL names a controller by its mapping, which both virtual ones share)
        check(len(re.findall(r"controller 'rush2049pc virtual controller \d' \(.*\) -> player [12]\n", out)) == 2 and
              "-> player 2" in out, "race2p: the controllers are players 1 and 2")
    print(f"input_check: {len(fails)} failed" if fails else "input_check: all passed")
    return len(fails)


if __name__ == "__main__":
    sys.exit(main())
