#!/usr/bin/env python3
"""rush2049pc.exe's PE header and resources (docs/port/WINDOWS.md, "The console" and "Icon and version"):

  - the subsystem: GUI (Windows opens no console window with it), or console with --console (RUSH_WINDOWS_CONSOLE);
  - the image base and ASLR the port relies on (WINDOWS.md 3): 0x140000000, high-entropy and dynamic base;
  - the icon: one icon group with 16 to 256 px images, each one there;
  - the version information: ProductName Rush2049PC, FileVersion and ProductVersion as git describe gave them
    (the build's pc_version_rc.h), and the same numbers in the fixed part.

Pure Python, so it runs where the program was built, a Linux host cross-compiling included:

    pe_check.py --exe build/win/rush2049pc.exe --version-file build/win/version/pc_version_rc.h
"""
import argparse
import re
import struct
import sys

RT_ICON, RT_GROUP_ICON, RT_VERSION = 3, 14, 16
SIZES = {16, 20, 24, 32, 40, 48, 64, 128, 256}


class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise SystemExit("pe_check: not an MZ executable")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise SystemExit("pe_check: no PE signature")
        nsec, opt_size = struct.unpack_from("<H", data, pe + 6)[0], struct.unpack_from("<H", data, pe + 20)[0]
        opt = pe + 24
        if struct.unpack_from("<H", data, opt)[0] != 0x20B:
            raise SystemExit("pe_check: not PE32+ (64-bit)")
        self.image_base = struct.unpack_from("<Q", data, opt + 24)[0]
        self.subsystem, self.dll_chars = struct.unpack_from("<HH", data, opt + 68)
        self.rsrc_rva, self.rsrc_size = struct.unpack_from("<II", data, opt + 112 + 2 * 8)
        self.sections = []
        for i in range(nsec):
            s = opt + opt_size + 40 * i
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, s + 8)
            self.sections.append((va, max(vsize, rsize), raw))

    def offset(self, rva):
        for va, size, raw in self.sections:
            if va <= rva < va + size:
                return raw + rva - va
        raise SystemExit(f"pe_check: RVA 0x{rva:x} is in no section")

    def resources(self):
        """{type: {name: data}} (the first language of each)."""
        if not self.rsrc_rva:
            return {}
        base = self.offset(self.rsrc_rva)

        def entries(off):
            named, ids = struct.unpack_from("<HH", self.data, base + off + 12)
            for i in range(named + ids):
                name, target = struct.unpack_from("<II", self.data, base + off + 16 + 8 * i)
                yield (name if not name & 0x80000000 else f"name@{name & 0x7FFFFFFF:x}"), target

        out = {}
        for rtype, t in entries(0):
            if not t & 0x80000000:
                continue
            for name, n in entries(t & 0x7FFFFFFF):
                for _, leaf in entries(n & 0x7FFFFFFF):
                    rva, size = struct.unpack_from("<II", self.data, base + leaf)
                    out.setdefault(rtype, {})[name] = self.data[self.offset(rva):self.offset(rva) + size]
                    break
        return out


def version_strings(blob):
    """The StringFileInfo values of a VS_VERSIONINFO, by walking its nodes: wLength, wValueLength, wType, key."""
    out = {}

    def node(off, end):
        while off + 6 <= end:
            length, vlen, vtype = struct.unpack_from("<HHH", blob, off)
            if length == 0:
                break
            k = off + 6
            key_end = k
            while blob[key_end:key_end + 2] != b"\0\0":
                key_end += 2
            key = blob[k:key_end].decode("utf-16-le")
            v = (key_end + 2 + 3) & ~3
            if vtype == 1 and vlen:   # a string value: vlen is in characters, with the NUL
                out[key] = blob[v:v + 2 * vlen].decode("utf-16-le").rstrip("\0")
            children = (v + (2 * vlen if vtype == 1 else vlen) + 3) & ~3
            node(children, off + length)
            off = (off + length + 3) & ~3

    node(0, len(blob))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--exe", required=True)
    ap.add_argument("--version-file", required=True, help="the build's pc_version_rc.h")
    ap.add_argument("--console", action="store_true", help="a console build (RUSH_WINDOWS_CONSOLE)")
    a = ap.parse_args()
    pe = PE(open(a.exe, "rb").read())
    header = open(a.version_file).read()
    want_str = re.search(r'PC_VERSION_STR "([^"]*)"', header).group(1)
    want_num = [int(x) for x in re.search(r"PC_VERSION_NUM ([0-9,]+)", header).group(1).split(",")]
    bad = []

    want_sub = 3 if a.console else 2
    if pe.subsystem != want_sub:
        bad.append(f"subsystem {pe.subsystem}, expected {want_sub} ({'console' if a.console else 'GUI'})")
    if pe.image_base != 0x140000000:
        bad.append(f"image base 0x{pe.image_base:x}, expected 0x140000000")
    for bit, name in ((0x20, "HIGH_ENTROPY_VA"), (0x40, "DYNAMIC_BASE"), (0x100, "NX_COMPAT")):
        if not pe.dll_chars & bit:
            bad.append(f"no {name}")

    res = pe.resources()
    groups = res.get(RT_GROUP_ICON, {})
    if len(groups) != 1:
        bad.append(f"{len(groups)} icon groups, expected 1")
    for name, grp in groups.items():
        _, kind, count = struct.unpack_from("<HHH", grp, 0)
        sizes = set()
        for i in range(count):
            w, h, _, _, _, bits, size, icon_id = struct.unpack_from("<BBBBHHIH", grp, 6 + 14 * i)
            sizes.add(w or 256)
            icon = res.get(RT_ICON, {}).get(icon_id)
            if icon is None or len(icon) != size:
                bad.append(f"icon image {icon_id} ({w or 256} px) missing or of the wrong size")
            if bits != 32:
                bad.append(f"icon image {icon_id}: {bits} bits a pixel")
        if sizes != SIZES:
            bad.append(f"icon sizes {sorted(sizes)}, expected {sorted(SIZES)}")
        print(f"pe_check: icon group {name}: {count} images, {', '.join(str(s) for s in sorted(sizes))} px")

    versions = res.get(RT_VERSION, {})
    if len(versions) != 1:
        bad.append(f"{len(versions)} version resources, expected 1")
    for blob in versions.values():
        strings = version_strings(blob)
        print("pe_check: version: " + "; ".join(f"{k} = {v}" for k, v in strings.items()))
        for key, want in (("ProductName", "Rush2049PC"), ("FileVersion", want_str), ("ProductVersion", want_str),
                          ("OriginalFilename", "rush2049pc.exe")):
            if strings.get(key) != want:
                bad.append(f"{key} is {strings.get(key)!r}, expected {want!r}")
        if "unofficial" not in strings.get("FileDescription", ""):
            bad.append("the file description doesn't say it is unofficial")
        fixed = blob.find(struct.pack("<I", 0xFEEF04BD))
        ms, ls = struct.unpack_from("<II", blob, fixed + 8)
        got = [ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF]
        if got != want_num:
            bad.append(f"file version {got}, expected {want_num}")

    print(f"pe_check: subsystem {pe.subsystem}, image base 0x{pe.image_base:x}, DLL characteristics "
          f"0x{pe.dll_chars:x}")
    for b in bad:
        print(f"pe_check: FAIL: {b}")
    print(f"pe_check: {'FAILED' if bad else 'all passed'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
