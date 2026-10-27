# Graphics: F3DEX2 interpreter and OpenGL renderer (`port/gfx/`)

Rush 2049 sends one graphics task per frame. Its microcode is F3DEX2 NoN fifo 2.08 (`gspF3DEX2_fifo`,
`docs/memory_map.md` §1.1a). The PC build replaces the RSP and RDP with a Fast3D-style renderer of its own (the
renderer decision in [PORT_PLAN.md](../PORT_PLAN.md)). The RSP's work (lists, matrices, vertices, lighting,
clipping) runs on the CPU. The RDP's work becomes OpenGL draws: a combiner and blender compiled to GLSL, and textures
decoded from an emulated TMEM. No code was taken from sm64-port/sm64ex or GPL projects. Semantics come from the SDK
documentation and n64brew.

## Files

| File | Role |
|---|---|
| `gfx_api.c` | `pc_gfx_*` (port/include/pc_port.h), settings |
| `gfx_rsp.c` | list walk, addresses, matrices, vertices, lights, fog, texgen, clipping, culling, triangles |
| `gfx_rdp.c` | other modes, colors, images, scissor, TMEM loads, texture decode and cache, rectangles, draw state, batching |
| `gfx_combine.c` | combiner and blender to GLSL |
| `gfx_gl.c` | OpenGL backend: loader, render targets, depth buffers, shader cache, draws, present, readback |
| `gfx_egl.c` | headless context (EGL surfaceless, then EGL device, then the default display) |
| `gfx_sdl.c` | SDL2 window and context (built only when SDL2 is found) |
| `gfx_png.c` | PNG writer on zlib |
| `gfx_gbi.h`, `gfx_internal.h` | the renderer's opcode definitions, shared state |
| `tests/` | `gfx_test` and its own GBI encoders (`test_gbi.h`) |

## Command formats and addresses

- **Native lists** (game C on `TARGET_PC`): `PcGfxWords { uint32_t w0; uintptr_t w1; }`, 16 bytes per command.
- **N64-form lists** (ROM data in a `pc_mem_register_be` range): 8-byte big-endian commands with 32-bit addresses.
- The form is decided per list, by `pc_mem_is_be(list)` at the task start and at every G_DL or BRANCH_Z target. So
  native lists can call N64-form lists and the reverse.
- **Address values** (w1, segment bases, `pc_gfx_present`'s argument) are resolved the same way in both forms:

| Value | Meaning | Host address |
|---|---|---|
| ≥ 4 GB | host pointer (the build is PIE; its data lives high) | as is |
| 0x80000000–0xBFFFFFFF | K0/K1 | `pc_n64_ptr(v)` (identity through the RDRAM window) |
| 0x0S000000 + off, segment S has a base | segmented | `base[S] + off` |
| anything else below 0x80000000 | physical (segment 0, or no base) | `pc_n64_ptr(0x80000000 \| (v & 0xFFFFFF))` |
| 0 | null | none (the command is skipped) |

- The segment table holds host bases (`uintptr_t`). A base set by MOVEWORD G_MW_SEGMENT goes through the same rules,
  so a host pointer, a K0 address or a physical address all work, from either list form. The table resets at every
  task, as the microcode's DMEM does.
- **Data byte order.** Vertices, matrices, the viewport, 16-bit texels and TLUT entries are read big-endian when
  `pc_mem_is_be(ptr)` is true, and native otherwise.
  - Native matrices are read as 32-bit words, as `guMtxF2L` writes them.
  - Native 32-bit texels are `0xRRGGBBAA` words.
  - Lights, 4-bit and 8-bit texels are bytes.
- **16-bit loads from native memory.** A 16b load may carry 4b/8b data (`gDPLoadTextureBlock` loads 8-bit textures as
  16b). So TMEM keeps raw bytes plus a flag per 64-bit word. A 16-bit texel fetch byte-swaps flagged words. TMEM
  itself is in RDP (big-endian) order.

## RSP

- **Matrices.** A 32-deep modelview stack and one projection, with MP = MV × P (row vectors), recomputed on change.
  - G_MTX handles push/load/mul and projection.
  - G_POPMTX pops n (it never goes below the base).
  - G_MW_MATRIX (insert into MP) and G_MV_MATRIX (force MP) are implemented.
- **Viewport.** MOVEMEM G_MV_VIEWPORT: x/y in 10.2, z in 0..0x3FF. Screen y = trans − y/w × scale (the RSP's
  flip).
- **Vertices.**
  - G_VTX takes v0 and n from the F3DEX2 encoding into the 32-entry buffer.
  - G_MODIFYVTX changes RGBA, ST, XYSCREEN (the viewport is inverted, w kept) or ZSCREEN.
  - Texture coordinates are scaled by G_TEXTURE when the vertex loads, as the microcode does.
- **Lighting** (G_LIGHTING).
  - Color = ambient + Σ col × max(0, n·l). The directional lights and the lookat are taken into model space through
    the modelview, then normalized.
  - The ambient light is entry NUMLIGHT, which matches gSPSetLights*.
  - Lights come from MOVEMEM G_MV_LIGHT (offset/24 − 2; offsets 0 and 24 are lookat X and Y) and MOVEWORD
    G_MW_LIGHTCOL.
- **Texgen.** s = (n·lookatX + 1)/4 × scale_s; t the same with lookatY. The linear variant uses acos.
- **Fog** (G_FOG). The shade alpha becomes clamp(z/w × fm + fo, 0, 255) from G_MW_FOG.
- **Clipping.** The NoN microcode has no near plane.
  - Before the divide, polygons are clipped on the CPU against w ≥ 1e-4. This removes the eye plane and everything
    behind it, which the x/y planes would remove on the RSP anyway.
  - Depth is clamped to [0, 1] per vertex. Geometry closer than the near plane is therefore drawn at depth 0, as on
    the N64, instead of vanishing.
  - GL then clips x/y in clip space. Vertices go to GL as clip coordinates with the N64 viewport folded in, so GL
    never near-clips and nothing wraps around.
- **Culling** uses the winding in N64 screen space after clipping (front = counterclockwise on screen). The game's
  mirrored modelview (x = −1) and mirrored viewports therefore flip it, as on the RSP. GL face culling stays off.
- **Display-list control.**
  - G_DL push/branch, with an 18-deep stack.
  - G_ENDDL.
  - G_CULLDL: every vertex outside the same screen edge (or all behind the eye) ends the list like ENDDL.
  - G_BRANCH_Z: screen z (16.16) ≤ w1 branches to RDPHALF_1.
  - G_SPNOOP (0xE0) is a no-op whatever its bytes, which covers the game's unrewritten LOD markers.
  - G_LOAD_UCODE is ignored with a warning.
  - A task stops after 4M commands, which guards against list loops.
- **Flat shading** uses the first vertex (F3DEX2). Triangles are batched per draw state.

## RDP

- **Other modes.** SETOTHERMODE_H/L (F3DEX2 shift/len encoding) and RDPSETOTHERMODE.
- **Colors.** prim (with min level and LOD fraction), env, blend, fog, fill.
- **Images.** SETCIMG, SETZIMG, SETTIMG. SETSCISSOR (its interlace mode is ignored). SETPRIMDEPTH.
- **TMEM.** 4 KB, filled the hardware's way:
  - LOADBLOCK: the dxt line counter swaps the 32-bit halves of the words on odd lines.
  - LOADTILE: rows go at the tile's line stride, with the same swap on odd rows.
  - 32-bit loads: RG goes to the low half and BA to the high half.
  - LOADTLUT: each entry is written four times, 8 bytes apart, as the four TLUT banks.
  - Sampling undoes the odd-line swap, so pre-swapped data loaded with dxt = 0 also works.
- **Textures.** Decoded from TMEM through the tile descriptor (format, size, line, tmem, palette) and tile size.
  - Coordinates use the RDP's clamp/mirror/mask rules: clamp when the clamp bit is set or the mask is 0.
  - A clamping tile is decoded at its size, with mask and mirror baked in. A wrapping tile is decoded as one period
    (two when mirrored) and drawn with GL_REPEAT.
  - Shift (1–10 divide, 11–15 multiply) and the tile origin (uls/ult, fractions kept) are applied per vertex, so the
    game's scrolling tile-size windows need no new decode.
- **Formats.** RGBA16, RGBA32, IA16, IA8, IA4, I8, I4, CI8 and CI4.
  - CI formats go through the TLUT, RGBA16 or IA16. As on the RDP, any 4b/8b texel is an index whenever the TLUT is
    enabled.
  - CI without a TLUT reads as intensity, and so does YUV (the game never uses YUV).
- **Texture cache.** The key is the tile parameters, the TLUT mode and a 64-bit hash of the TMEM bytes the tile
  reads, including the palette. So identical content loaded at different times shares one GL texture.
  - A texture unit reuses its last lookup until a tile, load or TLUT-mode change.
  - Entries unused for 2 frames are evicted once there are 3072.
- **Combiner.** Generated GLSL from the 64-bit combine mode: (A − B) × C + D for color and alpha, clamped per cycle.
  Programs are cached by key.
  - 1-cycle runs the second cycle's settings, as the RDP does. The game sets both cycles alike there; its "X then
    PASS2" modes appear only with fog on, in 2-cycle mode (`src/blob/groups/resurrect/particle_context.c:1377-1428`).
  - 2-cycle feeds COMBINED into cycle 2.
- **Blender.** P × A + M × B, per cycle.
  - The shader writes the part without memory as RGB and memory's total weight as alpha. GL blends with
    (ONE, SRC_ALPHA).
  - The 2-cycle first cycle always blends, and memory's weight is carried through CLR_IN. The game's fog-on
    rectangles use XLU_SURF in both cycles, which is in·a² + mem·(1−a)(1+a), and that comes out exact.
  - Without FORCE_BL the final cycle passes its first input, as on the RDP.
  - Covers OPA/XLU surfaces, FOG_SHADE_A/FOG_PRIM_A, the additive modes and the game's fog + XLU combinations.
- **Alpha.**
  - G_AC_THRESHOLD passes alpha ≥ blend alpha; G_AC_DITHER compares against noise.
  - With CVG_X_ALPHA, coverage = alpha. Zero coverage writes nothing: alpha < 1/8 with FORCE_BL (TEX_EDGE).
    Without FORCE_BL the AA blend is not emulated, so AA_ZB_TEX_EDGE becomes a 0.5 threshold.
  - ALPHA_CVG_SEL alone feeds coverage (1) to the blender.
  - Copy mode with alpha compare drops texels with alpha < 0.5.
- **Depth.**
  - Triangles: z compare/update when the render mode has Z_CMP/Z_UPD and the geometry mode has G_ZBUFFER. The
    test is LEQUAL; Z_UPD alone writes without testing.
  - G_ZS_PRIM uses the primitive depth (triangles and rectangles). Otherwise rectangles get z = 0.
  - ZMODE_DEC: a polygon offset.
  - A FILLRECT whose color image is the z image clears depth (0xFFFC is the far plane).
- **Rectangles.**
  - TEXRECT/TEXRECTFLIP read s,t and dsdx,dtdy from the following RDPHALF_1/2. Copy mode divides dsdx by 4 and
    makes the lower-right edge inclusive.
  - The RDP samples each pixel at its corner and GL at pixel centers, so the start coordinate is shifted to match
    (negative steps included).
  - Above 1x, point-sampled rectangles snap every sample to its N64 pixel's corner in the shader. 2D output and tile
    tricks (CI8 read as CI4 with shift 15) stay exact.
  - FILLRECT in fill/copy mode is a scissored clear with the fill color (the 16-bit pattern's high half). In
    1/2-cycle mode it goes through the combiner, with an exclusive lower-right edge.
  - Rectangles have no shade (0).
- **Cycle types.** Fill, copy, 1-cycle and 2-cycle. Fill-mode triangles use the fill color.

## Backend, frame and settings

- **APIs.** One GLSL body with a version preamble: `#version 300 es`, `330 core` or `140` (desktop GL 3.1, which is
  what V3D offers). Attributes are bound by name, so there are no layout qualifiers. GL entry points are loaded at run
  time, so the same build runs on GLES 3.0+ and desktop GL 3.1+.
- **Render targets.** One FBO per color image address, with a depth renderbuffer per z image and size.
  - A target is cimg width × width·3/4 (320 × 240), times the internal scale.
  - The game's frame buffer is 320 × 220 (survey). Presentation shows rows down to the largest scissor bottom drawn
    with, or `RUSH_GFX_HEIGHT`.
  - Offscreen color images get their own target. Nothing reads one back as a texture yet.
- **Present.** `pc_gfx_present(fb)` copies fb's target (or the last one drawn) into a "presented" image. In a window
  it shows that image at the largest integer multiple that fits, otherwise aspect-fit, letterboxed, with square N64
  pixels. Screenshots read the presented image.
- **Context.** `pc_gfx_init(headless, w, h)`:
  - Headless uses EGL surfaceless (Mesa), which needs no display and works on a tty.
  - A window needs SDL2. When the window can't be created, or the build has no SDL2, it falls back to headless and
    says so.

| Variable | Effect |
|---|---|
| `RUSH_GFX_API=gl` | desktop GL (3.3 core, then 3.1) instead of GLES 3.0 |
| `RUSH_GFX_SCALE=n` | internal resolution multiple (1–8) |
| `RUSH_GFX_HEIGHT=rows` | frame rows to show (default: the largest scissor bottom) |
| `RUSH_GFX_VERBOSE=1/2` | renderer and per-task counts / generated shaders |
| `LIBGL_ALWAYS_SOFTWARE=1` | Mesa llvmpipe |

- **Hooks for later.**
  - `GfxConfig.widescreen` is reserved: it would widen the projection and the target.
  - `pc_gfx_read_framebuffer` and `pc_gfx_read_window` (in `gfx_internal.h`, not `pc_port.h`) return target and
    window pixels, for tests and tools.

### SDL2

- **Detection.** The window backend builds when CMake finds SDL2 (config package or pkg-config). The Pi has the
  SDL2 runtime but no development package.
- **Opt-in fetch.** `-DRUSH_FETCH_SDL2=ON` downloads SDL2 2.30.9 (SHA-256 pinned) and builds it statically, which
  takes about 50 s on the Pi 5 with `-j3`. It was checked here with `SDL_VIDEODRIVER=offscreen`: GLES 3.0 and desktop
  GL contexts come up, and every case passes in window mode. The window shows the frame at exactly 2× (CTest
  `gfx.all.sdl-window`).
- **No live display tested.** No KMSDRM test was possible: the Pi has no gbm headers, and SDL's KMSDRM backend
  needs them. A real display (X11, Wayland or KMSDRM) is untested.

## What the game code must do

- **Wide `Gfx`.** `Gfx` is `{ u32 w0; uintptr_t w1; }` (16 bytes) on `TARGET_PC`.
  - Every pointer stored in w1 must be the full pointer: macros, raw `words.w1 =` writes, and the 32-bit pointer
    store that patches the inline CULLDL branch (particle_context.c:1116).
  - List arithmetic must count in `Gfx` units, never in bytes or `<< 3`.
- **Masked addresses.** A masked address (`osVirtualToPhysical`, `& 0xFFFFFF`, `| 0x80000000`) is fine for memory
  inside the RDRAM window, because it stays an N64 address. Host pointers above 4 GB must never be masked.
- **Tasks and present.** `osSpTaskStart` → `pc_gfx_run_dl(task->t.data_ptr)`. `osViSwapBuffer(fb)` →
  `pc_gfx_present(fb)`, with fb as the game holds it (K0 or host). Color images set through
  `osVirtualToPhysical(fb)` resolve to the same target.
- **Data structures.** Build Mtx with `guMtxF2L` (32-bit words) and write Vtx, Vp and Light by field.
  - A `memcpy` of big-endian ROM fields into a native `Vtx` leaves them big-endian. particle_context.c:1372 copies tc
    and colors from `poly` this way, so it needs a swap if `poly` comes from ROM data.
  - Lights written as raw words need the byte layout `col[3], pad, colc[3], pad, dir[3], pad`.
- **ROM lists.** ROM display lists, vertices, matrices, texels and TLUTs stay big-endian in registered BE ranges
  (ASSETS.md). The game's walkers over them need big-endian accessors: `entity_lod_select` (relocation, LOD markers),
  `display_list_traverse`, the mirror winding flip (`func_8008AD6C`) and the vertex flag pass (`func_800966D8`).

## The game's commands and modes, and their status

From a source survey (2026-10-26). "Code" = built by game C. "ROM" = only in ROM lists, as seen through the game's
walkers. **T** = covered by `gfx_test`.

| Op | Command | Game | Status |
|---|---|---|---|
| 01 | VTX | code (~8), ROM | done, T |
| 02 | MODIFYVTX | not emitted | done (RGBA T; ST/XYSCREEN/ZSCREEN untested) |
| 03 | CULLDL | code (inline culled sub-lists) | done, T |
| 04 | BRANCH_Z (+E1) | ROM | done, T |
| 05 / 06 / 07 | TRI1 / TRI2 / QUAD | code, ROM | done, T |
| D7 | TEXTURE | code (texgen scale w<<7) | done, T |
| D8 | POPMTX | code | done, T |
| D9 | GEOMETRYMODE | code | done, T |
| DA | MTX (proj load/mul, MV load, MV mul push/nopush) | code, ROM | done, T |
| DB | MOVEWORD: SEGMENT, NUMLIGHT, FOG, PERSPNORM | code | done, T (PERSPNORM has no effect) |
| DB | MOVEWORD: LIGHTCOL, MATRIX, CLIP, FORCEMTX | not emitted | LIGHTCOL/MATRIX done, untested |
| DC | MOVEMEM: VIEWPORT, LIGHT (lookat 0/24, lights 48/72/96) | code | done, T |
| DD | LOAD_UCODE | only in a walker's relocation list | ignored |
| DE / DF | DL / ENDDL | code, ROM | done, T |
| E0 | SPNOOP (LOD marker) | ROM | no-op, T |
| E1 / F1 | RDPHALF_1/2 | code (texrect), ROM (BRANCH_Z) | done, T |
| E2 / E3 | SETOTHERMODE_L/H | code | done, T |
| E4 | TEXRECT | code | done, T |
| E5 | TEXRECTFLIP | not emitted (the game flips with negative steps) | done, T |
| E6–E9 | syncs | code | no-ops |
| EA–EC | key, convert | not emitted | ignored |
| ED | SETSCISSOR | code | done, T |
| EE | SETPRIMDEPTH | code | done, T |
| EF | RDPSETOTHERMODE | not emitted | done |
| F0 | LOADTLUT (256, banks of 16, partial ranges) | code | done, T |
| F2–F5 | SETTILESIZE, LOADBLOCK, LOADTILE, SETTILE | code, ROM | done, T |
| F6 / F7 | FILLRECT / SETFILLCOLOR | code | done, T |
| F8–FB | fog, blend, prim, env colors | code | done, T |
| FC | SETCOMBINE | code | done, T |
| FD / FE / FF | SETTIMG / SETZIMG / SETCIMG | code, ROM | done, T |

| Mode | Game | Status |
|---|---|---|
| Combiners | DECALRGBA, MODULATERGBA(_PRIM), MODULATEIDECALA_PRIM, PRIMITIVE, SHADE; with PASS2: T0·SHADE/T0·PRIM, SHADE/PRIM, SHADE/1, T0 with alpha lerp(T0,T1,ENV.a) | all expressible; DECALRGBA, PRIMITIVE, SHADE, a T0/T1 lerp, PASS2 T |
| Render modes | OPA_SURF, XLU_SURF, ZB_XLU_SURF (± Z_UPD), AA_ZB_TEX_EDGE, TEX_EDGE, FOG_SHADE_A + XLU_SURF2 / ZB_XLU_SURF2 / ZB_OPA_SURF2 | done; OPA, XLU, ZB_OPA, AA_ZB_TEX_EDGE, FOG_SHADE_A, 2-cycle XLU T |
| Cycle types | fill (clears), copy (sprites), 1-cycle (2D, no fog), 2-cycle (3D with fog, CI dual tile) | done, T |
| Texture formats | RGBA16, RGBA32, CI8, CI4, I8, I4, IA16, IA8, IA4 | done, T (each through LOADBLOCK and LOADTILE) |
| TLUT | RGBA16 | done, T (IA16 too) |
| CI8 + CI4 dual tile (CI8 read as CI4, shift 15, palette bank) | code | done, T |
| Geometry | ZBUFFER, FOG, LIGHTING, TEXTURE_GEN; SHADE, CULL_*, SHADING_SMOOTH from the static lists | done, T |
| Filter | POINT, BILERP, AVERAGE (menu) | POINT/BILERP; AVERAGE = BILERP |
| Alpha compare | THRESHOLD (blend alpha 16) | done, T |
| Depth source | PIXEL, PRIM | done, T |
| Dither (MAGICSQ, BAYER, NOISE) | code | ignored |
| Texture perspective off (2D list) | code | ignored (rectangles are affine anyway) |
| Frame buffer | RGBA16 320 × 220, three buffers; z buffer cleared through a fill | done (RGBA8 targets) |

## Skipped or approximate

- **Frame-buffer effects.**
  - Color images used as textures, and CPU reads of the frame buffer, are not emulated (the survey found none).
  - The VI's filters (AA, divot, dither, gamma) and its one-line origin offset are not emulated.
  - Targets are RGBA8: no 5551 quantization and no color dither.
- **Coverage.** The AA blend at partial coverage, CVG_DST modes and CLR_ON_CVG are not emulated (texture edge, as
  above).
- **Blender.** Memory alpha (A_MEM) is taken as 1, and there is no (A + B) normalization.
- **Combiner.**
  - K4/K5, key center and key scale are 0.
  - NOISE is a hash.
  - COMBINED in the first cycle is 0.
  - In cycle 2, TEXEL0/TEXEL1 mean tiles t and t+1, without the hardware's swap.
- **Filtering and LOD.** No mipmap or LOD selection: LOD_FRACTION is 0, and TEXEL1 is always tile+1. GL bilinear
  stands in for the 3-point filter.
- **Interpolation.** Shade is interpolated perspective-correctly (the RDP is affine there). Triangles ignore
  TEXTPERSP.
- **Depth.**
  - 24-bit depth stands in for the 18-bit floating z. LEQUAL in every z mode; decal is a polygon offset.
  - Packed z in clears is approximated.
  - Rectangles without G_ZS_PRIM get z = 0.
- **Fill.** Only the high half of the 16-bit fill pattern is used.
- **Microcode.** Point lights and G_LOAD_UCODE are not supported. G_MW_CLIP (the clip ratio) has no effect, because
  GL clips at the screen edges.
- **Not exercised by tests.** Texgen linear, several lights at once, MODIFYVTX ST/XY/Z, G_MW_MATRIX, G_MV_MATRIX,
  ZMODE_DEC, G_AC_DITHER, 32-bit color images, offscreen targets.

## Tests

```
cmake -S port/gfx -B build/gfx && cmake --build build/gfx -j3 && ctest --test-dir build/gfx
# or inside the full PC build:
cmake -S . -B build/pc -G Ninja && cmake --build build/pc --target gfx_test && ctest --test-dir build/pc/port/gfx
# one case, keeping its PNG:
build/gfx/gfx_test texrect --png /tmp/out
```

- **How the test works.** `gfx_test` builds display lists with its own encoders (`tests/test_gbi.h`, written from
  the SDK formats and not from the renderer's decoder). It renders them headless and checks pixels against values
  worked out from the N64 rules. It provides its own `pc_mem_is_be`, `pc_n64_ptr` and `pc_n64_addr`, with an 8 MB
  RDRAM array.
- **Cases.** 17, one CTest each:

| Case | Checks |
|---|---|
| `fill` | fill-mode inclusive edges, 1-cycle exclusive edges, scissor |
| `shade` | smooth and flat shading, MODIFYVTX |
| `zbuf` | z buffer in both draw orders, primitive depth |
| `textures` | every format and TLUT through LOADBLOCK and LOADTILE, RGB and alpha per texel |
| `texwrap` | wrap, mirror, clamp |
| `texrect` | copy mode (inclusive, 4× step), flip, negative step, copy alpha compare, 1-cycle 2× and flip |
| `lit` | directional and ambient light, a light through a rotated modelview, texgen |
| `fog` | 2-cycle FOG_SHADE_A at three depths |
| `twocycle` | a TEXEL0/TEXEL1 lerp, then a lerp toward env by the primitive LOD fraction |
| `dualtile` | CI8 read as CI4 |
| `belist` | N64-form lists through a host segment, a K0 address, a physical address and a segment set from a BE list; native K0 texture |
| `matrix` | push/mul/pop |
| `cull` | front/back, mirrored modelview, CULLDL |
| `branchz` | BRANCH_Z |
| `nearclip` | a floor behind the eye; a triangle inside the near plane |
| `xlu` | XLU, 2-cycle double XLU, alpha compare, texture edge |
| `screenshot` | PNG |

- **Variants.** `gfx.all.desktop-gl`, `gfx.all.scale2` and `gfx.all.software` (llvmpipe) rerun every case on desktop
  GL, at 2× and on llvmpipe. `gfx.all.sdl-window` runs them in window mode (SDL builds).
- **Results on the Pi 5 (V3D 7.1, Mesa 24.2).**
  - 20/20 CTest entries pass (21/21 with SDL).
  - Every case also passes at internal scales 1–4, on GLES, on desktop GL and on llvmpipe.
