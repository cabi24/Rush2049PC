#!/usr/bin/env python3
"""Download and install the IDO compilers the build uses (`make setup`).

    python3 tools/setup_ido.py [--dest tools/ido]

Fetches decompals' ido-static-recomp v1.2 release archives, checks each
archive's SHA-256 before unpacking anything, and unpacks them into
<dest>/5.3 and <dest>/7.1 (the Makefile's default IDO53_DIR / IDO71_DIR).
The binaries are x86-64 Linux static recompilations of SGI's IDO.
"""
import argparse
import hashlib
import io
import sys
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RELEASE = "https://github.com/decompals/ido-static-recomp/releases/download/v1.2/"
ARCHIVES = {
    "5.3": ("ido-5.3-recomp-linux.tar.gz",
            "ab5c741561f80913d58c8b074771f23941a3edd312505a8ebed6d1dfeb65e506"),
    "7.1": ("ido-7.1-recomp-linux.tar.gz",
            "0d411696e178fcca34c31c3bf02011b928d7fd9c1fa7f8bf45070e0781b58e15"),
}


def install(version, dest):
    name, sha = ARCHIVES[version]
    out = dest / version
    if (out / "cc").is_file():
        print(f"setup_ido: IDO {version} already in {out}")
        return
    print(f"setup_ido: downloading {RELEASE + name}")
    data = urllib.request.urlopen(RELEASE + name, timeout=120).read()
    got = hashlib.sha256(data).hexdigest()
    if got != sha:
        sys.exit(f"setup_ido: {name} has sha256 {got}, expected {sha}; refusing to unpack")
    out.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for member in tar.getmembers():
            target = (out / member.name).resolve()
            if not str(target).startswith(str(out.resolve())) or member.issym() or member.islnk():
                sys.exit(f"setup_ido: refusing archive member {member.name}")
        tar.extractall(out)
    print(f"setup_ido: IDO {version} -> {out} (sha256 {sha[:12]}... verified)")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dest", default=str(ROOT / "tools" / "ido"))
    args = ap.parse_args(argv)
    for version in ARCHIVES:
        install(version, Path(args.dest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
