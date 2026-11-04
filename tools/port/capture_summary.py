#!/usr/bin/env python3
"""What the voices of an audio capture (RUSH2049_AUDIO_CAPTURE, docs/port/AUDIO.md 8 and 10) played.

    python3 tools/port/capture_summary.py CAPTURE [--every N] [--top K]

The capture holds every audio task the game ran: its SFDs in the N64 form (32 voice records each, or more with
[audio] voices above original: the SFD array's size over its count gives how many), each source range, codebook and
output copied in. A voice is told by its sample: an ADPCM voice's codebook (the bytes, hashed: each sample has its own),
a PCM16 voice's first bytes. Prints, for every N tasks (default about a second), the voices a subframe (least, mean,
most), the samples playing, and the K samples with the most voices at once, each with its voices, its pitch range and
its mean level (the left and right volumes' high halves, 0-32767); then the same over the whole capture. Nothing of
the sound itself is printed, only counts. Stdlib only, Python 3.9+.
"""
import argparse
import hashlib
import struct
import sys
from collections import defaultdict

MX_SFD_VOICES, MX_V_SIZE = 0x10, 0x50


def tasks(data):
    if data[:8] != b"MXCAP1\0\0":
        raise SystemExit("capture_summary: not an MXCAP1 file")
    at = 8
    while at + 28 <= len(data):
        magic, count, nregions, nouts, flags, sfd_addr, sfd_size = struct.unpack_from("<7I", data, at)
        if magic != 0x4B534154:
            raise SystemExit(f"capture_summary: no task at byte {at}")
        at += 28
        regions = {sfd_addr: data[at:at + sfd_size]}
        sfd = regions[sfd_addr]
        at += sfd_size
        for _ in range(nregions - 1):
            addr, n = struct.unpack_from("<2I", data, at)
            regions[addr] = data[at + 8:at + 8 + n]
            at += 8 + n
        at += nouts * (4 + 384 * 2)
        yield count, sfd, regions


def region_bytes(regions, addr, n):
    for base, b in regions.items():
        if base <= addr < base + len(b):
            return b[addr - base:addr - base + n]
    return b""


def voices_of(count, sfd, regions):
    """Per SFD of the task, its voices: (sample key, pitch, level)."""
    stride = len(sfd) // count if count else 0
    n = (stride - MX_SFD_VOICES) // MX_V_SIZE if stride else 32
    while n > 32 and MX_SFD_VOICES + n * MX_V_SIZE + 4 * ((n - 32 + 31) // 32) > stride:
        n -= 1
    for k in range(count):
        o = sfd[k * stride:(k + 1) * stride]
        out = []
        size1 = struct.unpack_from(">H", o, MX_SFD_VOICES + 0x24 + 8)[0]
        if size1 == 0:
            yield out
            continue
        for i in range(n):
            r = o[MX_SFD_VOICES + i * MX_V_SIZE:MX_SFD_VOICES + (i + 1) * MX_V_SIZE]
            if len(r) < MX_V_SIZE:
                break
            envl, envr = struct.unpack_from(">ii", r, 0)
            pitch = struct.unpack_from(">H", r, 0x22)[0]
            frames = r[0x3C]
            if frames:
                book = struct.unpack_from(">I", r, 0x40)[0]
                key = "adpcm " + hashlib.sha1(region_bytes(regions, book, 256)).hexdigest()[:10]
            else:
                src = struct.unpack_from(">I", r, 0x24)[0]
                key = "pcm " + hashlib.sha1(region_bytes(regions, src, 64)).hexdigest()[:10]
            out.append((key, pitch, ((envl >> 16) + (envr >> 16)) / 2))
            if struct.unpack_from(">I", r, 0x44)[0]:
                break
        yield out


def report(label, subframes, top):
    if not subframes:
        return
    counts = [len(v) for v in subframes]
    most = defaultdict(int)
    pitches = defaultdict(list)
    levels = defaultdict(list)
    for v in subframes:
        per = defaultdict(int)
        for key, pitch, level in v:
            per[key] += 1
            pitches[key].append(pitch)
            levels[key].append(level)
        for key, c in per.items():
            most[key] = max(most[key], c)
    print(f"{label}: {len(subframes)} subframes, voices {min(counts)}/{sum(counts) / len(counts):.1f}/{max(counts)} "
          f"(least/mean/most), {len(most)} samples")
    for key in sorted(most, key=lambda k: -most[k])[:top]:
        p = pitches[key]
        print(f"  {key}: up to {most[key]} voices at once, pitch {min(p) / 4096:.2f}-{max(p) / 4096:.2f}, "
              f"level {sum(levels[key]) / len(levels[key]):.0f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("capture")
    ap.add_argument("--every", type=int, default=60, help="tasks a line")
    ap.add_argument("--top", type=int, default=6)
    a = ap.parse_args()
    data = open(a.capture, "rb").read()
    window, total, t = [], [], 0
    for count, sfd, regions in tasks(data):
        for v in voices_of(count, sfd, regions):
            window.append(v)
        t += 1
        if t % a.every == 0:
            report(f"tasks {t - a.every + 1}-{t}", window, a.top)
            total += window
            window = []
    total += window
    report(f"all {t} tasks", total, a.top)
    return 0


if __name__ == "__main__":
    sys.exit(main())
