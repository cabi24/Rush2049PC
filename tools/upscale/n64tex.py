"""N64-style texture degradation, synthetic test images, classical 2x scalers and metrics (docs/port/UPSCALE.md).

A training pair is a clean high-resolution image HR (2N x 2N) and LR = degrade(HR) (N x N): blurred a little,
downscaled 2x with one of several filters, then quantised to one of the N64's texel formats the way the port's
renderer expands them (port/gfx/gfx_rdp.c): RGBA5551, CI4/CI8 (a palette of RGBA5551 entries, from median cut,
octree or k-means), I4/IA4 (3-bit)/I8 intensity, with or without dithering. Intensity formats make HR grey too.
All randomness comes from the numpy Generator passed in, so a seed fixes a pair.
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

FORMATS = ["rgba16", "ci4", "ci8", "i4", "ia4", "i8", "rgba32"]
FORMAT_WEIGHTS = [0.24, 0.26, 0.14, 0.12, 0.04, 0.10, 0.10]

BAYER4 = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) + 0.5) / 16 - 0.5


def luma(rgb):
    """BT.601 luma of float RGB (..., 3)."""
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def expand5(q):
    q = q.astype(np.int32)
    return ((q << 3) | (q >> 2)).astype(np.float32) / 255


def dither_offsets(shape, kind, rng, levels):
    """Per-texel offsets in units of one quantisation step for 'none', 'bayer' or 'noise'."""
    h, w = shape
    if kind == "bayer":
        oy, ox = rng.integers(0, 4, 2)
        t = np.tile(BAYER4, (h // 4 + 2, w // 4 + 2))[oy:oy + h, ox:ox + w]
        return t * rng.uniform(0.5, 1.0)
    if kind == "noise":
        return rng.uniform(-0.5, 0.5, (h, w)).astype(np.float32) * rng.uniform(0.4, 1.0)
    return np.zeros((h, w), np.float32)


def quant_levels(x, bits, dither):
    """x in [0,1] to `bits` bits with offsets `dither` (steps), expanded back as the RDP's texel units do."""
    n = (1 << bits) - 1
    d = dither[..., None] if x.ndim == 3 else dither
    q = np.clip(np.floor(x * n + 0.5 + d), 0, n)
    if bits == 5:
        return expand5(q)
    if bits == 3:   # IA4's intensity: (v >> 1) * 255 / 7
        return np.floor(q * 255 / 7).astype(np.float32) / 255
    return (q * (255 // n)).astype(np.float32) / 255   # 4 bits: v * 17; 8 bits: v


def kmeans_palette(px, k, rng, iters=8):
    """k-means over the texels (M, 3) in [0,1]; returns (k', 3)."""
    uniq = np.unique(px, axis=0)
    if len(uniq) <= k:
        return uniq
    c = uniq[rng.choice(len(uniq), k, replace=False)]
    for _ in range(iters):
        d = ((px[:, None, :] - c[None, :, :]) ** 2).sum(-1)
        a = d.argmin(1)
        for j in range(k):
            m = a == j
            if m.any():
                c[j] = px[m].mean(0)
    return c


def palette_quantise(rgb, ncolors, method, dither, rng):
    """CI4/CI8: a palette of RGBA5551 colours, mapped with or without Floyd-Steinberg / ordered dithering."""
    h, w, _ = rgb.shape
    img = Image.fromarray(np.clip(rgb * 255 + 0.5, 0, 255).astype(np.uint8))
    if method == "kmeans":
        pal = kmeans_palette(rgb.reshape(-1, 3), ncolors, rng)
    else:
        m = Image.Quantize.MEDIANCUT if method == "mediancut" else Image.Quantize.FASTOCTREE
        q = img.quantize(colors=ncolors, method=m, dither=Image.Dither.NONE)
        pal = np.array(q.getpalette()[:3 * ncolors], np.float32).reshape(-1, 3) / 255
        used = np.unique(np.array(q))
        pal = pal[used[used < len(pal)]]
    pal = expand5(np.clip(np.floor(pal * 31 + 0.5), 0, 31))   # TLUT entries are RGBA5551
    pal = np.unique(pal, axis=0)
    if dither == "fs":
        full = np.concatenate([pal, np.repeat(pal[:1], 256 - len(pal), axis=0)])   # unused entries: a copy
        palimg = Image.new("P", (1, 1))
        palimg.putpalette(list(np.clip(full * 255 + 0.5, 0, 255).astype(np.uint8).reshape(-1)))
        idx = np.array(img.quantize(palette=palimg, dither=Image.Dither.FLOYDSTEINBERG))
        return full[idx]
    src = rgb
    if dither in ("bayer", "noise"):   # ordered dither: offset by about one palette spacing
        step = 1.0 / max(2.0, ncolors ** (1 / 3))
        src = np.clip(rgb + dither_offsets((h, w), dither, rng, 0)[..., None] * step, 0, 1)
    d = ((src.reshape(-1, 1, 3) - pal[None]) ** 2).sum(-1)
    return pal[d.argmin(1)].reshape(h, w, 3)


def downscale2(hr, method):
    """Downscale float RGB (2N, 2N, 3) by 2 with a PIL filter ('box', 'bilinear', 'bicubic', 'lanczos')."""
    h, w, _ = hr.shape
    img = Image.fromarray(np.clip(hr * 255 + 0.5, 0, 255).astype(np.uint8))
    f = {"box": Image.Resampling.BOX, "bilinear": Image.Resampling.BILINEAR, "bicubic": Image.Resampling.BICUBIC,
         "lanczos": Image.Resampling.LANCZOS}[method]
    return np.asarray(img.resize((w // 2, h // 2), f), np.float32) / 255


def blur(rgb, sigma):
    if sigma <= 0.05:
        return rgb
    img = Image.fromarray(np.clip(rgb * 255 + 0.5, 0, 255).astype(np.uint8))
    return np.asarray(img.filter(ImageFilter.GaussianBlur(sigma)), np.float32) / 255


def degrade(hr, rng, fmt=None, opts=None):
    """HR float RGB (2N, 2N, 3) -> (HR target, LR input, description). Intensity formats make the target grey."""
    o = dict(opts or {})
    fmt = fmt or rng.choice(FORMATS, p=FORMAT_WEIGHTS)
    grey = fmt in ("i4", "ia4", "i8")
    if grey:
        hr = np.repeat(luma(hr)[..., None], 3, axis=2)
    sigma = o.get("blur", rng.uniform(0.0, 0.9) if rng.random() < 0.5 else 0.0)
    method = o.get("down", rng.choice(["box", "bilinear", "bicubic", "lanczos"], p=[0.35, 0.2, 0.3, 0.15]))
    lr = downscale2(blur(hr, sigma), method)
    h, w, _ = lr.shape
    dkind = o.get("dither", rng.choice(["none", "bayer", "noise"], p=[0.55, 0.25, 0.2]))
    if fmt == "rgba16":
        lr = quant_levels(lr, 5, dither_offsets((h, w), dkind, rng, 31))
    elif fmt in ("ci4", "ci8"):
        n = o.get("colors", int(rng.integers(6, 17)) if fmt == "ci4" else int(rng.integers(64, 257)))
        pm = o.get("palette", rng.choice(["mediancut", "octree", "kmeans"], p=[0.45, 0.2, 0.35]) if n <= 32
                   else rng.choice(["mediancut", "octree"]))
        pd = o.get("pdither", rng.choice(["none", "fs", "bayer"], p=[0.55, 0.3, 0.15]))
        lr = palette_quantise(lr, n, pm, pd, rng)
        dkind = f"{pm}/{n}/{pd}"
    elif fmt in ("i4", "ia4", "i8"):
        bits = {"i4": 4, "ia4": 3, "i8": 8}[fmt]
        y = quant_levels(lr[..., 0], bits, dither_offsets((h, w), dkind if bits < 8 else "none", rng, 0))
        lr = np.repeat(y[..., None], 3, axis=2)
    else:
        lr = np.floor(lr * 255 + 0.5) / 255
    return hr, lr.astype(np.float32), f"{fmt} {method} blur{sigma:.2f} {dkind}"


def colour_jitter(rgb, rng):
    """N64 art is often more saturated and contrasty than photographs."""
    y = luma(rgb)[..., None]
    s = rng.uniform(0.8, 1.7)
    out = y + (rgb - y) * s
    c = rng.uniform(0.85, 1.3)
    m = out.mean()
    out = (out - m) * c + m + rng.uniform(-0.08, 0.08)
    if rng.random() < 0.3:   # channel swaps keep hues varied (lots of photo textures are brown and grey)
        out = out[..., rng.permutation(3)]
    return np.clip(out, 0, 1).astype(np.float32)


def random_colour(rng):
    if rng.random() < 0.5:   # saturated, as paint and signs
        c = np.zeros(3, np.float32)
        c[rng.integers(3)] = 1
        c[rng.integers(3)] = rng.uniform(0, 1)
        return np.clip(c * rng.uniform(0.5, 1.0) + rng.uniform(0, 0.25), 0, 1)
    return rng.uniform(0, 1, 3).astype(np.float32)


def synthetic(size, rng, background=None):
    """Flat-colour art (shapes, strokes, glyph-like marks, stripes) drawn at 4x and reduced: sharp edges, as signs,
    decals and painted textures have. `background` is an optional float RGB (size, size, 3) under the shapes."""
    s4 = size * 4
    if background is not None:
        img = Image.fromarray(np.clip(background * 255 + 0.5, 0, 255).astype(np.uint8)).resize((s4, s4))
    elif rng.random() < 0.4:   # gradient
        a, b = random_colour(rng), random_colour(rng)
        t = np.linspace(0, 1, s4, dtype=np.float32)
        g = t[None, :, None] if rng.random() < 0.5 else t[:, None, None]
        arr = a * (1 - g) + b * g
        img = Image.fromarray((np.broadcast_to(arr, (s4, s4, 3)) * 255).astype(np.uint8))
    else:
        img = Image.new("RGB", (s4, s4), tuple(int(v * 255) for v in random_colour(rng)))
    d = ImageDraw.Draw(img)

    def col():
        return tuple(int(v * 255) for v in random_colour(rng))

    if rng.random() < 0.25:   # stripes or a checkerboard
        p = int(rng.integers(8, 48))
        c1 = col()
        for i in range(0, s4 * 2, p * 2):
            if rng.random() < 0.5:
                d.polygon([(i, 0), (i + p, 0), (i + p - s4, s4), (i - s4, s4)], fill=c1)
            else:
                d.rectangle([i, 0, i + p - 1, s4], fill=c1)
    for _ in range(int(rng.integers(2, 10))):
        kind = rng.choice(["rect", "ellipse", "poly", "line", "glyphs"], p=[0.25, 0.2, 0.2, 0.15, 0.2])
        x0, y0 = rng.integers(-s4 // 4, s4, 2)
        sw, sh = rng.integers(s4 // 10, s4 // 2, 2)
        if kind == "rect":
            d.rectangle([x0, y0, x0 + sw, y0 + sh], fill=col(), outline=col() if rng.random() < 0.3 else None,
                        width=int(rng.integers(2, 8)))
        elif kind == "ellipse":
            d.ellipse([x0, y0, x0 + sw, y0 + sh], fill=col(), outline=col() if rng.random() < 0.3 else None,
                      width=int(rng.integers(2, 8)))
        elif kind == "poly":
            pts = [tuple(int(v) for v in rng.integers(-s4 // 4, s4 + s4 // 4, 2)) for _ in range(rng.integers(3, 7))]
            d.polygon(pts, fill=col())
        elif kind == "line":
            pts = [tuple(int(v) for v in rng.integers(0, s4, 2)) for _ in range(rng.integers(2, 5))]
            d.line(pts, fill=col(), width=int(rng.integers(3, 14)))
        else:   # rows of block-letter-like marks
            c = col()
            gh = int(rng.integers(12, 36))
            gw = int(gh * rng.uniform(0.5, 0.9))
            st = max(3, gh // 5)
            for gx in range(int(x0), int(x0) + int(sw) * 2, gw + st):
                for _ in range(int(rng.integers(2, 5))):
                    if rng.random() < 0.5:
                        yy = int(y0) + int(rng.integers(0, gh - st + 1))
                        d.rectangle([gx, yy, gx + gw, yy + st], fill=c)
                    else:
                        xx = gx + int(rng.integers(0, gw - st + 1))
                        d.rectangle([xx, int(y0), xx + st, int(y0) + gh], fill=c)
    filt = Image.Resampling.LANCZOS if rng.random() < 0.5 else Image.Resampling.BOX
    return np.asarray(img.resize((size, size), filt), np.float32) / 255


# ---------------------------------------------------------------- classical 2x scalers (numpy reference)

CUBIC_PHASE = (np.array([-3, 29, 111, -9], np.float32) / 128, np.array([-9, 111, 29, -3], np.float32) / 128)


def pad(x, n, wrap_s, wrap_t):
    x = np.pad(x, ((0, 0), (n, n), (0, 0)), mode="wrap" if wrap_s else "edge")
    return np.pad(x, ((n, n), (0, 0), (0, 0)), mode="wrap" if wrap_t else "edge")


def _axis2x(x, taps, axis):
    """x padded by 2 along `axis`; taps = (phase0 kernel over i-2..i+1, phase1 over i-1..i+2)."""
    n = x.shape[axis] - 4
    def sl(a, b):
        idx = [slice(None)] * x.ndim
        idx[axis] = slice(a, a + n)
        return x[tuple(idx)]
    p0 = sum(taps[0][k] * sl(k, 0) for k in range(4))
    p1 = sum(taps[1][k] * sl(k + 1, 0) for k in range(4))
    out = np.stack([p0, p1], axis=axis + 1)
    shape = list(p0.shape)
    shape[axis] *= 2
    return out.reshape(shape)


def cubic2x(x, wrap_s=0, wrap_t=0):
    """Catmull-Rom 2x of float (h, w, c), sample positions i -/+ 0.25 (port/upscale/upscale.c)."""
    p = pad(x, 2, wrap_s, wrap_t)
    t = _axis2x(p, CUBIC_PHASE, 1)        # (h+4, 2w, c)
    return _axis2x(t, CUBIC_PHASE, 0)     # (2h, 2w, c)


def bilinear2x(x, wrap_s=0, wrap_t=0):
    k = (np.array([0, 0.25, 0.75, 0], np.float32), np.array([0, 0.75, 0.25, 0], np.float32))
    p = pad(x, 2, wrap_s, wrap_t)
    return _axis2x(_axis2x(p, k, 1), k, 0)


def lanczos2x(x, wrap_s=0, wrap_t=0):
    def l3(d):
        d = np.abs(d)
        return np.where(d < 3, np.sinc(d) * np.sinc(d / 3), 0)
    k0 = np.array([l3(d) for d in (2.75, 1.75, 0.75, 0.25, 1.25, 2.25)], np.float32)   # u = i - 0.25
    k0 /= k0.sum()
    p = pad(x, 3, wrap_s, wrap_t)
    def axis(xp, ax):
        n = xp.shape[ax] - 6
        def sl(a):
            idx = [slice(None)] * xp.ndim
            idx[ax] = slice(a, a + n)
            return xp[tuple(idx)]
        p0 = sum(k0[k] * sl(k) for k in range(6))            # taps i-3..i+2
        p1 = sum(k0[::-1][k] * sl(k + 1) for k in range(6))  # taps i-2..i+3
        out = np.stack([p0, p1], axis=ax + 1)
        shape = list(p0.shape)
        shape[ax] *= 2
        return out.reshape(shape)
    return axis(axis(p, 1), 0)


def nearest2x(x, wrap_s=0, wrap_t=0):
    return np.repeat(np.repeat(x, 2, axis=0), 2, axis=1)


# ---------------------------------------------------------------- metrics

def psnr(a, b):
    """a, b float in [0,1] (same shape), after rounding to 8 bits."""
    a8 = np.clip(np.floor(a * 255 + 0.5), 0, 255)
    b8 = np.clip(np.floor(b * 255 + 0.5), 0, 255)
    mse = ((a8 - b8) ** 2).mean()
    return 99.0 if mse == 0 else 10 * np.log10(255 ** 2 / mse)


def _gauss(x, sigma=1.5, r=5):
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    x = np.pad(x, r, mode="reflect")
    x = sum(k[i] * x[:, i:i + x.shape[1] - 2 * r] for i in range(2 * r + 1))
    return sum(k[i] * x[i:i + x.shape[0] - 2 * r, :] for i in range(2 * r + 1))


def ssim(a, b):
    """SSIM of the luma of two float RGB images (Gaussian window 11, sigma 1.5, as Wang et al.)."""
    x = np.floor(np.clip(luma(a), 0, 1) * 255 + 0.5)
    y = np.floor(np.clip(luma(b), 0, 1) * 255 + 0.5)
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    mx, my = _gauss(x), _gauss(y)
    sxx = _gauss(x * x) - mx * mx
    syy = _gauss(y * y) - my * my
    sxy = _gauss(x * y) - mx * my
    m = ((2 * mx * my + c1) * (2 * sxy + c2)) / ((mx * mx + my * my + c1) * (sxx + syy + c2))
    return float(m.mean())
