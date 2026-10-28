#!/usr/bin/env python3
"""A slow disk's pak writes cost the main loop no retraces (docs/port/ULTRA.md "Controller pak").

    python3 tools/port/pakwrite_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
                                         [--logs DIR] [--delay-ms 200] [--no-control]

Runs the synthetic cartridge's pak path (tools/port/synth/pak.press: a race with a controller pak) on the host's
clock, with each write of the pak held --delay-ms first (RUSH_SIM_PAK_WRITE_MS, a slow disk) and the pak written
at every flush, once a second, changed or not (RUSH_SIM_PAK_REWRITE), so the race has a write each second. It checks:

    the writes      on the writer's thread, during the race, each at least the delay
    the retraces    none dropped, at most 6 caught up (the line the program writes at exit)
    the pak         a good controller pak at exit (pc_pak_image_check's rules, checked here by its ID checksums)
    the end         SIGTERM during a held write (3 s): the program waits for it, and the file is written
    a fault         the blank cartridge's boot (--no-rom), which faults at once, during a held write (1.5 s): the
                    fault handler waits for it, and the file is written (when the fault came after the first
                    hand-over; before it there is nothing to wait for, and the step says so)

and, unless --no-control, the same race with the writes on the main loop as before (RUSH_SIM_PAK_WRITE_SYNC=1),
which must drop retraces: the check sees the stall it is there to catch. Exit status 0 when all hold. Stdlib only.
"""
import argparse
import os
import re
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def run(args, logs, name, env_extra, until="0x400000:600", seconds="90", term_after=None, no_rom=False):
    pak = logs / f"{name}.mpk"
    if pak.exists():
        pak.unlink()
    env = dict(os.environ, **env_extra)
    cmd = [str(Path(args.exe).resolve()), "--headless", "--no-config", "--audio", "null", "--log", f"{name}.game.log",
           "-v", "--pak", pak.name, "--seconds", seconds]
    if no_rom:
        cmd += ["--no-rom"]
    else:
        cmd += ["--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--until-state", until,
                "--press-file", str(ROOT / "tools/port/synth/pak.press")]
    p = subprocess.Popen(cmd, cwd=logs, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if term_after:
        time.sleep(term_after)
        p.send_signal(signal.SIGTERM)
    try:
        out, _ = p.communicate(timeout=300)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
    out = out.decode(errors="replace")
    (logs / f"{name}.log").write_text(out, encoding="utf-8")
    return p.returncode, out, pak


def retraces(out):
    m = re.search(r"rush2049pc: (\d+) retraces in ([\d.]+) s; (\d+) came a whole retrace late and were caught up, "
                  r"(\d+) dropped", out)
    return (int(m[1]), int(m[3]), int(m[4])) if m else None


def good_pak(path):
    """The ID block (or a copy) with right host-order checksums, as port/ultra/pakfile.c's pc_pak_id_valid."""
    try:
        img = path.read_bytes()
    except OSError:
        return False
    if len(img) != 0x8000:
        return False
    for off in (0x20, 0x60, 0x80, 0xC0):
        words = struct.unpack_from(f"<14H", img, off) if sys.byteorder == "little" else \
            struct.unpack_from(f">14H", img, off)
        s, i = sum(words) & 0xFFFF, sum(~w & 0xFFFF for w in words) & 0xFFFF
        s2, i2 = struct.unpack_from("<HH" if sys.byteorder == "little" else ">HH", img, off + 0x1C)
        if (s, i) == (s2, i2):
            return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--logs", default=str(ROOT / "build/synth/pakwrite"))
    ap.add_argument("--delay-ms", type=float, default=200)
    ap.add_argument("--no-control", action="store_true", help="skip the run with the writes on the main loop")
    args = ap.parse_args()
    logs = Path(args.logs).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    slow = {"RUSH_SIM_PAK_WRITE_MS": str(args.delay_ms), "RUSH_SIM_PAK_REWRITE": "1"}
    fails = []

    code, out, pak = run(args, logs, "pakwrite", slow)
    r = retraces(out)
    race = re.search(r"game state 0x00400000 \(was 0x[0-9A-F]+\) at vi (\d+)", out)
    race_vi = int(race[1]) if race else None
    handed = [int(v) for v in re.findall(r"controller 1's pak handed to the writer at retrace (\d+)", out)]
    in_race = [v for v in handed if race_vi is not None and v > race_vi]
    st = re.search(r"paks: (\d+) writes on the writer's thread \((\d+) images replaced.*?(\d+) writes failed\), "
                   r"the longest ([\d.]+) ms", out)
    print(f"pakwrite_check: writes held {args.delay_ms:.0f} ms: exit {code}, "
          f"{len(handed)} handed to the writer ({len(in_race)} in the race, from retrace {race_vi}), "
          f"{st[1] if st else '?'} written, the longest {st[4] if st else '?'} ms; retraces "
          f"{'%d, %d caught up, %d dropped' % r if r else '?'}")
    if code != 0:
        fails.append(f"the run exited {code}")
    if not r:
        fails.append("no retrace line at exit")
    elif r[2] != 0 or r[1] > 6:
        fails.append(f"the slow writes cost retraces: {r[1]} caught up, {r[2]} dropped")
    if len(in_race) < 5:
        fails.append(f"only {len(in_race)} writes during the race")
    if not st or int(st[1]) < 5 or float(st[4]) < args.delay_ms * 0.9:
        fails.append("the writer's line at exit lacks the slow writes")
    if not good_pak(pak):
        fails.append(f"{pak.name} is not a good pak at exit")

    # SIGTERM while the first write (the new pak's, at the first flush) is held: the end waits for it.
    code, out, pak = run(args, logs, "pakwrite-term", {"RUSH_SIM_PAK_WRITE_MS": "3000"}, term_after=1.5)
    waited = "the saves are still being written" in out
    print(f"pakwrite_check: SIGTERM during a 3 s write: exit {code}, the end {'waited' if waited else 'did not wait'}"
          f", {pak.name} {'written, a good pak' if good_pak(pak) else 'NOT written'}")
    if not good_pak(pak):
        fails.append("SIGTERM during a write: the pak was not written")
    if "could not all be written" in out:
        fails.append("SIGTERM during a write: the end gave up")

    # A fault during a held write: the blank cartridge's boot faults in its first retraces.
    code, out, pak = run(args, logs, "pakwrite-fault", {"RUSH_SIM_PAK_WRITE_MS": "1500"}, seconds="20", no_rom=True)
    faulted = "Segmentation fault" in out or "Access violation" in out
    waited = "writing the saves before the program stops" in out
    print(f"pakwrite_check: a fault during a 1.5 s write: exit {code}, "
          f"{'faulted' if faulted else 'no fault'}, the handler {'waited' if waited else 'had nothing to wait for'}, "
          f"{pak.name} {'written, a good pak' if good_pak(pak) else 'not written'}")
    if waited and ("the saves are written" not in out or not good_pak(pak)):
        fails.append("a fault during a write: the handler did not see it written")

    if not args.no_control:
        code, out, pak = run(args, logs, "pakwrite-sync", dict(slow, RUSH_SIM_PAK_WRITE_SYNC="1"))
        r = retraces(out)
        print(f"pakwrite_check: the control, writes on the main loop: retraces "
              f"{'%d, %d caught up, %d dropped' % r if r else '?'}")
        if not r or r[2] == 0:
            fails.append("the control dropped no retraces: the check can't see a stall on this host")

    for f in fails:
        print(f"pakwrite_check: FAIL: {f}")
    print(f"pakwrite_check: {'ok' if not fails else 'FAILED'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
