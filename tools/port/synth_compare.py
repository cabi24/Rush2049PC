#!/usr/bin/env python3
"""Compare two builds of the game along the press scripts' paths, on the virtual clock (docs/port/OPTIMIZED.md).

    python3 tools/port/synth_compare.py --exe build/pc/rush2049pc --exe build/rel/rush2049pc \
        [--rom build/synth/rush2049_synth.z64] [--runs 2] [--status 1] [--logs DIR] [-j 4] [NAME ...]
    python3 tools/port/synth_compare.py --exe A --exe B --rom ROM --script PRESS --until MASK:N --seconds S

Each path (tools/port/synth/NAME.press, as tools/port/synth_paths.py runs it, but always with --fast) runs --runs
times with each executable. On the virtual clock a run takes the same course every time (the main loop works only
while every N64 thread waits, docs/port/ULTRA.md 1), so two runs of one program give the same trace, and so must two
builds. Two settings:

- RUSH_STATE_EXACT: the state log gives the first car's position and speed as exact floats (--coarse: whole feet,
  for a build without it).
- --status: the log gives them every --status seconds of game time too.
- --shots: the screenshots the path takes (its --screenshot) join the trace, each by its bytes (docs/port/DETAIL.md
  7: the settings at original draw the N64's picture, pixel for pixel).
- --audio: each run writes everything it plays to a WAV (--dump-audio), which joins the trace by its bytes (on the
  virtual clock the sound is the same every run but for its end: the last subframe, 192 frames, may reach the WAV
  before the exit or not, which on a loaded machine makes some paths vary; docs/port/AUDIO.md 10.6).

A run's trace is the loader's summary line (without the size of its symbol table, which is the build's data.c: a
decoupled own-storage symbol leaves it, docs/port/DECOUPLE.md 4), its state changes (the state, the retrace it
happened at, the players, windows, cars, mode, track, pause state, the car) and its status lines (the same at a
fixed retrace), without the graphics-task counts. Runs of one executable give the baseline. The verdict per path:

    same      every run of both executables has one trace
    varies    the executables' traces differ, but a run of one is a run of the other, or they differ no earlier
              than one executable's runs among themselves (the first place each differs is shown)
    DIFFERS   otherwise
    MISSES    a run's last state line doesn't say what the path's or the --script's `# expect:` line does (as
              synth_paths.py checks it): the same course in every run is no proof if it isn't the one the script
              is for (tools/port/synth/battle2p.press on the retail cartridge picks practice, docs/port/QA.md)

--script, --until and --seconds run one press script of any path instead (the retail cartridge: --rom, no
--no-rom-check then, docs/port/BRINGUP.md); the logs stay in --logs. --set-b, --env-b and --args-b give the second
executable settings, environment variables and options of its own, so one program can be compared with itself under
other settings (docs/port/MEMORY.md 6), or in another mode: paced mode against --fast (docs/port/ULTRA.md 10), with a
device fed from the tap (--args-b takes the options as one string, after --fast and --audio null, which it overrides);
--set gives both runs a setting (two builds compared at game.frame_rate = 60):

    --exe build/pc/rush2049pc --exe build/pc/rush2049pc --set-b game.memory=64 --env-b RUSH2049_TEST_HEAP_FLOOR=17
    --exe build/pc/rush2049pc --exe build/pc/rush2049pc --env-b RUSH_PACED=1 --env-b SDL_AUDIODRIVER=disk \
        --args-b="--audio sdl"

A path's own `# env:` line (synth_paths.py) applies to both. --from-logs runs nothing: it compares two folders of
logs that have been written already, each NAME.log of one against the other's (synth_paths.py --exact's, or
input_check.py's), as CI compares the Windows program's with Linux's (docs/port/WINDOWS.md 12):

    --from-logs linux/build/pc/synth/paths windows/synth/paths

A log must have exact floats (RUSH_STATE_EXACT) and status lines; a path on the host's clock (`# realtime`) is left
out, since its runs differ among themselves. Exit status: the number of paths that DIFFER or MISS (a run that doesn't end as its
`# expect:` line says), or with --same (CI) that aren't the same (with --from-logs: that aren't the same, or are missing from one folder). Stdlib only, Python 3.9+.
"""
import argparse
import concurrent.futures
import hashlib
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_paths  # noqa: E402

_STATE = re.compile(r"^rush2049pc: game state (0x[0-9A-F]{8}) \(was 0x[0-9A-F]{8}\) at vi (\d+), "
                    r"after \d+ graphics tasks(?:: (.*))?$")
EXACT = True   # RUSH_STATE_EXACT: positions and speeds as exact floats (--coarse: whole feet)
SAME = False   # --same: only "same" passes
SHOTS = False  # --shots: the run's screenshots (a path's --screenshot) are part of its trace, by their bytes
AUDIO = False  # --audio: the run's sound (--dump-audio) is part of its trace, by its bytes
_STATUS = re.compile(r"^rush2049pc: (status|state reached:|time reached:) t=\S+ vi=(\d+) gfx_tasks=\d+ "
                     r"audio_tasks=\d+(?: state=(0x[0-9A-F]{8}))?(?:: (.*))?$")


_HEXFLOAT = re.compile(r"(?<![\w.])[-+]?0x[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?p[-+]?\d+")
_NAN = re.compile(r"(?<![\w.])(-?)nan\([^)]*\)")


def exact_floats(info):
    """A line's exact floats (%a) in one spelling: the C libraries print one value differently (glibc and mingw-w64's
    own printf 0x0p+0 and 0x1.8p+0, the UCRT's 0x0.0000000000000p+0 and 0x1.8000000000000p+0, a NaN as -nan or
    -nan(ind)), so a Windows program built against the UCRT (MSYS2's UCRT64, the windows job) and Linux's are compared
    by the values (docs/port/WINDOWS.md 12). Each spelling is exact, so this keeps every difference of value."""
    info = _NAN.sub(r"\1nan", info)
    return _HEXFLOAT.sub(lambda m: float.fromhex(m.group(0)).hex(), info)


def trace(text):
    """The comparable events of one run's log: ('state', vi, state, info) and ('status', vi, state, info). The
    end line ("state reached:", after a number of graphics tasks) keeps only its state and info. Exact floats are
    compared by their values (exact_floats)."""
    out = []
    for line in text.splitlines():
        line = exact_floats(line)
        if line.startswith("pc_data: "):   # the loader's summary: what it filled and how pointers resolved
            out.append(("data", None, "", re.sub(r"^\d+ symbols, ", "", line[9:].strip())))
            continue
        m = _STATE.match(line)
        if m:
            out.append(("state", int(m.group(2)), m.group(1), m.group(3) or ""))
            continue
        m = _STATUS.match(line)
        if m:
            kind = "status" if m.group(1) == "status" else "end"
            out.append((kind, int(m.group(2)) if kind == "status" else None, m.group(3) or "", m.group(4) or ""))
        elif "Segmentation fault" in line or "rush2049pc: fault" in line:
            out.append(("fault", None, "", line.strip()))
    return tuple(out)


def first_difference(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i, x, y
    if len(a) != len(b):
        i = min(len(a), len(b))
        return i, a[i] if i < len(a) else None, b[i] if i < len(b) else None
    return None


def show(e):
    if e is None:
        return "(nothing: the run ended)"
    kind, vi, state, info = e
    return f"{kind} vi {vi} {state}: {info}" if vi is not None else f"{kind} {state}: {info}"


B_SETS, B_ENV, B_ARGS = [], {}, []   # --set-b, --env-b, --args-b: the second executable's own
SETS = []                # --set: both executables' (game.frame_rate=60: the game at 60 compared, docs/port/LOCKSTEP.md 9)
LINKS = []                           # --link: (name, target) in each run's folder


def run_one(exe, rom, cmd_tail, logs, tag, retail, path_env=None, second=False):
    env = dict(os.environ)
    env.update(synth_paths.data_env(logs))   # the runs' own data folder: no player's mods
    env.update(path_env or {})
    if second:
        env.update(B_ENV)   # the executable's own over the path's
    if EXACT:
        env["RUSH_STATE_EXACT"] = "1"
    extra = [w for s in SETS for w in ("--set", s)] + ([w for s in B_SETS for w in ("--set", s)] + B_ARGS if second
                                                       else [])
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve())] + ([] if retail else ["--no-rom-check"]) + \
        ["--headless", "--no-config", "--audio", "null", "--no-pak", "--fast"] + cmd_tail + extra + \
        (["--dump-audio", "run.wav"] if AUDIO else [])
    with tempfile.TemporaryDirectory(dir=logs) as cwd:   # screenshots and paks start afresh, and don't collide
        for name, target in LINKS:   # --link: the mods paths' mods (their --mods-dir is relative)
            os.symlink(Path(target).resolve(), Path(cwd) / name, target_is_directory=True)
        t0 = time.time()
        p = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        took = time.time() - t0
        shots = tuple(("shot", None, png.name, hashlib.sha256(png.read_bytes()).hexdigest())
                      for png in sorted(Path(cwd).glob("*.png"))) if SHOTS else ()
        if AUDIO:
            wav = Path(cwd) / "run.wav"
            shots += (("audio", None, f"{wav.stat().st_size if wav.exists() else 0} bytes",
                       hashlib.sha256(wav.read_bytes()).hexdigest() if wav.exists() else "none"),)
    text = p.stdout.decode(errors="replace")
    (Path(logs) / f"{tag}.log").write_text(text, encoding="utf-8")
    return trace(text) + shots, p.returncode, took


def compare(name, exes, rom, cmd_tail, runs, logs, retail, pool=None, path_env=None, expect=""):
    """The path's verdict (0 or 1), printed. With a pool, its runs are submitted to it and this returns a function
    that waits for them and judges: each run is a process of its own on the virtual clock, whose course doesn't
    depend on what else runs."""
    jobs = [(k, (pool.submit if pool else _now)(run_one, exe, rom, cmd_tail, logs, f"{name}.{k}.{r}", retail,
                                                path_env, k == 1))
            for k, exe in enumerate(exes) for r in range(runs)]
    return (lambda: judge(name, jobs, expect)) if pool else judge(name, jobs, expect)


class _now:
    """A finished future: run_one called at once."""

    def __init__(self, fn, *args):
        self.value = fn(*args)

    def result(self):
        return self.value


def script_expect(path):
    """A press script's `# expect:` line (synth_paths.py's format), or ""."""
    for line in Path(path).read_text().splitlines():
        m = re.match(r"#\s*expect\s*:\s*(.*)$", line)
        if m:
            return m.group(1).strip()
    return ""


def reaches(t, expect):
    """Whether a run's trace ends as the script's `# expect:` line says (synth_paths.py's check of its last state)."""
    ends = [e for e in t if e[0] == "end"]
    return not expect or bool(ends) and expect in f"state={ends[-1][2]}: {ends[-1][3]}"


def judge(name, jobs, expect=""):
    traces, times = [[], []], [[], []]   # per executable: [trace, ...], [seconds, ...]
    for k, fut in jobs:
        t, code, took = fut.result()
        traces[k].append(t + ((("exit", None, "", str(code)),) if code else ()))
        times[k].append(took)
    times = [min(tt) for tt in times]
    missed = [t for ts in traces for t in ts if not reaches(t, expect)]
    if missed:   # the same course in every run proves nothing if it isn't the one the script is for
        ends = [e for e in missed[0] if e[0] == "end"]
        print(f"synth_compare: {name:10} MISSES   {len(missed)} of {len(jobs)} runs don't end as the script expects "
              f"('{expect}'): {show(ends[-1]) if ends else '(no end line)'}", flush=True)
        return 1
    sets = [set(ts) for ts in traces]
    events = len(traces[0][0])
    timing = ", ".join(f"{t:.2f} s" for t in times)
    if len(sets[0]) == 1 and sets[0] == sets[1]:
        print(f"synth_compare: {name:10} same     {events} events ({timing})", flush=True)
        return 0

    def first(pairs):   # the earliest event at which two traces of the pairs differ
        return min((first_difference(x, y) or (len(x),))[0] for x, y in pairs) if pairs else None
    within = first([(x, y) for s in sets for x in s for y in s if x != y])
    across = first([(x, y) for x in sets[0] for y in sets[1]])
    overlap = bool(sets[0] & sets[1])
    # The builds agree if a run of one is a run of the other, or if they start to differ no earlier than one build's
    # own runs do (the virtual clock keeps a graphics task's tolerance, docs/port/ULTRA.md 1).
    verdict = "varies " if overlap or (within is not None and across >= within) else "DIFFERS"
    print(f"synth_compare: {name:10} {verdict}  {events} events ({timing}); runs give {len(sets[0])} and "
          f"{len(sets[1])} traces, {'some shared' if overlap else 'none shared'}", flush=True)
    for label, a, b in (("A vs B", traces[0][0], traces[1][0]),
                        ("A runs", traces[0][0], next((t for t in traces[0] if t != traces[0][0]), None)),
                        ("B runs", traces[1][0], next((t for t in traces[1] if t != traces[1][0]), None))):
        if b is None:
            continue
        d = first_difference(a, b)
        if d:
            i, x, y = d
            print(f"  {label}: event {i}\n    {show(x)}\n    {show(y)}")
    return 0 if verdict != "DIFFERS" and not SAME else 1


def from_logs(a, b):
    """--from-logs: each NAME.log in folder a against b's, by its trace; the number that aren't the same."""
    a, b = Path(a), Path(b)
    names = sorted({p.stem for d in (a, b) for p in d.glob("*.log")   # not the program's own (--log), nor its last
                    if not p.name.endswith((".game.log", ".game.old.log"))})
    if not names or not a.is_dir() or not b.is_dir():   # a folder a download put elsewhere compares nothing
        print(f"synth_compare: nothing to compare in {a} and {b}")
        return 1
    bad = same = 0
    for name in names:
        press = synth_paths.PRESS_DIR / f"{name}.press"
        if press.exists() and synth_paths.read_script(press)["realtime"]:
            print(f"synth_compare: {name:10} left out (on the host's clock)")
            continue
        logs = [d / f"{name}.log" for d in (a, b)]
        if not all(p.exists() for p in logs):
            print(f"synth_compare: {name:10} MISSING  in {', '.join(str(p.parent) for p in logs if not p.exists())}")
            bad += 1
            continue
        ta, tb = (trace(p.read_text(encoding="utf-8", errors="replace")) for p in logs)
        coarse = [str(p.parent) for p, t in zip(logs, (ta, tb))
                  if not any(e[0] == "status" and "0x" in e[3] for e in t)]
        d = first_difference(ta, tb)
        if coarse:
            print(f"synth_compare: {name:10} COARSE   no exact status lines (synth_paths.py --exact) in "
                  f"{', '.join(coarse)}")
        elif d is None:
            print(f"synth_compare: {name:10} same     {len(ta)} events")
            same += 1
            continue
        else:
            i, x, y = d
            print(f"synth_compare: {name:10} DIFFERS  at event {i} of {len(ta)} and {len(tb)}\n    {show(x)}\n"
                  f"    {show(y)}")
        bad += 1
    print(f"synth_compare: {same} of {same + bad} logs the same")
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--from-logs", nargs=2, metavar=("DIR_A", "DIR_B"),
                    help="compare two folders of logs already written, running nothing")
    ap.add_argument("--exe", action="append", help="an executable (give two)")
    ap.add_argument("--rom", default=str(ROOT / "build/synth/rush2049_synth.z64"))
    ap.add_argument("--runs", type=int, default=2, help="runs of each path per executable")
    ap.add_argument("--status", default="1", help="whole seconds of game time between status lines")
    ap.add_argument("--logs", default=str(ROOT / "build/synth/compare"))
    ap.add_argument("--script", help="a press script to run instead of the synthetic paths")
    ap.add_argument("--until", help="with --script: --until-state")
    ap.add_argument("--seconds", default="90", help="with --script: --seconds")
    ap.add_argument("--coarse", action="store_true",
                    help="positions in whole feet, as builds without RUSH_STATE_EXACT print them")
    ap.add_argument("--same", action="store_true", help="a path whose runs aren't all the same fails")
    ap.add_argument("--shots", action="store_true",
                    help="compare the screenshots a path takes (its --screenshot) too, byte for byte")
    ap.add_argument("--audio", action="store_true",
                    help="compare what each run plays (--dump-audio) too, byte for byte")
    ap.add_argument("-j", "--jobs", type=int, default=1, help="runs at once (each a process on the virtual clock)")
    ap.add_argument("--set", dest="sets", action="append", default=[], metavar="SECTION.KEY=VALUE",
                    help="a setting for both executables' runs (--set)")
    ap.add_argument("--set-b", action="append", default=[], metavar="SECTION.KEY=VALUE",
                    help="a setting for the second executable's runs (--set)")
    ap.add_argument("--env-b", action="append", default=[], metavar="NAME=VALUE",
                    help="an environment variable for the second executable's runs")
    ap.add_argument("--link", action="append", default=[], metavar="NAME=PATH",
                    help="a symbolic link in each run's folder (mods=build/synth/paths/mods: the mods paths' own)")
    ap.add_argument("--args-b", action="append", default=[], metavar="'OPTIONS'",
                    help="options for the second executable's runs, as one string (--args-b='--audio sdl')")
    ap.add_argument("names", nargs="*", help="the paths (default: every tools/port/synth/*.press)")
    a = ap.parse_args()
    if a.from_logs:
        sys.exit(from_logs(*a.from_logs))
    if not a.exe or len(a.exe) != 2:
        ap.error("give --exe twice")
    global EXACT, SAME, SHOTS, AUDIO
    EXACT, SAME, SHOTS, AUDIO = not a.coarse, a.same, a.shots, a.audio
    B_SETS.extend(a.set_b)
    B_ARGS.extend(w for s in a.args_b for w in shlex.split(s))
    for e in a.link:
        if "=" not in e:
            ap.error(f"--link takes NAME=PATH, not '{e}'")
        LINKS.append(tuple(e.split("=", 1)))
    SETS.extend(a.sets)
    for e in a.env_b:
        if "=" not in e:
            ap.error(f"--env-b takes NAME=VALUE, not '{e}'")
        B_ENV[e.split("=", 1)[0]] = e.split("=", 1)[1]
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    differ = 0
    if a.script:
        if not a.until:
            ap.error("--script needs --until")
        tail = ["--press-file", str(Path(a.script).resolve()), "--until-state", a.until, "--seconds", a.seconds,
                "--status", a.status]
        differ += compare(Path(a.script).stem, a.exe, a.rom, tail, a.runs, a.logs, retail=True,
                          expect=script_expect(a.script))
    else:
        names = a.names or sorted(p.stem for p in synth_paths.PRESS_DIR.glob("*.press"))
        names_out = []
        with concurrent.futures.ThreadPoolExecutor(max(1, a.jobs)) as pool:
            verdicts = []
            for name in names:
                press = synth_paths.PRESS_DIR / f"{name}.press"
                spec = synth_paths.read_script(press)
                if not spec["virtual"]:   # `# realtime: only`: it can't run on the virtual clock
                    print(f"synth_compare: {name:10s} left out (on the host's clock only)")
                    names_out.append(name)
                    continue
                tail = ["--press-file", str(press), "--until-state", spec["until"], "--seconds", spec["seconds"],
                        "--status", a.status] + spec["args"]
                verdicts.append(compare(name, a.exe, a.rom, tail, a.runs, a.logs, retail=False,
                                        pool=pool if a.jobs > 1 else None, path_env=spec["env"],
                                        expect=spec["expect"]))
            differ += sum(v() if callable(v) else v for v in verdicts)   # printed in the paths' order
        names = [n for n in names if n not in names_out]
        if len(names) > 1:
            print(f"synth_compare: {len(names) - differ} of {len(names)} paths agree")
    sys.exit(differ)


if __name__ == "__main__":
    main()
