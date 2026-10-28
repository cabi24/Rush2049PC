# Upscaled textures: the 2x upscaler (port/upscale)

Written 2026-10-27. The "upscaled textures" setting (`video.textures = upscaled`, the renderer's side is
[TEXTURES.md](TEXTURES.md)) draws the game's textures at twice (or four times) their size. The renderer hands each
texture, decoded to RGBA8, to one function:

```c
/* port/upscale/upscale.h */
int pc_upscale_rgba2x(const uint8_t *in, int w, int h, int wrap_s, int wrap_t, uint8_t *out);
```

`out` gets 2w x 2h RGBA8; `wrap_s` / `wrap_t` say whether the texture repeats in that direction (the texels past
an edge come from the opposite one) or is clamped (the edge texel repeats). It returns -1 for a texture it won't
take (none, or more than 512 x 512 texels) or when memory runs out. It keeps no state between calls and can be
called from several threads at once.

This file says what it does (1), the network (2), where its training data comes from and under what licence (3),
how N64-looking inputs are made (4), how to train it again (5), how good it is on held-out images (6), how fast it
is (7), how it is tested (8) and what it doesn't do (9).

## 1. What it does

**Colour: a small network.** A convolutional network trained for this (section 2) looks at the texture on its own
grid and predicts, for each texel, a correction to the Catmull-Rom 2x of its four new texels. Catmull-Rom alone
keeps the N64's 5-bit steps, its palettes' banding and its dither patterns, and leaves edges soft; the network was
trained to take those out and to put the sharpness back that a 2x reduction removes.

**Alpha: no network.** Alpha is scaled on its own, by the kind of alpha the texture has:

- all 255 (opaque): 255.
- only 0 and 255 (a cut-out: trees, fences, RGBA5551 and IA4's 1-bit alpha, CI with an RGBA5551 palette): stays
  only 0 and 255. EPX (Scale2x) on the mask: each new texel is its texel's, except that a corner takes the value of
  the two neighbours beside it when they agree with each other and not with the two opposite ones. Single texels
  and one-texel lines stay whole, straight edges stay where they are, diagonal staircases get their corners filled
  or cut. Thresholding an interpolation of the mask at one half doesn't do that: bilinear and Catmull-Rom at the
  new texels' positions come out exactly as nearest neighbour (the nearest texel's weight is over one half for
  both), and thresholding a smoothed mask loses thin lines. On the held-out leaf atlases EPX matches the real
  cut-out at 2x better than nearest neighbour (section 6).
- anything else (IA8/IA16 translucency, glows): Catmull-Rom, held within the range of the four nearest texels,
  so no ringing and no halo; a flat alpha stays exactly flat.
- intensity textures (I4/I8 decode to r = g = b = a): the four channels stay equal, alpha being the network's grey.

**Transparent texels' colour.** The colour of a fully transparent texel is often black or anything, and a scaler
would pull it into the visible texels next to it (a dark fringe around every leaf). Before the colour is scaled,
each fully transparent texel takes the mean colour of its visible neighbours, in rings outward. Afterwards, the
new texels that come out fully transparent take the original colours again (bilinear), since the renderer's
filtering may still blend them in at an edge, as the N64 does; the visible ones keep the network's colour from
the filled image.

**Grey stays grey.** When every texel has r = g = b, the output's three channels are their mean.

**Edges.** Every step (the network's padding before each layer, Catmull-Rom's taps, EPX's neighbours, the fill's
rings) reads past an edge by wrapping or by repeating the edge texel, per axis, as the flags say. So a repeating
texture upscales to a repeating texture with no seam: its 2x is exactly the middle of the 2x of its 3x3 tiling
(tested). A mirrored texture should be passed as the renderer decodes it, with the mirror baked in (period 2x) and
the wrap flag set; passing it as clamped is close too.

**The same bytes everywhere.** Single precision, no fused multiply-adds (`-ffp-contract=off`), every sum in one
fixed order whatever the vector width, texel grouping or thread split: the output doesn't depend on the machine,
the compiler, the build type or the number of threads. ctest `upscale.golden` checks a checksum that aarch64 (the
Pi 5) and x86-64 (Rocky) agree on. It is built with -O2 in every build type, Debug included.

**Threads.** For larger textures the network's rows are shared out among threads, with one wait per layer: half
the processors, at most 4, each taking at least 8 rows and 1024 texels. 32 x 32 and smaller run on the caller's
thread alone. The wait per layer is cheap on an idle machine but dear on a busy one (on the Pi under other builds,
four threads took half again as long as one), so the default leaves the game its cores; a caller with many
textures gets more from calling it on several threads at once.

## 2. The network

All on the texture's own grid (16 channels, 3x3 convolutions, padding by wrap or edge before each one):

```
x - 0.5 ──conv 3->16, PReLU──┬──[conv 16->16, PReLU] x 4──(+)──conv 16->12──pixel shuffle──(+)── output
   │                         └──────────────────────────────┘                   (2x, RGB)     │
   └───────────────────────── Catmull-Rom 2x ─────────────────────────────────────────────────┘
```

11,548 parameters (45 KB as floats; the exported header `upscale_weights.h` is 180 KB of text). About 11,500
multiply-adds per input texel. Its receptive field is 13 x 13 texels, enough for N64 textures, which are mostly
16-64 texels across. The last layer starts at zero, so training starts from plain Catmull-Rom. Inference
(`port/upscale/upscale.c`) is written for this: GCC vector extensions, four output channels a vector (SSE2 on
x86-64, NEON on aarch64), two or four texels at a time sharing each weight load, the sums kept in registers. No
library, no runtime; PyTorch is used for training only.

A 24-channel version (24,228 parameters, twice the work) was trained alongside for a while: at 8,000 steps its
validation PSNR was 31.34 dB against the 16-channel one's 31.09 (Catmull-Rom: 30.25). A quarter of a decibel was
not worth twice the time; it was stopped to give the shared machine back.

## 3. Training data and licences

Only openly licensed images, all CC0 1.0 (public domain dedication), downloaded by `tools/upscale/fetch_data.py`
and listed with their URL and SHA-256 in `tools/upscale/sources.json` (1,192 assets):

| Source | Licence | What | Count |
|---|---|---|---|
| [Poly Haven](https://polyhaven.com) (api.polyhaven.com) | CC0 1.0, <https://polyhaven.com/license> | every texture's 1k diffuse map (JPEG) | 863 |
| [ambientCG](https://ambientcg.com) (ambientcg.com/api/v2) | CC0 1.0, <https://docs.ambientcg.com/license/> | every 7th material's 1K-JPG colour map | 290 |
| ambientCG | CC0 1.0 | the leaf, foliage and graffiti atlases' colour and opacity maps | 39 |

Poly Haven's entries in sources.json also name their authors (CC0 asks for no credit; it's there anyway). The
split is by a hash of the asset's name: one asset in ten is held out for testing (123: 86 Poly Haven, 37
ambientCG), and one in twenty of the rest is held out for validation during training (56). Nothing of the game was
used: no texture, no screenshot, no data from the ROM; the ROM was never on the training machine.

The weights in `port/upscale/upscale_weights.h` are this project's own, trained from scratch on the images above
(no pretrained weights from anywhere). The inference code is written for this port.

## 4. Making N64-looking inputs (tools/upscale/n64tex.py)

A pair is a clean image HR (2N x 2N) and LR = degrade(HR) (N x N, here 32 x 32 for training):

- **HR**: a random square crop (20-100 % of the image) reduced whole to 64-256 texels across, so that a material
  is at the scale an N64 texture has it, 64 x 64 cut out of that, rotated or flipped; 70 % colour-jittered
  (saturation 0.8-1.7, contrast, brightness, sometimes the channels swapped), since N64 art is often more
  saturated than photographs. 12 % are synthetic flat-colour art instead (shapes, strokes, block-letter-like marks,
  stripes, drawn at 4x and reduced: the sharp edges of signs and decals), 10 % such art over a photo crop.
- **Blur**: half the time a Gaussian of sigma 0-0.9 texels.
- **2x reduction**: box (35 %), bilinear (20 %), bicubic (30 %) or Lanczos (15 %), antialiased (PIL).
- **Texel format**, decoded as the renderer decodes it (`port/gfx/gfx_rdp.c`):

| Format | Share | How |
|---|---|---|
| RGBA16 | 24 % | 5 bits a channel, expanded `v << 3 \| v >> 2`; no dither, ordered (Bayer 4x4) or random dither |
| CI4 | 26 % | 6-16 colours by median cut, octree or k-means, palette entries in RGBA5551; mapped plainly, with Floyd-Steinberg or ordered dither |
| CI8 | 14 % | 64-256 colours by median cut or octree, RGBA5551 entries |
| I4 | 12 % | grey 4 bits (`v * 17`), dithered or not; the target is grey too |
| IA4 | 4 % | grey 3 bits (`v * 255 / 7`) |
| I8 | 10 % | grey 8 bits |
| RGBA32 | 10 % | 8 bits a channel |

The target is the clean HR (grey for the intensity formats): the network learns to undo the quantisation and the
dithering as well as to add resolution.

## 5. Training it again

On a Linux machine with Python 3.12 (Rocky's CPU here: an i5-14500T, shared with other builds that night):

```sh
python3 -m venv ~/upscale-venv
~/upscale-venv/bin/pip install torch --index-url https://download.pytorch.org/whl/cpu   # 2.14.1+cpu used
~/upscale-venv/bin/pip install numpy pillow                                              # 2.5.4, 13.0.0
PY="~/upscale-venv/bin/python -I"
$PY tools/upscale/fetch_data.py --out ~/upscale-data/src          # sources.json's files, SHA-256 checked (~1 GB)
$PY tools/upscale/make_pairs.py --data ~/upscale-data/src --out ~/upscale-data/pairs.npz   # 101,300 pairs, 40 s
nice -n 10 $PY tools/upscale/train.py --pairs ~/upscale-data/pairs.npz --out ~/upscale-data/stage1.pt \
    --channels 16 --blocks 4 --iters 40000 --stop 28000 --threads 9          # 28,000 steps, about 45 min
nice -n 10 $PY tools/upscale/train.py --pairs ~/upscale-data/pairs.npz --out ~/upscale-data/c16n4.pt \
    --init ~/upscale-data/stage1.pt --iters 8000 --lr 4e-4 --seed 2 --threads 6   # 8,000 more, about 7 min
$PY tools/upscale/export_c.py --model ~/upscale-data/c16n4.pt     # writes port/upscale/upscale_weights.h
cmake -S port/upscale -B build/upscale && cmake --build build/upscale
$PY tools/upscale/evaluate.py --data ~/upscale-data/src --tool build/upscale/upscale_tool --sheets
build/upscale/test_upscale golden    # prints the new checksum: put it in tests/test_upscale.c's GOLDEN
```

`fetch_data.py --discover` builds the list again from the two libraries' APIs (and rewrites sources.json).
Training: batches of 64 pairs, L1 loss, Adam (2e-3, cosine decay over 40,000 steps), validation PSNR every 2,000
steps, the best kept. The shipped weights came in two stages because Rocky was shared: the 40,000-step run was
stopped at 28,000 to give the machine back (its best checkpoint, 28,000 steps, val 31.26 dB against Catmull-Rom's
30.25), then 8,000 steps from there on 6 threads with a fresh Adam from 4e-4 decaying to 2e-6; `--stop` and
`--init` repeat exactly that. Seeded throughout; PyTorch's CPU kernels may still differ in the last bits between
machines and thread counts.

## 6. Quality on the held-out images

`evaluate.py` on the 117 held-out materials (86 Poly Haven, 31 ambientCG; never trained or validated on), each
reduced whole to 64x64 and 128x128 as the truth and degraded to every format of section 4 (seeded by name), so the
inputs are 32x32 and 64x64; everything wraps, as these materials tile. 1,638 cases. The network is the C code as
shipped (`upscale_tool`); "stand-in" is the classical upscaler the renderer shipped with first (Catmull-Rom held to
the nearest texels' range, then a light unsharp mask). PSNR in dB on 8-bit RGB / SSIM on luma, means:

| Input | Format | bilinear | Catmull-Rom | Lanczos-3 | stand-in | **network** |
|---|---|---|---|---|---|---|
| 32x32 | RGBA16 | 32.68 / 0.761 | 32.70 / 0.785 | 32.63 / 0.791 | 32.70 / 0.789 | **33.41 / 0.810** |
| 32x32 | CI4 | 32.22 / 0.747 | 32.18 / 0.762 | 32.12 / 0.767 | 32.13 / 0.762 | **32.85 / 0.793** |
| 32x32 | CI8 | 32.38 / 0.751 | 32.29 / 0.767 | 32.21 / 0.772 | 32.21 / 0.766 | **33.03 / 0.798** |
| 32x32 | I4 | 30.85 / 0.717 | 30.59 / 0.718 | 30.42 / 0.715 | 30.53 / 0.715 | **31.85 / 0.744** |
| 32x32 | IA4 | 28.00 / 0.641 | 27.28 / 0.612 | 26.99 / 0.601 | 27.14 / 0.602 | **29.45 / 0.681** |
| 32x32 | I8 | 34.55 / 0.768 | 35.04 / 0.798 | 35.18 / 0.808 | 35.17 / 0.806 | **35.63 / 0.834** |
| 32x32 | RGBA32 | 34.22 / 0.767 | 34.72 / 0.797 | 34.87 / 0.808 | 34.85 / 0.805 | **35.21 / 0.833** |
| 32x32 | all | 32.13 / 0.736 | 32.11 / 0.748 | 32.06 / 0.751 | 32.10 / 0.749 | **33.06 / 0.785** |
| 64x64 | RGBA16 | 32.33 / 0.783 | 32.45 / 0.807 | 32.46 / 0.815 | 32.44 / 0.810 | **33.32 / 0.837** |
| 64x64 | CI4 | 31.54 / 0.755 | 31.47 / 0.766 | 31.42 / 0.769 | 31.36 / 0.761 | **32.38 / 0.804** |
| 64x64 | CI8 | 32.14 / 0.780 | 32.15 / 0.799 | 32.10 / 0.804 | 32.08 / 0.798 | **33.14 / 0.835** |
| 64x64 | I4 | 30.78 / 0.747 | 30.55 / 0.747 | 30.38 / 0.744 | 30.47 / 0.742 | **31.86 / 0.778** |
| 64x64 | IA4 | 27.69 / 0.654 | 27.00 / 0.626 | 26.71 / 0.614 | 26.87 / 0.616 | **28.99 / 0.696** |
| 64x64 | I8 | 33.62 / 0.792 | 34.21 / 0.823 | 34.39 / 0.833 | 34.36 / 0.831 | **34.98 / 0.859** |
| 64x64 | RGBA32 | 33.44 / 0.794 | 34.03 / 0.825 | 34.22 / 0.835 | 34.18 / 0.833 | **34.64 / 0.860** |
| 64x64 | all | 31.65 / 0.758 | 31.69 / 0.770 | 31.67 / 0.774 | 31.68 / 0.770 | **32.76 / 0.810** |

Per case, the network's PSNR is above bilinear's in 1,435 of 1,638 (mean +1.02 dB), Catmull-Rom's in 1,382
(+1.01), Lanczos-3's in 1,400 (+1.05), the stand-in's in 1,348 (+1.02), and above the best of all four for that
case in 1,263 (77 %). The gain is largest where the N64's formats lose most (IA4 +1.3 to +1.5 dB, I4 +1.0 to
+1.1, RGBA16, CI4 and CI8 +0.6 to +1.0) and smallest on 8-bit inputs (+0.3 to +0.6), where there is little to
undo (each against the best classical method for that format). On the validation set used during training (1,400
32x32 pairs from other assets) it is 31.28 dB against Catmull-Rom's 30.25. The C classical scalers agree with
evaluate.py's numpy ones to one level.

**Cut-out alpha**, on the leaf, foliage and graffiti atlases (their opacity maps thresholded at one half as the
truth at each size; the input is the half-size threshold), intersection over union with the truth:

| Atlases | Input | nearest (= bilinear or Catmull-Rom, thresholded) | EPX (port/upscale) |
|---|---|---|---|
| 6 held out | 64x64 -> 128x128 | 0.909 | **0.927** |
| 6 held out | 32x32 -> 64x64 | 0.833 | **0.856** |
| all 39 | 64x64 -> 128x128 | 0.817 | **0.833** |
| all 39 | 32x32 -> 64x64 | 0.728 | **0.748** |

EPX was chosen over nearest neighbour and over thresholding Gaussian-smoothed masks (sigma 0.4-0.6 texels, all
below nearest) on the 33 training atlases.

**By eye** (`evaluate.py --sheets`: input, bilinear, Catmull-Rom, network and truth side by side): the network
takes out the 5-bit steps, palette banding and both kinds of dither, keeps edges and lines sharp where bilinear and
Catmull-Rom blur them (corrugated metal, planks, bricks), and adds no halos. It also smooths away fine regular
patterns that the input had already lost to dithering (a diamond plate whose 32x32 version is only dither comes out
flat). On cut-outs, the leaves keep their colour up to the edge with no dark fringe, where a plain bilinear scale of
the same input shows the black under the transparent texels as a dark outline.

**On the game's own textures**, looked at locally on the Pi only (a renderer hook, not committed, that wrote each
decoded texture of a retail race to a scratch file, then contact sheets through upscale_tool; nothing of it kept
or copied): car liveries, logos and number plates come out with clean, sharp outlines and no staircase; concrete,
road and wall textures keep their grain; cut-out foliage stays crisp with its own colour at the edge. Mip levels
of a texture come out alike.

## 7. Speed

`upscale_tool bench 300 [threads]`: 300 synthetic textures in the N64's usual sizes (per twelve: three 32x32, two
64x32, two 16x16, one each of 64x64, 32x16, 128x32, 32x64 and 8x8), 462,400 texels in all (1,541 a texture), half
of them wrapped. About 5.3 billion multiply-adds for the network.

| Machine (busy with other agents' work that night) | network, 1 thread | default threads | 4 threads a texture | Catmull-Rom |
|---|---|---|---|---|
| Rocky, Core i5-14500T (load 4 on 20 threads) | 0.38-0.43 s | 0.32-0.33 s | 0.25 s | 0.03 s |
| Rocky (load 15-30 on 20 threads) | 0.69-1.0 s | | | 0.04-0.06 s |
| Raspberry Pi 5, Cortex-A76 2.4 GHz (load 10-12 on 4 cores) | 1.19-1.38 s | 1.19-1.27 s | 1.9-2.0 s | 0.07-0.10 s |

On one quiet x86-64 core that is about 13 billion multiply-adds a second (SSE2, separate multiplies and adds), so
a few hundred textures take a third of a second on one core of a desktop or laptop CPU, a quarter with four
threads. The default threads help little here because most of these textures are 32x32 or smaller and run on one
thread. The renderer upscales on a worker thread while it draws the original texture, so the time never stalls a
frame. An AVX2 path (eight channels
a vector, the same results) was tried and came out slower on Rocky's mix of cores; it was left out.

## 8. Tests

`ctest -R upscale` (port/upscale/tests/test_upscale.c, synthetic images only, well under a second):

| Test | Checks |
|---|---|
| `upscale.golden` | four small textures (opaque wrapped, a cut-out clamped, translucent grey wrapped in s, intensity) hash to a fixed FNV-1a, with the default, one and three threads |
| `upscale.seams` | a wrapped texture's 2x is exactly the middle of its 3x3 tiling's 2x (both ways, s only, t only; opaque and cut-out); a clamped edge takes nothing from the far edge, a wrapped one does |
| `upscale.alpha` | for each method: binary alpha stays binary, texels inside or outside a cut-out keep their alpha, coverage within 10 % of 4x, visible texels keep the cut-out's colour (no fringe from magenta transparent texels); a translucent ramp stays monotonic and keeps its ends; a flat 128 stays 128; grey stays grey; intensity keeps alpha = intensity; fully transparent stays so |
| `upscale.quality` | a synthetic disc, bar and ramp, box-reduced 2x and RGBA5551-quantised: the network beats bilinear by over 1 dB and Catmull-Rom |
| `upscale.args` | NULL, zero or negative sizes, too large, a bad method; 1x1 and 2x2 work |

`upscale_tool` (built with the tests): `run net|cubic|bilinear IN OUT` for batch files (evaluate.py),
`bench [N [T]]` for timing, `info`.

## 9. Limits

- **It's small.** 11.5k parameters and a 13-texel view: it sharpens edges, takes out quantisation steps, banding
  and dither patterns and keeps textures' grain, but it can't put back detail that 16-64 texels never had. The
  gain over the best classical scaler is around a decibel on the held-out set, more on the palette and 4-bit
  formats, least on 8-bit ones (section 6).
- **Trained on photographs and synthetic flat-colour art,** not on N64 art (none was used, by rule). Hand-painted
  textures and pixel art come out well in the local look at the game's textures (section 6) but weren't measured.
  Dithering that an artist meant as a pattern is smoothed away like the converter's dithering.
- **Cut-outs** are EPX: 45-degree staircases are rounded, shallower slopes keep (half-size) steps. Translucent alpha
  is Catmull-Rom held in range, not learned.
- **Alpha the renderer doesn't use.** When a texture's alpha is ignored (an RGBA16 texture drawn opaque with stray
  zero alpha bits), the "transparent" texels are visible: their new texels get the original colours bilinear, and
  the colour next to them comes from the filled image. A renderer that knows the alpha is unused gets the network
  everywhere by passing alpha 255.
- **4x** is the 2x twice (the renderer does that); the second pass sees the first one's output, which is cleaner
  than anything it was trained on, and mostly sharpens.
- **Memory**: about 350 bytes of scratch per texel while it runs (1.4 MB for 64 x 64, 90 MB at the 512 x 512 limit).
- **The same bytes** need GCC or Clang with `-ffp-contract=off` and no `-ffast-math`, as CMake builds it; other flags
  would still give a good image, but not the golden checksum.
