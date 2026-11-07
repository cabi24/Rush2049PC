#!/usr/bin/env python3
"""The F1 Online page's restarts and the sessions' replays (docs/port/NETPLAY.md 13), on the synthetic cartridge: real
rush2049pc processes over UDP on 127.0.0.1, driven through the overlay by press scripts.

    net_menu_check.py restart --exe EXE --rom ROM --logs DIR
    net_menu_check.py replay  --exe EXE --rom ROM --logs DIR

restart: two sessions, each with one side started from the page.
  host: a process in real time opens F1, Online, Host a race, Host now (its port net.port's, --set); it restarts into
    --net-host (exec: the same process, its output the same stream), and a guest joins it (--net-join, port 2's presses
    of race2p). Both run paced at 4 times the pace (RUSH_PACE_SPEED); the hashes agree at every retrace both have.
  join: a host (--net-host, race2p's port 1 presses: the menus to a race); a process in real time opens F1, Online,
    Join a race, picks the host from the hosts joined before (--set net.recent) and Join now; it restarts into
    --net-join and the hashes agree. The host stops first at its limit: the joined side, started from the page
    (--net-return), restarts once more without the session (its command line the first's but the press script), and
    is stopped once it has booted.
  Each restart's command line (said in the log) is checked: the session's own options at its end, the press script
  left out, the rest kept.
replay: a session of two processes (--fast, race2p, each player's pak in), then its host's input log replayed offline as
  it is (--replay-inputs alone: the session's settings, rules and paks from the log and the .mpk beside it): the
  hashes are the session's at every retrace; the paks beside the log are unchanged; and watched from the page (F1,
  Online, Watch a replay, Watch the last session's: a restart into --replay-inputs FILE --paced, RUSH_PACE_SPEED 4),
  the hashes are the session's again.

Exit status: 0 when it holds. Stdlib only, Python 3.9+.
"""
import argparse
import hashlib
import os
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import hash_compare  # noqa: E402
import net_session_check as nsc  # noqa: E402
import synth_paths  # noqa: E402

_RESTART = re.compile(r"^rush2049pc: restarting: (.*)$")
SPEED = {"RUSH_PACE_SPEED": "4"}


def say(*a):
    print("net_menu_check:", *a, flush=True)


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def presses(path, steps, start=1.0, gap=0.3):
    """A press script of the overlay's steps, one every gap seconds of real time: MENU, then each button."""
    lines, t = [f"{start:.2f} MENU 0.1"], start + 0.6
    for b in steps:
        lines.append(f"{t:.2f} {b} 0.1")
        t += gap
    Path(path).write_text("\n".join(lines) + "\n")
    return t


# The overlay's way to each action: the main page's eighth item is Online (after Saves).
TO_ONLINE = ["DOWN"] * 7 + ["A"]
HOST_NOW = TO_ONLINE + ["A", "DOWN", "DOWN", "A"]                    # Host a race; Port, Join code, Host now
JOIN_RECENT = TO_ONLINE + ["DOWN", "A", "DOWN", "DOWN", "DOWN", "A", "A"]   # Join a race; the first recent; Join now
WATCH_LAST = TO_ONLINE + ["DOWN", "DOWN", "A", "A"]                  # Watch a replay; Watch the last session's


def base(a, extra):
    return [str(Path(a.exe).resolve()), "--rom", str(Path(a.rom).resolve()), "--no-rom-check", "--headless",
            "--no-config", "--audio", "null", "--no-log", "--net-bind", "127.0.0.1", "--log-hash", "1,30",
            "--status", "1"] + list(extra)


def env_for(run_dir, extra=None):
    e = dict(os.environ, RUSH_STATE_EXACT="1", **synth_paths.data_env(run_dir))
    for k in ("RUSH_SIM_NET", "RUSH_SIM_DESYNC", "RUSH_SIM_GATE", "RUSH2049_TEST_NET_BUILD", "RUSH_PACED"):
        e.pop(k, None)
    e.update(extra or {})
    return e


def start(a, tag, role, cmd, env):
    run_dir = Path(a.logs) / f"{tag}.run" / role
    run_dir.mkdir(parents=True, exist_ok=True)
    return nsc.Proc(role, cmd, env_for(run_dir, env), run_dir, Path(a.logs) / f"{tag}.{role}.log"), run_dir


def parts(text):
    """A process's output split at its restarts: [before, after the first, after the second, ...]; and the restarts'
    command lines."""
    out, cmds, cur = [], [], []
    for line in text.splitlines():
        m = _RESTART.match(line)
        if m:
            out.append("\n".join(cur) + "\n")
            cur = []
            cmds.append(m.group(1))
        else:
            cur.append(line)
    out.append("\n".join(cur) + "\n")
    return out, cmds


def wait_for(proc, pattern, timeout, after=0):
    """Until a line of proc's from line `after` on matches; the line's index, or -1."""
    t0, rx = time.monotonic(), re.compile(pattern)
    while time.monotonic() - t0 < timeout:
        for i in range(after, len(proc.lines)):
            if rx.search(proc.lines[i]):
                return i
        if proc.proc.poll() is not None and proc.exit_time:
            break
        time.sleep(0.05)
    return -1


def same_hashes(name, ta, tb, least=60):
    ha, hb = hash_compare.hashes(ta), hash_compare.hashes(tb)
    common = sorted(r for r in ha if r in hb)
    bad = [r for r in common if ha[r] != hb[r]]
    if bad:
        say(f"{name}: the hashes DIFFER from retrace {bad[0]} ({len(bad)} of {len(common)})")
        return 1, len(common)
    if len(common) < least:
        say(f"{name}: only {len(common)} retraces hashed by both ({len(ha)} and {len(hb)})")
        return 1, len(common)
    return 0, len(common)


def check_cmd(name, cmd, want, unwanted):
    bad = 0
    for w in want:
        if w not in cmd:
            say(f"{name}: the restart's command line lacks '{w}': {cmd}")
            bad = 1
    for u in unwanted:
        if u in cmd:
            say(f"{name}: the restart's command line has '{u}': {cmd}")
            bad = 1
    return bad


# ---------------------------------------------------------------- restart

def restart_host(a):
    tag, port = "menu-host", free_port()
    d = Path(a.logs)
    script = nsc.SCRIPTS / "race2p.press"
    _, guest_part, _ = nsc.split(script.read_text())
    (d / f"{tag}.guest.press").write_text(guest_part)
    presses(d / f"{tag}.host.press", HOST_NOW)
    hcmd = base(a, ["--set", f"net.port={port}", "--seconds", "16", "--press-file", str(d / f"{tag}.host.press")])
    host, _ = start(a, tag, "host", hcmd, SPEED)
    if wait_for(host, r"rush2049pc: net: (?:host: )?listening on port", 60) < 0:
        host.kill()
        say(f"host: the page's Host now didn't restart into a host:\n" + "\n".join(host.lines[-15:]))
        return 1
    gcmd = base(a, ["--net-join", f"127.0.0.1:{port}", "--seconds", "20", "--press-file",
                    str(d / f"{tag}.guest.press")])
    guest, _ = start(a, tag, "guest", gcmd, SPEED)
    th, tg = host.wait(300), guest.wait(300)
    hp, hcmds = parts(th)
    bad = 0
    if len(hcmds) != 1:
        say(f"host: {len(hcmds)} restarts, not one")
        return 1
    bad |= check_cmd("host", hcmds[0], ["--net-return --net-host", f"--net-host {port} --net-code ''",
                                        f"net.port={port}", "--seconds 16", "--net-bind 127.0.0.1"],
                     ["--press-file"])
    b, n = same_hashes("host (restarted from the page) and guest", hp[1], tg)
    bad |= b
    for role, t, want in (("host", th, (0,)), ("guest", tg, (0,))):
        if nsc.exit_code(t) not in want:
            say(f"host: the {role} exits {nsc.exit_code(t)}: {nsc.ended(t)}")
            bad = 1
    if "online page: Restarting to host on port" not in hp[0]:
        say("host: the page's act isn't in the log")
        bad = 1
    if not bad:
        say(f"host: F1, Online, Host now restarted the process into the host ({'exec' if os.name != 'nt' else 'CreateProcess'}): "
            f"{hcmds[0][hcmds[0].find('--net-return'):]}; a guest joined; {n} retraces hashed by both, all the same; "
            f"the host ended at its limit and stayed ended (no return: its own end)")
    return bad


def restart_join(a):
    tag, port = "menu-join", free_port()
    d = Path(a.logs)
    script = nsc.SCRIPTS / "race2p.press"
    host_part, _, _ = nsc.split(script.read_text())
    (d / f"{tag}.host.press").write_text(host_part)
    hcmd = base(a, ["--net-host", str(port), "--seconds", "14", "--press-file", str(d / f"{tag}.host.press")])
    host, _ = start(a, tag, "host", hcmd, SPEED)
    if wait_for(host, r"listening on port", 60) < 0:
        host.kill()
        say("join: the host never listened")
        return 1
    presses(d / f"{tag}.guest.press", JOIN_RECENT)
    # --seconds 18: the guest's limit before its restart (real time), in the session (game time: the host's 14 come
    # first) and in the run it goes back to (real time again: it ends by itself, also where a restart is a child
    # process the first waits for, Windows')
    gcmd = base(a, ["--set", f"net.recent=127.0.0.1:{port}, other-pc", "--seconds", "18", "--press-file",
                    str(d / f"{tag}.guest.press")])
    guest, _ = start(a, tag, "guest", gcmd, SPEED)
    th = host.wait(300)
    # the guest: joined (restart 1), the host left at its limit, back to the game (restart 2), to its limit
    tg = guest.wait(300)
    gp, gcmds = parts(tg)
    bad = 0
    if len(gcmds) != 2:
        say(f"join: {len(gcmds)} restarts of the guest, not two (the join, then back):\n" + "\n".join(tg.splitlines()[-12:]))
        return 1
    bad |= check_cmd("join", gcmds[0], [f"--net-return --net-join 127.0.0.1:{port} --net-code ''", "--seconds 18",
                                        f"net.recent=127.0.0.1:{port}, other-pc"], ["--press-file"])
    bad |= check_cmd("back", gcmds[1], ["--seconds 18", "--net-bind 127.0.0.1"],
                     ["--net-join", "--net-return", "--net-code", "--press-file"])
    if "back to the game (--net-return)" not in gp[1]:
        say("join: the joined side didn't say it goes back to the game")
        bad = 1
    if "online: ended (peer-left)" not in gp[1]:
        say(f"join: the joined side's session didn't end with the host leaving: {nsc.ended(gp[1])}")
        bad = 1
    b, n = same_hashes("host and guest (restarted from the page)", th, gp[1])
    bad |= b
    if nsc.exit_code(th) != 0:
        say(f"join: the host exits {nsc.exit_code(th)}: {nsc.ended(th)}")
        bad = 1
    if nsc.exit_code(tg) != 0 or "time reached" not in gp[2] or "online:" in gp[2].replace("online: this PC", ""):
        say(f"join: the run back in the game didn't run offline to its limit (exit {nsc.exit_code(tg)})")
        bad = 1
    if not bad:
        say(f"join: F1, Online, Join a race, the recent host, Join now restarted the process into the guest: "
            f"{gcmds[0][gcmds[0].find('--net-return'):]}; {n} retraces hashed by both, all the same; the host left at "
            f"its limit and the guest, started from the page, restarted without the session and ran offline to its "
            f"limit: {gcmds[1][len(str(Path(a.exe).resolve())):][:160]}...")
    return bad


def mode_restart(a):
    return restart_host(a) | restart_join(a)


# ---------------------------------------------------------------- replay

def mode_replay(a):
    tag, port = "menu-replay", free_port()
    d = Path(a.logs)
    host_part, guest_part, _ = nsc.split((nsc.SCRIPTS / "race2p.press").read_text())
    (d / f"{tag}.host.press").write_text(host_part)
    (d / f"{tag}.guest.press").write_text(guest_part)
    shutil.rmtree(d / f"{tag}.run", ignore_errors=True)
    common = ["--fast", "--seconds", "40", "--until-state", "0x400000:300", "--net-delay", "2"]
    host, hdir = start(a, tag, "host", base(a, common + ["--net-host", str(port), "--press-file",
                                                        str(d / f"{tag}.host.press")]), None)
    if wait_for(host, r"listening on port", 60) < 0:
        host.kill()
        say("replay: the host never listened")
        return 1
    guest, _ = start(a, tag, "guest", base(a, common + ["--net-join", f"127.0.0.1:{port}", "--press-file",
                                                       str(d / f"{tag}.guest.press")]), None)
    th, tg = host.wait(300), guest.wait(300)
    bad, n = same_hashes("the session (host and guest)", th, tg)
    m = re.search(r"online: the session's input log: (\S+)", th)
    if bad or not m:
        say("replay: no session to replay" + ("" if m else ": the host names no input log"))
        return 1
    log = m.group(1)
    paks = sorted(Path(log).parent.glob(Path(log).stem + ".p*.mpk"))
    before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paks}
    # as it is: --replay-inputs alone (no --session-rules, --saves, --set or pak files given)
    rdir = d / f"{tag}.run" / "offline"
    rdir.mkdir(parents=True, exist_ok=True)
    rcmd = [str(Path(a.exe).resolve()), "--rom", str(Path(a.rom).resolve()), "--no-rom-check", "--headless",
            "--no-config", "--audio", "null", "--no-log", "--fast", "--replay-inputs", log, "--log-hash", "1,30",
            "--seconds", "40", "--until-state", "0x400000:300"]
    p = subprocess.run(rcmd, env=env_for(rdir), cwd=rdir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       timeout=300)
    tr = p.stdout.decode(errors="replace")
    (d / f"{tag}.offline.log").write_text(tr)
    if "was recorded by another build" in tr or p.returncode not in (0,):
        say(f"replay: refused or failed (exit {p.returncode}): " +
            " / ".join(l for l in tr.splitlines() if "replay" in l)[:600])
        return 1
    b, nr = same_hashes("the session and its log replayed offline as it is", th, tr)
    bad |= b
    hh, hr = hash_compare.hashes(th), hash_compare.hashes(tr)
    if max(hr, default=0) < max(hh, default=0):
        say(f"replay: the replay hashed to retrace {max(hr, default=0)}, the session to {max(hh, default=0)}")
        bad = 1
    after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paks}
    if before != after or len(before) != 2:
        say(f"replay: the paks beside the log: {len(before)} of 2, changed: {before != after}")
        bad = 1
    adopted = len(re.findall(r"replay: an online session's input log", tr))
    if not adopted:
        say("replay: the replay doesn't say it took the session's settings and paks")
        bad = 1
    # watched from the page: F1, Online, Watch a replay, Watch the last session's, in the host's data folder
    presses(d / f"{tag}.watch.press", WATCH_LAST)
    wcmd = [str(Path(a.exe).resolve()), "--rom", str(Path(a.rom).resolve()), "--no-rom-check", "--headless",
            "--no-config", "--audio", "null", "--no-log", "--log-hash", "1,30", "--seconds", "40", "--until-state",
            "0x400000:300", "--press-file", str(d / f"{tag}.watch.press")]
    w = subprocess.run(wcmd, env=env_for(hdir, SPEED), cwd=hdir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       timeout=300)
    tw = w.stdout.decode(errors="replace")
    (d / f"{tag}.watch.log").write_text(tw)
    wp, wcmds = parts(tw)
    if len(wcmds) != 1:
        say(f"replay: watching restarted {len(wcmds)} times, not once:\n" + "\n".join(tw.splitlines()[-10:]))
        return 1
    bad |= check_cmd("watch", wcmds[0], [f"--net-return --replay-inputs {log} --paced"], ["--press-file"])
    b, nw = same_hashes("the session and its replay watched from the page (paced)", th, wp[1])
    bad |= b
    if "virtual clock paced" not in wp[1] and "paced" not in wp[1]:
        say("replay: the watched replay doesn't say it is paced")
    if not bad:
        say(f"replay: a session of {n} retraces hashed alike on both; its host's log replayed offline as it is "
            f"(--replay-inputs alone): {nr} retraces, every hash the session's; the paks beside it unchanged; "
            f"watched from the page (a restart into --replay-inputs FILE --paced): {nw} retraces, every hash the "
            f"session's")
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("mode", choices=["restart", "replay"])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/pc/synth/net-menu"))
    a = ap.parse_args()
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    a.logs = str(Path(a.logs).resolve())
    sys.exit(1 if {"restart": mode_restart, "replay": mode_replay}[a.mode](a) else 0)


if __name__ == "__main__":
    main()
