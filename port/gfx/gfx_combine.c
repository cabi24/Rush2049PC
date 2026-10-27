/* port/gfx/gfx_combine.c: compiles a color combiner + blender configuration (GfxShaderKey) into GLSL.
 *
 * Combiner: (A - B) * C + D per cycle, color and alpha. 1-cycle mode runs the second cycle's settings (as the
 * RDP does; games set both cycles alike), 2-cycle runs both, with COMBINED carrying cycle 1 into cycle 2.
 * Blender: the 2-cycle first blender cycle is computed in the shader (memory reads as 0). The final cycle splits
 * into a part without memory (the shader's rgb) and the weight of memory (the shader's alpha); GL then blends
 * with (ONE, SRC_ALPHA). See GFX.md "Blender". */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

typedef struct {
    int a, b, c, d;       /* color */
    int aa, ab, ac, ad;   /* alpha */
} Cycle;

static void decode(const GfxShaderKey *k, Cycle cyc[2]) {
    uint32_t w0 = k->cc_w0, w1 = k->cc_w1;
    cyc[0].a = (w0 >> 20) & 0xF;
    cyc[0].c = (w0 >> 15) & 0x1F;
    cyc[0].aa = (w0 >> 12) & 7;
    cyc[0].ac = (w0 >> 9) & 7;
    cyc[1].a = (w0 >> 5) & 0xF;
    cyc[1].c = w0 & 0x1F;
    cyc[0].b = (w1 >> 28) & 0xF;
    cyc[1].b = (w1 >> 24) & 0xF;
    cyc[1].aa = (w1 >> 21) & 7;
    cyc[1].ac = (w1 >> 18) & 7;
    cyc[0].d = (w1 >> 15) & 7;
    cyc[0].ab = (w1 >> 12) & 7;
    cyc[0].ad = (w1 >> 9) & 7;
    cyc[1].d = (w1 >> 6) & 7;
    cyc[1].ab = (w1 >> 3) & 7;
    cyc[1].ad = w1 & 7;
}

/* Which cycles run: 1-cycle uses cycle index 1 only. */
static int first_cycle(const GfxShaderKey *k) {
    return k->mode == GFX_MODE_2CYC ? 0 : 1;
}

static int cycle_uses(const Cycle *c, int texel) {
    int t = texel + 1;   /* TEXEL0 = 1, TEXEL1 = 2 */
    if (c->a == t || c->b == t || c->c == t || c->d == t || c->c == t + 7)
        return 1;
    if (c->aa == t || c->ab == t || c->ac == t || c->ad == t)
        return 1;
    return 0;
}

int gfx_combine_uses(const GfxShaderKey *k, int texel) {
    Cycle cyc[2];
    int i;
    if (k->mode == GFX_MODE_COPY)
        return texel == 0;
    if (k->mode == GFX_MODE_FILL)
        return 0;
    decode(k, cyc);
    for (i = first_cycle(k); i < 2; i++)
        if (cycle_uses(&cyc[i], texel))
            return 1;
    return 0;
}

static const char *cc_a(int v) {
    switch (v) {
    case 0: return "cmb.rgb";
    case 1: return "t0.rgb";
    case 2: return "t1.rgb";
    case 3: return "uPrim.rgb";
    case 4: return "sh.rgb";
    case 5: return "uEnv.rgb";
    case 6: return "vec3(1.0)";
    case 7: return "vec3(noise())";
    default: return "vec3(0.0)";
    }
}

static const char *cc_b(int v) {
    switch (v) {
    case 0: return "cmb.rgb";
    case 1: return "t0.rgb";
    case 2: return "t1.rgb";
    case 3: return "uPrim.rgb";
    case 4: return "sh.rgb";
    case 5: return "uEnv.rgb";
    default: return "vec3(0.0)";   /* key center, K4: not emulated */
    }
}

static const char *cc_c(int v) {
    switch (v) {
    case 0: return "cmb.rgb";
    case 1: return "t0.rgb";
    case 2: return "t1.rgb";
    case 3: return "uPrim.rgb";
    case 4: return "sh.rgb";
    case 5: return "uEnv.rgb";
    case 7: return "vec3(cmb.a)";
    case 8: return "vec3(t0.a)";
    case 9: return "vec3(t1.a)";
    case 10: return "vec3(uPrim.a)";
    case 11: return "vec3(sh.a)";
    case 12: return "vec3(uEnv.a)";
    case 13: return "vec3(uLodFrac)";
    case 14: return "vec3(uPrimLod)";
    default: return "vec3(0.0)";   /* key scale, K5 */
    }
}

static const char *cc_d(int v) {
    switch (v) {
    case 0: return "cmb.rgb";
    case 1: return "t0.rgb";
    case 2: return "t1.rgb";
    case 3: return "uPrim.rgb";
    case 4: return "sh.rgb";
    case 5: return "uEnv.rgb";
    case 6: return "vec3(1.0)";
    default: return "vec3(0.0)";
    }
}

static const char *ac_abd(int v) {
    switch (v) {
    case 0: return "cmb.a";
    case 1: return "t0.a";
    case 2: return "t1.a";
    case 3: return "uPrim.a";
    case 4: return "sh.a";
    case 5: return "uEnv.a";
    case 6: return "1.0";
    default: return "0.0";
    }
}

static const char *ac_c(int v) {
    switch (v) {
    case 0: return "uLodFrac";
    case 1: return "t0.a";
    case 2: return "t1.a";
    case 3: return "uPrim.a";
    case 4: return "sh.a";
    case 5: return "uEnv.a";
    case 6: return "uPrimLod";
    default: return "0.0";
    }
}

/* Blender inputs. P/M: color, A: alpha, B: second weight. Memory never comes through here. */
static const char *bl_pm(int v) {
    switch (v) {
    case G_BL_CLR_IN: return "bin";
    case G_BL_CLR_BL: return "uBlend.rgb";
    case G_BL_CLR_FOG: return "uFog.rgb";
    default: return "vec3(0.0)";   /* memory */
    }
}

static const char *bl_a(int v) {
    switch (v) {
    case G_BL_A_IN: return "ain";
    case G_BL_A_FOG: return "uFog.a";
    case G_BL_A_SHADE: return "sh.a";
    default: return "0.0";
    }
}

static const char *bl_b(int v) {
    switch (v) {
    case G_BL_1MA: return "(1.0 - a)";
    case G_BL_A_MEM: return "1.0";   /* memory alpha (coverage) taken as full */
    case G_BL_1: return "1.0";
    default: return "0.0";
    }
}

typedef struct {
    int p, a, m, b;
} BlCycle;

static BlCycle blender_cycle(const GfxShaderKey *k, int cyc) {
    BlCycle c;
    unsigned bl = k->blender;
    if (cyc == 0) {
        c.p = (bl >> 14) & 3;
        c.a = (bl >> 10) & 3;
        c.m = (bl >> 6) & 3;
        c.b = (bl >> 2) & 3;
    } else {
        c.p = (bl >> 12) & 3;
        c.a = (bl >> 8) & 3;
        c.m = (bl >> 4) & 3;
        c.b = bl & 3;
    }
    return c;
}

/* 1-cycle: the blender's first cycle; 2-cycle: the second. */
static BlCycle final_cycle(const GfxShaderKey *k) {
    return blender_cycle(k, k->mode == GFX_MODE_2CYC ? 1 : 0);
}

/* Does the result depend on memory? Directly (the final cycle reads it) or through CLR_IN when the 2-cycle first
 * cycle read it. */
int gfx_blend_uses_mem(const GfxShaderKey *k) {
    BlCycle c;
    int in_has_mem = 0;
    if (k->mode != GFX_MODE_1CYC && k->mode != GFX_MODE_2CYC)
        return 0;
    if (k->mode == GFX_MODE_2CYC) {
        BlCycle c1 = blender_cycle(k, 0);
        in_has_mem = c1.p == G_BL_CLR_MEM || c1.m == G_BL_CLR_MEM;
    }
    c = final_cycle(k);
    if (k->flags & GFX_KF_FORCE_BL)
        return c.p == G_BL_CLR_MEM || c.m == G_BL_CLR_MEM ||
               (in_has_mem && (c.p == G_BL_CLR_IN || c.m == G_BL_CLR_IN));
    return c.p == G_BL_CLR_MEM || (in_has_mem && c.p == G_BL_CLR_IN);
}

typedef struct {
    char *buf;
    size_t len, cap;
} Str;

static void put(Str *s, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(Str *s, const char *fmt, ...) {
    va_list ap;
    int n;
    for (;;) {
        va_start(ap, fmt);
        n = vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
        va_end(ap);
        if (n >= 0 && (size_t)n < s->cap - s->len) {
            s->len += (size_t)n;
            return;
        }
        s->cap = s->cap * 2 + (size_t)(n > 0 ? n : 256);
        s->buf = (char *)realloc(s->buf, s->cap);
    }
}

static const char *preamble(int gles, int glsl_version) {
    if (gles)
        return "#version 300 es\nprecision highp float;\nprecision highp int;\n";
    return glsl_version >= 330 ? "#version 330 core\n" : "#version 140\n";
}

static char s_vs[1024];

static const char *vertex_body(void) {
    return "in vec4 aPos;\n"
           "in vec4 aShade;\n"
           "in vec4 aUV;\n"
           "out vec4 vShade;\n"
           "out vec4 vUV;\n"
           "void main() {\n"
           "    vShade = aShade;\n"
           "    vUV = aUV;\n"
           "    gl_Position = aPos;\n"
           "}\n";
}

const char *gfx_vertex_source(int gles, int glsl_version) {
    snprintf(s_vs, sizeof(s_vs), "%s%s", preamble(gles, glsl_version), vertex_body());
    return s_vs;
}

/* One blender term, color input v weighted by w: memory adds to memw, CLR_IN carries its own memory weight. */
static void put_term(Str *s, int v, const char *w) {
    if (v == G_BL_CLR_MEM)
        put(s, "    memw += %s;\n", w);
    else if (v == G_BL_CLR_IN)
        put(s, "    rgb += bin * %s;\n    memw += binw * %s;\n", w, w);
    else
        put(s, "    rgb += %s * %s;\n", bl_pm(v), w);
}

char *gfx_combine_glsl(const GfxShaderKey *k, int gles, int glsl_version) {
    Str s = {0};
    Cycle cyc[2];
    int i;
    s.cap = 4096;
    s.buf = (char *)malloc(s.cap);
    s.buf[0] = 0;

    put(&s, "%s", preamble(gles, glsl_version));
    put(&s, "uniform vec4 uPrim;\nuniform vec4 uEnv;\nuniform vec4 uFog;\nuniform vec4 uBlend;\nuniform vec4 uFill;\n"
            "uniform float uPrimLod;\nuniform float uLodFrac;\nuniform float uNoise;\nuniform float uScale;\n"
            "uniform sampler2D uTex0;\nuniform sampler2D uTex1;\n"
            "in vec4 vShade;\nin vec4 vUV;\nout vec4 fragColor;\n"
            "float noise() {\n"
            "    return fract(sin(dot(gl_FragCoord.xy + vec2(uNoise), vec2(12.9898, 78.233))) * 43758.5453);\n"
            "}\n"
            "void main() {\n"
            "    vec4 sh = vShade;\n"
            "    vec4 cmb = vec4(0.0);\n"
            "    vec4 uv = vUV;\n");
    if (k->flags & GFX_KF_SNAP)   /* back to the top-left sample of this N64 pixel (GL rows run bottom-up) */
        put(&s, "    vec2 j = mod(floor(gl_FragCoord.xy), uScale);\n"
                "    uv = uv - j.x * dFdx(vUV) + (uScale - 1.0 - j.y) * dFdy(vUV);\n");
    put(&s, "    vec4 t0 = %s;\n", (k->flags & GFX_KF_TEX0) ? "texture(uTex0, uv.xy)" : "vec4(0.0)");
    put(&s, "    vec4 t1 = %s;\n", (k->flags & GFX_KF_TEX1) ? "texture(uTex1, uv.zw)" : "vec4(0.0)");

    if (k->mode == GFX_MODE_FILL) {
        put(&s, "    fragColor = vec4(uFill.rgb, 1.0);\n}\n");
        return s.buf;
    }
    if (k->mode == GFX_MODE_COPY) {
        if (k->ac)
            put(&s, "    if (t0.a < 0.5) discard;\n");
        put(&s, "    fragColor = vec4(t0.rgb, 1.0);\n}\n");
        return s.buf;
    }

    decode(k, cyc);
    for (i = first_cycle(k); i < 2; i++) {
        const Cycle *c = &cyc[i];
        put(&s, "    cmb = clamp(vec4((%s - %s) * %s + %s, (%s - %s) * %s + %s), 0.0, 1.0);\n",
            cc_a(c->a), cc_b(c->b), cc_c(c->c), cc_d(c->d), ac_abd(c->aa), ac_abd(c->ab), ac_c(c->ac),
            ac_abd(c->ad));
    }

    /* Alpha compare and coverage. CVG_X_ALPHA turns alpha into coverage: zero coverage writes nothing. */
    if (k->ac == G_AC_THRESHOLD)
        put(&s, "    if (cmb.a < uBlend.a) discard;\n");
    else if (k->ac == G_AC_DITHER)
        put(&s, "    if (cmb.a < noise()) discard;\n");
    if (k->flags & GFX_KF_CVG_X_ALPHA)
        put(&s, "    if (cmb.a < %s) discard;\n", (k->flags & GFX_KF_FORCE_BL) ? "0.125" : "0.5");

    /* Blender. Values are kept as (rgb without memory, weight of memory): exact for these linear blends. */
    put(&s, "    vec3 bin = cmb.rgb;\n    float binw = 0.0;\n");
    put(&s, "    float ain = %s;\n",
        ((k->flags & GFX_KF_ALPHA_CVG_SEL) && !(k->flags & GFX_KF_CVG_X_ALPHA)) ? "1.0" : "cmb.a");
    put(&s, "    vec3 rgb = vec3(0.0);\n    float memw = 0.0;\n");
    if (k->mode == GFX_MODE_2CYC) {   /* the first cycle always blends */
        BlCycle c = blender_cycle(k, 0);
        put(&s, "    {\n    float a = %s;\n    float b = %s;\n", bl_a(c.a), bl_b(c.b));
        put_term(&s, c.p, "a");
        put_term(&s, c.m, "b");
        put(&s, "    bin = rgb;\n    binw = memw;\n    rgb = vec3(0.0);\n    memw = 0.0;\n    }\n");
    }
    {
        BlCycle c = final_cycle(k);
        if (k->flags & GFX_KF_FORCE_BL) {
            put(&s, "    float a = %s;\n    float b = %s;\n", bl_a(c.a), bl_b(c.b));
            put_term(&s, c.p, "a");
            put_term(&s, c.m, "b");
        } else {   /* no blend: the first input passes */
            put_term(&s, c.p, "1.0");
        }
        put(&s, "    fragColor = vec4(rgb, %s);\n", gfx_blend_uses_mem(k) ? "memw" : "1.0");
    }
    put(&s, "}\n");
    return s.buf;
}
