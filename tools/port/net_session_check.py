#!/usr/bin/env python3
"""Online sessions of the game: two rush2049pc processes over UDP on 127.0.0.1 (docs/port/NETPLAY.md "Status: the
session"), on the synthetic cartridge.

    net_session_check.py path     --exe EXE --rom ROM --logs DIR NAME [--delay N]
    net_session_check.py sim      --exe EXE --rom ROM --logs DIR --model MODEL [NAME]
    net_session_check.py auto     --exe EXE --rom ROM --logs DIR [--model MODEL] [NAME]
    net_session_check.py paced    --exe EXE --rom ROM --logs DIR [--model MODEL] [NAME]
    net_session_check.py fuzz     --exe EXE --rom ROM --logs DIR [--seeds N] [--first S] [--seconds T]
    net_session_check.py desync   --exe EXE --rom ROM --logs DIR [NAME] [--at N]
    net_session_check.py mismatch --exe EXE --rom ROM --logs DIR [--mods DIR]
    net_session_check.py adopt    --exe EXE --rom ROM --logs DIR
    net_session_check.py ends     --exe EXE --rom ROM --logs DIR
    net_session_check.py wait     --exe EXE --rom ROM --logs DIR
    net_session_check.py view     --exe EXE --rom ROM --logs DIR

A session is a host (--net-host 0: any port, read from its log) and a guest (--net-join 127.0.0.1:PORT), both bound
to 127.0.0.1 (--net-bind), each with a data folder of its own, each running the path's press script for its own
player: a two-player script is split by port (its @2 presses are the guest's, as its controller 1's), a one-player
script is the host's and the guest's player stays idle. Each run's trace is replay_check.py's (every state change, a
status line a second, every retrace's physics as exact floats, the game's pads at each change, the state hash every
retrace and the full one every 30).

path: the two processes' traces are the same, line for line, and with a fixed delay (--net-delay N, default 2) they
  are the trace of one process running the whole script with --input-delay N (its port 2 connected throughout, as
  the session's is). A process that ends before its limit because the other left must have run a prefix of it.
sim: the same with RUSH_SIM_NET's model on both PCs' receive paths (docs/port/NETWORK.md 8; the pause model on the
  guest only), at a delay that covers the model's latency: the network changes when retraces happen, never what
  they do.
auto: the adaptive delay under a model: both traces the same, and the host's input log replays in one process
  (--replay-inputs) to the same trace.
paced: the session's own clock, paced (no --fast): at 4 times the pace (RUSH_PACE_SPEED) with a fixed delay, the
  trace of one --fast process; at the real pace under a model with the adaptive delay, both the same, at 60 retraces
  a second of wall time (the time re-based after a stall aside), the stalls counted.
fuzz: seeded random presses on both players (buttons, sticks, START and the D-pad through the game's menus, the
  settings overlay opened and closed, which holds nothing in a session) for --seconds of game time per seed, the
  delay adaptive: both processes' hashes agree at every retrace, and nothing ends the session early.
desync: RUSH_SIM_DESYNC changes a word of the guest's state at retrace --at: both end with "desync" at that retrace,
  exit 3, each writes its report beside its input log, and the reports name the word.
mismatch: a guest with another ROM, another build (RUSH2049_TEST_NET_BUILD) or other mods is refused with what
  differs (both end, exit 3); a setup difference the handshake doesn't see (a test model's environment) ends both at
  retrace 0, "the two PCs' setups differ".
adopt: a guest with other settings (game.cars, audio.voices...) takes the host's for the session, logged, and both
  traces are the same; each player's pak (new ones, the same ID) is installed, port 2's copy with an ID of its own,
  and nothing is written to the disk.
ends: the guest killed (the host ends with "timeout" within --net-timeout), the guest leaving at its own limit (the
  host ends with "peer-left", exit 0), a guest joining nobody ("no-answer", and its message box says so: --window
  without a display, the boxes printed).
wait: the host's waiting screen (port/app/netwait.c), headless at 1280 x 720: RUSH2049_TEST_NET_WAIT_SHOT saves it
  (the panel, its text) and cancels it as Escape would (exit 0, no box, nothing of the game run); a host nobody joins
  gives up (RUSH2049_TEST_NET_HOST_WAIT: 2 s) with a message box saying so, exit 3.
view: own view (net.view = own, docs/port/NETPLAY.md 13) on one PC and the split screen on the other, each way round
  (race2p, delay 2): one game, the same trace and hash at every retrace as each other and as one process; the PC with
  own view says whose view it shows (the host player 1's, the guest player 2's), and its screenshot is one view across
  the frame (synth_shot.py own) while the other's is the split screen's two views.

Exit status: 0 when it holds. Stdlib only, Python 3.9+.
"""
import argparse
import os
import random
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import hash_compare  # noqa: E402
import replay_check  # noqa: E402
import synth_paths  # noqa: E402
import synth_shot  # noqa: E402

SCRIPTS = synth_paths.PRESS_DIR
# The models (NETWORK.md 8's, the real-time versions of tools/port/net_check.py; shift's changes sooner still, inside a
# run of 10-20 s) and the fixed delay that covers each one-way latency (more delay, more retraces a round trip on the
# unpaced clock: the runs stay short; shift's 4 stalls through its 200 ms).
MODELS = {
    "clean": ("", "", 2),
    "lan20": ("delay=20", "delay=20", 3),
    "wan80": ("delay=80,jitter=20", "delay=80,jitter=20", 8),
    "bad150": ("delay=150,jitter=40,loss=10,dup=2,reorder=2", "delay=150,jitter=40,loss=10,dup=2,reorder=2", 14),
    "shift": ("delay=10,at=1,delay=200,at=3,delay=30", "delay=10,at=1,delay=200,at=3,delay=30", 4),   # sooner
    "pause": ("delay=20", "delay=20,pause=4:3", 3),
    "burst": ("delay=30,jitter=5,burst=3:0.5:30", "delay=30,jitter=5,burst=3:0.5:30", 4),
}
_PRESS = re.compile(r"^(\s*\d+(?:\.\d+)?\s+)([A-Z]+)(?:@(\d))?(.*)$")
_END = re.compile(r"^rush2049pc: online: ended \(([a-z-]+)\): (.*)$", re.M)


def say(*a):
    print("net_session_check:", *a, flush=True)


# ---------------------------------------------------------------- scripts

def split(text):
    """A press script's host part (no port, or @1) and guest part (@2, as the guest's own controller 1)."""
    host, guest, two = [], [], False
    for line in text.splitlines():
        m = _PRESS.match(line.split("#", 1)[0])
        if not m:   # comments, pak, frames: both
            host.append(line)
            guest.append(line)
            continue
        port = int(m.group(3) or 1)
        if port > 2:
            raise SystemExit(f"net_session_check: a press on port {port}: sessions are two players")
        if port == 2:
            two = True
            guest.append(f"{m.group(1)}{m.group(2)}{m.group(4)}")
        else:
            host.append(line)
    return "\n".join(host) + "\n", "\n".join(guest) + "\n", two


def merged(text, two):
    """The whole script for one process: port 2 connected from the start, as a session's is."""
    return text if two else text + "\n9999 A@2 0.01   # port 2 connected throughout (never pressed in the run)\n"


# ---------------------------------------------------------------- processes

class Proc:
    """One rush2049pc, its output read by a thread: the lines kept, its listening port noted."""

    def __init__(self, name, cmd, env, cwd, log_path):
        self.name, self.lines, self.port = name, [], None
        self.log_path = log_path
        self.got_port = threading.Event()
        self.t0 = time.monotonic()
        self.exit_time = None
        self.proc = subprocess.Popen(cmd, env=env, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        for raw in self.proc.stdout:
            line = raw.decode(errors="replace").rstrip("\r\n")
            self.lines.append(line)
            m = re.match(r"^rush2049pc: net: (?:host: )?listening on port (\d+)", line)
            if m:
                self.port = int(m.group(1))
                self.got_port.set()
        self.exit_time = time.monotonic()
        self.got_port.set()

    def wait(self, timeout):
        try:
            self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            self.lines.append(f"net_session_check: {self.name} killed after {timeout} s")
        self.reader.join(10)
        text = "\n".join(self.lines) + "\n"
        if self.proc.returncode:
            text += f"rush2049pc: state reached: exit {self.proc.returncode}\n"
        Path(self.log_path).write_text(text, encoding="utf-8")
        return text

    def kill(self):
        self.proc.kill()


def base_cmd(a, press, spec, extra, hashes=True, until=True, fast=True):
    cmd = [str(Path(a.exe).resolve()), "--rom", str(Path(a.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--audio", "null", "--no-pak"] + (["--fast"] if fast else []) + \
          ["--no-log", "--press-file", str(Path(press).resolve()), "--seconds", spec["seconds"], "--status", "1",
           "--log-physics", "1", "--log-pads"]
    if until and spec.get("until"):
        cmd += ["--until-state", spec["until"]]
    if hashes:
        cmd += ["--log-hash", "1,30"]
    args = [str(Path(replay_check.MODS_ROOT) / x[len("mods/"):]) if x.startswith("mods/") and replay_check.MODS_ROOT
            else x for x in spec["args"]]
    return cmd + args + list(extra)


def session(a, tag, host_press, guest_press, spec, host_extra=(), guest_extra=(), host_env=None, guest_env=None,
            timeout=900, guest_args=None, after_start=None, fast=True):
    """A host and a guest; their log texts (and the Proc objects)."""
    logs = Path(a.logs)
    run_dir = logs / f"{tag}.run"
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    base_env = dict(os.environ, RUSH_STATE_EXACT="1")
    for k in ("RUSH_SIM_NET", "RUSH_SIM_DESYNC", "RUSH_SIM_GATE", "RUSH2049_TEST_NET_BUILD"):
        base_env.pop(k, None)
    base_env.update(spec["env"])
    procs = []
    for role, press, extra, env in (("host", host_press, host_extra, host_env), ("guest", guest_press, guest_extra,
                                                                                 guest_env)):
        e = dict(base_env, **synth_paths.data_env(run_dir / role))
        e.update(env or {})
        cwd = run_dir / f"{role}.cwd"
        cwd.mkdir(parents=True, exist_ok=True)
        s = dict(spec)
        if role == "guest" and guest_args is not None:
            s["args"] = guest_args
        net = ["--net-bind", "127.0.0.1"]
        if role == "host":
            net += ["--net-host", "0"]
        else:
            procs[0].got_port.wait(120)
            if not procs[0].port:
                say(f"{tag}: the host never listened")
                procs[0].kill()
                procs[0].wait(10)
                return None
            net += ["--net-join", f"127.0.0.1:{procs[0].port}"]
        procs.append(Proc(role, base_cmd(a, press, s, net + list(extra), fast=fast), e, cwd, logs / f"{tag}.{role}.log"))
    if after_start:
        after_start(procs)
    texts = [p.wait(timeout) for p in procs]
    return texts[0], texts[1], procs


def one(a, tag, press, spec, extra=(), env=None):
    """One process (no session), with the same options."""
    run_dir = Path(a.logs) / f"{tag}.run"
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    e = dict(os.environ, RUSH_STATE_EXACT="1", **synth_paths.data_env(run_dir))
    for k in ("RUSH_SIM_NET", "RUSH_SIM_DESYNC", "RUSH_SIM_GATE", "RUSH2049_TEST_NET_BUILD"):
        e.pop(k, None)
    e.update(spec["env"])
    e.update(env or {})
    p = subprocess.run(base_cmd(a, press, spec, extra), env=e, cwd=run_dir, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=900)
    text = p.stdout.decode(errors="replace")
    if p.returncode:
        text += f"\nrush2049pc: state reached: exit {p.returncode}\n"
    (Path(a.logs) / f"{tag}.log").write_text(text, encoding="utf-8")
    return text


def ended(text):
    m = _END.search(text)
    return (m.group(1), m.group(2)) if m else (None, None)


def exit_code(text):
    m = re.search(r"state reached: exit (\d+)", text)
    return int(m.group(1)) if m else 0


def compare_session(name, th, tg, tone=None):
    """The host's and the guest's traces the same; a side that stopped early (the other left) a prefix of the other;
    and with tone, both the single process's. 0 when it holds."""
    a, b = replay_check.trace(th), replay_check.trace(tg)
    bad = 0
    n = min(len(a), len(b))
    d = replay_check.first_diff(a[:n], b[:n])
    if d:
        i, x, y = d
        say(f"{name}: host and guest DIFFER at line {i}\n    {x}\n    {y}")
        return 1
    if len(a) != len(b):
        short, long_ = ("host", a) if len(a) < len(b) else ("guest", b)
        say(f"{name}: the {short}'s trace is {min(len(a), len(b))} lines of the other's {max(len(a), len(b))} "
            f"(it ended first: {ended(th if short == 'host' else tg)})")
        if not any("reached:" in l for l in long_[-3:]):
            bad = 1
    if tone is not None:
        c = replay_check.trace(tone)
        longest = a if len(a) >= len(b) else b
        d = replay_check.first_diff(longest, c[:len(longest)])
        if d:
            i, x, y = d
            say(f"{name}: the session and one process DIFFER at line {i} of {len(longest)} and {len(c)}\n    {x}\n    {y}")
            bad = 1
        elif len(longest) != len(c):
            say(f"{name}: the session's trace is {len(longest)} lines, one process's {len(c)}")
            bad = 1
    ha, hb = hash_compare.hashes(th), hash_compare.hashes(tg)
    if not ha or not hb:
        say(f"{name}: no hash lines")
        bad = 1
    common = [r for r in ha if r in hb]
    if any(ha[r] != hb[r] for r in common):
        say(f"{name}: the hashes differ at retrace {min(r for r in common if ha[r] != hb[r])}")
        bad = 1
    return bad


def path_files(a, name, tag):
    text = (SCRIPTS / f"{name}.press").read_text()
    host, guest, two = split(text)
    d = Path(a.logs)
    hp, gp, mp = d / f"{tag}.host.press", d / f"{tag}.guest.press", d / f"{tag}.one.press"
    hp.write_text(host)
    gp.write_text(guest)
    mp.write_text(merged(text, two))
    return hp, gp, mp, two


def session_stats(text):
    m = re.search(r"online: (\d+) retraces; (\d+) waited for the \w+'s input \(([\d.]+) ms in all", text)
    d = re.search(r"delay (\d+) \(peer (\d+), (\d+)-(\d+), (\d+) changes\)", text)
    s = re.search(r"stalls (\d+) \(([\d.]+) ms", text)
    return (f"{m.group(1)} retraces, {m.group(2)} waited ({float(m.group(3)) / 1000:.1f} s)" if m else "?") + \
           (f", delay {d.group(3)}-{d.group(4)}" if d else "") + (f", {s.group(1)} stalls" if s else "")


# ---------------------------------------------------------------- the modes

def mode_path(a, name, model="clean", delay=None, tag=None, until=None):
    spec = synth_paths.read_script(SCRIPTS / f"{name}.press")
    if until:
        spec = dict(spec, until=until)
    hsim, gsim, mdelay = MODELS[model]
    delay = delay or (a.delay if model == "clean" and a.delay else mdelay)
    tag = tag or (f"{name}" if model == "clean" else f"{name}.{model}")
    hp, gp, mp, two = path_files(a, name, tag)
    t0 = time.time()
    r = session(a, tag, hp, gp, spec, ["--net-delay", str(delay)], ["--net-delay", str(delay)],
                {"RUSH_SIM_NET": hsim} if hsim else None, {"RUSH_SIM_NET": gsim} if gsim else None)
    took = time.time() - t0
    if not r:
        return 1
    th, tg, _ = r
    tone = one(a, f"{tag}.one", mp, spec, ["--input-delay", str(delay), "--session-rules"])
    bad = compare_session(f"{name} ({model}, delay {delay})", th, tg, tone)
    for role, t in (("host", th), ("guest", tg)):
        if exit_code(t) not in (0,):
            say(f"{name}: the {role} exits {exit_code(t)}: {ended(t)}")
            bad = 1
    reached = spec["expect"]
    ends = [l for l in replay_check.trace(th) if "reached:" in l]
    if reached and not (ends and reached in ends[-1]):
        say(f"{name}: the session doesn't end as the path expects ('{reached}'): {ends[-1] if ends else 'no end'}")
        bad = 1
    if not bad:
        hh = hash_compare.hashes(th)
        say(f"{name:10} {model:7} delay {delay:2}: host = guest = one process, {len(replay_check.trace(th))} lines, "
            f"{len(hh)} retraces hashed ({'two players' if two else 'the guest idle'}); host {session_stats(th)}; "
            f"guest {session_stats(tg)} ({took:.1f} s)")
    return bad


def mode_sim(a, names):
    bad = 0
    for name in names or ["race2p"]:
        # to the race and 2 s into it: the menus, both players' presses, the start (the runs stay short)
        bad |= mode_path(a, name, model=a.model, until="0x400000:60")
    return bad


def mode_auto(a, names):
    bad = 0
    for name in names or ["race2p"]:
        spec = synth_paths.read_script(SCRIPTS / f"{name}.press")
        spec = dict(spec, until="0x400000:60")
        hsim, gsim, _ = MODELS[a.model]
        tag = f"{name}.auto.{a.model}"
        hp, gp, mp, two = path_files(a, name, tag)
        r = session(a, tag, hp, gp, spec, (), (), {"RUSH_SIM_NET": hsim} if hsim else None,
                    {"RUSH_SIM_NET": gsim} if gsim else None)
        if not r:
            return 1
        th, tg, _ = r
        b = compare_session(f"{name} (auto, {a.model})", th, tg)
        m = re.search(r"online: the session's input log: (\S+)", th)
        if not m:
            say(f"{name}: the host names no input log")
            return 1
        rep = one(a, f"{tag}.replay", mp, spec, ["--replay-inputs", m.group(1), "--session-rules"])
        if "was recorded by another build" in rep:
            say(f"{name}: the host's input log is refused by one process: " +
                " / ".join(l for l in rep.splitlines() if " against " in l or "file's, then" in l)[:400])
            b = 1
        b |= compare_session(f"{name} (auto, {a.model}) against its replay", th, rep)
        if not b:
            say(f"{name:10} auto {a.model:7}: host = guest, and the host's input log replays to the same trace in one "
                f"process ({len(replay_check.trace(th))} lines); host {session_stats(th)}; guest {session_stats(tg)}")
        bad |= b
    return bad


# ---------------------------------------------------------------- paced

_PACED = re.compile(r"paced: (\d+) retraces in ([\d.]+) s of wall time, ([\d.]+) a second \(([\d.]+) without the "
                    r"([\d.]+) s held or re-based\)")
_GATE = re.compile(r"paced: the pads gate: (\d+) retraces' pads came over half a retrace late, re-based \(([\d.]+) s\); "
                   r"(\d+) less late")


def paced_line(text):
    m, g = _PACED.search(text), _GATE.search(text)
    if not m:
        return None, "no paced line"
    return float(m.group(4)), (f"{m.group(1)} retraces in {float(m.group(2)):.1f} s, {float(m.group(3)):.3f} a second "
                               f"({float(m.group(4)):.3f} without {float(m.group(5)):.2f} s re-based)" +
                               (f"; the gate: {g.group(1)} re-based ({float(g.group(2)):.2f} s), {g.group(3)} on the grid"
                                if g else "; the gate never late"))


def mode_paced(a, names):
    """A session on the paced clock (no --fast: ULTRA.md 10). At 4 times the pace (RUSH_PACE_SPEED, both PCs) and a
    fixed delay: host = guest = one --fast process with --input-delay, paced mode taking --fast's course. At the real
    pace under --model with the adaptive delay: host = guest, and the wall time's rate, the stalls re-based."""
    bad = 0
    for name in names or ["race2p"]:
        spec = dict(synth_paths.read_script(SCRIPTS / f"{name}.press"), until="0x400000:60")
        hp, gp, mp, two = path_files(a, name, f"{name}.paced")
        speed = {"RUSH_PACE_SPEED": "4"}
        r = session(a, f"{name}.paced4", hp, gp, spec, ["--net-delay", "2"], ["--net-delay", "2"], speed, speed,
                    fast=False)
        if not r:
            return 1
        th, tg, _ = r
        tone = one(a, f"{name}.paced4.one", mp, spec, ["--input-delay", "2", "--session-rules"])
        b = compare_session(f"{name} paced x4", th, tg, tone)
        rate, said = paced_line(th)
        if rate is None or "online: the virtual clock paced to real time" not in th:
            say(f"{name} paced x4: not paced: {said}")
            b = 1
        if not b:
            say(f"{name:10} paced x4, delay 2: host = guest = one --fast process, {len(replay_check.trace(th))} lines; "
                f"host {said}")
        bad |= b
        hsim, gsim, _ = MODELS[a.model]
        r = session(a, f"{name}.paced.{a.model}", hp, gp, spec, (), (), {"RUSH_SIM_NET": hsim} if hsim else None,
                    {"RUSH_SIM_NET": gsim} if gsim else None, fast=False)
        if not r:
            return 1
        th, tg, _ = r
        b = compare_session(f"{name} paced {a.model}", th, tg)
        rates = [paced_line(t) for t in (th, tg)]
        for role, (rate, said) in zip(("host", "guest"), rates):
            if rate is None or not 59.5 <= rate <= 60.5:
                say(f"{name} paced {a.model}: the {role}'s pace: {said}")
                b = 1
        for role, t in (("host", th), ("guest", tg)):   # a stopped peer isn't a slow network (NETWORK.md 5)
            d = re.search(r"delay \d+ \(peer \d+, (\d+)-(\d+),", t)
            if a.model == "pause" and (not d or int(d.group(2)) > 4):
                say(f"{name} paced pause: the {role}'s delay went to {d.group(2) if d else '?'} for a pause")
                b = 1
        if not b:
            say(f"{name:10} paced {a.model}, the adaptive delay: host = guest, {len(replay_check.trace(th))} lines; "
                f"host {rates[0][1]}; guest {rates[1][1]}; {session_stats(th)}")
        bad |= b
    return bad


# ---------------------------------------------------------------- fuzz

BUTTONS = ["A", "B", "Z", "START", "UP", "DOWN", "LEFT", "RIGHT", "L", "R", "CU", "CD", "CL", "CR", "STICKUP",
           "STICKDOWN", "STICKLEFT", "STICKRIGHT"]


def fuzz_script(rng, seconds, port2):
    """A player's random presses: the game's way to a two-player race first (as race2p's), then button changes and
    stick sweeps at random times, START and the D-pad through the menus, now and then the settings overlay opened and
    closed again (nothing else pressed while it is open)."""
    lines = ["5 A", "8 START", "9 RIGHT 0.1", "11 A", "14 A", "17 A", "20 A", "23 A"] if not port2 else \
            ["14 A", "23 A"]
    t = 25.0
    while t < seconds:
        r = rng.random()
        if r < 0.03:   # the overlay over the running game: open, a while, closed
            stay = rng.uniform(0.2, 3)
            lines.append(f"{t:.3f} MENU 0.05")
            lines.append(f"{t + stay:.3f} MENU 0.05")
            t += stay + 0.3
            continue
        b = rng.choice(BUTTONS if r > 0.1 else ["START", "A", "B", "UP", "DOWN"])
        dur = rng.choice([rng.uniform(0.02, 0.2), rng.uniform(0.2, 2.5)])
        lines.append(f"{t:.3f} {b} {dur:.3f}")
        if rng.random() < 0.4:   # a stick sweep across a press
            lines.append(f"{t + rng.uniform(0, dur):.3f} {rng.choice(BUTTONS[14:])} {rng.uniform(0.05, 1.5):.3f}")
        t += rng.expovariate(2.5)
    return "\n".join(lines) + "\n"


def mode_fuzz(a):
    bad = 0
    total = 0
    hung_at = []
    for seed in range(a.first, a.first + a.seeds):
        rng = random.Random(seed)
        d = Path(a.logs)
        hp, gp = d / f"fuzz{seed}.host.press", d / f"fuzz{seed}.guest.press"
        hp.write_text(fuzz_script(rng, a.seconds, False))
        gp.write_text(fuzz_script(rng, a.seconds, True))
        spec = {"until": None, "seconds": str(a.seconds), "args": [], "env": {}, "expect": ""}
        t0 = time.time()
        r = session(a, f"fuzz{seed}", hp, gp, spec, host_extra=["--log-hash", "1,60"],
                    guest_extra=["--log-hash", "1,60"])
        took = time.time() - t0
        if not r:
            return 1
        th, tg, _ = r
        b = compare_session(f"fuzz seed {seed}", th, tg)
        # The game itself hanging (the virtual clock's "no retrace for 30 s"), on both PCs at one retrace, is the two
        # games still one: said, as a finding for the game, and the seed's time cut short.
        hung = [re.search(r"^rush2049pc: hung: t=\S+ vi=(\d+)", t, re.M) for t in (th, tg)]
        both_hung = all(hung) and hung[0].group(1) == hung[1].group(1)
        if both_hung:
            hung_at.append((seed, int(hung[0].group(1))))
        for role, t in (("host", th), ("guest", tg)):
            e = ended(t)
            if not both_hung and (e[0] not in ("local", "peer-left") or exit_code(t)):
                say(f"fuzz seed {seed}: the {role} ended {e} (exit {exit_code(t)})")
                b = 1
            if not both_hung and "time reached:" not in t and e[0] != "peer-left":
                say(f"fuzz seed {seed}: the {role} didn't run its {a.seconds} s")
                b = 1
        hh = hash_compare.hashes(th)
        opened = len(re.findall(r"settings open over the online session", th + tg))
        states = len(set(re.findall(r"game state (0x[0-9A-F]+)", th)))
        total += len(hh)
        if not b:
            say(f"fuzz seed {seed}: {len(hh)} retraces ({len(hh) / 60:.0f} s of game time) with the same hash on both, "
                f"{states} game states, the overlay opened {opened} times; host {session_stats(th)} ({took:.1f} s)" +
                (f"; THE GAME HUNG on both PCs at retrace {hung[0].group(1)} (the same: the two games stayed one; a "
                 f"hang of the game's for its own report)" if both_hung else ""))
        bad |= b
    if not bad:
        say(f"fuzz: {a.seeds} seeds, {total} retraces, every hash the same on both PCs" +
            (f"; the game hung (on both alike) with seed{'s' if len(hung_at) > 1 else ''} " +
             ", ".join(f"{s} at retrace {n}" for s, n in hung_at) if hung_at else ""))
    return bad


# ---------------------------------------------------------------- the failure modes

def mode_desync(a, name):
    spec = synth_paths.read_script(SCRIPTS / f"{name}.press")
    hp, gp, mp, two = path_files(a, name, f"{name}.desync")
    r = session(a, f"{name}.desync", hp, gp, spec, ["--net-delay", "2"], ["--net-delay", "2"],
                guest_env={"RUSH_SIM_DESYNC": f"{a.at}:{a.address}:1"})
    if not r:
        return 1
    th, tg, _ = r
    bad = 0
    reports = []
    for role, t in (("host", th), ("guest", tg)):
        e, msg = ended(t)
        if e != "desync" or f"retrace {a.at}" not in msg or exit_code(t) != 3:
            say(f"desync: the {role} ended {e}: {msg} (exit {exit_code(t)}), not a desync at retrace {a.at} (exit 3)")
            bad = 1
        m = re.search(r"online: the desync report \(retrace (\d+)'s state as hashed\): (\S+)", t)
        if not m or int(m.group(1)) != a.at or not Path(m.group(2)).exists():
            say(f"desync: the {role} wrote no report of retrace {a.at}")
            bad = 1
        else:
            reports.append(m.group(2))
        if not re.search(r"online: the session's input log: \S+\.pads", t) or "to replay the session" not in t:
            say(f"desync: the {role} doesn't say where its input log is and how to replay it")
            bad = 1
    if len(reports) == 2:
        out = subprocess.run([sys.executable, str(ROOT / "tools/port/hash_compare.py"), "--reports", reports[0],
                              reports[1]], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
        (Path(a.logs) / f"{name}.desync.reports.txt").write_text(out)
        if "rng+0x0" not in out:
            say(f"desync: the reports don't name the random seed (rng):\n{out[:800]}")
            bad = 1
    if not bad:
        say(f"desync: the guest's random seed changed at retrace {a.at}: both ended with a desync at retrace {a.at} "
            f"(exit 3), each wrote its report and named its input log; the reports differ in the seed")
    return bad


def corrupt_rom(a):
    src = Path(a.rom)
    out = Path(a.logs) / "other_rom.z64"
    b = bytearray(src.read_bytes())
    b[-1] ^= 0x5A   # the cartridge's last byte: another SHA-1, the same game
    out.write_bytes(bytes(b))
    return out.resolve()


def host_ends_later(procs):
    """The guest ends by itself; then the host, waiting for another guest, is told to stop (Ctrl-C)."""
    def run():
        procs[1].proc.wait()
        time.sleep(1)
        if os.name == "nt":   # no Ctrl-C to a single process there
            procs[0].kill()
        else:
            procs[0].proc.send_signal(signal.SIGINT)
    threading.Thread(target=run, daemon=True).start()


def mode_mismatch(a):
    bad = 0
    spec = synth_paths.read_script(SCRIPTS / "race2p.press")
    hp, gp, mp, two = path_files(a, "race2p", "mismatch")
    short = dict(spec, until="0x4:30")
    cases = [("rom", dict(guest_extra=["--rom", str(corrupt_rom(a))]), "the host's ROM is", "rejected"),
             ("build", dict(guest_env={"RUSH2049_TEST_NET_BUILD": "0123456789abcdef-test"}, after_start=host_ends_later),
              "same build", "version")]
    if a.mods and (Path(a.mods) / "colour").is_dir():
        cases.append(("mods", dict(guest_extra=["--mods-dir", str(Path(a.mods) / "colour")]), "the host's mods are",
                      "rejected"))
    else:
        say("mismatch: no mods folder (--mods): the mods case left out")
    for what, kw, says, why in cases:
        r = session(a, f"mismatch.{what}", hp, gp, short, timeout=120, **kw)
        if not r:
            return 1
        th, tg, _ = r
        b = 0
        if what == "build":   # the host turns the guest away and waits for another (stopped here with Ctrl-C)
            if "runs build 0123456789abcdef-test; this is" not in th or "can't join" not in th or \
                    (ended(th)[0] != "local" and os.name != "nt") or "physics vi=" in th:
                say(f"mismatch build: the host doesn't turn the guest away and go on waiting: {ended(th)}")
                b = 1
            th = None
        for role, t in (("host", th), ("guest", tg)):
            if t is None:
                continue
            e, msg = ended(t)
            if e != why or exit_code(t) != 3 or "physics vi=" in t:
                say(f"mismatch {what}: the {role} ended {e}: {msg} (exit {exit_code(t)}); expected {why}, exit 3, "
                    f"before the game")
                b = 1
        if says not in tg:
            say(f"mismatch {what}: the guest doesn't say '{says}'")
            b = 1
        if not b:
            line = [l for l in tg.splitlines() if says in l][0]
            say(f"mismatch {what}: refused before the game, " +
                ("the guest exits 3 and the host turns it away and waits on: " if what == "build" else
                 "both exit 3: ") + line[:200])
        bad |= b
    # a setup the handshake can't see: the final check at retrace 0
    r = session(a, "mismatch.setup", hp, gp, short, timeout=120, guest_env={"RUSH2049_TEST_HEAP_FLOOR": "17"},
                host_extra=["--net-delay", "2"], guest_extra=["--net-delay", "2"])
    if not r:
        return 1
    th, tg, _ = r
    b = 0
    for role, t in (("host", th), ("guest", tg)):
        e, msg = ended(t)
        if e != "desync" or "retrace 0" not in msg or "setups differ" not in t or exit_code(t) != 3:
            say(f"mismatch setup: the {role} ended {e}: {msg} (exit {exit_code(t)}); expected a desync at retrace 0, "
                f"'the two PCs' setups differ'")
            b = 1
    if "must match: env.RUSH2049_TEST_HEAP_FLOOR=17" not in tg:
        say("mismatch setup: the guest's log doesn't list its must-match text")
        b = 1
    if not b:
        say("mismatch setup: a test model on the guest only: both ended at retrace 0, 'the two PCs' setups differ', "
            "each log with its must-match text")
    return bad | b


def mode_adopt(a):
    spec = synth_paths.read_script(SCRIPTS / "race2p12.press")
    hp, gp, mp, two = path_files(a, "race2p12", "adopt")
    d = Path(a.logs) / "adopt.paks"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    hpak, gpak = d / "host.mpk", d / "guest.mpk"
    # the host has the script's game.cars = 12; the guest asks for 7 cars, 64 voices and far, and gets the host's
    r = session(a, "adopt", hp, gp, spec, ["--pak", str(hpak)],
                ["--pak", str(gpak), "--set", "audio.voices=64", "--set", "video.draw_distance=far"],
                guest_args=["--set", "game.cars=7", "--log-cars"])
    if not r:
        return 1
    th, tg, _ = r
    bad = compare_session("adopt (race2p12)", th, tg)
    for want in ("game.cars 7 -> 12 (the host's", "audio.voices 64 -> original (the host's",
                 "video.draw_distance far -> original (the host's"):
        if want not in tg:
            say(f"adopt: the guest doesn't say '{want}'")
            bad = 1
    if "(the host's, for this session)" in th:
        say("adopt: the host took settings")
        bad = 1
    for t, role in ((th, "host"), (tg, "guest")):
        if "the two paks have one ID: port 2's copy takes controller 2's own" not in t:
            say(f"adopt: the {role} doesn't give port 2's pak an ID of its own")
            bad = 1
        if "controller 1 (the host's player) has the host's controller pak, controller 2 (the guest's) has the " \
           "guest's controller pak" not in t:
            say(f"adopt: the {role} doesn't install both paks")
            bad = 1
        for ch in (1, 2):
            if f"controller {ch}: the game reads the controller pak" not in t:
                say(f"adopt: the {role}'s game doesn't read controller {ch}'s pak")
                bad = 1
    if hpak.exists() or gpak.exists():
        say("adopt: a session wrote a pak file")
        bad = 1
    ends = [l for l in replay_check.trace(th) if "reached:" in l]
    if not ends or "cars 12" not in ends[-1]:
        say(f"adopt: the race isn't the host's 12 cars: {ends[-1] if ends else 'no end'}")
        bad = 1
    if not bad:
        say(f"adopt: the guest took game.cars, audio.voices and video.draw_distance from the host (logged), both paks "
            f"in (port 2's with an ID of its own), none written; the same trace ({len(replay_check.trace(th))} lines), "
            f"a race of 12 cars")
    return bad


def free_udp_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def mode_ends(a):
    bad = 0
    spec = synth_paths.read_script(SCRIPTS / "race2p.press")
    hp, gp, mp, two = path_files(a, "race2p", "ends")
    # the guest killed in the race: the host ends with a timeout
    killed = {}

    def kill_later(procs):
        def run():
            g = procs[1]
            while g.proc.poll() is None:
                if any(l.startswith("rush2049pc: physics vi=1700:") for l in procs[0].lines[-200:]):
                    break
                time.sleep(0.01)
            killed["t"] = time.monotonic()
            g.kill()
        threading.Thread(target=run, daemon=True).start()

    r = session(a, "ends.disconnect", hp, gp, spec, ["--net-timeout", "3"], ["--net-timeout", "3"],
                after_start=kill_later, timeout=300)
    if not r:
        return 1
    th, tg, procs = r
    e, msg = ended(th)
    after = procs[0].exit_time - killed.get("t", procs[0].exit_time)
    if e != "timeout" or exit_code(th) != 3 or after > 3 + 2.5:
        say(f"ends disconnect: the host ended {e}: {msg} (exit {exit_code(th)}) {after:.1f} s after the guest was "
            f"killed; expected a timeout within 3 s (+ margin)")
        bad = 1
    else:
        say(f"ends disconnect: the guest killed in the race; the host ended with a timeout {after:.1f} s later (exit 3)")
    # the guest leaves at its own limit: BYE, the host ends at once with the guest gone
    r = session(a, "ends.bye", hp, gp, spec, guest_extra=["--frames", "900"], timeout=300)
    if not r:
        return 1
    th, tg, procs = r
    e, msg = ended(th)
    eg, _ = ended(tg)
    # the host runs on to the guest's last retrace, and at most the guest's delay past it (the inputs it had before
    # the BYE came: the guest assigned them that far ahead)
    last = [max([int(x) for x in re.findall(r"^rush2049pc: physics vi=(\d+):", t, re.M)] or [-1]) for t in (th, tg)]
    d = re.search(r"delay \d+ \(peer \d+, \d+-(\d+),", tg)
    ahead = int(d.group(1)) if d else 1
    if not last[1] <= last[0] <= last[1] + ahead:
        say(f"ends bye: the host's last retrace is {last[0]}, the guest's {last[1]} (its delay up to {ahead})")
        bad = 1
    if e != "peer-left" or eg != "local" or exit_code(th) or exit_code(tg) or "frames reached:" not in tg:
        say(f"ends bye: host {e} ({msg}, exit {exit_code(th)}), guest {eg} (exit {exit_code(tg)}); expected "
            f"peer-left and local, both exit 0")
        bad = 1
    else:
        gap = procs[0].exit_time - procs[1].exit_time
        say(f"ends bye: the guest left at its 900th graphics task; the host ended too (peer-left, exit 0), "
            f"{abs(gap):.2f} s {'after' if gap >= 0 else 'before'} the guest's process exited, "
            f"{last[0] - last[1]} retrace(s) past the guest's last (its delay {ahead})")
    # nobody there: the guest gives up
    # (--window without a display: the message boxes a player would see are printed; it never gets to its window)
    port = free_udp_port()
    run_dir = Path(a.logs) / "ends.noanswer.run"
    run_dir.mkdir(parents=True, exist_ok=True)
    e = boxes_env(run_dir)
    t0 = time.monotonic()
    p = subprocess.run(base_cmd(a, gp, spec, ["--net-bind", "127.0.0.1", "--net-join", f"127.0.0.1:{port}",
                                              "--net-timeout", "2", "--window"]), env=e, cwd=run_dir,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    took = time.monotonic() - t0
    out = p.stdout.decode(errors="replace")
    (Path(a.logs) / "ends.noanswer.log").write_text(out)
    box = box_text(out)
    if "(no-answer)" not in out or p.returncode != 3 or took > 10 or \
            not box.startswith(f"Couldn't join the online race at 127.0.0.1: no answer from 127.0.0.1:{port}"):
        say(f"ends no-answer: exit {p.returncode} after {took:.1f} s; its box: {box!r}; the log: "
            f"{[l for l in out.splitlines() if 'online' in l][-3:]}")
        bad = 1
    else:
        say(f"ends no-answer: a guest joining a port nobody listens on ends with no-answer after {took:.1f} s (exit 3), "
            f"its box: {box!r}")
    return bad


def boxes_env(run_dir):
    """A run's environment that prints its message boxes (RUSH2049_MESSAGE_BOX=print), with no display to open a
    window on (--window then renders headless, and the boxes are a window's)."""
    e = dict(os.environ, RUSH2049_MESSAGE_BOX="print", **synth_paths.data_env(run_dir))
    for k in ("DISPLAY", "WAYLAND_DISPLAY", "RUSH_SIM_NET", "RUSH_SIM_DESYNC", "RUSH_SIM_GATE",
              "RUSH2049_TEST_NET_BUILD", "RUSH2049_TEST_NET_WAIT_SHOT", "RUSH2049_TEST_NET_HOST_WAIT"):
        e.pop(k, None)
    return e


def box_text(out):
    """The text of the first message box a run printed ("" for none)."""
    m = re.search(r"^rush2049pc: message box: [^\n]*\n(.*?)\nrush2049pc: \(end of message box\)", out, re.M | re.S)
    return m.group(1) if m else ""


def mode_wait(a):
    bad = 0
    spec = synth_paths.read_script(SCRIPTS / "race2p.press")
    hp, gp, mp, two = path_files(a, "race2p", "wait")
    logs = Path(a.logs)
    # the screen, saved, then cancelled as Escape would
    shot = logs / "wait.screen.png"
    if shot.exists():
        shot.unlink()
    run_dir = logs / "wait.screen.run"
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    e = boxes_env(run_dir)
    e["RUSH2049_TEST_NET_WAIT_SHOT"] = str(shot)
    t0 = time.monotonic()
    p = subprocess.run(base_cmd(a, hp, spec, ["--net-bind", "127.0.0.1", "--net-host", "0", "--size", "1280x720"]),
                       env=e, cwd=run_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    took = time.monotonic() - t0
    out = p.stdout.decode(errors="replace")
    (logs / "wait.screen.log").write_text(out)
    for want in ("online: this PC's addresses: ", "online: test: the waiting screen saved to",
                 "online: hosting cancelled", "online: ended (local): hosting cancelled"):
        if want not in out:
            say(f"wait: the host doesn't say '{want}'")
            bad = 1
    if p.returncode != 0 or "message box" in out or "physics vi=" in out:
        say(f"wait: cancelled, the host exits {p.returncode}{', with a message box' if 'message box' in out else ''}"
            f"{', after running the game' if 'physics vi=' in out else ''}; expected 0, none, before the game")
        bad = 1
    if not shot.exists():
        say("wait: no screenshot")
        return 1
    w, h, rows = synth_shot.read_png(str(shot))
    panel = sum(1 for y in range(0, h, 8) for x in range(0, w, 8) if rows[y][x] == (0x0C, 0x10, 0x18))
    white = sum(1 for y in range(h) for x in range(w) if rows[y][x] == (255, 255, 255))
    blue = sum(1 for y in range(h) for x in range(w) if rows[y][x] == (0x8C, 0xC8, 0xFF))
    corner = rows[2][2]
    # the panel 408 x 236 units at 3 pixels a unit, centered on black; white text (the lines), blue (the addresses)
    if (w, h) != (1280, 720) or corner != (0, 0, 0) or not 0.6 < panel * 64 / (1224 * 708) < 1.0 or white < 2000:
        say(f"wait: the screenshot is {w} x {h}, its corner {corner}, {panel * 64} pixels of panel (of 1224 x 708), "
            f"{white} white, {blue} of addresses")
        bad = 1
    if not bad:
        addrs = re.search(r"online: this PC's addresses: (.*)", out).group(1)
        say(f"wait: the waiting screen drawn ({w} x {h}: the panel, {white} pixels of white text, {blue} of addresses: "
            f"{addrs[:160]}), saved to {shot}, then cancelled: exit 0, no box, {took:.1f} s")
    # nobody joins: the host gives up, and its box says so
    run_dir = logs / "wait.giveup.run"
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    e = boxes_env(run_dir)
    e["RUSH2049_TEST_NET_HOST_WAIT"] = "2"
    t0 = time.monotonic()
    p = subprocess.run(base_cmd(a, hp, spec, ["--net-bind", "127.0.0.1", "--net-host", "0", "--window"]), env=e,
                       cwd=run_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    took = time.monotonic() - t0
    out = p.stdout.decode(errors="replace")
    (logs / "wait.giveup.log").write_text(out)
    box = box_text(out)
    if p.returncode != 3 or not box.startswith("Nobody joined the online race in 2 seconds") or took > 30 or \
            "physics vi=" in out:
        say(f"wait: nobody joining, the host exits {p.returncode} after {took:.1f} s, its box {box!r}; expected 3 "
            f"after 2 s, 'Nobody joined...'")
        bad = 1
    else:
        say(f"wait: nobody joined in 2 s (RUSH2049_TEST_NET_HOST_WAIT): the host gave up after {took:.1f} s, exit 3, "
            f"its box: {box!r}")
    return bad


def mode_view(a):
    spec = synth_paths.read_script(SCRIPTS / "race2p.press")
    bad, tone = 0, None
    for own_role in ("host", "guest"):
        tag = f"view.{own_role}"
        hp, gp, mp, two = path_files(a, "race2p", tag)
        extra = {r: ["--net-delay", "2"] + (["--set", "net.view=own"] if r == own_role else []) for r in ("host", "guest")}
        r = session(a, tag, hp, gp, spec, extra["host"], extra["guest"])
        if not r:
            bad = 1
            continue
        th, tg, _ = r
        if tone is None:
            tone = one(a, "view.one", mp, spec, ["--input-delay", "2", "--session-rules"])
        b = compare_session(f"view ({own_role}'s own)", th, tg, tone)
        texts = {"host": th, "guest": tg}
        player = 1 if own_role == "host" else 2
        shares = {}
        for role, t in texts.items():
            if exit_code(t) != 0:
                say(f"view: the {role} exits {exit_code(t)}: {ended(t)}")
                b = 1
            said = re.search(r"online view: player (\d)'s view alone", t)
            if (role == own_role) != bool(said) or (said and int(said.group(1)) != player):
                say(f"view: the {role} says {said.group(0) if said else 'nothing of its view'}; expected "
                    f"{f'player {player}' if role == own_role else 'nothing (split)'}")
                b = 1
            shot = Path(a.logs) / f"{tag}.run" / f"{role}.cwd" / "race2p.png"
            if not shot.exists():
                say(f"view: no screenshot from the {role}")
                b = 1
                continue
            lines = []
            fails = synth_shot.check(str(shot), "own" if role == own_role else 2, say=lines.append)
            shares[role] = ", ".join(l.split(": ", 1)[1] for l in lines)
            for f in fails:
                say(f"view: the {role}'s screenshot: {f}")
                b = 1
        if not b:
            other = "guest" if own_role == "host" else "host"
            say(f"view: the {own_role} own (player {player}'s view), the {other} split: host = guest = one process, "
                f"{len(replay_check.trace(th))} lines, {len(hash_compare.hashes(th))} retraces hashed; the {own_role}'s "
                f"screenshot one view across the frame ({shares[own_role]}), the {other}'s two views ({shares[other]})")
        bad |= b
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("mode", choices=["path", "sim", "auto", "paced", "fuzz", "desync", "mismatch", "adopt", "ends",
                                     "wait", "view"])
    ap.add_argument("names", nargs="*")
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/pc/synth/net"))
    ap.add_argument("--delay", type=int, default=0, help="path: the fixed delay (default 2)")
    ap.add_argument("--model", default="wan80", choices=sorted(MODELS))
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--first", type=int, default=1)
    ap.add_argument("--seconds", type=int, default=120, help="fuzz: game time per seed")
    ap.add_argument("--at", type=int, default=1000)
    ap.add_argument("--address", default="8011735C", help="desync: the word to change (the random seed)")
    ap.add_argument("--mods", help="the mods paths' mods (ctest's mods_synth: build/pc/synth/paths/mods)")
    a = ap.parse_args()
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    a.logs = str(Path(a.logs).resolve())
    replay_check.MODS_ROOT = str(Path(a.mods).resolve()) if a.mods else \
        str(Path(a.logs).resolve().parent / "paths" / "mods")
    a.mods = str(Path(a.mods or replay_check.MODS_ROOT).resolve())
    if a.mode == "path":
        sys.exit(1 if sum(mode_path(a, n) for n in a.names or ["race2p"]) else 0)
    fn = {"sim": lambda: mode_sim(a, a.names), "auto": lambda: mode_auto(a, a.names), "fuzz": lambda: mode_fuzz(a),
          "paced": lambda: mode_paced(a, a.names),
          "desync": lambda: mode_desync(a, (a.names or ["race2p"])[0]), "mismatch": lambda: mode_mismatch(a),
          "adopt": lambda: mode_adopt(a), "ends": lambda: mode_ends(a), "wait": lambda: mode_wait(a),
          "view": lambda: mode_view(a)}
    sys.exit(1 if fn[a.mode]() else 0)


if __name__ == "__main__":
    main()
