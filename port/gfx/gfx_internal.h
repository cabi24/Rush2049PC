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

/* A display list position: N64 form (8-byte big-endian commands) or native form (PcGfxWords, 16 bytes). */
typedef struct {
    const uint8_t *p;
    int be;
} GfxDlPtr;

uint16_t pcgfx_rd16(const void *p, int be);
uint32_t pcgfx_rd32(const void *p, int be);
int pcgfx_is_be(const void *p);   /* pc_mem_is_be, tolerating NULL */
/* An address from a command's w1 or an API argument: host pointer (>= 4 GB) or N64 address (GFX.md "Addresses"). */
const uint8_t *pcgfx_resolve(uintptr_t addr);
uintptr_t pcgfx_segment_value(uintptr_t v);   /* a segment base as set by MOVEWORD -> host base (0 = physical) */

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
    uint8_t pad[3];
} GfxShaderKey;

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
    uint32_t tex[2];                /* GL texture names, 0 = unused */
    uint8_t linear[2], repeat_s[2], repeat_t[2];
    uint8_t depth_test, depth_write, decal, blend;
    int32_t scissor[4];             /* x0, y0, x1, y1 in N64 pixels, exclusive */
    float prim[4], env[4], fog[4], blendc[4], fill[4];
    float prim_lod, lod_frac;
    float noise_seed;
    float scale;                    /* internal resolution multiple */
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
} GfxUnitInfo;

/* What turning a triangle's vertices into GL vertices needs besides them (gfx_rsp.c): the viewport and where it
 * lands in the target, the geometry mode's culling and shading, the depth source. The live interpreter fills it
 * from its state; the frame interpolator (gfx_interp.c) replays recorded ones. */
typedef struct {
    float vp_scale[3], vp_trans[3];
    float vx, xs;           /* view_x: the viewport's x centre in the target, and clip x's widening for outcodes */
    int tw, th;             /* the target, N64 pixels */
    uint32_t geom;          /* G_CULL_*, G_SHADING_SMOOTH */
    uint16_t prim_z;
    uint8_t zs_prim;        /* G_ZS_PRIM: the primitive depth */
    uint8_t tex;            /* G_TEXTURE on */
    uint8_t wide;           /* widescreen 3D: the draw state's scissor widens */
} GfxTriCtx;

/* A triangle's visible polygon: trivially rejected, clipped to the guard band and the eye plane, culled by its winding.
 * Returns its vertex count (0: nothing to draw). */
int pcgfx_tri_visible(const GfxVtx in[3], const GfxTriCtx *c, GfxVtx poly[8]);
/* The polygon as GL triangles (up to 18 vertices); flat shading takes first's colour. Returns the vertex count. */
int pcgfx_tri_output(const GfxVtx *poly, int n, const GfxVtx *first, const GfxTriCtx *c, const GfxDrawState *st,
                     const GfxUnitInfo units[2], GfxOutVtx *tri);
uint8_t pcgfx_outcode(const GfxVtx *v, float xs);
float pcgfx_fog_alpha(float z, float w, int16_t fog_mul, int16_t fog_off);

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
int pcgfx_wide_width(int frame_width);    /* a frame's target width at the aspect in effect */
/* Texture coordinate transform for tile (tile-relative, normalized for the bound texture). */
void pcgfx_tile_uv(int unit, float s_texel, float t_texel, float *u, float *v);
void pcgfx_unit_uv(const GfxUnitInfo *ui, float s_texel, float t_texel, float *u, float *v);
void pcgfx_emit(const GfxOutVtx *v, int n);          /* triangles, 3 vertices each */
void pcgfx_flush(void);

/* gfx_combine.c */
char *pcgfx_combine_glsl(const GfxShaderKey *key, int gles, int glsl_version);   /* malloc'd fragment source */
const char *pcgfx_vertex_source(int gles, int glsl_version);
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
void pcgfx_gl_present(GfxTarget *t, int win_w, int win_h, int to_window);
/* The frame in t becomes the presented image (screenshots read it), without showing it. */
void pcgfx_gl_present_image(GfxTarget *t);
/* The presented frame in the window (black if none yet): to_window, the default framebuffer; else the headless
 * window, an offscreen image of the window's size. */
void pcgfx_gl_show(int win_w, int win_h, int to_window);
/* A target's top rows in the window instead, fitted as the presented frame is (frame interpolation). */
void pcgfx_gl_show_target(GfxTarget *t, int rows, int win_w, int win_h, int to_window);
/* A render target of its own, outside the targets the color images name, with its own depth buffer: (re)made at
 * w x h N64 pixels and scale s when t is NULL or differs. The frame interpolator draws into it. */
GfxTarget *pcgfx_gl_aux_target(GfxTarget *t, int w, int h, int s);
void pcgfx_gl_aux_free(GfxTarget *t);
void pcgfx_gl_finish(void);   /* glFinish: for timing the GPU's part (RUSH_GFX_TIMES=2) */
/* A fence after the GL work so far; whether the GPU hasn't passed the last one within 2 ms (0 without fences). */
void pcgfx_gl_fence_set(void);
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
int pcgfx_gl_target_scale(const GfxTarget *t);   /* NULL: the scale a new target gets */
void pcgfx_gl_target_shown(const GfxTarget *t, int *w, int *rows);   /* N64 pixels the present shows */
void pcgfx_gl_target_dims(const GfxTarget *t, int *w, int *h);       /* N64 pixels */

/* gfx_png.c */
int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);

/* gfx_interp.c: frame interpolation (GFX.md "High frame rate"). While pcgfx_rec is set, a graphics task is being
 * recorded: the interpreter reports its vertex loads, triangles, rectangles and clears, so that the frame can be
 * drawn again later with its transforms moved between the previous frame's and its own. */
extern void *pcgfx_rec;
void pcgfx_rec_task_begin(void);
void pcgfx_rec_task_end(void);
void pcgfx_rec_load(const uint8_t *p, int be, int v0, int n);   /* after load_vertices filled pcgfx.vtx */
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
void pcgfx_sdl_shutdown(void);

/* Settings (gfx_api.c). The internal scale multiplies the N64 frame size. Widescreen widens the frame to a display
 * aspect (GFX.md "Widescreen"). The auto values follow the window and are taken again at each present. */
typedef struct {
    int internal_scale;     /* 1 = the N64 frame size; 0 = auto: the largest whole multiple the window shows */
    int scale;              /* the scale in effect, taken at each color image */
    float widescreen;       /* display aspect to widen the 4:3 frame to: 0 off, < 0 the window's */
    float aspect;           /* the aspect in effect (the window's for < 0), taken at each color image; 0 = 4:3 */
    int verbose;
    int frame_height;       /* rows shown; 0 = the largest scissor bottom drawn with */
    int trace_task;         /* RUSH_GFX_TRACE: print every command of this graphics task (1 = the first); 0 none */
    int frame_rate;         /* RUSH_GFX_FRAME_RATE: 0 the game's own; > 0 frames shown a second; -1 the display's */
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
/* The internal scale at run time: 1-8, or 0 for auto. It applies from the next color image. */
void pc_gfx_set_scale(int scale);
/* The size the auto settings follow when there is no window (headless tests and tools). */
void pc_gfx_set_window_size(int w, int h);

#ifdef __cplusplus
}
#endif

#endif
