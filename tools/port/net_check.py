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
"""
import argparse
import os
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, help="net_peer")
    ap.add_argument("--work", default="net-check", help="where the logs and hashes go")
    ap.add_argument("case", choices=["loopback", "desync", "disconnect", "bye", "sigstop", "soak"])
    ap.add_argument("--model", default="wan80", choices=sorted(MODELS))
    ap.add_argument("--ticks", type=int)
    a = ap.parse_args()
    a.work = os.path.abspath(a.work)
    if a.case in ("loopback", "soak"):
        case_loopback(a, a.model, a.ticks, a.case == "soak")
    else:
        globals()["case_" + a.case](a)


if __name__ == "__main__":
    main()
