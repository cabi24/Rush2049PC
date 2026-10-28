#!/usr/bin/env python3
"""Make a Rush2049PC texture pack from a texture dump with an upscaler you installed yourself.

    texpack.py DUMP PACK --upscaler PATH [--scale 4] [--model NAME] [--pad wrap|edge|none] [--min-size 4] [-j 1]

DUMP is a folder the game wrote with video.texture_dump (every texture it drew, named by its identity). Each PNG in it
is upscaled into PACK under the same name, which is what the game looks for with video.textures = pack. Files already
in PACK are left alone, so a run can be stopped and started again, and a texture can be edited by hand afterwards.

The upscaler is any program that takes `-i IN -o OUT -s SCALE` and writes a PNG, as realesrgan-ncnn-vulkan and its
relatives (realcugan-ncnn-vulkan, upscayl-bin) do; --model passes `-n NAME`. Nothing is downloaded or bundled: this
script only runs the program you name. With --upscaler builtin-nearest it makes a pack by pixel repetition, for trying
the pipeline without one.

Around the upscaler:
  - each texture is padded on every side (--pad wrap, the default: copies from the opposite edge, as the game's
    tiling textures repeat; edge: the edge texels repeated; none), upscaled, and cut back, so that a tiling texture
    gets no seam where it repeats;
  - a texture whose alpha is only 0 and 255 (a cut-out) gets its alpha cut at half again, so alpha-tested edges stay
    crisp in the game;
  - textures under --min-size texels on a side are skipped (the game draws the original).
The result must be exactly SCALE times the texture; anything else is reported and skipped.

docs/port/TEXTURES.md has the whole process. Your dump and your pack are made from your own ROM: keep them to
yourself.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

# ---------------------------------------------------------------- PNG (8-bit RGBA out; most kinds in)


def png_read(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not a PNG file')
    pos, idat, plte, trns = 8, [], None, None
    w = h = bits = ctype = interlace = None
    while pos + 12 <= len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, bits, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break
        pos += 12 + n
    if interlace or bits not in (1, 2, 4, 8, 16):
        raise ValueError('interlaced, or an unknown bit depth')
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = (w * ch * bits + 7) // 8
    bpp = max(1, ch * bits // 8)
    raw = zlib.decompress(b''.join(idat))
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f, cur = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = cur[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                cur[x] = (cur[x] + a) & 255
            elif f == 2:
                cur[x] = (cur[x] + b) & 255
            elif f == 3:
                cur[x] = (cur[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[x] = (cur[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(cur)
        prev = cur
    out = bytearray(w * h * 4)

    def sample(row, i):
        if bits == 16:
            return row[i * 2]
        if bits == 8:
            return row[i]
        bit = i * bits
        return (row[bit >> 3] >> (8 - bits - (bit & 7))) & ((1 << bits) - 1)

    scale = 255 // ((1 << bits) - 1) if bits < 8 else 1
    for y, row in enumerate(rows):
        for x in range(w):
            o = (y * w + x) * 4
            if ctype == 0:
                v = sample(row, x) * scale
                out[o:o + 4] = bytes((v, v, v, 255))
            elif ctype == 2:
                out[o:o + 4] = bytes((sample(row, x * 3), sample(row, x * 3 + 1), sample(row, x * 3 + 2), 255))
            elif ctype == 3:
                i = sample(row, x)
                a = trns[i] if trns is not None and i < len(trns) else 255
                out[o:o + 4] = bytes((plte[i * 3], plte[i * 3 + 1], plte[i * 3 + 2], a))
            elif ctype == 4:
                v = sample(row, x * 2)
                out[o:o + 4] = bytes((v, v, v, sample(row, x * 2 + 1)))
            else:
                out[o:o + 4] = bytes(sample(row, x * 4 + k) for k in range(4))
    return w, h, out


def png_write(path, w, h, rgba):
    raw = b''.join(b'\x00' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


# ---------------------------------------------------------------- the steps


def pad_image(w, h, rgba, p, mode):
    """The image with p texels added on each side: wrapped, or the edge repeated."""
    W, H = w + 2 * p, h + 2 * p
    out = bytearray(W * H * 4)
    for Y in range(H):
        y = Y - p
        y = y % h if mode == 'wrap' else min(max(y, 0), h - 1)
        for X in range(W):
            x = X - p
            x = x % w if mode == 'wrap' else min(max(x, 0), w - 1)
            out[(Y * W + X) * 4:(Y * W + X) * 4 + 4] = rgba[(y * w + x) * 4:(y * w + x) * 4 + 4]
    return W, H, out


def crop(w, h, rgba, x0, y0, cw, ch):
    out = bytearray(cw * ch * 4)
    for y in range(ch):
        out[y * cw * 4:(y + 1) * cw * 4] = rgba[((y0 + y) * w + x0) * 4:((y0 + y) * w + x0 + cw) * 4]
    return out


def nearest(w, h, rgba, s):
    out = bytearray(w * s * h * s * 4)
    for Y in range(h * s):
        for X in range(w * s):
            o, i = (Y * w * s + X) * 4, ((Y // s) * w + X // s) * 4
            out[o:o + 4] = rgba[i:i + 4]
    return out


def run_upscaler(args, src, dst):
    if args.upscaler == 'builtin-nearest':
        w, h, px = png_read(src)
        png_write(dst, w * args.scale, h * args.scale, nearest(w, h, px, args.scale))
        return
    cmd = [args.upscaler, '-i', src, '-o', dst, '-s', str(args.scale)]
    if args.model:
        cmd += ['-n', args.model]
    cmd += args.extra
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0 or not os.path.exists(dst):
        raise RuntimeError('the upscaler failed: %s\n%s' % (' '.join(cmd), r.stdout.decode(errors='replace')[-2000:]))


def one(args, name, tmp):
    src = os.path.join(args.dump, name)
    dst = os.path.join(args.pack, name)
    w, h, px = png_read(src)
    if w < args.min_size or h < args.min_size:
        return 'small'
    alphas = set(px[3::4])
    binary = alphas <= {0, 255} and len(alphas) > 1
    p = 0 if args.pad == 'none' else min(8, max(w, h))
    pw, ph, padded = pad_image(w, h, px, p, args.pad) if p else (w, h, px)
    a, b = os.path.join(tmp, 'in.png'), os.path.join(tmp, 'out.png')
    if os.path.exists(b):
        os.remove(b)
    png_write(a, pw, ph, padded)
    run_upscaler(args, a, b)
    ow, oh, up = png_read(b)
    s = args.scale
    if ow != pw * s or oh != ph * s:
        return 'size %dx%d, not %dx%d' % (ow, oh, pw * s, ph * s)
    out = crop(ow, oh, up, p * s, p * s, w * s, h * s) if p else up
    if binary:
        out[3::4] = bytes(255 if v >= 128 else 0 for v in out[3::4])
    tmpdst = dst + '.part'
    png_write(tmpdst, w * s, h * s, out)
    os.replace(tmpdst, dst)
    return None


def self_test():
    """A synthetic dump of two textures through builtin-nearest at 3x: sizes, texels, binary alpha, skips."""
    with tempfile.TemporaryDirectory() as d:
        dump, pack = os.path.join(d, 'dump'), os.path.join(d, 'pack')
        os.makedirs(dump)
        px = bytearray()
        for y in range(8):
            for x in range(8):
                px += bytes((x * 30, y * 30, (x ^ y) * 20, 255 if (x - 3.5) ** 2 + (y - 3.5) ** 2 < 9 else 0))
        png_write(os.path.join(dump, 'rgba16_8x8_0123456789abcdef.png'), 8, 8, px)
        png_write(os.path.join(dump, 'ia8_2x2_0123456789abcdef.png'), 2, 2, bytes(16))
        sys.argv = ['texpack', dump, pack, '--upscaler', 'builtin-nearest', '--scale', '3']
        if main() != 0:
            return 'the run failed'
        w, h, out = png_read(os.path.join(pack, 'rgba16_8x8_0123456789abcdef.png'))
        if (w, h) != (24, 24):
            return 'size %dx%d' % (w, h)
        if out != nearest(8, 8, px, 3):
            return 'the texels differ from the dump at 3x'
        if set(out[3::4]) - {0, 255}:
            return 'alpha is no longer binary'
        if os.path.exists(os.path.join(pack, 'ia8_2x2_0123456789abcdef.png')):
            return 'a 2x2 texture was not skipped'
        if main() != 0 or len(os.listdir(pack)) != 1:   # again: the file there is kept
            return 'the second run'
    return None


def main():
    if sys.argv[1:] == ['--self-test']:
        why = self_test()
        print('texpack self-test: %s' % ('ok' if why is None else 'FAIL: ' + why))
        return 0 if why is None else 1
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 epilog='docs/port/TEXTURES.md: making a texture pack')
    ap.add_argument('dump', help='the folder video.texture_dump wrote')
    ap.add_argument('pack', help='the pack folder to fill (video.texture_pack)')
    ap.add_argument('--upscaler', required=True,
                    help='the upscaler program (e.g. realesrgan-ncnn-vulkan), or builtin-nearest')
    ap.add_argument('--scale', type=int, default=4, help='2, 3 or 4 (default 4); the upscaler must support it')
    ap.add_argument('--model', help="the upscaler's model, passed as -n (e.g. realesrgan-x4plus)")
    ap.add_argument('--pad', choices=('wrap', 'edge', 'none'), default='wrap',
                    help='what surrounds each texture while it is upscaled (default wrap)')
    ap.add_argument('--min-size', type=int, default=4, help='skip textures smaller than this on a side (default 4)')
    ap.add_argument('--only', help='only the names that start with this (e.g. rgba16_ or ci8_)')
    ap.add_argument('extra', nargs='*', help='more options for the upscaler, after --')
    args = ap.parse_args()
    if args.upscaler != 'builtin-nearest' and shutil.which(args.upscaler) is None and not os.path.exists(args.upscaler):
        sys.exit('texpack: no upscaler at %s: install one yourself and give its path' % args.upscaler)
    names = sorted(n for n in os.listdir(args.dump) if n.lower().endswith('.png') and
                   (not args.only or n.startswith(args.only)))
    os.makedirs(args.pack, exist_ok=True)
    done = skipped = failed = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i, name in enumerate(names):
            if os.path.exists(os.path.join(args.pack, name)):
                skipped += 1
                continue
            try:
                why = one(args, name, tmp)
            except Exception as e:   # noqa: BLE001: report and go on
                why = str(e)
            if why is None:
                done += 1
            elif why == 'small':
                skipped += 1
            else:
                failed += 1
                print('texpack: %s: %s' % (name, why), file=sys.stderr)
            if (i + 1) % 25 == 0 or i + 1 == len(names):
                print('texpack: %d/%d: %d made, %d skipped, %d failed' % (i + 1, len(names), done, skipped, failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
