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
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLGENTEXTURESPROC, glGenTextures) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLGETERRORPROC, glGetError) \
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

struct GfxShader {
    GfxShaderKey key;
    GLuint prog;
    GLint u_prim, u_env, u_fog, u_blend, u_fill, u_primlod, u_lodfrac, u_noise, u_scale;
    GfxShader *next;
};

typedef struct GfxDepth {
    const void *addr;
    int w, h, s;
    GLuint rb;
    struct GfxDepth *next;
} GfxDepth;

struct GfxTarget {
    const void *addr;
    int w, h, vis_h;    /* N64 pixels; vis_h: rows shown (the largest scissor bottom drawn with) */
    int s;              /* the internal scale it was made at */
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
    GLuint window_fbo, window_tex;  /* headless: the window, offscreen */
    int window_w, window_h;
    GLuint ui_prog, ui_tex;         /* the overlay (gfx_ui.c) */
    GLint u_ui_tex;
    int ui_failed;
    GLuint bound_fbo;
} G;

/* The scale new targets get: the setting, or the window's for auto (gfx_api.c). A target keeps its own until it
 * is made again, so a change applies from the next color image. */
static int scale(void) {
    return pcgfx_cfg.scale > 0 ? pcgfx_cfg.scale : 1;
}

int pcgfx_gl_target_scale(const GfxTarget *t) {
    return t != NULL ? t->s : scale();
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

int pcgfx_gl_init(GfxGetProc get_proc, int gles) {
    const char *ver;
    int ok = 1;
    memset(&G, 0, sizeof(G));
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
    if (pcgfx_cfg.verbose)
        fprintf(stderr, "gfx: %s / %s / %s\n", (const char *)glGetString(GL_RENDERER),
                (const char *)glGetString(GL_VERSION), ver ? ver : "?");
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
    return 0;
}

void pcgfx_gl_shutdown(void) {
    int b;
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
            if (t->w == d->w && t->h == d->h && t->s == d->s)
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
    int s = scale(), resized = t != NULL;
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
    t->w = w;
    t->h = h;
    t->s = s;
    t->depth = NULL;
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w * s, h * s, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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
    int s;
    if (t == NULL)
        return;
    s = t->s;
    for (d = G.depths; d != NULL; d = d->next)
        if (d->addr == zaddr && d->w == t->w && d->h == t->h && d->s == s)
            break;
    if (d == NULL) {
        float one = 1.0f;
        d = (GfxDepth *)calloc(1, sizeof(*d));
        d->addr = zaddr;
        d->w = t->w;
        d->h = t->h;
        d->s = s;
        glGenRenderbuffers(1, &d->rb);
        glBindRenderbuffer(GL_RENDERBUFFER, d->rb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t->w * s, t->h * s);
        d->next = G.depths;
        G.depths = d;
        t->depth = NULL;
        bind_fbo(t->fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, d->rb);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);
        glClearBufferfv(GL_DEPTH, 0, &one);
        t->depth = d;
    }
    if (t->depth != d) {
        bind_fbo(t->fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, d->rb);
        t->depth = d;
    }
}

static void set_scissor(const GfxTarget *t, const int32_t r[4]) {
    int s = t->s;
    int x0 = r[0] < 0 ? 0 : r[0], y0 = r[1] < 0 ? 0 : r[1];
    int x1 = r[2] > t->w ? t->w : r[2], y1 = r[3] > t->h ? t->h : r[3];
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0 * s, (t->h - y1) * s, (x1 - x0) * s, (y1 - y0) * s);
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
    glViewport(0, 0, t->w * t->s, t->h * t->s);
    set_scissor(t, rect);
    if (depth) {
        if (t->depth == NULL)
            return;
        glDepthMask(GL_TRUE);
        glClearBufferfv(GL_DEPTH, 0, &z);
    } else {
        float c[4];
        memcpy(c, rgba, sizeof(c));
        c[3] = 1.0f;
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearBufferfv(GL_COLOR, 0, c);
        note_rows(t, rect[3]);
    }
}

/* ---------------------------------------------------------------- textures */

uint32_t pcgfx_gl_texture(const uint8_t *rgba, int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

void pcgfx_gl_delete_texture(uint32_t tex) {
    GLuint t = tex;
    if (t != 0)
        glDeleteTextures(1, &t);
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
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "uTex0"), 0);
    glUniform1i(glGetUniformLocation(prog, "uTex1"), 1);
    s->next = G.shaders[b];
    G.shaders[b] = s;
    return s;
}

/* ---------------------------------------------------------------- draw */

void pcgfx_gl_draw(const GfxDrawState *st, const GfxOutVtx *v, int n) {
    GfxTarget *t = st->target;
    const GfxShader *sh = st->shader;
    int u;
    if (t == NULL || sh == NULL || n <= 0)
        return;
    bind_fbo(t->fbo);
    glViewport(0, 0, t->w * t->s, t->h * t->s);
    glUseProgram(sh->prog);
    glUniform4fv(sh->u_prim, 1, st->prim);
    glUniform4fv(sh->u_env, 1, st->env);
    glUniform4fv(sh->u_fog, 1, st->fog);
    glUniform4fv(sh->u_blend, 1, st->blendc);
    glUniform4fv(sh->u_fill, 1, st->fill);
    glUniform1f(sh->u_primlod, st->prim_lod);
    glUniform1f(sh->u_lodfrac, st->lod_frac);
    glUniform1f(sh->u_noise, st->noise_seed);
    glUniform1f(sh->u_scale, st->scale);
    for (u = 0; u < 2; u++) {
        glActiveTexture(GL_TEXTURE0 + u);
        if (st->tex[u]) {
            GLint f = st->linear[u] ? GL_LINEAR : GL_NEAREST;
            glBindTexture(GL_TEXTURE_2D, st->tex[u]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, st->repeat_s[u] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, st->repeat_t[u] ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_2D, G.white);
        }
    }
    if (st->depth_test && t->depth != NULL) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(st->depth_test == 2 ? GL_ALWAYS : GL_LEQUAL);
        glDepthMask(st->depth_write ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    if (st->decal) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -2.0f);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (st->blend) {
        /* rgb: what the shader computed without memory; alpha: memory's weight (gfx_combine.c) */
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_ONE);
    } else {
        glDisable(GL_BLEND);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDisable(GL_CULL_FACE);   /* culling happens in gfx_rsp.c, in N64 screen space */
    set_scissor(t, st->scissor);
    note_rows(t, st->scissor[3]);
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(*v) * (size_t)n), v, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, n);
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
    int s, rows, pw, ph;
    if (t == NULL)
        return;
    s = t->s;
    rows = target_rows(t);
    pw = t->w * s;
    ph = rows * s;
    if (G.present_fbo == 0) {
        glGenFramebuffers(1, &G.present_fbo);
        glGenTextures(1, &G.present_tex);
    }
    if (G.present_w != pw || G.present_h != ph) {
        glBindTexture(GL_TEXTURE_2D, G.present_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        bind_fbo(G.present_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, G.present_tex, 0);
        G.present_w = pw;
        G.present_h = ph;
    }
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    /* N64 row 0 is the top: GL rows (h - rows) .. h of the target */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, t->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, G.present_fbo);
    glBlitFramebuffer(0, (t->h - rows) * s, pw, t->h * s, 0, 0, pw, ph, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    G.present_valid = 1;
    G.present_fw = t->w;
    G.present_rows = rows;
}

/* Clear the window (or the headless window's image) and blit (sx0, sy0)-(sx0 + pw, sy0 + ph) of src into it: a
 * frame fw x rows N64 pixels, with square pixels, at a whole multiple when one fits, else aspect fit. src 0: none. */
static void show_from(GLuint src, int sx0, int sy0, int pw, int ph, int fw, int rows, int win_w, int win_h,
                      int to_window) {
    static const float black[4] = {0, 0, 0, 1};
    GLuint dst = 0;
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
        /* square N64 pixels; integer multiples when they fit, else aspect fit */
        int k = win_w / fw < win_h / rows ? win_w / fw : win_h / rows, dw, dh, dx, dy;
        if (k >= 1) {
            dw = fw * k;
            dh = rows * k;
        } else if ((long)win_w * rows > (long)win_h * fw) {
            dh = win_h;
            dw = win_h * fw / rows;
        } else {
            dw = win_w;
            dh = win_w * rows / fw;
        }
        dx = (win_w - dw) / 2;
        dy = (win_h - dh) / 2;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, src);
        glBlitFramebuffer(sx0, sy0, sx0 + pw, sy0 + ph, dx, dy, dx + dw, dy + dh, GL_COLOR_BUFFER_BIT,
                          (dw % pw == 0 && dh % ph == 0) ? GL_NEAREST : GL_LINEAR);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    G.bound_fbo = 0;
}

void pcgfx_gl_show(int win_w, int win_h, int to_window) {
    show_from(G.present_valid ? G.present_fbo : 0, 0, 0, G.present_w, G.present_h, G.present_fw, G.present_rows,
              win_w, win_h, to_window);
}

void pcgfx_gl_show_target(GfxTarget *t, int rows, int win_w, int win_h, int to_window) {
    int s;
    if (t == NULL)
        return;
    s = t->s;
    if (rows <= 0 || rows > t->h)
        rows = t->h;
    /* N64 row 0 is the top: GL rows (h - rows) .. h of the target, as the present copies them */
    show_from(t->fbo, 0, (t->h - rows) * s, t->w * s, rows * s, t->w, rows, win_w, win_h, to_window);
}

GfxTarget *pcgfx_gl_aux_target(GfxTarget *t, int w, int h, int s) {
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
    t->s = t->depth->s = s;
    t->vis_h = 0;
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w * s, h * s, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindRenderbuffer(GL_RENDERBUFFER, t->depth->rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w * s, h * s);
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
    glBindBuffer(GL_ARRAY_BUFFER, G.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(*v) * (size_t)n), v, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, n);
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
    int s;
    if (t == NULL)
        return -1;
    s = t->s;
    *w = t->w * s;
    *h = t->h * s;
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
    return read_fbo(G.present_fbo, 0, 0, *w, *h, rgba);
}
