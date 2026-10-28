#!/usr/bin/env python3
"""The first run's messages, end to end (docs/port/RELEASE.md, "The ROM"): rush2049pc with no ROM, a missing one, a
file dropped on it, another game, another region, a bad dump in each byte order, a short file, an archive, a folder,
an unreadable file; the message box printed (RUSH2049_MESSAGE_BOX=print) or tried through SDL's dummy video driver;
--no-rom-check and the lookup order with the synthetic cartridge. The "Open ROM..." file dialog, its answers given by
RUSH2049_TEST_ROM_PICK: cancelled, quit, a wrong file then another, .v64 and .n64 dumps, the choice kept in
rush2049pc.ini and found by the next start. The saves: the controller pak backed up when it changed, the backups
rotated, a damaged pak set aside and the newest good backup put in its place.

Every run happens in a temporary folder that is the working directory and the user data and settings folders
(XDG_DATA_HOME, XDG_CONFIG_HOME, HOME, APPDATA), so nothing of the user's is read or written. No retail ROM is read:
the files are the synthetic cartridge (tools/port/synth_rom.py) and copies of it with their header changed.

    firstrun_check.py --exe build/pc/rush2049pc --rom build/synth/rush2049_synth.z64
"""
import argparse
import os
import shutil
import stat
import struct
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


def fresh_pak(changed=0):
    """A blank formatted controller pak as port/ultra/pakfile.c makes it (host order: little-endian), with one byte of
    a data page (no checksum covers it) set to changed: a save that changed."""
    img = bytearray(32768)
    img[0] = 0x81
    img[1:32] = bytes(range(1, 32))
    ident = bytearray(32)
    ident[0:8] = struct.pack("<II", 0xFFFFFFFF, 0x135F1A05)
    ident[0x18:0x1C] = struct.pack("<HBB", 1, 1, 0)
    words = struct.unpack_from("<14H", ident)
    ident[0x1C:0x20] = struct.pack("<HH", sum(words) & 0xFFFF, sum(~w & 0xFFFF for w in words) & 0xFFFF)
    for at in (0x20, 0x60, 0x80, 0xC0):
        img[at:at + 32] = ident
    inode = bytearray(256)
    for page in range(5, 128):
        inode[2 * page:2 * page + 2] = struct.pack("<H", 3)
    inode[1] = sum(inode[10:]) & 0xFF
    img[0x100:0x200] = img[0x200:0x300] = inode
    img[0x500] = changed
    return bytes(img)


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
                (work / name).parent.mkdir(parents=True, exist_ok=True)
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
        env.update({k: v.replace("@WORK@", str(work)) for k, v in (env_extra or {}).items()})
        cmd = [exe] + (["--no-config"] if no_config else []) + ["--audio", "null"] + \
            [a.replace("@WORK@", str(work)) for a in args]
        for _ in range(runs):
            try:
                p = subprocess.run(cmd, cwd=work, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   timeout=60, input=stdin)
                # LF line ends: on Windows the CRT writes CRLF to text-mode files and pipes
                out, rc = p.stdout.decode(errors="replace").replace("\r\n", "\n"), p.returncode
            except subprocess.TimeoutExpired:
                out, rc = "(timed out)", None
        for name in locked:   # readable again, for the clean-up
            os.chmod(work / name, stat.S_IRWXU)
        more = after({"work": work, "user": user, "program": prog, "config": conf / "rush2049pc"}, out) if after else []
        out = out.replace(str(work), "@WORK@").replace(str(user), "@USER@").replace(str(prog), "@PROGRAM@") \
            .replace(str(conf), "@CONFIG@")
    why = list(more)
    if code != "any" and rc != code:
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
        "message box: Rush 2049 PC: no ROM", "It looked for baserom.us.z64 in:", drag], ["Open ROM", "buttons:"])
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

    # The "Open ROM..." dialog (docs/port/RELEASE.md, "Choosing the ROM"): RUSH2049_TEST_ROM_PICK answers for the
    # player. The box asks; its answer is the first button while answers are left, else the last (Quit).
    def pick(*answers):
        return {"RUSH2049_TEST_ROM_PICK": "|".join(answers)}

    asked = "buttons: [Open ROM...] [Quit]; chosen: Open ROM..."
    again = "buttons: [Choose another...] [Quit]; chosen: "
    other = header(synth, "SOME OTHER RACER", "NXRE", 0)
    run(exe, "pick: cancelled", ["--window"], [
        "no ROM: baserom.us.z64 is in none of these places:", "message box: Rush 2049 PC: no ROM",
        "Open ROM... lets you choose the ROM file now.", asked, "file dialog (RUSH2049_TEST_ROM_PICK): :cancel",
        "no ROM chosen (the file dialog was cancelled)"], ["Rush 2049 PC stopped"], box=None, env_extra=pick(":cancel"))
    run(exe, "pick: quit", ["--window"], ["buttons: [Open ROM...] [Quit]; chosen: Quit", "no ROM chosen (Quit)"],
        ["file dialog"], box=None, env_extra=pick())
    run(exe, "pick: another game", ["--window"], [
        asked, f"the ROM @WORK@{sep}other.z64 (the file chosen) can't be used: it is another N64 game",
        "message box: Rush 2049 PC: wrong ROM", "You can choose another file, or quit.", again + "Quit",
        "no ROM chosen (Quit)"], ["--no-rom-check runs it"], box=None, files={"other.z64": other},
        env_extra=pick(f"@WORK@{sep}other.z64"))
    run(exe, "pick: Europe, a bad dump", ["--window"], [
        "San Francisco Rush 2049 for Europe (game code NRUP)", "--no-rom-check runs it anyway",
        "its header says San Francisco Rush 2049 (USA), but its contents differ", again + "Choose another...",
        "the file dialog was cancelled"], box=None,
        files={"eu.z64": header(synth, "Rush 2049", "NRUP", 0), "bad.v64": swap16(usa_header)},
        env_extra=pick(f"@WORK@{sep}eu.z64", f"@WORK@{sep}bad.v64", ":cancel"))
    run(exe, "pick: no dialog", ["--window"], ["no file dialog could be shown", "No file dialog could be shown.",
                                               "Copy your ROM"], box=None, env_extra=pick(":fail"))
    run(exe, "pick: a wrong ROM found", ["--window"], [
        "message box: Rush 2049 PC: wrong ROM", "(the working directory) can't be used: it is another N64 game",
        "buttons: [Open ROM...] [Quit]"], box=None, files={"baserom.us.z64": other}, env_extra=pick(":cancel"))
    run(exe, "pick: game.rom moved", ["--window", "--set", f"game.rom=@WORK@{sep}moved.z64"], [
        f"no ROM at @WORK@{sep}moved.z64 (the path given)", "There is no ROM at", asked], box=None,
        env_extra=pick(":cancel"))
    run(exe, "pick: headless", ["--headless"], ["no ROM: baserom.us.z64 is in none"],
        ["buttons:", "file dialog", "message box:"], box=None, env_extra=pick(":cancel"))
    run(exe, "pick: --no-rom", ["--no-rom", "--headless", "--fast", "--seconds", "0.2"], [], ["buttons:", "no ROM:"],
        code="any", box=None, env_extra=pick(":cancel"))
    if not win:   # the ROM chosen runs: a window (SDL's offscreen driver)
        play = ["--window", "--fast", "--seconds", "0.5", "--no-rom-check"]
        offscreen = {"SDL_VIDEODRIVER": "offscreen"}
        run(exe, "pick: chosen", play, [
            asked, f"warning: the ROM @WORK@{sep}synth.z64 (chosen; .z64): it is another N64 game",
            "the ROM chosen is not kept: no settings file (--no-config)", "saves (the controller pak)"], code=0,
            box=None, files={"synth.z64": synth}, env_extra=dict(offscreen, **pick(f"@WORK@{sep}synth.z64")))
        run(exe, "pick: text, then .v64", play, [
            f"the ROM @WORK@{sep}notes.txt (the file chosen) can't be used: it is not an N64 ROM",
            again + "Choose another...", f"the ROM @WORK@{sep}my roms{sep}synth.v64 (chosen; .v64)"], code=0,
            box=None, files={"notes.txt": b"# not a rom" * 1000, "my roms/synth.v64": swap16(synth)},
            env_extra=dict(offscreen, **pick(f"@WORK@{sep}notes.txt", f"@WORK@{sep}my roms{sep}synth.v64")))
        run(exe, "pick: .n64, relative", play, ["(chosen; .n64)", f"@WORK@{sep}synth.n64"], code=0, box=None,
            files={"synth.n64": swap32(synth)}, env_extra=dict(offscreen, **pick("synth.n64")))

        # The real dialog code (:dialog) with stand-ins for zenity and kdialog on PATH: each writes its arguments
        # next to itself and prints FAKE_PICK, or exits 1 (cancelled) when FAKE_PICK is empty
        fake = Path(tempfile.mkdtemp(prefix="rush2049pc-fakebin-"))
        for tool in ("zenity", "kdialog"):
            (fake / tool).write_text(f'#!/bin/sh\nprintf "%s\\n" "$@" > "{fake}/{tool}.args"\n'
                                     '[ -n "$FAKE_PICK" ] || exit 1\nprintf "%s\\n" "$FAKE_PICK"\n')
            (fake / tool).chmod(0o755)

        def used(tool, *want):
            def check(d, out):
                f = fake / f"{tool}.args"
                args = f.read_text() if f.exists() else ""
                if f.exists():
                    f.unlink()
                return [f"{tool} not run with '{w}'" for w in want if w not in args]
            return check

        path_env = f"{fake}:{os.environ.get('PATH', '/usr/bin:/bin')}"
        run(exe, "pick: zenity", play, [asked, "file dialog (RUSH2049_TEST_ROM_PICK): :dialog",
                                        f"the ROM @WORK@{sep}a b{sep}synth.z64 (chosen; .z64)"], code=0, box=None,
            files={"a b/synth.z64": synth}, after=used("zenity", "--file-selection", "*.z64 *.v64 *.n64"),
            env_extra=dict(offscreen, PATH=path_env, XDG_CURRENT_DESKTOP="GNOME",
                           FAKE_PICK=f"@WORK@{sep}a b{sep}synth.z64", **pick(":dialog")))
        run(exe, "pick: kdialog on KDE", play, ["(chosen; .z64)"], code=0, box=None, files={"synth.z64": synth},
            after=used("kdialog", "--getopenfilename"),
            env_extra=dict(offscreen, PATH=path_env, XDG_CURRENT_DESKTOP="KDE", FAKE_PICK=f"@WORK@{sep}synth.z64",
                           **pick(":dialog")))
        run(exe, "pick: zenity cancelled", ["--window"], ["no ROM chosen (the file dialog was cancelled)"], box=None,
            after=used("zenity", "--file-selection"),
            env_extra=dict(PATH=path_env, XDG_CURRENT_DESKTOP="GNOME", FAKE_PICK="", **pick(":dialog")))
        shutil.rmtree(fake)

        # The choice kept in rush2049pc.ini (the user settings folder here): the next start finds it there, with no
        # box and no dialog
        kept = f"@WORK@{sep}my roms{sep}synth.z64"

        def ini_has(d, out):
            ini = d["config"] / "rush2049pc.ini"
            text = ini.read_text() if ini.exists() else ""
            want = f"rom = {d['work']}{sep}my roms{sep}synth.z64"
            return [] if want in text else [f"rush2049pc.ini lacks '{want}'"]

        run(exe, "pick: kept in the ini", play, [
            f"game.rom = {kept} kept in @CONFIG@{sep}rush2049pc{sep}rush2049pc.ini"], code=0, box=None,
            no_config=False, files={"my roms/synth.z64": synth}, env_extra=dict(offscreen, **pick(kept)),
            after=ini_has)
        run(exe, "pick: the next start", play, [
            f"the ROM {kept} (the path in the settings (game.rom)): it is another N64 game"],
            ["message box:", "file dialog"], code=0, box=None, no_config=False, files={"my roms/synth.z64": synth}, runs=2, env_extra=dict(offscreen, **pick(kept)),
            after=ini_has)

    # The saves (docs/port/RELEASE.md, "Saves"): a changed pak backed up, the backups rotated; a damaged one set
    # aside, unchanged, and the newest good backup in its place, or a new pak
    mpk = "rush2049pc.mpk"
    play = ["--headless", "--fast", "--seconds", "0.5", "--no-rom-check", "synth.z64"]
    p1, p2, p3 = fresh_pak(1), fresh_pak(2), fresh_pak(3)

    def holds(folder, name, data):
        return lambda d, out: [] if (d[folder] / name).exists() and (d[folder] / name).read_bytes() == data else \
            [f"{folder}/{name} is not as expected"]

    def there(folder, name):
        return lambda d, out: [] if (d[folder] / name).exists() else [f"no {folder}/{name}"]

    def corrupt(data):
        def check(d, out):
            kept = list(d["user"].glob(mpk + ".corrupt-*"))
            if len(kept) != 1:
                return [f"{len(kept)} .corrupt- files"]
            return [] if kept[0].read_bytes() == data else ["the file set aside changed"]
        return check

    def all_of(*checks):
        return lambda d, out: sum((c(d, out) for c in checks), [])

    run(exe, "saves: backed up", play, [f"saves backed up, since they changed: @USER@{sep}{mpk}.bak"], code=0,
        files={"synth.z64": synth}, user_files={mpk: p1}, after=holds("user", mpk + ".bak", p1))
    run(exe, "saves: unchanged", play, [], ["backed up"], code=0, files={"synth.z64": synth},
        user_files={mpk: p1, mpk + ".bak": p1},
        after=lambda d, out: [".bak2 made"] if (d["user"] / (mpk + ".bak2")).exists() else [])
    run(exe, "saves: rotated", play, ["saves backed up"], code=0, files={"synth.z64": synth},
        user_files={mpk: p3, mpk + ".bak": p2, mpk + ".bak2": p1},
        after=all_of(holds("user", mpk + ".bak", p3), holds("user", mpk + ".bak2", p2),
                     holds("user", mpk + ".bak3", p1)))
    run(exe, "saves: damaged", play, [
        f"the saves (the controller pak) @USER@{sep}{mpk} can't be used: its ID block and the three copies of it are "
        "damaged", f"it is kept, unchanged, as @USER@{sep}{mpk}.corrupt-",
        "there is no good backup: the game starts with a new, empty controller pak"], code=0,
        files={"synth.z64": synth}, user_files={mpk: bytes(32768)},
        after=all_of(corrupt(bytes(32768)), there("user", mpk)))
    run(exe, "saves: restored", play, [
        "can't be used: it is 100 bytes, not 32 KiB", f"the saves are restored from @USER@{sep}{mpk}.bak2"], code=0,
        files={"synth.z64": synth}, user_files={mpk: b"x" * 100, mpk + ".bak": b"y" * 32768, mpk + ".bak2": p2},
        after=all_of(corrupt(b"x" * 100), holds("user", mpk, p2)))
    if not win:   # the box, with a window
        run(exe, "saves: the box", ["--window"] + play[1:], [
            "message box: Rush 2049 PC: saves", "Your saves, the controller pak", "Nothing was written over it",
            "The saves were restored from the backup of"], code=0, files={"synth.z64": synth},
            user_files={mpk: bytes(32768), mpk + ".bak": p1}, env_extra={"SDL_VIDEODRIVER": "offscreen"},
            after=holds("user", mpk, p1))

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
