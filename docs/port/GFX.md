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
| `gfx_sdl.c` | SDL2 window and context, display modes (built only when SDL2 is found) |
| `gfx_size.c` | sizes without GL: the scale for a window, internal sizes, N64 edges in internal pixels, where the frame goes, pixels against window units |
| `gfx_png.c` | PNG writer and reader on zlib |
| `gfx_tex.c`, `gfx_tex.h` | upscaled textures and texture packs: identities, the worker thread, the disk cache ([TEXTURES.md](TEXTURES.md)) |
| `gfx_ui.c`, `gfx_font.c` | drawing over the frame in window pixels, and its font: the settings overlay ("The overlay" below) |
| `gfx_interp.c`, `gfx_hfr.h` | a high frame rate: graphics tasks recorded, frames drawn between the game's ("High frame rate" below) |
| `gfx_hud.c` | the race HUD at the screen's edges in widescreen: 2D queued, grouped and moved ("The HUD at the screen's edges" below) |
| `gfx_gbi.h`, `gfx_internal.h` | the renderer's opcode definitions, shared state |
| `tests/` | `gfx_test` and its own GBI encoders (`test_gbi.h`); `gfx_ui_test` (the overlay's drawing); `gfx_interp_test` (frames between); `gfx_size_test` (gfx_size.c with made-up sizes) |

Every external symbol except the `pc_gfx_*` API is `pcgfx`/`pcgfx_*`. The game has its own `gfx_*` functions
(`gfx_frame_begin`, `gfx_lock`), and the renderer links into the same executable.

## Command formats and addresses

- **Native lists** (game C on `TARGET_PC`): `PcGfxWords { uint32_t w0; uintptr_t w1; }`, 16 bytes per command.
- **N64-form lists** (ROM data in a `pc_mem_register_be` range): 8-byte big-endian commands with 32-bit addresses.
- The form is decided per list, by `pc_mem_is_be(list)` at the task start and at every G_DL or BRANCH_Z target. So
  native lists can call N64-form lists and the reverse.
- **Address values** (w1, segment bases, `pc_gfx_present`'s argument) are resolved the same way in both forms:

| Value | Meaning | Host address |
|---|---|---|
| ≥ 4 GB | host pointer (the build is PIE; its data with pointers lives high) | as is |
| 0x80000000–0xBFFFFFFF | K0/K1: the heap, frame buffers, the game's pointer-free data (DATA.md §3) | `pc_n64_ptr(v)` (identity through the RDRAM window) |
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
- **Clipping.** The NoN microcode has no near plane. As on the RSP, a triangle with a vertex outside the guard band
  (|x| ≤ 2w, |y| ≤ 2w; x as the view sees it in widescreen) is clipped to the guard band, on the CPU, before the
  divide (`clip_guard`, gfx_rsp.c). Those planes leave nothing behind the eye, since |x| ≤ 2w needs w ≥ 0. Only a
  polygon through the eye point itself is also clipped at w ≥ 1e-4, and a vertex made there gets w = 1e-4 exactly.
  - So GL only ever gets vertices within twice the view, in front of the eye: x/w and y/w within ±2. Until
    2026-10-27 the polygon was clipped at w ≥ 1e-4 alone, with the new vertex's w lerped in float. From w in the
    thousands that lerp loses the 1e-4: the vertex came out at w = 0 (most of the time without fused multiply-adds,
    as x86-64 builds are), or below 0, with x/w beyond 1e30. The road under the camera reaches behind the eye in
    most frames of a race, so GL got such vertices nearly every frame. A w below 0 turned the polygon around: the
    whole road under the car was culled for single frames, and the sky under it showed (the first player on a
    Windows PC reported it as "lots of graphical errors on the world", flickering road; on the Pi, on Mesa, the
    same). And when the road was drawn, its depth near the camera came out far too deep: the vertex on the eye
    plane had depth 0 but lay millions of pixels out, so GL's depth across the screen was nearly the far vertex's,
    and what lay under or behind the road and was drawn after it showed through.
  - New vertices on the guard band have their own depth, as the RSP's do. Depth is clamped to [0, 1] per vertex.
    Geometry closer than the near plane is therefore drawn at depth 0, as on the N64, instead of vanishing.
  - The winding is summed in double, from the first vertex.
  - GL then clips x/y at the screen in clip space. Vertices go to GL as clip coordinates with the N64 viewport
    folded in, so GL never near-clips and nothing wraps around.
  - GL's own guard band: a polygon with a vertex past |x| = 2w or |y| = 2w in GL's clip coordinates (the target's,
    not the view's) is clipped to it on the CPU as well (`out_clip` in `pcgfx_tri_output`), linearly in clip space as
    GL clips, with each new vertex on the plane exactly (2026-10-28). Where the view spans the target, as a
    perspective view does in widescreen, the view's band is GL's, and its float lerp left vertices a rounding
    beyond it (x = 2.000005 w): V3D clipped those triangles itself, with their colours off ("The same pixels on any
    GPU" below). A viewport larger than the screen is held to GL's band too. Polygons within it are untouched.
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
- **Texture LOD** (G_TL_LOD on a triangle in 1- or 2-cycle mode; added 2026-10-27). The retail cartridge's world
  is drawn so, trilinearly in 2-cycle mode ((TEXEL1 − TEXEL0) × LOD_FRACTION + TEXEL0) from mip chains, G_TEXTURE's
  level field the last level. Before, every level but the first was ignored: the far road and scenery were drawn
  from the largest texture, a texel here and there, and sparkled as they moved.
  - Each level is its tile (G_TEXTURE's tile + k), decoded as any other into its own GL texture, on texture unit
    k. A vertex then carries its s, t in texels, and the shader maps them per level through the tile's shift and
    origin (`GfxDrawState.lod_a`, `lod_b`).
  - The shader picks the levels per pixel as the RDP does: the step is the largest change of s or t (texels of
    the first level) to the next pixel across or down (`dFdx`, `dFdy`), times the internal scale, so every scale
    shows the levels the N64 shows. Below 1 (magnified): the first level, LOD_FRACTION 0. From 2^l: TEXEL0 level l,
    TEXEL1 level l + 1, LOD_FRACTION step / 2^l − 1. From the last level's power on: the last level in both,
    LOD_FRACTION 1.
  - **At any scale, a fraction too.** The scale per axis is the target's internal pixels over its N64 pixels
    (`uScale` = px / n across and down, the sizes `pcgfx_scaled` rounded, as the edges use), so a step across an
    internal pixel times it is the step across an N64 pixel exactly, in N64 texels, whatever the GL texture's size.
    What comes out is continuous in the step (at 2^l, TEXEL1 at fraction 1 is the next TEXEL0 at 0; at 1 and at the
    last level's power the fraction meets 0 and 1), so a float's rounding of a derivative moves a blend by as much
    and never makes a level jump.
  - **Texels are the internal pixel's own.** A triangle is drawn at the internal resolution: each internal pixel
    samples each level at its own centre (only point-sampled rectangles are snapped to their N64 pixel: "Resolution
    and scale"). So where a level's texels are smaller than an N64 pixel's step through them, an N64 pixel's internal
    pixels show more than one of them, at whole scales as at fractions; the level and the blend are the N64's. Found
    2026-10-28: `lod` failed at 1.25 (0,127,128 where 0,0,128 was wanted at (94,32)) with the levels and fraction
    right: the internal pixel it read has its centre 0.8 of the way across N64 pixel 94, and level 1's one-texel
    stripes are 1.5 N64 pixels wide at that step, so it is in the next stripe. The case expected the N64 pixel's own
    texel at one internal pixel, which held at whole scales by luck of where the centres fell; it now checks every
    internal pixel against the N64's levels and fraction and its own texels ("Tests"). The renderer did not change.
  - Not emulated: detail and sharpen textures (those draw as without LOD), the primitive's minimum level, LOD on
    texture rectangles.
- **Formats.** RGBA16, RGBA32, IA16, IA8, IA4, I8, I4, CI8 and CI4.
  - CI formats go through the TLUT, RGBA16 or IA16. As on the RDP, any 4b/8b texel is an index whenever the TLUT is
    enabled.
  - CI without a TLUT reads as intensity, and so does YUV (the game never uses YUV).
- **Texture cache.** The key is the tile parameters, the TLUT mode and a 64-bit hash of the TMEM bytes the tile
  reads, including the palette. So identical content loaded at different times shares one GL texture.
  - A texture unit reuses its last lookup until a tile, load or TLUT-mode change.
  - Entries unused for 2 frames are evicted once there are 3072. Their GL textures, and textures replaced by a
    better one, are deleted 3 frames later (the frame interpolator may draw the last two frames again).
  - The key also has the variant (the original, or the upscaled or replaced texture, which 3D and 2D draws may
    want differently). With `video.textures` the GL texture may be k times the N64 texture; the entry's size,
    and with it every coordinate, the tile maths and the LOD, stays the N64's ([TEXTURES.md](TEXTURES.md)).
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
  - A target is cimg width × width·3/4 (320 × 240) N64 pixels, round(n × scale) internal pixels on each side, at
    any scale from 1 ("Resolution and scale" below). Widescreen widens it (below).
  - Each target and depth buffer keeps the scale it was made at, and is made again at the next color image when
    the scale or the size changes (the auto settings follow the window). Depth buffers of a size no target has
    any more are dropped.
  - The game's frame buffer is 320 × 220 (survey). Presentation shows rows down to the largest scissor bottom drawn
    with, or `RUSH_GFX_HEIGHT`.
  - Offscreen color images get their own target. Nothing reads one back as a texture yet.
- **Present.** `pc_gfx_present(fb)` makes fb's target (or the last one drawn) the "presented" image. In a window
  it shows that image with square N64 pixels, letterboxed: at a native scale pixel for pixel (it was drawn for the
  window), else fitted; at other scales at the largest integer multiple that fits, otherwise aspect-fit
  (`pcgfx_fit`). Headless, it does the same into an offscreen image of the window's size, which `pc_gfx_read_window`
  reads. Screenshots read the presented image. The overlay, when there is one, is drawn after the frame is shown
  and before the swap (below).
  - The presented image is the target's own rows until that target is drawn into, cleared or made again; only
    then are they copied into a texture of its own (`present_copy`). The game draws its next frame into another
    frame buffer, so a frame is copied once, into the window, where it was copied twice (2026-10-28; 17 MB each way
    at 2880 × 1488). A frame shown filtered (not pixel for pixel or a whole multiple) is shown from the copy, as
    before. The copy is flushed before its source is drawn into: on Mesa's iris a fast clear of the copy's source
    in the same batch kept 8 × 4 pixel blocks of the old frame.
  - At each present the renderer waits for the GPU to finish the frame two presents back (`pcgfx_gl_frame_done`):
    at most two frames of GL work in flight. A window's swap holds the game back so in the driver; headless
    (tests, tools, `--fast`) nothing did, and frames piled up in the GPU's queue, single GL calls blocked for
    seconds, and on rocky's i915 several such processes at once got contexts reset for not yielding the GPU
    (7.5 s preemption timeout). It holds the CPU back to the GPU only; what is drawn doesn't change.
- **Draws.** Batches of triangles with one draw state each (gfx_rdp.c). The draw state is set through a record of
  what GL has (`dc_*` in gfx_gl.c; everything else that changes GL state forgets it), so a draw makes only the calls
  that change something: uniforms per program when their values change, texture parameters when the texture's
  differ, enables, masks, scissor. Vertices are streamed into one 4 MB buffer through unsynchronized mappings of
  each draw's range, orphaned when full (2026-10-28; a buffer per draw before, and `glBufferSubData` into a buffer
  the frame's draws read made the Pi's V3D finish its tiles at each draw). The output is the same: every frame of
  the gfx tests, 2,512 images over the variants, byte for byte.
- **Context.** `pc_gfx_init(headless, w, h)`:
  - Headless uses EGL surfaceless (Mesa), which needs no display and works on a tty.
  - A window needs SDL2. When the window can't be created, or the build has no SDL2, it falls back to headless and
    says so.

| Variable | Effect |
|---|---|
| `RUSH_GFX_API=auto`, `gl`, `gles` | which API to try first, the other after it: `gl` desktop GL (3.3 core, then 3.1), `gles` GLES 3.0; `auto` (`video.api`'s default) desktop GL on Windows, whose drivers are made and tested for it (their ES profile through WGL is a side path), GLES elsewhere, where Mesa serves both alike (`pcgfx_want_gles`). `gfx.all.gles`, `.desktop-gl`, `.desktop-gl-software` and `.desktop-gl-wide-scale2` run every case on each |
| `RUSH_GFX_SCALE=s` | internal resolution: `native` (`video.scale`'s default: the window's own pixels, any fraction, 1-16), `auto` (the largest whole multiple of the frame the window holds), a multiple 1–8, or for tests and tools a fixed fraction up to 16 (`2.25`); native and auto are taken at each present, so the frame is drawn at the size it is shown |
| `RUSH_GFX_WIDESCREEN=a` | widen the picture to aspect a: `off`, `auto` (the window's), `16:9`, any `W:H` or ratio up to 4 (below) |
| `RUSH_GFX_HUD=edges\|centered` | the race HUD in widescreen: at the screen's edges (`video.hud`'s default) or in the 4:3 frame ("The HUD at the screen's edges" below) |
| `RUSH_GFX_HEIGHT=rows` | frame rows to show (default: the largest scissor bottom) |
| `RUSH_GFX_VERBOSE=1/2` | renderer and per-task counts / generated shaders |
| `RUSH_GFX_TIMES=1\|2\|3` | at exit, the time of graphics tasks, frames drawn between two and frames shown: 1 the CPU's, 2 with the GPU's (glFinish around each part), 3 the CPU's alone from an idle GPU (glFinish before each part) and the GPU's from timer queries (`GL_TIME_ELAPSED`: desktop GL 3.3, `ARB_timer_query`, `EXT_disjoint_timer_query`; not on the Pi's V3D), with the draws and vertices per task and per frame between ("What it costs" below) |
| `RUSH_GFX_TRACE=N` | every command of the Nth graphics task (w0, w1, and the depth of the list it is in) |
| `RUSH_GFX_TEXTURES=original\|upscaled\|pack` | the textures ([TEXTURES.md](TEXTURES.md)); with `RUSH_GFX_TEXTURE_PACK=DIR` (the pack), `RUSH_GFX_TEXTURE_DUMP=DIR` (every texture drawn, as PNG), `RUSH_GFX_TEXTURE_CACHE=DIR` (the upscaled textures kept on disk; empty: none), `RUSH_GFX_UPSCALE=2\|4` (the factor, instead of the internal scale's), `RUSH_GFX_UPSCALE_2D=1` (2D upscaled too), `RUSH_GFX_TEXTURE_SYNC=1` (upscale and load at once, on the game's thread: tests and screenshots) |
| `RUSH_GFX_DUMP=DIR[:FIRST[:COUNT]]` | the color image of each graphics task from the FIRST (1-based) on, COUNT of them (100), as `DIR/task-NNNNN.png` at the internal scale: every frame the game draws, where a screenshot takes one. `tools/port/frame_spikes.py DIR` lists the frames that differ from both neighbours far more than those from each other (a polygon lost for a frame), which is how the road lost under the camera was found (BRINGUP.md 14) |
| `LIBGL_ALWAYS_SOFTWARE=1` | Mesa llvmpipe |

- **Hooks for later.**
  - `pc_gfx_read_framebuffer` and `pc_gfx_read_window` (in `gfx_internal.h`, not `pc_port.h`) return target and
    window pixels, for tests and tools; headless, the window's are the offscreen image the present fills.
    `pc_gfx_set_widescreen` and `pc_gfx_set_scale` (there too) change those settings at run time, which the
    settings overlay does ([FRONTEND.md](FRONTEND.md)); `pc_gfx_set_window_size` sets the size the auto settings
    follow, and the headless window's size, when there is no window. `pc_gfx_framebuffer_dims` gives a target's
    N64 and internal sizes (tests).

### Resolution and scale

`GfxConfig.internal_scale` (`RUSH_GFX_SCALE`, `video.scale`): native (-1, the PC default), auto (0) or a multiple.
Added 2026-10-27 night: before, every scale was a whole number.

- **Native.** At each present the scale is the next frame's fit in the window (its 4:3 frame widened to the aspect
  in effect, the rows shown): min(window w / frame w, window h / rows), any fraction from 1 to 16
  (`pcgfx_window_scale`). At 2880 × 1620 with widescreen auto, the 426 × 220 frame gets 6.76: a 2880 × 1623 target,
  shown 2880 × 1488 pixel for pixel; at 4:3, 7.36: 2356 × 1620 shown. A render size (`pc_gfx_set_render_size`:
  `video.resolution` in desktop fullscreen) stands in for the window, and the frame is then fitted, filtered.
- **Internal sizes.** A target of n N64 pixels is round(n × s) internal pixels (`pcgfx_scaled`), so the scale per
  axis is px / n (a hair different across and down; the drawing uses each). Targets wider than the GPU's largest
  render target get the largest scale that fits.
- **Edges.** An N64 edge v of a side n pixels long drawn as px internal pixels lands at the first internal pixel
  whose centre is at or past it: ceil(v · px / n − 1/2), in whole numbers (`pcgfx_edge`; `pcgfx_edge_q` for the
  rectangles' quarter pixels). A centre exactly on an edge (2.5, 6.75: an N64 edge at x.5) goes, in GL's window
  coordinates, to the rectangle right of it and above it: rows are counted from the bottom for that
  (`pcgfx_row_edge`, `pcgfx_row_of`), as iris and llvmpipe draw. The scissor and clears (glScissor + glClear) are
  set in those whole pixels, and rectangles are drawn to them (below), so they meet without a gap or an overlap. At
  a whole scale every edge is v × s, as before.
- **The same pixels on any GPU** (2026-10-28). GL promises only that triangles sharing an edge neither overlap nor
  leave a gap, and that its tie rule is the same within one implementation; which side gets a centre on an edge, and
  how finely a vertex is placed (at least 4 subpixel bits; the Pi's V3D has 6, iris 8), differ. So:
  - **Rectangles** (texture rectangles, 1/2-cycle fills) go to GL with their sides on internal pixel boundaries,
    the ones `pcgfx_edge`'s rule gives them, held within the target (`pcgfx_rect_x`, `pcgfx_rect_y`, gfx_size.c),
    and their texture coordinates are taken where the sides are. Every pixel centre is half a pixel from every
    side, so a rectangle covers the pixels whose centres are in it on any GL, whatever its tie rule or rounding, and
    GL never clips one. The HUD at the screen's edges puts moved corners on the boundaries of their new place. A
    side on a boundary already (a whole scale and a whole N64 pixel) keeps its old vertex, bit for bit. Before,
    the sides went where the scale put them: on V3D, N64 x 215 of 426 at 2876 internal pixels (1451.502)
    was rounded onto pixel 1451's centre and the pixel was left out, a column of the clear through `seams` that
    iris filled.
  - **Point-sampled rectangles' texels** come from `gl_FragCoord` in whole numbers (below), the same on any GL.
  - **Scissors and clears** are whole pixels.
  - **Triangles** keep GL's own promise: the game's meshes share their vertices, so they are watertight on any
    GPU. Their vertices come from the float transform, as on any PC renderer: where a 2D triangle's side meets a
    rectangle's at a fractional scale, two GPUs may differ by one internal pixel along it at an exact tie. The 3D
    has no N64 pixel grid to keep.
  - **No GPU clips:** GL gets no triangle vertex past its own guard band, |x|, |y| ≤ 2w ("Clipping" above). Past it
    V3D clips the triangle itself, and its clipped triangles' colours and texture coordinates were off by up to
    1.6 % at 2876 pixels across (the `eyeplane` road, 255 at every vertex, came out 254 and 251 in a corner): not a
    precision qualifier (GLES has `highp` throughout) but V3D's clipper. Rectangles stay within the target.
  - What is left to a GPU: a 2D triangle's side at an exact tie (above), and a guard band of its own smaller than
    twice the viewport (it would clip what GL gets, as GL allows). NVIDIA and AMD haven't run this; nothing in it
    depends on their tie rule or subpixel bits.
  - Checked by `ties` (every quarter-pixel phase, moved 3/8 pixel each way), `gfx.size.rectedge` and `clip`
    ("Tests").
  - **What changed in the pictures:** every frame of `gfx_test all` (88) in 15 variants, at 1, 2, 3 and auto, in
    16:9 and 21:9, at 1.5, 2.25, 2.5, 6.75 and both native sizes, on GLES and desktop GL, on iris and llvmpipe, is
    the same byte for byte before and after but one: in one `eyeplane` frame at 2876 across, 24 pixels along the
    line where the wall stands on the road (equal depths there; the road clipped to GL's band, its depth a rounding
    different) show the wall instead of the road. On V3D the whole-scale variants are the same byte for byte; the
    fractional ones now pass where they failed. So iris's and llvmpipe's tie rule and rounding gave the pictures
    these rules define, and V3D now gives them too.
- **Point-sampled rectangles** (copy mode, and 1/2-cycle with point sampling, above 1×: `GFX_KF_SNAP`). The RDP
  samples each pixel at its top-left corner. The shader finds the fragment's N64 pixel from `gl_FragCoord` in whole
  numbers (`uTarget`: the N64 and internal sizes; the same rule as the edges) and moves the texture coordinate back
  by the centre's distance from that pixel's corner (`dFdx`, `dFdy`), so every internal pixel of an N64 pixel
  shows the N64's texel; `rect_start` puts the RDP's value at the rectangle's corner (plus 1/64 against float
  error). Filtered rectangles interpolate across, per axis (`rect_start`'s half internal pixel).
- **LOD** is measured in N64 pixels: the shader's steps per internal pixel times the scale per axis (`uScale`, px /
  n of the target), so the levels and the blend are the N64's at any scale; the texels are each internal pixel's own
  ("Texture LOD" above). `gfx.lod.*` checks it at the fractions on GLES and desktop GL.
- **Widescreen** works in N64 pixels (ox is whole), so it is the same at any scale. **Frame interpolation** draws
  into an auxiliary target of the frame's N64 size and scale, so the same internal size.
- **What it costs** (2026-10-28, before and after that night's work on the renderer). The synthetic cartridge's
  drive path (`--fast`, 1,669 graphics tasks, the race 900 of them) on rocky: Intel UHD 770 (ADL-S GT1), Mesa 25.2
  iris, GLES, headless, RelWithDebInfo, `--scale native --widescreen auto`, `RUSH_GFX_TIMES=3`: CPU milliseconds
  from an idle GPU, GPU milliseconds from timer queries, means over the run. "Copy" is the frame into the window
  (with a high frame rate, each frame shown); with the frame rate original, the copy into the presented image as
  well. The synthetic scenes are a few boxes over a checkered ground drawn as large overlapping triangles: the
  GPU's time is fill (a race frame at 2880 x 1623 covers its pixels about ten times over, 74 draws), which no
  change to the renderer reduces without changing the picture.

  | Window | Textures | Frame rate | CPU a task | CPU between | GPU a task | GPU between | GPU a copy |
  |---|---|---|---|---|---|---|---|
  | 1920 × 1080 | original | original | 0.33 → 0.26 |  | 3.34 → 3.15 |  | 1.20 → 0.59 |
  | 1920 × 1080 | original | 120 | 0.52 → 0.28 | 0.66 → 0.24 | 3.41 → 2.81 | 4.41 → 3.71 | 0.56 → 0.27 |
  | 1920 × 1080 | upscaled | original | 0.25 → 0.17 |  | 3.60 → 2.92 |  | 1.23 → 0.41 |
  | 1920 × 1080 | upscaled | 120 | 0.30 → 0.22 | 0.38 → 0.19 | 2.76 → 2.75 | 3.64 → 3.58 | 0.34 → 0.27 |
  | 2880 × 1620 | original | original | 0.33 → 0.16 |  | 6.01 → 5.85 |  | 2.05 → 0.83 |
  | 2880 × 1620 | original | 120 | 0.35 → 0.22 | 0.45 → 0.20 | 5.88 → 5.51 | 7.88 → 7.38 | 0.83 → 0.55 |
  | 2880 × 1620 | upscaled | original | 0.38 → 0.15 |  | 6.22 → 5.55 |  | 1.51 → 0.85 |
  | 2880 × 1620 | upscaled | 120 | 0.43 → 0.29 | 0.54 → 0.30 | (11.26) → 5.83 | (15.92) → 7.98 | 1.09 → 0.56 |

  (In brackets: measured with other agents' work on the GPU, which timer queries count; the other runs had the
  GPU to themselves.)

  - **The CPU's part** is small on that machine: the interpreter, the draws' GL calls and, with a high frame rate,
    the recording and the frames between. Draws that set only the state that changes, with the vertices streamed
    into one buffer, took a task's down by a quarter to a half. With the pairing of a frame between working out
    each transform's place once (the synthetic scenes are many instances of a few models, and the
    nearest-neighbour search compared every pair), a frame between's CPU time halved.
  - **The copies** halved: the window is filled straight from the frame's target, without the copy into the
    presented image ("Present" above).
  - **The GPU's part** of a task or a frame between is the scene's fill: 3-18 % less with fewer state changes,
    much of it within the runs' spread. A race frame at 2880 × 1620 takes 7-8 ms of this iGPU, so at 120 frames
    a second it has no room for every frame between (8.3 ms a tick): the host draws those it has time for
    (FRAMERATE.md 2); 60 fits. A discrete GPU has several times the fill rate. `video.resolution` in desktop
    fullscreen (1080p on a 1620p panel) or a whole scale trades sharpness for it.
  - **Left as they were, measured:** texture loads and lookups and the HUD's grouping (each under 1 % of a
    sampled profile of the synthetic race at 120 frames a second); the upscaled textures' uploads (a task's
    largest CPU time with them, 4x at 2880 × 1620, was 7-9 ms against 6-9 without: the 4 MB a frame cap holds).

### Window and display

`gfx_sdl.c`, `pc_port.h`'s window section. Added 2026-10-27 night. Sizes the settings give are pixels.

- **DPI.** `SDL_WINDOWS_DPI_AWARENESS=permonitorv2` before the video subsystem starts, and
  `SDL_WINDOW_ALLOW_HIGHDPI`: on a 200% Windows desktop the window has real pixels, where a DPI-unaware program gets
  a 1440 × 810 window that Windows scales up blurred. Rendering and the overlay use the drawable's size
  (`SDL_GL_GetDrawableSize`) throughout. Where the window system counts in points (macOS, Wayland at 2×), a size in
  pixels becomes points by the drawable against the window (`pcgfx_units_of_pixels`).
- **The first window.** 0 × 0 (`video.width = auto`) is two thirds of the desktop at the picture's aspect (the
  desktop's when widescreen is auto, 4:3 when off), within 90% of the usable area, at least 480 rows
  (`pcgfx_default_window`): 1920 × 1080 on a 2880 × 1620 desktop. Headless, 320 × 240. A saved place opens there
  when a display still shows it, else centred.
- **Fullscreen.** `pc_gfx_set_fullscreen`: desktop (`SDL_WINDOW_FULLSCREEN_DESKTOP`, borderless) or exclusive
  (`SDL_SetWindowDisplayMode` with the closest mode to `video.resolution`, at the desktop's refresh when there is
  one, then `SDL_WINDOW_FULLSCREEN`). Desktop fullscreen at a resolution draws the frame for that size and fits it.
  Switching the mode or the resolution while in fullscreen applies at once.
- **Vsync** is the swap interval. On, frames shown are capped at the display's refresh; `pc_gfx_set_vsync` logs when
  `video.frame_rate` asks for more. Off, `frame_rate` paces alone and frames may tear.
- **The window's place** (`pc_gfx_window_geometry`): kept at every move and resize while it is neither fullscreen
  nor maximized; main writes it at exit (`video.remember_window`).

### Widescreen

`GfxConfig.widescreen` (`RUSH_GFX_WIDESCREEN`, `video.widescreen` in [FRONTEND.md](FRONTEND.md)) is a display
aspect. 0 is off. `auto` follows the window: the aspect is taken again at each present, so a resize applies from
the next frame. A new value applies from the next SETCIMG.

- **The target.** The 4:3 frame (cimg width × width·3/4) gets `ox = round(width × (aspect·3/4 − 1) / 2)` N64
  pixels on each side. ox is a whole number, so 2D stays on the N64 pixel grid and the copy-mode and snapped
  rectangles stay texel-exact. The height doesn't change. The internal scale multiplies the whole target, and
  the present fits it to the window as before: square pixels, a whole multiple when it fits, letterboxed.

  | Aspect | ox | Target (the game's 320-wide frame) |
  |---|---:|---|
  | off, 4:3 | 0 | 320 × 240 |
  | 16:10 | 32 | 384 × 240 |
  | 16:9 | 53 | 426 × 240 |
  | 21:9 | 120 | 560 × 240 |
  | 4 (the most) | 320 | 960 × 240 |

  The game shows 220 rows, so 16:9 presents 426 × 220. A 16:9 screen is the full VI frame (240 rows) at 16:9,
  with the game's own 10-row bands above and below, as on a TV.
- **3D or 2D: the projection's w column.** A perspective projection makes w depend on the position. With row
  vectors, its w column (`proj[0..2][3]`) is not zero: guPerspective's is (0, 0, −1), and the lookat rotation the
  game multiplies into the projection (`func_8009F058`) keeps it nonzero. guOrtho's, and the identity's
  that a task starts with, is zero. So triangles drawn under a perspective projection are 3D, and everything else
  is 2D. After a G_MV_MATRIX (forced MP) the MP's own w column decides.
  - Limits: a 2D overlay drawn with a perspective projection loaded (none in the game's code: its only projection
    loads are in `func_8009F058`) would widen like 3D; at a full-width view that only shows more at the sides,
    since the frame's middle stays the target's middle. An orthographic 3D view (`cam->ortho`, guOrtho) is
    treated as 2D: it stays 4:3, in the middle.
- **Where a side of the frame goes** (`pcgfx_wide_x`): an x at the frame's left side (within 4 pixels, or past
  it) stays; one at the right side (within 4 pixels, or past it) moves 2·ox, to the target's right side; any
  other x moves ox, with the frame. The 4-pixel margin is for the game's split-screen views, which keep a 1-2
  pixel border (`wheel_render_full`, 0x800A6BE4):

  | Players | Views (x, from the code; 320 × 220 frame) | At 16:9 (ox 53) |
  |---|---|---|
  | 1 | 0-320 | 0-426: the whole target |
  | 2 | 2-242 each (top and bottom); a 78-pixel panel to the right | 2-295: the views widen to the left, the panel stays with the frame (295-373), and 373-426 has only what full-width draws put there |
  | 3, 4 | 1-161 and 158-318; scissors 0-159 and 161-320 | 1-214 and 211-424; scissors 0-212 and 214-426 |

  The car-select screen's 2-player views (`car_render_full`) are 0-320 halves, so they widen like 1 player.
- **3D.** A perspective view's sides go through `pcgfx_wide_x`, so a view across the frame spans the target and
  split-screen views widen in proportion. The output position keeps the N64 viewport's scale and moves its
  centre to the widened view's centre: the picture is not stretched, it shows more at the sides (the projection's
  aspect grows by the view's widening, 4/3 at 16:9). Clip x is divided by the same factor for the outcodes, so
  the trivial reject and G_CULLDL use the widened frustum. The scissor of a 3D draw goes through
  `pcgfx_wide_x` too.
- **2D** (orthographic triangles, texture rectangles) moves ox, with the frame, at its 4:3 size. Its scissor
  moves ox as well, so 2D that the N64 clipped at the frame's edge (a sprite sliding in, something parked off
  screen) stays hidden instead of showing in the side bars. In a race, the HUD's 2D goes to the screen's edges
  instead (`video.hud = edges`, the next section).
- **Fills** (FILLRECT in every cycle type, z clears included) go through `pcgfx_wide_x` with their scissor: a
  full-width clear covers the target, a view's own clear covers the widened view, and a divider in the middle
  stays in the middle. A fill that touches only one side of the frame (a HUD bar from x 0) reaches the target's
  side: it is a solid color, so it only grows.
- **A texture rectangle across the whole frame** (both sides within 4 pixels) is a full-screen background or
  overlay: it is stretched across the target (its x step divided by the widening), by 4/3 at 16:9. Others stay
  4:3 (a background drawn as tiles stays 4:3, in the middle, with whatever full-width fills are under it at the
  sides).
- **Off** draws exactly as before: ox is 0, and each widescreen branch tests it first. The 17 cases before it pass
  unchanged, and `gfx.widescreen` draws its scene off, then at 16:9, then off again, and compares the two off
  frames byte for byte.
- **The game's own culling** (a survey of the game code, 2026-10-27). The game never tests objects, track or
  particles against the horizontal field of view, so nothing pops in at the widened sides from the game's side:
  - Frustum culling is G_CULLDL in the lists (the scene walk `track_collision_wall` puts one after each node's
    bounding box), and the renderer runs it against the widened frustum.
  - The world's visibility (`physics_float_calc`, MAT3's WorldCullObjects) picks a ROM bitmask table by the cell
    the eye is in: position only, not direction or field of view. Whether those tables were baked for a 90° view
    can only be seen with the ROM.
  - The rest is distance (LOD, the fog radius), the per-view hide mask (own car, cockpit) and range guards.
  - One function reads the horizontal field of view to decide what is on screen: the battle-mode off-screen arrow
    (`entity_render_setup`, AnimateArrow, `src/blob/groups/animate_arrow/group.c:156,174`). It draws an arrow at
    the side when an opponent's angle is past `0.48 × fovx` (`view->radius`, the view record's 0x0C). In
    widescreen an opponent between the 4:3 edge and the wide edge is on screen and still gets an arrow, drawn at
    the 4:3 edge. Fixing it would mean `fovx' = 2·atan(tan(fovx/2)·k)` for that view, with k the renderer's
    widening of that view (which `pcgfx_wide_x` decides at draw time). It is left: it is an indicator in one mode,
    not a cull, and it can only be checked with the ROM. The record's other horizontal fields (invx at 0x1C, the
    width at 0x24) are read by menu and HUD code that places 2D by 3D points (ZOID_ProjectWindow,
    `car_lights_render` for the coin icons), which stays 4:3, so they must not be widened.
- **What can only be judged with the ROM.**
  - Menus with full-screen art drawn as one rectangle per row band: stretched by 4/3. Art drawn as tiles: 4:3 in
    the middle, with the clear color or the stretched background beside it.
  - 2D that tracks 3D (a name or marker over a car, the coin icons): in a full-width view it stays on its car,
    since the frame's middle is the view's middle. In the 3- and 4-player views the view's centre moves by ox/2
    against the frame (the left views' from 134 to 107.5 at 16:9), so such markers are off by that much.
  - The 2-player screen's right panel and the empty band beside it.
  - Whatever the game draws at the frame's edges to hide something (a border fill, a fade) that touches only
    one side: it now reaches the target's side.

### The HUD at the screen's edges

`video.hud` (`RUSH_GFX_HUD`, `pc_gfx_set_hud`; F1's Video page, [FRONTEND.md](FRONTEND.md)): `edges`, the default,
or `centered`. Added 2026-10-28, `gfx_hud.c`. In widescreen the 2D stays in the 4:3 frame, in the middle (above),
so on a 16:9 screen the race HUD sits ox pixels in from the screen's sides: the speedometer and the radar panel
left, the position, laps, coin counts, map and timer right. With `edges`, in a race, a piece of the HUD at the
frame's left side moves left by ox and one at its right side moves right by ox, so it keeps the distance to the
screen's edge that it has on a 4:3 screen. Everything else stays as the section above has it.

- **When.** In a race with one view: main.c tells the renderer (`pc_gfx_set_hud_race`) from the game's state word
  (the countdown, 0x200000, and the race, 0x400000, its pause screen included) and gNumWindows 1, taken at each
  graphics task. Menus, the attract demo, the results and game-over screens, and split screen are `centered`
  whatever the setting. `centered`, 4:3 (ox 0) and those screens run none of it: the drawing is the same as without
  it (the `hud` test compares the frames byte for byte).
- **Split screen** is left as it is: the 2-player screen's views widen to the left and its side panel stays with
  the frame, the 3- and 4-player views widen in proportion (above). Moving each view's HUD would need the rule per
  viewport, and the 2-player panel's place against the widened views; it is not done.
- **Pieces move whole.** The game draws its race HUD (the speedometer's bezel, digits and odometer, the radar and
  its blips, the place, laps and coin counts, the map and its dots, the timer's bezel and digits) as texture
  rectangles and 1-cycle fills, in one list after the 3D; text is a rectangle per glyph, and the bezels are drawn
  with the primitive depth. While the rule is on, each 2D primitive (a texture rectangle, a 1/2-cycle fill, an
  orthographic triangle) is queued with its draw state and GL vertices (`pcgfx_hud_add`) instead of drawn. The
  queue is drawn (`pcgfx_hud_resolve`) before anything else is drawn or cleared (a 3D triangle, a fill-mode
  rectangle), at a new color or depth image and at the task's end, in its order, so the picture's order is the
  game's:
  - **Groups.** Pieces whose boxes (within their scissor) overlap or nearly touch, 8 pixels apart along a line and
    3 between lines, are one group: a bezel and its digits, a text run with its word spaces, the radar and its
    blips, a pane and its text.
  - **Panels.** A panel is a box at least half the frame wide and an eighth of it high (the pause screen's
    full-frame darkening, a menu's pane). It covers what was drawn before it: a piece drawn on a panel (or the
    panel itself) never groups with a piece drawn before that panel. So the pause screen's option pane over the
    radar's side doesn't take the radar with it, while a value the video page draws on its pane after drawing
    another panel (its test pattern) still groups with its pane.
  - **The move.** A group wider than half the frame stays (a full-width bar, a menu pane). Otherwise its box's
    centre decides: left of the middle band (the frame's middle sixth on each side: 107-213 of 320) the group moves
    left by ox, right of it right by ox, in it it stays (a centred banner: "insert rumble pak", the pause menu).
    ox is whole, so the 2D stays on the N64 pixel grid and texel-exact at any scale.
  - **The scissor** of a moved piece is its 2D scissor (the frame's, at the 4:3 place) moved by the same amount:
    a scissored sub-view (a radar clipped to its window) is clipped where it is drawn. A fill that touches the
    frame's side, which "Widescreen" stretches to the target's side, is moved at its 4:3 size instead.
  - Fill-mode rectangles (clears) are not queued and stay as above; the game's race HUD has none.
- **3D models at HUD points.** The coin icons beside their counts are coin models the game places in 3D at a 2D
  point (ZOID_WindowTo3D, `car_lights_render`), so the rule above can't see them. On the PC that function moves its
  point's x by `pc_gfx_hud_dx` (the same rule for a point: -ox, 0 or ox), so they stay beside their counts; the
  battle and stunt HUD's models go through it too (ovl_b's status scene). It is the one game C change
  (`src/blob/car_lights_render.c`, N64-neutral: one macro). `pc_gfx_hud_dx` runs on the game's thread while the
  renderer runs in real time on the main thread, so it reads only atomics: the setting, and the frame width and
  widening the renderer publishes at each color image (`pcgfx_hud_publish`). After a resize or a widescreen change
  the models follow one frame later than the 2D.
- **A high frame rate** records the queue as it is drawn (`pcgfx_rec_verts`, the moved vertices and scissor), so
  frames between keep the HUD where the nearer frame has it ("High frame rate" below; `gfx.interp.hudedges`).
  Screenshots read the presented image, which has it. 2D keeps its original textures with `video.textures`
  (the draw state is the one 2D always gets). The overlay is drawn over the window, not through this.
- **Checked on the retail cartridge** (2026-10-28, the Pi, 16:9, local screenshots only): a single race's countdown
  and race, the pause screen and its options, controls, audio and video pages, the stunt arena, a frame between at
  60 frames a second, all with the HUD at the edges and the menus, banners and the countdown in the middle; the
  2-player race and the results screen came out byte for byte the same with `edges` and `centered`.

### High frame rate

`GfxConfig.frame_rate` (`RUSH_GFX_FRAME_RATE`, `video.frame_rate` in [FRONTEND.md](FRONTEND.md); why and what it
costs: [FRAMERATE.md](FRAMERATE.md)). 0, the default, is the game's own rate: `pc_gfx_present` shows each frame when the
VI swaps it in, and nothing below runs. Otherwise `pc_gfx_present` only makes the frame the presented image (what
screenshots read) and notes the retrace (`pc_gfx_retrace`); the main loop calls `pc_gfx_present_tick(vt)` that many
times a second, vt in retraces, and it shows the frame for that moment. Added 2026-10-27, `gfx_interp.c`.

- **The swap.** With `PC_GFX_TICK_NO_SWAP` in the mode, `pc_gfx_present_tick` draws into the window and leaves the
  swap to `pc_gfx_swap_tick`, so the main loop times the drawing apart from a swap that waits for vsync, and swaps
  when the vblank it drew for comes (after a retrace due before it: [FRAMERATE.md](FRAMERATE.md) 2). A swap since
  (`pc_gfx_redraw`) makes `pc_gfx_swap_tick` do nothing. `pc_gfx_vsync` says whether the window's swap waits for
  vsync, `pc_gfx_display_hz` the display's refresh (SDL's, whole hertz), `pc_gfx_display_timing` its exact period and
  a recent vblank where the platform gives them (`gfx_display.c`: Windows' display configuration and compositor,
  X11's XRandR; [FRAMERATE.md](FRAMERATE.md) 2; `gfx.display.*` test its arithmetic), and `pc_gfx_tick_waits(vt)` whether the frame for vt is between
  the one shown and the game's next, not drawn yet (`pcgfx_hfr_wants_next`): the loop then waits for that graphics
  task while the tick still makes its vblank. `pc_gfx_frame_waiting` says the game has swapped in a frame no tick has
  shown: the loop shows it at the next retrace.

- **Recording.** While it is on, `pc_gfx_run_dl` records each task as the interpreter runs it; what the interpreter
  draws doesn't change. The hooks (`pcgfx_rec_*`):
  - a vertex load: each vertex in model space (read again from the Vtx), its clip position, shade, s, t and fog flag
    as loaded, and the transform it was loaded under. A transform is made whenever the modelview, the projection or
    MP changed (`pcgfx.xf_gen`), or the viewport or fog: its modelview, projection and MP, whether it is perspective,
    whether MP is modelview x projection (not after G_MV_MATRIX or G_MW_MATRIX), the view (projection loads so far),
    its parent (the transform one level up with the modelview this one was pushed from), the bounds of its vertices
    and a signature: the view and each load's source address and size;
  - a triangle: its three vertices, the draw state and texture units it was drawn with, and its `GfxTriCtx` (the
    viewport, culling, smooth shading, depth source, where the viewport lands in widescreen). Triangles the frame
    rejects (outside, behind, back faces) are recorded too when the draw state in effect is still theirs
    (`pcgfx_draw_state_peek`): another moment may show them;
  - rectangles as their GL vertices; clears.
- **Which frames.** Four recordings are kept. When frame B is on screen (swapped in at retrace b) and the next one, C,
  is drawn (the scheduler swaps C in a period later, so on the PC C is drawn well before it is due), the moment
  `a = (vt - b) / period` (period: B's retrace minus the one before, 1-4, else 2) shows:
  - a <= 0: B (the presented image); a >= 1 and C not swapped in yet (the game is late): C as drawn;
  - between: a frame drawn again from the nearer recording, B's up to halfway, then C's, toward the other's. What is
    in view at that moment is mostly what that frame drew (the road under a camera moving forward is in B's list, not
    in C's), and what appears or goes, 2D and teleports change there, not earlier;
  - B again when there is no C yet, when either frame has no 3D (menus), draws into two color images, differs in
    size, or when every view is a cut.
- **Pairing** (`match_pair`, once per pair and direction). Transforms with the same signature, one in each frame,
  pair. The game's per-frame buffers alternate between two frame records, so its matrix and list addresses pair
  nothing; its models' vertex data stays put, and each scene node loads its own culling box (`D_80157248[k]`), so
  most transforms are unique. Several with one signature (a car's wheels, smoke puffs): those under a paired parent
  pair in their order under it; the rest as mutual nearest neighbours by where they are against the camera, the
  other frame's moved by the camera's move.
- **The camera's move** per view is the median change of the paired modelviews' translations among those that didn't
  turn: the game puts the camera's turn in the projection (`func_8009F058` multiplies its lookat into it) and its
  place in every node's modelview, so the world's pieces all change by the camera's move.
- **Cuts and jumps.** A view is cut (drawn as the frame has it; with every view cut, B is held) when its projection
  turned more than 45 degrees or zoomed 1.5x, its camera moved more than 150 units a retrace or 0.35 of the median
  distance to what it shows, more than 30% of its paired transforms jumped, or nothing in it paired. A transform
  jumped (a teleport: unpaired) when it moved, against the camera, more than 100 units a retrace and 0.6 of its
  distance; one that turned more than 50 degrees or scaled 1.67x (a wheel at speed) spun: its place is blended, its
  turn is its own frame's.
- **Drawing a moment** (`draw_between`, into an auxiliary target of the frame's size and scale with its own depth
  buffer, `pcgfx_gl_aux_target`):
  - each view's projection is blended elementwise; each paired transform's modelview too, and MP = MV x P with
    gfx_rsp.c's operations, so a transform that didn't move gives its MP to the bit;
  - an unpaired 3D transform (only that frame has it, or it jumped) keeps its modelview, moved by the camera's move
    to the moment: it stays where the frame has it in the world. So do native (game-built) vertices under a paired
    transform that didn't move while the camera did (eye-relative skid marks under the identity modelview);
  - 2D, transforms in a cut view, and vertices placed on screen (G_MODIFYVTX XY/Z) keep their recorded clip position;
  - fog is recomputed from the new position; lighting, texture generation and colours are the frame's;
  - triangles go through the interpreter's own `pcgfx_tri_visible` and `pcgfx_tri_output`, clipped as the game's
    frames are ("Clipping" above: this replay found the eye-plane error, which the live path then had too, until
    2026-10-27);
  - batches are drawn with the recorded draw state into the auxiliary target; it is shown like the presented image
    (`pcgfx_gl_show_target`), with the frame's rows.
- **The window and screenshots.** `pc_gfx_redraw` shows the last thing shown (a frame between, while the overlay holds
  the game). Screenshots and F12 read the presented image: always a frame of the game. A tick that would show what is
  on screen already (a held frame, the same window size, no overlay) shows nothing and doesn't swap.
- **Load** (`pc_gfx_present_tick`'s mode, from the main loop; [FRAMERATE.md](FRAMERATE.md) 2): 0 with `--fast`,
  everything drawn as the moment says; 1 on the host's clock, a frame between is drawn only once the GPU has passed
  the fence set after the last graphics task or frame between (`pcgfx_gl_fence_set`/`_busy`, `glClientWaitSync`, at
  most 2 ms; without fences, always); 2 while the host is behind, none is drawn. A frame not drawn shows nothing new.
- **Textures** in a recording are GL names from the texture cache; an entry is evicted only after 2 presents unused,
  and a recording is drawn again only until the frame after it is swapped in.
- **Tools.** `RUSH_GFX_VERBOSE=1` prints a line per pair and direction (transforms blended, moved with the camera,
  eye-fixed, as drawn; views cut), 2 each view's camera move, 3 every transform and each recorded frame;
  `RUSH_GFX_HFR_DUMP=DIR[:FIRST[:COUNT]]` writes each frame shown as `hfr-TICK-AAA.png` (AAA the moment in
  hundredths, `real` for a frame of the game). At exit, with the setting on: the frames shown, drawn between, early
  and held, and the CPU time of graphics tasks and frames drawn between.

| Case (`gfx.interp.*`) | Checks |
|---|---|
| `exact` | a pair of identical frames (two views, a textured back-face-culled quad, a texture rectangle, an orthographic triangle), with and without a fogged ground through the eye plane: the frame between is the game's frame to the pixel |
| `move` | an object moving across a still camera, at 0.25, 0.5 and 0.75 against frames drawn at those moments; its centroid halfway |
| `camera` | the camera turning in the projection and moving in the modelviews; within 2 pixels of the true turn |
| `cut` | a quarter turn: no frame between, B until C is due, then C as drawn |
| `teleport` | a jump among moving and still objects, with a still and a moving camera: where B has it, then where C has it |
| `split` | 2 and 4 views, each with its own camera and objects moving their own ways |
| `hud` | a texture rectangle and an orthographic triangle that move: B's, then C's, texel for texel; the 3D with them blended |
| `appear` | an object only C has appears past halfway where C has it, through the moving camera; one only B has stays until halfway |
| `instances` | four instances of one model, the first gone in C: each pairs with its own |
| `children` | a parent the camera follows, with four instances of a model under it as far apart as the camera moves: paired in their order |
| `spin` | an object turning 60 degrees a frame: its place blended, its turn the nearer frame's |
| `eyerel` | eye-relative native vertices under the identity modelview, in the frame's buffer and at one address: they stay put in the world |
| `timing` | held, drawn, drawn, early (C late), then C; a period of 3; the presented image (screenshots) unchanged by a frame between; a redraw shows the frame between; off, a tick shows nothing and the present shows the frame |
| `parse` | the setting's text; `display` is 60 without a display |
| `hudedges` | the race HUD at the screen's edges in 16:9: a frame between draws the HUD's rectangle where the nearer frame has it, as the frames drawn directly do with `edges` and with `centered`, and further left with `edges` |
| `depthclear` | B far, C near, D far again, with no tick between C's present and D's task: D's depth clear (through the last target, the z buffer as the color image) reaches the depth buffer, so D's object shows. Until 2026-10-28 the present's copy left another framebuffer bound for drawing while the renderer took C's as still bound, and from the second present on that clear went nowhere: with a high frame rate a frame could keep the last one's depth when no frame was shown between |
| `gfx.interp.all.*` | every case at 2x, in 16:9, on GLES, on desktop GL, and on llvmpipe's GLES and desktop GL |

### The overlay

port/input's settings overlay ([FRONTEND.md](FRONTEND.md) "The settings overlay") is drawn here, outside the
display-list interpreter (`gfx_ui.c`, `pc_port.h`'s graphics section). Added 2026-10-27. Its mouse input is
port/input's: the drawable's pixels against the window's units, the same sizes as here ([FRONTEND.md](FRONTEND.md)
"The mouse"). `pc_gfx_driver` gives its About page the GL strings the `gfx:` log line prints.

- **When.** While a UI hook is set (`pc_gfx_set_ui_hook`), each present calls it after the frame is shown in the
  window, and before the swap, with the window's size in pixels. With no frame yet it draws over black. The overlay
  is set only while it is open, so with it closed nothing here runs and the output is what it was.
- **What it draws.** `pc_gfx_ui_rect` and `pc_gfx_ui_text`, in window pixels from the top left, colors as
  `0xRRGGBBAA`. They are batched as triangles and drawn in one call by `pcgfx_gl_ui_draw`:
  - a program of their own: the renderer's vertex shader (clip coordinates computed on the CPU) and a fragment
    shader that multiplies the color by one texture (`pcgfx_ui_fragment_source`);
  - the texture is an atlas of the font's 95 glyphs, 6 texels apart, and a solid cell that the rectangles sample.
    It is uploaded once, nearest-sampled;
  - alpha blending (`SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`) on the color channels, with the window's alpha kept; no
    scissor, depth or culling. The renderer sets all of its state per draw, so nothing has to be put back.
  - At a whole scale, each glyph is one quad whose window pixels land in the middle of their texels, so the text is
    exact (the font test checks it pixel for pixel).
- **Not through the frame.** Nothing goes through a render target or the presented image: the overlay is the same
  at every internal scale and in widescreen, and a screenshot (which reads the presented image) has the frame
  without it.
- **`pc_gfx_redraw`** shows the presented image again with the overlay and swaps. The overlay redraws with it after
  each change while the game is held, when no frames come; the pause key's redraw of an exposed window uses it too.
- **The font** (`gfx_font.c`) was drawn for this port: printable ASCII in a 5 x 9 cell, capitals and digits 7 rows
  tall, lower case 5 from row 2, descenders 2 rows (`, ; _ g j p q y`). The table is the glyphs as they look, eight
  to a band of nine strings, `#` for a lit pixel; `pc_gfx_font_glyph` reads a glyph's rows out of it. No font data
  was copied from anywhere.
- **The event pump's tick.** `pc_gfx_poll_events` calls the tick hook (`pc_gfx_set_tick_hook`) after the window's
  events, with or without a window: port/input's turn at each retrace and while the main loop holds the game.

### SDL2

- **DPI awareness** and the display modes: "Window and display" above.
- **Detection.** The window backend builds when CMake finds SDL2 (config package or pkg-config). The Pi has the
  SDL2 runtime but no development package. In the top-level build, SDL2 is found (or fetched) once and shared
  with port/audio and port/input ([FRONTEND.md](FRONTEND.md)). `cmake -S port/gfx` on its own still looks for
  its own.
- **Events.** `pc_gfx_poll_events` passes each SDL event to port/input's hook (`pc_gfx_set_event_hook`) before
  its own handling, so the keys, controllers, fullscreen and focus need no second event loop.
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
| Texture LOD | G_TL_LOD, 2-cycle trilinear over mip chains (the world, from ROM lists) | done, T |
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
- **Filtering and LOD.** GL bilinear stands in for the 3-point filter. Without G_TL_LOD, LOD_FRACTION is 0 and
  TEXEL1 is tile+1; with it, see "Texture LOD" (no detail or sharpen textures).
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

- **Textures.** `gfx_tex_test` (`gfx.tex.*`): the PNG reader, texture identities, a pack's round trip, wrap and
  mirror seams at 2x, LOD levels at 2x, 6.75 and 2.25, cut-out edges, the worker and the disk cache
  ([TEXTURES.md](TEXTURES.md) "Tests").
- **How the test works.** `gfx_test` builds display lists with its own encoders (`tests/test_gbi.h`, written from
  the SDK formats and not from the renderer's decoder). It renders them headless and checks pixels against values
  worked out from the N64 rules. It provides its own `pc_mem_is_be`, `pc_n64_ptr` and `pc_n64_addr`, with an 8 MB
  RDRAM array.
- **Cases.** 25, one CTest each:

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
| `lod` | G_TL_LOD with four mip levels (level 1 striped a texel wide, to check its coordinates), point sampled, quads with their corners on whole N64 pixels (a projection exact in fixed point) at steps of 0.5-16 texels an N64 pixel: the first level magnified, level 1 and 2 exactly, halfway between levels, the last level from its power on, 3 across and 0.5 down and the other way round (each axis at its own scale); without G_TL_LOD, tile 0. Every internal pixel inside a quad against the N64's levels and fraction for its step and the texels at its own centre (those within 1/16 texel of a stripe's side left out) |
| `dualtile` | CI8 read as CI4 |
| `belist` | N64-form lists through a host segment, a K0 address, a physical address and a segment set from a BE list; native K0 texture |
| `matrix` | push/mul/pop |
| `cull` | front/back, mirrored modelview, CULLDL |
| `branchz` | BRANCH_Z |
| `nearclip` | a floor behind the eye; a triangle inside the near plane |
| `eyeplane` | a road under the camera and a wall beside it, from behind the eye to far ahead, back faces culled, in 12 placements (five of them gave the old float lerp a w below 0 with and without fused multiply-adds): the road reaches the screen's bottom and the horizon, the wall the right edge, and a floor 20 units under the road, drawn after it, stays hidden (the old clipping showed it through the whole near road in every placement); then a chase camera over a road of two big triangles, 30-120 units up, pitched 4-16 degrees down, at 8 headings: the road fills the bottom rows (the old clipping lost it in 12 of the 24) |
| `clip` | on the CPU (`tests/clip_check.c`): 40,000 triangles (anything, roads through the eye plane, tiny w next to the eye, inside; also widened 4/3): every vertex handed to GL has w ≥ 1e-4 and lies on or within the guard band, finite; one inside it is untouched; one behind the eye or past one side is dropped; and every GL vertex `pcgfx_tri_output` gives is within GL's own band, \|x\|, \|y\| ≤ 2w exactly (over 1,000 of the widened view's polygons clipped to it) |
| `xlu` | XLU, 2-cycle double XLU, alpha compare, texture edge |
| `widescreen` | the aspect parser; at 16:9: the clear covers the target, a perspective view across the frame shows strips past the 4:3 frustum's sides and reaches the target's last column, a square keeps its 4:3 size, texrects (one at the frame's left side) and an ortho triangle stay 4:3 in the middle, texel for texel, a full-width fill and a full-width texrect (stretched) span the target; four quadrant views widen to 0-213 and 213-426, with their squares unstretched and a texrect cut by its 4:3 scissor; the game's 2- and 4-player panes; off, on, off again: the off frames are byte-identical |
| `autosize` | headless window sizes from 640 × 480 to 2880 × 1620: `auto` widescreen takes the size's aspect, `auto` scale the largest multiple that fits (3 at 1280 × 720 for 426 × 240), native the fit at any fraction (2876 × 1620 at 2880 × 1620, 1363 × 768 at 1366 × 768), a fixed scale or aspect stays; a texrect stays texel-exact at each (not in window mode) |
| `seams` | at the scale in effect, every internal pixel of the 4:3 frame against its N64 pixel's color: copy-mode tiles side by side, 13-pixel 1-cycle tiles from x 3, glyph-sized rectangles a pixel apart, 2× magnified tiles, 1-pixel fill columns and 1-cycle rows, a fill to the bottom: no gap, no bleed |
| `ties` | rectangles at every quarter-pixel phase of both sides, side by side and row on row, drawn as they are and with every GL vertex moved 3/8 of an internal pixel left, right, down and up (`pcgfx_gl_test_nudge`: more than a GPU rounds a vertex, 1/32 pixel at most, less than the half pixel between a side on its boundary and every centre): each time every internal pixel has the rectangle its centre is in, counted in whole numbers (rows in GL's y-up coordinates). With the sides where the scale put them (before 2026-10-28) the moved frames fail at every scale, 1 too |
| `screenshot` | PNG |
| `hud` | the race HUD at the screen's edges, on a made-up race frame (GFX.md "The HUD at the screen's edges"), at 16:9 and 21:9: a bezel fill with a copy-mode digit on it and an orthographic triangle at the left side end up ox further left, texel for texel; a run of digits a pixel apart at the right ox further right, together; a fill cut by its scissor at the right moves with its scissor; a banner with a digit at each end and a run of digits across the middle band's side stay; a full-width fill spans the target; a panel drawn over a radar fill's side stays, the radar moves; a digit drawn on that panel after a second panel stays with it; the 3D strip spans the target; `pc_gfx_hud_dx` for points; `centered`, `edges` outside a race and `edges` at 4:3 give the same frames, byte for byte, as `centered` |

- **Variants.** `gfx.all.gles`, `gfx.all.desktop-gl`, `gfx.all.scale2` and `gfx.all.software` (llvmpipe) rerun
  every case on GLES, on desktop GL, at 2× and on llvmpipe's GLES; `gfx.all.desktop-gl-software` on llvmpipe's
  desktop GL, `gfx.all.desktop-gl-wide-scale2` on desktop GL at 2× in 16:9 (the plain cases take the default API:
  desktop GL on Windows, GLES elsewhere). `gfx.all.sdl-window` runs them in window mode (SDL builds). `gfx.all.wide` (16:9) and
  `gfx.all.wide21-scale3` (21:9 at 3×) rerun every case in widescreen, with every sample taken ox pixels in: the
  4:3 frame's pixels must come out the same in the middle of the wider target. `gfx.all.scale-auto` reruns them
  with the auto scale (2 in the 640 × 480 headless size, 1 while a case widens to 16:9). Fractional scales:
  `gfx.all.scale1.25`, `.scale1.5`, `.scale2.25-desktop-gl`, `.scale2.5-software` (ties: N64 edges on internal
  pixel centres), `.wide-scale6.75` (16:9, 2876 × 1620), `.native-2880x1620` and `.native-1366x768`
  (`RUSH_GFX_TEST_WINDOW` sets the headless size): every case's N64 pixels, texel for texel (`lod` every internal
  pixel's), and `seams` and `ties` every internal pixel. (Until 2026-10-28 1.25 ran `seams` and `ties` alone, as
  `lod` failed there: "Texture LOD".)
  `gfx.lod.<scale>.gles` and `.desktop-gl` run `lod` on each API at 1.25, 1.5, 2.25, 2.5, 3.2, 6.75 (`scale6.75`,
  and `wide-scale6.75` in 16:9) and native in 2880 × 1620 (16:9 auto) and 1366 × 768 windows.
  `gfx.interp.all.scale2.5` and `.wide-scale3.4` run the frame interpolation's cases at fractions. Each variant has
  its own directory for the screenshot case's PNG, since they run in parallel.
- **Sizes alone** (`gfx_size_test`, no GL): `gfx.size.scale` (native and auto scales for 2880 × 1620, 1440 × 810,
  1920 × 1080, 3840 × 2160 and the limits; internal sizes), `.edge` (at 1.5-9 and the 3K sizes: every N64 pixel at
  least one internal pixel, edges at 0 and the end, each internal pixel inside its N64 pixel's edges, rows the same
  with GL's ties; whole scales v × s), `.rectedge` (rectangles' sides at every quarter pixel at 1-8, the fractions
  and the native sizes: the first internal pixel whose centre is at or past the side, counted, rows by GL's ties,
  the scissor's edges at whole pixels, the GL coordinate within 1/1000 pixel of its boundary as a GPU computes it in
  floats, past the target held to it, and at whole scales the old vertex bit for bit), `.fit` (native pixel for pixel and fitted, whole multiples, small windows),
  `.units` (pixels and window units with per-monitor DPI awareness, a 2× points output, 175%), `.window` (the first
  window's size on 3K, 1080p, 4K, 4:3, small and no displays), `.texfactor` (the upscaled textures' factor, the
  largest power of two up to the scale, and a pack's limit, the scale rounded up, at whole and fractional scales).
- **The overlay** has its own program, `gfx_ui_test`, headless: it reads the window's offscreen image.

| Case | Checks |
|---|---|
| `gfx.ui.font` | every printable glyph present, within 5 columns, distinct, descenders only on `, ; _ g j p q y`; four glyphs against bitmaps written out in the test; a string drawn at scales 1, 2 and 3 matches them pixel for pixel, gaps included, in three colors, and its width |
| `gfx.ui.panel` | a rectangle at alpha 128 blends over the frame by its alpha, to its last pixel and not past it; an opaque one covers it; the 640 x 480 headless window |
| `gfx.ui.closed` | the overlay shows when presented and when redrawn; a screenshot with it is byte for byte the one without; without the hook the window, after a redraw and after presents, and the render target are byte for byte what they were before |
| `gfx.ui.scaled` | the overlay's pixels at internal scale 1 and at 3 in 16:9 (a 1278 x 720 target) are the same; the hook gets the window's size, which follows `pc_gfx_set_window_size` (1280 x 960) |
| `gfx.ui.all.gles`, `.desktop-gl`, `.software`, `.desktop-gl-software` | every case on GLES, on desktop GL, and on llvmpipe's GLES and desktop GL |

- **Results on rocky (x86-64, Mesa, 2026-10-27):** the 19 cases and their variants pass as before, with the headless
  window image now filled at each present; the 6 `gfx.ui` tests pass.
- **Results on the Pi 5 (V3D 7.1, Mesa 24.2).**
  - 25/25 CTest entries pass (26/26 with SDL).
  - Every case also passes at internal scales 1–4, on GLES, on desktop GL and on llvmpipe, and in widescreen
    (16:9 on GLES and desktop GL, 16:10 on llvmpipe, 21:9 at 3× and 4×).
  - On x86-64 (Intel UHD 770, Mesa 25.2, with SDL2) all 26 pass as well.
- **Texture LOD at fractions (2026-10-28).** `lod` passes, every internal pixel, at 1-4, 1.25, 1.5, 2.25, 2.5, 3.2,
  6.75 (4:3 and 16:9) and native in 2880 × 1620 and 1366 × 768 windows, on GLES and desktop GL, on the Pi's V3D,
  rocky's iris and llvmpipe on both; `gfx_test all` passes at 1.25 and 3.2 on V3D. The Pi's 111 gfx CTest entries
  and rocky's 225 (Debug and RelWithDebInfo) pass.
