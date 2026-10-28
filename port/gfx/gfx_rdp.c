/* port/gfx/gfx_rdp.c: the RDP half of the interpreter. Other modes, colors, images, scissor; a 4 KB TMEM fed by
 * LOADBLOCK/LOADTILE/LOADTLUT with the hardware's odd-line word swap; textures decoded from TMEM through the tile
 * descriptors and cached by content; rectangles; the draw state and triangle batching. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "gfx_tex.h"

#define GFX_BATCH_MAX (3 * 2048)
#define TEX_BUCKETS 4096
#define TEX_MAX_ENTRIES 3072
#define TEX_MAX_DIM 1024

static GfxOutVtx s_batch[GFX_BATCH_MAX];
static int s_batch_n;
static GfxDrawState s_state;       /* state of the batch being built */
static int s_state_valid;
static int s_state_rect_tile = -2;
static int s_state_wide;
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

int pcgfx_frame_width(void) {
    return pcgfx.cimg.width ? pcgfx.cimg.width : 320;
}

/* Widescreen (GFX.md "Widescreen"): the 4:3 frame of a width-wide color image grows by this many N64 pixels on
 * each side, so the target has the display aspect. A whole number, so that 2D stays on the N64 pixel grid. */
static int wide_offset(int width) {
    float k = pcgfx_cfg.aspect * 0.75f;   /* the target's width over the frame's */
    if (k <= 1.0f)
        return 0;
    if (k > 3.0f)
        k = 3.0f;
    return (int)lrintf(width * (k - 1.0f) * 0.5f);
}

int pcgfx_wide_width(int frame_width) {
    return frame_width + 2 * wide_offset(frame_width);
}

void pcgfx_target_dims(int *w, int *h) {
    int width = pcgfx_frame_width();
    *w = width + 2 * pcgfx.wide_ox;
    *h = frame_height(width);
}

/* A frame x edge in the target: the frame sits in the middle, and an edge at (within GFX_WIDE_EDGE pixels of) or
 * past the frame's left or right side goes to the target's, so a view or a fill that spans the frame spans the
 * target. The game's split-screen views keep a 1-2 pixel border (2 players: x 2-242; 3-4: 1-161 and 158-318). */
#define GFX_WIDE_EDGE 4.0f

float pcgfx_wide_x_at(float x, int frame_width, int ox) {
    if (x <= GFX_WIDE_EDGE)
        return x;
    if (x >= (float)frame_width - GFX_WIDE_EDGE)
        return x + 2.0f * ox;
    return x + ox;
}

float pcgfx_wide_x(float x) {
    return pcgfx_wide_x_at(x, pcgfx_frame_width(), pcgfx.wide_ox);
}

static GfxTarget *current_target(void) {
    int w, h;
    if (s_target == NULL) {
        pcgfx_target_dims(&w, &h);
        s_target = pcgfx_gl_target(pcgfx.cimg.addr, w, h);
    }
    return s_target;
}

/* ---------------------------------------------------------------- TMEM */

static uint16_t tmem16(uint32_t a) {
    uint16_t v;
    a &= 0xFFE;
    v = (uint16_t)(pcgfx.tmem[a] << 8 | pcgfx.tmem[a + 1]);
    if (pcgfx.tmem_le16[a >> 3])
        v = (uint16_t)(v >> 8 | v << 8);
    return v;
}

static void tmem_put(uint32_t a, uint8_t v, uint8_t le16) {
    a &= 0xFFF;
    pcgfx.tmem[a] = v;
    pcgfx.tmem_le16[a >> 3] = le16;
}

static unsigned bpp_x2(unsigned siz) {   /* bytes per texel, times two */
    return 1u << siz;
}

static void load_block(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &pcgfx.tiles[(w1 >> 24) & 7];
    unsigned uls = (w0 >> 12) & 0xFFF, ult = w0 & 0xFFF, lrs = (w1 >> 12) & 0xFFF, dxt = w1 & 0xFFF;
    unsigned siz = pcgfx.timg.siz, texels, dst = tile->tmem * 8u, i;
    const uint8_t *src;
    int be;
    if (pcgfx.timg.addr == NULL || lrs < uls)
        return;
    texels = lrs - uls + 1;
    src = pcgfx.timg.addr + (((size_t)ult * pcgfx.timg.width + uls) * bpp_x2(siz)) / 2;
    be = pcgfx_is_be(src) || !host_is_le();
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
                uint32_t v = pcgfx_rd32(src + 4 * i, 0);
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
    pcgfx.tmem_gen++;
    pcgfx.tex_dirty = pcgfx.state_dirty = 1;
}

static void load_tile(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &pcgfx.tiles[(w1 >> 24) & 7];
    unsigned sl = ((w0 >> 12) & 0xFFF) >> 2, tl = (w0 & 0xFFF) >> 2;
    unsigned sh = ((w1 >> 12) & 0xFFF) >> 2, th = (w1 & 0xFFF) >> 2;
    unsigned siz = pcgfx.timg.siz, line = tile->line * 8u, dst = tile->tmem * 8u, r, i, w, h;
    int be;
    if (pcgfx.timg.addr == NULL || sh < sl || th < tl)
        return;
    w = sh - sl + 1;
    h = th - tl + 1;
    be = pcgfx_is_be(pcgfx.timg.addr) || !host_is_le();
    for (r = 0; r < h && r < 1024; r++) {
        const uint8_t *src = pcgfx.timg.addr + (((size_t)(tl + r) * pcgfx.timg.width + sl) * bpp_x2(siz)) / 2;
        unsigned swap = (r & 1) ? 4u : 0u, base = dst + r * line;
        if (siz == G_IM_SIZ_32b) {
            for (i = 0; i < w; i++) {
                uint32_t a = ((base + i * 2) ^ swap) & 0x7FF;
                uint32_t v = be ? pcgfx_rd32(src + 4 * i, 1) : pcgfx_rd32(src + 4 * i, 0);
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
    pcgfx.tmem_gen++;
    pcgfx.tex_dirty = pcgfx.state_dirty = 1;
}

/* Each palette entry is stored four times, 8 bytes apart per entry, as the hardware's four TLUT banks. */
static void load_tlut(uint32_t w0, uint32_t w1) {
    const GfxTile *tile = &pcgfx.tiles[(w1 >> 24) & 7];
    unsigned sl = ((w0 >> 12) & 0xFFF) >> 2, tl = (w0 & 0xFFF) >> 2, sh = ((w1 >> 12) & 0xFFF) >> 2;
    unsigned dst = tile->tmem * 8u, i, k, n;
    const uint8_t *src;
    int be;
    if (pcgfx.timg.addr == NULL || sh < sl)
        return;
    n = sh - sl + 1;
    if (n > 256)
        n = 256;
    src = pcgfx.timg.addr + ((size_t)tl * pcgfx.timg.width + sl) * 2;
    be = pcgfx_is_be(src);
    for (i = 0; i < n; i++) {
        uint16_t v = pcgfx_rd16(src + 2 * i, be);
        for (k = 0; k < 4; k++) {
            tmem_put(dst + i * 8 + k * 2, v >> 8, 0);
            tmem_put(dst + i * 8 + k * 2 + 1, v & 0xFF, 0);
        }
    }
    pcgfx.tmem_gen++;
    pcgfx.tex_dirty = pcgfx.state_dirty = 1;
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

/* One texel's raw value at tile coordinates (s, t), already clamped and masked: 4b and 8b the bits as loaded (a
 * palette index when the TLUT applies), 16b the 16-bit word, 32b 0xRRGGBBAA. */
static uint32_t texel_raw(const GfxTile *tl, unsigned s, unsigned t) {
    uint32_t row = tl->tmem * 8u + t * tl->line * 8u, swap = (t & 1) ? 4u : 0u, v, a;
    switch (tl->siz) {
    case G_IM_SIZ_4b:
        v = pcgfx.tmem[((row + (s >> 1)) ^ swap) & 0xFFF];
        return (s & 1) ? (v & 0xF) : (v >> 4);
    case G_IM_SIZ_8b:
        return pcgfx.tmem[((row + s) ^ swap) & 0xFFF];
    case G_IM_SIZ_16b:
        return tmem16((row + s * 2) ^ swap);
    default:
        a = ((row + s * 2) ^ swap) & 0x7FF;
        return (uint32_t)pcgfx.tmem[a] << 24 | (uint32_t)pcgfx.tmem[a + 1] << 16 |
               (uint32_t)pcgfx.tmem[a + 0x800] << 8 | pcgfx.tmem[a + 0x801];
    }
}

/* The TLUT entry a 4b/8b texel value selects. */
static uint16_t tlut_entry(const GfxTile *tl, uint32_t v) {
    uint32_t idx = tl->siz == G_IM_SIZ_4b ? ((uint32_t)tl->palette << 4 | v) : v;
    return tmem16(0x800 + idx * 8);
}

/* A raw texel value as RGBA8. */
static void texel_rgba(const GfxTile *tl, unsigned tlut, uint32_t v, uint8_t *o) {
    unsigned siz = tl->siz;
    if (siz == G_IM_SIZ_32b) {
        o[0] = (uint8_t)(v >> 24);
        o[1] = (uint8_t)(v >> 16);
        o[2] = (uint8_t)(v >> 8);
        o[3] = (uint8_t)v;
        return;
    }
    if (siz <= G_IM_SIZ_8b && (tlut & 2)) {   /* palette: TLUT enabled turns any 4b/8b texel into an index */
        uint16_t e = tlut_entry(tl, v);
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

/* The RDP's clamp, mirror and mask of texel coordinate c, for a texture k times bigger than the N64's (an upscaled
 * or replaced one; k = 1 the N64's): sub-texel f (0 .. k-1) of N64 texel c. The N64 texel comes out as on the RDP;
 * *fo is its sub-texel: the last one where the clamp holds the edge, the mirror image where the mirror turns it. So
 * the tile maths stays in N64 texels and only the sampling gets finer. */
static unsigned map_sub(unsigned c, unsigned f, int clamp, unsigned size, unsigned mask, int mirror, unsigned k,
                        unsigned *fo) {
    if (clamp && c > size - 1) {
        c = size - 1;
        f = k - 1;
    }
    if (mask) {
        if (mirror && ((c >> mask) & 1)) {
            c = ~c;
            f = k - 1 - f;
        }
        c &= (1u << mask) - 1;
    }
    *fo = f;
    return c;
}

typedef struct {
    uint8_t fmt, siz, palette, tlut, cms, cmt, masks, maskt;
    uint16_t line, tmem, tw, th;
    uint8_t variant, pad[3];   /* pcgfx_tex_variant: the original, or the replaced or upscaled texture */
} TexKey;

enum { PEND_NONE, PEND_UPSCALE, PEND_PACK };

typedef struct TexEntry {
    TexKey key;
    uint64_t hash;
    uint32_t gl;
    int w, h, repeat_s, repeat_t;   /* N64 texels: the tile maths and the LOD never see the GL texture's size */
    int scale;                      /* the GL texture is this many times w x h (upscaled or replaced) */
    int pending;                    /* PEND_*: a better texture may come; until then this is the original */
    int factor;                     /* PEND_UPSCALE: the factor and the upscaler's key */
    uint64_t ukey;
    PcgfxTexId id;                  /* PEND_PACK: the identity */
    uint32_t last_use, checked;
    struct TexEntry *next;
} TexEntry;

#define TEX_GRAVE 8192
#define TEX_UPLOAD_BUDGET (4L << 20)   /* bytes of better textures uploaded a frame */
#define TEX_GL_MAX 2048                /* the largest GL texture side made (GLES 3.0's least maximum) */
#define TEX_UP_MAX_TEXELS 65536        /* textures larger than 256 x 256 are not upscaled */

static TexEntry *s_tex_buckets[TEX_BUCKETS];
static int s_tex_count;
static uint8_t *s_decode, *s_src;
/* The last lookup per texture unit, valid while no tile, TMEM or TLUT-mode change happened (epoch). */
static uint32_t s_tex_epoch = 1;
static struct {
    int tile, variant;
    uint32_t epoch;
    TexEntry *e;
} s_unit_cache[GFX_LOD_MAX];
/* GL textures replaced or evicted, deleted a few frames later: the frame interpolator may draw the last two frames
 * again (gfx_interp.c), with the texture names they had. */
static struct {
    uint32_t gl, frame;
} s_grave[TEX_GRAVE];
static int s_grave_n;
static uint32_t s_tex_gen;       /* pcgfx_tex_generation of the textures cached */
static long s_upload_left = TEX_UPLOAD_BUDGET;
static int s_tex_retry;          /* a better texture waited for the budget: look again next frame */

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
        memcpy(&v, &pcgfx.tmem[a], 8);
        h = mix64(h, v ^ ((uint64_t)pcgfx.tmem_le16[a >> 3] << 63));
    }
    return h;
}

static void grave_add(uint32_t gl) {
    if (gl == 0)
        return;
    if (s_grave_n == TEX_GRAVE) {
        pcgfx_gl_delete_texture(s_grave[0].gl);
        memmove(s_grave, s_grave + 1, sizeof(s_grave[0]) * (TEX_GRAVE - 1));
        s_grave_n--;
    }
    s_grave[s_grave_n].gl = gl;
    s_grave[s_grave_n].frame = s_frame;
    s_grave_n++;
}

static void grave_sweep(int all) {
    int i, n = 0;
    for (i = 0; i < s_grave_n; i++) {
        if (all || s_frame - s_grave[i].frame > 3)
            pcgfx_gl_delete_texture(s_grave[i].gl);
        else
            s_grave[n++] = s_grave[i];
    }
    s_grave_n = n;
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
                grave_add(e->gl);
                free(e);
                s_tex_count--;
            } else {
                pp = &e->next;
            }
        }
    }
}

/* Every cached texture dropped (a textures setting changed): now, or through the grave. */
static void tex_cache_drop(int now) {
    int b;
    for (b = 0; b < TEX_BUCKETS; b++) {
        while (s_tex_buckets[b]) {
            TexEntry *e = s_tex_buckets[b];
            s_tex_buckets[b] = e->next;
            if (now)
                pcgfx_gl_delete_texture(e->gl);
            else
                grave_add(e->gl);
            free(e);
        }
    }
    s_tex_count = 0;
    s_tex_epoch++;
}

static void key_clamps(const TexKey *k, int *clamp_s, int *clamp_t) {
    *clamp_s = (k->cms & G_TX_CLAMP) || k->masks == 0;
    *clamp_t = (k->cmt & G_TX_CLAMP) || k->maskt == 0;
}

/* The GL image: W x H N64 texels at f x f sub-texels each, from src (the tile's distinct texels at f, sw texels
 * wide), with the clamp, mask and mirror baked in. f = 1 is the N64 texture itself. */
static void expand(const TexKey *k, const uint8_t *src, unsigned sw, unsigned W, unsigned H, unsigned f,
                   uint8_t *out) {
    unsigned X, Y;
    int clamp_s, clamp_t;
    key_clamps(k, &clamp_s, &clamp_t);
    for (Y = 0; Y < H * f; Y++) {
        unsigned ft, t = map_sub(Y / f, Y % f, clamp_t, k->th, k->maskt, k->cmt & G_TX_MIRROR, f, &ft);
        const uint8_t *row = src + (size_t)(t * f + ft) * sw * 4;
        uint8_t *o = out + (size_t)Y * W * f * 4;
        for (X = 0; X < W * f; X++) {
            unsigned fs, s = map_sub(X / f, X % f, clamp_s, k->tw, k->masks, k->cms & G_TX_MIRROR, f, &fs);
            memcpy(o + (size_t)X * 4, row + (size_t)(s * f + fs) * 4, 4);
        }
    }
}

static uint64_t image_hash(const uint8_t *img, unsigned w, unsigned h, unsigned extra) {
    uint64_t hh = pcgfx_tex_mix(0x55505343u, (uint64_t)w << 32 | (uint64_t)h << 8 | extra);
    unsigned i;
    for (i = 0; i < w * h; i++, img += 4)
        hh = pcgfx_tex_mix(hh, (uint32_t)img[0] << 24 | (uint32_t)img[1] << 16 | (uint32_t)img[2] << 8 | img[3]);
    return hh;
}

static int is_solid(const uint8_t *img, unsigned n) {
    unsigned i;
    for (i = 1; i < n; i++)
        if (memcmp(img + i * 4, img, 4) != 0)
            return 0;
    return 1;
}

/* A replacement d times smaller (box filtered), malloc'd. */
static uint8_t *box_down(const uint8_t *img, int w, int h, int d) {
    int W = w / d, H = h / d, x, y, i, j, c;
    uint8_t *o = (uint8_t *)malloc((size_t)W * H * 4);
    if (o == NULL)
        return NULL;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            for (c = 0; c < 4; c++) {
                unsigned s = 0;
                for (j = 0; j < d; j++)
                    for (i = 0; i < d; i++)
                        s += img[((size_t)(y * d + j) * w + x * d + i) * 4 + c];
                o[((size_t)y * W + x) * 4 + c] = (uint8_t)((s + (unsigned)(d * d) / 2) / (unsigned)(d * d));
            }
    return o;
}

static void tex_swap(TexEntry *e, uint32_t gl, int scale) {
    if (gl == 0)
        return;
    grave_add(e->gl);
    e->gl = gl;
    e->scale = scale;
}

/* A pack's replacement (iw x ih, a whole multiple f of the tile's distinct texels) as e's GL texture. Above the
 * internal scale, or past the GL size, it is box filtered down by a divisor of f: a texture sampled coarser than
 * its texels shimmers. */
static void pack_upload(TexEntry *e, const uint8_t *img, int iw, int ih) {
    int f = iw / e->id.w, d, limit = pcgfx_tex_pack_limit();
    const uint8_t *src = img;
    uint8_t *small = NULL, *out;
    for (d = 1; d < f; d++)
        if (f % d == 0 && f / d <= limit && e->w * (f / d) <= TEX_GL_MAX && e->h * (f / d) <= TEX_GL_MAX)
            break;
    if (d > 1) {
        small = box_down(img, iw, ih, d);
        if (small == NULL)
            return;
        src = small;
        f /= d;
    }
    out = (uint8_t *)malloc((size_t)e->w * f * e->h * f * 4);
    if (out != NULL) {
        expand(&e->key, src, (unsigned)(e->id.w * f), (unsigned)e->w, (unsigned)e->h, (unsigned)f, out);
        if (f == 1)
            tex_swap(e, pcgfx_gl_texture(out, e->w, e->h), 1);
        else
            tex_swap(e, pcgfx_gl_texture(out, e->w * f, e->h * f), f);
        s_upload_left -= (long)e->w * f * e->h * f * 4;
    }
    free(out);
    free(small);
}

/* A texture waiting for a better one: once a frame, has it come? Uploads stop for the frame past the budget (one
 * always goes), so a burst of results spreads over frames. */
static void tex_upgrade(TexEntry *e) {
    const uint8_t *img = NULL;
    int iw = 0, ih = 0, r;
    if (e->checked == s_frame)
        return;
    if (s_upload_left <= 0) {
        s_tex_retry = 1;
        return;
    }
    e->checked = s_frame;
    if (e->pending == PEND_UPSCALE) {
        r = pcgfx_tex_upscaled(e->ukey, e->factor, s_frame, NULL, 0, 0, 0, 0, 0, &img);
        if (r == PCGFX_TEX_READY) {
            tex_swap(e, pcgfx_gl_texture(img, e->w * e->factor, e->h * e->factor), e->factor);
            s_upload_left -= (long)e->w * e->factor * e->h * e->factor * 4;
        }
    } else {
        r = pcgfx_tex_pack(&e->id, &img, &iw, &ih);
        if (r == PCGFX_TEX_READY)
            pack_upload(e, img, iw, ih);
    }
    if (r != PCGFX_TEX_WAIT)
        e->pending = PEND_NONE;
}

static void tex_touch(TexEntry *e) {
    e->last_use = s_frame;
    if (e->pending)
        tex_upgrade(e);
}

static TexEntry *tex_lookup(int tile_idx, int variant) {
    const GfxTile *tl = &pcgfx.tiles[tile_idx & 7];
    TexKey k;
    TexEntry *e;
    PcgfxTexId id;
    uint64_t h;
    unsigned W, H, S, T, x, y, tw, th, bytes, line;
    int clamp_s, clamp_t, ci, need_id, mode;
    memset(&k, 0, sizeof(k));
    k.fmt = tl->fmt;
    k.siz = tl->siz;
    k.palette = tl->palette;
    k.tlut = (pcgfx.om_h >> G_MDSFT_TEXTLUT) & 3;
    k.cms = tl->cms;
    k.cmt = tl->cmt;
    k.masks = tl->masks > 10 ? 10 : tl->masks;
    k.maskt = tl->maskt > 10 ? 10 : tl->maskt;
    k.line = tl->line;
    k.tmem = tl->tmem;
    k.variant = (uint8_t)variant;
    tw = tl->lrs >= tl->uls ? (tl->lrs >> 2) - (tl->uls >> 2) + 1 : 1;
    th = tl->lrt >= tl->ult ? (tl->lrt >> 2) - (tl->ult >> 2) + 1 : 1;
    if (tw > TEX_MAX_DIM) tw = TEX_MAX_DIM;
    if (th > TEX_MAX_DIM) th = TEX_MAX_DIM;
    k.tw = (uint16_t)tw;
    k.th = (uint16_t)th;

    /* Decoded size: the tile when it clamps (mask/mirror baked in), one period when it wraps. */
    key_clamps(&k, &clamp_s, &clamp_t);
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
            tex_touch(e);
            return e;
        }

    if (s_tex_count >= TEX_MAX_ENTRIES)
        tex_evict();
    if (s_decode == NULL)
        s_decode = (uint8_t *)malloc(TEX_MAX_DIM * TEX_MAX_DIM * 4);
    if (s_src == NULL)
        s_src = (uint8_t *)malloc(TEX_MAX_DIM * TEX_MAX_DIM * 4);

    /* The tile's distinct texels, S x T: what a mask period or the clamped tile holds. Their identity (TEXTURES.md)
     * when a replacement or the dump needs it: the raw values (CI: the indices) with the format and the size, and
     * for CI the TLUT entries up to the largest index used, so TMEM placement, the load's kind, the memory's byte
     * order and unused palette entries don't change it. */
    S = k.masks ? (clamp_s ? (tw < (1u << k.masks) ? tw : (1u << k.masks)) : (1u << k.masks)) : tw;
    T = k.maskt ? (clamp_t ? (th < (1u << k.maskt) ? th : (1u << k.maskt)) : (1u << k.maskt)) : th;
    if (S > TEX_MAX_DIM) S = TEX_MAX_DIM;
    if (T > TEX_MAX_DIM) T = TEX_MAX_DIM;
    ci = k.siz <= G_IM_SIZ_8b && (k.tlut & 2);
    mode = variant ? pcgfx_tex_mode() : PCGFX_TEX_ORIGINAL;
    need_id = mode == PCGFX_TEX_PACK || pcgfx_tex_dumping();
    memset(&id, 0, sizeof(id));
    id.fmt = (uint8_t)(ci ? G_IM_FMT_CI : k.fmt);
    id.siz = k.siz;
    id.w = (uint16_t)S;
    id.h = (uint16_t)T;
    {
        uint64_t ih = pcgfx_tex_mix(pcgfx_tex_mix(0x52323034u, (uint64_t)id.fmt << 8 | id.siz), S << 16 | T);
        uint32_t maxv = 0;
        for (y = 0; y < T; y++)
            for (x = 0; x < S; x++) {
                uint32_t v = texel_raw(tl, x, y);
                if (need_id)
                    ih = pcgfx_tex_mix(ih, v);
                if (v > maxv)
                    maxv = v;
                texel_rgba(tl, k.tlut, v, s_src + ((size_t)y * S + x) * 4);
            }
        id.texels = ih;
        if (need_id && ci) {
            uint64_t ph = pcgfx_tex_mix(0x544C5554u, k.tlut & 1);
            for (x = 0; x <= maxv; x++)
                ph = pcgfx_tex_mix(ph, tlut_entry(tl, x));
            id.palette = (uint32_t)(ph ^ (ph >> 32));
        }
    }
    if (pcgfx_tex_dumping())
        pcgfx_tex_dump(&id, s_src);
    expand(&k, s_src, S, W, H, 1, s_decode);

    e = (TexEntry *)calloc(1, sizeof(*e));
    e->key = k;
    e->hash = h;
    e->w = (int)W;
    e->h = (int)H;
    e->repeat_s = !clamp_s;
    e->repeat_t = !clamp_t;
    e->scale = 1;
    e->last_use = e->checked = s_frame;
    if (mode == PCGFX_TEX_UPSCALED) {
        /* Upscaled: the GL image itself (the period, mirror baked in, wrapping as it repeats), so that the upscaler
         * sees each edge's real neighbours. Left alone: textures under 4 texels a side (bars, gradients, dots), over
         * 256 x 256, and of one colour, which upscaling can't improve. */
        int f = pcgfx_tex_factor();
        if (f > 1 && W >= 4 && H >= 4 && W * H <= TEX_UP_MAX_TEXELS && W * f <= TEX_GL_MAX && H * f <= TEX_GL_MAX &&
            !is_solid(s_decode, W * H)) {
            int indep = ci || k.fmt == G_IM_FMT_IA || (k.fmt == G_IM_FMT_RGBA && k.siz >= G_IM_SIZ_16b);
            const uint8_t *img = NULL;
            int r;
            e->ukey = image_hash(s_decode, W, H, (unsigned)(e->repeat_s << 2 | e->repeat_t << 1 | indep));
            e->factor = f;
            r = pcgfx_tex_upscaled(e->ukey, f, s_frame, s_decode, (int)W, (int)H, e->repeat_s, e->repeat_t, indep,
                                   &img);
            if (r == PCGFX_TEX_READY) {
                e->gl = pcgfx_gl_texture(img, (int)W * f, (int)H * f);
                e->scale = f;
            } else if (r == PCGFX_TEX_WAIT) {
                e->pending = PEND_UPSCALE;
            }
        }
    } else if (mode == PCGFX_TEX_PACK) {
        const uint8_t *img = NULL;
        int iw = 0, ih = 0, r;
        e->id = id;
        r = pcgfx_tex_pack(&id, &img, &iw, &ih);
        if (r == PCGFX_TEX_READY)
            pack_upload(e, img, iw, ih);
        else if (r == PCGFX_TEX_WAIT)
            e->pending = PEND_PACK;
    }
    if (e->gl == 0)
        e->gl = pcgfx_gl_texture(s_decode, (int)W, (int)H);
    e->next = s_tex_buckets[h % TEX_BUCKETS];
    s_tex_buckets[h % TEX_BUCKETS] = e;
    s_tex_count++;
    return e;
}

/* At shutdown: every texture and the replacement's tables and worker. */
void pcgfx_tex_cache_clear(void) {
    tex_cache_drop(1);
    grave_sweep(1);
    free(s_decode);
    s_decode = NULL;
    free(s_src);
    s_src = NULL;
    pcgfx_tex_shutdown();
}

/* Each frame: the textures setting (a change drops every texture), results that arrived, the upload budget. */
static void tex_new_frame(void) {
    uint32_t g = pcgfx_tex_generation();
    if (g != s_tex_gen) {
        s_tex_gen = g;
        tex_cache_drop(0);
    }
    if (pcgfx_tex_new_frame(s_frame) || s_tex_retry) {
        s_tex_retry = 0;
        s_tex_epoch++;   /* the texture units look their textures up again, and find what arrived */
        pcgfx.tex_dirty = 1;
    }
    s_upload_left = TEX_UPLOAD_BUDGET;
    grave_sweep(0);
}

/* Texture unit u's texture for tile ti: its last lookup while nothing changed, else a cache lookup. */
static TexEntry *unit_texture(int u, int ti, int variant) {
    TexEntry *e;
    if (s_unit_cache[u].tile == ti && s_unit_cache[u].variant == variant && s_unit_cache[u].epoch == s_tex_epoch &&
        s_unit_cache[u].e != NULL) {
        e = s_unit_cache[u].e;
        tex_touch(e);
        return e;
    }
    e = tex_lookup(ti, variant);
    s_unit_cache[u].tile = ti;
    s_unit_cache[u].variant = variant;
    s_unit_cache[u].epoch = s_tex_epoch;
    s_unit_cache[u].e = e;
    return e;
}

/* ---------------------------------------------------------------- texture coordinates */

static GfxUnitInfo s_unit[2];

static float tile_shift(float c, int sh) {
    if (sh == 0)
        return c;
    if (sh <= 10)
        return c / (float)(1 << sh);
    return c * (float)(1 << (16 - sh));
}

void pcgfx_unit_uv(const GfxUnitInfo *ui, float s, float t, float *u, float *v) {
    s = tile_shift(s, ui->shifts) - ui->uls;
    t = tile_shift(t, ui->shiftt) - ui->ult;
    if (ui->linear) {   /* bilinear centers texels on integer coordinates */
        s += 0.5f;
        t += 0.5f;
    }
    *u = s / ui->w;
    *v = t / ui->h;
}

void pcgfx_tile_uv(int unit, float s, float t, float *u, float *v) {
    pcgfx_unit_uv(&s_unit[unit], s, t, u, v);
}

const GfxUnitInfo *pcgfx_units(void) {
    return s_unit;
}

/* ---------------------------------------------------------------- draw state */

static void color4(const uint8_t *c, float *o) {
    o[0] = c[0] / 255.0f;
    o[1] = c[1] / 255.0f;
    o[2] = c[2] / 255.0f;
    o[3] = c[3] / 255.0f;
}

static void fill_rgba(uint32_t fc, float *o) {
    if (pcgfx.cimg.siz == G_IM_SIZ_32b) {
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
    return (pcgfx.om_h >> G_MDSFT_CYCLETYPE) & 3;
}

static void build_state(GfxDrawState *st, int rect_tile, int wide) {
    GfxShaderKey key;
    unsigned cyc = cycle_type(), u;
    uint32_t oml = pcgfx.om_l;
    int tile = rect_tile >= 0 ? rect_tile : pcgfx.tex_tile;
    /* Replaced and upscaled textures (TEXTURES.md): 3D draws, under a perspective projection; 2D as the setting
     * says. scaled: a texture bound is bigger than the N64's. */
    int variant = pcgfx_tex_variant(rect_tile < 0 && pcgfx.persp), scaled = 0;

    memset(st, 0, sizeof(*st));
    memset(&key, 0, sizeof(key));
    key.cc_w0 = pcgfx.cc_w0 & 0x00FFFFFF;
    key.cc_w1 = pcgfx.cc_w1;
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

    /* Texture LOD (G_TL_LOD) on a triangle: the RDP picks the mip level per pixel, each a tile from the G_TEXTURE
     * tile on, G_TEXTURE's level field the last; the shader does the same (gfx_combine.c). Detail and sharpen
     * textures are not emulated: those draw as without LOD, from the tile and the next. */
    if (rect_tile < 0 && pcgfx.tex_on && (key.mode == GFX_MODE_1CYC || key.mode == GFX_MODE_2CYC) &&
        ((pcgfx.om_h >> G_MDSFT_TEXTLOD) & 1) && ((pcgfx.om_h >> G_MDSFT_TEXTDETAIL) & 3) == 0 &&
        (pcgfx_combine_uses(&key, 0) || pcgfx_combine_uses(&key, 1))) {
        int levels = pcgfx.tex_level + 1, k;
        unsigned filt = (pcgfx.om_h >> G_MDSFT_TEXTFILT) & 3;
        for (k = 0; k < levels; k++) {
            int ti = (tile + k) & 7;
            const GfxTile *tl = &pcgfx.tiles[ti];
            TexEntry *e = unit_texture(k, ti, variant);
            int lin = filt != G_TF_POINT;
            st->tex[k] = e->gl;
            st->repeat_s[k] = (uint8_t)e->repeat_s;
            st->repeat_t[k] = (uint8_t)e->repeat_t;
            st->linear[k] = (uint8_t)lin;
            st->lod_a[k][0] = tile_shift(1.0f, tl->shifts);
            st->lod_a[k][1] = tile_shift(1.0f, tl->shiftt);
            st->lod_a[k][2] = tl->uls / 4.0f - (lin ? 0.5f : 0.0f);   /* bilinear centres texels on integers */
            st->lod_a[k][3] = tl->ult / 4.0f - (lin ? 0.5f : 0.0f);
            st->lod_b[k][0] = 1.0f / (float)e->w;
            st->lod_b[k][1] = 1.0f / (float)e->h;
        }
        key.lod_levels = (uint8_t)levels;
        st->lod_levels = (uint8_t)levels;
        key.flags |= GFX_KF_TEX0 | GFX_KF_TEX1;
    }
    for (u = 0; u < 2 && !key.lod_levels; u++) {
        int want = key.mode == GFX_MODE_COPY ? u == 0
                 : (key.mode == GFX_MODE_FILL ? 0 : pcgfx_combine_uses(&key, (int)u));
        if (rect_tile < 0 && !pcgfx.tex_on)
            want = want && 0;
        if (want) {
            int ti = (tile + (int)u) & 7;
            const GfxTile *tl = &pcgfx.tiles[ti];
            TexEntry *e = unit_texture((int)u, ti, variant);
            unsigned filt = (pcgfx.om_h >> G_MDSFT_TEXTFILT) & 3;
            st->tex[u] = e->gl;
            scaled |= e->scale > 1;
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
        float scale = pcgfx_gl_target_scale(current_target());
        st->scale = scale;
        /* A replaced texture has texels finer than the N64 pixel: its samples are not snapped to the pixel's corner */
        if (rect_tile >= 0 && rect_tile < 8 && scale != 1.0f && !scaled &&
            (key.mode == GFX_MODE_COPY || ((pcgfx.om_h >> G_MDSFT_TEXTFILT) & 3) == G_TF_POINT))
            key.flags |= GFX_KF_SNAP;
    }
    st->shader = pcgfx_gl_shader(&key);
    st->shader_flags = key.flags;
    st->target = current_target();

    /* Depth. Triangles need G_ZBUFFER for z (or the primitive depth); rectangles only have the primitive depth. */
    if (key.mode == GFX_MODE_1CYC || key.mode == GFX_MODE_2CYC) {
        int has_z = rect_tile >= 0 ? (oml & G_ZS_PRIM) != 0 : ((pcgfx.geom & G_ZBUFFER) || (oml & G_ZS_PRIM));
        int cmp = has_z && (oml & Z_CMP), upd = has_z && (oml & Z_UPD);
        st->depth_test = cmp ? 1 : upd ? 2 : 0;   /* 2: always pass, write only */
        st->depth_write = (uint8_t)(upd != 0);
        st->decal = (oml & ZMODE_MASK) == ZMODE_DEC;
        st->blend = (uint8_t)pcgfx_blend_uses_mem(&key);
    }

    /* Scissor, N64 pixels, exclusive. In widescreen it is in the target: its sides at the frame's sides go to the
     * target's for a widened draw, and a 2D draw keeps the N64's (GFX.md "Widescreen"). */
    st->scissor[0] = pcgfx.sc_ulx >> 2;
    st->scissor[1] = pcgfx.sc_uly >> 2;
    st->scissor[2] = pcgfx.sc_lrx >> 2;
    st->scissor[3] = pcgfx.sc_lry >> 2;
    if (pcgfx.wide_ox != 0) {
        if (wide) {
            st->scissor[0] = (int32_t)pcgfx_wide_x((float)st->scissor[0]);
            st->scissor[2] = (int32_t)pcgfx_wide_x((float)st->scissor[2]);
        } else {
            st->scissor[0] += pcgfx.wide_ox;
            st->scissor[2] += pcgfx.wide_ox;
        }
    }

    color4(pcgfx.prim, st->prim);
    color4(pcgfx.env, st->env);
    color4(pcgfx.fog, st->fog);
    color4(pcgfx.blend, st->blendc);
    fill_rgba(pcgfx.fill_color, st->fill);
    st->prim_lod = pcgfx.prim_lod / 255.0f;
    st->lod_frac = 0.0f;   /* no LOD computation: see GFX.md */
    st->noise_seed = (float)(s_frame & 255);
}

const GfxDrawState *pcgfx_draw_state(int rect_tile, int wide) {
    GfxDrawState st;
    if (s_state_valid && !pcgfx.state_dirty && !pcgfx.tex_dirty && s_state_rect_tile == rect_tile &&
        s_state_wide == wide)
        return &s_state;
    if (pcgfx.tex_dirty)
        s_tex_epoch++;
    build_state(&st, rect_tile, wide);
    pcgfx.state_dirty = 0;
    pcgfx.tex_dirty = 0;
    s_state_rect_tile = rect_tile;
    s_state_wide = wide;
    if (!s_state_valid || memcmp(&st, &s_state, sizeof(st)) != 0) {
        pcgfx_flush();
        s_state = st;
        s_state_valid = 1;
    }
    return &s_state;
}

const GfxDrawState *pcgfx_draw_state_peek(int rect_tile, int wide) {
    if (s_state_valid && !pcgfx.state_dirty && !pcgfx.tex_dirty && s_state_rect_tile == rect_tile &&
        s_state_wide == wide)
        return &s_state;
    return NULL;
}

void pcgfx_emit(const GfxOutVtx *v, int n) {
    if (pcgfx_hud_pending())   /* the HUD's queued 2D was drawn before this (gfx_hud.c) */
        pcgfx_hud_resolve();
    if (s_batch_n + n > GFX_BATCH_MAX)
        pcgfx_flush();
    memcpy(&s_batch[s_batch_n], v, sizeof(*v) * (size_t)n);
    s_batch_n += n;
}

void pcgfx_flush(void) {
    if (s_batch_n > 0 && s_state_valid) {
        pcgfx_gl_draw(&s_state, s_batch, s_batch_n);
        pcgfx.stat_draws++;
    }
    s_batch_n = 0;
}

void pcgfx_rdp_new_frame(void) {
    s_frame++;
    pcgfx.state_dirty = 1;
    tex_new_frame();
}

void pcgfx_rdp_forget_targets(void) {
    s_target = NULL;
    s_state_valid = 0;
}

/* ---------------------------------------------------------------- rectangles */

/* A corner at clip x, y (pcgfx_rect_x, pcgfx_rect_y) with texture coordinates s, t there. */
static void rect_vertex(GfxOutVtx *o, float x, float y, float s, float t, int textured) {
    float d = (pcgfx.om_l & G_ZS_PRIM) ? pcgfx.prim_z / 32767.0f : 0.0f;
    int k;
    o->pos[0] = x;
    o->pos[1] = y;
    o->pos[2] = 2.0f * d - 1.0f;
    o->pos[3] = 1.0f;
    o->shade[0] = o->shade[1] = o->shade[2] = o->shade[3] = 0.0f;   /* rectangles have no shade */
    for (k = 0; k < 2; k++) {
        if (textured && s_state.tex[k])
            pcgfx_tile_uv(k, s, t, &o->uv[2 * k], &o->uv[2 * k + 1]);
        else
            o->uv[2 * k] = o->uv[2 * k + 1] = 0.0f;
    }
}

/* The RDP samples a rectangle pixel at its corner: pixel k gets texel floor(c0 + k * d). GL samples at (internal)
 * pixel centers, so start the interpolation so that each of the samples inside N64 pixel k stays in that texel
 * (exact for |d| <= 1), plus a small bias against float error on texel boundaries. scale: internal pixels per N64
 * pixel along this coordinate's axis. Snapped (point-sampled above 1x), the shader moves every sample to its N64
 * pixel's corner (gfx_combine.c), so the corner's value is the RDP's at any scale, a fraction too. */
static float rect_start(float c0, float d, float scale, int snap) {
    if (snap)
        return c0 + 1.0f / 64;
    if (d >= 0.0f)
        return c0 - 0.5f * d / scale + 1.0f / 64;
    return c0 - d + 0.5f * d / scale + 1.0f / 64;
}

/* A texture coordinate c0 at N64 coordinate 0 of a rectangle's side, at d (from that side): c0 + step * d. At d = 0,
 * c0 itself, and at the far side (d the side's length), c0 + step * length, as before the boundaries. */
static float rect_lin(float c0, float step, float d) {
    return d == 0.0f ? c0 : c0 + step * d;
}

/* Corners in N64 pixels; s/t at (x0, y0) for the pixel's corner, per-pixel steps. The sides go to GL on internal
 * pixel boundaries (pcgfx_rect_x), so the pixels a rectangle covers are the same on any GL; the texture coordinates
 * are taken where the sides are. */
static void draw_rect(float x0, float y0, float x1, float y1, int tile, float s, float t, float dsdx, float dtdy,
                      int flip) {
    GfxOutVtx q[4], tri[6];
    int tw, th, pw, ph, textured = tile >= 0, wide = 0, snap, stretched = 0, u;
    float sx = 1.0f, sy = 1.0f, cx0, cx1, cy0, cy1, xe0, xe1, ye0, ye1, du[4] = {0, 0, 0, 0};
    /* the corners' x at the 4:3 place, for the HUD at the screen's edges (gfx_hud.c) */
    float a0 = x0 + (float)pcgfx.wide_ox, a1 = x1 + (float)pcgfx.wide_ox;
    if (pcgfx.wide_ox != 0) {
        /* Widescreen (GFX.md): a fill's sides at the frame's go to the target's, and a textured rectangle across
         * the whole frame is stretched across the target. Other textured rectangles stay 4:3, in the middle. */
        wide = !textured || (pcgfx_wide_x(x0) == x0 && pcgfx_wide_x(x1) == x1 + 2.0f * pcgfx.wide_ox);
        if (wide) {
            float nx0 = pcgfx_wide_x(x0), nx1 = pcgfx_wide_x(x1), k = (x1 - x0) / (nx1 - nx0);
            stretched = textured;
            if (flip)   /* the step along x */
                dtdy *= k;
            else
                dsdx *= k;
            x0 = nx0;
            x1 = nx1;
        } else {
            x0 += (float)pcgfx.wide_ox;
            x1 += (float)pcgfx.wide_ox;
        }
    }
    pcgfx_draw_state(textured ? tile : 8, wide);
    pcgfx_target_dims(&tw, &th);
    pw = tw;
    ph = th;
    if (s_state.target != NULL)
        pcgfx_gl_target_px(s_state.target, &pw, &ph, &sx, &sy);
    snap = (s_state.shader_flags & GFX_KF_SNAP) != 0;
    s = rect_start(s, dsdx, flip ? sy : sx, snap);   /* flipped, s follows y */
    t = rect_start(t, dtdy, flip ? sx : sy, snap);
    cx0 = pcgfx_rect_x(x0, tw, pw, &xe0);
    cx1 = pcgfx_rect_x(x1, tw, pw, &xe1);
    cy0 = pcgfx_rect_y(y0, th, ph, &ye0);
    cy1 = pcgfx_rect_y(y1, th, ph, &ye1);
    xe0 -= x0, xe1 -= x0, ye0 -= y0, ye1 -= y0;   /* from the corner (x0, y0) */
    if (!flip) {
        rect_vertex(&q[0], cx0, cy0, rect_lin(s, dsdx, xe0), rect_lin(t, dtdy, ye0), textured);
        rect_vertex(&q[1], cx1, cy0, rect_lin(s, dsdx, xe1), rect_lin(t, dtdy, ye0), textured);
        rect_vertex(&q[2], cx0, cy1, rect_lin(s, dsdx, xe0), rect_lin(t, dtdy, ye1), textured);
        rect_vertex(&q[3], cx1, cy1, rect_lin(s, dsdx, xe1), rect_lin(t, dtdy, ye1), textured);
    } else {   /* s follows y, t follows x */
        rect_vertex(&q[0], cx0, cy0, rect_lin(s, dsdx, ye0), rect_lin(t, dtdy, xe0), textured);
        rect_vertex(&q[1], cx1, cy0, rect_lin(s, dsdx, ye0), rect_lin(t, dtdy, xe1), textured);
        rect_vertex(&q[2], cx0, cy1, rect_lin(s, dsdx, ye1), rect_lin(t, dtdy, xe0), textured);
        rect_vertex(&q[3], cx1, cy1, rect_lin(s, dsdx, ye1), rect_lin(t, dtdy, xe1), textured);
    }
    tri[0] = q[0];
    tri[1] = q[2];
    tri[2] = q[1];
    tri[3] = q[1];
    tri[4] = q[2];
    tri[5] = q[3];
    if (pcgfx_hud_on()) {   /* the HUD at the screen's edges: queued, moved with its group (gfx_hud.c) */
        const float x43[6] = {a0, a0, a1, a1, a0, a1};   /* q[0], q[2], q[1], q[1], q[2], q[3] */
        const float y43[6] = {y0, y1, y0, y0, y1, y1};
        for (u = 0; u < 2 && textured; u++) {   /* the texture coordinates' change per N64 pixel across */
            if (s_state.tex[u]) {
                float u0, v0, u1, v1;
                pcgfx_tile_uv(u, s, t, &u0, &v0);
                pcgfx_tile_uv(u, flip ? s : s + dsdx, flip ? t + dtdy : t, &u1, &v1);
                du[2 * u] = u1 - u0;
                du[2 * u + 1] = v1 - v0;
            }
        }
        if (pcgfx_hud_add(&s_state, tri, x43, y43, du, 6, !stretched, 0) == 0)
            return;
    }
    if (pcgfx_rec != NULL)
        pcgfx_rec_verts(&s_state, tri, 6);
    pcgfx_emit(tri, 6);
}

void pcgfx_texrect(uint32_t w0, uint32_t w1, uint32_t half1, uint32_t half2, int flip) {
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
        if (r[0] < (int32_t)(pcgfx.sc_ulx >> 2)) r[0] = pcgfx.sc_ulx >> 2;
        if (r[1] < (int32_t)(pcgfx.sc_uly >> 2)) r[1] = pcgfx.sc_uly >> 2;
        if (r[2] > (int32_t)(pcgfx.sc_lrx >> 2)) r[2] = pcgfx.sc_lrx >> 2;
        if (r[3] > (int32_t)(pcgfx.sc_lry >> 2)) r[3] = pcgfx.sc_lry >> 2;
        if (r[2] <= r[0] || r[3] <= r[1])
            return;
        if (pcgfx.wide_ox != 0) {   /* widescreen: sides at the frame's go to the target's (GFX.md) */
            r[0] = (int32_t)pcgfx_wide_x((float)r[0]);
            r[2] = (int32_t)pcgfx_wide_x((float)r[2]);
        }
        if (pcgfx_hud_pending())    /* the HUD's queued 2D first (gfx_hud.c) */
            pcgfx_hud_resolve();
        pcgfx_flush();
        if (pcgfx.cimg.addr != NULL && pcgfx.cimg.addr == pcgfx.zimg) {
            /* the z buffer as color image: a depth clear (packed z: 14 bits after the 2-bit dz) */
            uint32_t z = (pcgfx.fill_color >> 16) >> 2;
            GfxTarget *t = pcgfx_gl_last_target();
            if (t == NULL) {
                int tw, th;
                pcgfx_target_dims(&tw, &th);
                t = pcgfx_gl_target(NULL, tw, th);
            }
            pcgfx_gl_attach_depth(t, pcgfx.zimg);
            pcgfx_gl_clear(t, r, 1, NULL, (float)(z & 0x3FFF) / 16383.0f);
            if (pcgfx_rec != NULL)
                pcgfx_rec_clear(t, r, 1, NULL, (float)(z & 0x3FFF) / 16383.0f);
        } else {
            fill_rgba(pcgfx.fill_color, c);
            pcgfx_gl_clear(current_target(), r, 0, c, 0.0f);
            if (pcgfx_rec != NULL)
                pcgfx_rec_clear(current_target(), r, 0, c, 0.0f);
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

void pcgfx_rdp_reset(void) {
    pcgfx.om_h = 0;
    pcgfx.om_l = 0;
    pcgfx.cc_w0 = pcgfx.cc_w1 = 0;
    pcgfx.sc_ulx = pcgfx.sc_uly = 0;
    pcgfx.sc_lrx = 320 << 2;
    pcgfx.sc_lry = 240 << 2;
    memset(pcgfx.tiles, 0, sizeof(pcgfx.tiles));
    memset(pcgfx.tmem, 0, sizeof(pcgfx.tmem));
    memset(pcgfx.tmem_le16, 0, sizeof(pcgfx.tmem_le16));
    pcgfx.state_dirty = pcgfx.tex_dirty = 1;
    s_target = NULL;
    s_state_valid = 0;
    s_tex_gen = pcgfx_tex_generation();   /* the settings at init: no needless drop of the cache at the first frame */
}

void pcgfx_rdp_command(uint32_t op, uint32_t w0, uintptr_t w1p) {
    uint32_t w1 = (uint32_t)w1p;
    GfxTile *tl;
    switch (op) {
    case G_SETOTHERMODE_L:
        set_othermode(&pcgfx.om_l, w0, w1);
        pcgfx.state_dirty = 1;
        break;
    case G_SETOTHERMODE_H:
        set_othermode(&pcgfx.om_h, w0, w1);
        pcgfx.state_dirty = pcgfx.tex_dirty = 1;
        break;
    case G_RDPSETOTHERMODE:
        pcgfx.om_h = w0 & 0x00FFFFFF;
        pcgfx.om_l = w1;
        pcgfx.state_dirty = pcgfx.tex_dirty = 1;
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
        pcgfx.sc_ulx = (w0 >> 12) & 0xFFF;
        pcgfx.sc_uly = w0 & 0xFFF;
        pcgfx.sc_lrx = (w1 >> 12) & 0xFFF;
        pcgfx.sc_lry = w1 & 0xFFF;
        pcgfx.state_dirty = 1;
        break;
    case G_SETPRIMDEPTH:
        pcgfx.prim_z = (uint16_t)(w1 >> 16);
        pcgfx.prim_dz = (uint16_t)w1;
        break;
    case G_LOADTLUT:
        load_tlut(w0, w1);
        break;
    case G_SETTILESIZE:
        tl = &pcgfx.tiles[(w1 >> 24) & 7];
        tl->uls = (w0 >> 12) & 0xFFF;
        tl->ult = w0 & 0xFFF;
        tl->lrs = (w1 >> 12) & 0xFFF;
        tl->lrt = w1 & 0xFFF;
        pcgfx.tex_dirty = pcgfx.state_dirty = 1;
        break;
    case G_LOADBLOCK:
        load_block(w0, w1);
        break;
    case G_LOADTILE:
        tl = &pcgfx.tiles[(w1 >> 24) & 7];
        tl->uls = (w0 >> 12) & 0xFFF;   /* a load also sets the tile's size */
        tl->ult = w0 & 0xFFF;
        tl->lrs = (w1 >> 12) & 0xFFF;
        tl->lrt = w1 & 0xFFF;
        load_tile(w0, w1);
        break;
    case G_SETTILE:
        tl = &pcgfx.tiles[(w1 >> 24) & 7];
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
        pcgfx.tex_dirty = pcgfx.state_dirty = 1;
        break;
    case G_FILLRECT:
        fill_rect(w0, w1);
        break;
    case G_SETFILLCOLOR:
        pcgfx.fill_color = w1;
        pcgfx.state_dirty = 1;
        break;
    case G_SETFOGCOLOR:
    case G_SETBLENDCOLOR:
    case G_SETPRIMCOLOR:
    case G_SETENVCOLOR: {
        uint8_t *c = op == G_SETFOGCOLOR ? pcgfx.fog : op == G_SETBLENDCOLOR ? pcgfx.blend
                   : op == G_SETPRIMCOLOR ? pcgfx.prim : pcgfx.env;
        c[0] = w1 >> 24;
        c[1] = w1 >> 16;
        c[2] = w1 >> 8;
        c[3] = w1;
        if (op == G_SETPRIMCOLOR) {
            pcgfx.prim_min_level = (w0 >> 8) & 0x1F;
            pcgfx.prim_lod = w0 & 0xFF;
        }
        pcgfx.state_dirty = 1;
        break;
    }
    case G_SETCOMBINE:
        pcgfx.cc_w0 = w0;
        pcgfx.cc_w1 = w1;
        pcgfx.state_dirty = 1;
        break;
    case G_SETTIMG:
        pcgfx.timg.addr = pcgfx_resolve(w1p);
        pcgfx.timg.fmt = (w0 >> 21) & 7;
        pcgfx.timg.siz = (w0 >> 19) & 3;
        pcgfx.timg.width = (uint16_t)((w0 & 0xFFF) + 1);
        break;
    case G_SETZIMG:
        if (pcgfx_hud_pending())   /* the HUD's queued 2D goes where it was drawn (gfx_hud.c) */
            pcgfx_hud_resolve();
        pcgfx.zimg = pcgfx_resolve(w1p);
        pcgfx.zimg_width = pcgfx.cimg.width;
        pcgfx.state_dirty = 1;
        break;
    case G_SETCIMG: {
        const uint8_t *a = pcgfx_resolve(w1p);
        if (pcgfx_hud_pending())   /* the HUD's queued 2D goes where it was drawn (gfx_hud.c) */
            pcgfx_hud_resolve();
        pcgfx.cimg.fmt = (w0 >> 21) & 7;
        pcgfx.cimg.siz = (w0 >> 19) & 3;
        pcgfx.cimg.width = (uint16_t)((w0 & 0xFFF) + 1);
        pcgfx.cimg.addr = a;
        if (a != pcgfx.zimg) {   /* the z buffer as color image clears the frame's depth: keep its geometry */
            pcgfx.wide_ox = wide_offset(pcgfx.cimg.width);
            pcgfx_hud_publish(pcgfx_frame_width(), pcgfx.wide_ox);   /* for pc_gfx_hud_dx (gfx_hud.c) */
        }
        if (a == NULL || a != pcgfx.zimg) {   /* drawing into the z buffer only ever clears it */
            int tw, th;
            pcgfx_target_dims(&tw, &th);
            s_target = pcgfx_gl_target(a, tw, th);
            pcgfx_gl_attach_depth(s_target, pcgfx.zimg);
        }
        pcgfx.state_dirty = 1;
        break;
    }
    default:
        pcgfx.stat_unknown++;
        break;
    }
}
