/* port/gfx/gfx_rdp.c: the RDP half of the interpreter. Other modes, colors, images, scissor; a 4 KB TMEM fed by
 * LOADBLOCK/LOADTILE/LOADTLUT with the hardware's odd-line word swap; textures decoded from TMEM through the tile
 * descriptors and cached by content; rectangles; the draw state and triangle batching. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

#define GFX_BATCH_MAX (3 * 2048)
#define TEX_BUCKETS 4096
#define TEX_MAX_ENTRIES 3072
#define TEX_MAX_DIM 1024

static GfxOutVtx s_batch[GFX_BATCH_MAX];
static int s_batch_n;
static GfxDrawState s_state;       /* state of the batch being built */
static int s_state_valid;
static int s_state_rect_tile = -2;
static uint32_t s_frame;

static int host_is_le(void) {
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 1;
}

/* ---------------------------------------------------------------- color images and targets */

static GfxTarget *s_target;   /* the color image's render target */

static int frame_height(int width) {
    return width * 3 / 4;   /* the VI shows 4:3; 320 -> 240 */
}

void gfx_target_dims(int *w, int *h) {
    int width = g_gfx.cimg.width ? g_gfx.cimg.width : 320;
    *w = width;
    *h = frame_height(width);
}

static GfxTarget *current_target(void) {
    int w, h;
    if (s_target == NULL) {
        gfx_target_dims(&w, &h);
        s_target = gfx_gl_target(g_gfx.cimg.addr, w, h);
    }
    return s_target;
}

/* ---------------------------------------------------------------- TMEM */

static uint16_t tmem16(uint32_t a) {
    uint16_t v;
    a &= 0xFFE;
    v = (uint16_t)(g_gfx.tmem[a] << 8 | g_gfx.tmem[a + 1]);
    if (g_gfx.tmem_le16[a >> 3])
        v = (uint16_t)(v >> 8 | v << 8);
    return v;
}

static void tmem_put(uint32_t a, uint8_t v, uint8_t le16) {
    a &= 0xFFF;
    g_gfx.tmem[a] = v;
    g_gfx.tmem_le16[a >> 3] = le16;
}

static unsigned bpp_x2(unsigned siz) {   /* bytes per texel, times two */
    return 1u << siz;
}

static void load_block(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &g_gfx.tiles[(w1 >> 24) & 7];
    unsigned uls = (w0 >> 12) & 0xFFF, ult = w0 & 0xFFF, lrs = (w1 >> 12) & 0xFFF, dxt = w1 & 0xFFF;
    unsigned siz = g_gfx.timg.siz, texels, dst = tile->tmem * 8u, i;
    const uint8_t *src;
    int be;
    if (g_gfx.timg.addr == NULL || lrs < uls)
        return;
    texels = lrs - uls + 1;
    src = g_gfx.timg.addr + (((size_t)ult * g_gfx.timg.width + uls) * bpp_x2(siz)) / 2;
    be = gfx_is_be(src) || !host_is_le();
    if (siz == G_IM_SIZ_32b) {
        /* RG to the low half, BA to the high half, two bytes per texel each */
        for (i = 0; i < texels && i < 1024; i++) {
            unsigned row = (((i >> 1) * dxt) >> 11) & 1;
            uint32_t a = ((dst + i * 2) ^ (row ? 4u : 0u)) & 0x7FF;
            uint8_t r, g, b, al;
            if (be) {
                r = src[4 * i];
                g = src[4 * i + 1];
                b = src[4 * i + 2];
                al = src[4 * i + 3];
            } else {
                uint32_t v = gfx_rd32(src + 4 * i, 0);
                r = v >> 24;
                g = v >> 16;
                b = v >> 8;
                al = v;
            }
            tmem_put(a, r, 0);
            tmem_put(a + 1, g, 0);
            tmem_put(a + 0x800, b, 0);
            tmem_put(a + 0x801, al, 0);
        }
    } else {
        unsigned bytes = (texels * bpp_x2(siz) + 1) / 2, words = (bytes + 7) / 8, w, k;
        uint8_t le16 = (!be && siz == G_IM_SIZ_16b) ? 1 : 0;
        if (words > 512)
            words = 512;
        for (w = 0; w < words; w++) {
            unsigned swap = ((w * dxt) >> 11) & 1 ? 4u : 0u;
            for (k = 0; k < 8; k++)
                tmem_put((dst + w * 8 + (k ^ swap)), src[w * 8 + k], le16);
        }
    }
    g_gfx.tmem_gen++;
    g_gfx.tex_dirty = g_gfx.state_dirty = 1;
}

static void load_tile(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &g_gfx.tiles[(w1 >> 24) & 7];
    unsigned sl = ((w0 >> 12) & 0xFFF) >> 2, tl = (w0 & 0xFFF) >> 2;
    unsigned sh = ((w1 >> 12) & 0xFFF) >> 2, th = (w1 & 0xFFF) >> 2;
    unsigned siz = g_gfx.timg.siz, line = tile->line * 8u, dst = tile->tmem * 8u, r, i, w, h;
    int be;
    if (g_gfx.timg.addr == NULL || sh < sl || th < tl)
        return;
    w = sh - sl + 1;
    h = th - tl + 1;
    be = gfx_is_be(g_gfx.timg.addr) || !host_is_le();
    for (r = 0; r < h && r < 1024; r++) {
        const uint8_t *src = g_gfx.timg.addr + (((size_t)(tl + r) * g_gfx.timg.width + sl) * bpp_x2(siz)) / 2;
        unsigned swap = (r & 1) ? 4u : 0u, base = dst + r * line;
        if (siz == G_IM_SIZ_32b) {
            for (i = 0; i < w; i++) {
                uint32_t a = ((base + i * 2) ^ swap) & 0x7FF;
                uint32_t v = be ? gfx_rd32(src + 4 * i, 1) : gfx_rd32(src + 4 * i, 0);
                tmem_put(a, v >> 24, 0);
                tmem_put(a + 1, v >> 16, 0);
                tmem_put(a + 0x800, v >> 8, 0);
                tmem_put(a + 0x801, v, 0);
            }
        } else {
            unsigned bytes = (w * bpp_x2(siz) + 1) / 2;
            uint8_t le16 = (!be && siz == G_IM_SIZ_16b) ? 1 : 0;
            for (i = 0; i < bytes; i++)
                tmem_put((base + i) ^ swap, src[i], le16);
        }
    }
    g_gfx.tmem_gen++;
    g_gfx.tex_dirty = g_gfx.state_dirty = 1;
}

/* Each palette entry is stored four times, 8 bytes apart per entry, as the hardware's four TLUT banks. */
static void load_tlut(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &g_gfx.tiles[(w1 >> 24) & 7];
    unsigned sl = ((w0 >> 12) & 0xFFF) >> 2, tl = (w0 & 0xFFF) >> 2, sh = ((w1 >> 12) & 0xFFF) >> 2;
    unsigned dst = tile->tmem * 8u, i, k, n;
    const uint8_t *src;
    int be;
    if (g_gfx.timg.addr == NULL || sh < sl)
        return;
    n = sh - sl + 1;
    if (n > 256)
        n = 256;
    src = g_gfx.timg.addr + ((size_t)tl * g_gfx.timg.width + sl) * 2;
    be = gfx_is_be(src);
    for (i = 0; i < n; i++) {
        uint16_t v = gfx_rd16(src + 2 * i, be);
        for (k = 0; k < 4; k++) {
            tmem_put(dst + i * 8 + k * 2, v >> 8, 0);
            tmem_put(dst + i * 8 + k * 2 + 1, v & 0xFF, 0);
        }
    }
    g_gfx.tmem_gen++;
    g_gfx.tex_dirty = g_gfx.state_dirty = 1;
}

/* ---------------------------------------------------------------- texture decode */

static void rgba16_px(uint16_t c, uint8_t *o) {
    unsigned r = (c >> 11) & 31, g = (c >> 6) & 31, b = (c >> 1) & 31;
    o[0] = (uint8_t)(r << 3 | r >> 2);
    o[1] = (uint8_t)(g << 3 | g >> 2);
    o[2] = (uint8_t)(b << 3 | b >> 2);
    o[3] = (c & 1) ? 255 : 0;
}

static void ia16_px(uint16_t c, uint8_t *o) {
    o[0] = o[1] = o[2] = (uint8_t)(c >> 8);
    o[3] = (uint8_t)c;
}

static void i_px(unsigned i, unsigned a, uint8_t *o) {
    o[0] = o[1] = o[2] = (uint8_t)i;
    o[3] = (uint8_t)a;
}

/* One texel at tile coordinates (s, t), already clamped and masked. */
static void texel(const GfxTile *tl, unsigned tlut, unsigned s, unsigned t, uint8_t *o) {
    uint32_t row = tl->tmem * 8u + t * tl->line * 8u, swap = (t & 1) ? 4u : 0u, v = 0, a;
    unsigned siz = tl->siz;
    switch (siz) {
    case G_IM_SIZ_4b:
        v = g_gfx.tmem[((row + (s >> 1)) ^ swap) & 0xFFF];
        v = (s & 1) ? (v & 0xF) : (v >> 4);
        break;
    case G_IM_SIZ_8b:
        v = g_gfx.tmem[((row + s) ^ swap) & 0xFFF];
        break;
    case G_IM_SIZ_16b:
        v = tmem16((row + s * 2) ^ swap);
        break;
    default:
        a = ((row + s * 2) ^ swap) & 0x7FF;
        o[0] = g_gfx.tmem[a];
        o[1] = g_gfx.tmem[a + 1];
        o[2] = g_gfx.tmem[a + 0x800];
        o[3] = g_gfx.tmem[a + 0x801];
        return;
    }
    if (siz <= G_IM_SIZ_8b && (tlut & 2)) {   /* palette: TLUT enabled turns any 4b/8b texel into an index */
        uint32_t idx = siz == G_IM_SIZ_4b ? ((uint32_t)tl->palette << 4 | v) : v;
        uint16_t e = tmem16(0x800 + idx * 8);
        if (tlut & 1)
            ia16_px(e, o);
        else
            rgba16_px(e, o);
        return;
    }
    switch (tl->fmt << 2 | siz) {
    case G_IM_FMT_RGBA << 2 | G_IM_SIZ_16b:
        rgba16_px((uint16_t)v, o);
        break;
    case G_IM_FMT_IA << 2 | G_IM_SIZ_16b:
    case G_IM_FMT_I << 2 | G_IM_SIZ_16b:
        ia16_px((uint16_t)v, o);
        break;
    case G_IM_FMT_IA << 2 | G_IM_SIZ_8b:
        i_px((v >> 4) * 17, (v & 15) * 17, o);
        break;
    case G_IM_FMT_IA << 2 | G_IM_SIZ_4b:
        i_px((v >> 1) * 255 / 7, (v & 1) ? 255 : 0, o);
        break;
    case G_IM_FMT_I << 2 | G_IM_SIZ_4b:
    case G_IM_FMT_CI << 2 | G_IM_SIZ_4b:
    case G_IM_FMT_RGBA << 2 | G_IM_SIZ_4b:
        i_px(v * 17, v * 17, o);
        break;
    default:   /* I8, CI8 without a TLUT, RGBA8, YUV16: intensity */
        if (siz == G_IM_SIZ_16b)
            i_px(v >> 8, 255, o);
        else
            i_px(v, v, o);
        break;
    }
}

static unsigned map_coord(unsigned c, int clamp, unsigned size, unsigned mask, int mirror) {
    if (clamp && c > size - 1)
        c = size - 1;
    if (mask) {
        if (mirror && ((c >> mask) & 1))
            c = ~c;
        c &= (1u << mask) - 1;
    }
    return c;
}

typedef struct {
    uint8_t fmt, siz, palette, tlut, cms, cmt, masks, maskt;
    uint16_t line, tmem, tw, th;
} TexKey;

typedef struct TexEntry {
    TexKey key;
    uint64_t hash;
    uint32_t gl;
    int w, h, repeat_s, repeat_t;
    uint32_t last_use;
    struct TexEntry *next;
} TexEntry;

static TexEntry *s_tex_buckets[TEX_BUCKETS];
static int s_tex_count;
static uint8_t *s_decode;
/* The last lookup per texture unit, valid while no tile, TMEM or TLUT-mode change happened (epoch). */
static uint32_t s_tex_epoch = 1;
static struct {
    int tile;
    uint32_t epoch;
    TexEntry *e;
} s_unit_cache[2];

static uint64_t mix64(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    return h * 0xFF51AFD7ED558CCDull;
}

static uint64_t hash_tmem(uint64_t h, uint32_t start, uint32_t len) {
    uint32_t i;
    if (len > 4096)
        len = 4096;
    for (i = 0; i < len; i += 8) {
        uint32_t a = (start + i) & 0xFF8;
        uint64_t v;
        memcpy(&v, &g_gfx.tmem[a], 8);
        h = mix64(h, v ^ ((uint64_t)g_gfx.tmem_le16[a >> 3] << 63));
    }
    return h;
}

static void tex_evict(void) {
    int b;
    s_tex_epoch++;
    for (b = 0; b < TEX_BUCKETS; b++) {
        TexEntry **pp = &s_tex_buckets[b];
        while (*pp) {
            TexEntry *e = *pp;
            if (s_frame - e->last_use > 2) {
                *pp = e->next;
                gfx_gl_delete_texture(e->gl);
                free(e);
                s_tex_count--;
            } else {
                pp = &e->next;
            }
        }
    }
}

static TexEntry *tex_lookup(int tile_idx) {
    const GfxTile *tl = &g_gfx.tiles[tile_idx & 7];
    TexKey k;
    TexEntry *e;
    uint64_t h;
    unsigned W, H, x, y, tw, th, bytes, line;
    int clamp_s, clamp_t;
    memset(&k, 0, sizeof(k));
    k.fmt = tl->fmt;
    k.siz = tl->siz;
    k.palette = tl->palette;
    k.tlut = (g_gfx.om_h >> G_MDSFT_TEXTLUT) & 3;
    k.cms = tl->cms;
    k.cmt = tl->cmt;
    k.masks = tl->masks > 10 ? 10 : tl->masks;
    k.maskt = tl->maskt > 10 ? 10 : tl->maskt;
    k.line = tl->line;
    k.tmem = tl->tmem;
    tw = tl->lrs >= tl->uls ? (tl->lrs >> 2) - (tl->uls >> 2) + 1 : 1;
    th = tl->lrt >= tl->ult ? (tl->lrt >> 2) - (tl->ult >> 2) + 1 : 1;
    if (tw > TEX_MAX_DIM) tw = TEX_MAX_DIM;
    if (th > TEX_MAX_DIM) th = TEX_MAX_DIM;
    k.tw = (uint16_t)tw;
    k.th = (uint16_t)th;

    /* Decoded size: the tile when it clamps (mask/mirror baked in), one period when it wraps. */
    clamp_s = (k.cms & G_TX_CLAMP) || k.masks == 0;
    clamp_t = (k.cmt & G_TX_CLAMP) || k.maskt == 0;
    W = clamp_s ? tw : (1u << k.masks) << ((k.cms & G_TX_MIRROR) ? 1 : 0);
    H = clamp_t ? th : (1u << k.maskt) << ((k.cmt & G_TX_MIRROR) ? 1 : 0);
    if (W > TEX_MAX_DIM) W = TEX_MAX_DIM;
    if (H > TEX_MAX_DIM) H = TEX_MAX_DIM;

    h = mix64(0x1234567, 0);
    {
        const uint8_t *kb = (const uint8_t *)&k;
        for (x = 0; x < sizeof(k); x++)
            h = mix64(h, kb[x]);
    }
    line = k.line * 8u;
    {
        unsigned rows = clamp_t ? th : (1u << k.maskt);
        unsigned cols = clamp_s ? tw : (1u << k.masks);
        if (!clamp_t && th > rows) rows = th;
        bytes = (rows ? rows - 1 : 0) * line + (cols * bpp_x2(k.siz) + 1) / 2 + 8;
        if (k.siz == G_IM_SIZ_32b) {
            h = hash_tmem(h, k.tmem * 8u, bytes > 2048 ? 2048 : bytes);
            h = hash_tmem(h, k.tmem * 8u + 0x800, bytes > 2048 ? 2048 : bytes);
        } else {
            h = hash_tmem(h, k.tmem * 8u, bytes);
        }
        if (k.siz <= G_IM_SIZ_8b && (k.tlut & 2)) {
            if (k.siz == G_IM_SIZ_4b)
                h = hash_tmem(h, 0x800 + k.palette * 128u, 128);
            else
                h = hash_tmem(h, 0x800, 2048);
        }
    }

    for (e = s_tex_buckets[h % TEX_BUCKETS]; e != NULL; e = e->next)
        if (e->hash == h && memcmp(&e->key, &k, sizeof(k)) == 0) {
            e->last_use = s_frame;
            return e;
        }

    if (s_tex_count >= TEX_MAX_ENTRIES)
        tex_evict();
    if (s_decode == NULL)
        s_decode = (uint8_t *)malloc(TEX_MAX_DIM * TEX_MAX_DIM * 4);
    for (y = 0; y < H; y++) {
        unsigned t = map_coord(y, clamp_t, th, k.maskt, k.cmt & G_TX_MIRROR);
        for (x = 0; x < W; x++) {
            unsigned s = map_coord(x, clamp_s, tw, k.masks, k.cms & G_TX_MIRROR);
            texel(tl, k.tlut, s, t, s_decode + (y * W + x) * 4);
        }
    }
    e = (TexEntry *)calloc(1, sizeof(*e));
    e->key = k;
    e->hash = h;
    e->w = (int)W;
    e->h = (int)H;
    e->repeat_s = !clamp_s;
    e->repeat_t = !clamp_t;
    e->gl = gfx_gl_texture(s_decode, (int)W, (int)H);
    e->last_use = s_frame;
    e->next = s_tex_buckets[h % TEX_BUCKETS];
    s_tex_buckets[h % TEX_BUCKETS] = e;
    s_tex_count++;
    return e;
}

void gfx_tex_cache_clear(void) {
    int b;
    for (b = 0; b < TEX_BUCKETS; b++) {
        while (s_tex_buckets[b]) {
            TexEntry *e = s_tex_buckets[b];
            s_tex_buckets[b] = e->next;
            gfx_gl_delete_texture(e->gl);
            free(e);
        }
    }
    s_tex_count = 0;
    s_tex_epoch++;
    free(s_decode);
    s_decode = NULL;
}

/* ---------------------------------------------------------------- texture coordinates */

typedef struct {
    int tile;
    float uls, ult;   /* texels */
    int shifts, shiftt;
    float w, h;
    int linear;
} UnitInfo;

static UnitInfo s_unit[2];

static float tile_shift(float c, int sh) {
    if (sh == 0)
        return c;
    if (sh <= 10)
        return c / (float)(1 << sh);
    return c * (float)(1 << (16 - sh));
}

void gfx_tile_uv(int unit, float s, float t, float *u, float *v) {
    const UnitInfo *ui = &s_unit[unit];
    s = tile_shift(s, ui->shifts) - ui->uls;
    t = tile_shift(t, ui->shiftt) - ui->ult;
    if (ui->linear) {   /* bilinear centers texels on integer coordinates */
        s += 0.5f;
        t += 0.5f;
    }
    *u = s / ui->w;
    *v = t / ui->h;
}

/* ---------------------------------------------------------------- draw state */

static void color4(const uint8_t *c, float *o) {
    o[0] = c[0] / 255.0f;
    o[1] = c[1] / 255.0f;
    o[2] = c[2] / 255.0f;
    o[3] = c[3] / 255.0f;
}

static void fill_rgba(uint32_t fc, float *o) {
    if (g_gfx.cimg.siz == G_IM_SIZ_32b) {
        o[0] = (fc >> 24) / 255.0f;
        o[1] = ((fc >> 16) & 0xFF) / 255.0f;
        o[2] = ((fc >> 8) & 0xFF) / 255.0f;
        o[3] = (fc & 0xFF) / 255.0f;
    } else {
        uint8_t px[4];
        rgba16_px((uint16_t)(fc >> 16), px);
        color4(px, o);
        o[3] = 1.0f;
    }
}

static unsigned cycle_type(void) {
    return (g_gfx.om_h >> G_MDSFT_CYCLETYPE) & 3;
}

static void build_state(GfxDrawState *st, int rect_tile) {
    GfxShaderKey key;
    unsigned cyc = cycle_type(), u;
    uint32_t oml = g_gfx.om_l;
    int tile = rect_tile >= 0 ? rect_tile : g_gfx.tex_tile;

    memset(st, 0, sizeof(*st));
    memset(&key, 0, sizeof(key));
    key.cc_w0 = g_gfx.cc_w0 & 0x00FFFFFF;
    key.cc_w1 = g_gfx.cc_w1;
    key.blender = (uint16_t)(oml >> 16);
    key.mode = cyc == G_CYC_1CYCLE ? GFX_MODE_1CYC : cyc == G_CYC_2CYCLE ? GFX_MODE_2CYC
             : cyc == G_CYC_COPY ? GFX_MODE_COPY : GFX_MODE_FILL;
    key.ac = (uint8_t)(oml & 3);
    if (key.mode == GFX_MODE_COPY)
        key.ac = key.ac ? 4 : 0;
    if (oml & FORCE_BL) key.flags |= GFX_KF_FORCE_BL;
    if (oml & ALPHA_CVG_SEL) key.flags |= GFX_KF_ALPHA_CVG_SEL;
    if (oml & CVG_X_ALPHA) key.flags |= GFX_KF_CVG_X_ALPHA;
    if (key.mode != GFX_MODE_1CYC && key.mode != GFX_MODE_2CYC) {   /* only these use the combiner/blender */
        key.cc_w0 = key.cc_w1 = 0;
        key.blender = 0;
        key.flags = 0;
    }

    /* Textures */
    for (u = 0; u < 2; u++) {
        int want = key.mode == GFX_MODE_COPY ? u == 0
                 : (key.mode == GFX_MODE_FILL ? 0 : gfx_combine_uses(&key, (int)u));
        if (rect_tile < 0 && !g_gfx.tex_on)
            want = want && 0;
        if (want) {
            int ti = (tile + (int)u) & 7;
            const GfxTile *tl = &g_gfx.tiles[ti];
            TexEntry *e;
            if (s_unit_cache[u].tile == ti && s_unit_cache[u].epoch == s_tex_epoch) {
                e = s_unit_cache[u].e;
                e->last_use = s_frame;
            } else {
                e = tex_lookup(ti);
                s_unit_cache[u].tile = ti;
                s_unit_cache[u].epoch = s_tex_epoch;
                s_unit_cache[u].e = e;
            }
            unsigned filt = (g_gfx.om_h >> G_MDSFT_TEXTFILT) & 3;
            st->tex[u] = e->gl;
            st->repeat_s[u] = (uint8_t)e->repeat_s;
            st->repeat_t[u] = (uint8_t)e->repeat_t;
            st->linear[u] = key.mode != GFX_MODE_COPY && filt != G_TF_POINT;
            s_unit[u].tile = ti;
            s_unit[u].uls = tl->uls / 4.0f;
            s_unit[u].ult = tl->ult / 4.0f;
            s_unit[u].shifts = tl->shifts;
            s_unit[u].shiftt = tl->shiftt;
            s_unit[u].w = (float)e->w;
            s_unit[u].h = (float)e->h;
            s_unit[u].linear = st->linear[u];
            key.flags |= u == 0 ? GFX_KF_TEX0 : GFX_KF_TEX1;
        }
    }

    {
        int scale;
        gfx_gl_target_size(&scale);
        st->scale = (float)scale;
        if (rect_tile >= 0 && rect_tile < 8 && scale > 1 &&
            (key.mode == GFX_MODE_COPY || ((g_gfx.om_h >> G_MDSFT_TEXTFILT) & 3) == G_TF_POINT))
            key.flags |= GFX_KF_SNAP;
    }
    st->shader = gfx_gl_shader(&key);
    st->shader_flags = key.flags;
    st->target = current_target();

    /* Depth. Triangles need G_ZBUFFER for z (or the primitive depth); rectangles only have the primitive depth. */
    if (key.mode == GFX_MODE_1CYC || key.mode == GFX_MODE_2CYC) {
        int has_z = rect_tile >= 0 ? (oml & G_ZS_PRIM) != 0 : ((g_gfx.geom & G_ZBUFFER) || (oml & G_ZS_PRIM));
        int cmp = has_z && (oml & Z_CMP), upd = has_z && (oml & Z_UPD);
        st->depth_test = cmp ? 1 : upd ? 2 : 0;   /* 2: always pass, write only */
        st->depth_write = (uint8_t)(upd != 0);
        st->decal = (oml & ZMODE_MASK) == ZMODE_DEC;
        st->blend = (uint8_t)gfx_blend_uses_mem(&key);
    }

    /* Scissor, N64 pixels, exclusive */
    st->scissor[0] = g_gfx.sc_ulx >> 2;
    st->scissor[1] = g_gfx.sc_uly >> 2;
    st->scissor[2] = g_gfx.sc_lrx >> 2;
    st->scissor[3] = g_gfx.sc_lry >> 2;

    color4(g_gfx.prim, st->prim);
    color4(g_gfx.env, st->env);
    color4(g_gfx.fog, st->fog);
    color4(g_gfx.blend, st->blendc);
    fill_rgba(g_gfx.fill_color, st->fill);
    st->prim_lod = g_gfx.prim_lod / 255.0f;
    st->lod_frac = 0.0f;   /* no LOD computation: see GFX.md */
    st->noise_seed = (float)(s_frame & 255);
}

const GfxDrawState *gfx_draw_state(int rect_tile) {
    GfxDrawState st;
    if (s_state_valid && !g_gfx.state_dirty && !g_gfx.tex_dirty && s_state_rect_tile == rect_tile)
        return &s_state;
    if (g_gfx.tex_dirty)
        s_tex_epoch++;
    build_state(&st, rect_tile);
    g_gfx.state_dirty = 0;
    g_gfx.tex_dirty = 0;
    s_state_rect_tile = rect_tile;
    if (!s_state_valid || memcmp(&st, &s_state, sizeof(st)) != 0) {
        gfx_flush();
        s_state = st;
        s_state_valid = 1;
    }
    return &s_state;
}

void gfx_emit(const GfxOutVtx *v, int n) {
    if (s_batch_n + n > GFX_BATCH_MAX)
        gfx_flush();
    memcpy(&s_batch[s_batch_n], v, sizeof(*v) * (size_t)n);
    s_batch_n += n;
}

void gfx_flush(void) {
    if (s_batch_n > 0 && s_state_valid) {
        gfx_gl_draw(&s_state, s_batch, s_batch_n);
        g_gfx.stat_draws++;
    }
    s_batch_n = 0;
}

void gfx_rdp_new_frame(void) {
    s_frame++;
    g_gfx.state_dirty = 1;
}

void gfx_rdp_forget_targets(void) {
    s_target = NULL;
    s_state_valid = 0;
}

/* ---------------------------------------------------------------- rectangles */

static void rect_vertex(GfxOutVtx *o, float x, float y, float s, float t, int textured, int tw, int th) {
    float d = (g_gfx.om_l & G_ZS_PRIM) ? g_gfx.prim_z / 32767.0f : 0.0f;
    int k;
    o->pos[0] = x * 2.0f / tw - 1.0f;
    o->pos[1] = 1.0f - y * 2.0f / th;
    o->pos[2] = 2.0f * d - 1.0f;
    o->pos[3] = 1.0f;
    o->shade[0] = o->shade[1] = o->shade[2] = o->shade[3] = 0.0f;   /* rectangles have no shade */
    for (k = 0; k < 2; k++) {
        if (textured && s_state.tex[k])
            gfx_tile_uv(k, s, t, &o->uv[2 * k], &o->uv[2 * k + 1]);
        else
            o->uv[2 * k] = o->uv[2 * k + 1] = 0.0f;
    }
}

/* The RDP samples a rectangle pixel at its corner: pixel k gets texel floor(c0 + k * d). GL samples at (internal)
 * pixel centers, so start the interpolation so that each of the scale x scale samples inside N64 pixel k stays in
 * that texel (exact for |d| <= 1), plus a small bias against float error on texel boundaries. */
static float rect_start(float c0, float d, int scale, int snap) {
    if (d >= 0.0f || snap)   /* snap: the shader moves every sample to the first one of its N64 pixel */
        return c0 - 0.5f * d / scale + 1.0f / 64;
    return c0 - d + 0.5f * d / scale + 1.0f / 64;
}

/* Corners in N64 pixels; s/t at (x0, y0) for the pixel's corner, per-pixel steps. */
static void draw_rect(float x0, float y0, float x1, float y1, int tile, float s, float t, float dsdx, float dtdy,
                      int flip) {
    GfxOutVtx q[4], tri[6];
    int tw, th, scale, textured = tile >= 0;
    float w = x1 - x0, h = y1 - y0;
    gfx_draw_state(textured ? tile : 8);
    gfx_target_dims(&tw, &th);
    gfx_gl_target_size(&scale);
    s = rect_start(s, dsdx, scale, (s_state.shader_flags & GFX_KF_SNAP) != 0);
    t = rect_start(t, dtdy, scale, (s_state.shader_flags & GFX_KF_SNAP) != 0);
    if (!flip) {
        rect_vertex(&q[0], x0, y0, s, t, textured, tw, th);
        rect_vertex(&q[1], x1, y0, s + dsdx * w, t, textured, tw, th);
        rect_vertex(&q[2], x0, y1, s, t + dtdy * h, textured, tw, th);
        rect_vertex(&q[3], x1, y1, s + dsdx * w, t + dtdy * h, textured, tw, th);
    } else {   /* s follows y, t follows x */
        rect_vertex(&q[0], x0, y0, s, t, textured, tw, th);
        rect_vertex(&q[1], x1, y0, s, t + dtdy * w, textured, tw, th);
        rect_vertex(&q[2], x0, y1, s + dsdx * h, t, textured, tw, th);
        rect_vertex(&q[3], x1, y1, s + dsdx * h, t + dtdy * w, textured, tw, th);
    }
    tri[0] = q[0];
    tri[1] = q[2];
    tri[2] = q[1];
    tri[3] = q[1];
    tri[4] = q[2];
    tri[5] = q[3];
    gfx_emit(tri, 6);
}

void gfx_texrect(uint32_t w0, uint32_t w1, uint32_t half1, uint32_t half2, int flip) {
    unsigned lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF, ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF;
    int tile = (w1 >> 24) & 7;
    float s = (int16_t)(half1 >> 16) / 32.0f, t = (int16_t)(half1 & 0xFFFF) / 32.0f;
    float dsdx = (int16_t)(half2 >> 16) / 1024.0f, dtdy = (int16_t)(half2 & 0xFFFF) / 1024.0f;
    unsigned cyc = cycle_type();
    if (cyc == G_CYC_FILL)
        return;
    if (cyc == G_CYC_COPY) {   /* four texels per clock: dsdx is 4x; lower-right edge inclusive */
        dsdx /= 4.0f;
        lrx += 4;
        lry += 4;
    }
    if (lrx <= ulx || lry <= uly)
        return;
    draw_rect(ulx / 4.0f, uly / 4.0f, lrx / 4.0f, lry / 4.0f, tile, s, t, dsdx, dtdy, flip);
}

static void fill_rect(uint32_t w0, uint32_t w1) {
    unsigned lrx = (w0 >> 12) & 0xFFF, lry = w0 & 0xFFF, ulx = (w1 >> 12) & 0xFFF, uly = w1 & 0xFFF;
    unsigned cyc = cycle_type();
    if (cyc == G_CYC_FILL || cyc == G_CYC_COPY) {
        int32_t r[4];
        float c[4];
        r[0] = (int32_t)(ulx >> 2);
        r[1] = (int32_t)(uly >> 2);
        r[2] = (int32_t)(lrx >> 2) + 1;
        r[3] = (int32_t)(lry >> 2) + 1;
        /* clip to the scissor */
        if (r[0] < (int32_t)(g_gfx.sc_ulx >> 2)) r[0] = g_gfx.sc_ulx >> 2;
        if (r[1] < (int32_t)(g_gfx.sc_uly >> 2)) r[1] = g_gfx.sc_uly >> 2;
        if (r[2] > (int32_t)(g_gfx.sc_lrx >> 2)) r[2] = g_gfx.sc_lrx >> 2;
        if (r[3] > (int32_t)(g_gfx.sc_lry >> 2)) r[3] = g_gfx.sc_lry >> 2;
        if (r[2] <= r[0] || r[3] <= r[1])
            return;
        gfx_flush();
        if (g_gfx.cimg.addr != NULL && g_gfx.cimg.addr == g_gfx.zimg) {
            /* the z buffer as color image: a depth clear (packed z: 14 bits after the 2-bit dz) */
            uint32_t z = (g_gfx.fill_color >> 16) >> 2;
            GfxTarget *t = gfx_gl_last_target();
            if (t == NULL) {
                int tw, th;
                gfx_target_dims(&tw, &th);
                t = gfx_gl_target(NULL, tw, th);
            }
            gfx_gl_attach_depth(t, g_gfx.zimg);
            gfx_gl_clear(t, r, 1, NULL, (float)(z & 0x3FFF) / 16383.0f);
        } else {
            fill_rgba(g_gfx.fill_color, c);
            gfx_gl_clear(current_target(), r, 0, c, 0.0f);
        }
        return;
    }
    if (lrx <= ulx || lry <= uly)
        return;
    draw_rect(ulx / 4.0f, uly / 4.0f, lrx / 4.0f, lry / 4.0f, -1, 0, 0, 0, 0, 0);
}

/* ---------------------------------------------------------------- RDP commands */

static void set_othermode(uint32_t *word, uint32_t w0, uint32_t w1) {
    uint32_t len = (w0 & 0xFF) + 1, shift = 32 - ((w0 >> 8) & 0xFF) - len;
    uint32_t mask = (len >= 32 ? 0xFFFFFFFFu : ((1u << len) - 1)) << shift;
    *word = (*word & ~mask) | (w1 & mask);
}

void gfx_rdp_reset(void) {
    g_gfx.om_h = 0;
    g_gfx.om_l = 0;
    g_gfx.cc_w0 = g_gfx.cc_w1 = 0;
    g_gfx.sc_ulx = g_gfx.sc_uly = 0;
    g_gfx.sc_lrx = 320 << 2;
    g_gfx.sc_lry = 240 << 2;
    memset(g_gfx.tiles, 0, sizeof(g_gfx.tiles));
    memset(g_gfx.tmem, 0, sizeof(g_gfx.tmem));
    memset(g_gfx.tmem_le16, 0, sizeof(g_gfx.tmem_le16));
    g_gfx.state_dirty = g_gfx.tex_dirty = 1;
    s_target = NULL;
    s_state_valid = 0;
}

void gfx_rdp_command(uint32_t op, uint32_t w0, uintptr_t w1p) {
    uint32_t w1 = (uint32_t)w1p;
    GfxTile *tl;
    switch (op) {
    case G_SETOTHERMODE_L:
        set_othermode(&g_gfx.om_l, w0, w1);
        g_gfx.state_dirty = 1;
        break;
    case G_SETOTHERMODE_H:
        set_othermode(&g_gfx.om_h, w0, w1);
        g_gfx.state_dirty = g_gfx.tex_dirty = 1;
        break;
    case G_RDPSETOTHERMODE:
        g_gfx.om_h = w0 & 0x00FFFFFF;
        g_gfx.om_l = w1;
        g_gfx.state_dirty = g_gfx.tex_dirty = 1;
        break;
    case G_RDPLOADSYNC:
    case G_RDPPIPESYNC:
    case G_RDPTILESYNC:
    case G_RDPFULLSYNC:
    case G_SETKEYGB:
    case G_SETKEYR:
    case G_SETCONVERT:
        break;
    case G_SETSCISSOR:
        g_gfx.sc_ulx = (w0 >> 12) & 0xFFF;
        g_gfx.sc_uly = w0 & 0xFFF;
        g_gfx.sc_lrx = (w1 >> 12) & 0xFFF;
        g_gfx.sc_lry = w1 & 0xFFF;
        g_gfx.state_dirty = 1;
        break;
    case G_SETPRIMDEPTH:
        g_gfx.prim_z = (uint16_t)(w1 >> 16);
        g_gfx.prim_dz = (uint16_t)w1;
        break;
    case G_LOADTLUT:
        load_tlut(w0, w1);
        break;
    case G_SETTILESIZE:
        tl = &g_gfx.tiles[(w1 >> 24) & 7];
        tl->uls = (w0 >> 12) & 0xFFF;
        tl->ult = w0 & 0xFFF;
        tl->lrs = (w1 >> 12) & 0xFFF;
        tl->lrt = w1 & 0xFFF;
        g_gfx.tex_dirty = g_gfx.state_dirty = 1;
        break;
    case G_LOADBLOCK:
        load_block(w0, w1);
        break;
    case G_LOADTILE:
        tl = &g_gfx.tiles[(w1 >> 24) & 7];
        tl->uls = (w0 >> 12) & 0xFFF;   /* a load also sets the tile's size */
        tl->ult = w0 & 0xFFF;
        tl->lrs = (w1 >> 12) & 0xFFF;
        tl->lrt = w1 & 0xFFF;
        load_tile(w0, w1);
        break;
    case G_SETTILE:
        tl = &g_gfx.tiles[(w1 >> 24) & 7];
        tl->fmt = (w0 >> 21) & 7;
        tl->siz = (w0 >> 19) & 3;
        tl->line = (w0 >> 9) & 0x1FF;
        tl->tmem = w0 & 0x1FF;
        tl->palette = (w1 >> 20) & 0xF;
        tl->cmt = (w1 >> 18) & 3;
        tl->maskt = (w1 >> 14) & 0xF;
        tl->shiftt = (w1 >> 10) & 0xF;
        tl->cms = (w1 >> 8) & 3;
        tl->masks = (w1 >> 4) & 0xF;
        tl->shifts = w1 & 0xF;
        g_gfx.tex_dirty = g_gfx.state_dirty = 1;
        break;
    case G_FILLRECT:
        fill_rect(w0, w1);
        break;
    case G_SETFILLCOLOR:
        g_gfx.fill_color = w1;
        g_gfx.state_dirty = 1;
        break;
    case G_SETFOGCOLOR:
    case G_SETBLENDCOLOR:
    case G_SETPRIMCOLOR:
    case G_SETENVCOLOR: {
        uint8_t *c = op == G_SETFOGCOLOR ? g_gfx.fog : op == G_SETBLENDCOLOR ? g_gfx.blend
                   : op == G_SETPRIMCOLOR ? g_gfx.prim : g_gfx.env;
        c[0] = w1 >> 24;
        c[1] = w1 >> 16;
        c[2] = w1 >> 8;
        c[3] = w1;
        if (op == G_SETPRIMCOLOR) {
            g_gfx.prim_min_level = (w0 >> 8) & 0x1F;
            g_gfx.prim_lod = w0 & 0xFF;
        }
        g_gfx.state_dirty = 1;
        break;
    }
    case G_SETCOMBINE:
        g_gfx.cc_w0 = w0;
        g_gfx.cc_w1 = w1;
        g_gfx.state_dirty = 1;
        break;
    case G_SETTIMG:
        g_gfx.timg.addr = gfx_resolve(w1p);
        g_gfx.timg.fmt = (w0 >> 21) & 7;
        g_gfx.timg.siz = (w0 >> 19) & 3;
        g_gfx.timg.width = (uint16_t)((w0 & 0xFFF) + 1);
        break;
    case G_SETZIMG:
        g_gfx.zimg = gfx_resolve(w1p);
        g_gfx.zimg_width = g_gfx.cimg.width;
        g_gfx.state_dirty = 1;
        break;
    case G_SETCIMG: {
        const uint8_t *a = gfx_resolve(w1p);
        g_gfx.cimg.fmt = (w0 >> 21) & 7;
        g_gfx.cimg.siz = (w0 >> 19) & 3;
        g_gfx.cimg.width = (uint16_t)((w0 & 0xFFF) + 1);
        g_gfx.cimg.addr = a;
        if (a == NULL || a != g_gfx.zimg) {   /* drawing into the z buffer only ever clears it */
            int tw, th;
            gfx_target_dims(&tw, &th);
            s_target = gfx_gl_target(a, tw, th);
            gfx_gl_attach_depth(s_target, g_gfx.zimg);
        }
        g_gfx.state_dirty = 1;
        break;
    }
    default:
        g_gfx.stat_unknown++;
        break;
    }
}
