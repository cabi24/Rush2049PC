#!/usr/bin/env python3
"""The sound's voices along a press script's path, at each [audio] voices size or [audio] engines mode
(docs/port/AUDIO.md 10).

    python3 tools/port/voices_check.py --exe build/pc/rush2049pc --rom build/pc/synth/rush2049_synth_snd.z64 \\
        [--logs DIR] voices PATH [--sizes original,64,96]
    python3 tools/port/voices_check.py --exe ... --rom ... engines PATH --mode nearest4 [--voices 64] [--players 1]
    python3 tools/port/voices_check.py --summary LOG ...          # the counts of logs already made

Each run is the path (tools/port/synth/PATH.press, as synth_paths.py runs it, on the virtual clock) with
--log-voices: every --every retraces a line of the voices the mixer mixed and the starts refused since the line
before, and an end line over the whole run (port/main.c). The cartridge should be the synthetic one with sounds
(synth_rom.py --sounds): on the default one nothing plays.

voices: one run per size. Passes if every size reached the path's state with the same trace (the game's state log:
the setting changes only the sound); no subframe at original mixed more than 32 voices; and each size above the one
before it mixed more voices (the most in a subframe, and the mean) with fewer of its starts refused (the tags the
sound manager held back for its budget a pass, the starts MusyX refused, the voices it stole, the effects it didn't
start, the starts that found no entry), unless the one before refused nothing: then it had all the voices it asked
for, and the next mixes at least as many.

engines: one run at --voices. Passes if no line has more drones' engines on than the mode allows (nearest N: N for
each player; all: every drone), and some line has as many as the mode asks for (or, for nearest N, at least one);
original: no line names one.

Prints each run's counts. Exit status: 0 if every check passed. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_paths  # noqa: E402

_END = re.compile(r"voices at the end: mixed ([\d.]+) voices a subframe of (\d+) \((\d+) subframes\); musyx refused "
                  r"(\d+), stole (\d+); effects refused (\d+); over budget ([\d.]+) a pass \((\d+) passes\); no "
                  r"emitter (\d+), no message (\d+), no entry (\d+); .*messages most (\d+); most voices a subframe (\d+)"
                  r"(?:; most drones' engine layers playing (\d+))?")
_LINE = re.compile(r"^rush2049pc: voices vi=(\d+): .*; engines on(.*)$")
_STATE = re.compile(r"^rush2049pc: (game state .* at vi \d+|state reached:).*$")


def counts(text):
    m = None
    for line in text.splitlines():
        m = _END.search(line) or m
    if not m:
        return None
    g = m.groups()
    return {"mean": float(g[0]), "slots": int(g[1]), "subframes": int(g[2]), "refused": int(g[3]),
            "stolen": int(g[4]), "fx_refused": int(g[5]), "over_budget": float(g[6]), "passes": int(g[7]),
            "no_emitter": int(g[8]), "no_message": int(g[9]), "no_entry": int(g[10]), "msg_most": int(g[11]),
            "most": int(g[12]), "engines_most": int(g[13] or 0)}


def trace(text):
    """The state log without the times: what the game did, which the voices must not change."""
    return [re.sub(r" t=\S+", "", l) for l in text.splitlines() if _STATE.match(l)]


def engines_on(text):
    """Per voices line, the drones whose engines were on (a list), or None where it names none or says original."""
    out = []
    for line in text.splitlines():
        m = _LINE.match(line)
        if m:
            rest = m.group(2).strip()
            out.append([int(x) for x in rest.split()] if re.fullmatch(r"[\d ]+", rest) else None)
    return out


def run(exe, rom, path, logs, tag, sets, every):
    spec = synth_paths.read_script(synth_paths.PRESS_DIR / f"{path}.press")
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--no-rom-check", "--headless", "--no-config",
           "--audio", "null", "--no-pak", "--fast", "--press-file", str(synth_paths.PRESS_DIR / f"{path}.press"),
           "--until-state", spec["until"], "--seconds", spec["seconds"], "--log-voices", str(every)]
    cmd += spec["args"]
    for s in sets:
        cmd += ["--set", s]
    env = dict(os.environ, RUSH_STATE_EXACT="1", **spec["env"])
    t0 = time.time()
    p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
    text = p.stdout.decode(errors="replace")
    (Path(logs) / f"{tag}.log").write_text(text, encoding="utf-8")
    return text, p.returncode, time.time() - t0


def show(name, c):
    return (f"{name}: {c['mean']:.2f} voices a subframe (most {c['most']} of {c['slots']}); over budget "
            f"{c['over_budget']:.2f} a pass, musyx refused {c['refused']}, stole {c['stolen']}, effects refused "
            f"{c['fx_refused']}, no entry {c['no_entry']}; no emitter {c['no_emitter']}, no message "
            f"{c['no_message']}, messages most "
            f"{c['msg_most']}; the most drones' engine layers playing at once {c['engines_most']}")


def check_voices(args):
    sizes = args.sizes.split(",")
    results, traces, ok = [], [], True
    for size in sizes:
        text, code, took = run(args.exe, args.rom, args.path, args.logs, f"{args.path}.voices-{size}",
                               [f"audio.voices={size}"], args.every)
        c = counts(text)
        reached = any("state reached:" in l for l in text.splitlines())
        if code != 0 or not reached or not c:
            print(f"voices_check: {args.path} at {size}: FAIL (exit {code}{', not reached' if not reached else ''}"
                  f"{', no voices line' if not c else ''})")
            return 1
        print(f"voices_check: {args.path} {show(size, c)} ({took:.1f} s)")
        results.append((size, c))
        traces.append(trace(text))
    for (size, c), t in zip(results, traces):
        if t != traces[0]:
            print(f"voices_check: {args.path}: FAIL: the trace at {size} differs from {sizes[0]}'s")
            ok = False
        if size == "original" and c["most"] > 32:
            print(f"voices_check: {args.path}: FAIL: {c['most']} voices in a subframe at original")
            ok = False
    for (a, ca), (b, cb) in zip(results, results[1:]):
        refused = lambda c: (c["over_budget"] * c["passes"] + c["refused"] + c["stolen"] + c["fx_refused"] +
                             c["no_entry"])
        # A size that refused nothing had every voice it asked for: the next one can only mix as many
        if refused(ca) == 0:
            more = cb["most"] >= ca["most"]
        else:
            more = cb["most"] > ca["most"] and cb["mean"] > ca["mean"]
        fewer = refused(cb) < refused(ca) or refused(ca) == 0 == refused(cb)
        if not more:
            print(f"voices_check: {args.path}: FAIL: {b} mixed no more voices than {a}")
            ok = False
        if not fewer:
            print(f"voices_check: {args.path}: FAIL: {b} refused no fewer starts than {a} ({refused(cb):.0f} "
                  f"against {refused(ca):.0f})")
            ok = False
    print(f"voices_check: {args.path}: {'ok' if ok else 'FAIL'}")
    return 0 if ok else 1


def check_engines(args):
    text, code, took = run(args.exe, args.rom, args.path, args.logs, f"{args.path}.engines-{args.mode}",
                           [f"audio.voices={args.voices}", f"audio.engines={args.mode}"], args.every)
    lines = engines_on(text)
    reached = any("state reached:" in l for l in text.splitlines())
    if code != 0 or not reached or not lines:
        print(f"voices_check: {args.path} engines {args.mode}: FAIL (exit {code}, {len(lines)} lines)")
        return 1
    named = [on for on in lines if on]
    most = max((len(on) for on in named), default=0)
    ok = True
    if args.mode == "original":
        ok = not named
        why = "lines name drones" if named else ""
    elif args.mode == "all":
        drones = args.drones
        ok = most == drones and all(len(on) <= drones for on in named)
        why = f"the most on was {most}, of {drones} drones"
    else:
        n = int(args.mode[len("nearest"):]) * args.players
        ok = most >= 1 and all(len(on) <= n for on in named)
        why = f"the most on was {most}, the mode allows {n}"
    c = counts(text)
    print(f"voices_check: {args.path} engines {args.mode}: {len(named)} of {len(lines)} lines name drones, the most "
          f"{most} at once ({took:.1f} s); {show('voices', c) if c else ''}")
    print(f"voices_check: {args.path} engines {args.mode}: {'ok' if ok else 'FAIL: ' + why}")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth_snd.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/synth/voices"))
    ap.add_argument("--every", type=int, default=30, help="retraces between voices lines")
    ap.add_argument("--summary", nargs="+", metavar="LOG", help="print the counts of these logs and exit")
    sub = ap.add_subparsers(dest="what")
    v = sub.add_parser("voices")
    v.add_argument("path")
    v.add_argument("--sizes", default="original,64")
    e = sub.add_parser("engines")
    e.add_argument("path")
    e.add_argument("--mode", required=True)
    e.add_argument("--voices", default="64")
    e.add_argument("--players", type=int, default=1)
    e.add_argument("--drones", type=int, default=11)
    args = ap.parse_args()
    if args.summary:
        for log in args.summary:
            c = counts(Path(log).read_text(errors="replace"))
            print(show(log, c) if c else f"{log}: no voices line")
        return 0
    Path(args.logs).mkdir(parents=True, exist_ok=True)
    if args.what == "voices":
        return check_voices(args)
    if args.what == "engines":
        return check_engines(args)
    ap.error("voices or engines")
    return 2


if __name__ == "__main__":
    sys.exit(main())
