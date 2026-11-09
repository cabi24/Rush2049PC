#!/usr/bin/env python3
"""The crossing counter's runs and report: where the game and the PC layer still cross between 32-bit N64 addresses and
host pointers, by kind and by site, along the synthetic paths (docs/port/NATIVE.md 9).

    python3 tools/port/crossings.py --build build/count [--guard] [--stop] [-j 6] [NAME ...]
    python3 tools/port/crossings.py --exe build/count/rush2049pc [--rom ROM] [--guard] [--stop] [NAME ...]
    python3 tools/port/crossings.py --report DUMP_OR_FOLDER ... [--exe EXE]

The first form configures (Debug, -DRUSH_COUNT_CROSSINGS=ON, Ninja) and builds the counting build in that folder
when it isn't one yet, builds it, makes its synthetic cartridge if it has none, and runs every synthetic path with it
(those synth_compare.py runs: all but the ones on the host's clock only, with its options and on the virtual clock),
then prints the report. The second runs an executable built so already. --report reads dumps a counting build wrote
(RUSH2049_CROSSINGS=FILE: one per run) and prints their report. Each run's log and dump stay in --logs (default
build/crossings/paths): NAME.log, NAME.crossings, and with --guard NAME.guard.log and NAME.guard.crossings.

--guard runs each path a second time in the arena guard's counting mode (RUSH2049_WINDOW_GUARD=arenas,count): the
boot and game arenas' pages in the RDRAM window without access, every page's first access in a frame counted by its
instruction. The report then has the guard's sites too (from the second runs; the crossings are the first runs').
--stop runs each path once more in the guard's stopping mode (RUSH2049_WINDOW_GUARD=arenas), where the first access
stops the program with a note and the stack (NAME.stop.log): the report says where each run stopped, if it did.

The report: a line per kind (its live sites, its crossings and where they point: the regions of crossings.c), then
every site of each kind with its count, the number of paths that reach it, where its addresses point and the data
there (for the arenas and the rest of RDRAM outside the heap), and with --guard the guard's instructions. A game C site
is a source line (its file macro's use); a PC layer site is the code that called the counting function, named with
addr2line from the executable (the dump's `exe` line, or --exe). Sites are sorted by name (--sort count: by count),
so two reports diff line by line; --top N keeps each kind's N largest. --out FILE writes it there too, --json FILE the
numbers.

Checks (exit status 1 when one fails, after the report):

    --check                 every run ended as its press script's `# expect:` line says and wrote its dump (but a run
                            the guard stopped), and each guarded run's trace (synth_compare.py's) is its plain run's
    --zero-kind K,...       those kinds cross nowhere              e.g. pc_statll_link,pc_statll_set_link
    --zero-region R,...     no crossing points into those regions  e.g. game_arena,boot_arena
    --zero-site TEXT        no site whose name has TEXT            e.g. src/blob/forces1.c
    --no-faults             the guard found nothing and stopped no run (after NATIVE.md step 5); --expect-faults: it
                            found something
    --baseline FILE         no kind has more sites than FILE says, nor more crossings (but the 0.1% the frames
                            around the exit vary by); --write-baseline FILE writes one

A later step's proof is a run of this with its zero checks: "these sites cross 0 times along every path". Stdlib only,
Python 3.9+; addr2line (binutils) names the PC layer's sites.
"""
import argparse
import collections
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import synth_compare  # noqa: E402
import synth_paths  # noqa: E402

# The kinds in crossings.c's order (pc_crossings.h): the report's lines, in this order, always all of them.
KINDS = ["PC_N64P", "PC_N64A", "PC_N64W", "PC_SLOT_N64", "PC_SLOT_SET", "pc_statll_link", "pc_statll_set_link",
         "pc_n64_ptr", "pc_n64_addr", "osVirtualToPhysical", "gfx.rom_list", "gfx.native_list", "audio.n64ptr",
         "audio.field"]
WHAT = {
    "PC_N64P": "game C: an N64 address taken as a host pointer",
    "PC_N64A": "game C: a host pointer's N64 address",
    "PC_N64W": "game C: a pointer stored in a 32-bit field",
    "PC_SLOT_N64": "game C: a heap handle's slot read as an N64 address",
    "PC_SLOT_SET": "game C: an N64 address stored in a handle's slot",
    "pc_statll_link": "game C: a StatLL list's 32-bit link read",
    "pc_statll_set_link": "game C: a StatLL list's 32-bit link written",
    "pc_n64_ptr": "PC layer: an N64 address made a host pointer",
    "pc_n64_addr": "PC layer: a host pointer's N64 address",
    "osVirtualToPhysical": "the colour and depth images' physical addresses",
    "gfx.rom_list": "the renderer: an address in a ROM-form display list",
    "gfx.native_list": "the renderer: an address below 4 GiB in a native list",
    "audio.n64ptr": "the audio interpreter: an N64 address read (resolve: a native field's)",
    "audio.field": "the audio interpreter: a native pointer field holding an N64 address",
}
# Two runs of one tree give the same sites, but the frames around the exit add a few crossings to some (a handful in
# 73 M along the 63 paths: NATIVE.md 9.4): --baseline lets a kind's crossings exceed the baseline's by this factor and
# 64 more, and its sites not at all.
BASELINE_SLACK = 1.001
# Regions whose addresses are data a name describes (the dump gives the datum).
NAMED = re.compile(r"_arena$|^low_memory$|^image_block$|^window$|^kseg1$")
_SITE = re.compile(r"^site (\S+) (\S+) (\d+)((?: \S+)*)$")
_FAULT = re.compile(r"^fault (\S+) (\d+)((?: \S+)*)$")
_REGION = re.compile(r"^([A-Za-z0-9_]+)=(\d+)@(0x[0-9a-fA-F]+)(?::(.*))?$")
_GUARD = re.compile(r"^guard (\S+) faults=(\d+) neighbour=(\d+) closes=(\d+) lost=(\d+)$")
_CODE = re.compile(r"^(.+)\+0x([0-9a-fA-F]+)$")


class Site:
    __slots__ = ("count", "regions", "data", "runs", "wheres")

    def __init__(self):
        self.count = 0
        self.regions = collections.Counter()
        self.data = collections.defaultdict(set)   # region -> datum names
        self.runs = set()
        self.wheres = set()   # the dumps' names for it (a source line, or code: `rush2049pc+0x1234`)

    def merge(self, other):
        self.count += other.count
        self.regions.update(other.regions)
        for r, d in other.data.items():
            self.data[r] |= d
        self.runs |= other.runs
        self.wheres |= other.wheres
        return self


def natural(text):
    """A sort key that orders numbers by value: group.c:44 before group.c:105."""
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", text)]


class Report:
    """Dumps added together: sites[(kind, where)], faults[where] (the guard's), and the guard's totals. The crossings
    are the plain runs' when there are any (a guarded run of the same path counts the same ones again), else the
    guarded runs'."""

    def __init__(self):
        self.plain_sites = collections.defaultdict(Site)
        self.guard_sites = collections.defaultdict(Site)
        self.faults = collections.defaultdict(Site)
        self.runs = []
        self.guard_runs = []
        self.guard = collections.Counter()
        self.stops = []   # the stopping mode's runs: (path, (what, instruction) or None, exit status)
        self.lost = 0
        self.exes = set()

    @property
    def sites(self):
        return self.plain_sites if self.runs or not self.guard_runs else self.guard_sites

    def add(self, path, run=None):
        run = run or Path(path).name
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        guarded = any(_GUARD.match(line) for line in lines)
        sites = self.guard_sites if guarded else self.plain_sites
        for line in lines:
            if line.startswith("exe "):
                self.exes.add(line[4:].strip())
            elif line.startswith("lost "):
                self.lost += int(line.split()[1]) if not guarded else 0
            elif line.startswith("guard "):
                m = _GUARD.match(line)
                for key, v in zip(("faults", "neighbour", "closes", "lost"), m.groups()[1:]):
                    self.guard[key] += int(v)
            elif line.startswith("site "):
                m = _SITE.match(line)
                if m:
                    self._add(sites[(m.group(1), m.group(2))], m.group(2), int(m.group(3)), m.group(4), run)
            elif line.startswith("fault "):
                m = _FAULT.match(line)
                if m:
                    self._add(self.faults[m.group(1)], m.group(1), int(m.group(2)), m.group(3), run)
        (self.guard_runs if guarded else self.runs).append(run)
        return self

    @staticmethod
    def _add(site, where, n, regions, run):
        site.count += n
        site.runs.add(run)
        site.wheres.add(where)
        for word in regions.split():
            m = _REGION.match(word)
            if not m:
                continue
            region, count, name = m.group(1), int(m.group(2)), m.group(4)
            site.regions[region] += count
            if name and NAMED.search(region):
                site.data[region].add(re.sub(r"\+0x[0-9A-Fa-f]+$", "", name))

    def named(self, exe=None):
        """The sites by their names: ({kind: {name: Site}} with every kind, {name: Site} for the guard), the PC layer's
        code named by addr2line (`function file:line`) and the places of one name (two calls on a line) merged."""
        exe = exe or (sorted(self.exes)[0] if len(self.exes) == 1 else None)
        names = symbolize([w for _, w in self.sites] + list(self.faults), exe)
        kinds = {k: {} for k in KINDS}
        for (kind, w), s in self.sites.items():
            n = names.get(w, w)
            kinds.setdefault(kind, {}).setdefault(n, Site()).merge(s)
        faults = {}
        for w, s in self.faults.items():
            faults.setdefault(names.get(w, w), Site()).merge(s)
        return kinds, faults


def totals(kinds):
    """kind -> (sites, crossings, Counter of regions), from Report.named's kinds."""
    out = {}
    for kind, sites in kinds.items():
        regions = collections.Counter()
        for s in sites.values():
            regions.update(s.regions)
        out[kind] = (len(sites), sum(s.count for s in sites.values()), regions)
    return out


def source_root(exe):
    """The tree an executable was built from (its build folder's CMakeCache.txt), else this one."""
    for d in (Path(exe).resolve().parent, *Path(exe).resolve().parents):
        cache = d / "CMakeCache.txt"
        if cache.exists():
            m = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$", cache.read_text(errors="replace"), re.M)
            if m:
                return m.group(1).rstrip("/") + "/"
            break
    return str(ROOT) + "/"


def symbolize(wheres, exe):
    """{where: "function file:line"} for the executable's sites (`name+0xOFFSET`), with addr2line; the rest as they
    are. Paths are made relative to the tree the executable was built from. A guard's fault in a library's code
    (`libc.so.6+0x1584c3,from=rush2049pc+0x1234`) is named with its caller: `libc.so.6+0x1584c3 from function
    file:line`."""
    names = {}
    if not exe or not Path(exe).exists() or not shutil.which("addr2line"):
        return names
    base = Path(exe).name
    parts = [p for w in wheres for p in w.split(",from=")]
    offs = sorted({m.group(2).lower() for p in parts for m in [_CODE.match(p)] if m and m.group(1) == base})
    if not offs:
        return names
    root = source_root(exe)
    out = subprocess.run(["addr2line", "-f", "-e", exe] + ["0x" + o for o in offs], capture_output=True,
                         text=True).stdout.splitlines()
    for i, off in enumerate(offs):
        if 2 * i + 1 >= len(out):
            break
        fn, loc = out[2 * i], out[2 * i + 1]
        loc = re.sub(r" \(discriminator \d+\)$", "", loc)
        if loc.startswith(root):
            loc = loc[len(root):]
        names[f"{base}+0x{off}"] = f"{fn} {loc}"

    def one(p):
        return names.get(re.sub(r"\+0x([0-9a-fA-F]+)$", lambda m: "+0x" + m.group(1).lower(), p), p)
    out = {}
    for w in wheres:
        head, _, caller = w.partition(",from=")
        out[w] = one(head) + (f" from {one(caller)}" if caller else "")
    return out


def regions_text(site):
    parts = []
    for region, n in sorted(site.regions.items(), key=lambda x: (-x[1], x[0])):
        data = sorted(site.data.get(region, ()))
        more = f" ({', '.join(data[:4])}{', ...' if len(data) > 4 else ''})" if data else ""
        parts.append(f"{region} {n}{more}")
    return ", ".join(parts)


def render(rep, exe=None, sort="site", top=0):
    """The report's text."""
    kinds, faults = rep.named(exe)
    tot = totals(kinds)
    L = [f"crossings: {len(rep.runs)} run{'s' if len(rep.runs) != 1 else ''}"
         + (f", {len(rep.guard_runs)} guarded" if rep.guard_runs else "")
         + " (tools/port/crossings.py, docs/port/NATIVE.md 9)"]
    if rep.lost:
        L.append(f"lost: {rep.lost} crossings found no entry for their site (crossings.c's SITES)")
    L += ["", f"{'kind':20} {'sites':>6} {'crossings':>12}  where they point"]
    for kind, (sites, n, regions) in tot.items():
        where = ", ".join(f"{r} {c}" for r, c in sorted(regions.items(), key=lambda x: (-x[1], x[0])))
        L.append(f"{kind:20} {sites:6} {n:12}  {where}".rstrip())
    L.append(f"{'total':20} {sum(t[0] for t in tot.values()):6} {sum(t[1] for t in tot.values()):12}")

    def listing(sites):
        items = sorted(sites.items(), key=lambda x: (-x[1].count, natural(x[0])) if sort == "count"
                       else natural(x[0]))
        if top:
            keep = {name for name, _ in sorted(items, key=lambda x: (-x[1].count, natural(x[0])))[:top]}
            items = [x for x in items if x[0] in keep]
        width = min(max((len(name) for name, _ in items), default=0), 72)
        return [f"  {name:{width}} {s.count:11}  paths {len(s.runs):2}  {regions_text(s)}" for name, s in items]

    for kind, sites in kinds.items():
        if not sites:
            continue
        L += ["", f"{kind}: {len(sites)} site{'s' if len(sites) != 1 else ''}, {tot[kind][1]} crossings "
                  f"({WHAT.get(kind, '')})"]
        L += listing(sites)
    if rep.guard_runs:
        g = rep.guard
        L += ["", f"window guard (RUSH2049_WINDOW_GUARD=arenas,count): {g['faults']} faults at {len(faults)} "
                  f"instructions in {len(rep.guard_runs)} runs; {g['neighbour']} more on the guarded pages' other bytes "
                  f"(the heap's first page); the pages closed {g['closes']} times"]
        if g["lost"]:
            L.append(f"  lost: {g['lost']} faults found no entry for their instruction")
        L += listing(faults)
    if rep.stops:
        stopped = [s for s in rep.stops if s[1]]
        L += ["", f"window guard, stopping (RUSH2049_WINDOW_GUARD=arenas): {len(stopped)} of {len(rep.stops)} runs "
                  f"stopped at their first access (each log has the stack)"]
        width = max(len(n) for n, _, _ in rep.stops)
        where = symbolize([s[1] for _, s, _ in rep.stops if s], exe or (sorted(rep.exes)[0] if len(rep.exes) == 1
                                                                        else None))
        for name, stop, code in sorted(rep.stops):
            if stop:
                L.append(f"  {name:{width}}  {stop[0]}, by {where.get(stop[1], stop[1])}")
            else:
                L.append(f"  {name:{width}}  {'ran through' if code == 0 else f'exit {code}, no note from the guard'}")
    return "\n".join(L) + "\n"


def as_json(rep, exe=None):
    kinds, faults = rep.named(exe)

    def site(name, s):
        return {"name": name, "where": sorted(s.wheres), "count": s.count, "paths": len(s.runs),
                "regions": dict(sorted(s.regions.items())), "data": {r: sorted(d) for r, d in sorted(s.data.items())}}
    return {"runs": len(rep.runs), "guard_runs": len(rep.guard_runs),
            "kinds": {k: {"sites": t[0], "crossings": t[1], "regions": dict(sorted(t[2].items()))}
                      for k, t in totals(kinds).items()},
            "sites": {k: [site(n, s) for n, s in sorted(sites.items(), key=lambda x: natural(x[0]))]
                      for k, sites in kinds.items()},
            "guard": dict(rep.guard, instructions=len(faults)),
            "faults": [site(n, s) for n, s in sorted(faults.items(), key=lambda x: natural(x[0]))]}


def checks(rep, a, exe=None):
    """The failed checks' messages."""
    bad = []
    kinds, faults = rep.named(exe)
    tot = totals(kinds)
    for kind in filter(None, (a.zero_kind or "").split(",")):
        if tot.get(kind, (0, 0))[1]:
            bad.append(f"--zero-kind {kind}: {tot[kind][1]} crossings at {tot[kind][0]} sites")
    zero_regions = set(filter(None, (a.zero_region or "").split(",")))
    for kind, sites in kinds.items():
        for name, s in sorted(sites.items(), key=lambda x: natural(x[0])):
            hit = sorted(r for r in s.regions if r in zero_regions)
            if hit:
                bad.append(f"--zero-region: {kind} {name}: {', '.join(f'{r} {s.regions[r]}' for r in hit)}")
            if a.zero_site and (a.zero_site in name or any(a.zero_site in w for w in s.wheres)):
                bad.append(f"--zero-site {a.zero_site}: {kind} {name}: {s.count}")
    stopped = [name for name, stop, _ in rep.stops if stop]
    if a.no_faults and rep.guard["faults"]:
        bad.append(f"--no-faults: the guard found {rep.guard['faults']} faults at {len(faults)} instructions")
    if a.no_faults and stopped:
        bad.append(f"--no-faults: the guard stopped {len(stopped)} runs: {', '.join(stopped)}")
    if a.expect_faults and not rep.guard["faults"] and not stopped:
        bad.append("--expect-faults: the guard found nothing"
                   + ("" if rep.guard_runs or rep.stops else " (no guarded run)"))
    if a.baseline:   # sites exactly; crossings within the few the frames around the exit add (section 9.4)
        base = json.loads(Path(a.baseline).read_text())["kinds"]
        for kind, t in tot.items():
            b = base.get(kind, {"sites": 0, "crossings": 0})
            if t[0] > b["sites"] or t[1] > b["crossings"] * BASELINE_SLACK + 64:
                bad.append(f"--baseline: {kind} has {t[0]} sites and {t[1]} crossings, the baseline {b['sites']} and "
                           f"{b['crossings']}")
    return bad


# ---------------------------------------------------------------- runs

# A run's mode: counting; the guard counting (--guard); the guard stopping at the first access (--stop). Each with its
# logs' suffix and its RUSH2049_WINDOW_GUARD.
MODES = {"count": ("", None), "guard": (".guard", "arenas,count"), "stop": (".stop", "arenas")}
Run = collections.namedtuple("Run", "name mode trace code dump expect stop")
# The guard's note when it stops a run (crossings.c): what was reached, and the instruction.
_NOTE = re.compile(r"rush2049pc: window guard: (.*) reached by its N64 address, by the instruction at (\S+) ")


def run_path(name, exe, rom, logs, mode):
    """One run of a synthetic path as synth_compare.py makes it, in one of MODES. stop is the guard's note when it
    stopped the run, (what, instruction), else None."""
    spec = synth_paths.read_script(synth_paths.PRESS_DIR / f"{name}.press")
    suffix, guard = MODES[mode]
    dump = Path(logs).resolve() / f"{name}{suffix}.crossings"
    if dump.exists():
        dump.unlink()
    env = dict(os.environ, RUSH_STATE_EXACT="1", RUSH2049_CROSSINGS=str(dump), **synth_paths.data_env(logs))
    env.update(spec["env"])
    env.pop("RUSH2049_WINDOW_GUARD", None)
    if guard:
        env["RUSH2049_WINDOW_GUARD"] = guard
    cmd = [str(Path(exe).resolve()), "--rom", str(Path(rom).resolve()), "--no-rom-check", "--headless", "--no-config",
           "--audio", "null", "--no-pak", "--fast", "--press-file", str(synth_paths.PRESS_DIR / f"{name}.press"),
           "--until-state", spec["until"], "--seconds", spec["seconds"], "--status", "1"] + spec["args"]
    with tempfile.TemporaryDirectory(dir=logs) as cwd:   # screenshots and paks start afresh, and don't collide
        try:
            p = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
            text, code = p.stdout.decode(errors="replace"), p.returncode
        except subprocess.TimeoutExpired as e:
            text, code = (e.stdout or b"").decode(errors="replace") + "\ncrossings: timed out\n", -1
    (Path(logs) / f"{name}{suffix}.log").write_text(text, encoding="utf-8")
    m = _NOTE.search(text) if mode == "stop" else None
    return Run(name, mode, synth_compare.trace(text), code, dump, spec["expect"], m.groups() if m else None)


def run_paths(names, exe, rom, logs, modes, jobs):
    Path(logs).mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max(1, jobs)) as pool:
        futs = [pool.submit(run_path, n, exe, rom, logs, m) for n in names for m in modes]
        results = []
        for f in futs:
            r = f.result()
            label = r.name + ("" if r.mode == "count" else f" ({r.mode})")
            state = "stopped" if r.stop else "ok" if r.code == 0 and r.dump.exists() else "FAIL"
            print(f"crossings: {label:22} {state}{'' if r.code == 0 or r.stop else f' exit {r.code}'}"
                  f"{'' if r.dump.exists() or r.stop else ', no dump'}", flush=True)
            results.append(r)
    return results


def run_checks(results):
    """--check on the runs: each ended as its script expects and wrote its dump (a run the guard stopped is judged by
    --no-faults and --expect-faults instead); a guarded run's trace is its plain run's."""
    bad = []
    plain = {r.name: r.trace for r in results if r.mode == "count"}
    for r in results:
        if r.stop:
            continue
        tag = r.name + ("" if r.mode == "count" else f" ({r.mode})")
        if r.code != 0:
            bad.append(f"{tag}: exit {r.code}")
        if not r.dump.exists():
            bad.append(f"{tag}: no dump")
        if not synth_compare.reaches(r.trace, r.expect):
            bad.append(f"{tag}: doesn't end as its script expects ('{r.expect}')")
        if r.mode != "count" and r.name in plain and r.trace != plain[r.name]:
            d = synth_compare.first_difference(plain[r.name], r.trace)
            bad.append(f"{tag}: the trace differs from the plain run's at event {d[0]}: {synth_compare.show(d[1])} / "
                       f"{synth_compare.show(d[2])}")
    return bad


def build(folder, jobs):
    """Configure (when it isn't yet) and build the counting build in folder; its executable."""
    folder = Path(folder)
    cache = folder / "CMakeCache.txt"
    if not cache.exists():
        subprocess.run(["cmake", "-S", str(ROOT), "-B", str(folder), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug",
                        "-DRUSH_COUNT_CROSSINGS=ON"], check=True)
    elif not re.search(r"^RUSH_COUNT_CROSSINGS:BOOL=ON$", cache.read_text(errors="replace"), re.M):
        raise SystemExit(f"crossings: {folder} is not a counting build (cmake -DRUSH_COUNT_CROSSINGS=ON)")
    subprocess.run(["cmake", "--build", str(folder), "-j", str(jobs)], check=True)
    return folder / "rush2049pc"


def synth_rom(folder):
    """The build's synthetic cartridge, made as ctest's synth_rom makes it when it isn't there."""
    rom = Path(folder) / "synth/rush2049_synth.z64"
    if not rom.exists():
        subprocess.run([sys.executable, str(ROOT / "tools/port/synth_rom.py"), "--out", str(rom), "--data-symbols",
                        str(Path(folder) / "pc-data/data_symbols.txt")], check=True)
    return rom


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", help="the counting build's folder: configured and built first")
    ap.add_argument("--exe", help="a counting build's executable (or, with --report, the one that wrote the dumps)")
    ap.add_argument("--rom", help="the synthetic cartridge (default: the build's synth/rush2049_synth.z64)")
    ap.add_argument("--logs", default=str(ROOT / "build/crossings/paths"))
    ap.add_argument("-j", "--jobs", type=int, default=6, help="runs at once")
    ap.add_argument("--build-jobs", type=int, default=8)
    ap.add_argument("--guard", action="store_true", help="each path again in the arena guard's counting mode")
    ap.add_argument("--stop", action="store_true", help="each path again in the arena guard's stopping mode")
    ap.add_argument("--report", nargs="+", metavar="DUMP", help="report on these dumps (or folders of *.crossings)")
    ap.add_argument("--sort", choices=("site", "count"), default="site")
    ap.add_argument("--top", type=int, default=0, help="each kind's N largest sites only")
    ap.add_argument("--out", help="write the report to this file too")
    ap.add_argument("--json", help="write the numbers as JSON")
    ap.add_argument("--check", action="store_true", help="the runs reached their states, wrote their dumps, and the "
                    "guard changed no trace")
    ap.add_argument("--zero-kind")
    ap.add_argument("--zero-region")
    ap.add_argument("--zero-site")
    ap.add_argument("--no-faults", action="store_true")
    ap.add_argument("--expect-faults", action="store_true")
    ap.add_argument("--baseline")
    ap.add_argument("--write-baseline")
    ap.add_argument("names", nargs="*", help="the paths (default: every synthetic path on the virtual clock)")
    a = ap.parse_args()

    rep = Report()
    bad = []
    exe = a.exe
    if a.report:
        for p in a.report:
            p = Path(p)
            for f in sorted(p.glob("*.crossings")) if p.is_dir() else [p]:
                rep.add(f, f.name)
    else:
        if a.build:
            exe = build(a.build, a.build_jobs)
        if not exe:
            ap.error("give --build, --exe or --report")
        rom = a.rom or synth_rom(a.build or Path(exe).parent)
        names = a.names or sorted(p.stem for p in synth_paths.PRESS_DIR.glob("*.press")
                                  if synth_paths.read_script(p)["virtual"])
        modes = ["count"] + (["guard"] if a.guard else []) + (["stop"] if a.stop else [])
        results = run_paths(names, exe, rom, a.logs, modes, a.jobs)
        for r in results:
            if r.mode == "stop":
                rep.stops.append((r.name, r.stop, r.code))
            elif r.dump.exists():
                rep.add(r.dump, r.name)
        if a.check:
            bad += run_checks(results)
    text = render(rep, exe, a.sort, a.top)
    sys.stdout.write("\n" + text)
    if a.out:
        Path(a.out).write_text(text)
    if a.json:
        Path(a.json).write_text(json.dumps(as_json(rep, exe), indent=1) + "\n")
    if a.write_baseline:
        Path(a.write_baseline).write_text(json.dumps({"kinds": as_json(rep, exe)["kinds"]}, indent=1) + "\n")
    bad += checks(rep, a, exe)
    for b in bad:
        print(f"crossings: FAIL {b}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
