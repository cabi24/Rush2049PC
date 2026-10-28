#!/usr/bin/env python3
"""The audio output and the retraces in real time, under a model of a Windows desktop (docs/port/AUDIO.md 7).

    python3 tools/port/realtime_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
                                         [--seconds 30] [--logs DIR] [--no-sim]

Runs the synthetic cartridge's race (tools/port/synth/race.press) on the host's clock with SDL's disk audio driver
(48 kHz, consumed in real time) when the program has SDL2, else the null output. Unless --no-sim, a run on Linux
models a Windows desktop: each present waits for the next vblank of a 60 Hz display that isn't in step with the
game's retraces (RUSH_SIM_VSYNC_HZ=59.94), and every timed wait ends at a 15.625 ms step, as winpthreads' do
(RUSH_SIM_TIMER_STEP_US). On Windows the run has Windows' own timing. It checks the lines the program writes at
exit:

    retraces    at least 59 a second, at most 12 dropped (late ones are caught up; a stall of 6 drops them)
    audio       with a device: no underrun after the first second; under 2% of the buffers played as silence or
                late, where their render came after the device needed them; none skipped by the game's renderer
                (func_80013DEC, which the PC build fixes: docs/port/AUDIO.md 7)

With --frame-rate R [--present-ms MS] it checks the high frame rate's window instead (docs/port/QA.md "Integration
soak"): the run at R frames a second, each swap held MS longer (a slow GPU) and then until the vsync, must still
show at least 5 frames a second, the game's own when no frame between fits; a skipped tick once left the window
unchanged for minutes.

With --pacing R[,R...] it runs the race once at each video.frame_rate R (display, 60, 120) under a vsync swap
(RUSH_SIM_VSYNC_HZ, --vsync-hz; the ticks are then paced by the display's vblanks, docs/port/FRAMERATE.md 2) and
checks each run's retraces and audio as above and its frames: at most 10% of the display's vblanks lost to skipped
ticks, and at least --between-ratio (0.5) of the frames drawn between the game's that the same host draws in the
same race with vsync off (the scheduling before the pacing, a run of its own at 60 first), and at least
--min-between. How many frames between a host draws depends on its GPU (or CPU renderer: llvmpipe on CI's runners
draws 80-140 in 30 s, the Pi 5 150, rocky 600-670) far more than on the pacing, which this bound is about: main
before the pacing drew 16-104 in 36 s where vsync off drew 264. On Windows --pacing is skipped (exit 77): the model
is Linux's, and a headless run there has no vsync swap to pace by. --timer-step-us 0 leaves the loop's waits precise, as Windows' high-resolution timer has
them (sched.c); the default 15625 makes them coarse, where the swap is the clock.

The modelled display (the headless model in port/ultra/loop.c, docs/port/FRAMERATE.md 4): --vsync-queue N makes it a
driver that queues frames (a swap blocks only with N queued; never: no swap blocks, WGL on NVIDIA and Intel in steady
state), --display-hz H the whole hertz SDL says (59 for 59.94 on Windows), --display-timing none,rate,vblank what the
platform's query gives (nothing; the exact refresh, as XRandR; and the vblanks' times, as Windows' compositor), each
run once per rate, and --vsync-phase F places the vblanks F of a refresh after the first retrace. --max-doubled N then
checks that at most N vblanks showed a frame twice with nothing decided for them (a vblank grid at 59 on a 59.94 Hz
display: about one a second).

Exit status 0 when all hold. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--seconds", type=float, default=30)
    ap.add_argument("--logs", default=str(ROOT / "build/synth/realtime"))
    ap.add_argument("--no-sim", action="store_true", help="the host's own timing, no Windows model")
    ap.add_argument("--frame-rate", help="check the frames shown at this video.frame_rate instead of the audio")
    ap.add_argument("--present-ms", type=float, default=0, help="each swap held this long first (RUSH_SIM_PRESENT_MS)")
    ap.add_argument("--pacing", help="frame rates to check the pacing of under a vsync swap, e.g. display,60,120")
    ap.add_argument("--vsync-hz", default="59.94", help="the modelled display's refresh (RUSH_SIM_VSYNC_HZ)")
    ap.add_argument("--timer-step-us", default="15625", help="the modelled timed waits' step (0: precise)")
    ap.add_argument("--min-between", type=int, default=0, help="--pacing: frames drawn between, at least")
    ap.add_argument("--between-ratio", type=float, default=0.5, help="--pacing: frames drawn between, at least this "
                    "fraction of the same race's with vsync off on this host (0: no reference run)")
    ap.add_argument("--vsync-queue", default="", help="the modelled driver queues this many frames (never: no swap "
                    "blocks; RUSH_SIM_VSYNC_QUEUE)")
    ap.add_argument("--vsync-phase", default="", help="the vblanks this fraction of a refresh after the first retrace")
    ap.add_argument("--display-hz", default="", help="the whole hertz SDL says (RUSH_SIM_DISPLAY_HZ)")
    ap.add_argument("--display-timing", default="", help="none, rate, vblank, or a list: what the platform gives "
                    "(RUSH_SIM_DISPLAY_TIMING), a run each")
    ap.add_argument("--max-doubled", type=int, default=-1, help="--pacing: vblanks showing a frame twice with nothing "
                    "decided for them, at most")
    args = ap.parse_args()
    if args.pacing:
        if os.name == "nt":
            print("realtime_check: --pacing models a vsync swap on Linux; a headless run on Windows has none: skipped")
            return 77
        ok = True
        args.ref_between = 0
        if args.between_ratio > 0:
            args.ref_between = reference(args, Path(args.logs).resolve() / "reference")
            if args.ref_between < 0:
                return 1
        for timing in (args.display_timing.split(",") if args.display_timing else [""]):
            for rate in args.pacing.split(","):
                drv = {"": "a vsync swap", "0": "a vsync swap", "never": "a queueing driver whose swap never blocks"}
                print(f"realtime_check: frame rate {rate}, a {args.vsync_hz} Hz display"
                      f"{' (SDL: ' + args.display_hz + ')' if args.display_hz else ''}, "
                      f"{drv.get(args.vsync_queue, 'a driver queueing ' + args.vsync_queue + ' frames')}"
                      f"{', the platform giving ' + timing if timing else ''}"
                      f"{', vblanks ' + args.vsync_phase + ' of a refresh after the retraces' if args.vsync_phase else ''}"
                      f", timed waits "
                      f"{'precise' if args.timer_step_us in ('', '0') else 'in steps of ' + args.timer_step_us + ' us'}:")
                name = f"pacing-{rate}" + (f"-{timing}" if timing else "")
                ok &= check(args, rate, Path(args.logs).resolve() / name, timing) == 0
        return 0 if ok else 1
    return check(args, args.frame_rate, Path(args.logs).resolve())


def reference(args, logs):
    """The frames drawn between in the same race at 60 with vsync off (the scheduling before the pacing): what this
    host's renderer manages, the frames-between bound's reference. -1 when the run has no frame rate line."""
    logs.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, SDL_AUDIODRIVER="disk", SDL_DISKAUDIOFILE=str(logs / "disk.raw"),
               RUSH2049_AUDIO_RATE="48000")
    if args.timer_step_us not in ("", "0"):
        env.update(RUSH_SIM_TIMER_STEP_US=args.timer_step_us)
    for key in ("RUSH_SIM_VSYNC_HZ", "RUSH_SIM_VSYNC_QUEUE", "RUSH_SIM_VSYNC_PHASE", "RUSH_SIM_DISPLAY_HZ",
                "RUSH_SIM_DISPLAY_TIMING", "RUSH_SIM_PRESENT_MS"):
        env.pop(key, None)
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--no-pak", "--press-file", str(ROOT / "tools/port/synth/race.press"),
           "--seconds", str(args.seconds), "--audio", "sdl", "--frame-rate", "60"]
    p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    (logs / "run.log").write_text(out)
    m = re.search(r"^gfx: frame rate \d+: (\d+) frames shown, (\d+) drawn between two", out, re.M)
    if not m:
        print("realtime_check: the reference run (vsync off, 60) has no frame rate line")
        return -1
    print(f"realtime_check: reference, the same race at 60 with vsync off on this host: {m[2]} frames drawn between")
    return int(m[2])


def check(args, frame_rate, logs, timing=""):
    logs.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, SDL_AUDIODRIVER="disk", SDL_DISKAUDIOFILE=str(logs / "disk.raw"),
               RUSH2049_AUDIO_RATE="48000")
    if not args.no_sim and os.name != "nt":
        # On Windows the timing is its own; the model's sleeps would be winpthreads' 15.6 ms ones there.
        env.update(RUSH_SIM_VSYNC_HZ=args.vsync_hz)
        if args.timer_step_us not in ("", "0"):
            env.update(RUSH_SIM_TIMER_STEP_US=args.timer_step_us)
        for key, value in (("RUSH_SIM_VSYNC_QUEUE", args.vsync_queue), ("RUSH_SIM_VSYNC_PHASE", args.vsync_phase),
                           ("RUSH_SIM_DISPLAY_HZ", args.display_hz), ("RUSH_SIM_DISPLAY_TIMING", timing)):
            if value:
                env[key] = value
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--no-pak", "--press-file", str(ROOT / "tools/port/synth/race.press"),
           "--seconds", str(args.seconds), "--audio", "sdl"]
    if frame_rate:
        cmd += ["--frame-rate", frame_rate]
        if args.present_ms > 0:
            env["RUSH_SIM_PRESENT_MS"] = str(args.present_ms)
    p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    (logs / "run.log").write_text(out)
    ok = True

    if args.pacing:
        m = re.search(r"^gfx: frame rate \d+: (\d+) frames shown, (\d+) drawn between two", out, re.M)
        k = re.search(r"^rush2049pc: frame rate \d+: (\d+) frames not shown", out, re.M)
        v = re.search(r"^rush2049pc: paced by the display's vblanks \(([\d.]+) Hz", out, re.M)
        if not m or not k or not v:
            print(f"realtime_check: no frame rate lines{'' if v else ' (not paced by the vblanks)'}")
            return 1
        between, skipped = int(m[2]), int(k[1])
        vblanks = args.seconds * float(args.vsync_hz)
        need = max(args.min_between, int(args.between_ratio * getattr(args, "ref_between", 0) + 0.5))
        good = skipped <= 0.10 * vblanks and between >= need
        ok &= good
        print(f"realtime_check: {vblanks:.0f} vblanks ({v[1]} Hz measured), {skipped} lost to skipped ticks (at most 10%), "
              f"{between} frames drawn between (at least {need}: {args.between_ratio:g} of vsync off's"
              f"{', and ' + str(args.min_between) if args.min_between else ''}): {'ok' if good else 'FAIL'}")
        if args.max_doubled >= 0:
            d = re.search(r"^rush2049pc: simulated vsync at [\d.]+ Hz, [^:]*: (\d+) vblanks, (\d+) showed no new frame, "
                          r"(\d+) of them with none decided", out, re.M)
            if not d:
                print("realtime_check: no simulated vsync line")
                return 1
            doubled = int(d[3])
            good = doubled <= args.max_doubled
            ok &= good
            print(f"realtime_check: of {d[1]} vblanks, {doubled} showed a frame twice with nothing decided for them "
                  f"(at most {args.max_doubled}): {'ok' if good else 'FAIL'}")
    elif frame_rate:
        m = re.search(r"^gfx: frame rate \d+: (\d+) frames shown", out, re.M)
        if not m or p.returncode != 0:
            print(f"realtime_check: {'no frame rate line' if not m else 'exit ' + str(p.returncode)}")
            return 1
        shown = int(m[1]) / args.seconds
        print(f"realtime_check: frame rate {frame_rate}, swaps held {args.present_ms:g} ms: {shown:.1f} frames "
              f"shown a second (at least 5): {'ok' if shown >= 5 else 'FAIL'}")
        return 0 if shown >= 5 else 1

    m = re.search(r"rush2049pc: (\d+) retraces in ([\d.]+) s; (\d+) came a whole retrace late and were caught up, "
                  r"(\d+) dropped", out)
    if not m:
        print("realtime_check: no retrace line")
        return 1
    vi, secs, caught, dropped = int(m[1]), float(m[2]), int(m[3]), int(m[4])
    rate = vi / secs if secs else 0
    good = rate >= 59.0 and dropped <= 12
    ok &= good
    print(f"realtime_check: retraces {rate:.2f} a second, {caught} caught up, {dropped} dropped: "
          f"{'ok' if good else 'FAIL'}")

    m = re.search(r"^audio: (\w+) at (\d+) Hz \(([^)]*)\): ([\d.]+) s played; (\d+) underruns .*?; (\d+) buffers "
                  r"pushed before their render: (\d+) rendered in time, (\d+) played late, (\d+) as silence", out, re.M)
    if not m:
        print("realtime_check: no audio line")
        return 1
    backend, played, underruns, waited = m[1], float(m[4]), int(m[5]), int(m[6])
    late, silent = int(m[8]), int(m[9])
    buffers = played * 22047 / 192
    s = re.search(r"(\d+) buffers the renderer skipped", out)
    skipped = int(s[1]) if s else -1
    first = [float(t) for t in re.findall(r"^audio: underrun \d+, ([\d.]+) s in", out, re.M)]
    after_start = sum(1 for t in first if t > 1.0) if underruns <= len(first) else underruns
    if backend == "sdl":
        good = after_start == 0 and late + silent < 0.02 * buffers and skipped == 0
        ok &= good
        print(f"realtime_check: audio through {backend} ({m[3]}): {underruns} underruns ({after_start} after the first "
              f"second); of {buffers:.0f} buffers {waited} pushed before their render, {late} played late, {silent} as "
              f"silence; {skipped} skipped by the renderer: {'ok' if good else 'FAIL'}")
    else:
        print(f"realtime_check: audio through {backend}: no device, not checked")
    return 0 if ok and p.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
