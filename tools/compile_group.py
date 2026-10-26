#!/usr/bin/env python3
"""Compile an IDO -O3 whole-program call group into one object.

    python3 tools/compile_group.py IDO_DIR GROUP_DIR OUTPUT

A group (GROUP_DIR/group.json: members, context, files, keep, flags) is the
source of functions whose register allocation was decided by IDO's
interprocedural optimisation across several of the original files. They only
reproduce when compiled together as one program, with the linker told which
procedures stay external (`keep`). The `cc` driver always passes
-preserve_dead_code to uld, which keeps every global external and so defeats
IPA, so the stages are run by hand: `cc -j` (ucode only) per the group flags,
then uld -kp keep.txt -> usplit -> umerge -> uopt -> ugen -> as1, with the
same stage arguments `cc -O3 -c` uses, in a private temporary directory (IDO
writes its intermediate files into the working directory).
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def stages(ido, flags, files):
    units = [re.sub(r"\.c$", ".u", f) for f in files]
    opt = "-O3"
    t = lambda name: os.path.join(ido, name)  # noqa: E731
    return [
        ([t("cc"), "-j", *flags.split(), *files], "cc.log"),
        ([t("uld"), "-L/usr/lib/mips2/nonshared", "-_SYSTYPE_SVR4", "-mips2", "-non_shared",
          "-g0", "-no_AutoGnum", "-kp", "keep.txt", *units, "-ko", "linked"], None),
        ([t("usplit"), "-mips2", "-o", "split", "-t", "st", "linked"], None),
        ([t("umerge"), "-Olimit", "5000", "-mips2", "-EB", "-g0", opt, "split",
          "-o", "merged", "-t", "st"], os.devnull),
        ([t("uopt"), "-G", "0", "-Olimit", "5000", "-mips2", "-EB", "-g0", opt, "merged",
          "opt", "-t", "st", "optlog"], os.devnull),
        ([t("ugen"), "-G", "0", "-mips2", "-EB", "-g0", opt, "opt", "-o", "gen", "-t", "st",
          "-temp", "ugtmp"], os.devnull),
        ([t("as1"), "-elf", "-G", "0", "-p0", "-mips2", "-EB", "-g0", opt, "-r4300_mul",
          "-Olimit", "5000", "gen", "-o", "group.o", "-t", "st"], os.devnull),
    ]


def main(argv):
    if len(argv) != 3:
        sys.exit(__doc__)
    ido, group_dir, out = os.path.abspath(argv[0]), Path(argv[1]), Path(argv[2])
    spec = json.loads((group_dir / "group.json").read_text())
    if "-O3" not in spec["flags"].split():
        sys.exit(f"{group_dir}: an IPA group must be compiled at -O3")
    work = Path(tempfile.mkdtemp(prefix="ido-group-"))
    try:
        for name in spec["files"]:
            shutil.copy(group_dir / name, work / name)
        (work / "keep.txt").write_text("".join(f"{k}\n" for k in spec["keep"]))
        for cmd, log in stages(ido, spec["flags"], spec["files"]):
            if log == "cc.log":
                with open(work / log, "w") as handle:
                    rc = subprocess.call(cmd, cwd=work, stdout=handle, stderr=subprocess.STDOUT)
            elif log:
                with open(log, "w") as handle:
                    rc = subprocess.call(cmd, cwd=work, stdout=handle)
            else:
                rc = subprocess.call(cmd, cwd=work)
            if rc != 0:
                detail = (work / "cc.log").read_text()[-2000:] if (work / "cc.log").is_file() else ""
                sys.exit(f"compile_group {group_dir.name}: {Path(cmd[0]).name} failed ({rc})\n{detail}")
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(work / "group.o", out)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
