#!/usr/bin/env python3
"""game.saves = files on the synthetic cartridge, end to end (docs/port/SAVES.md 10).

    python3 tools/port/saves_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
                                      [--logs DIR] [SCENARIO ...]

Each scenario runs the game (the press scripts of tools/port/synth/, with --saves files) in a data folder of its own
under --logs (XDG_DATA_HOME, so the saves folder is DIR/NAME/xdg/rush2049pc/saves/p1) and checks the files the game
leaves and what its log says:

    import    synthetic ghosts made by tools/port/ghost.py (never the ROM's) and hostile files made from them
              (truncated, oversized, garbage, a bad CRC, a bad header checksum, hostile sizes, a track out of range,
              not a ghost, the other byte order) dropped into import/: the good ones imported (pinned, on the pak the
              game sees, the game reading the pak), each bad one refused with its reason, the game reaching its race
    migrate   an .mpk of a profile and two ghosts (ghost.py files2mpk), started with --saves files: each note a file,
              the .mpk unchanged, imported.txt; then ghost.py files2mpk of the folder gives the .mpk's notes again
    swap      a folder of ghosts, and the F1 Saves page (a # realtime script) pinning one at the main menu: the pak
              taken out and put back once the overlay closes, and the game reading it again
    game      the game's own save code: the press scripts that make a profile, read it after a restart, change it,
              save a ghost, delete a profile (tools/port/synth/saves_*.press), each run checked on disk
    race      an imported synthetic ghost raced in the ghost race on track 0: GhostStart takes it (players 2)

Exit status: the number of failed checks. Stdlib only, Python 3.9+.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GHOST = ROOT / "tools/port/ghost.py"
SYNTH = ROOT / "tools/port/synth"
FAILS = []


def check(ok, what):
    print(f"saves_check: {'ok  ' if ok else 'FAIL'} {what}")
    if not ok:
        FAILS.append(what)
    return ok


def save_hash(data):
    """The save code's checksum (format_string_parse), as port/ultra/pakfs.c's pc_save_hash."""
    r = 0
    for v in data:
        k = v & 7
        if k == 0:
            r = (r - v) & 0xFFFFFFFF
        elif k == 1:
            r |= v
        elif k == 2:
            r &= v
        elif k == 3:
            r ^= v
        elif k == 4:
            r = (r * v) & 0xFFFFFFFF
        elif k == 5:
            r //= v
        else:
            r = (r + v) & 0xFFFFFFFF
    return r


def fix_crc(b):
    """The R49S header's CRC-32 made right again (the field zero while it is computed)."""
    b = bytearray(b)
    b[0x10:0x14] = b"\0\0\0\0"
    struct.pack_into("<I", b, 0x10, zlib.crc32(bytes(b)) & 0xFFFFFFFF)
    return bytes(b)


def fix_header(b):
    """A ghost file's header checksum (over the note's +4..+0x44) made right, then its CRC."""
    b = bytearray(b)
    struct.pack_into("<I", b, 128, save_hash(b[128 + 4:128 + 0x44]))
    return fix_crc(bytes(b))


def make_ghost(path, track, backward, name, seconds, frames=900, car=2):
    r = subprocess.run([sys.executable, str(GHOST), "make", "--track", str(track), "--backward", str(backward),
                        "--name", name, "--time", str(seconds), "--frames", str(frames), "--car", str(car),
                        "--out", str(path)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode:
        print(r.stdout)
    return r.returncode == 0


def ghost_tool(*args):
    r = subprocess.run([sys.executable, str(GHOST)] + [str(a) for a in args], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    return r.returncode, r.stdout


def run_game(args, work, script, until, seconds, extra=(), realtime=False, env_extra=None, tag="run"):
    """One run in work/ (its XDG_DATA_HOME work/xdg): its exit code and log."""
    env = dict(os.environ, XDG_DATA_HOME=str(work / "xdg"), RUSH_STATE_EXACT="1")
    env.pop("XDG_CONFIG_HOME", None)
    if env_extra:
        env.update(env_extra)
    cmd = [str(Path(args.exe).resolve()), "--rom", str(Path(args.rom).resolve()), "--no-rom-check", "--headless",
           "--no-config", "--audio", "null", "--saves", "files", "--press-file", str(script), "--until-state", until,
           "--seconds", str(seconds), "--log", str(work / f"{tag}.game.log")] + list(extra)
    if not realtime:
        cmd.append("--fast")
    p = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=900)
    out = p.stdout.decode(errors="replace")
    (work / f"{tag}.log").write_text(out, encoding="utf-8")
    return p.returncode, out


def folder(work, player=1):
    return work / "xdg" / "rush2049pc" / "saves" / f"p{player}"


def files(d, ext):
    return sorted(p.name for p in d.glob(f"*{ext}")) if d.is_dir() else []


def fresh(logs, name):
    work = logs / name
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    return work


# ---------------------------------------------------------------- scenarios

def scenario_import(args, logs):
    work = fresh(logs, "import")
    imp = folder(work) / "import"
    imp.mkdir(parents=True)
    good = [("a.r49ghost", 0, 0, "SYNTHA", 40.0), ("b.r49ghost", 0, 1, "SYNTHB", 45.5)]
    for f, t, bk, n, s in good:
        if not check(make_ghost(imp / f, t, bk, n, s), f"ghost.py make {f}"):
            return
    a = (imp / "a.r49ghost").read_bytes()
    bad = {
        "truncated.r49ghost": (a[:300], "bytes"),
        "oversized.r49ghost": (a + b"\0" * 512, "header says"),
        "garbage.r49ghost": (b"R49S" + bytes(range(256)) * 3, ""),
        "notours.r49ghost": (b"PK\3\4 a zip, not a ghost", "not a Rush 2049 PC save file"),
        "crc.r49ghost": (a[:200] + bytes([a[200] ^ 0xFF]) + a[201:], "CRC"),
        "header.r49ghost": (fix_crc(a[:128] + struct.pack("<I", struct.unpack_from("<I", a, 128)[0] ^ 0x12345678) +
                                    a[132:]), "header's checksum"),
        "frames.r49ghost": (fix_header(a[:128 + 0x40] + struct.pack("<I", 3 * 72001) + a[128 + 0x44:]), "frames"),
        "inflate.r49ghost": (fix_header(a[:128 + 0x40] + struct.pack("<I", struct.unpack_from("<I", a, 128 + 0x40)[0] - 3)
                                        + a[128 + 0x44:]), "unzip"),
        "track.r49ghost": (fix_header(a[:128 + 6] + b"\x09" + a[128 + 7:]), "track"),
        "order.r49ghost": (fix_crc(a[:11] + b"B" + a[12:]), "byte order"),
    }
    for f, (data, _) in bad.items():
        (imp / f).write_bytes(data)
    code, out = run_game(args, work, SYNTH / "pak.press", "0x400000:300", 90)
    check(code == 0 and "state reached:" in out, f"import: the game runs to its race ({code})")
    for f, *_ in good:
        check(f"{f} imported as" in out, f"import: {f} imported")
    for f, (_, why) in bad.items():
        line = next((l for l in out.splitlines() if f"{f} refused:" in l), "")
        check(line and why in line, f"import: {f} refused, saying why: {line.split('refused: ')[-1] if line else '-'}")
    d = folder(work)
    check(len(files(d, ".r49ghost")) == 2, f"import: two ghosts in the library ({files(d, '.r49ghost')})")
    check(len(files(imp / "done", ".r49ghost")) == 2 and len(files(imp / "refused", ".r49ghost")) == len(bad),
          "import: import/done/ and import/refused/")
    pins = (d / "pins.txt").read_text().split("\n") if (d / "pins.txt").exists() else []
    check(len([p for p in pins if p]) == 2, "import: both pinned")
    check("the pak holds 2 notes" in out and "controller 1: the game reads the controller pak" in out,
          "import: both on the pak, and the game reads it")
    code, info = ghost_tool("validate", *[d / f for f in files(d, ".r49ghost")])
    check(code == 0, "import: ghost.py validates the library's files")


def scenario_migrate(args, logs):
    work = fresh(logs, "migrate")
    src = work / "src"
    src.mkdir()
    if not (check(make_ghost(src / "g1.r49ghost", 1, 0, "MIG", 50.0), "migrate: make g1") and
            check(make_ghost(src / "g2.r49ghost", 2, 1, "MIG", 55.0, frames=1200), "migrate: make g2")):
        return
    mpk = work / "old.mpk"
    code, out = ghost_tool("files2mpk", src, "--out", mpk)
    if not check(code == 0 and mpk.exists(), f"migrate: ghost.py files2mpk ({out.strip()[-80:]})"):
        return
    before = mpk.read_bytes()
    code, out = run_game(args, work, SYNTH / "pak.press", "0x400000:300", 90, extra=["--pak", str(mpk)])
    check(code == 0, f"migrate: the game runs ({code})")
    d = folder(work)
    check("imported 2 of 2 notes" in out, "migrate: the log says what it imported")
    check(mpk.read_bytes() == before, "migrate: the .mpk unchanged")
    check((d / "imported.txt").exists() and (d / "pak-id.bin").read_bytes() == before[:256],
          "migrate: imported.txt, and the pak's ID kept")
    check(len(files(d, ".r49ghost")) == 2, f"migrate: a file per note ({files(d, '.r49ghost')})")
    back = work / "back.mpk"
    code, _ = ghost_tool("files2mpk", d, "--out", back)
    if check(code == 0, "migrate: files2mpk of the folder"):
        check(back.read_bytes() == before, "migrate: mpk -> files -> mpk byte for byte (laid out from page 5)")
    code, out = run_game(args, work, SYNTH / "pak.press", "0x400000:300", 90, extra=["--pak", str(mpk)], tag="again")
    check(code == 0 and "imported" not in out.replace("0 imported", ""), "migrate: not imported a second time")


def scenario_swap(args, logs):
    work = fresh(logs, "swap")
    d = folder(work)
    d.mkdir(parents=True)
    if not (check(make_ghost(d / "fast.r49ghost", 0, 0, "SWAP", 40.0), "swap: make the faster ghost") and
            check(make_ghost(d / "slow.r49ghost", 0, 0, "SWAP", 50.0), "swap: make the slower one")):
        return
    code, out = run_game(args, work, ROOT / "tools/port/saves/swap.press", "0x10:200", 60, realtime=True)
    check(code == 0, f"swap: the run reaches its state ({code})")
    check("saves (controller 1): Pinned SWAP" in out, "swap: the Saves page pinned the slower ghost")
    lines = out.splitlines()
    at = next((i for i, l in enumerate(lines) if "taken out and put back" in l), -1)
    check(at >= 0, "swap: the pak taken out and put back")
    check(at >= 0 and any("controller 1: the game reads the controller pak" in l for l in lines[at:]),
          "swap: the game reads the pak again")
    pak = (d / "pak.txt").read_text() if (d / "pak.txt").exists() else ""
    check("slow.r49ghost" in pak and "fast.r49ghost" not in pak, f"swap: the pinned ghost on the pak ({pak.strip()})")
    # the same pin in a race: no swap there; the next start has it
    work = fresh(logs, "swap-race")
    d = folder(work)
    d.mkdir(parents=True)
    make_ghost(d / "fast.r49ghost", 0, 0, "SWAP", 40.0)
    make_ghost(d / "slow.r49ghost", 0, 0, "SWAP", 50.0)
    code, out = run_game(args, work, ROOT / "tools/port/saves/swap_race.press", "0x400000:400", 90, realtime=True)
    check(code == 0 and "saves (controller 1): Pinned SWAP" in out, f"swap: pinned during a race ({code})")
    check("taken out and put back" not in out, "swap: in a race the pak is not swapped")
    pak = (d / "pak.txt").read_text() if (d / "pak.txt").exists() else ""
    check("fast.r49ghost" in pak, f"swap: the race's pak kept ({pak.strip()})")
    code, out = run_game(args, work, SYNTH / "pak.press", "0x400000:300", 90, tag="next")
    pak = (d / "pak.txt").read_text() if (d / "pak.txt").exists() else ""
    check(code == 0 and "slow.r49ghost" in pak and "fast.r49ghost" not in pak,
          f"swap: the next start's pak has the pin ({pak.strip()})")


def header(script):
    """A press script's # until and # seconds."""
    until, seconds = "0x400000:300", 90
    for line in Path(script).read_text().splitlines():
        if line.startswith("# until:"):
            until = line.split(":", 1)[1].strip()
        elif line.startswith("# seconds:"):
            seconds = int(line.split(":", 1)[1].strip())
    return until, seconds


def run_script(args, work, script, tag, **kw):
    until, seconds = header(script)
    return run_game(args, work, script, until, seconds, tag=tag, **kw)


def race_lines(out):
    return [l for l in out.splitlines() if "game state 0x00400000" in l]


def scenario_game(args, logs):
    """The game's own save code on the files backend: tools/port/synth/profile.press and ghost.press, and their next
    runs in again/ (each on the folder the run before left)."""
    work = fresh(logs, "game")
    d = folder(work)
    code, out = run_script(args, work, SYNTH / "profile.press", "profile")
    check(code == 0, f"game: profile.press reaches its state ({code})")
    check(files(d, ".r49save") == ["SYN.r49save"] and not files(d, ".r49ghost"),
          f"game: CreatePlayer's profile is a file, SYN.r49save ({files(d, '.r49save')})")
    check("the game saved SYN.r49save" in out, "game: the log says the game saved it")
    check(not (work / "xdg" / "rush2049pc" / "rush2049pc.mpk").exists(), "game: no .mpk made")
    first = (d / "SYN.r49save").read_bytes() if (d / "SYN.r49save").exists() else b""
    code, out = run_script(args, work, SYNTH / "again" / "profile2.press", "profile2")
    check(code == 0, f"game: profile2 reads the profile back after a restart and races ({code})")
    now = (d / "SYN.r49save").read_bytes() if (d / "SYN.r49save").exists() else b""
    check(now and now != first and files(d, ".r49save") == ["SYN.r49save"] and not files(d / "trash", ".r49save"),
          "game: the profile changed in place (its file written again, nothing in the trash)")
    code, out = run_script(args, work, SYNTH / "again" / "delete2.press", "delete2")
    check(code == 0, f"game: delete2 deletes it ({code})")
    check(not files(d, ".r49save") and files(d / "trash", ".r49save") == ["SYN.r49save"],
          "game: the deleted profile in the trash, nothing removed")
    work = fresh(logs, "game-ghost")
    d = folder(work)
    code, out = run_script(args, work, SYNTH / "ghost.press", "ghost")
    ghosts = files(d, ".r49ghost")
    check(code == 0, f"game: ghost.press reaches its state ({code})")
    check(len(ghosts) == 1 and ghosts[0].startswith("track 1 backward - SYN - "),
          f"game: SaveGhost's ghost is a file ({ghosts})")
    if ghosts:
        r, o = ghost_tool("validate", d / ghosts[0], d / "SYN.r49save")
        check(r == 0, f"game: ghost.py validates the profile and the ghost ({o.strip()[-90:]})")
    code, out = run_script(args, work, SYNTH / "again" / "ghost2.press", "ghost2")
    check(code == 0, f"game: ghost2 races the saved ghost after a restart ({code})")
    check(any("players 2" in l and "mode 2" in l for l in race_lines(out)),
          "game: the ghost race has the ghost player (players 2)")
    check(len(files(d, ".r49ghost")) == 1 and len(files(d / "trash", ".r49ghost")) == 1,
          "game: the new ghost a file, the one SaveGhost replaced in the trash")


def scenario_race(args, logs):
    """A ghost of the tool's (synthetic) imported and raced as the player's own would be."""
    work = fresh(logs, "race")
    imp = folder(work) / "import"
    imp.mkdir(parents=True)
    if not check(make_ghost(imp / "friend.r49ghost", 0, 1, "FRIEND", 30.0), "race: ghost.py make a ghost"):
        return
    code, out = run_script(args, work, ROOT / "tools/port/saves/race_import.press", "race")
    check(code == 0, f"race: the ghost race reaches its state ({code})")
    check("friend.r49ghost imported as" in out, "race: the ghost imported")
    check("was deleted by the game" not in out, "race: the game kept it (read as a ghost)")
    check(any("players 2" in l and "mode 2" in l for l in race_lines(out)),
          f"race: GhostStart races it (players 2): {race_lines(out)[-1:]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--logs", default=str(ROOT / "build/synth/saves"))
    ap.add_argument("scenarios", nargs="*")
    args = ap.parse_args()
    logs = Path(args.logs).resolve()
    logs.mkdir(parents=True, exist_ok=True)
    known = {"import": scenario_import, "migrate": scenario_migrate, "game": scenario_game, "race": scenario_race,
             "swap": scenario_swap}
    for name in args.scenarios or list(known):
        if name not in known:
            check(False, f"no scenario {name}")
            continue
        print(f"saves_check: --- {name}")
        known[name](args, logs)
    print(f"saves_check: {len(FAILS)} failed")
    return len(FAILS)


if __name__ == "__main__":
    sys.exit(main())
