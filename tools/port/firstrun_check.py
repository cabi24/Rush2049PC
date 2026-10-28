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
        locked=(), copy_exe=False, program_files=None, runs=1, after=None, stdin=None, no_config=True):
    """Runs the program in a fresh folder holding files; checks its exit code and that its output has each of expect
    and none of unexpect. copy_exe runs a copy of the program from its own folder there, with program_files next to
    it. after(folders, output) returns more failures, looked for before the folders go."""
    global failures
    with tempfile.TemporaryDirectory(prefix="rush2049pc-firstrun-") as tmp:
        tmp = Path(tmp)
        work, data, conf, prog = tmp / "work", tmp / "data", tmp / "config", tmp / "program"
        for d in (work, data, conf, prog):
            d.mkdir()
        user = data / "rush2049pc"
        if not after:
            user.mkdir()
        if copy_exe:
            shutil.copy2(exe, prog / Path(exe).name)
            for dll in Path(exe).parent.glob("*.dll"):   # Windows: Mesa's opengl32.dll in CI
                shutil.copy2(dll, prog / dll.name)
            exe = str(prog / Path(exe).name)
        for name, content in (program_files or {}).items():
            (prog / name).write_bytes(content)
        for name, content in (files or {}).items():
            if content is None:
                (work / name).mkdir()
            else:
                (work / name).write_bytes(content)
        for name, content in (user_files or {}).items():
            user.mkdir(exist_ok=True)
            (user / name).write_bytes(content)
        for name in locked:
            os.chmod(work / name, 0)
        env = dict(os.environ, XDG_DATA_HOME=str(data), XDG_CONFIG_HOME=str(conf), HOME=str(tmp), APPDATA=str(data))
        for v in ("DISPLAY", "WAYLAND_DISPLAY", "RUSH2049_MESSAGE_BOX", "RUSH2049_WAIT_KEY"):
            env.pop(v, None)
        if box:
            env["RUSH2049_MESSAGE_BOX"] = box
        env.update(env_extra or {})
        cmd = [exe] + (["--no-config"] if no_config else []) + ["--audio", "null"] + \
            [a.replace("@WORK@", str(work)) for a in args]
        for _ in range(runs):
            try:
                p = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=60, input=stdin)
                out, rc = p.stdout.decode(errors="replace"), p.returncode
            except subprocess.TimeoutExpired:
                out, rc = "(timed out)", None
        for name in locked:   # readable again, for the clean-up
            os.chmod(work / name, stat.S_IRWXU)
        more = after({"work": work, "user": user, "program": prog}, out) if after else []
        out = out.replace(str(work), "@WORK@").replace(str(user), "@USER@").replace(str(prog), "@PROGRAM@")
    why = list(more)
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

    # The log (docs/port/RELEASE.md, "The log"): in the user data folder, with the version first, everything the
    # console got, a fault's report and backtrace; the run before kept; next to a portable program; --log, --no-log
    def log_has(folder, name, *want):
        def check(d, out):
            f = d[folder] / name
            if not f.exists():
                return [f"no {folder}/{name}"]
            text = f.read_text(encoding="utf-8", errors="replace")
            return [f"{name} lacks '{w}'" for w in want if w not in text]
        return check

    def both(*checks):
        return lambda d, out: sum((c(d, out) for c in checks), [])

    fault = "Access violation (read) (0xC0000005)" if win else "Segmentation fault"
    boot = ["--no-rom", "--headless", "--fast", "--seconds", "10"]
    run(exe, "log: a fault", boot, [f"the log is @USER@{sep}rush2049pc.log", fault],
        code=0xC0000005 if win else -11, env_extra={"RUSH_TRACE_WAVES": "1"},
        after=log_has("user", "rush2049pc.log", "rush2049pc ", "commit ", "command line:", "wave 0 loaded",
                      f"{fault} at address", "  #1 ", "the log is "))
    run(exe, "log: the run before", ["--window"], ["the log is "], runs=2,
        after=both(log_has("user", "rush2049pc.log", "no ROM: baserom.us.z64"),
                   log_has("user", "rush2049pc.old.log", "no ROM: baserom.us.z64", "started ")))
    run(exe, "log: portable", ["--window"], [f"the log is @PROGRAM@{sep}rush2049pc.log"], copy_exe=True,
        program_files={"rush2049pc.ini": b"[video]\nscale = 2\n"}, no_config=False,
        after=log_has("program", "rush2049pc.log", "portable (rush2049pc.ini next to the program)",
                      "It looked for baserom.us.z64 in:"))
    run(exe, "log: --log", ["--log", "my.log"], ["the log is my.log"], after=log_has("work", "my.log", "no ROM"))
    run(exe, "log: --no-log", ["--no-log"], ["no ROM"], ["the log is"],
        after=lambda d, out: [f"a log in {d['user']}"] if (d["user"] / "rush2049pc.log").exists() else [])
    run(exe, "--version", ["--version"], ["rush2049pc ", "commit ", "Linux" if not win else "Windows"], code=0,
        after=lambda d, out: ["--version wrote a log"] if d["user"].exists() else [])
    if win:   # the wait for a key after an error, forced (CI's console is not the program's own)
        run(exe, "wait for a key", ["--window"], ["press a key to close this window"], stdin=b"x\n",
            env_extra={"RUSH2049_WAIT_KEY": "1"})

    # What prints on stdout reaches a pipe (Windows: the GUI program's redirected handles; docs/port/WINDOWS.md,
    # "The console")
    run(exe, "--help", ["--help"], ["usage: rush2049pc", "--print-config"], code=0)
    run(exe, "--print-config", ["--print-config"], ["[game]", "; in effect: "], code=0)

    # Nobody to see the console (rush2049pc.exe double-clicked; RUSH2049_CONSOLE=none here): what is printed is kept,
    # and an error that no box explained yet shows a box with the last lines. The box is printed to the real stderr.
    def only_box(d, out):
        return [] if out.startswith("rush2049pc: message box:") else ["something printed outside the box"]

    quiet = {"RUSH2049_CONSOLE": "none"}
    run(exe, "no console: an error", ["a.z64", "b.z64"], [
        "message box: Rush 2049 PC stopped", "The program stopped with an error (exit code 2). Its last messages:",
        "\n    rush2049pc: one ROM at a time (a.z64, then b.z64)\n"], code=2, env_extra=quiet, after=only_box)
    run(exe, "no console: bad option", ["--frobnicate"], ["unknown option --frobnicate"], ["usage: rush2049pc"],
        code=2, env_extra=quiet, after=only_box)
    run(exe, "no console: no ROM", ["--window"], ["message box: Rush 2049 PC: no ROM"],
        ["message box: Rush 2049 PC stopped"], env_extra=quiet,
        after=both(only_box, log_has("user", "rush2049pc.log", "no ROM: baserom.us.z64", "the log is ")))
    run(exe, "no console: --version", ["--version"], [], ["rush2049pc "], code=0, env_extra=quiet)
    if win:   # a fault: its own box, the log has the report
        run(exe, "no console: a fault", boot, ["message box: Rush 2049 PC stopped", fault, "The log, with the "
                                               "details, is"], ["exit code"], code=0xC0000005, env_extra=quiet,
            after=both(only_box, log_has("user", "rush2049pc.log", f"{fault} at address", "  #1 ")))

    # Where saves go (docs/port/RELEASE.md, "Files"): the controller pak in the user data folder; one an earlier build
    # left in the working directory used where it is; --pak from the working directory; a portable copy's empty
    # rush2049pc.ini filled with the defaults, and everything next to the program
    def exists(folder, name, want=True):
        return lambda d, out: [] if (d[folder] / name).exists() == want else \
            [f"{folder}/{name} {'missing' if want else 'there'}"]

    pak = "rush2049pc.mpk"
    run(exe, "saves: installed", fast + ["synth.z64"], [f"saves (the controller pak): @USER@{sep}{pak}"], code=0,
        files={"synth.z64": synth}, after=both(exists("user", pak), exists("work", pak, False)))
    run(exe, "saves: earlier build's", fast + ["synth.z64"],
        [f"saves (the controller pak): @WORK@{sep}{pak}, in the working directory, where earlier builds kept it"],
        code=0, files={"synth.z64": synth, pak: bytes(32768)}, after=exists("user", pak, False))
    run(exe, "saves: --pak", fast + ["--pak", "mine.mpk", "synth.z64"], ["saves (the controller pak): mine.mpk"],
        code=0, files={"synth.z64": synth}, after=exists("work", "mine.mpk"))
    run(exe, "saves: portable", fast, ["wrote the default settings to @PROGRAM@", f"saves (the controller pak): "
                                       f"@PROGRAM@{sep}{pak}", f"@PROGRAM@{sep}baserom.us.z64 (next to the program"],
        code=0, copy_exe=True, program_files={"rush2049pc.ini": b"", "baserom.us.z64": synth}, no_config=False,
        after=both(exists("program", pak), exists("program", "rush2049pc.log"), exists("user", pak, False),
                   log_has("program", "rush2049pc.ini", "[game]", "pak_file = rush2049pc.mpk")))
    if not win:   # a window (SDL's offscreen driver): F12's folder is made
        run(exe, "screenshots folder", ["--window", "--fast", "--seconds", "0.5", "--no-rom-check", "synth.z64"],
            [f"saves (the controller pak): @USER@{sep}{pak}"], code=0, files={"synth.z64": synth},
            env_extra={"SDL_VIDEODRIVER": "offscreen"}, after=exists("user", "screenshots"))

    print(f"firstrun: {'FAILED' if failures else 'all passed'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
