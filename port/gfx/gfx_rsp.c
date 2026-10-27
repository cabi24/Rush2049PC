/* port/gfx/gfx_rsp.c: the RSP half of the F3DEX2 interpreter. Walks display lists (native or N64 form), keeps
 * the matrix stack, vertex buffer, lights, fog and segments, transforms and lights vertices on the CPU, and turns
 * triangles into clipped, culled GL vertices. RDP commands go to gfx_rdp.c. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

GfxState pcgfx;

/* Vertices with w below this are clipped away before the divide (the NoN microcode has no near plane; see
 * GFX.md "Clipping"). */
#define GFX_W_EPS 1e-4f

/* ---------------------------------------------------------------- memory */

uint16_t pcgfx_rd16(const void *p, int be) {
    const uint8_t *b = (const uint8_t *)p;
    uint16_t v;
    if (be)
        return (uint16_t)(b[0] << 8 | b[1]);
    memcpy(&v, p, 2);
    return v;
}

uint32_t pcgfx_rd32(const void *p, int be) {
    const uint8_t *b = (const uint8_t *)p;
    uint32_t v;
    if (be)
        return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    memcpy(&v, p, 4);
    return v;
}

int pcgfx_is_be(const void *p) {
    return p != NULL && pc_mem_is_be(p);
}

static const uint8_t *rdram(uint32_t phys) {
    return (const uint8_t *)pc_n64_ptr(PC_RDRAM_BASE | (phys & 0x1FFFFFFFu));
}

/* Addresses (docs/port/GFX.md "Addresses"). Host pointers are at or above 4 GB (the build is PIE and RDRAM is the
 * window at 0x80000000); anything below is an N64 address, in a native list or an N64-form one alike:
 * K0/K1 (0x80000000-0xBFFFFFFF), segmented (0x0S000000 + offset, S = 1..15 with a base set) or physical
 * (segment 0, or a segment with no base: RDRAM offset). */
static int is_host(uintptr_t a) {
#if UINTPTR_MAX > 0xFFFFFFFFu
    return a > 0xFFFFFFFFu;
#else
    (void)a;
    return 0;
#endif
}

uintptr_t pcgfx_segment_value(uintptr_t v) {
    uint32_t a;
    if (v == 0 || is_host(v))
        return v;
    a = (uint32_t)v;
    return (uintptr_t)(a >= 0x80000000u ? pc_n64_ptr(a) : rdram(a));
}

const uint8_t *pcgfx_resolve(uintptr_t a) {
    uint32_t a32, seg;
    if (a == 0)
        return NULL;
    if (is_host(a))
        return (const uint8_t *)a;
    a32 = (uint32_t)a;
    if (a32 >= 0x80000000u)
        return (const uint8_t *)pc_n64_ptr(a32);
    seg = (a32 >> 24) & 0x0F;
    if (pcgfx.seg[seg] != 0)
        return (const uint8_t *)(pcgfx.seg[seg] + (a32 & 0x00FFFFFF));
    return rdram(a32 & 0x00FFFFFF);
}

/* ---------------------------------------------------------------- matrices */

static void mtx_mul(float out[4][4], const float a[4][4], const float b[4][4]) {
    float r[4][4];
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
    memcpy(out, r, sizeof(r));
}

static void mtx_ident(float m[4][4]) {
    memset(m, 0, sizeof(float) * 16);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

/* Mtx: 16 s15.16 values, integer halves in the first 32 bytes, fractions in the last 32, two per 32-bit word.
 * guMtxF2L writes 32-bit words, so native matrices are read as native words. */
static void mtx_read(const uint8_t *p, int be, float f[4][4]) {
    int k;
    for (k = 0; k < 8; k++) {
        uint32_t ip = pcgfx_rd32(p + 4 * k, be);
        uint32_t fp = pcgfx_rd32(p + 32 + 4 * k, be);
        int32_t e0 = (int32_t)((ip & 0xFFFF0000u) | (fp >> 16));
        int32_t e1 = (int32_t)((ip << 16) | (fp & 0xFFFFu));
        f[(2 * k) >> 2][(2 * k) & 3] = (float)e0 * (1.0f / 65536.0f);
        f[(2 * k + 1) >> 2][(2 * k + 1) & 3] = (float)e1 * (1.0f / 65536.0f);
    }
}

/* A perspective matrix makes w depend on the position: its w column (row vectors) is not (0, 0, 0, 1).
 * guPerspective's is (0, 0, -1, 0), and the game's lookat rotation multiplied onto it keeps the top three nonzero;
 * guOrtho's is (0, 0, 0, 1). */
static int is_persp(const float m[4][4]) {
    return m[0][3] != 0.0f || m[1][3] != 0.0f || m[2][3] != 0.0f;
}

static void update_mp(void) {
    if (pcgfx.mp_dirty) {
        mtx_mul(pcgfx.mp, pcgfx.mv[pcgfx.mv_top], pcgfx.proj);
        pcgfx.persp = is_persp(pcgfx.proj);
        pcgfx.mp_dirty = 0;
    }
}

/* Widescreen (GFX.md "Widescreen"): where the viewport lands in the wide target. A perspective view is widened:
 * its sides go through pcgfx_wide_x, so a view across the frame spans the target and split-screen views widen in
 * proportion, and the frustum widens with it (*xs: clip x over w as the view sees it, for the outcodes). The
 * picture keeps its scale, so the view shows more at the sides instead of a stretched image. Anything else is 2D
 * and moves with the 4:3 frame. Off: the N64's viewport. */
static float view_x(float *xs) {
    float t = pcgfx.vp_trans[0], h, l, r;
    *xs = 1.0f;
    if (pcgfx.wide_ox == 0)
        return t;
    update_mp();
    if (!pcgfx.persp)
        return t + (float)pcgfx.wide_ox;
    h = fabsf(pcgfx.vp_scale[0]);
    l = pcgfx_wide_x(t - h);
    r = pcgfx_wide_x(t + h);
    if (h > 0.0f)
        *xs = 2.0f * h / (r - l);
    return 0.5f * (l + r);
}

/* G_MW_MATRIX: replace the integer or fraction halves of two MP elements. */
static void mp_insert(uint32_t off, uint32_t val) {
    int e = (int)((off & 0x1F) >> 1), k;
    update_mp();
    for (k = 0; k < 2; k++) {
        float *f = &pcgfx.mp[(e + k) >> 2][(e + k) & 3];
        int32_t fx = (int32_t)lrintf(*f * 65536.0f);
        uint32_t half = k == 0 ? val >> 16 : val & 0xFFFFu;
        if (off < 0x20)
            fx = (int32_t)((half << 16) | ((uint32_t)fx & 0xFFFFu));
        else
            fx = (int32_t)(((uint32_t)fx & 0xFFFF0000u) | half);
        *f = (float)fx * (1.0f / 65536.0f);
    }
}

/* ---------------------------------------------------------------- lights */

/* Light directions are given in eye space; F3DEX2 dots them with model-space normals, so they are taken into model
 * space through the modelview's upper 3x3 (row-vector convention) and normalized. */
static void dir_to_model(const int8_t d[3], float out[3]) {
    const float(*m)[4] = pcgfx.mv[pcgfx.mv_top];
    float x = d[0] / 127.0f, y = d[1] / 127.0f, z = d[2] / 127.0f, len;
    int i;
    for (i = 0; i < 3; i++)
        out[i] = m[i][0] * x + m[i][1] * y + m[i][2] * z;
    len = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
    if (len > 1e-6f)
        for (i = 0; i < 3; i++)
            out[i] /= len;
}

static void update_lights(void) {
    int i;
    for (i = 0; i < pcgfx.num_lights && i < GFX_MAX_LIGHTS; i++)
        dir_to_model(pcgfx.lights[i].dir, pcgfx.light_dir[i]);
    dir_to_model(pcgfx.lookat[0].dir, pcgfx.lookat_dir[0]);
    dir_to_model(pcgfx.lookat[1].dir, pcgfx.lookat_dir[1]);
    pcgfx.lights_dirty = 0;
}

static void light_read(const uint8_t *p, GfxLight *l) {
    l->col[0] = p[0];
    l->col[1] = p[1];
    l->col[2] = p[2];
    l->colc[0] = p[4];
    l->colc[1] = p[5];
    l->colc[2] = p[6];
    l->dir[0] = (int8_t)p[8];
    l->dir[1] = (int8_t)p[9];
    l->dir[2] = (int8_t)p[10];
}

/* ---------------------------------------------------------------- vertices */

static uint8_t outcode(const GfxVtx *v, float xs) {
    uint8_t c = 0;
    float x = v->x * xs;
    if (x < -v->w) c |= 1;
    if (x > v->w) c |= 2;
    if (v->y < -v->w) c |= 4;
    if (v->y > v->w) c |= 8;
    if (v->w < GFX_W_EPS) c |= 16;
    return c;
}

static float fog_alpha(float z, float w) {
    float f;
    if (w < GFX_W_EPS)
        w = GFX_W_EPS;
    f = z / w * (float)pcgfx.fog_mul + (float)pcgfx.fog_off;
    if (f < 0.0f) f = 0.0f;
    if (f > 255.0f) f = 255.0f;
    return f / 255.0f;
}

static void load_vertices(const uint8_t *p, int be, int v0, int n) {
    const float(*mp)[4];
    int i, k;
    float xs;
    update_mp();
    view_x(&xs);
    mp = (const float(*)[4])pcgfx.mp;
    if ((pcgfx.geom & (G_LIGHTING | G_TEXTURE_GEN)) && pcgfx.lights_dirty)
        update_lights();
    for (i = 0; i < n; i++, p += 16) {
        GfxVtx *v;
        float x, y, z;
        int16_t s, t;
        if (v0 + i >= 32)
            break;
        v = &pcgfx.vtx[v0 + i];
        x = (int16_t)pcgfx_rd16(p + 0, be);
        y = (int16_t)pcgfx_rd16(p + 2, be);
        z = (int16_t)pcgfx_rd16(p + 4, be);
        s = (int16_t)pcgfx_rd16(p + 8, be);
        t = (int16_t)pcgfx_rd16(p + 10, be);
        v->x = x * mp[0][0] + y * mp[1][0] + z * mp[2][0] + mp[3][0];
        v->y = x * mp[0][1] + y * mp[1][1] + z * mp[2][1] + mp[3][1];
        v->z = x * mp[0][2] + y * mp[1][2] + z * mp[2][2] + mp[3][2];
        v->w = x * mp[0][3] + y * mp[1][3] + z * mp[2][3] + mp[3][3];
        if (pcgfx.geom & G_LIGHTING) {
            float nx = (int8_t)p[12] / 127.0f, ny = (int8_t)p[13] / 127.0f, nz = (int8_t)p[14] / 127.0f;
            const GfxLight *amb = &pcgfx.lights[pcgfx.num_lights < GFX_MAX_LIGHTS ? pcgfx.num_lights : GFX_MAX_LIGHTS];
            float c[3];
            for (k = 0; k < 3; k++)
                c[k] = amb->col[k];
            for (k = 0; k < pcgfx.num_lights && k < GFX_MAX_LIGHTS; k++) {
                const float *d = pcgfx.light_dir[k];
                float dot = nx * d[0] + ny * d[1] + nz * d[2];
                if (dot > 0.0f) {
                    c[0] += dot * pcgfx.lights[k].col[0];
                    c[1] += dot * pcgfx.lights[k].col[1];
                    c[2] += dot * pcgfx.lights[k].col[2];
                }
            }
            v->r = (c[0] > 255.0f ? 255.0f : c[0]) / 255.0f;
            v->g = (c[1] > 255.0f ? 255.0f : c[1]) / 255.0f;
            v->b = (c[2] > 255.0f ? 255.0f : c[2]) / 255.0f;
            if (pcgfx.geom & G_TEXTURE_GEN) {
                float dx = nx * pcgfx.lookat_dir[0][0] + ny * pcgfx.lookat_dir[0][1] + nz * pcgfx.lookat_dir[0][2];
                float dy = nx * pcgfx.lookat_dir[1][0] + ny * pcgfx.lookat_dir[1][1] + nz * pcgfx.lookat_dir[1][2];
                if (dx > 1.0f) dx = 1.0f;
                if (dx < -1.0f) dx = -1.0f;
                if (dy > 1.0f) dy = 1.0f;
                if (dy < -1.0f) dy = -1.0f;
                if (pcgfx.geom & G_TEXTURE_GEN_LINEAR) {
                    v->s = acosf(-dx) / 3.14159265f * 0.5f * pcgfx.tex_scale_s;
                    v->t = acosf(-dy) / 3.14159265f * 0.5f * pcgfx.tex_scale_t;
                } else {
                    v->s = (dx + 1.0f) * 0.25f * pcgfx.tex_scale_s;
                    v->t = (dy + 1.0f) * 0.25f * pcgfx.tex_scale_t;
                }
            } else {
                v->s = s * (pcgfx.tex_scale_s / 65536.0f);
                v->t = t * (pcgfx.tex_scale_t / 65536.0f);
            }
        } else {
            v->r = p[12] / 255.0f;
            v->g = p[13] / 255.0f;
            v->b = p[14] / 255.0f;
            v->s = s * (pcgfx.tex_scale_s / 65536.0f);
            v->t = t * (pcgfx.tex_scale_t / 65536.0f);
        }
        v->a = (pcgfx.geom & G_FOG) ? fog_alpha(v->z, v->w) : p[15] / 255.0f;
        v->clip = outcode(v, xs);
    }
}

static float screen_z(const GfxVtx *v) {
    float w = v->w < GFX_W_EPS ? GFX_W_EPS : v->w;
    return pcgfx.vp_trans[2] + v->z / w * pcgfx.vp_scale[2];
}

static void modify_vertex(uint32_t w0, uint32_t w1) {
    unsigned idx = (w0 & 0xFFFF) >> 1;
    GfxVtx *v;
    if (idx >= 32)
        return;
    v = &pcgfx.vtx[idx];
    switch ((w0 >> 16) & 0xFF) {
    case G_MWO_POINT_RGBA:
        v->r = (w1 >> 24) / 255.0f;
        v->g = ((w1 >> 16) & 0xFF) / 255.0f;
        v->b = ((w1 >> 8) & 0xFF) / 255.0f;
        v->a = (w1 & 0xFF) / 255.0f;
        break;
    case G_MWO_POINT_ST:
        v->s = (int16_t)(w1 >> 16);
        v->t = (int16_t)(w1 & 0xFFFF);
        break;
    case G_MWO_POINT_XYSCREEN: {   /* screen position in 10.2: invert the viewport, keep w */
        float sx = (int16_t)(w1 >> 16) / 4.0f, sy = (int16_t)(w1 & 0xFFFF) / 4.0f, xs;
        if (pcgfx.vp_scale[0] != 0.0f)
            v->x = (sx - pcgfx.vp_trans[0]) / pcgfx.vp_scale[0] * v->w;
        if (pcgfx.vp_scale[1] != 0.0f)
            v->y = -(sy - pcgfx.vp_trans[1]) / pcgfx.vp_scale[1] * v->w;
        view_x(&xs);
        v->clip = outcode(v, xs);
        break;
    }
    case G_MWO_POINT_ZSCREEN:
        if (pcgfx.vp_scale[2] != 0.0f)
            v->z = ((float)w1 / 65536.0f - pcgfx.vp_trans[2]) / pcgfx.vp_scale[2] * v->w;
        break;
    }
}

/* ---------------------------------------------------------------- triangles */

static void vtx_lerp(GfxVtx *o, const GfxVtx *a, const GfxVtx *b, float t) {
    o->x = a->x + (b->x - a->x) * t;
    o->y = a->y + (b->y - a->y) * t;
    o->z = a->z + (b->z - a->z) * t;
    o->w = a->w + (b->w - a->w) * t;
    o->r = a->r + (b->r - a->r) * t;
    o->g = a->g + (b->g - a->g) * t;
    o->b = a->b + (b->b - a->b) * t;
    o->a = a->a + (b->a - a->a) * t;
    o->s = a->s + (b->s - a->s) * t;
    o->t = a->t + (b->t - a->t) * t;
    o->clip = 0;
}

static int clip_w(const GfxVtx *in, int n, GfxVtx *out) {
    int i, m = 0;
    for (i = 0; i < n; i++) {
        const GfxVtx *a = &in[i], *b = &in[(i + 1) % n];
        float da = a->w - GFX_W_EPS, db = b->w - GFX_W_EPS;
        if (da >= 0.0f)
            out[m++] = *a;
        if ((da >= 0.0f) != (db >= 0.0f))
            vtx_lerp(&out[m++], a, b, da / (da - db));
    }
    return m;
}

static void triangle(unsigned i0, unsigned i1, unsigned i2) {
    GfxVtx in[3], poly[8];
    GfxOutVtx out[18];
    const GfxDrawState *st;
    int n = 3, i, k, tw, th, tex;
    float flat[4], ax, ay, bx, by, vx, xs;
    uint32_t cull = pcgfx.geom & G_CULL_BOTH;

    if (i0 >= 32 || i1 >= 32 || i2 >= 32)
        return;
    in[0] = pcgfx.vtx[i0];
    in[1] = pcgfx.vtx[i1];
    in[2] = pcgfx.vtx[i2];
    if (in[0].clip & in[1].clip & in[2].clip)
        return;
    if (cull == G_CULL_BOTH)
        return;
    flat[0] = in[0].r;
    flat[1] = in[0].g;
    flat[2] = in[0].b;
    flat[3] = in[0].a;
    if ((in[0].clip | in[1].clip | in[2].clip) & 16) {
        n = clip_w(in, 3, poly);
        if (n < 3)
            return;
    } else {
        memcpy(poly, in, sizeof(in));
    }

    /* Cull on the screen-space winding (y down): front faces (counterclockwise with y up) have negative area,
     * and a mirrored viewport flips it, as on the RSP. */
    if (cull) {
        float area = 0.0f;
        for (i = 0; i < n; i++) {
            const GfxVtx *a = &poly[i], *b = &poly[(i + 1) % n];
            ax = pcgfx.vp_trans[0] + a->x / a->w * pcgfx.vp_scale[0];
            ay = pcgfx.vp_trans[1] - a->y / a->w * pcgfx.vp_scale[1];
            bx = pcgfx.vp_trans[0] + b->x / b->w * pcgfx.vp_scale[0];
            by = pcgfx.vp_trans[1] - b->y / b->w * pcgfx.vp_scale[1];
            area += ax * by - bx * ay;
        }
        if (area == 0.0f)
            return;
        if ((cull & G_CULL_BACK) && area > 0.0f)
            return;
        if ((cull & G_CULL_FRONT) && area < 0.0f)
            return;
    }

    vx = view_x(&xs);
    st = pcgfx_draw_state(-1, pcgfx.wide_ox != 0 && pcgfx.persp);
    pcgfx_target_dims(&tw, &th);
    tex = pcgfx.tex_on;
    for (i = 0; i < n; i++) {
        const GfxVtx *v = &poly[i];
        GfxOutVtx *o = &out[i];
        float d;
        if (pcgfx.om_l & G_ZS_PRIM)
            d = pcgfx.prim_z / 32767.0f;
        else
            d = screen_z(v) / 1023.0f;
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        o->pos[0] = v->w * (2.0f * vx / tw - 1.0f) + v->x * 2.0f * pcgfx.vp_scale[0] / tw;
        o->pos[1] = v->w * (1.0f - 2.0f * pcgfx.vp_trans[1] / th) + v->y * 2.0f * pcgfx.vp_scale[1] / th;
        o->pos[2] = (2.0f * d - 1.0f) * v->w;
        o->pos[3] = v->w;
        if (pcgfx.geom & G_SHADING_SMOOTH) {
            o->shade[0] = v->r;
            o->shade[1] = v->g;
            o->shade[2] = v->b;
            o->shade[3] = v->a;
        } else {
            memcpy(o->shade, flat, sizeof(flat));
        }
        for (k = 0; k < 2; k++) {
            if (tex && st->tex[k])
                pcgfx_tile_uv(k, v->s / 32.0f, v->t / 32.0f, &o->uv[2 * k], &o->uv[2 * k + 1]);
            else
                o->uv[2 * k] = o->uv[2 * k + 1] = 0.0f;
        }
    }
    {
        GfxOutVtx tri[18];
        int m = 0;
        for (i = 1; i + 1 < n; i++) {
            tri[m++] = out[0];
            tri[m++] = out[i];
            tri[m++] = out[i + 1];
        }
        pcgfx_emit(tri, m);
    }
    pcgfx.stat_tris++;
}

static void tri_word(uint32_t w) {
    triangle(((w >> 16) & 0xFF) >> 1, ((w >> 8) & 0xFF) >> 1, (w & 0xFF) >> 1);
}

/* ---------------------------------------------------------------- display list walk */

static void fetch(GfxDlPtr *d, uint32_t *w0, uintptr_t *w1) {
    if (d->be) {
        *w0 = pcgfx_rd32(d->p, 1);
        *w1 = pcgfx_rd32(d->p + 4, 1);
        d->p += 8;
    } else {
        const PcGfxWords *g = (const PcGfxWords *)(const void *)d->p;
        *w0 = g->w0;
        *w1 = g->w1;
        d->p += sizeof(PcGfxWords);
    }
}

static GfxDlPtr dl_at(uintptr_t addr) {
    GfxDlPtr d;
    d.p = pcgfx_resolve(addr);
    d.be = pcgfx_is_be(d.p);
    return d;
}

void pcgfx_rsp_reset(void) {
    int i;
    mtx_ident(pcgfx.mv[0]);
    pcgfx.mv_top = 0;
    mtx_ident(pcgfx.proj);
    pcgfx.mp_dirty = 1;
    pcgfx.geom = 0;
    pcgfx.num_lights = 1;
    memset(pcgfx.lights, 0, sizeof(pcgfx.lights));
    memset(pcgfx.lookat, 0, sizeof(pcgfx.lookat));
    pcgfx.lookat[0].dir[0] = 127;   /* the microcode's default lookat: x = +x, y = +y */
    pcgfx.lookat[1].dir[1] = 127;
    pcgfx.lights_dirty = 1;
    pcgfx.fog_mul = 0;
    pcgfx.fog_off = 0;
    pcgfx.tex_on = 0;
    pcgfx.tex_scale_s = pcgfx.tex_scale_t = 0xFFFF;
    for (i = 0; i < 16; i++)
        pcgfx.seg[i] = 0;
    /* A full-screen viewport until the list sets one. */
    pcgfx.vp_scale[0] = 160.0f;
    pcgfx.vp_scale[1] = 120.0f;
    pcgfx.vp_scale[2] = 511.0f;
    pcgfx.vp_trans[0] = 160.0f;
    pcgfx.vp_trans[1] = 120.0f;
    pcgfx.vp_trans[2] = 511.0f;
    memset(pcgfx.vtx, 0, sizeof(pcgfx.vtx));
    pcgfx.state_dirty = 1;
}

static void moveword(uint32_t w0, uintptr_t w1) {
    uint32_t off = w0 & 0xFFFF, v = (uint32_t)w1;
    switch ((w0 >> 16) & 0xFF) {
    case G_MW_NUMLIGHT:
        pcgfx.num_lights = (int)(v / 24);
        if (pcgfx.num_lights > GFX_MAX_LIGHTS)
            pcgfx.num_lights = GFX_MAX_LIGHTS;
        pcgfx.lights_dirty = 1;
        break;
    case G_MW_SEGMENT:
        pcgfx.seg[(off >> 2) & 0xF] = pcgfx_segment_value(w1);
        break;
    case G_MW_FOG:
        pcgfx.fog_mul = (int16_t)(v >> 16);
        pcgfx.fog_off = (int16_t)(v & 0xFFFF);
        break;
    case G_MW_LIGHTCOL: {
        unsigned n = off / 0x18;
        GfxLight *l;
        if (n > GFX_MAX_LIGHTS)
            break;
        l = &pcgfx.lights[n];
        if (off % 0x18 == 0) {
            l->col[0] = v >> 24;
            l->col[1] = v >> 16;
            l->col[2] = v >> 8;
        } else {
            l->colc[0] = v >> 24;
            l->colc[1] = v >> 16;
            l->colc[2] = v >> 8;
        }
        break;
    }
    case G_MW_MATRIX:
        mp_insert(off, v);
        break;
    default:   /* G_MW_CLIP, G_MW_PERSPNORM, G_MW_FORCEMTX: nothing to emulate */
        break;
    }
}

static void movemem(uint32_t w0, uintptr_t w1) {
    const uint8_t *p = pcgfx_resolve(w1);
    unsigned ofs = ((w0 >> 8) & 0xFF) * 8;
    int be;
    if (p == NULL)
        return;
    be = pcgfx_is_be(p);
    switch (w0 & 0xFF) {
    case G_MV_VIEWPORT:
        pcgfx.vp_scale[0] = (int16_t)pcgfx_rd16(p + 0, be) / 4.0f;
        pcgfx.vp_scale[1] = (int16_t)pcgfx_rd16(p + 2, be) / 4.0f;
        pcgfx.vp_scale[2] = (int16_t)pcgfx_rd16(p + 4, be);
        pcgfx.vp_trans[0] = (int16_t)pcgfx_rd16(p + 8, be) / 4.0f;
        pcgfx.vp_trans[1] = (int16_t)pcgfx_rd16(p + 10, be) / 4.0f;
        pcgfx.vp_trans[2] = (int16_t)pcgfx_rd16(p + 12, be);
        break;
    case G_MV_LIGHT: {
        unsigned n = ofs / 24;   /* 0, 1: lookat x, y; 2..: lights */
        if (n < 2)
            light_read(p, &pcgfx.lookat[n]);
        else if (n - 2 <= GFX_MAX_LIGHTS)
            light_read(p, &pcgfx.lights[n - 2]);
        pcgfx.lights_dirty = 1;
        break;
    }
    case G_MV_MATRIX:   /* gSPForceMatrix: load MP directly */
        mtx_read(p, be, pcgfx.mp);
        pcgfx.persp = is_persp(pcgfx.mp);
        pcgfx.mp_dirty = 0;
        break;
    default:
        break;
    }
}

static void matrix(uint32_t w0, uintptr_t w1) {
    const uint8_t *p = pcgfx_resolve(w1);
    uint32_t params = (w0 & 0xFF) ^ G_MTX_PUSH;
    float m[4][4];
    if (p == NULL)
        return;
    mtx_read(p, pcgfx_is_be(p), m);
    if (params & G_MTX_PROJECTION) {
        if (params & G_MTX_LOAD)
            memcpy(pcgfx.proj, m, sizeof(m));
        else
            mtx_mul(pcgfx.proj, m, pcgfx.proj);
    } else {
        if ((params & G_MTX_PUSH) && pcgfx.mv_top + 1 < GFX_MTX_STACK) {
            memcpy(pcgfx.mv[pcgfx.mv_top + 1], pcgfx.mv[pcgfx.mv_top], sizeof(m));
            pcgfx.mv_top++;
        }
        if (params & G_MTX_LOAD)
            memcpy(pcgfx.mv[pcgfx.mv_top], m, sizeof(m));
        else
            mtx_mul(pcgfx.mv[pcgfx.mv_top], m, pcgfx.mv[pcgfx.mv_top]);
        pcgfx.lights_dirty = 1;
    }
    pcgfx.mp_dirty = 1;
}

/* G_CULLDL: true when every vertex in [v0, v1] is outside the same screen edge. */
static int cull_dl(uint32_t w0, uint32_t w1) {
    unsigned v0 = (w0 & 0xFFFF) >> 1, v1 = (w1 & 0xFFFF) >> 1, i;
    uint8_t all = 0x0F;
    if (v1 >= 32)
        v1 = 31;
    for (i = v0; i <= v1; i++)
        all &= pcgfx.vtx[i].clip;
    return all != 0;
}

void pcgfx_run_list(const void *dl) {
    GfxDlPtr stack[GFX_DL_STACK];
    GfxDlPtr cur;
    int sp = 0;
    uint32_t budget = 1u << 22;
    static int warned_ucode;

    pcgfx_rsp_reset();
    cur.p = (const uint8_t *)dl;
    cur.be = pcgfx_is_be(dl);
    while (cur.p != NULL && budget-- > 0) {
        uint32_t w0, op;
        uintptr_t w1;
        fetch(&cur, &w0, &w1);
        op = w0 >> 24;
        pcgfx.stat_cmds++;
        switch (op) {
        case G_NOOP:
        case G_SPNOOP:
        case G_DMA_IO:
        case G_SPECIAL_1:
        case G_SPECIAL_2:
        case G_SPECIAL_3:
            break;
        case G_VTX: {
            unsigned n = (w0 >> 12) & 0xFF, end = (w0 >> 1) & 0x7F;
            const uint8_t *p = pcgfx_resolve(w1);
            if (p != NULL && n <= end)
                load_vertices(p, pcgfx_is_be(p), (int)(end - n), (int)n);
            break;
        }
        case G_MODIFYVTX:
            modify_vertex(w0, (uint32_t)w1);
            break;
        case G_CULLDL:
            if (!cull_dl(w0, (uint32_t)w1))
                break;
            /* fall through - a culled list ends here */
        case G_ENDDL:
            if (sp == 0)
                cur.p = NULL;
            else
                cur = stack[--sp];
            break;
        case G_BRANCH_Z: {
            unsigned idx = (w0 & 0xFFF) >> 1;
            if (idx < 32 && screen_z(&pcgfx.vtx[idx]) * 65536.0f <= (float)(int32_t)(uint32_t)w1) {
                GfxDlPtr t = dl_at(pcgfx.half1);
                if (t.p != NULL)
                    cur = t;
            }
            break;
        }
        case G_TRI1:
            tri_word(w0);
            break;
        case G_TRI2:
        case G_QUAD:
            tri_word(w0);
            tri_word((uint32_t)w1);
            break;
        case G_TEXTURE:
            pcgfx.tex_scale_s = (uint16_t)(w1 >> 16);
            pcgfx.tex_scale_t = (uint16_t)(w1 & 0xFFFF);
            pcgfx.tex_level = (w0 >> 11) & 7;
            pcgfx.tex_tile = (w0 >> 8) & 7;
            pcgfx.tex_on = ((w0 >> 1) & 0x7F) != 0;
            pcgfx.tex_dirty = pcgfx.state_dirty = 1;
            break;
        case G_POPMTX: {
            int n = (int)((uint32_t)w1 / 64);
            pcgfx.mv_top = pcgfx.mv_top > n ? pcgfx.mv_top - n : 0;
            pcgfx.mp_dirty = pcgfx.lights_dirty = 1;
            break;
        }
        case G_GEOMETRYMODE:
            pcgfx.geom = (pcgfx.geom & (w0 & 0x00FFFFFF)) | (uint32_t)w1;
            pcgfx.state_dirty = 1;
            break;
        case G_MTX:
            matrix(w0, w1);
            break;
        case G_MOVEWORD:
            moveword(w0, w1);
            break;
        case G_MOVEMEM:
            movemem(w0, w1);
            break;
        case G_LOAD_UCODE:
            if (!warned_ucode++)
                fprintf(stderr, "gfx: G_LOAD_UCODE ignored (F3DEX2 only)\n");
            break;
        case G_DL: {
            GfxDlPtr t = dl_at(w1);
            if (t.p == NULL)
                break;
            if (((w0 >> 16) & 0xFF) == 0) {   /* G_DL_PUSH */
                if (sp >= GFX_DL_STACK)
                    break;   /* the RSP would overrun its stack; drop the call */
                stack[sp++] = cur;
            }
            cur = t;
            break;
        }
        case G_RDPHALF_1:
            pcgfx.half1 = w1;
            break;
        case G_RDPHALF_2:
            pcgfx.half2 = (uint32_t)w1;
            break;
        case G_TEXRECT:
        case G_TEXRECTFLIP: {
            /* F3DEX2 sends the rectangle's second half in the next two commands (RDPHALF_1, RDPHALF_2). */
            uint32_t h1 = (uint32_t)pcgfx.half1, h2 = pcgfx.half2, a0;
            uintptr_t a1;
            GfxDlPtr peek = cur;
            fetch(&peek, &a0, &a1);
            if ((a0 >> 24) == G_RDPHALF_1) {
                h1 = (uint32_t)a1;
                cur = peek;
                fetch(&peek, &a0, &a1);
                if ((a0 >> 24) == G_RDPHALF_2) {
                    h2 = (uint32_t)a1;
                    cur = peek;
                }
            }
            pcgfx_texrect(w0, (uint32_t)w1, h1, h2, op == G_TEXRECTFLIP);
            break;
        }
        default:
            if (op >= 0xE2)
                pcgfx_rdp_command(op, w0, w1);
            else
                pcgfx.stat_unknown++;
            break;
        }
    }
    pcgfx_flush();
}
