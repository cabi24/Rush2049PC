#!/usr/bin/env python3
"""The first run's messages, end to end (docs/port/RELEASE.md, "The ROM"): rush2049pc with no ROM, a missing one, a
file dropped on it, another game, another region, a bad dump in each byte order, a short file, an archive, a folder,
an unreadable file; the message box printed (RUSH2049_MESSAGE_BOX=print) or tried through SDL's dummy video driver;
--no-rom-check and the lookup order with the synthetic cartridge.

Every run happens in a temporary folder that is the working directory and the user data and settings folders
(XDG_DATA_HOME, XDG_CONFIG_HOME, HOME, APPDATA), so nothing of the user's is read or written. No retail ROM is read:
the files are the synthetic cartridge (tools/port/synth_rom.py) and copies of it with their header changed.

    firstrun_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
"""
import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

USA_SHA1 = "3f99351d7bb61656614bdb2aa1a90cfe55d1922c"
failures = 0


def header(rom, title=None, code=None, version=None):
    b = bytearray(rom)
    if title is not None:
        b[0x20:0x34] = title.encode().ljust(20, b" ")
    if code is not None:
        b[0x3B:0x3F] = code.encode()
    if version is not None:
        b[0x3F] = version
    return bytes(b)


def swap16(b):
    a = bytearray(b)
    a[0::2], a[1::2] = b[1::2], b[0::2]
    return bytes(a)


def swap32(b):
    a = bytearray(b)
    a[0::4], a[1::4], a[2::4], a[3::4] = b[3::4], b[2::4], b[1::4], b[0::4]
    return bytes(a)


def run(exe, case, args, expect, unexpect=(), code=1, env_extra=None, box="print", files=None, user_files=None,
        locked=()):
    """Runs the program in a fresh folder holding files; checks its exit code and that its output has each of expect
    and none of unexpect."""
    global failures
    with tempfile.TemporaryDirectory(prefix="rush2049pc-firstrun-") as tmp:
        tmp = Path(tmp)
        work, data, conf = tmp / "work", tmp / "data", tmp / "config"
        for d in (work, data, conf):
            d.mkdir()
        user = data / "rush2049pc"
        user.mkdir()
        for name, content in (files or {}).items():
            if content is None:
                (work / name).mkdir()
            else:
                (work / name).write_bytes(content)
        for name, content in (user_files or {}).items():
            (user / name).write_bytes(content)
        for name in locked:
            os.chmod(work / name, 0)
        env = dict(os.environ, XDG_DATA_HOME=str(data), XDG_CONFIG_HOME=str(conf), HOME=str(tmp), APPDATA=str(data))
        for v in ("DISPLAY", "WAYLAND_DISPLAY", "RUSH2049_MESSAGE_BOX"):
            env.pop(v, None)
        if box:
            env["RUSH2049_MESSAGE_BOX"] = box
        env.update(env_extra or {})
        cmd = [exe, "--no-config", "--audio", "null"] + [a.replace("@WORK@", str(work)) for a in args]
        try:
            p = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            out, rc = p.stdout.decode(errors="replace"), p.returncode
        except subprocess.TimeoutExpired:
            out, rc = "(timed out)", None
        for name in locked:   # readable again, for the clean-up
            os.chmod(work / name, stat.S_IRWXU)
        out = out.replace(str(work), "@WORK@").replace(str(user), "@USER@")
    why = []
    if rc != code:
        why.append(f"exit {rc}, expected {code}")
    why += [f"no '{e}'" for e in expect if e not in out]
    why += [f"unexpected '{e}'" for e in unexpect if e in out]
    print(f"firstrun: {case:24} {'ok' if not why else 'FAIL: ' + '; '.join(why)}")
    shown = [l for l in out.splitlines() if "wave " not in l and "wad " not in l]
    if why or os.environ.get("FIRSTRUN_VERBOSE"):
        print("\n".join(f"  | {l}" for l in shown[:60]))
    failures += bool(why)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--rom", required=True, help="the synthetic cartridge")
    a = ap.parse_args()
    exe = str(Path(a.exe).resolve())
    synth = Path(a.rom).read_bytes()
    win = sys.platform == "win32"
    sep = "\\" if win else "/"
    usa_header = header(synth, "Rush 2049", "NRUE", 0)
    drag = "drag the ROM file onto rush2049pc.exe" if win else "--rom PATH"

    # No ROM: the three places, in order, on the console and in the box
    run(exe, "no ROM", ["--window"], [
        "no ROM: baserom.us.z64 is in none of these places:",
        f"(next to the program)", f"@USER@{sep}baserom.us.z64 (the user data folder)",
        f"@WORK@{sep}baserom.us.z64 (the working directory)",
        "message box: Rush 2049 PC: no ROM", "It looked for baserom.us.z64 in:", drag])
    run(exe, "no ROM, headless", ["--headless"], ["no ROM: baserom.us.z64 is in none"], ["message box:"])
    run(exe, "--rom missing", ["--window", "--rom", "missing.z64"],
        ["no ROM at missing.z64 (the path given; the working directory is @WORK@)", "message box:",
         "There is no ROM at"], ["none of these places"])
    run(exe, "dropped, missing", ["--window", f"@WORK@{sep}gone.z64"],
        [f"no ROM at @WORK@{sep}gone.z64 (the path given)", "message box:"])
    run(exe, "two dropped", ["a.z64", "b.z64"], ["one ROM at a time"], code=2)
    run(exe, "game.rom absolute", ["--window", "--set", f"game.rom=@WORK@{sep}x.z64"],
        [f"no ROM at @WORK@{sep}x.z64 (the path given)"])

    # Files that aren't the USA cartridge: what each is, its SHA-1 when it is an N64 ROM, and what is needed
    need = f"This program needs San Francisco Rush 2049 (USA), SHA-1 {USA_SHA1}."
    run(exe, "another game", ["--window"], [
        "can't be used: it is another N64 game: \"SOME OTHER RACER\" (game code NXRE)", "its SHA-1 is ",
        "message box: Rush 2049 PC: wrong ROM", need], ["--no-rom-check runs it"],
        files={"baserom.us.z64": header(synth, "SOME OTHER RACER", "NXRE", 0)})
    run(exe, "Europe", ["--window"], ["San Francisco Rush 2049 for Europe (game code NRUP)", need,
                                      "--no-rom-check runs it anyway"],
        files={"baserom.us.z64": header(synth, "Rush 2049", "NRUP", 0)})
    run(exe, "revision 1", ["--window"], ["revision 1 of San Francisco Rush 2049 (USA)"],
        files={"baserom.us.z64": header(synth, "Rush 2049", "NRUE", 1)})
    for order, image in (("z64", usa_header), ("v64", swap16(usa_header)), ("n64", swap32(usa_header))):
        run(exe, f"bad dump .{order}", ["--window"],
            ["its header says San Francisco Rush 2049 (USA), but its contents differ", "its SHA-1 is ", need],
            files={"baserom.us.z64": image})
    run(exe, "cut dump", ["--window"], ["but it is 4194304 bytes, not 12582912 (12 MiB)"],
        files={"baserom.us.z64": usa_header[:4 << 20]})
    run(exe, "short file", ["--window"], ["it is 2048 bytes, too small for an N64 ROM", need], ["its SHA-1"],
        files={"baserom.us.z64": synth[:2048]})
    run(exe, "empty file", ["--window"], ["it is empty"], files={"baserom.us.z64": b""})
    run(exe, "not a ROM", ["--window"], ["it is not an N64 ROM: it starts with 23 20 6E 6F"],
        files={"baserom.us.z64": b"# not a rom" * 1000})
    zpath = Path(tempfile.mkdtemp(prefix="rush2049pc-zip-")) / "r.zip"
    with zipfile.ZipFile(zpath, "w") as z:
        z.writestr("rom.z64", synth[:65536])
    run(exe, "a zip", ["--window", "--rom", "r.zip"], ["it is a ZIP archive, not a ROM: extract the ROM from it first"],
        files={"r.zip": zpath.read_bytes()})
    shutil.rmtree(zpath.parent)
    run(exe, "a folder", ["--window"], ["it is a folder, not a file"], files={"baserom.us.z64": None})
    if not win and os.geteuid() != 0:
        run(exe, "unreadable", ["--window", "--rom", "locked.z64"], ["it cannot be opened: Permission denied"],
            files={"locked.z64": usa_header}, locked=["locked.z64"])

    # --no-rom-check, the lookup order and a dropped file: the synthetic cartridge runs
    ok = ["running it anyway (--no-rom-check)"]
    fast = ["--headless", "--fast", "--seconds", "0.5", "--no-rom-check"]
    run(exe, "dropped, --no-rom-check", fast + ["synth.z64"], ok + ["synth.z64 (the path given)"], code=0,
        files={"synth.z64": synth})
    run(exe, "user data folder", fast, ok + [f"@USER@{sep}baserom.us.z64 (the user data folder)"], code=0,
        user_files={"baserom.us.z64": synth})
    run(exe, "working dir, .v64", fast, ok + [f"@WORK@{sep}baserom.us.z64 (the working directory)"], code=0,
        files={"baserom.us.z64": swap16(synth)})

    # The real message box call, with nothing to show it on (SDL's dummy video driver, no display): it fails
    # quietly and the program still exits
    if not win:
        run(exe, "SDL box, no display", ["--window"], ["no ROM: baserom.us.z64 is in none"], ["message box: Rush"],
            box=None, env_extra={"SDL_VIDEODRIVER": "dummy"})

    print(f"firstrun: {'FAILED' if failures else 'all passed'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
