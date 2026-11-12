#!/usr/bin/env python3
"""Paced mode against --fast: the same game, in real time (docs/port/ULTRA.md 10, NETPLAY.md 5 phase 0.1-0.2).

    python3 tools/port/paced_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64 \\
        [--path race | --script PRESS --until MASK:N] [--seconds 30] [--audio null|sdl] [--speed 1] \\
        [--model NAME=VALUE ...] [--set SECTION.KEY=VALUE ...] [--frame-rate R] [--load N] [--realtime] [--logs DIR]

Runs one press script twice: on the virtual clock as fast as the host can (--fast, the null output: the reference),
and in paced mode (--paced: the same virtual clock, each move of it at its wall time). Both with RUSH_STATE_EXACT and a
status line a second of game time, and --dump-audio. Then it checks:

    trace       paced mode's states and status lines are --fast's, to the bit (synth_compare.py's trace: every state
                change at its retrace, the first car's position and speed as exact floats)
    sound       the WAV each wrote, the game's pushed sound, is the same (but for its end: what the game's audio thread
                pushes after the main loop's end, answering events sent before it, reaches the file before it is closed
                or not, as with --fast alone, docs/port/AUDIO.md 10.6; with a device the close takes longer, and up to
                --max-tail subframes more arrive)
    pace        the exit line `paced: N retraces in W s of wall time, R a second (P without the time held or
                re-based)`: P within --min-rate and --max-rate (60 a second, times RUSH_PACE_SPEED's --speed), and at
                most --max-rebases re-based lags (a slow host, a stall); R too, unless the run is meant to be slowed
                (--slowed: a model of a slow host, where the game slows in wall time and only the trace must hold),
                its lower bound with the allowed lags' time taken out (each up to --rebase-ms: a lag re-bases past two
                retraces, so one alone would put a 36 s run under 59.95 and allow none in effect). The lags the run
                says were the host's work (`paced: of the re-based lags, K the host's work (S s ...)`: the game, the
                renderer, the start; up to --max-work-rebases of them) are the host's, not the pace's, and aside:
                not counted, their time out of R's bound too. A software renderer's first draw with a new shader
                (Mesa's llvmpipe with no shader cache of its own, as on the Windows runner: 40-50 ms) is one.
    tap         with --audio sdl (SDL's disk driver, which consumes in real time, unless SDL_AUDIODRIVER is set): the
                tap's line at exit, the FIFO's level and the nudge of its converter, and no dry FIFO after the first
                second (`audio: tap: the FIFO ran dry ... T s in`). The level only when the device keeps time: a
                converter nudged over --device-off-pct on average follows a device that doesn't (the disk driver on
                Windows sleeps 10 ms a pull and Sleep rounds up, about 5% slow; a sound card's crystal is within
                0.01%), whose FIFO settles where the nudge's controller holds that rate, and drops what the nudge's
                reach can't follow (so the drops are judged only then too). A dry FIFO after a lag of the host's
                work is allowed (the game made no sound while the host was busy): no more of them than those lags.

--hash N[,W] gives both runs --log-hash and compares their state hashes too: the game's memory every Nth retrace
(docs/port/LOCKSTEP.md 5).

--model NAME=VALUE sets an environment variable for both runs (the timing models: RUSH_SIM_PRESENT_MS,
RUSH_SIM_TIMER_STEP_US, RUSH_SIM_VSYNC_HZ, RUSH_SIM_GFX_MS); --env one for the paced run alone (RUSH_SIM_HOLD=VI:MS:
the game held at retrace VI for MS of wall time, as the pause key holds it); --set a setting for the paced run
(video.frame_rate: render-only, which the reference doesn't need). --load N runs N busy processes beside the paced run
(a loaded host). --stop SECONDS@AT stops the paced run whole (SIGSTOP, as a debugger does; POSIX) at AT s of wall time
for SECONDS. With --held S (a hold or a stop of S seconds in all) the time held or re-based must cover S, the rest be
at the pace, and nothing come in a burst after it (no lag past two retraces caught up): the course is still --fast's,
which a hold doesn't change in paced mode (with --fast it moves the retraces).
--realtime also runs the script on the host's clock, for contrast: its trace is reported, not judged (it differs
from --fast's on most hosts, docs/port/NETPLAY.md E7). Exit status 0 when all hold, 77 when --audio sdl and the
program has no SDL2 audio. Stdlib only, Python 3.9+.
"""
import argparse
import multiprocessing
import os
import re
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import hash_compare  # noqa: E402
import synth_compare  # noqa: E402
import synth_paths  # noqa: E402


def busy():
    while True:
        pass


def wav_data(path):
    """The data chunk's bytes of a WAV the program wrote, or None."""
    try:
        b = Path(path).read_bytes()
    except OSError:
        return None
    i = 12
    while i + 8 <= len(b):
        tag, size = b[i:i + 4], struct.unpack("<I", b[i + 4:i + 8])[0]
        if tag == b"data":
            return b[i + 8:i + 8 + size]
        i += 8 + size + (size & 1)
    return None


def run(args, mode, logs, env_extra, extra, stop=None):
    logs.mkdir(parents=True, exist_ok=True)
    for a in args.tail + ["run.wav"]:   # what the run writes starts afresh: a screenshot, a controller pak, the sound
        a = a.split("=", 1)[-1]
        if a.endswith((".png", ".mpk", ".wav")) and (logs / a).exists():
            (logs / a).unlink()
    env = dict(os.environ)
    env.update(synth_paths.data_env(logs))
    env.update(args.path_env)
    env.update(env_extra)
    env["RUSH_STATE_EXACT"] = "1"
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--no-pak", "--press-file", str(Path(args.script).resolve()), "--seconds", str(args.seconds),
           "--status", "1", "--dump-audio", "run.wav"] + args.tail + extra + \
          (["--log-hash", args.hash] if args.hash else [])
    if args.until:
        cmd += ["--until-state", args.until]
    t0 = time.time()
    with open(logs / "run.log", "wb") as log:
        p = subprocess.Popen(cmd, cwd=logs, env=env, stdout=log, stderr=subprocess.STDOUT)
        if stop:   # a debugger's stop: the process stopped whole, then let go
            secs, at = stop
            try:
                p.wait(timeout=at)
            except subprocess.TimeoutExpired:
                os.kill(p.pid, signal.SIGSTOP)
                time.sleep(secs)
                os.kill(p.pid, signal.SIGCONT)
                print(f"paced_check: {mode}: stopped {secs:g} s at {at:g} s of wall time (SIGSTOP)")
        p.wait(timeout=args.timeout)
    took = time.time() - t0
    out = (logs / "run.log").read_text(encoding="utf-8", errors="replace")
    print(f"paced_check: {mode}: exit {p.returncode}, {took:.1f} s of wall time")
    return out, p.returncode, wav_data(logs / "run.wav")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--path", default="race", help="a synthetic path (tools/port/synth/NAME.press), its expect lines")
    ap.add_argument("--script", help="a press script instead of --path")
    ap.add_argument("--until", help="--until-state (default: the path's)")
    ap.add_argument("--seconds", type=float, default=0, help="--seconds (default: the path's, or 30)")
    ap.add_argument("--audio", default="null", help="the paced run's backend: null or sdl (the tap)")
    ap.add_argument("--speed", type=float, default=1.0, help="RUSH_PACE_SPEED: N64 time this many times the wall's")
    ap.add_argument("--model", action="append", default=[], metavar="NAME=VALUE", help="for both runs")
    ap.add_argument("--set", action="append", default=[], metavar="SECTION.KEY=VALUE", help="for the paced run")
    ap.add_argument("--env", action="append", default=[], metavar="NAME=VALUE", help="for the paced run (RUSH_SIM_HOLD)")
    ap.add_argument("--stop", metavar="SECONDS@AT", help="stop the paced run SECONDS at AT s of wall time (SIGSTOP)")
    ap.add_argument("--held", type=float, default=0, help="the paced run is held or stopped this many seconds in all: "
                    "the time held or re-based must cover it, and the rest is paced")
    ap.add_argument("--frame-rate", help="the paced run's video.frame_rate")
    ap.add_argument("--load", type=int, default=0, help="busy processes beside the paced run")
    ap.add_argument("--realtime", action="store_true", help="also the host's clock, for contrast (not judged)")
    ap.add_argument("--slowed", action="store_true", help="a model of a slow host: the wall rate isn't judged")
    ap.add_argument("--min-rate", type=float, default=59.95)
    ap.add_argument("--max-rate", type=float, default=60.05)
    ap.add_argument("--max-rebases", type=int, default=2)
    ap.add_argument("--max-work-rebases", type=int, default=8, help="re-based lags of the host's work set aside "
                    "(not counted against --max-rebases), at most")
    ap.add_argument("--rebase-ms", type=float, default=50, help="the time each allowed re-based lag may take from "
                    "the whole run's rate (three retraces)")
    ap.add_argument("--device-off-pct", type=float, default=3, help="the tap's FIFO level judged only when its "
                    "converter's mean nudge is within this (a device that keeps time)")
    ap.add_argument("--fifo-slack-ms", type=float, default=15, help="the tap's mean level within this of its target")
    ap.add_argument("--max-tail", type=int, default=32, help="subframes one WAV may have past the other's end")
    ap.add_argument("--hash", metavar="N[,W]", help="--log-hash for both runs: the state hash compared at every Nth "
                    "retrace too (the full hash, the heap and own storage among it, every W)")
    ap.add_argument("--logs", default=str(ROOT / "build/synth/paced"))
    ap.add_argument("--timeout", type=float, default=1200)
    args = ap.parse_args()

    args.path_env, args.tail = {}, []
    if not args.script:
        press = synth_paths.PRESS_DIR / f"{args.path}.press"
        spec = synth_paths.read_script(press)
        args.script = str(press)
        args.until = args.until or spec["until"]
        args.seconds = args.seconds or float(spec["seconds"])
        args.path_env, args.tail = spec["env"], spec["args"]
        expect = spec["expect"]
    else:
        expect = synth_compare.script_expect(args.script)
    args.seconds = args.seconds or 30
    models = dict(m.split("=", 1) for m in args.model)
    logs = Path(args.logs).resolve()
    ok = True

    ref, ref_code, ref_wav = run(args, "--fast, null output (the reference)", logs / "fast", models,
                                 ["--fast", "--audio", "null"])
    paced_env = dict(models)
    paced_env.update(dict(e.split("=", 1) for e in args.env))
    if args.speed != 1:
        paced_env["RUSH_PACE_SPEED"] = f"{args.speed:g}"
    extra = ["--paced", "--audio", args.audio] + [w for s in args.set for w in ("--set", s)]
    if args.frame_rate:
        extra += ["--frame-rate", args.frame_rate]
    if args.audio == "sdl":
        paced_env.setdefault("SDL_AUDIODRIVER", os.environ.get("SDL_AUDIODRIVER", "disk"))
        paced_env.setdefault("SDL_DISKAUDIOFILE", str(logs / "paced" / "disk.raw"))
        paced_env.setdefault("RUSH2049_AUDIO_RATE", "48000")
        paced_env.setdefault("RUSH2049_AUDIO_STATS", "10")
    hogs = [multiprocessing.Process(target=busy, daemon=True) for _ in range(args.load)]
    for h in hogs:
        h.start()
    try:
        stop = tuple(float(x) for x in args.stop.split("@")) if args.stop else None
        if stop and os.name == "nt":
            print("paced_check: --stop stops a process with SIGSTOP, which Windows hasn't: skipped")
            return 77
        out, code, wav = run(args, f"paced{', ' + str(args.load) + ' busy processes beside it' if hogs else ''}",
                             logs / "paced", paced_env, extra, stop)
    finally:
        for h in hogs:
            h.terminate()
    no_device = args.audio == "sdl" and "audio: paced: the queue" not in out   # no SDL2 audio, or it didn't open

    # the trace
    ta = synth_compare.trace(ref) + ((("exit", None, "", str(ref_code)),) if ref_code else ())
    tb = synth_compare.trace(out) + ((("exit", None, "", str(code)),) if code else ())
    d = synth_compare.first_difference(ta, tb)
    reached = synth_compare.reaches(tb, expect)
    good = d is None and reached and len(ta) > 2
    ok &= good
    if d is None:
        print(f"paced_check: the trace: the same as --fast's, {len(ta)} events"
              f"{'' if reached else ', but not the end the script expects (' + expect + ')'}: "
              f"{'ok' if good else 'FAIL'}")
    else:
        i, x, y = d
        print(f"paced_check: the trace: DIFFERS from --fast's at event {i} of {len(ta)}\n"
              f"    {synth_compare.show(x)}\n    {synth_compare.show(y)}")

    # the state hash (--hash): the game's memory, retrace by retrace (docs/port/LOCKSTEP.md 5)
    if args.hash:
        ha, hb = hash_compare.hashes(ref), hash_compare.hashes(out)
        first, regions, common, only = hash_compare.compare(ha, hb)
        good = bool(common) and first is None and not only
        ok &= good
        full = sum(1 for n in common if "heap" in ha[n])
        print(f"paced_check: the state hash: " + (f"the same at all {len(common)} retraces hashed ({full} with the "
              f"full hash)" if first is None else f"DIFFERS from --fast's at retrace {first} ({', '.join(regions)})") +
              (f"; {len(only)} retraces hashed by one run only" if only else "") + f": {'ok' if good else 'FAIL'}")

    # the sound the game pushed
    if ref_wav is None or wav is None:
        print("paced_check: the sound: no WAV written: FAIL")
        ok = False
    else:
        n = min(len(ref_wav), len(wav))
        tail = abs(len(ref_wav) - len(wav)) // (192 * 4)
        same = ref_wav[:n] == wav[:n] and tail <= args.max_tail and n > 0
        ok &= same
        print(f"paced_check: the sound: {len(wav)} bytes against --fast's {len(ref_wav)}, "
              f"{'the same' if ref_wav[:n] == wav[:n] else 'DIFFERENT'} in the {n} both have; {tail} subframes more in "
              f"one (at most {args.max_tail}: what the game's audio thread pushes after the loop's end, before the "
              f"file is closed): {'ok' if same else 'FAIL'}")

    # the pace
    m = re.search(r"^rush2049pc: paced: (\d+) retraces in ([\d.]+) s of wall time, ([\d.]+) a second \(([\d.]+) "
                  r"without the ([\d.]+) s held or re-based\); (\d+) holds \(([\d.]+) s\), (\d+) re-based after a lag "
                  r"\(([\d.]+) s, the longest ([\d.]+) ms\), (\d+) retraces caught up a lag of over \d+ ms \(the most "
                  r"([\d.]+) ms\)", out, re.M)
    if not m:
        print("paced_check: no paced line: FAIL")
        return 1
    rate, paced, rebases, gaps = float(m[3]), float(m[4]), int(m[8]), float(m[5])
    lo, hi = args.min_rate * args.speed, args.max_rate * args.speed
    if args.held:   # held or stopped: that time held (hold()) or re-based (a lag), the rest at the pace, and no burst
        good = gaps >= args.held * 0.95 and gaps <= args.held + 1 and lo <= paced <= hi and float(m[12]) <= 34
        print(f"paced_check: held {args.held:g} s: {gaps:.3f} s held or re-based ({m[6]} holds, {rebases} re-based, "
              f"the longest {m[10]} ms), no burst after it (the most caught up {m[12]} ms, at most two retraces): "
              f"{'ok' if good else 'FAIL'}")
        ok &= good
    w = re.search(r"^rush2049pc: paced: of the re-based lags, (\d+) the host's work \(([\d.]+) s", out, re.M)
    work_n, work_s = (int(w[1]), float(w[2])) if w else (0, 0.0)
    aside = min(work_n, args.max_work_rebases)   # the host's, not the pace's: not counted, their time out of the bound
    aside_s = work_s * aside / work_n if work_n else 0.0
    n = int(m[1])   # the whole run's rate: the allowed lags' time out of its bound
    rate_lo = n / (n / lo + aside_s + args.max_rebases * args.rebase_ms / 1000) if lo > 0 and n else lo
    good = args.slowed or bool(args.held) or (lo <= paced <= hi and rate_lo <= rate <= hi and
                                              rebases - aside <= args.max_rebases)
    ok &= good
    print(f"paced_check: the pace: {m[1]} retraces in {m[2]} s of wall time, {rate:.3f} a second "
          f"({'' if args.slowed or args.held else f'{rate_lo:.3f}-{hi:g}, '}the allowed lags out), {paced:.3f} without "
          f"the {m[5]} s held or re-based ({'not judged: the run is slowed by its model' if args.slowed else f'{lo:g}-{hi:g}'}"
          f"); {rebases} re-based (the longest {m[10]} ms; {work_n} the host's work"
          f"{'' if args.slowed else f', up to {args.max_work_rebases} of those aside'}"
          f"{'' if args.slowed else f', at most {args.max_rebases} others'}), "
          f"{m[11]} retraces caught up (the most {m[12]} ms): "
          f"{'ok' if good else 'FAIL'}")
    if args.slowed and float(m[2]) > 0:
        n64 = int(m[1]) / 60.0 / args.speed
        print(f"paced_check: the run took {float(m[2]):.1f} s of wall time for {n64:.1f} s of game time: "
              f"{float(m[2]) / n64:.2f} times as long")

    # the tap
    if no_device:
        print("paced_check: the tap: SDL's audio isn't in this program, or didn't open: not checked")
    elif args.audio == "sdl" and args.speed != 1:
        print("paced_check: the tap: not judged at RUSH_PACE_SPEED's speed (the game makes its sound that much faster "
              "than a device plays it)")
    elif args.audio == "sdl":
        t = re.search(r"^audio: tap to (\w+) \(([^)]*)\) at (\d+) Hz, pulls of (\d+) frames: .*?; the FIFO "
                      r"([\d.]+)/([\d.]+)/([\d.]+) ms \(least/mean/most\) against ([\d.]+) ms; the ratio nudged "
                      r"([-+\d.]+)% on average \(([-+\d.]+)% to ([-+\d.]+)%\), ([-+\d.]+)% now; (\d+) underruns "
                      r"\(([\d.]+) ms of silence\), ([\d.]+) ms filling, ([\d.]+) ms idle, (\d+) frames dropped", out,
                      re.M)
        if not t:
            print("paced_check: no tap line: FAIL")
            return 1
        dry = [float(x) for x in re.findall(r"^audio: tap: the FIFO ran dry \(underrun \d+, ([\d.]+) s in\)", out,
                                             re.M)]
        late = sum(1 for x in dry if x > 1.0) if int(t[13]) <= len(dry) else int(t[13])
        mean, target = float(t[6]), float(t[8])
        keeps_time = abs(float(t[9])) <= args.device_off_pct
        good = late <= work_n and (not keeps_time or (int(t[17]) == 0 and abs(mean - target) <= args.fifo_slack_ms))
        ok &= good
        level = (f"the mean within {args.fifo_slack_ms:g}" if keeps_time else
                 f"the level and the drops not judged: the device runs over {args.device_off_pct:g}% off real time")
        print(f"paced_check: the tap to {t[1]} ({t[2]}) at {t[3]} Hz, pulls of {t[4]}: the FIFO {t[5]}/{t[6]}/{t[7]} "
              f"ms (least/mean/most) against {t[8]} ms ({level}); its converter nudged "
              f"{t[9]}% on average ({t[10]}% to {t[11]}%); {t[13]} underruns ({late} after the first second, at most "
              f"{work_n}: the lags of the host's work; "
              f"{t[14]} ms of silence), {t[17]} frames dropped: {'ok' if good else 'FAIL'}")

    if args.realtime:   # beside the same busy processes
        hogs = [multiprocessing.Process(target=busy, daemon=True) for _ in range(args.load)]
        for h in hogs:
            h.start()
        try:
            rt, rt_code, _ = run(args, "the host's clock (for contrast)", logs / "realtime", models,
                                 ["--audio", "null"])
        finally:
            for h in hogs:
                h.terminate()
        tr = synth_compare.trace(rt)
        dr = synth_compare.first_difference(ta, tr)
        print("paced_check: on the host's clock the trace is " + ("the same as --fast's" if dr is None else
              f"not --fast's from event {dr[0]}: {synth_compare.show(dr[2])}") + " (not judged)")

    print(f"paced_check: {'ok' if ok else 'FAIL'}{' (the tap skipped)' if ok and no_device else ''}")
    return 1 if not ok else 77 if no_device else 0


if __name__ == "__main__":
    sys.exit(main())
