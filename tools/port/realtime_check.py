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
    args = ap.parse_args()
    logs = Path(args.logs).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, SDL_AUDIODRIVER="disk", SDL_DISKAUDIOFILE=str(logs / "disk.raw"),
               RUSH2049_AUDIO_RATE="48000")
    if not args.no_sim and os.name != "nt":
        # On Windows the timing is its own; the model's sleeps would be winpthreads' 15.6 ms ones there.
        env.update(RUSH_SIM_VSYNC_HZ="59.94", RUSH_SIM_TIMER_STEP_US="15625")
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--no-pak", "--press-file", str(ROOT / "tools/port/synth/race.press"),
           "--seconds", str(args.seconds), "--audio", "sdl"]
    if args.frame_rate:
        cmd += ["--frame-rate", args.frame_rate]
        if args.present_ms > 0:
            env["RUSH_SIM_PRESENT_MS"] = str(args.present_ms)
    p = subprocess.run(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
    out = p.stdout.decode(errors="replace")
    (logs / "run.log").write_text(out)
    ok = True

    if args.frame_rate:
        m = re.search(r"^gfx: frame rate \d+: (\d+) frames shown", out, re.M)
        if not m or p.returncode != 0:
            print(f"realtime_check: {'no frame rate line' if not m else 'exit ' + str(p.returncode)}")
            return 1
        shown = int(m[1]) / args.seconds
        print(f"realtime_check: frame rate {args.frame_rate}, swaps held {args.present_ms:g} ms: {shown:.1f} frames "
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
