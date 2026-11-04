#!/usr/bin/env python3
"""A synthetic path's next run on the same saves (docs/port/SYNTH.md 2): a `# check:` of a press script.

    # check: tools/port/synth_again.py FIRST.game.log again/NAME

Runs tools/port/synth/again/NAME.press as synth_paths.py runs a path (its `# until`, `# seconds`, `# expect`,
`# visits`, `# args`, `# env` and `# check` lines), in the current directory (the paths' logs directory), with the
program and the cartridge of the run whose log FIRST.game.log is (its "command line:"), and without deleting what its
`# args` name: its controller pak is the one the first run left, so the game reads back what it saved. Its output
goes to NAME.log and NAME.game.log. The scripts in again/ are not paths of their own (synth_paths.py runs
tools/port/synth/*.press): each needs the pak a first run leaves. Exit status 0 if the run passes.
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import synth_paths  # noqa: E402


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__.split("\n\n")[1])
    first, name = sys.argv[1], sys.argv[2]
    line = next((l for l in Path(first).read_text(encoding="utf-8", errors="replace").splitlines()
                 if l.startswith("command line: ")), None)
    if not line:
        raise SystemExit(f"synth_again: {first} has no command line")
    words = line[len("command line: "):].split()
    exe, rom = words[0], words[words.index("--rom") + 1]
    script = synth_paths.PRESS_DIR / f"{name}.press"
    spec = synth_paths.read_script(script)
    stem = Path(name).name
    cmd = [exe, "--rom", rom, "--no-rom-check", "--headless", "--no-config", "--audio", "null", "--no-pak", "--log",
           f"{stem}.game.log", "--press-file", str(script), "--until-state", spec["until"], "--seconds",
           spec["seconds"]]
    if not spec["realtime"]:
        cmd.append("--fast")
    cmd += spec["args"]
    env = dict(os.environ, RUSH_TRACE_WAVES="1", **spec["env"])
    t0 = time.time()
    p = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    out = p.stdout.decode(errors="replace")
    Path(f"{stem}.log").write_text(out, encoding="utf-8")
    reached = [l for l in out.splitlines() if "state reached:" in l]
    last = reached[-1] if reached else ""
    why = []
    if p.returncode != 0:
        why.append(f"exit {p.returncode}")
    if not last:
        why.append("the state was not reached")
    elif spec["expect"] and spec["expect"] not in last:
        why.append(f"expected '{spec['expect']}'")
    if "nan" in last.split("car 0 at", 1)[-1]:
        why.append("the first car's position is NaN")
    for state in spec["visits"]:
        if f"game state 0x{int(state, 0):08X} " not in out:
            why.append(f"never in state {state}")
    for check in spec["checks"] if not why else []:
        c = [sys.executable, str(synth_paths.ROOT / check[0])] + check[1:] if check[0].startswith("tools/") else check
        r = subprocess.run(c, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        text = r.stdout.decode(errors="replace").strip()
        if text:
            print("\n".join(f"  {l}" for l in text.splitlines()))
        if r.returncode != 0:
            why.append(f"check {' '.join(check)} failed")
    print(f"synth_again: {name} {'ok  ' if not why else 'FAIL'} {time.time() - t0:5.1f} s "
          f"{last.split('state reached: ', 1)[-1] if last else ''}{'; ' + ', '.join(why) if why else ''}")
    if why:
        tail = [l for l in out.splitlines() if not re.search(r"wave .* loaded|wad .* converted", l)][-30:]
        print("\n".join(f"  | {l}" for l in tail))
    return 1 if why else 0


if __name__ == "__main__":
    sys.exit(main())
