/* port/gfx/gfx_gl.c: the OpenGL backend (GLES 3.0 or desktop GL 3.1+ core). Render targets per color image,
 * depth buffers per z image, the shader cache, texture upload, batched draws, presentation and readback.
 * All GL calls live here, through a loader, so one build serves GLES and desktop GL. */
#ifdef _WIN32
/* mingw-w64 ships desktop GL's glcorearb.h rather than GLES3/gl3.h: the same function, type and enum names, and
 * no prototypes unless asked for (docs/port/WINDOWS.md). */
#include <GL/glcorearb.h>
#define GL_APIENTRYP APIENTRYP
#else
#define GL_GLES_PROTOTYPES 0
#include <GLES3/gl3.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
typedef void(GL_APIENTRYP PFNGLBINDFRAGDATALOCATIONPROC)(GLuint program, GLuint color, const GLchar *name);

#define GL_FUNCS(X) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLBINDTEXTUREPROC, glBindTexture) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLCLEARBUFFERFVPROC, glClearBufferfv) \
    X(PFNGLCOLORMASKPROC, glColorMask) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLDELETETEXTURESPROC, glDeleteTextures) \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
    X(PFNGLDEPTHFUNCPROC, glDepthFunc) \
    X(PFNGLDEPTHMASKPROC, glDepthMask) \
    X(PFNGLDISABLEPROC, glDisable) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays) \
    X(PFNGLDRAWBUFFERSPROC, glDrawBuffers) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLFINISHPROC, glFinish) \
    X(PFNGLFLUSHPROC, glFlush) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLPIXELSTOREIPROC, glPixelStorei) \
    X(PFNGLPOLYGONOFFSETPROC, glPolygonOffset) \
    X(PFNGLREADBUFFERPROC, glReadBuffer) \
    X(PFNGLREADPIXELSPROC, glReadPixels) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLSCISSORPROC, glScissor) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
    X(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM2FVPROC, glUniform2fv) \
    X(PFNGLUNIFORM4IPROC, glUniform4i) \
    X(PFNGLUNIFORM4FVPROC, glUniform4fv) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLVIEWPORTPROC, glViewport)

#define DECL(type, name) static type name;
GL_FUNCS(DECL)
#undef DECL
static PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation;
/* Fences (GLES 3.0, GL 3.2, ARB_sync), optional: the frame interpolator's check that the GPU keeps up. */
static PFNGLFENCESYNCPROC glFenceSync;
static PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
static PFNGLDELETESYNCPROC glDeleteSync;
static GLsync g_fence;
/* GPU timers (RUSH_GFX_TIMES=3), optional: GL_TIME_ELAPSED queries (desktop GL 3.3, ARB_timer_query,
 * EXT_disjoint_timer_query on GLES), read back when they are ready. */
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
typedef void(GL_APIENTRYP PcGlGenQueries)(GLsizei n, GLuint *ids);
typedef void(GL_APIENTRYP PcGlDeleteQueries)(GLsizei n, const GLuint *ids);
typedef void(GL_APIENTRYP PcGlBeginQuery)(GLenum target, GLuint id);
typedef void(GL_APIENTRYP PcGlEndQuery)(GLenum target);
typedef void(GL_APIENTRYP PcGlGetQueryObjectuiv)(GLuint id, GLenum pname, GLuint *params);
typedef void(GL_APIENTRYP PcGlGetQueryObjectui64v)(GLuint id, GLenum pname, uint64_t *params);
#define GPU_TIMERS 256
static struct {
    PcGlGenQueries gen;
    PcGlDeleteQueries del;
    PcGlBeginQuery begin;
    PcGlEndQuery end;
    PcGlGetQueryObjectuiv get;
    PcGlGetQueryObjectui64v get64;
    GLuint q[GPU_TIMERS];
    uint8_t cat[GPU_TIMERS];
    uint32_t head, tail;          /* pending queries: tail .. head - 1 (mod GPU_TIMERS) */
    int active, ok;
    double ms[PCGFX_GPU_TIMER_CATS];
    uint32_t n[PCGFX_GPU_TIMER_CATS];
} GT;

/* A program's uniform values as last uploaded: a uniform is uploaded again only when it changes (they are the
 * program's state, kept across glUseProgram). */
typedef struct {
    int set;
    float prim[4], env[4], fog[4], blendc[4], fill[4];
    float prim_lod, lod_frac, noise, scale[2];
    int target[4];
    float lod_a[GFX_LOD_MAX][4], lod_b[GFX_LOD_MAX][2];
} GfxUniforms;

struct GfxShader {
    GfxShaderKey key;
    GLuint prog;
    GLint u_prim, u_env, u_fog, u_blend, u_fill, u_primlod, u_lodfrac, u_noise, u_scale, u_target, u_loda, u_lodb;
    GfxUniforms last;
    GfxShader *next;
};

typedef struct GfxDepth {
    const void *addr;
    int w, h;           /* N64 pixels */
    int pw, ph;         /* internal pixels */
    GLuint rb;
    struct GfxDepth *next;
} GfxDepth;

struct GfxTarget {
    const void *addr;
    int w, h, vis_h;    /* N64 pixels; vis_h: rows shown (the largest scissor bottom drawn with) */
    float s;            /* the internal scale it was made at */
    int pw, ph;         /* its size in internal pixels: pcgfx_scaled(w, s) x pcgfx_scaled(h, s) */
    GLuint fbo, color;
    GfxDepth *depth;
    GfxTarget *next;
};

#define SHADER_BUCKETS 256

static struct {
    int gles, glsl;
    GLuint vao, vbo, white;
    GfxShader *shaders[SHADER_BUCKETS];
    GfxTarget *targets;
    GfxTarget *last;
    GfxDepth *depths;
    GLuint present_fbo, present_tex;
    int present_w, present_h, present_valid;
    int present_fw, present_rows;   /* the presented frame in N64 pixels */
    /* The presented image is the rows from present_sy up of present_src's color, until that target is drawn into or
     * remade; then (present_copy) it is copied to present_tex first. The game draws into another frame buffer next,
     * so the copy is seldom needed. NULL: present_tex holds it. */
    GfxTarget *present_src;
    int present_sy, present_tex_w, present_tex_h;
    GLuint window_fbo, window_tex;  /* headless: the window, offscreen */
    int window_w, window_h;
    GLuint ui_prog, ui_tex;         /* the overlay (gfx_ui.c) */
    GLint u_ui_tex;
    int ui_failed;
    GLuint bound_fbo;
    int max_size;                   /* the largest render target side (GL_MAX_TEXTURE_SIZE, _RENDERBUFFER_SIZE) */
    uint32_t draws;                 /* pcgfx_gl_draw calls and their vertices so far (RUSH_GFX_TIMES) */
    uint64_t verts;
    size_t vbo_size, vbo_used;      /* the vertex buffer: its size, and the bytes written since it was orphaned */
} G;

/* ---------------------------------------------------------------- GL state as last set */

/* The draws set their whole state each time (gfx_rdp.c batches by draw state); calls that would set what is set
 * already are skipped. The dc_* functions set state through this record; everything else here that changes state
 * it keeps calls dc_forget() first, so what it says is always what GL has. */
#define DC_UNITS GFX_LOD_MAX
static struct {
    int valid;
    GLuint prog;
    int unit;                         /* the active texture unit */
    GLuint tex[DC_UNITS];             /* bound to each unit */
    int8_t scissor_test, depth_test, blend, poly_offset, cull, depth_mask, alpha_mask, blend_func;
    GLenum depth_func;
    int viewport[4], scissor[4];
} DC;

/* Texture parameters by GL name, as last set (filter and wraps packed; 0 unknown): textures made by
 * pcgfx_gl_texture are known from their creation on, and forgotten when they are deleted. */
static uint8_t *s_tex_params;
static uint32_t s_tex_params_n;
#define TEX_PARAMS_MAX (1u << 20)

static void present_copy(const GfxTarget *t);

static void dc_forget(void) {
    DC.valid = 0;
}

static void dc_check(void) {
    if (!DC.valid) {
        memset(&DC, 0, sizeof(DC));
        DC.valid = 1;
        DC.prog = (GLuint)-1;
        DC.unit = -1;
        memset(DC.tex, 0xFF, sizeof(DC.tex));
        DC.scissor_test = DC.depth_test = DC.blend = DC.poly_offset = DC.cull = DC.depth_mask = DC.alpha_mask = -1;
        DC.blend_func = -1;
        DC.depth_func = 0;
        DC.viewport[2] = DC.scissor[2] = -1;
    }
}

static void dc_enable(GLenum cap, int8_t *cur, int on) {
    if (*cur != on) {
        if (on)
            glEnable(cap);
        else
            glDisable(cap);
        *cur = (int8_t)on;
    }
}

static void dc_viewport(int x, int y, int w, int h) {
    if (DC.viewport[0] != x || DC.viewport[1] != y || DC.viewport[2] != w || DC.viewport[3] != h) {
        glViewport(x, y, w, h);
        DC.viewport[0] = x, DC.viewport[1] = y, DC.viewport[2] = w, DC.viewport[3] = h;
    }
}

static void dc_scissor(int x, int y, int w, int h) {
    if (DC.scissor[0] != x || DC.scissor[1] != y || DC.scissor[2] != w || DC.scissor[3] != h) {
        glScissor(x, y, w, h);
        DC.scissor[0] = x, DC.scissor[1] = y, DC.scissor[2] = w, DC.scissor[3] = h;
    }
}

static void dc_depth_mask(int on) {
    if (DC.depth_mask != on) {
        glDepthMask(on ? GL_TRUE : GL_FALSE);
        DC.depth_mask = (int8_t)on;
    }
}

/* red, green and blue are always written; alpha as asked */
static void dc_color_mask(int alpha) {
    if (DC.alpha_mask != alpha) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, alpha ? GL_TRUE : GL_FALSE);
        DC.alpha_mask = (int8_t)alpha;
    }
}

static void dc_unit(int unit) {
    if (DC.unit != unit) {
        glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
        DC.unit = unit;
    }
}

static void dc_texture(int unit, GLuint tex) {
    if (DC.tex[unit] == tex)
        return;
    dc_unit(unit);
    glBindTexture(GL_TEXTURE_2D, tex);
    DC.tex[unit] = tex;
}

static uint8_t tex_param_bits(int linear, int repeat_s, int repeat_t) {
    return (uint8_t)(0x80 | (linear ? 1 : 0) | (repeat_s ? 2 : 0) | (repeat_t ? 4 : 0));
}

static void tex_params_note(GLuint tex, uint8_t bits) {
    if (tex >= TEX_PARAMS_MAX)
        return;
    if (tex >= s_tex_params_n) {
        uint32_t n = s_tex_params_n ? s_tex_params_n : 1024;
        uint8_t *p;
        while (n <= tex)
            n *= 2;
        p = (uint8_t *)realloc(s_tex_params, n);
        if (p == NULL)
            return;
        memset(p + s_tex_params_n, 0, n - s_tex_params_n);
        s_tex_params = p;
        s_tex_params_n = n;
    }
    s_tex_params[tex] = bits;
}

/* Unit's bound texture (dc_texture) gets these parameters, unless it has them already. */
static void dc_tex_params(int unit, GLuint tex, int linear, int repeat_s, int repeat_t) {
    uint8_t bits = tex_param_bits(linear, repeat_s, repeat_t);
    GLint f = linear ? GL_LINEAR : GL_NEAREST;
    if (tex < s_tex_params_n && s_tex_params[tex] == bits)
        return;
    dc_unit(unit);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    tex_params_note(tex, bits);
}

/* The vertex buffer as a stream: each draw's vertices go after the last one's, and the buffer is orphaned (new
 * storage, the old one left to the draws that read it) only when it is full, so the driver neither waits for the
 * GPU nor makes a buffer per draw. Returns the first vertex's index for glDrawArrays. */
#define VBO_SIZE ((size_t)4 << 20)

static GLint stream_vertices(const GfxOutVtx *v, int n) {
    size_t bytes = sizeof(*v) * (size_t)n;
    GLint first;
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    if (G.vbo_used + bytes > G.vbo_size) {
        size_t size = bytes > VBO_SIZE ? bytes : VBO_SIZE;
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, NULL, GL_STREAM_DRAW);
        G.vbo_size = size;
        G.vbo_used = 0;
    }
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)G.vbo_used, (GLsizeiptr)bytes, v);
    first = (GLint)(G.vbo_used / sizeof(*v));
    G.vbo_used += bytes;
    return first;
}

/* The scale new targets get: the setting, or the window's for auto (gfx_api.c). A target keeps its own until it
 * is made again, so a change applies from the next color image. */
static float scale(void) {
    return pcgfx_cfg.scale > 0.0f ? pcgfx_cfg.scale : 1.0f;
}

float pcgfx_gl_target_scale(const GfxTarget *t) {
    return t != NULL ? t->s : scale();
}

void pcgfx_gl_target_px(const GfxTarget *t, int *pw, int *ph, float *sx, float *sy) {
    *pw = t->pw;
    *ph = t->ph;
    *sx = (float)t->pw / (float)t->w;
    *sy = (float)t->ph / (float)t->h;
}

/* A scale that keeps w x h within the GPU's largest render target. */
static float fit_scale(int w, int h, float s) {
    if (G.max_size > 0 && (pcgfx_scaled(w, s) > G.max_size || pcgfx_scaled(h, s) > G.max_size)) {
        float a = (float)G.max_size / (float)w, b = (float)G.max_size / (float)h;
        s = a < b ? a : b;
        s = s < 1.0f ? 1.0f : floorf(s * 64.0f) / 64.0f;
    }
    return s;
}

static int target_rows(const GfxTarget *t);

void pcgfx_gl_target_shown(const GfxTarget *t, int *w, int *rows) {
    *w = t->w;
    *rows = target_rows(t);
}

void pcgfx_gl_target_dims(const GfxTarget *t, int *w, int *h) {
    *w = t->w;
    *h = t->h;
}

/* ---------------------------------------------------------------- init */

static char s_driver[4][160];   /* GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION, as logged */

void pc_gfx_driver(const char **vendor, const char **renderer, const char **version, const char **glsl) {
    *vendor = s_driver[0];
    *renderer = s_driver[1];
    *version = s_driver[2];
    *glsl = s_driver[3];
}

static void keep_driver(int i, const char *s) {
    snprintf(s_driver[i], sizeof(s_driver[i]), "%s", s != NULL ? s : "?");
}

int pcgfx_gl_init(GfxGetProc get_proc, int gles) {
    const char *ver;
    int ok = 1;
    memset(&G, 0, sizeof(G));
    dc_forget();
#define LOAD(type, name) \
    name = (type)get_proc(#name); \
    if (name == NULL) { \
        fprintf(stderr, "gfx: missing GL function %s\n", #name); \
        ok = 0; \
    }
    GL_FUNCS(LOAD)
#undef LOAD
    if (!ok)
        return -1;
    glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC)get_proc("glBindFragDataLocation");
    glFenceSync = (PFNGLFENCESYNCPROC)get_proc("glFenceSync");
    glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)get_proc("glClientWaitSync");
    glDeleteSync = (PFNGLDELETESYNCPROC)get_proc("glDeleteSync");
    if (glFenceSync == NULL || glClientWaitSync == NULL || glDeleteSync == NULL)
        glFenceSync = NULL;
    G.gles = gles;
    G.glsl = 300;
    ver = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (!gles && ver != NULL) {
        int maj = 0, min = 0;
        if (sscanf(ver, "%d.%d", &maj, &min) == 2)
            G.glsl = maj * 100 + min;
    }
    /* always: which GPU and driver drew a run is the first question about a log (two-GPU laptops) */
    fprintf(stderr, "gfx: %s / %s / %s / %s\n", (const char *)glGetString(GL_VENDOR),
            (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), ver ? ver : "?");
    keep_driver(0, (const char *)glGetString(GL_VENDOR));   /* the same for the overlay's About page */
    keep_driver(1, (const char *)glGetString(GL_RENDERER));
    keep_driver(2, (const char *)glGetString(GL_VERSION));
    keep_driver(3, ver);
    glGenVertexArrays(1, &G.vao);
    glBindVertexArray(G.vao);
    glGenBuffers(1, &G.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GfxOutVtx), (void *)offsetof(GfxOutVtx, pos));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GfxOutVtx), (void *)offsetof(GfxOutVtx, shade));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GfxOutVtx), (void *)offsetof(GfxOutVtx, uv));
    {
        static const uint8_t white[4] = {255, 255, 255, 255};
        G.white = pcgfx_gl_texture(white, 1, 1);
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    {
        GLint tex = 0, rb = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &tex);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &rb);
        G.max_size = tex < rb ? tex : rb;
        while (glGetError() != GL_NO_ERROR)
            ;
    }
    return 0;
}

void pcgfx_gl_shutdown(void) {
    int b;
    if (GT.ok) {
        if (GT.active)
            GT.end(GL_TIME_ELAPSED);
        GT.del(GPU_TIMERS, GT.q);
    }
    memset(&GT, 0, sizeof(GT));
    if (g_fence != NULL)
        glDeleteSync(g_fence);
    g_fence = NULL;
    for (b = 0; b < SHADER_BUCKETS; b++)
        while (G.shaders[b]) {
            GfxShader *s = G.shaders[b];
            G.shaders[b] = s->next;
            glDeleteProgram(s->prog);
            free(s);
        }
    while (G.targets) {
        GfxTarget *t = G.targets;
        G.targets = t->next;
        glDeleteFramebuffers(1, &t->fbo);
        glDeleteTextures(1, &t->color);
        free(t);
    }
    while (G.depths) {
        GfxDepth *d = G.depths;
        G.depths = d->next;
        glDeleteRenderbuffers(1, &d->rb);
        free(d);
    }
    if (G.present_fbo) {
        glDeleteFramebuffers(1, &G.present_fbo);
        glDeleteTextures(1, &G.present_tex);
    }
    if (G.window_fbo) {
        glDeleteFramebuffers(1, &G.window_fbo);
        glDeleteTextures(1, &G.window_tex);
    }
    if (G.ui_prog)
        glDeleteProgram(G.ui_prog);
    if (G.ui_tex)
        glDeleteTextures(1, &G.ui_tex);
    glDeleteTextures(1, &G.white);
    glDeleteBuffers(1, &G.vbo);
    glDeleteVertexArrays(1, &G.vao);
    memset(&G, 0, sizeof(G));
    free(s_tex_params);
    s_tex_params = NULL;
    s_tex_params_n = 0;
    dc_forget();
}

static void bind_fbo(GLuint fbo) {
    if (G.bound_fbo != fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        G.bound_fbo = fbo;
    }
}

/* ---------------------------------------------------------------- targets */

GfxTarget *pcgfx_gl_find_target(const void *addr) {
    GfxTarget *t;
    for (t = G.targets; t != NULL; t = t->next)
        if (t->addr == addr)
            return t;
    return NULL;
}

GfxTarget *pcgfx_gl_last_target(void) {
    return G.last;
}

/* After a target changes size (widescreen or the scale follows the window), the depth buffers of a size no target
 * has any more are dropped. A target's depth always has the target's size, so none is still attached. */
static void drop_unused_depths(void) {
    GfxDepth **pd = &G.depths;
    while (*pd != NULL) {
        GfxDepth *d = *pd;
        const GfxTarget *t;
        for (t = G.targets; t != NULL; t = t->next)
            if (t->w == d->w && t->h == d->h && t->pw == d->pw && t->ph == d->ph)
                break;
        if (t != NULL) {
            pd = &d->next;
            continue;
        }
        *pd = d->next;
        glDeleteRenderbuffers(1, &d->rb);
        free(d);
    }
}

GfxTarget *pcgfx_gl_target(const void *addr, int w, int h) {
    GfxTarget *t = pcgfx_gl_find_target(addr);
    float s = fit_scale(w, h, scale());
    int resized = t != NULL;
    static const float black[4] = {0, 0, 0, 1};
    if (t != NULL && t->w == w && t->h == h && t->s == s) {
        G.last = t;
        return t;
    }
    if (t == NULL) {
        t = (GfxTarget *)calloc(1, sizeof(*t));
        t->addr = addr;
        t->next = G.targets;
        G.targets = t;
        glGenFramebuffers(1, &t->fbo);
        glGenTextures(1, &t->color);
    }
    present_copy(t);   /* made again: the presented image keeps its pixels */
    t->w = w;
    t->h = h;
    t->s = s;
    t->pw = pcgfx_scaled(w, s);
    t->ph = pcgfx_scaled(h, s);
    t->depth = NULL;
    dc_forget();
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t->pw, t->ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    bind_fbo(t->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearBufferfv(GL_COLOR, 0, black);
    G.last = t;
    if (resized)
        drop_unused_depths();
    return t;
}

void pcgfx_gl_attach_depth(GfxTarget *t, const void *zaddr) {
    GfxDepth *d;
    if (t == NULL)
        return;
    for (d = G.depths; d != NULL; d = d->next)
        if (d->addr == zaddr && d->w == t->w && d->h == t->h && d->pw == t->pw && d->ph == t->ph)
            break;
    if (d == NULL) {
        float one = 1.0f;
        d = (GfxDepth *)calloc(1, sizeof(*d));
        d->addr = zaddr;
        d->w = t->w;
        d->h = t->h;
        d->pw = t->pw;
        d->ph = t->ph;
        glGenRenderbuffers(1, &d->rb);
        glBindRenderbuffer(GL_RENDERBUFFER, d->rb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t->pw, t->ph);
        d->next = G.depths;
        G.depths = d;
        t->depth = NULL;
        bind_fbo(t->fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, d->rb);
        dc_check();
        dc_enable(GL_SCISSOR_TEST, &DC.scissor_test, 0);
        dc_depth_mask(1);
        glClearBufferfv(GL_DEPTH, 0, &one);
        t->depth = d;
    }
    if (t->depth != d) {
        bind_fbo(t->fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, d->rb);
        t->depth = d;
    }
}

/* N64 pixel edges to internal pixels by pcgfx_edge, the rule GL covers rectangles by (rows in GL's y-up window
 * coordinates, where its tie rule works), so a scissor or a clear and the rectangles beside it meet without a gap
 * or an overlap at any scale (GFX.md "Resolution and scale"). */
static void set_scissor(const GfxTarget *t, const int32_t r[4]) {
    int x0 = r[0] < 0 ? 0 : r[0], y0 = r[1] < 0 ? 0 : r[1];
    int x1 = r[2] > t->w ? t->w : r[2], y1 = r[3] > t->h ? t->h : r[3];
    int px0, px1, py0, py1;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    px0 = pcgfx_edge(x0, t->w, t->pw);
    px1 = pcgfx_edge(x1, t->w, t->pw);
    py0 = pcgfx_edge(t->h - y1, t->h, t->ph);   /* GL rows, from the bottom */
    py1 = pcgfx_edge(t->h - y0, t->h, t->ph);
    dc_enable(GL_SCISSOR_TEST, &DC.scissor_test, 1);
    dc_scissor(px0, py0, px1 - px0, py1 - py0);
}

static void note_rows(GfxTarget *t, int32_t bottom) {
    if (bottom > t->h)
        bottom = t->h;
    if (bottom > t->vis_h)
        t->vis_h = bottom;
}

void pcgfx_gl_clear(GfxTarget *t, const int32_t rect[4], int depth, const float rgba[4], float z) {
    if (t == NULL)
        return;
    bind_fbo(t->fbo);
    dc_check();
    dc_viewport(0, 0, t->pw, t->ph);
    set_scissor(t, rect);
    if (depth) {
        if (t->depth == NULL)
            return;
        dc_depth_mask(1);
        glClearBufferfv(GL_DEPTH, 0, &z);
    } else {
        float c[4];
        if (t == G.present_src) {
            present_copy(t);
            bind_fbo(t->fbo);
            dc_check();
            dc_viewport(0, 0, t->pw, t->ph);
            set_scissor(t, rect);
        }
        memcpy(c, rgba, sizeof(c));
        c[3] = 1.0f;
        dc_color_mask(1);
        glClearBufferfv(GL_COLOR, 0, c);
        note_rows(t, rect[3]);
    }
}

/* ---------------------------------------------------------------- textures */

uint32_t pcgfx_gl_texture(const uint8_t *rgba, int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    if (DC.valid && DC.unit >= 0 && DC.unit < DC_UNITS)   /* bound to the active unit from here on */
        DC.tex[DC.unit] = tex;
    else
        dc_forget();
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex_params_note(tex, tex_param_bits(0, 0, 0));
    return tex;
}

void pcgfx_gl_delete_texture(uint32_t tex) {
    GLuint t = tex;
    int u;
    if (t == 0)
        return;
    glDeleteTextures(1, &t);
    if (t < s_tex_params_n)
        s_tex_params[t] = 0;
    for (u = 0; u < DC_UNITS; u++)   /* a deleted texture is unbound: its name may come back as another's */
        if (DC.tex[u] == t)
            DC.tex[u] = (GLuint)-1;
}

/* ---------------------------------------------------------------- shaders */

static GLuint compile(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "gfx: shader compile failed:\n%s\n--- source ---\n%s\n", log, src);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static unsigned key_hash(const GfxShaderKey *k) {
    const uint8_t *p = (const uint8_t *)k;
    unsigned h = 2166136261u, i;
    for (i = 0; i < sizeof(*k); i++)
        h = (h ^ p[i]) * 16777619u;
    return h % SHADER_BUCKETS;
}

GfxShader *pcgfx_gl_shader(const GfxShaderKey *key) {
    unsigned b = key_hash(key);
    GfxShader *s;
    GLuint vs, fs, prog;
    GLint ok = 0;
    char *fsrc;
    for (s = G.shaders[b]; s != NULL; s = s->next)
        if (memcmp(&s->key, key, sizeof(*key)) == 0)
            return s;
    fsrc = pcgfx_combine_glsl(key, G.gles, G.glsl);
    if (pcgfx_cfg.verbose > 1)
        fprintf(stderr, "gfx: new shader\n%s\n", fsrc);
    vs = compile(GL_VERTEX_SHADER, pcgfx_vertex_source(G.gles, G.glsl));
    fs = compile(GL_FRAGMENT_SHADER, fsrc);
    free(fsrc);
    prog = glCreateProgram();
    if (vs)
        glAttachShader(prog, vs);
    if (fs)
        glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aShade");
    glBindAttribLocation(prog, 2, "aUV");
    if (!G.gles && glBindFragDataLocation != NULL)
        glBindFragDataLocation(prog, 0, "fragColor");
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (vs)
        glDeleteShader(vs);
    if (fs)
        glDeleteShader(fs);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "gfx: shader link failed: %s\n", log);
    }
    s = (GfxShader *)calloc(1, sizeof(*s));
    s->key = *key;
    s->prog = prog;
    s->u_prim = glGetUniformLocation(prog, "uPrim");
    s->u_env = glGetUniformLocation(prog, "uEnv");
    s->u_fog = glGetUniformLocation(prog, "uFog");
    s->u_blend = glGetUniformLocation(prog, "uBlend");
    s->u_fill = glGetUniformLocation(prog, "uFill");
    s->u_primlod = glGetUniformLocation(prog, "uPrimLod");
    s->u_lodfrac = glGetUniformLocation(prog, "uLodFrac");
    s->u_noise = glGetUniformLocation(prog, "uNoise");
    s->u_scale = glGetUniformLocation(prog, "uScale");
    s->u_target = glGetUniformLocation(prog, "uTarget");
    dc_forget();
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uTex0"), 0);
    glUniform1i(glGetUniformLocation(prog, "uTex1"), 1);
    if (key->lod_levels) {   /* texture LOD: a sampler per mip level (gfx_combine.c) */
        int k;
        for (k = 2; k < key->lod_levels; k++) {
            char name[8];
            snprintf(name, sizeof(name), "uTex%d", k);
            glUniform1i(glGetUniformLocation(prog, name), k);
        }
        s->u_loda = glGetUniformLocation(prog, "uLodA");
        s->u_lodb = glGetUniformLocation(prog, "uLodB");
    } else {
        s->u_loda = s->u_lodb = -1;
    }
    s->next = G.shaders[b];
    G.shaders[b] = s;
    return s;
}

/* ---------------------------------------------------------------- draw */

static void uniform4(GLint loc, float *last, const float *v, int force) {
    if (loc >= 0 && (force || memcmp(last, v, sizeof(float) * 4) != 0)) {
        glUniform4fv(loc, 1, v);
        memcpy(last, v, sizeof(float) * 4);
    }
}

static void uniform1(GLint loc, float *last, float v, int force) {
    if (loc >= 0 && (force || memcmp(last, &v, sizeof(v)) != 0)) {
        glUniform1f(loc, v);
        *last = v;
    }
}

void pcgfx_gl_draw(const GfxDrawState *st, const GfxOutVtx *v, int n) {
    GfxTarget *t = st->target;
    GfxShader *sh = st->shader;
    GfxUniforms *U;
    int u, force;
    float sc[2];
    if (t == NULL || sh == NULL || n <= 0)
        return;
    if (t == G.present_src)
        present_copy(t);
    bind_fbo(t->fbo);
    dc_check();
    dc_viewport(0, 0, t->pw, t->ph);
    if (DC.prog != sh->prog) {
        glUseProgram(sh->prog);
        DC.prog = sh->prog;
    }
    U = &sh->last;
    force = !U->set;
    U->set = 1;
    uniform4(sh->u_prim, U->prim, st->prim, force);
    uniform4(sh->u_env, U->env, st->env, force);
    uniform4(sh->u_fog, U->fog, st->fog, force);
    uniform4(sh->u_blend, U->blendc, st->blendc, force);
    uniform4(sh->u_fill, U->fill, st->fill, force);
    uniform1(sh->u_primlod, &U->prim_lod, st->prim_lod, force);
    uniform1(sh->u_lodfrac, &U->lod_frac, st->lod_frac, force);
    uniform1(sh->u_noise, &U->noise, st->noise_seed, force);
    /* N64 pixels to internal ones, per axis (the LOD's steps, point-sampled rectangles: gfx_combine.c) */
    sc[0] = (float)t->pw / (float)t->w;
    sc[1] = (float)t->ph / (float)t->h;
    if (sh->u_scale >= 0 && (force || memcmp(U->scale, sc, sizeof(sc)) != 0)) {
        glUniform2f(sh->u_scale, sc[0], sc[1]);
        memcpy(U->scale, sc, sizeof(sc));
    }
    if (sh->u_target >= 0 && (force || U->target[0] != t->w || U->target[1] != t->h || U->target[2] != t->pw ||
                              U->target[3] != t->ph)) {
        glUniform4i(sh->u_target, t->w, t->h, t->pw, t->ph);
        U->target[0] = t->w, U->target[1] = t->h, U->target[2] = t->pw, U->target[3] = t->ph;
    }
    if (st->lod_levels) {
        if (sh->u_loda >= 0 && (force || memcmp(U->lod_a, st->lod_a, sizeof(U->lod_a)) != 0)) {
            glUniform4fv(sh->u_loda, GFX_LOD_MAX, &st->lod_a[0][0]);
            memcpy(U->lod_a, st->lod_a, sizeof(U->lod_a));
        }
        if (sh->u_lodb >= 0 && (force || memcmp(U->lod_b, st->lod_b, sizeof(U->lod_b)) != 0)) {
            glUniform2fv(sh->u_lodb, GFX_LOD_MAX, &st->lod_b[0][0]);
            memcpy(U->lod_b, st->lod_b, sizeof(U->lod_b));
        }
    }
    for (u = 0; u < (st->lod_levels > 2 ? st->lod_levels : 2); u++) {
        if (st->tex[u]) {
            dc_texture(u, st->tex[u]);
            dc_tex_params(u, st->tex[u], st->linear[u], st->repeat_s[u], st->repeat_t[u]);
        } else {
            dc_texture(u, G.white);
        }
    }
    if (st->depth_test && t->depth != NULL) {
        GLenum f = st->depth_test == 2 ? GL_ALWAYS : GL_LEQUAL;
        dc_enable(GL_DEPTH_TEST, &DC.depth_test, 1);
        if (DC.depth_func != f) {
            glDepthFunc(f);
            DC.depth_func = f;
        }
        dc_depth_mask(st->depth_write != 0);
    } else {
        dc_enable(GL_DEPTH_TEST, &DC.depth_test, 0);
        dc_depth_mask(0);
    }
    if (st->decal && DC.poly_offset != 1) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -2.0f);
        DC.poly_offset = 1;
    } else if (!st->decal) {
        dc_enable(GL_POLYGON_OFFSET_FILL, &DC.poly_offset, 0);
    }
    if (st->blend) {
        /* rgb: what the shader computed without memory; alpha: memory's weight (gfx_combine.c) */
        dc_enable(GL_BLEND, &DC.blend, 1);
        if (DC.blend_func != 1) {
            glBlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_ONE);
            DC.blend_func = 1;
        }
    } else {
        dc_enable(GL_BLEND, &DC.blend, 0);
    }
    dc_color_mask(0);
    dc_enable(GL_CULL_FACE, &DC.cull, 0);   /* culling happens in gfx_rsp.c, in N64 screen space */
    set_scissor(t, st->scissor);
    note_rows(t, st->scissor[3]);
    glDrawArrays(GL_TRIANGLES, stream_vertices(v, n), n);
    G.draws++;
    G.verts += (uint64_t)n;
}

void pcgfx_gl_draw_counts(uint32_t *draws, uint64_t *verts) {
    *draws = G.draws;
    *verts = G.verts;
}

/* ---------------------------------------------------------------- present and readback */

static int target_rows(const GfxTarget *t) {
    int rows = pcgfx_cfg.frame_height > 0 ? pcgfx_cfg.frame_height : t->vis_h;
    if (rows <= 0 || rows > t->h)
        rows = t->h;
    return rows;
}

void pcgfx_gl_present(GfxTarget *t, int win_w, int win_h, int to_window) {
    if (t == NULL)
        return;
    pcgfx_gl_present_image(t);
    pcgfx_gl_show(win_w, win_h, to_window);
}

void pcgfx_gl_present_image(GfxTarget *t) {
    int rows, ph;
    if (t == NULL)
        return;
    rows = target_rows(t);
    ph = t->ph - pcgfx_edge(t->h - rows, t->h, t->ph);   /* the internal rows of N64 rows 0 .. rows */
    G.present_src = t;
    G.present_sy = t->ph - ph;   /* N64 row 0 is the top: GL rows ph - shown .. ph of the target */
    G.present_w = t->pw;
    G.present_h = ph;
    G.present_valid = 1;
    G.present_fw = t->w;
    G.present_rows = rows;
}

/* The presented image into present_tex, from its target: before that target changes (t, or any: NULL). */
static void present_copy(const GfxTarget *t) {
    GfxTarget *src = G.present_src;
    int pw = G.present_w, ph = G.present_h;
    if (src == NULL || (t != NULL && t != src))
        return;
    G.present_src = NULL;
    dc_forget();
    if (G.present_fbo == 0) {
        glGenFramebuffers(1, &G.present_fbo);
        glGenTextures(1, &G.present_tex);
    }
    if (G.present_tex_w != pw || G.present_tex_h != ph) {
        glBindTexture(GL_TEXTURE_2D, G.present_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        bind_fbo(G.present_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, G.present_tex, 0);
        G.present_tex_w = pw;
        G.present_tex_h = ph;
    }
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, G.present_fbo);
    glBlitFramebuffer(0, G.present_sy, pw, G.present_sy + ph, 0, 0, pw, ph, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    /* Before the target is drawn into or cleared: on Mesa's iris (Intel) a fast clear of the copy's source in the
     * same batch left 8 x 4 pixel blocks of what was there before (gfx tests at 1x, UHD 770, Mesa 25.2). */
    glFlush();
    /* bound for drawing and reading are two others now: the next bind_fbo binds (with a high frame rate nothing may
     * come between this and the next task's depth clear through src: it must not take src's as still bound) */
    G.bound_fbo = (GLuint)-1;
}

/* Clear the window (or the headless window's image) and blit (sx0, sy0)-(sx0 + pw, sy0 + ph) of src into it: a
 * frame fw x rows N64 pixels, with square pixels, placed by pcgfx_fit (gfx_size.c): at a native scale, pixel for
 * pixel when it was drawn for this window, else fitted; otherwise at a whole multiple when one fits, else aspect
 * fit. src 0: none. */
static void show_from(GLuint src, int sx0, int sy0, int pw, int ph, int fw, int rows, int win_w, int win_h,
                      int to_window) {
    static const float black[4] = {0, 0, 0, 1};
    GLuint dst = 0;
    dc_forget();
    if (win_w > 0 && win_h > 0 && !to_window) {
        /* headless: the window is an image of its size, which pc_gfx_read_window reads */
        if (G.window_fbo == 0) {
            glGenFramebuffers(1, &G.window_fbo);
            glGenTextures(1, &G.window_tex);
        }
        if (G.window_w != win_w || G.window_h != win_h) {
            glBindTexture(GL_TEXTURE_2D, G.window_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            bind_fbo(G.window_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, G.window_tex, 0);
            G.window_w = win_w;
            G.window_h = win_h;
        }
        dst = G.window_fbo;
    }
    if (win_w > 0 && win_h > 0) {
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst);
        glViewport(0, 0, win_w, win_h);
        glClearBufferfv(GL_COLOR, 0, black);
    }
    if (src != 0 && fw > 0 && rows > 0 && win_w > 0 && win_h > 0) {
        int r[4], exact = pcgfx_fit(pw, ph, fw, rows, win_w, win_h, pcgfx_cfg.internal_scale < 0.0f, r);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
        glBlitFramebuffer(sx0, sy0, sx0 + pw, sy0 + ph, r[0], r[1], r[0] + r[2], r[1] + r[3], GL_COLOR_BUFFER_BIT,
                          exact ? GL_NEAREST : GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    G.bound_fbo = 0;
}

void pcgfx_gl_show(int win_w, int win_h, int to_window) {
    if (G.present_src != NULL && win_w > 0 && win_h > 0) {
        /* straight from its target when shown pixel for pixel or at a whole multiple (nearest): the same pixels as
         * from a copy; a filtered fit samples the copy, as it always did */
        int r[4];
        if (!pcgfx_fit(G.present_w, G.present_h, G.present_fw, G.present_rows, win_w, win_h,
                       pcgfx_cfg.internal_scale < 0.0f, r))
            present_copy(NULL);
    }
    if (G.present_src != NULL)
        show_from(G.present_src->fbo, 0, G.present_sy, G.present_w, G.present_h, G.present_fw, G.present_rows, win_w,
                  win_h, to_window);
    else
        show_from(G.present_valid ? G.present_fbo : 0, 0, 0, G.present_w, G.present_h, G.present_fw,
                  G.present_rows, win_w, win_h, to_window);
}

void pcgfx_gl_show_target(GfxTarget *t, int rows, int win_w, int win_h, int to_window) {
    int ph;
    if (t == NULL)
        return;
    if (rows <= 0 || rows > t->h)
        rows = t->h;
    /* N64 row 0 is the top: the internal rows of N64 rows 0 .. rows, as the present copies them */
    ph = t->ph - pcgfx_edge(t->h - rows, t->h, t->ph);
    show_from(t->fbo, 0, t->ph - ph, t->pw, ph, t->w, rows, win_w, win_h, to_window);
}

GfxTarget *pcgfx_gl_aux_target(GfxTarget *t, int w, int h, float s) {
    static const float black[4] = {0, 0, 0, 1};
    float one = 1.0f;
    if (t != NULL && t->w == w && t->h == h && t->s == s)
        return t;
    if (t == NULL) {
        t = (GfxTarget *)calloc(1, sizeof(*t));
        if (t == NULL)
            return NULL;
        t->depth = (GfxDepth *)calloc(1, sizeof(*t->depth));
        if (t->depth == NULL) {
            free(t);
            return NULL;
        }
        glGenFramebuffers(1, &t->fbo);
        glGenTextures(1, &t->color);
        glGenRenderbuffers(1, &t->depth->rb);
        t->addr = t;   /* a key of its own: no color image has this address */
        t->depth->addr = t;
    }
    t->w = t->depth->w = w;
    t->h = t->depth->h = h;
    t->s = s;
    t->pw = t->depth->pw = pcgfx_scaled(w, s);
    t->ph = t->depth->ph = pcgfx_scaled(h, s);
    t->vis_h = 0;
    dc_forget();
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t->pw, t->ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindRenderbuffer(GL_RENDERBUFFER, t->depth->rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t->pw, t->ph);
    bind_fbo(t->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t->depth->rb);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearBufferfv(GL_COLOR, 0, black);
    glClearBufferfv(GL_DEPTH, 0, &one);
    return t;
}

void pcgfx_gl_finish(void) {
    glFinish();
}

/* ---------------------------------------------------------------- GPU timers (RUSH_GFX_TIMES=3) */

static int has_extension(const char *name) {
    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    size_t n = strlen(name);
    while (ext != NULL && (ext = strstr(ext, name)) != NULL) {
        if (ext[n] == ' ' || ext[n] == '\0')
            return 1;
        ext += n;
    }
    return 0;
}

int pcgfx_gl_timers_init(GfxGetProc get_proc) {
    const char *ver = (const char *)glGetString(GL_VERSION);
    int maj = 0, min = 0;
    if (GT.ok)
        return 0;
    /* GL_EXTENSIONS through glGetString is gone from core profiles: the version decides there */
    if (ver != NULL && !G.gles)
        sscanf(ver, "%d.%d", &maj, &min);
    while (glGetError() != GL_NO_ERROR)
        ;
    GT.gen = (PcGlGenQueries)get_proc("glGenQueries");
    GT.del = (PcGlDeleteQueries)get_proc("glDeleteQueries");
    GT.begin = (PcGlBeginQuery)get_proc("glBeginQuery");
    GT.end = (PcGlEndQuery)get_proc("glEndQuery");
    GT.get = (PcGlGetQueryObjectuiv)get_proc("glGetQueryObjectuiv");
    if (G.gles) {
        if (!has_extension("GL_EXT_disjoint_timer_query"))
            return -1;
        GT.get64 = (PcGlGetQueryObjectui64v)get_proc("glGetQueryObjectui64vEXT");
    } else {
        if (maj * 10 + min < 33 && !has_extension("GL_ARB_timer_query"))
            return -1;
        GT.get64 = (PcGlGetQueryObjectui64v)get_proc("glGetQueryObjectui64v");
    }
    while (glGetError() != GL_NO_ERROR)
        ;
    if (GT.gen == NULL || GT.del == NULL || GT.begin == NULL || GT.end == NULL || GT.get == NULL || GT.get64 == NULL)
        return -1;
    GT.gen(GPU_TIMERS, GT.q);
    GT.ok = glGetError() == GL_NO_ERROR;
    return GT.ok ? 0 : -1;
}

/* Results that are ready (all of them, waiting, when wait is set). */
static void timers_collect(int wait) {
    while (GT.tail != GT.head) {
        GLuint id = GT.q[GT.tail % GPU_TIMERS], avail = 0;
        uint64_t ns = 0;
        if (!wait) {
            GT.get(id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if (!avail)
                break;
        }
        GT.get64(id, GL_QUERY_RESULT, &ns);
        GT.ms[GT.cat[GT.tail % GPU_TIMERS]] += (double)ns / 1e6;
        GT.n[GT.cat[GT.tail % GPU_TIMERS]]++;
        GT.tail++;
    }
}

void pcgfx_gl_timer_begin(int cat) {
    if (!GT.ok || GT.active)
        return;
    timers_collect(GT.head - GT.tail >= GPU_TIMERS - 1);
    GT.cat[GT.head % GPU_TIMERS] = (uint8_t)cat;
    GT.begin(GL_TIME_ELAPSED, GT.q[GT.head % GPU_TIMERS]);
    GT.active = 1;
}

void pcgfx_gl_timer_end(void) {
    if (!GT.ok || !GT.active)
        return;
    GT.end(GL_TIME_ELAPSED);
    GT.head++;
    GT.active = 0;
}

int pcgfx_gl_timer_stats(double ms[PCGFX_GPU_TIMER_CATS], uint32_t n[PCGFX_GPU_TIMER_CATS]) {
    int i;
    if (!GT.ok)
        return -1;
    timers_collect(1);
    for (i = 0; i < PCGFX_GPU_TIMER_CATS; i++) {
        ms[i] = GT.ms[i];
        n[i] = GT.n[i];
    }
    return 0;
}

void pcgfx_gl_fence_set(void) {
    if (glFenceSync == NULL)
        return;
    if (g_fence != NULL)
        glDeleteSync(g_fence);
    g_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

int pcgfx_gl_fence_busy(void) {
    if (g_fence == NULL || glFenceSync == NULL)
        return 0;
    /* work just handed over is in flight, not late: up to 2 ms for it */
    if (glClientWaitSync(g_fence, GL_SYNC_FLUSH_COMMANDS_BIT, 2000000u) == GL_TIMEOUT_EXPIRED)
        return 1;
    glDeleteSync(g_fence);
    g_fence = NULL;
    return 0;
}

void pcgfx_gl_aux_free(GfxTarget *t) {
    if (t == NULL)
        return;
    if (G.bound_fbo == t->fbo)
        bind_fbo(0);
    glDeleteFramebuffers(1, &t->fbo);
    glDeleteTextures(1, &t->color);
    if (t->depth != NULL) {
        glDeleteRenderbuffers(1, &t->depth->rb);
        free(t->depth);
    }
    free(t);
}

void pcgfx_gl_ui_draw(const GfxOutVtx *v, int n, int win_w, int win_h, int to_window, const uint8_t *atlas,
                      int atlas_w, int atlas_h) {
    if (n <= 0 || G.ui_failed || (!to_window && G.window_fbo == 0))
        return;
    if (G.ui_prog == 0) {
        GLuint vs = compile(GL_VERTEX_SHADER, pcgfx_vertex_source(G.gles, G.glsl));
        GLuint fs = compile(GL_FRAGMENT_SHADER, pcgfx_ui_fragment_source(G.gles, G.glsl));
        GLuint prog = glCreateProgram();
        GLint ok = 0;
        if (vs)
            glAttachShader(prog, vs);
        if (fs)
            glAttachShader(prog, fs);
        glBindAttribLocation(prog, 0, "aPos");
        glBindAttribLocation(prog, 1, "aShade");
        glBindAttribLocation(prog, 2, "aUV");
        if (!G.gles && glBindFragDataLocation != NULL)
            glBindFragDataLocation(prog, 0, "fragColor");
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        if (!ok || !vs || !fs) {
            fprintf(stderr, "gfx: the overlay's shader did not build; no overlay\n");
            glDeleteProgram(prog);
            G.ui_failed = 1;
            return;
        }
        G.ui_prog = prog;
        G.u_ui_tex = glGetUniformLocation(prog, "uTex0");
    }
    if (G.ui_tex == 0)
        G.ui_tex = pcgfx_gl_texture(atlas, atlas_w, atlas_h);
    dc_forget();
    bind_fbo(to_window ? 0 : G.window_fbo);
    glViewport(0, 0, win_w, win_h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glUseProgram(G.ui_prog);
    glUniform1i(G.u_ui_tex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, G.ui_tex);
    glDrawArrays(GL_TRIANGLES, stream_vertices(v, n), n);
    glDisable(GL_BLEND);
    bind_fbo(0);
}

static int read_fbo(GLuint fbo, int x, int y, int w, int h, uint8_t *rgba) {
    uint8_t *tmp;
    int r;
    tmp = (uint8_t *)malloc((size_t)w * h * 4);
    if (tmp == NULL)
        return -1;
    bind_fbo(fbo);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    for (r = 0; r < h; r++)   /* GL is bottom-up */
        memcpy(rgba + (size_t)r * w * 4, tmp + (size_t)(h - 1 - r) * w * 4, (size_t)w * 4);
    free(tmp);
    return glGetError() == GL_NO_ERROR ? 0 : -1;
}

int pcgfx_gl_read_target(GfxTarget *t, uint8_t *rgba, int *w, int *h) {
    if (t == NULL)
        return -1;
    *w = t->pw;
    *h = t->ph;
    if (rgba == NULL)
        return 0;
    return read_fbo(t->fbo, 0, 0, *w, *h, rgba);
}

int pcgfx_gl_read_window(uint8_t *rgba, int w, int h) {
    int rc = read_fbo(0, 0, 0, w, h, rgba);
    bind_fbo(0);
    return rc;
}

int pcgfx_gl_read_headless_window(uint8_t *rgba, int *w, int *h) {
    if (G.window_fbo == 0)
        return -1;
    *w = G.window_w;
    *h = G.window_h;
    if (rgba == NULL)
        return 0;
    return read_fbo(G.window_fbo, 0, 0, *w, *h, rgba);
}

int pcgfx_gl_read_presented(uint8_t *rgba, int *w, int *h) {
    if (!G.present_valid)
        return -1;
    *w = G.present_w;
    *h = G.present_h;
    if (rgba == NULL)
        return 0;
    if (G.present_src != NULL)
        return read_fbo(G.present_src->fbo, 0, G.present_sy, *w, *h, rgba);
    return read_fbo(G.present_fbo, 0, 0, *w, *h, rgba);
}
