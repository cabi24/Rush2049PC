#!/usr/bin/env python3
"""port/net's two-process tests (docs/port/NETWORK.md 8): two net_peer programs, a host and a guest, run the toy
simulation over UDP on 127.0.0.1 in real time, each with its own RUSH_SIM_NET model in its receive path.

    net_check.py --exe build/pc/port/net/net_peer loopback --model wan80   both peers' hash at every tick the same;
                                                                           the delay, stalls, the rate
    net_check.py --exe ... desync        the guest changes its state at tick 300: both end with DESYNC at 300
    net_check.py --exe ... disconnect    the guest is killed: the host ends with TIMEOUT within its timeout (3 s)
    net_check.py --exe ... bye           the guest quits at tick 240: the host ends at once (PEER_LEFT)
    net_check.py --exe ... sigstop       the guest is stopped (SIGSTOP) for 3 s and continued (Linux): the run
                                         completes with the same hashes, the delay not blown up by the stop
    net_check.py --exe ... soak --model bad150 --ticks 36000   a longer run of any model (a script, not ctest)

Rejoins and recovery (docs/port/NETWORK.md 12), each with a rejoin window of 60 s:
    net_check.py --exe ... rejoin        the guest killed at tick 300 (SIGKILL, by its PID); 3 s later a new guest
                                         rejoins the session by its id, replays the host's log and goes on: the same
                                         hash at every tick to the end
    net_check.py --exe ... rejoin-host   the same with the host killed (the guest knocks; a new host on the same port
                                         rejoins)
    net_check.py --exe ... outage        the link down 20 s (every packet lost both ways), then a stopped process
                                         (SIGSTOP, Linux) for 20 s: both wait and go on, the same hashes, the delay small
    net_check.py --exe ... recover       the guest's state changed once at tick 300: the host has it rebuild (its game
                                         ends "recover"), a new guest replays the log past 300 and the run completes
    net_check.py --exe ... recover-repeat   the change repeats in the rebuilt game: both end with a desync at 300,
                                         "deterministic"
"""
import argparse
import os
import socket
import signal
import subprocess
import sys
import threading
import time

# Real-time versions of net_test's models (shorter: the changes come sooner).
MODELS = {
    "clean": ("", "", 420, True),
    "lan20": ("delay=20", "delay=20", 420, True),
    "wan80": ("delay=80,jitter=20", "delay=80,jitter=20", 480, True),
    "bad150": ("delay=150,jitter=40,loss=10,dup=2,reorder=2", "delay=150,jitter=40,loss=10,dup=2,reorder=2", 600,
               False),
    "shift": ("delay=10,at=3,delay=200,at=7,delay=30", "delay=10,at=3,delay=200,at=7,delay=30", 720, False),
    "pause": ("delay=20", "delay=20,pause=4:3", 600, False),
    "burst": ("delay=30,jitter=5,burst=3:0.5:30", "delay=30,jitter=5,burst=3:0.5:30", 480, False),
}


class Peer:
    """One net_peer, its output read by a thread: lines kept, the port and the ticks noted."""

    def __init__(self, name, args, log):
        self.name, self.lines, self.port, self.tick = name, [], None, 0
        self.result, self.ended, self.exit_time = None, None, None
        self.log = log
        self.got_port = threading.Event()
        self.proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self):
        for line in self.proc.stdout:
            line = line.rstrip("\n")
            self.lines.append(line)
            self.log.write(f"[{self.name}] {line}\n")
            if line.startswith("port "):
                self.port = int(line.split()[1])
                self.got_port.set()
            elif line.startswith("tick "):
                self.tick = int(line.split()[1])
            elif line.startswith("result "):
                self.result = dict(kv.split("=", 1) for kv in line[7:].split(" ") if "=" in kv)
            elif line.startswith("ended "):
                self.ended = line[6:]
        self.exit_time = time.monotonic()
        self.got_port.set()

    def wait(self, timeout):
        try:
            self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            raise SystemExit(f"net_check: {self.name} didn't finish in {timeout} s")
        self.reader.join(5)


def start_pair(a, work, tag, host_sim, guest_sim, host_extra=(), guest_extra=()):
    os.makedirs(work, exist_ok=True)
    log = open(os.path.join(work, f"{tag}.log"), "w")
    hh, gh = os.path.join(work, f"{tag}.host.hashes"), os.path.join(work, f"{tag}.guest.hashes")
    for f in (hh, gh):
        if os.path.exists(f):
            os.remove(f)
    env = dict(os.environ)
    env.pop("RUSH_SIM_NET", None)
    host = Peer("host", [a.exe, "--host", "--bind", "127.0.0.1", "--port", "0", "--sim", host_sim, "--hashes", hh,
                         *host_extra], log)
    if not host.got_port.wait(10) or host.port is None:
        host.proc.kill()
        raise SystemExit("net_check: the host didn't say its port:\n" + "\n".join(host.lines))
    guest = Peer("guest", [a.exe, "--join", f"127.0.0.1:{host.port}", "--bind", "127.0.0.1", "--sim", guest_sim,
                           "--hashes", gh, *guest_extra], log)
    return host, guest, hh, gh, log


def read_hashes(path):
    if not os.path.exists(path):
        return []
    with open(path) as f:
        return [line.split()[1] for line in f if line.strip()]


def summary(p):
    r = p.result or {}
    return (f"{p.name}: end {r.get('end', '?')}, {r.get('ticks', '?')} ticks, {r.get('rate', '?')} a second, "
            f"stalls {r.get('stalls', '?')} ({r.get('stall_ms', '?')} ms, max {r.get('stall_max', '?')}), delay "
            f"{r.get('delay_min', '?')}-{r.get('delay_max', '?')} ({r.get('changes', '?')} changes), checks "
            f"{r.get('checks', '?')}, sync {r.get('sync', '?')}, delay each second {r.get('trace', '?')}")


def fail(msg, log_path):
    print(f"FAIL: {msg}")
    print(f"(the peers' output: {log_path})")
    sys.exit(1)


def case_loopback(a, model, ticks=None, soak=False):
    hs, gs, n, rate_check = MODELS[model]
    n = ticks or n
    print(f"loopback {model}: host RUSH_SIM_NET '{hs}', guest '{gs}', {n} ticks over 127.0.0.1")
    t0 = time.monotonic()
    host, guest, hh, gh, log = start_pair(a, a.work, f"loopback_{model}", hs, gs, ("--ticks", str(n)),
                                          ("--ticks", str(n)))
    limit = n / 60 * 2 + 60
    guest.wait(limit)
    host.wait(limit)
    log.close()
    lp = log.name
    print(f"  {time.monotonic() - t0:.1f} s")
    for p in (host, guest):
        print("  " + summary(p))
        if p.proc.returncode != 0 or not p.result:
            fail(f"{p.name} exited {p.proc.returncode}", lp)
    a_h, b_h = read_hashes(hh), read_hashes(gh)
    if len(a_h) != n or len(b_h) != n:
        fail(f"ticks: host {len(a_h)}, guest {len(b_h)}, not {n}", lp)
    diff = next((i for i in range(n) if a_h[i] != b_h[i]), None)
    if diff is not None:
        fail(f"the hashes differ from tick {diff}", lp)
    print(f"  the same hash at all {n} ticks on both")
    for p in (host, guest):
        r = p.result
        if r["end"] not in ("none", "peer-left", "local"):
            fail(f"{p.name} ended {r['end']}: {p.ended}", lp)
        if int(r["checks"]) < n - 30:
            fail(f"{p.name} checked only {r['checks']} hashes", lp)
        if rate_check and float(r["rate"]) < 59.9:
            fail(f"{p.name} ran {r['rate']} ticks a second", lp)
        if model == "pause" and int(r["delay_max"]) > 6:
            fail(f"{p.name}'s delay rose to {r['delay_max']} for a stopped peer", lp)
    print("ok")


def case_desync(a):
    print("desync: the guest flips a bit of its state after tick 300")
    host, guest, hh, gh, log = start_pair(a, a.work, "desync", "delay=30,jitter=5", "delay=30,jitter=5",
                                          ("--ticks", "1200"), ("--ticks", "1200", "--desync-at", "300"))
    guest.wait(60)
    host.wait(60)
    log.close()
    for p in (host, guest):
        print("  " + summary(p))
        r = p.result or {}
        if r.get("end") != "desync" or r.get("detail") != "300":
            fail(f"{p.name} ended {r.get('end')} at {r.get('detail')}: {p.ended}", log.name)
        print(f"  {p.name}: {p.ended}")
        if int(r["ticks"]) > 300 + 60:
            fail(f"{p.name} ran on to tick {r['ticks']}", log.name)
    print("ok")


def case_disconnect(a):
    print("disconnect: the guest is killed at tick 180; the host's timeout is 3 s")
    host, guest, hh, gh, log = start_pair(a, a.work, "disconnect", "delay=20", "delay=20",
                                          ("--ticks", "100000", "--timeout", "3000"),
                                          ("--ticks", "100000", "--timeout", "3000"))
    t = time.monotonic()
    while guest.tick < 180 and time.monotonic() - t < 30 and guest.proc.poll() is None:
        time.sleep(0.01)
    if guest.tick < 180:
        fail("the guest never reached tick 180", log.name)
    guest.proc.kill()
    killed = time.monotonic()
    guest.wait(10)
    host.wait(20)
    log.close()
    took = (host.exit_time or time.monotonic()) - killed
    print("  " + summary(host))
    r = host.result or {}
    if r.get("end") != "timeout":
        fail(f"the host ended {r.get('end')}: {host.ended}", log.name)
    print(f"  the host ended {took:.2f} s after the kill: {host.ended}")
    if not 2.9 <= took <= 5.0:
        fail(f"the host took {took:.2f} s", log.name)
    print("ok")


def case_bye(a):
    print("bye: the guest quits after tick 240")
    host, guest, hh, gh, log = start_pair(a, a.work, "bye", "delay=20", "delay=20", ("--ticks", "100000"),
                                          ("--ticks", "100000", "--quit-at", "240", "--linger", "0"))
    guest.wait(30)
    quit_at = guest.exit_time
    host.wait(30)
    log.close()
    for p in (host, guest):
        print("  " + summary(p))
    r = host.result or {}
    if r.get("end") != "peer-left":
        fail(f"the host ended {r.get('end')}: {host.ended}", log.name)
    took = host.exit_time - quit_at
    print(f"  the host ended {took:.2f} s after the guest exited: {host.ended}")
    if took > 1.0:
        fail(f"the host took {took:.2f} s", log.name)
    if int(r["ticks"]) > 240 + 30:
        fail(f"the host ran on to tick {r['ticks']}", log.name)
    print("ok")


def case_sigstop(a):
    if not hasattr(signal, "SIGSTOP"):
        print("sigstop: no SIGSTOP here")
        return
    print("sigstop: the guest process is stopped for 3 s at tick 240, then continued")
    host, guest, hh, gh, log = start_pair(a, a.work, "sigstop", "delay=20", "delay=20", ("--ticks", "720"),
                                          ("--ticks", "720"))
    t = time.monotonic()
    while guest.tick < 240 and time.monotonic() - t < 30:
        time.sleep(0.005)
    os.kill(guest.proc.pid, signal.SIGSTOP)
    time.sleep(3)
    os.kill(guest.proc.pid, signal.SIGCONT)
    guest.wait(60)
    host.wait(60)
    log.close()
    for p in (host, guest):
        print("  " + summary(p))
        r = p.result or {}
        if r.get("end") not in ("none", "peer-left", "local") or r.get("ticks") != "720":
            fail(f"{p.name} ended {r.get('end')} at {r.get('ticks')}", log.name)
        if int(r["delay_max"]) > 6:
            fail(f"{p.name}'s delay rose to {r['delay_max']}", log.name)
    a_h, b_h = read_hashes(hh), read_hashes(gh)
    if a_h != b_h or len(a_h) != 720:
        fail("the hashes differ", log.name)
    print("  the same hash at all 720 ticks on both; the host stalled through the stop")
    print("ok")


WINDOW = ("--window", "60000")


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def wait_tick(p, tick, limit=60):
    t = time.monotonic()
    while p.tick < tick and time.monotonic() - t < limit and p.proc.poll() is None:
        time.sleep(0.005)
    return p.tick >= tick


def session_of(p):
    for line in p.lines:
        if line.startswith("session "):
            return line.split()[1]
    return None


def compare(a_path, b_path, n, log_path, what):
    a_h, b_h = read_hashes(a_path), read_hashes(b_path)
    if len(a_h) != n or len(b_h) != n:
        fail(f"{what}: ticks {len(a_h)} and {len(b_h)}, not {n}", log_path)
    diff = next((i for i in range(n) if a_h[i] != b_h[i]), None)
    if diff is not None:
        fail(f"{what}: the hashes differ from tick {diff}", log_path)
    print(f"  {what}: the same hash at all {n} ticks")


def case_rejoin(a, host_side=False):
    ticks, kill_at = 900, 300
    who = "host" if host_side else "guest"
    print(f"rejoin: the {who} is killed at tick {kill_at}; 3 s later a new {who} rejoins the session")
    port = free_port()
    host, guest, hh, gh, log = start_pair(a, a.work, f"rejoin_{who}", "delay=20,jitter=5", "delay=20,jitter=5",
                                          ("--ticks", str(ticks), "--port", str(port), *WINDOW),
                                          ("--ticks", str(ticks), *WINDOW))
    victim, keeper = (host, guest) if host_side else (guest, host)
    if not wait_tick(victim, kill_at):
        fail(f"the {who} never reached tick {kill_at}", log.name)
    sid = session_of(keeper)
    if not sid:
        fail("no session id said", log.name)
    victim.proc.kill()   # SIGKILL by its own PID
    killed = time.monotonic()
    victim.wait(10)
    time.sleep(3)
    nh = os.path.join(a.work, f"rejoin_{who}.new.hashes")
    if os.path.exists(nh):
        os.remove(nh)
    env_sim = ("--sim", "delay=20,jitter=5")
    if host_side:
        args = [a.exe, "--host", "--bind", "127.0.0.1", "--port", str(port), *env_sim, "--hashes", nh, "--ticks",
                str(ticks), "--rejoin", sid, *WINDOW]
    else:
        args = [a.exe, "--join", f"127.0.0.1:{port}", "--bind", "127.0.0.1", *env_sim, "--hashes", nh, "--ticks",
                str(ticks), "--rejoin", sid, *WINDOW]
    new = Peer(f"new {who}", args, log)
    t0 = time.monotonic()
    new.wait(120)
    keeper.wait(120)
    log.close()
    for p in (keeper, new):
        print("  " + summary(p))
        r = p.result or {}
        if r.get("end") not in ("none", "peer-left", "local") or r.get("ticks") != str(ticks):
            fail(f"{p.name} ended {r.get('end')} at {r.get('ticks')}: {p.ended}", log.name)
    r = new.result
    if int(r["rejoins"]) != 1 or int(keeper.result["rejoins"]) != 1:
        fail("the rejoin isn't counted on both", log.name)
    compare(gh if host_side else hh, nh, ticks, log.name, f"the {'guest' if host_side else 'host'} and the new {who}")
    print(f"  the new {who} replayed {r['replayed']} ticks in {float(r['replay_ms']):.1f} ms; the {keeper.name} waited "
          f"{float(keeper.result['stall_max']) / 1000:.1f} s at tick {r['replayed']} (from the kill {killed - t0:+.1f} s "
          f"before the new {who} started, to its first live input)")
    print("ok")


def case_rejoin_host(a):
    case_rejoin(a, True)


def case_outage(a):
    print("outage: the link down 20 s from 4 s (every packet lost both ways), the rejoin window 60 s")
    sim = "delay=20,jitter=5,at=4,loss=100,at=24,loss=0"
    host, guest, hh, gh, log = start_pair(a, a.work, "outage", sim, sim, ("--ticks", "2100", *WINDOW),
                                          ("--ticks", "2100", *WINDOW))
    guest.wait(120)
    host.wait(120)
    for p in (host, guest):
        print("  " + summary(p))
        r = p.result or {}
        if r.get("end") not in ("none", "peer-left", "local") or r.get("ticks") != "2100":
            fail(f"{p.name} ended {r.get('end')} at {r.get('ticks')}: {p.ended}", log.name)
        if float(r["stall_max"]) < 19000 or int(r["delay_max"]) > 4:
            fail(f"{p.name}: the longest stall {r['stall_max']} ms, the delay up to {r['delay_max']}", log.name)
    compare(hh, gh, 2100, log.name, "host and guest through the outage")
    log.close()
    if not hasattr(signal, "SIGSTOP"):
        print("ok")
        return
    print("outage: the guest process stopped 20 s (SIGSTOP) at tick 240, then continued")
    host, guest, hh, gh, log = start_pair(a, a.work, "stop20", "delay=20", "delay=20", ("--ticks", "900", *WINDOW),
                                          ("--ticks", "900", *WINDOW))
    if not wait_tick(guest, 240):
        fail("the guest never reached tick 240", log.name)
    os.kill(guest.proc.pid, signal.SIGSTOP)
    time.sleep(20)
    os.kill(guest.proc.pid, signal.SIGCONT)
    guest.wait(120)
    host.wait(120)
    log.close()
    for p in (host, guest):
        print("  " + summary(p))
        r = p.result or {}
        if r.get("end") not in ("none", "peer-left", "local") or r.get("ticks") != "900":
            fail(f"{p.name} ended {r.get('end')} at {r.get('ticks')}", log.name)
        if int(r["delay_max"]) > 6:
            fail(f"{p.name}'s delay rose to {r['delay_max']}", log.name)
    compare(hh, gh, 900, log.name, "host and guest through the stop")
    print("ok")


def case_recover(a, repeat=False):
    ticks = 900
    print(f"recover{'-repeat' if repeat else ''}: the guest's state changes at tick 300; recover on; the rebuilt guest "
          f"{'changes it again' if repeat else 'does not'}")
    host, guest, hh, gh, log = start_pair(a, a.work, f"recover{'_repeat' if repeat else ''}", "delay=20,jitter=5",
                                          "delay=20,jitter=5", ("--ticks", str(ticks), "--recover", *WINDOW),
                                          ("--ticks", str(ticks), "--recover", "--desync-at", "300", *WINDOW))
    guest.wait(60)
    print("  " + summary(guest))
    r = guest.result or {}
    if r.get("end") != "recover" or r.get("detail") != "300":
        fail(f"the guest ended {r.get('end')} at {r.get('detail')}: {guest.ended}", log.name)
    sid = session_of(guest)
    nh = os.path.join(a.work, f"recover{'_repeat' if repeat else ''}.new.hashes")
    if os.path.exists(nh):
        os.remove(nh)
    new = Peer("new guest", [a.exe, "--join", f"127.0.0.1:{host.port}", "--bind", "127.0.0.1", "--sim",
                             "delay=20,jitter=5", "--hashes", nh, "--ticks", str(ticks), "--rejoin", sid, "--recover",
                             *WINDOW, *(("--desync-at", "300") if repeat else ())], log)
    new.wait(120)
    host.wait(120)
    log.close()
    for p in (host, new):
        print("  " + summary(p))
        if p.ended:
            print(f"  {p.name}: {p.ended}")
    if repeat:
        for p in (host, new):
            r = p.result or {}
            if r.get("end") != "desync" or r.get("detail") != "300":
                fail(f"{p.name} ended {r.get('end')} at {r.get('detail')}", log.name)
        if "deterministic" not in (new.ended or ""):
            fail("the rebuilt guest doesn't say the desync is deterministic", log.name)
    else:
        for p in (host, new):
            r = p.result or {}
            if r.get("end") not in ("none", "peer-left", "local") or r.get("ticks") != str(ticks):
                fail(f"{p.name} ended {r.get('end')} at {r.get('ticks')}", log.name)
        if int(host.result["recoveries"]) != 1 or int(new.result["recoveries"]) != 1:
            fail("the recovery isn't counted on both", log.name)
        compare(hh, nh, ticks, log.name, "the host and the rebuilt guest")
    print("ok")


def case_recover_repeat(a):
    case_recover(a, True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, help="net_peer")
    ap.add_argument("--work", default="net-check", help="where the logs and hashes go")
    ap.add_argument("case", choices=["loopback", "desync", "disconnect", "bye", "sigstop", "soak", "rejoin", "rejoin-host",
                                     "outage", "recover", "recover-repeat"])
    ap.add_argument("--model", default="wan80", choices=sorted(MODELS))
    ap.add_argument("--ticks", type=int)
    a = ap.parse_args()
    a.work = os.path.abspath(a.work)
    if a.case in ("loopback", "soak"):
        case_loopback(a, a.model, a.ticks, a.case == "soak")
    else:
        globals()["case_" + a.case.replace("-", "_")](a)


if __name__ == "__main__":
    main()
