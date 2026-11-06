#!/usr/bin/env python3
"""Which settings change the game's state: one replay per candidate setting, against the same replay unchanged
(docs/port/LOCKSTEP.md 7, NETPLAY.md 1.5 and phase 0.4's last bullet).

    python3 tools/port/hash_settings.py --exe build/pc/rush2049pc --rom build/pc/synth/rush2049_synth.z64 \
        [--logs DIR] [-j 8] [--path race2p --path drive ...] [--markdown]

Each path (tools/port/synth/PATH.press) is recorded once (replay_check.py's run: the virtual clock, exact floats, the
state hash every retrace and the window's every 30). Then the recording is replayed unchanged (the baseline) and once
with each candidate of NETPLAY.md 1.5's two tables set (--set, or an option; --replay-force, since the header says
what a replay must match and these differ on purpose). A setting changes the game if the replay's hashes differ from
the baseline's: the first retrace and the regions are named (the curated regions every retrace, the window, own
storage and the decoupled data every 30). The pads are the file's in every run, so [input] settings can't reach the
game by construction; they are in the list to show it.

Stdlib only, Python 3.9+.
"""
import argparse
import concurrent.futures
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/port"))
import hash_compare  # noqa: E402
import replay_check  # noqa: E402

# (the table of NETPLAY.md 1.5 it belongs to, what is set)
CANDIDATES = [
    ("must", ["--set", "game.frame_rate=60"]),
    ("must", ["--set", "game.cars=8"]),
    ("must", ["--set", "game.cars=12"]),
    ("must", ["--set", "game.memory=64"]),
    ("must", ["--set", "game.expansion_pak=off"]),
    ("must", ["--set", "game.pc_setup=on"]),
    ("must", ["--set", "game.ghost_prompt=on"]),
    ("must", ["--set", "game.cheats_menu=on"]),
    ("must", ["--set", "game.cheats_unlock=on"]),
    ("must", ["--set", "audio.voices=64"]),
    ("must", ["--set", "audio.engines=all"]),
    ("must", ["--set", "video.draw_distance=far"]),
    ("must", ["--set", "video.draw_distance=max"]),
    ("must", ["--set", "video.detail=high"]),
    ("must", ["--set", "video.fog=far"]),
    ("must", ["--set", "video.fog=off"]),
    ("must", ["--pak", "matrix.mpk"]),   # a memory pak (game.pak=memory), a new one in the run's folder
    ("must", ["--set", "game.pak=rumble"]),
    ("must", ["--set", "game.pak2=rumble"]),
    ("free", ["--set", "video.scale=2"]),
    ("free", ["--set", "video.widescreen=16:9"]),
    ("free", ["--set", "video.widescreen=16:9", "--session-rules"]),
    ("free", ["--set", "video.fill=on"]),
    ("free", ["--set", "video.frame_rate=120"]),
    ("free", ["--set", "video.hud=centered"]),
    ("free", ["--set", "video.textures=upscaled"]),
    ("free", ["--set", "video.texture_lod=sharp"]),
    ("free", ["--set", "video.vsync=off"]),
    ("free", ["--set", "video.fullscreen=on"]),
    ("free", ["--set", "audio.volume=50"]),
    ("free", ["--set", "audio.latency=100"]),
    ("free", ["--audio", "wav"]),
    ("free", ["--set", "input.deadzone=0.3"]),
    ("free", ["--set", "input.sensitivity=1.5"]),
    ("free", ["--set", "input.stick_range=60"]),
    ("free", ["--set", "input.keyboard_steering=smooth"]),
]


def label(args):
    return " ".join(a for a in args if a != "--set")


def one(a, name, pads, k, args):
    text, _ = replay_check.run(a.exe, a.rom, a.logs, name, f"set{k}", ["--replay-inputs", str(pads), "--replay-force"]
                               + args, full=30)
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", default=str(ROOT / "build/pc/rush2049pc"))
    ap.add_argument("--rom", default=str(ROOT / "build/pc/synth/rush2049_synth.z64"))
    ap.add_argument("--logs", default=str(ROOT / "build/pc/synth/settings"))
    ap.add_argument("--path", action="append", help="the paths (default: race2p, drive, stunt, battle2p)")
    ap.add_argument("-j", "--jobs", type=int, default=4)
    ap.add_argument("--markdown", action="store_true", help="the table as Markdown")
    a = ap.parse_args()
    Path(a.logs).mkdir(parents=True, exist_ok=True)
    replay_check.MODS_ROOT = None
    paths = a.path or ["race2p", "drive", "stunt", "battle2p"]
    results = {}   # (k, path) -> text
    with concurrent.futures.ThreadPoolExecutor(max(1, a.jobs)) as pool:
        base = {}
        for name in paths:
            pads = (Path(a.logs) / f"{name}.pads").resolve()
            replay_check.run(a.exe, a.rom, a.logs, name, "record", ["--record-inputs", str(pads)], full=30)
            base[name] = pool.submit(replay_check.run, a.exe, a.rom, a.logs, name, "base",
                                     ["--replay-inputs", str(pads)], None, 30)
            for k, (_, args) in enumerate(CANDIDATES):
                results[(k, name)] = pool.submit(one, a, name, pads, k, args)
        base = {n: hash_compare.hashes(f.result()[0]) for n, f in base.items()}
        rows = []
        for k, (table, args) in enumerate(CANDIDATES):
            cells = []
            for name in paths:
                text = results[(k, name)].result()
                h = hash_compare.hashes(text)
                first, regions, common, only = hash_compare.compare(base[name], h)
                if not h:
                    cells.append("no run")
                elif first is None:
                    cells.append("same" + (f" ({len(common)} of {len(base[name])})" if only else ""))
                else:
                    regs = [r for r in regions if "." not in r and r != "state"]
                    cells.append(f"from {first}: {', '.join(regs) or ', '.join(regions)}")
            rows.append((table, label(args), cells))
    if a.markdown:
        print("| Table | Setting | " + " | ".join(paths) + " |")
        print("|---|---|" + "---|" * len(paths))
        for table, lab, cells in rows:
            print(f"| {table} | `{lab}` | " + " | ".join(cells) + " |")
    else:
        for table, lab, cells in rows:
            print(f"{table:4}  {lab:45}  " + "  |  ".join(cells))


if __name__ == "__main__":
    main()
