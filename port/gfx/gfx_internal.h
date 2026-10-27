/* port/gfx/gfx_internal.h: state shared by the interpreter (gfx_rsp.c, gfx_rdp.c), the combiner compiler
 * (gfx_combine.c) and the OpenGL backend (gfx_gl.c). docs/port/GFX.md describes the design. */
#ifndef GFX_INTERNAL_H
#define GFX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "gfx_gbi.h"

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
    uint8_t clip;       /* outcodes: 1 x<-w, 2 x>w, 4 y<-w, 8 y>w */
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
    GfxTile tiles[8];
    uint8_t tmem[4096];          /* RDP byte order (big-endian texels) */
    uint8_t tmem_le16[512];      /* per 64-bit word: loaded raw from native little-endian memory as 16b data */
    uint32_t tmem_gen;
    int state_dirty;             /* draw state must be rebuilt */
    int tex_dirty;               /* textures must be looked up again */

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

/* gfx_rdp.c */
void pcgfx_rdp_command(uint32_t op, uint32_t w0, uintptr_t w1);
void pcgfx_texrect(uint32_t w0, uint32_t w1, uint32_t half1, uint32_t half2, int flip);
const GfxDrawState *pcgfx_draw_state(int rect_tile);   /* -1 triangles, 0-7 texrect tile, 8 untextured rect */
/* Texture coordinate transform for tile (tile-relative, normalized for the bound texture). */
void pcgfx_tile_uv(int unit, float s_texel, float t_texel, float *u, float *v);
void pcgfx_emit(const GfxOutVtx *v, int n);          /* triangles, 3 vertices each */
void pcgfx_flush(void);

/* gfx_combine.c */
char *pcgfx_combine_glsl(const GfxShaderKey *key, int gles, int glsl_version);   /* malloc'd fragment source */
const char *pcgfx_vertex_source(int gles, int glsl_version);
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
int pcgfx_gl_read_target(GfxTarget *t, uint8_t *rgba, int *w, int *h);   /* top row first, RGBA8 */
int pcgfx_gl_read_presented(uint8_t *rgba, int *w, int *h);
int pcgfx_gl_read_window(uint8_t *rgba, int w, int h);   /* the default framebuffer, top row first */
void pcgfx_gl_target_size(int *scale);

/* gfx_png.c */
int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h);

/* Context backends: gfx_egl.c (headless), gfx_sdl.c (window, when built with SDL2) */
int pcgfx_egl_init(int want_gles, GfxGetProc *get_proc, int *is_gles);
void pcgfx_egl_shutdown(void);
int pcgfx_sdl_init(int w, int h, GfxGetProc *get_proc, int *is_gles);
void pcgfx_sdl_swap(void);
int pcgfx_sdl_poll(void);
void pcgfx_sdl_size(int *w, int *h);
void pcgfx_sdl_shutdown(void);

/* Settings (gfx_api.c). The internal scale multiplies the N64 frame size; widescreen is a hook for later. */
typedef struct {
    int internal_scale;     /* 1 = the N64 frame size */
    int widescreen;         /* hook, not implemented yet */
    int verbose;
    int frame_height;       /* rows shown; 0 = the largest scissor bottom drawn with */
} GfxConfig;
extern GfxConfig pcgfx_cfg;

/* Test and tool access (not part of pc_port.h). */
int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);   /* what the window shows (window mode only) */

#ifdef __cplusplus
}
#endif

#endif
