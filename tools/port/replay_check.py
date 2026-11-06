#!/usr/bin/env python3
"""Input record, replay and delay, the pads gate and the state hash, along the synthetic paths (docs/port/LOCKSTEP.md 8).

    replay_check.py same     --exe EXE --rom ROM --logs DIR PATH
    replay_check.py delay    --exe EXE --rom ROM --logs DIR PATH [--delay 2] [--schedule 2:0,4:600,1:1200]
    replay_check.py gate     --exe EXE --rom ROM --logs DIR PATH [--sim 7,0.2,15]
    replay_check.py mutation --exe EXE --rom ROM --logs DIR PATH [--at 1000]
    replay_check.py aslr     --exe EXE --rom ROM --logs DIR PATH [--full 1]
    replay_check.py file     --exe EXE --rom ROM --logs DIR PATH
    replay_check.py replay-dir --exe EXE --rom ROM --logs DIR --from DIR [PATH ...]

Each run is the path as synth_paths.py runs it (tools/port/synth/PATH.press, its until, seconds, args and env), on the
virtual clock, in a folder of its own, with exact floats (RUSH_STATE_EXACT), a status line each second, the cars'
physics every retrace (--log-physics 1), the game's pads at each change (--log-pads) and the state hash every retrace
(--log-hash 1,W: the window and own storage every W). Its trace: those lines and the state log's, compared exactly
(each exact float by its value, as synth_compare.py's exact_floats reads them).

same: a run that records its pads (--record-inputs), then a run that replays them (--replay-inputs): the same trace and
  the same hashes. The two are processes of their own, so with ASLR their images are apart (said). The recording and
  the replay's log stay in --logs (PATH.pads, PATH.replay.log) for replay-dir on another OS.
delay: the path recorded with no delay (A), with --input-delay D (B) and with a schedule (C). B's pads are A's D
  retraces later, the first D idle, and C's are what the delay line's rule (NETPLAY.md 11.2: a slot skipped as the
  delay grows gets the last pads again, a sample whose slot is taken as it shrinks is dropped) makes of A's: the pads
  log says so retrace by retrace. And the game: B's run is that of the press script moved D retraces later, line for
  line (a press's effects come exactly D retraces later); C's recording replays to C's trace.
gate: the path without the gate, and with RUSH_SIM_GATE (its pads held back at random retraces for random real
  times, released by a thread of its own): the same trace and hashes.
mutation: the path, and the path with RUSH_SIM_DESYNC flipping the random seed at --at (then a window word nothing
  uses): hash_compare.py names that retrace as the first difference, and the hashes differ from it on.
aslr: two runs with the full hash at every --full retraces: the same hashes, with the images apart.
file: the file cut short replays what it has, then says so; a damaged header and a recording of other settings are
  refused (the latter with what differs), and --replay-force replays it.
replay-dir: the recordings of another run of `same` (CI: Linux's), replayed here: the replay's trace and hashes are
  that run's replay log's (CI's windows-replay, docs/port/WINDOWS.md 12).

Exit status: 0 when it holds. Stdlib only, Python 3.9+.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import hash_compare  # noqa: E402
import synth_compare  # noqa: E402
import synth_paths  # noqa: E402

VI_NS = 1000000000 // 60   # a retrace of the virtual clock (port/ultra/loop.c)
MODS_ROOT = None           # --mods: the mods paths' mods (ctest's mods_synth fixture)
SCRIPTS = synth_paths.PRESS_DIR   # --scripts: where PATH.press is (tools/port/retail for the retail cartridge's)
_KEEP = re.compile(r"^rush2049pc: (game state |status |state reached:|time reached:|frames reached:|physics vi=|hash vi=|"
                   r"pads vi=)")
_IMAGE = re.compile(r"^rush2049pc: hash: the program's image at (0x[0-9a-f]+)")


_COUNTS = re.compile(r" gfx_tasks=\d+ audio_tasks=\d+|, after \d+ graphics tasks")


def trace(text):
    """The lines a run's trace is made of, each exact float by its value, without the task counts (as synth_compare.py
    leaves them out: the states, retraces, physics and hashes say what the game did)."""
    return [_COUNTS.sub("", synth_compare.exact_floats(l.rstrip("\r"))) for l in text.splitlines() if _KEEP.match(l)]


def first_diff(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i, x, y
    if len(a) != len(b):
        i = min(len(a), len(b))
        return i, a[i] if i < len(a) else "(the run ended)", b[i] if i < len(b) else "(the run ended)"
    return None


def run(exe, rom, logs, name, tag, extra=(), env=None, full=30, press=None, hashes=True, log_pads=True):
    """One run of path `name` (or its press file `press`); its log text, written to logs/name.tag.log."""
    spec = synth_paths.read_script(SCRIPTS / f"{name}.press")
    press = press or SCRIPTS / f"{name}.press"
    # the mods paths' --mods-dir mods/...: the folder ctest's mods_synth makes beside the paths' logs (MODS.md 8)
    args = [str(Path(MODS_ROOT) / x[len("mods/"):]) if x.startswith("mods/") and MODS_ROOT else x
            for x in spec["args"]]
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--no-rom-check", "--headless", "--no-config",
           "--audio", "null", "--no-pak", "--fast", "--no-log", "--press-file", str(Path(press).resolve()),
           "--until-state", spec["until"], "--seconds", spec["seconds"], "--status", "1", "--log-physics", "1"] + \
        (["--log-pads"] if log_pads else []) + (["--log-hash", f"1,{full}"] if hashes else []) + args + list(extra)
    e = dict(os.environ, RUSH_STATE_EXACT="1", **synth_paths.data_env(logs))
    e.pop("RUSH_SIM_GATE", None), e.pop("RUSH_SIM_DESYNC", None)
    e.update(spec["env"])
    e.update(env or {})
    with tempfile.TemporaryDirectory(dir=logs) as cwd:
        t0 = time.time()
        p = subprocess.run(cmd, cwd=cwd, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
        took = time.time() - t0
    text = p.stdout.decode(errors="replace")
    (Path(logs) / f"{name}.{tag}.log").write_text(text, encoding="utf-8")
    if p.returncode:
        text += f"\nrush2049pc: state reached: exit {p.returncode}\n"   # in the trace, so it differs
    return text, took


def image(text):
    m = [_IMAGE.match(l) for l in text.splitlines()]
    m = [x.group(1) for x in m if x]
    return m[0] if m else None


def judge(what, ta, tb, la, lb):
    d = first_diff(ta, tb)
    if d:
        i, x, y = d
        print(f"replay_check: {what}: DIFFERS at line {i} of {len(ta)} and {len(tb)} ({la}, {lb})\n    {x}\n    {y}")
        return 1
    return 0


def aslr_note(texts):
    bases = [image(t) for t in texts]
    if None in bases:
        return "no image line"
    if len(set(bases)) == len(bases):
        return f"images apart ({', '.join(bases)})"
    return f"images at the same address ({bases[0]}: no ASLR between these runs)"


# ---------------------------------------------------------------- the modes

def mode_same(a, name):
    pads = Path(a.logs) / f"{name}.pads"
    ra, sa = run(a.exe, a.rom, a.logs, name, "record", ["--record-inputs", str(pads.resolve())], full=a.full)
    rb, sb = run(a.exe, a.rom, a.logs, name, "replay", ["--replay-inputs", str(pads.resolve())], full=a.full)
    ta, tb = trace(ra), trace(rb)
    bad = judge(f"{name} record against replay", ta, tb, f"{name}.record.log", f"{name}.replay.log")
    ha, hb = hash_compare.hashes(ra), hash_compare.hashes(rb)
    reached = synth_paths.read_script(SCRIPTS / f"{name}.press")["expect"]
    ends = [l for l in ta if "reached:" in l]
    if reached and not (ends and reached in ends[-1]):
        print(f"replay_check: {name}: the run doesn't end as the path expects ('{reached}'): {ends[-1] if ends else 'no end'}")
        bad = 1
    if not ha:
        print(f"replay_check: {name}: no hash lines")
        bad = 1
    if not bad:
        full = sum(1 for d in ha.values() if "window" in d)
        print(f"replay_check: {name:10} same: {len(ta)} lines, {len(ha)} retraces hashed ({full} with the window), "
              f"{pads.stat().st_size} bytes of pads; {aslr_note([ra, rb])} ({sa:.1f} s, {sb:.1f} s)")
    return bad


def pads_lines(text):
    """{retrace: 'the four pads'} of a run's --log-pads lines (each a change)."""
    out = {}
    for l in text.splitlines():
        m = re.match(r"^rush2049pc: pads vi=(\d+): (.*)$", l.rstrip("\r"))
        if m:
            out[int(m.group(1))] = m.group(2)
    return out


def expand(changes, last):
    """Pads by retrace, 0..last, from the change lines."""
    out, cur = [], None
    for n in range(last + 1):
        cur = changes.get(n, cur)
        out.append(cur)
    return out


def idle(p):
    """A pads line with no button and the sticks centred (the connected flags kept)."""
    return re.sub(r"(\d) [0-9a-f]{4} [-+]\d+ [-+]\d+", lambda m: f"{m.group(1)} 0000 +0 +0", p)


def delay_model(src, steps):
    """NETPLAY.md 11.2's delay line over pads by retrace: what the game gets at each retrace."""
    def at(n):
        d = 0
        for frm, dd in steps:
            if frm <= n:
                d = dd
        return d
    slots, last, last_p = {}, -1, None
    for n, s in enumerate(src):
        t = n + at(n)
        if t <= last:
            continue   # dropped: its slot is taken
        fill = idle(s) if last < 0 else last_p
        for k in range(last + 1, t):
            slots[k] = fill
        slots[t] = s
        last, last_p = t, s
    return [slots.get(n) for n in range(len(src))]


def shifted_press(name, retraces, out):
    """The path's press script with every press `retraces` retraces later (its time + retraces / 60 s)."""
    lines = []
    for l in (SCRIPTS / f"{name}.press").read_text().splitlines():
        body = l.split("#", 1)[0].split()
        if body and body[0] not in ("pak", "frames") and re.match(r"^\d+(\.\d+)?$", body[0]):
            t = float(body[0]) + retraces * VI_NS / 1e9
            rest = l.split(None, 1)[1] if len(l.split(None, 1)) > 1 else ""
            lines.append(f"{t!r} {rest}")
        else:
            lines.append(l)
    out.write_text("\n".join(lines) + "\n")
    return out


def mode_delay(a, name):
    bad = 0
    pa = Path(a.logs) / f"{name}.nodelay.pads"
    ra, _ = run(a.exe, a.rom, a.logs, name, "nodelay", ["--record-inputs", str(pa.resolve())])
    rb, _ = run(a.exe, a.rom, a.logs, name, f"delay{a.delay}", ["--input-delay", str(a.delay)])
    pc = Path(a.logs) / f"{name}.schedule.pads"
    rc, _ = run(a.exe, a.rom, a.logs, name, "schedule", ["--input-delay", a.schedule, "--record-inputs",
                                                          str(pc.resolve())])
    steps = [(int(x.split(":")[1]) if ":" in x else 0, int(x.split(":")[0])) for x in a.schedule.split(",")]
    # the pads, retrace by retrace: B is A moved D later, C the delay line's rule over A
    last = max(max(pads_lines(t) or [0]) for t in (ra, rb, rc))
    hashed = [max(hash_compare.hashes(t) or [0]) for t in (ra, rb, rc)]
    A, B, C = (expand(pads_lines(t), n) for t, n in zip((ra, rb, rc), hashed))
    n = min(len(A), len(B))
    wantB = delay_model(A, [(0, a.delay)])
    badB = [k for k in range(n) if B[k] != wantB[k]]
    if badB:
        k = badB[0]
        print(f"replay_check: {name}: with --input-delay {a.delay} the pads at retrace {k} are {B[k]}, not A's at "
              f"{k - a.delay}: {wantB[k]}")
        bad = 1
    m = min(len(A), len(C))
    wantC = delay_model(A[:m], steps)
    badC = [k for k in range(m) if C[k] != wantC[k]]
    if badC:
        k = badC[0]
        print(f"replay_check: {name}: with --input-delay {a.schedule} the pads at retrace {k} are {C[k]}, not "
              f"{wantC[k]} (the delay line's rule)")
        bad = 1
    presses = sum(1 for k in range(1, n) if A[k] != A[k - 1])
    # the game: B is the script moved D retraces later
    shifted = shifted_press(name, a.delay, Path(a.logs) / f"{name}.shifted{a.delay}.press")
    rs, _ = run(a.exe, a.rom, a.logs, name, f"shifted{a.delay}", press=shifted)
    tb, ts = trace(rb), trace(rs)
    bad |= judge(f"{name} --input-delay {a.delay} against the script {a.delay} retraces later", tb, ts,
                 f"{name}.delay{a.delay}.log", f"{name}.shifted{a.delay}.log")
    # and the schedule's recording replays to its trace
    rr, _ = run(a.exe, a.rom, a.logs, name, "schedule.replay", ["--replay-inputs", str(pc.resolve())])
    bad |= judge(f"{name} --input-delay {a.schedule} recorded against replayed", trace(rc), trace(rr),
                 f"{name}.schedule.log", f"{name}.schedule.replay.log")
    stats = re.search(r"input delay: (\d+) samples dropped as it shrank, (\d+) retraces repeated", rc)
    if not bad:
        print(f"replay_check: {name:10} delay: {n} retraces, {presses} changes of the pads; --input-delay {a.delay} gives "
              f"them {a.delay} retraces later, and its run is the script's moved {a.delay} retraces, line for line "
              f"({len(tb)} lines); --input-delay {a.schedule} gives the delay line's pads at each of {m} retraces "
              f"({stats.group(1) if stats else '?'} dropped, {stats.group(2) if stats else '?'} repeated) and replays "
              f"to its trace")
    return bad


def mode_gate(a, name):
    ra, sa = run(a.exe, a.rom, a.logs, name, "nogate", log_pads=False)   # no gate in the loop's path at all
    rb, sb = run(a.exe, a.rom, a.logs, name, "gate", env={"RUSH_SIM_GATE": a.sim}, log_pads=False)
    ta, tb = trace(ra), trace(rb)
    bad = judge(f"{name} without the gate against held pads", ta, tb, f"{name}.nogate.log", f"{name}.gate.log")
    held = re.search(r"held back at (\d+) retraces, ([\d.]+) ms", rb)
    waited = re.search(r"pads gate: (\d+) retraces, (\d+) waited for their pads \(([\d.]+) ms", rb)
    if not held or not waited or int(waited.group(2)) == 0:
        print(f"replay_check: {name}: the gate never waited (RUSH_SIM_GATE={a.sim})")
        bad = 1
    if not bad:
        print(f"replay_check: {name:10} gate: the same {len(ta)} lines with the pads held back at {held.group(1)} of "
              f"{waited.group(1)} retraces ({waited.group(3)} ms of waiting); {sa:.1f} s against {sb:.1f} s")
    return bad


def mode_mutation(a, name):
    ra, _ = run(a.exe, a.rom, a.logs, name, "base", full=1)
    pa, pb = Path(a.logs) / f"{name}.base.log", Path(a.logs) / f"{name}.mutated.log"
    bad = 0
    # the random seed: curated, and the course changes from there
    rb, _ = run(a.exe, a.rom, a.logs, name, "mutated", env={"RUSH_SIM_DESYNC": f"{a.at}:{a.address}:{a.xor}"}, full=1)
    print(f"replay_check: {name}: the random seed (0x{a.address}) changed at retrace {a.at}:")
    bad |= hash_compare.logs_main(str(pa), str(pb), a.at, None)
    # a window word the game never uses: only the window's hash can see it
    pc = Path(a.logs) / f"{name}.mutated_window.log"
    run(a.exe, a.rom, a.logs, name, "mutated_window", env={"RUSH_SIM_DESYNC": f"{a.at}:{a.window_address}:1"}, full=1)
    print(f"replay_check: {name}: a window word nothing uses (0x{a.window_address}) changed at retrace {a.at}:")
    bad |= hash_compare.logs_main(str(pa), str(pc), a.at, "window")
    hc = hash_compare.hashes(pc.read_text())
    ha = hash_compare.hashes(ra)
    if any(ha[n]["state"] != hc[n]["state"] for n in ha if n in hc):
        print("replay_check: the window word changed the curated hash too")
        bad = 1
    return bad


def mode_aslr(a, name):
    ra, sa = run(a.exe, a.rom, a.logs, name, "aslr1", full=a.full, log_pads=False)
    rb, sb = run(a.exe, a.rom, a.logs, name, "aslr2", full=a.full, log_pads=False)
    pa, pb = Path(a.logs) / f"{name}.aslr1.log", Path(a.logs) / f"{name}.aslr2.log"
    bad = hash_compare.logs_main(str(pa), str(pb), None, None)
    bad |= judge(f"{name} two processes", trace(ra), trace(rb), pa.name, pb.name)
    note = aslr_note([ra, rb])
    if "apart" not in note and os.name != "nt":
        print(f"replay_check: {name}: {note}: this proves nothing about host pointers")
        bad = 1
    if not bad:
        print(f"replay_check: {name:10} aslr: two processes agree on every hash ({note}; {sa:.1f} s, {sb:.1f} s)")
    return bad


def mode_file(a, name):
    """The file: cut short, it replays what it has and says so; recorded under other settings, it is refused with
    what differs, and replays with --replay-force."""
    bad = 0
    pads = Path(a.logs) / f"{name}.file.pads"
    full, _ = run(a.exe, a.rom, a.logs, name, "file", ["--record-inputs", str(pads.resolve())])
    data = pads.read_bytes()
    head = int.from_bytes(data[12:16], "little")
    records = (len(data) - head) // 20
    keep = records // 2
    cut = Path(a.logs) / f"{name}.cut.pads"
    cut.write_bytes(data[:head + keep * 20 + 7])   # half a run and 7 bytes of the next record
    text, _ = run(a.exe, a.rom, a.logs, name, "cut", ["--replay-inputs", str(cut.resolve())])
    said = re.search(r"replay: \S+ ends at retrace (\d+) \(cut short", text)
    if not said or int(said.group(1)) != keep:
        print(f"replay_check: {name}: a file cut after {keep} records doesn't say it ends there")
        bad = 1
    ha, hb = hash_compare.hashes(full), hash_compare.hashes(text)
    first, regions, common, _ = hash_compare.compare(ha, hb)
    if first is not None and first <= keep:   # hash n covers the pads of 0..n-1: equal through retrace `keep`
        print(f"replay_check: {name}: the cut file's replay differs at retrace {first}, before its end at {keep}")
        bad = 1
    # a damaged header, and a header of other settings
    broken = Path(a.logs) / f"{name}.broken.pads"
    broken.write_bytes(data[:30] + bytes([data[30] ^ 0xFF]) + data[31:])
    tb, _ = run(a.exe, a.rom, a.logs, name, "broken", ["--replay-inputs", str(broken.resolve()), "--seconds", "1"])
    if "damaged header" not in tb or "state reached: exit 2" not in tb:
        print(f"replay_check: {name}: a damaged header isn't refused")
        bad = 1
    other, _ = run(a.exe, a.rom, a.logs, name, "other", ["--replay-inputs", str(pads.resolve()), "--set",
                                                         "game.cars=8", "--seconds", "1"])
    if "game.cars: original in the file, here 8" not in other or "state reached: exit 2" not in other:
        print(f"replay_check: {name}: a recording of other settings isn't refused with what differs")
        bad = 1
    forced, _ = run(a.exe, a.rom, a.logs, name, "forced", ["--replay-inputs", str(pads.resolve()), "--set",
                                                           "game.cars=8", "--replay-force", "--seconds", "1"])
    if "--replay-force" not in forced or "exit 2" in forced:
        print(f"replay_check: {name}: --replay-force doesn't replay it")
        bad = 1
    if not bad:
        print(f"replay_check: {name:10} file: cut after {keep} of {records} records, it replays the same hashes "
              f"through retrace {keep} and says where it ends; a damaged header and other settings (game.cars) are "
              f"refused, --replay-force replays")
    return bad


def mode_replay_dir(a, names):
    src = Path(a.source)
    names = names or sorted(p.name[:-len(".pads")] for p in src.glob("*.pads") if not p.name.endswith(
        (".nodelay.pads", ".schedule.pads")))
    bad = 0
    for name in names:
        pads, other = src / f"{name}.pads", src / f"{name}.replay.log"
        if not pads.exists() or not other.exists():
            print(f"replay_check: {name:10} MISSING  {pads.name} or {other.name} in {src}")
            bad += 1
            continue
        text, took = run(a.exe, a.rom, a.logs, name, "replay", ["--replay-inputs", str(pads.resolve())], full=a.full)
        ta, tb = trace(other.read_text(encoding="utf-8", errors="replace")), trace(text)
        d = first_diff(ta, tb)
        if d:
            i, x, y = d
            hb = hash_compare.hashes(text)
            ha = hash_compare.hashes(other.read_text(encoding="utf-8", errors="replace"))
            first, regions, _, _ = hash_compare.compare(ha, hb)
            print(f"replay_check: {name:10} DIFFERS  at line {i} of {len(ta)} and {len(tb)}"
                  f"{f'; the hashes first at retrace {first}: ' + ', '.join(regions) if first is not None else ''}"
                  f"\n    {x}\n    {y}")
            bad += 1
        else:
            print(f"replay_check: {name:10} same     {len(ta)} lines, {len(hash_compare.hashes(text))} retraces hashed "
                  f"({took:.1f} s)")
    print(f"replay_check: {len(names) - bad} of {len(names)} recordings replay to the same trace and hashes")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("mode", choices=["same", "delay", "gate", "mutation", "aslr", "file", "replay-dir"])
    ap.add_argument("names", nargs="*", help="the paths (tools/port/synth/NAME.press)")
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/pc/synth/replay"))
    ap.add_argument("--full", type=int, default=30, help="the full hash every this many retraces")
    ap.add_argument("--delay", type=int, default=2)
    ap.add_argument("--schedule", default="2:0,4:600,1:1200")
    ap.add_argument("--sim", default="7,0.2,15", help="RUSH_SIM_GATE's SEED,PROBABILITY,MAX_MS")
    ap.add_argument("--at", type=int, default=1000)
    ap.add_argument("--address", default="8011735C", help="the curated word to change (the random seed)")
    ap.add_argument("--xor", default="1")
    ap.add_argument("--window-address", default="807FFFF0", help="a window word nothing uses")
    ap.add_argument("--from", dest="source", help="replay-dir: the folder of PATH.pads and PATH.replay.log")
    ap.add_argument("--mods", help="the folder the mods paths' mods/... are in (default: build/pc/synth/paths/mods "
                    "beside --logs)")
    ap.add_argument("--scripts", help="the folder of PATH.press (default tools/port/synth; tools/port/retail with the "
                    "retail cartridge, whose runs stay out of the repository)")
    a = ap.parse_args()
    global MODS_ROOT, SCRIPTS
    if a.scripts:
        SCRIPTS = Path(a.scripts)
    MODS_ROOT = a.mods or str(Path(a.logs).resolve().parent / "paths" / "mods")
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    if a.mode == "replay-dir":
        if not a.source:
            ap.error("replay-dir needs --from")
        sys.exit(mode_replay_dir(a, a.names))
    if not a.names:
        ap.error("name a path")
    fn = {"same": mode_same, "delay": mode_delay, "gate": mode_gate, "mutation": mode_mutation, "aslr": mode_aslr,
          "file": mode_file}
    sys.exit(1 if sum(fn[a.mode](a, n) for n in a.names) else 0)


if __name__ == "__main__":
    main()
