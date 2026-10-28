# Textures: upscaled textures and texture packs (`port/gfx/gfx_tex.c`)

The renderer ([GFX.md](GFX.md)) decodes each texture from its emulated TMEM. With `video.textures` it can draw a
bigger texture in its place: one upscaled 2x or 4x by `port/upscale`, or one from a texture pack, a folder of PNG
files. Added 2026-10-27.

| `video.textures` | What is drawn |
|---|---|
| `original` (default) | the N64's textures |
| `upscaled` | the 3D world's textures upscaled: 2x at internal scale 2 and 3, 4x from 4 (fractions the same: native at 2880 × 1620, 6.76, is 4x; at 1366 × 768, 3.2, 2x); at scale 1 the originals (a bigger texture would only be sampled coarser than its texels). The menus, the HUD and text stay original |
| `pack` | each texture with a file in the pack folder (`video.texture_pack`, default `textures/` in the data folder) replaced by it, the 2D ones too; the rest original |

The F1 overlay's Video page has it as **Textures** ([FRONTEND.md](FRONTEND.md)); a change applies from the game's
next frame. `RUSH_GFX_TEXTURES` sets it for a run.

## Files

| File | Role |
|---|---|
| `port/gfx/gfx_tex.c`, `gfx_tex.h` | the setting, identities and names, the upscaler's results, packs, the dump, the worker thread, the disk cache |
| `port/gfx/gfx_rdp.c` (texture decode) | the tile's distinct texels and their identity, the GL image at k sub-texels per texel, the cache's variants, upgrades |
| `port/gfx/gfx_png.c` | the PNG reader (and the RGBA writer the dump uses) |
| `port/upscale/upscale.h` | `pc_upscale_rgba2x`: the 2x upscaler, a small trained network in plain C ([UPSCALE.md](UPSCALE.md)) |
| `port/gfx/tests/tex_test.c` | `gfx.tex.*`: synthetic textures only |
| `tools/texpack/texpack.py`, `README.md` | batch-upscales a dump into a pack with an upscaler the user installs |

## A texture's identity

Computed in the decode (`tex_lookup`), only when a texture is new to the cache (the cache's own key stays the TMEM
hash, so a texture already decoded costs nothing more):

- **The distinct texels**: the tile's S x T texels that its clamp and mask can reach: the tile size where it
  clamps without a mask, one mask period where it wraps, the smaller of the two where it clamps with a mask. A
  mirrored tile's mirror image is not part of it.
- **The hash** (64 bits): the texels' raw values row by row, as the RDP reads them from TMEM (CI: the indices;
  16b the 16-bit word; 32b RGBA), after the format (CI whenever the TLUT applies) and S, T. The TMEM address and
  line, LOADBLOCK or LOADTILE, the odd-line swap and the memory's byte order (ROM data or native) don't change it.
- **CI palettes**: a second hash, of the TLUT type and the entries from the bank's first up to the largest index
  the texels use, so palette entries past it (often other textures' or stale) don't change the identity, and a
  palette animation gives each state its own.
- **The name** (pack files and the dump): `<fmt><bits>_<S>x<T>_<hash>`, for CI `_<palette>` after it:
  `rgba16_32x32_81568407ed53e77c`, `ci4_16x8_9c536938f44ff2ba_826481f4`. A CI replacement may also leave the
  palette out (`ci4_16x8_9c536938f44ff2ba.png`): it then stands for every palette.
- The hash is integer arithmetic on values (no pointers, no struct bytes), so a name is the same on every host;
  `gfx.tex.identity` checks fixed names.

Animated textures: the game's scrolling textures move the tile's origin, which the renderer applies per vertex, so
they keep one texture. Textures loaded anew each frame (frame animations) get one identity per frame, each upscaled
or replaced once. The renderer has no render-to-texture: textures always come from RDRAM, so a texture the game
writes on the CPU each frame would get a new identity each frame. Upscaling waits until a second frame sees a
texture, so such a texture is never upscaled (below).

## Bigger textures, the same N64 texel space

Only the GL texture gets bigger. `TexEntry.w`, `h` stay the N64 texture's size, and everything that maps
coordinates uses them: the tile's shift and origin, the GL texture coordinates (s / w), GL_REPEAT for a wrapping
tile, the LOD's per-level transform (`lod_a`, `lod_b`). A texture k times bigger is the same picture with k x k
sub-texels per texel, so the same coordinates sample the same place, finer.

- **The GL image** (`expand`): the decode builds it from the distinct texels through the RDP's clamp, mask and
  mirror (`map_sub`), per sub-texel: the clamp holds the edge's last sub-texel, the mirror turns the sub-texel
  order too. k = 1 is the decode as before.
- **Filtering**: bilinear's half-texel offset (the RDP centres texels on integers) is in N64 texels before the
  divide by w, so it holds at any k.
- **Mip chains** (G_TL_LOD): the level is chosen in the shader from the change of s, t in N64 texels of the first
  level, times the internal scale; the texture sizes never enter it. Each level is its own texture, upscaled or
  replaced on its own (tiny levels stay original). `gfx.tex.lod` draws a chain from magnified to past the last
  level, original and upscaled (2x, 4x at 6.75, 2x at 2.25), and compares the level shown at each pixel.
- **Rectangles**: above 1x a point-sampled or copy-mode rectangle snaps each sample to its N64 pixel's corner
  (GFX.md). With a bigger texture bound that would throw its detail away, so the snap is off there; at 1:1 a
  pack at the internal scale then shows each of its texels on one pixel (`gfx.tex.roundtrip`).
- **Coarser than the screen**: a texture with more texels than the screen has pixels shimmers (there are no GL
  mip maps). So the upscale factor follows the internal scale, and a pack's replacement past the internal scale is
  box filtered down by a divisor of its factor (a 4x pack at scale 2 is drawn at 2x; a fractional scale counts
  rounded up).

## Upscaled

- **Which textures**: those of draws under a perspective projection (the 3D world: road, buildings, signs, cars,
  sky). 2D (texture rectangles, orthographic triangles: menus, the HUD, text) stays original: it is drawn at the
  N64's pixel grid on purpose, and the snap above keeps it exact. `RUSH_GFX_UPSCALE_2D=1` upscales 2D too (the
  tests use it on rectangles).
- **Left alone**: textures under 4 texels on a side (bars, ramps, dots: nothing to gain), over 256 x 256, and of
  one colour.
- **What is upscaled** is the GL image itself (one period, the mirror image baked in), with the wrap flags of its
  GL_REPEAT, so the upscaler's neighbours at each edge are the ones the texture really has: the opposite edge
  where it repeats, the turned image where it mirrors (`pc_upscale_rgba2x`'s wrap flag is 1 for a mirrored tile:
  its period repeats). `gfx.tex.seams` checks both.
- **Cut-outs** (alpha only 0 and 255: RGBA16's bit, IA4, palette entries): before the upscaler, transparent texels
  take the colour of the opaque ones around them, so colour doesn't bleed in from the transparent side. After it,
  alpha is cut at half along the upscaler's smooth contour: still binary, so an alpha test's edge is as sharp as
  before and in the same place (no halo, no growth), with a smoother outline. Transparent texels get their own
  colour back, except next to the cut-out, where they keep the opaque side's, so bilinear filtering across the
  edge brings no dark fringe in (the original draws one). I formats, whose alpha is their intensity, are upscaled
  as they are. `gfx.tex.alpha` checks the area, the outline and the fringe.
- **When**: a texture is noted the first time it is drawn, queued when a second frame draws it (a texture new in
  every frame is never upscaled, and one-off textures cost nothing), upscaled on one worker thread, and swapped in
  at a later frame. Until then the original is drawn, so a frame never waits. Uploads of finished textures stop
  for the frame past 4 MB (one always goes), so a burst spreads over frames.
- **Kept**: in memory (up to 384 MB, then what the last two seconds didn't use goes), and on disk in
  `texture_cache/<fingerprint>/` in the data folder (`RUSH_GFX_TEXTURE_CACHE`, empty for none), zlib'd. The
  fingerprint is a hash of what the upscaler makes of a fixed pattern and of the edge handling's version, so
  another upscaler's (or version's) files are never read. The second run reads each texture from there, on the
  worker; deleting the folder is safe.
- **Cost**: the exit's log line says how many were upscaled and how long each took on the worker ("N upscaled
  (x ms each on the worker, at most y)"). The upscaler takes about 1 ms for a 32 x 32 texture at 2x on one core of
  a desktop CPU ([UPSCALE.md](UPSCALE.md) 7); 4x is about five times that, and a 256 x 256 texture at 4x the most
  (a few tenths of a second; large textures use up to 4 threads). The game's
  thread only hashes and copies a new texture and later uploads the result, so a race's frames take as long as
  with the originals (RUSH_GFX_TIMES=1's summary, "graphics tasks ... at most"); the second run reads the disk
  cache instead.

## Texture packs

A pack is a folder of PNG files, one per texture, named as the dump names them. Packs are made by each user from
their own ROM on their own machine; the project never ships or hosts one.

- **The dump**: `video.texture_dump = dump` (a folder; a name alone is in the data folder) or
  `RUSH_GFX_TEXTURE_DUMP=DIR` writes every texture the game draws, once, as `DIR/<name>.png` (its distinct
  texels, RGBA). Play the parts of the game you want in the pack: each track, the menus, each car.
- **Replacements**: any whole multiple of the dumped size (1x to 16x), the same multiple both ways; any PNG an
  editor writes (palette, gray, 16-bit, not interlaced). Files in folders inside the pack folder count too, so a
  pack can be sorted by hand. A file of another size is reported once in the log and the original is drawn.
- **Loading**: the folder's file names are read once (when the mode is chosen; choosing `pack` again in the
  overlay reads it again), and each replacement is read on the worker the first time its texture is drawn.
- **2D**: a pack replaces 2D textures too (fonts, the HUD, menus): that is its author's choice.

### Making a pack with an upscaler

1. Dump: set `video.texture_dump = dump` in rush2049pc.ini and play; the files land in `dump/` in the data folder
   (RELEASE.md says where that is). `--set video.texture_dump=DIR` takes DIR from the working directory instead.
2. Install an upscaler yourself, for example realesrgan-ncnn-vulkan (a command-line program, from its own
   project's releases). The helper doesn't download or bundle one.
3. Upscale the dump into the pack folder:

   ```
   python3 tools/texpack/texpack.py DATA/dump DATA/textures --upscaler /path/to/realesrgan-ncnn-vulkan --scale 4
   ```

   It pads each texture by wrapping (so tiling textures get no seams), runs the upscaler, cuts the padding back,
   and makes cut-outs' alpha binary again. It skips files the pack already has, so it can be stopped and run again,
   and textures that look worse can be deleted from the pack (the original is drawn) or edited by hand.
   `--upscaler builtin-nearest` makes a pack by pixel repetition, to try the steps without an upscaler.
4. Play with `video.textures = pack` (or Textures: pack in the F1 overlay).

## Tests

`ctest -R gfx.tex` (headless, synthetic textures only):

| Case | Checks |
|---|---|
| `png` | the reader: all five row filters (gray + alpha), a 2-bit palette with tRNS, a damaged header refused, the writer's RGBA read back |
| `identity` | one RGBA16 texture loaded three ways (LOADBLOCK, LOADTILE out of a wider image at another line, LOADBLOCK from big-endian memory at another TMEM address): one name; CI8 under palettes that differ past the largest index: one name; CI4 in two banks with the same colours: one name, with other colours: the same texel part, another palette part; fixed names; the dump reads back as the texels |
| `roundtrip` | a dumped texture made into a 3x replacement, drawn at scale 3 from a pack, pixel for pixel; at scale 1 box filtered down |
| `seams` | at 2x: a wrapping period's edges flow into each other; a mirrored period's halves mirror each other with no step where it turns or wraps; drawn over two periods, both alike |
| `lod` | a four-level chain, original and upscaled 2x: the level shown at each sample is the same |
| `alpha` | a cut-out at 2x: alpha stays binary, the area within 15 %, no opaque texel away from the original's, no darkened edge, transparent colours kept; drawn with an alpha test over red: no pixel but the cut-out's colour and the background |
| `async` | without RUSH_GFX_TEXTURE_SYNC: the original in the first two frames, the upscaled once the worker is done, one job; switching off and on (from memory at once); a new run from the disk cache, the same texels |
| `texpack` | `tools/texpack/texpack.py --self-test`: a synthetic dump through its built-in nearest upscaler at 3x: sizes, texels, binary alpha, a tiny texture skipped, a second run keeping what is there |

## Limits and what is left

- The upscaler (`port/upscale/upscale.c`, [UPSCALE.md](UPSCALE.md)) replaced the first, classical stand-in
  (Catmull-Rom with anti-ringing and a light sharpen); it does its own fill under transparent texels and keeps
  binary alpha binary, so the bleed and the re-threshold around it here change little. The interface
  (`upscale.h`) stayed.
- No GL mip maps: a pack bigger than the internal scale is filtered down instead.
- Detail and sharpen textures (not emulated in GFX.md) are drawn without LOD, from the tile and the next, upscaled
  as any other.
- `video.textures = pack` doesn't upscale the textures the pack lacks.
