/* port/gfx/gfx_internal.h: state shared by the interpreter (gfx_rsp.c, gfx_rdp.c), the combiner compiler
 * (gfx_combine.c) and the OpenGL backend (gfx_gl.c). docs/port/GFX.md describes the design. */
#ifndef GFX_INTERNAL_H
#define GFX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "gfx_gbi.h"
#include "gfx_hfr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- memory access */

/* Texels and TLUT entries, which the renderer reads in either byte order: high byte first in texel memory (a file's
 * texels and TLUTs, which keep the formats the N64's RDP reads: pc_mem_register_texels, pc_port.h), the host's
 * elsewhere (the texels and palettes the game builds). Everything else it reads (lists, vertices, matrices,
 * viewports, lights) is in the host's byte order. */
uint16_t pcgfx_rd16(const void *p, int high_first);
uint32_t pcgfx_rd32(const void *p, int high_first);
int pcgfx_is_texel_memory(const void *p);   /* pc_mem_is_texel_memory, tolerating NULL */
/* An address from a command's w1 or an API argument: a host pointer, or a segment's offset where a list set the
 * segment's base (GFX.md "Addresses"). */
const uint8_t *pcgfx_resolve(uintptr_t addr);

/* ---------------------------------------------------------------- RSP state */

typedef struct {
    float x, y, z, w;   /* clip space */
    float r, g, b, a;   /* shade, 0..1 (alpha holds fog when G_FOG) */
    float s, t;         /* texture coordinates after the G_TEXTURE scale, in texel*32 (s10.5) units */
    uint8_t clip;       /* outcodes: 1 x<-w, 2 x>w, 4 y<-w, 8 y>w, 16 w<1e-4, 32 outside the guard band (gfx_rsp.c) */
} GfxVtx;

typedef struct {
    uint8_t col[3];
    uint8_t colc[3];
    int8_t dir[3];
} GfxLight;

#define GFX_MTX_STACK 32
#define GFX_DL_STACK 18
#define GFX_MAX_LIGHTS 8

typedef struct {
    uint8_t fmt, siz, palette;
    uint8_t cms, cmt, masks, maskt, shifts, shiftt;
    uint16_t line, tmem;               /* in 64-bit words */
    uint16_t uls, ult, lrs, lrt;       /* 10.2 */
} GfxTile;

typedef struct {
    /* RSP */
    float mv[GFX_MTX_STACK][4][4];
    int mv_top;
    float proj[4][4];
    float mp[4][4];
    int mp_dirty;
    GfxVtx vtx[32];
    uint32_t geom;
    GfxLight lights[GFX_MAX_LIGHTS + 1];
    GfxLight lookat[2];
    int num_lights;
    int lights_dirty;
    float light_dir[GFX_MAX_LIGHTS][3];   /* in model space, normalized */
    float lookat_dir[2][3];
    float vp_scale[3], vp_trans[3];       /* pixels (x, y), z units (0..0x3FF) */
    int persp;                            /* the projection is perspective (its w column is not 0 0 0 1) */
    int16_t fog_mul, fog_off;
    /* draw_distance and fog (gfx_detail.c, docs/port/DETAIL.md 3): the projection's z/w = proj_za + proj_zb / w
     * (proj_depth: a perspective view's), and the depth map for it and the viewport, d' = d dz_mul + dz_add (dz_on) */
    float proj_za, proj_zb, dz_mul, dz_add;
    uint8_t proj_depth, dz_on, dz_dirty;
    uint16_t tex_scale_s, tex_scale_t;
    uint8_t tex_level, tex_tile, tex_on;
    uintptr_t seg[16];
    uintptr_t half1;
    uint32_t half2;

    /* RDP */
    uint32_t om_h, om_l;
    uint32_t cc_w0, cc_w1;
    uint32_t fill_color;
    uint8_t prim[4], env[4], blend[4], fog[4];
    uint8_t prim_lod, prim_min_level;
    uint16_t prim_z, prim_dz;
    uint16_t sc_ulx, sc_uly, sc_lrx, sc_lry;   /* 10.2 */
    struct { const uint8_t *addr; uint8_t fmt, siz; uint16_t width; } timg;
    struct { const uint8_t *addr; uint8_t fmt, siz; uint16_t width; } cimg;
    const uint8_t *zimg;
    uint16_t zimg_width;
    int wide_ox;                 /* widescreen: N64 pixels added on each side of the color image's frame */
    GfxTile tiles[8];
    uint8_t tmem[4096];          /* RDP byte order (big-endian texels) */
    uint8_t tmem_le16[512];      /* per 64-bit word: loaded raw from native little-endian memory as 16b data */
    uint32_t tmem_gen;
    int state_dirty;             /* draw state must be rebuilt */
    int tex_dirty;               /* textures must be looked up again */

    /* Frame interpolation (gfx_interp.c): what the recorder needs to tell transforms and views apart. */
    uint32_t xf_gen;             /* bumped by every change of the modelview, the projection or MP */
    uint16_t view_ord;           /* projection loads (G_MTX_PROJECTION | G_MTX_LOAD) in this task */
    uint16_t vp_ord;             /* viewports (G_MV_VIEWPORT) in this task */
    uint8_t mp_forced;           /* MP set directly (G_MV_MATRIX, G_MW_MATRIX): it is not modelview x projection */

    /* statistics for the last task */
    uint32_t stat_cmds, stat_tris, stat_draws, stat_unknown;
} GfxState;

extern GfxState pcgfx;

/* ---------------------------------------------------------------- output vertices and draw state */

/* What one GL vertex carries: clip position, shade, texture coordinates for the two texture units. */
typedef struct {
    float pos[4];
    float shade[4];
    float uv[4];
} GfxOutVtx;

/* Shader identity: everything that changes the generated GLSL. */
typedef struct {
    uint32_t cc_w0, cc_w1;   /* combine mode (w0 without opcode) */
    uint16_t blender;        /* othermode_l >> 16 */
    uint8_t mode;            /* GFX_MODE_* */
    uint8_t ac;              /* 0 none, 1 threshold, 3 dither, 4 copy-mode alpha test */
    uint8_t flags;           /* GFX_KF_* */
    uint8_t lod_levels;      /* texture LOD (G_TL_LOD): the mip levels, tiles from the G_TEXTURE tile on; 0 none */
    uint8_t pad[2];
} GfxShaderKey;

#define GFX_LOD_MAX 8   /* mip levels: tiles t .. t + 7, as G_TEXTURE's level field (0-7) allows */

enum { GFX_MODE_1CYC, GFX_MODE_2CYC, GFX_MODE_COPY, GFX_MODE_FILL };
enum {
    GFX_KF_FORCE_BL = 1,
    GFX_KF_ALPHA_CVG_SEL = 2,
    GFX_KF_CVG_X_ALPHA = 4,
    GFX_KF_TEX0 = 8,
    GFX_KF_TEX1 = 16,
    GFX_KF_SNAP = 32,   /* point-sampled rectangle above 1x: sample at each N64 pixel's corner */
};

typedef struct GfxShader GfxShader;
typedef struct GfxTarget GfxTarget;

typedef struct {
    GfxShader *shader;
    GfxTarget *target;
    uint32_t tex[GFX_LOD_MAX];      /* GL texture names, 0 = unused: TEXEL0, TEXEL1; with LOD, each mip level's */
    uint8_t linear[GFX_LOD_MAX], repeat_s[GFX_LOD_MAX], repeat_t[GFX_LOD_MAX];
    uint8_t lod_levels;             /* the key's: 0, or the mip levels bound (units 0 .. lod_levels - 1) */
    uint8_t depth_test, depth_write, decal, blend;
    /* With LOD, mip level k's texture coordinates from a vertex's s, t (texels): uv = (st * a.xy - a.zw) * b.xy:
     * the tile's shift, its origin (half a texel back when filtered) and its texture's size. */
    float lod_a[GFX_LOD_MAX][4], lod_b[GFX_LOD_MAX][2];
    int32_t scissor[4];             /* x0, y0, x1, y1 in N64 pixels, exclusive */
    float prim[4], env[4], fog[4], blendc[4], fill[4];
    float prim_lod, lod_frac;
    float noise_seed;
    float scale;                    /* internal resolution multiple (any fraction from 1: GFX.md "Resolution and scale") */
    uint32_t shader_flags;          /* the key's GFX_KF_* */
} GfxDrawState;

/* ---------------------------------------------------------------- interpreter entry points */

void pcgfx_rsp_reset(void);      /* RSP state at task start */
void pcgfx_rdp_reset(void);      /* RDP state at init */
void pcgfx_run_list(const void *dl);

/* How a texture unit maps a vertex's s, t to the bound texture (set with the draw state, gfx_rdp.c). */
typedef struct {
    int tile;
    float uls, ult;   /* texels */
    int shifts, shiftt;
    float w, h;
    int linear;
    float ml, wt;   /* side art (TEXTURES.md): the GL texture's columns left of the texture's own, and all of them
                     * (0: a plain texture, u = s / w) */
} GfxUnitInfo;

/* What turning a triangle's vertices into GL vertices needs besides them (gfx_rsp.c): the viewport and where it
 * lands in the target, the geometry mode's culling and shading, the depth source. The live interpreter fills it
 * from its state; the frame interpolator (gfx_interp.c) replays recorded ones. */
typedef struct {
    float vp_scale[3], vp_trans[3];
    float vx, xs;           /* view_x: the viewport's x centre in the target, and clip x's widening for outcodes */
    float ys;               /* clip y's for outcodes: 1, or a taller view's (own view, gfx_view.c) */
    int tw, th;             /* the target, N64 pixels */
    uint32_t geom;          /* G_CULL_*, G_SHADING_SMOOTH */
    uint16_t prim_z;
    uint8_t zs_prim;        /* G_ZS_PRIM: the primitive depth */
    uint8_t tex;            /* G_TEXTURE on */
    uint8_t wide;           /* widescreen 3D: the draw state's scissor widens (1); own view's 3D (2: gfx_view.c) */
    uint8_t side;           /* and side art is drawn: 3D ending at the frame's sides goes on (pcgfx_side_continue) */
    float side_l, side_r;   /* as far as the art reaches into the left and right bands, N64 pixels */
    uint8_t dz_on;          /* draw_distance far or max: the depth is d * dz_mul + dz_add (gfx_detail.c) */
    float dz_mul, dz_add;
} GfxTriCtx;

/* A triangle's visible polygon: trivially rejected, clipped to the guard band and the eye plane, culled by its winding.
 * Returns its vertex count (0: nothing to draw). */
int pcgfx_tri_visible(const GfxVtx in[3], const GfxTriCtx *c, GfxVtx poly[8]);
/* The polygon as GL triangles (up to GFX_TRI_OUT_MAX vertices), clipped to GL's guard band when it reaches past it
 * (gfx_rsp.c); flat shading takes first's colour. Returns the vertex count. */
#define GFX_OUT_POLY_MAX 12   /* the polygon's 8 vertices, and a vertex more for each of GL's guard band's 4 sides */
#define GFX_TRI_OUT_MAX (3 * (GFX_OUT_POLY_MAX - 2))
int pcgfx_tri_output(const GfxVtx *poly, int n, const GfxVtx *first, const GfxTriCtx *c, const GfxDrawState *st,
                     const GfxUnitInfo units[2], GfxOutVtx *tri);
uint8_t pcgfx_outcode(const GfxVtx *v, float xs, float ys);   /* xs, ys: the view's widening of clip x, y (1: none) */
/* Side art (TEXTURES.md): a triangle's vertices on the frame's sides moved out as far as the art reaches (c->side_l,
 * side_r), its texture coordinates going on along the triangle's change across the screen (c's viewport; the
 * frame's width). */
void pcgfx_side_continue(GfxVtx in[3], const GfxTriCtx *c, int frame_w);
/* G_FOG's shade alpha for a vertex at clip z, w; za: the projection's z/w at infinity (proj_za; 0 for none). With
 * video.fog far or off, or draw_distance far or max, as gfx_detail.c says; otherwise the N64's. */
float pcgfx_fog_alpha(float z, float w, int16_t fog_mul, int16_t fog_off, float za);

/* gfx_detail.c: video.draw_distance and video.fog in the renderer (docs/port/DETAIL.md 3) */
void pcgfx_detail_init(void);   /* RUSH_GFX_DRAW_DISTANCE, RUSH_GFX_FOG, RUSH_GFX_TEXTURE_LOD */
void pcgfx_detail_projection(const float p[4][4]);    /* a projection loaded or multiplied: proj_za, proj_zb */
void pcgfx_detail_ctx(GfxTriCtx *c);                  /* a triangle's depth map */
float pcgfx_detail_rect_depth(float d);               /* a rectangle's primitive depth, mapped as the view's */
void pcgfx_detail_task(uint32_t triangles);           /* a graphics task's triangles drawn (pc_gfx_scene_stats) */

/* gfx_rdp.c */
void pcgfx_rdp_command(uint32_t op, uint32_t w0, uintptr_t w1);
void pcgfx_texrect(uint32_t w0, uint32_t w1, uint32_t half1, uint32_t half2, int flip);
/* rect_tile: -1 triangles, 0-7 texrect tile, 8 untextured rect. wide: in widescreen, the scissor's sides at the
 * frame's go to the target's (3D and full-width draws), else the scissor moves with the 4:3 frame (2D). */
const GfxDrawState *pcgfx_draw_state(int rect_tile, int wide);
/* The draw state in effect when it is still the one for (rect_tile, wide) and nothing changed since; else NULL.
 * No side effects (the frame interpolator records rejected triangles with it). */
const GfxDrawState *pcgfx_draw_state_peek(int rect_tile, int wide);
const GfxUnitInfo *pcgfx_units(void);     /* the draw state's two texture units */
void pcgfx_target_dims(int *w, int *h);   /* the color image's target, N64 pixels (widened in widescreen) */
int pcgfx_frame_width(void);              /* the color image's 4:3 frame width, N64 pixels */
float pcgfx_wide_x(float x);              /* widescreen: a frame x edge's place in the target (GFX.md) */
float pcgfx_wide_x_at(float x, int frame_width, int ox);   /* the same for a given frame width and widening */
/* Side art drawn this frame or the last (gfx_rdp.c, TEXTURES.md): 1 if so, and how far into the left and right
 * bands it reached, N64 pixels (NULL for not wanted). */
int pcgfx_side_art_frame(float *left, float *right);
int pcgfx_wide_width(int frame_width);    /* a frame's target width at the aspect in effect */
/* Texture coordinate transform for tile (tile-relative, normalized for the bound texture). */
void pcgfx_tile_uv(int unit, float s_texel, float t_texel, float *u, float *v);
void pcgfx_unit_uv(const GfxUnitInfo *ui, float s_texel, float t_texel, float *u, float *v);
void pcgfx_emit(const GfxOutVtx *v, int n);          /* triangles, 3 vertices each */
void pcgfx_flush(void);

/* gfx_combine.c */
char *pcgfx_combine_glsl(const GfxShaderKey *key, int gles, int glsl_version);   /* malloc'd fragment source */
const char *pcgfx_vertex_source(int gles, int glsl_version);
double pcgfx_now_ms(void);   /* gfx_api.c: milliseconds on a monotonic clock */
const char *pcgfx_ui_fragment_source(int gles, int glsl_version);   /* the overlay's (gfx_ui.c) */
int pcgfx_combine_uses(const GfxShaderKey *key, int texel);   /* does the key read TEXEL0 (0) or TEXEL1 (1)? */
int pcgfx_blend_uses_mem(const GfxShaderKey *key);

/* gfx_gl.c */
typedef void *(*GfxGetProc)(const char *name);
int pcgfx_gl_init(GfxGetProc get_proc, int gles);
void pcgfx_gl_shutdown(void);
GfxTarget *pcgfx_gl_target(const void *addr, int width, int height);
GfxTarget *pcgfx_gl_find_target(const void *addr);
GfxTarget *pcgfx_gl_last_target(void);
void pcgfx_gl_attach_depth(GfxTarget *t, const void *zaddr);
void pcgfx_gl_clear(GfxTarget *t, const int32_t rect[4], int depth, const float rgba[4], float z);
uint32_t pcgfx_gl_texture(const uint8_t *rgba, int w, int h);
void pcgfx_gl_delete_texture(uint32_t tex);
GfxShader *pcgfx_gl_shader(const GfxShaderKey *key);
void pcgfx_gl_draw(const GfxDrawState *st, const GfxOutVtx *v, int n);
/* Whether vertices batched under state a draw the same under b: one shader, and the same state but for the uniforms
 * the shader doesn't read (pcgfx_draw_state then keeps the batch: one draw where there were two). */
int pcgfx_gl_same_draw(const GfxDrawState *a, const GfxDrawState *b);
void pcgfx_gl_present(GfxTarget *t, int win_w, int win_h, int to_window);
/* The presented frame in the window: fitted (gfx_size.c's pcgfx_fit) natively when the scale is native. */
/* The frame in t becomes the presented image (screenshots read it), without showing it. */
void pcgfx_gl_present_image(GfxTarget *t);
/* The presented frame in the window (black if none yet): to_window, the default framebuffer; else the headless
 * window, an offscreen image of the window's size. */
void pcgfx_gl_show(int win_w, int win_h, int to_window);
/* A target's top rows in the window instead, fitted as the presented frame is (frame interpolation). */
void pcgfx_gl_show_target(GfxTarget *t, int rows, int win_w, int win_h, int to_window);
/* A render target of its own, outside the targets the color images name, with its own depth buffer: (re)made at
 * w x h N64 pixels and scale s when t is NULL or differs. The frame interpolator draws into it. */
GfxTarget *pcgfx_gl_aux_target(GfxTarget *t, int w, int h, float s);
void pcgfx_gl_aux_free(GfxTarget *t);
void pcgfx_gl_finish(void);   /* glFinish: for timing the GPU's part (RUSH_GFX_TIMES=2) */
/* GPU timers (RUSH_GFX_TIMES=3): GL_TIME_ELAPSED queries around the parts timed, one at a time, by category; the
 * totals and counts read back at the end. Without timer queries (-1) they do nothing. */
enum { PCGFX_GPU_TASK, PCGFX_GPU_BETWEEN, PCGFX_GPU_SHOW, PCGFX_GPU_TIMER_CATS };
int pcgfx_gl_timers_init(GfxGetProc get_proc);
void pcgfx_gl_timer_begin(int cat);
void pcgfx_gl_timer_end(void);
int pcgfx_gl_timer_stats(double ms[PCGFX_GPU_TIMER_CATS], uint32_t n[PCGFX_GPU_TIMER_CATS]);
void pcgfx_gl_draw_counts(uint32_t *draws, uint64_t *verts);   /* pcgfx_gl_draw calls and vertices so far */
/* A fence after the GL work so far; whether the GPU hasn't passed the last one within 2 ms (0 without fences). */
void pcgfx_gl_fence_set(void);
void pcgfx_gl_frame_done(void);   /* at each present: at most two frames of GL work in flight */
int pcgfx_gl_fence_busy(void);
/* The overlay's triangles (gfx_ui.c), in window pixels, blended over the window; the atlas is uploaded once. */
void pcgfx_gl_ui_draw(const GfxOutVtx *v, int n, int win_w, int win_h, int to_window, const uint8_t *atlas,
                      int atlas_w, int atlas_h);
int pcgfx_gl_read_target(GfxTarget *t, uint8_t *rgba, int *w, int *h);   /* top row first, RGBA8 */
int pcgfx_gl_read_presented(uint8_t *rgba, int *w, int *h);
int pcgfx_gl_read_window(uint8_t *rgba, int w, int h);   /* the default framebuffer, top row first */
int pcgfx_gl_read_headless_window(uint8_t *rgba, int *w, int *h);   /* -1 before the first show */

/* gfx_ui.c */
int pcgfx_ui_active(void);                          /* a UI hook is set */
void pcgfx_ui_run(int w, int h, int to_window);     /* the hook's drawing, over the window */
float pcgfx_gl_target_scale(const GfxTarget *t);   /* NULL: the scale a new target gets */
/* The target's size in internal pixels (pcgfx_scaled of its N64 size), and its scale per axis: px / N64 pixels. */
void pcgfx_gl_target_px(const GfxTarget *t, int *pw, int *ph, float *sx, float *sy);
void pcgfx_gl_target_shown(const GfxTarget *t, int *w, int *rows);   /* N64 pixels the present shows */
void pcgfx_gl_target_dims(const GfxTarget *t, int *w, int *h);       /* N64 pixels */

/* gfx_size.c: sizes without GL (GFX.md "Resolution and scale"; gfx_size_test) */
#define PCGFX_SCALE_MAX 16   /* native's most: 5120 pixels across a 4:3 frame */
int pcgfx_scaled(int n, float s);            /* n N64 pixels at scale s, round(n * s) */
int pcgfx_edge(int v, int n, int px);        /* N64 edge v of n pixels drawn as px: its internal pixel */
int pcgfx_pixel_of(int i, int n, int px);    /* internal pixel i's N64 pixel (its centre's) */
int pcgfx_row_edge(int v, int n, int px);    /* the same for rows, from the top, by GL's y-up tie rule */
int pcgfx_row_of(int i, int n, int px);
int pcgfx_edge_q(long long q, int n, int px, int *exact);       /* an edge at q/4 N64 pixels, any sign */
int pcgfx_row_edge_q(long long q, int n, int px, int *exact);
/* A rectangle's side at N64 x (y) in GL clip coordinates, on its internal pixel boundary within the target; *xe the
 * boundary in N64 pixels, where its texture coordinates are taken */
float pcgfx_rect_x(float x, int n, int px, float *xe);
float pcgfx_rect_y(float y, int n, int px, float *ye);
float pcgfx_window_scale(int mode, int win_w, int win_h, int fw, int rows, int fill);   /* 0 auto, < 0 native */
int pcgfx_fit(int src_w, int src_h, int fw, int rows, int win_w, int win_h, int native, int fill, int r[4]);
void pcgfx_units_of_pixels(int px_w, int px_h, int win_w, int win_h, int draw_w, int draw_h, int *w, int *h);
void pcgfx_default_window(int desk_w, int desk_h, int usable_w, int usable_h, float aspect, int *w, int *h);

/* gfx_png.c */
int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);

/* gfx_hud.c: the race HUD at the screen's edges (GFX.md "The HUD at the screen's edges"). While pcgfx_hud_on(), 2D
 * primitives are queued with pcgfx_hud_add (their GL vertices as drawn 4:3, and for rectangles each vertex's x at the
 * 4:3 place in target N64 pixels; NULL for triangles) instead of drawn, and pcgfx_hud_resolve draws the queue, each
 * group moved to its side. It runs before anything else is drawn or cleared, at a new color image and at the task's
 * end. pcgfx_hud_add returns -1 when it can't queue (draw it as usual). */
void pcgfx_hud_init(void);         /* RUSH_GFX_HUD */
void pcgfx_hud_publish(int frame_width, int ox);   /* at SETCIMG: what pc_gfx_hud_dx (the game's thread) reads */
void pcgfx_hud_task_begin(void);
int pcgfx_hud_on(void);
int pcgfx_hud_pending(void);
/* Rectangles also give each vertex's y in target N64 pixels (y43: the box, since the GL vertex is on its internal
 * pixel boundary) and the texture coordinates' change per N64 pixel across and down (du[8]: both units' u and v across,
 * then down), so that a move puts the sides on the boundaries of their new place, with the texture coordinates there.
 * NULL for triangles. With own view (gfx_view.c) it is on in a race of several views at any aspect, and moves down too. */
int pcgfx_hud_add(const GfxDrawState *st, const GfxOutVtx *v, const float *x43, const float *y43, const float *du,
                  int n, int movable, int tri);
void pcgfx_hud_resolve(void);

/* gfx_view.c: one view of the split screen alone, across the frame (net.view = own; GFX.md "One view of the split
 * screen"). While pcgfx_view_on() (a race of several views, own view, a player to show), the interpreter asks it: */
#define PCGFX_VIEWS_MAX 4
void pcgfx_view_init(void);         /* RUSH_GFX_VIEW */
void pcgfx_view_task_begin(void);
int pcgfx_view_on(void);
int pcgfx_view_shown(void);         /* the view shown, 0-based; -1 off */
/* A perspective draw under a viewport centred at trans: -1 no view's (drawn as usual), 0 another view's (not drawn),
 * 1 the view shown's (pcgfx_view_place). */
int pcgfx_view_of(const float trans[3]);
/* The view shown's viewport (N64 pixels) as drawn: zoomed and centred in the frame (s_out, t_out), and the frame's half
 * size (half), which its picture reaches. */
void pcgfx_view_place(const float scale[3], const float trans[3], float s_out[2], float t_out[2], float half[2]);
/* A rectangle (x0, y0, x1, y1, frame N64 pixels) in the view shown, where its picture puts it: the 3D's scissor. */
void pcgfx_view_rect(int32_t r[4]);
/* A fill-mode rectangle (the same units): 1 to draw it (mapped when it is the view shown's), 0 not (another view's,
 * a divider, the side panel's). */
int pcgfx_view_fill(int32_t r[4]);
/* Which view a 2D box (frame N64 pixels) is in: its index, or -1 for none (the frame's). */
int pcgfx_view_box(float x0, float y0, float x1, float y1);
/* 1: the box covers the whole frame (the pause screen's darkening): what is drawn after it is the frame's. */
int pcgfx_view_covers(float x0, float y0, float x1, float y1);
/* A 2D group's move, N64 pixels across (dx, in the target) and down (dy), its box (frame N64 pixels) in view v; 0 to
 * draw it so moved, -1 not to draw it. ox: the widening; edges: video.hud's edges in effect. */
int pcgfx_view_move(int v, float x0, float y0, float x1, float y1, int ox, int edges, int *dx, int *dy);
/* A model at a HUD point in the view shown (ZOID_WindowTo3D: the coin icons): 1, and where its viewport's centre goes
 * (x, y: the target's N64 pixels) so that it moves as its point's 2D does, at the view's own scale. */
int pcgfx_view_hud_point(const float mp[4][4], const float scale[3], const float trans[3], int ox, int edges,
                         float *x, float *y);
void pcgfx_view_scales(float *xs, float *ys);   /* gfx_rsp.c: the outcodes' widening of x and y for the view now */

/* gfx_interp.c: frame interpolation (GFX.md "High frame rate"). While pcgfx_rec is set, a graphics task is being
 * recorded: the interpreter reports its vertex loads, triangles, rectangles and clears, so that the frame can be
 * drawn again later with its transforms moved between the previous frame's and its own. */
extern void *pcgfx_rec;
void pcgfx_rec_task_begin(void);
void pcgfx_rec_task_end(void);
/* After load_vertices filled pcgfx.vtx from p; model: p is a model's vertices, which stay put while its file is loaded
 * (pc_mem_is_model_data), where the game's own are made each frame. */
void pcgfx_rec_load(const uint8_t *p, int model, int v0, int n);
void pcgfx_rec_modify(unsigned idx, int moved);   /* G_MODIFYVTX; moved: its screen position was set */
/* A triangle of the vertex buffer's slots. st: the draw state it was drawn with, NULL when it was rejected (the
 * recorder then takes the state in effect if it is still current, since another frame may show the triangle). */
void pcgfx_rec_tri(unsigned i0, unsigned i1, unsigned i2, const GfxTriCtx *c, const GfxDrawState *st);
void pcgfx_rec_verts(const GfxDrawState *st, const GfxOutVtx *v, int n);   /* a rectangle's GL vertices */
void pcgfx_rec_clear(const GfxTarget *t, const int32_t rect[4], int depth, const float rgba[4], float z);
void pcgfx_rec_viewport(void);    /* after G_MV_VIEWPORT */
/* The presentation side (gfx_api.c). */
void pcgfx_hfr_presented(GfxTarget *t, uint32_t vi);   /* t's frame was swapped in at retrace vi */
/* What to show at time vt (retraces): 0 the presented image; else the target *t, its top *rows rows: 1 the next
 * frame, as the game drew it (it is due and not swapped in yet), 2 a frame drawn between the two; 3 nothing new
 * (mode 1 and the GPU hasn't finished what it was given, or mode 2: the screen keeps what it shows). The mode is
 * pc_gfx_present_tick's. */
int pcgfx_hfr_frame(double vt, GfxTarget **t, int *rows, int mode);
int pcgfx_hfr_wants_next(double vt);   /* a frame at vt is between the shown one and the next, not drawn yet */
double pcgfx_hfr_moment(void);         /* the moment of the next frame last shown as drawn, in retraces */
void pcgfx_hfr_dump(GfxTarget *t, int drawn);   /* RUSH_GFX_HFR_DUMP: the frame just shown (t NULL: presented) */
void pcgfx_hfr_reset(void);       /* forget the recorded frames */
void pcgfx_hfr_shutdown(void);
/* Its statistics (pcgfx_hfr_stats) and test hooks: gfx_hfr.h. */

/* Context backends: gfx_egl.c (headless), gfx_sdl.c (window, when built with SDL2) */
int pcgfx_egl_init(int want_gles, GfxGetProc *get_proc, int *is_gles);
void pcgfx_egl_shutdown(void);
int pcgfx_sdl_init(int w, int h, int want_gles, GfxGetProc *get_proc, int *is_gles);
int pcgfx_want_gles(const char *api);   /* RUSH_GFX_API: which API to try first (gfx_api.c) */
void pcgfx_sdl_swap(void);
int pcgfx_sdl_poll(void);
void pcgfx_sdl_size(int *w, int *h);
int pcgfx_sdl_refresh(void);   /* the window's display's refresh rate, Hz; 0 unknown */
/* The window's display's exact refresh and vblanks, where the platform gives them (gfx_display.c): PCGFX_TIMING_*
 * bits, *period_ns and *vblank_age_ns (how long ago a vblank was; negative: to come) set for those. */
int pcgfx_sdl_display_timing(double *period_ns, int64_t *vblank_age_ns);
/* The window and the display (pc_port.h's pc_gfx_set_fullscreen and the rest, through gfx_api.c) */
void pcgfx_sdl_place(int x, int y);
void pcgfx_sdl_set_fullscreen_mode(int mode);
void pcgfx_sdl_set_resolution(int w, int h);
int pcgfx_sdl_set_fullscreen(int on);
int pcgfx_sdl_is_fullscreen(void);
int pcgfx_sdl_set_vsync(int on);
void pcgfx_sdl_set_window_pixels(int w, int h);
int pcgfx_sdl_geometry(int *x, int *y, int *w, int *h);
void pcgfx_sdl_shutdown(void);

/* Settings (gfx_api.c). The internal scale multiplies the N64 frame size. Widescreen widens the frame to a display
 * aspect (GFX.md "Widescreen"). The auto values follow the window and are taken again at each present. */
typedef struct {
    float internal_scale;   /* 1 = the N64 frame size, up to 8 (a fraction from RUSH_GFX_SCALE too); 0 = auto: the
                             * largest whole multiple the window shows; -1 = native: the window's own pixels */
    float scale;            /* the scale in effect, taken at each color image */
    float widescreen;       /* display aspect to widen the 4:3 frame to: 0 off, < 0 the window's */
    float aspect;           /* the aspect in effect (the window's for < 0), taken at each color image; 0 = 4:3 */
    int verbose;
    int frame_height;       /* rows shown; 0 = the largest scissor bottom drawn with */
    int trace_task;         /* RUSH_GFX_TRACE: print every command of this graphics task (1 = the first); 0 none */
    int frame_rate;         /* RUSH_GFX_FRAME_RATE: 0 the game's own; > 0 frames shown a second; -1 the display's */
    int draw_distance;      /* RUSH_GFX_DRAW_DISTANCE: PC_DRAW_* (the depth range's far plane: gfx_detail.c) */
    int fog;                /* RUSH_GFX_FOG: PC_FOG_* */
    int texture_lod;        /* RUSH_GFX_TEXTURE_LOD: PC_TEXLOD_* (the mip level's pixels: gfx_combine.c, gfx_gl.c) */
    int fill;               /* RUSH_GFX_FILL: the aspect is the rows shown's, so the picture fills the window's height
                             * (GFX.md "Fill the window"); 0: the VI's 240-row frame's, the game's bands above and below */
} GfxConfig;
extern GfxConfig pcgfx_cfg;

/* Test and tool access (not part of pc_port.h). */
int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
/* What the window shows: the default framebuffer, or headless the offscreen window image the present also fills
 * (the frame letterboxed at the window's size, the overlay over it). */
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
/* Widescreen at run time: the display aspect (16/9.0f, ...), 0 for off, < 0 for the window's. It applies from the
 * next color image (SETCIMG). */
void pc_gfx_set_widescreen(float aspect);
float pc_gfx_parse_aspect(const char *text, int *ok);   /* "off", "auto", "16:9", "2.39" -> as above */
/* Fill the window at run time (video.fill): 1 the widescreen aspect is the shown picture's, 0 the VI frame's. It
 * applies from the next color image. */
void pc_gfx_set_fill(int on);
/* The internal scale at run time: 1-8, 0 for auto, -1 for native. It applies from the next color image. */
void pc_gfx_set_scale(int scale);
void pc_gfx_set_scale_value(float scale);   /* the same, or a fixed fraction from 1 to 16 (tests, tools) */
/* The size the native and auto scales fit instead of the window's (0 x 0: the window's): video.resolution in
 * desktop fullscreen. The frame is drawn for that size, then fitted to the window. */
void pc_gfx_set_render_size(int w, int h);
/* "native", "auto", "n64" (1), or a multiple from 1 to 16 (fractions too: 2.25); *ok = 0 for anything else. */
float pc_gfx_parse_scale(const char *text, int *ok);
/* The frame's internal size in N64 and in internal pixels (tests: where an N64 pixel is in the target). */
int pc_gfx_framebuffer_dims(const void *cimg, int *n64_w, int *n64_h, int *px_w, int *px_h);
/* The size the auto settings follow when there is no window (headless tests and tools). */
void pc_gfx_set_window_size(int w, int h);

/* ---------------------------------------------------------------- the display's refresh (gfx_display.c) */
#define PCGFX_TIMING_PERIOD 1   /* the refresh period is the display's own, not SDL's whole hertz */
#define PCGFX_TIMING_VBLANK 2   /* and a vblank's time */
double pcgfx_mode_hz(uint64_t dot_clock, uint32_t htotal, uint32_t vtotal, int interlace, int doublescan);
double pcgfx_ratio_hz(uint32_t num, uint32_t den);
int pcgfx_refresh_plausible(double exact_hz, int whole_hz);
int64_t pcgfx_counter_age_ns(uint64_t at, uint64_t now, uint64_t freq);
int pcgfx_display_pick(double mon_hz, double comp_hz, int comp_vblank, int64_t comp_age_ns, int whole_hz,
                       double *period_ns, int64_t *vblank_age_ns);
int pcgfx_win_display_timing(void *hwnd, int whole_hz, double *period_ns, int64_t *vblank_age_ns);
int pcgfx_x11_display_timing(void *display, int x, int y, int whole_hz, double *period_ns);

#ifdef __cplusplus
}
#endif

#endif
