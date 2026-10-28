/* port/gfx/tests/tex_test.c: texture replacement (gfx_tex.c, docs/port/TEXTURES.md), headless, on synthetic
 * textures only:
 *   png       the PNG reader: every filter, palette + tRNS, gray + alpha, a round trip through the writer
 *   identity  a texture's name doesn't depend on its load (LOADBLOCK, LOADTILE), TMEM address, memory byte order or
 *             unused palette entries; a CI texture's palette is its own part; the names are fixed values
 *   roundtrip dump a texture, make a 3x replacement from the dumped PNG, draw it from a pack at scale 3
 *   seams     wrap and mirror at 2x: the upscaled period is seamless where it repeats and where it turns
 *   lod       a mip chain upscaled 2x picks the same levels as the original
 *   alpha     a cut-out upscaled stays binary, doesn't grow and gets no dark fringe; drawn with an alpha test too
 *   async     without RUSH_GFX_TEXTURE_SYNC the original is drawn until the worker is done, then the upscaled
 *   live      pc_gfx_set_textures switches at the next frame, and back
 * Usage: gfx_tex_test CASE|all SCRATCH_DIR */
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

#include "gfx_tex.h"
#include "pc_port.h"
#include "test_gbi.h"

#define PI 3.14159265358979323846
/* test and tool access (gfx_internal.h, which test_gbi.h's definitions clash with) */
int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
void pc_gfx_set_scale(int scale);

int pcgfx_png_write_rgba(const char *path, const uint8_t *rgba, int w, int h);
int pcgfx_png_read(const char *path, uint8_t **rgba, int *w, int *h, char *why, size_t why_size);
int pcgfx_png_decode(const uint8_t *data, size_t size, uint8_t **out, int *out_w, int *out_h, char *why,
                     size_t why_size);

/* ---------------------------------------------------------------- the memory functions port/ultra provides */

static uint8_t *s_rdram;

void *pc_n64_ptr(uint32_t a) {
    uint32_t p = a & 0x1FFFFFFFu;
    return p < PC_RDRAM_SIZE ? s_rdram + p : NULL;
}

uint32_t pc_n64_addr(const void *h) {
    const uint8_t *p = (const uint8_t *)h;
    return p >= s_rdram && p < s_rdram + PC_RDRAM_SIZE ? PC_RDRAM_BASE + (uint32_t)(p - s_rdram) : 0;
}

static const uint8_t *s_be_p;
static size_t s_be_n;
void pc_mem_register_be(const void *start, size_t size) {
    s_be_p = (const uint8_t *)start;
    s_be_n = size;
}
void pc_mem_unregister_be(const void *start) {
    if (s_be_p == start)
        s_be_p = NULL;
}
int pc_mem_is_be(const void *p) {
    return s_be_p != NULL && (const uint8_t *)p >= s_be_p && (const uint8_t *)p < s_be_p + s_be_n;
}

/* ---------------------------------------------------------------- frame and helpers */

#define FB_W 320
#define FB_H 240

typedef struct {
    int16_t ob[3];
    uint16_t flag;
    int16_t tc[2];
    uint8_t cn[4];
} Vtx;
typedef struct {
    int16_t vscale[4], vtrans[4];
} Vp;
typedef struct {
    uint32_t m[16];
} Mtx;

static uint16_t s_fb[FB_W * FB_H], s_zb[FB_W * FB_H];
static PcGfxWords s_list[16384];
static uint8_t *s_px;
static int s_px_w, s_px_h, s_fail;
static const char *s_case, *s_dir;
static Vp s_vp = {{FB_W * 2, FB_H * 2, 511, 0}, {FB_W * 2, FB_H * 2, 511, 0}};
static Mtx s_ident, s_ortho;

#define CHECK(c, ...)                                                                                                \
    do {                                                                                                         \
        if (!(c)) {                                                                                              \
            printf("  FAIL %s: ", s_case);                                                                       \
            printf(__VA_ARGS__);                                                                                 \
            printf("\n");                                                                                        \
            s_fail++;                                                                                            \
        }                                                                                                        \
    } while (0)

static void mtx_f2l(const float f[4][4], Mtx *m) {
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j += 2) {
            int32_t a = (int32_t)(f[i][j] * 65536.0f), b = (int32_t)(f[i][j + 1] * 65536.0f);
            m->m[i * 2 + j / 2] = ((uint32_t)a & 0xFFFF0000u) | ((uint32_t)b >> 16);
            m->m[8 + i * 2 + j / 2] = ((uint32_t)a << 16) | ((uint32_t)b & 0xFFFFu);
        }
}

static void mtx_setup(void) {
    float f[4][4] = {{0}};
    int i;
    for (i = 0; i < 4; i++)
        f[i][i] = 1.0f;
    mtx_f2l(f, &s_ident);
    memset(f, 0, sizeof(f));   /* ortho: pixels, y down, z -1000..1000 */
    f[0][0] = 2.0f / FB_W;
    f[1][1] = -2.0f / FB_H;
    f[2][2] = -2.0f / 2000.0f;
    f[3][0] = -1.0f;
    f[3][1] = 1.0f;
    f[3][3] = 1.0f;
    mtx_f2l(f, &s_ortho);
}

static void vtx(Vtx *v, int x, int y, int s, int t) {
    v->ob[0] = (int16_t)x, v->ob[1] = (int16_t)y, v->ob[2] = 0;
    v->flag = 0;
    v->tc[0] = (int16_t)s, v->tc[1] = (int16_t)t;
    v->cn[0] = v->cn[1] = v->cn[2] = v->cn[3] = 255;
}

static uint16_t rgba16(int r, int g, int b, int a) {
    return (uint16_t)((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | (a ? 1 : 0));
}

static void frame_begin(int r, int g, int b) {
    uint16_t c = rgba16(r, g, b, 1);
    gp = s_list;
    g_be_mode = 0;
    gsDPPipeSync();
    gsDPSetScissor(0, 0, FB_W, FB_H);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPSetRenderMode(0, 0);
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, FB_W, (uintptr_t)s_fb);
    gsDPSetFillColor((uint32_t)c << 16 | c);
    gsDPFillRectangle(0, 0, FB_W - 1, FB_H - 1);
    gsDPPipeSync();
    gsSPViewport((uintptr_t)&s_vp);
    gsSPMatrix((uintptr_t)&s_ortho, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gsSPMatrix((uintptr_t)&s_ident, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gsSPGeometryMode(0xFFFFFFFF, G_SHADE | G_SHADING_SMOOTH);
    gsDPSetCycleType(G_CYC_1CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    gsDPSetTextureLUT(G_TT_NONE);
    gsDPSetTextureFilter(G_TF_POINT);
    gsDPSetAlphaCompare(G_AC_NONE);
    gsDPSetDepthSource(G_ZS_PIXEL);
    cc1(CC_0, CC_0, CC_0, CC_TEXEL0, AC_0, AC_0, AC_0, AC_TEXEL0);
}

static void frame_end(void) {
    gsDPFullSync();
    gsSPEndDisplayList();
    pc_gfx_run_dl(s_list);
    pc_gfx_present(s_fb);
    free(s_px);
    pc_gfx_read_framebuffer(s_fb, NULL, &s_px_w, &s_px_h);
    s_px = (uint8_t *)malloc((size_t)s_px_w * s_px_h * 4);
    pc_gfx_read_framebuffer(s_fb, s_px, &s_px_w, &s_px_h);
}

/* the target's pixel (internal pixels) */
static const uint8_t *ipx(int x, int y) {
    return s_px + ((size_t)y * s_px_w + x) * 4;
}

static int scale(void) {
    return s_px_h / FB_H;
}

/* A fresh folder for a case: DIR/case/name, emptied. */
static void case_dir(char *out, size_t n, const char *name) {
    DIR *d;
    struct dirent *e;
    char sub[600];
    snprintf(sub, sizeof(sub), "%s/%s", s_dir, s_case);
    mkdir(s_dir, 0777);
    mkdir(sub, 0777);
    snprintf(out, n, "%s/%s", sub, name);
    mkdir(out, 0777);
    if ((d = opendir(out)) != NULL) {
        while ((e = readdir(d)) != NULL) {
            char p[1200];
            if (e->d_name[0] == '.')
                continue;
            snprintf(p, sizeof(p), "%s/%s", out, e->d_name);
            remove(p);
        }
        closedir(d);
    }
}

/* The replacement setting, as pc_gfx_init reads it: env, then configure. */
static void textures(const char *mode, const char *pack, const char *dump, const char *cache, int sync, int up2d) {
    int ok;
    setenv("RUSH_GFX_TEXTURE_PACK", pack ? pack : "", 1);
    setenv("RUSH_GFX_TEXTURE_DUMP", dump ? dump : "", 1);
    setenv("RUSH_GFX_TEXTURE_CACHE", cache ? cache : "", 1);
    setenv("RUSH_GFX_TEXTURE_SYNC", sync ? "1" : "0", 1);
    setenv("RUSH_GFX_UPSCALE_2D", up2d ? "1" : "0", 1);
    setenv("RUSH_GFX_TEXTURES", mode, 1);
    pcgfx_tex_configure();
    pc_gfx_set_textures(pc_gfx_parse_textures(mode, &ok));
}

static int list_dir(const char *dir, char names[][128], int max) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (d == NULL)
        return 0;
    while ((e = readdir(d)) != NULL && n < max)
        if (e->d_name[0] != '.')
            snprintf(names[n++], 128, "%s", e->d_name);
    closedir(d);
    return n;
}

/* ---------------------------------------------------------------- png */

static void be32p(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

static size_t put_chunk(uint8_t *o, const char *type, const uint8_t *d, uint32_t n) {
    uint32_t crc;
    be32p(o, n);
    memcpy(o + 4, type, 4);
    if (n)
        memcpy(o + 8, d, n);
    crc = (uint32_t)crc32(0, o + 4, 4 + n);
    be32p(o + 8 + n, crc);
    return 12 + n;
}

/* A PNG of rows already filtered (raw: each row's filter byte then its bytes). */
static size_t make_png(uint8_t *o, int w, int h, int bits, int ctype, const uint8_t *raw, size_t raw_n,
                       const uint8_t *plte, int nplte, const uint8_t *trns, int ntrns) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    uint8_t ihdr[13], z[8192];
    uLongf zn = sizeof(z);
    size_t n = 8;
    memcpy(o, sig, 8);
    be32p(ihdr, (uint32_t)w);
    be32p(ihdr + 4, (uint32_t)h);
    ihdr[8] = (uint8_t)bits, ihdr[9] = (uint8_t)ctype, ihdr[10] = ihdr[11] = ihdr[12] = 0;
    n += put_chunk(o + n, "IHDR", ihdr, 13);
    if (nplte)
        n += put_chunk(o + n, "PLTE", plte, (uint32_t)nplte * 3);
    if (ntrns)
        n += put_chunk(o + n, "tRNS", trns, (uint32_t)ntrns);
    compress2(z, &zn, raw, (uLong)raw_n, 9);
    n += put_chunk(o + n, "IDAT", z, (uint32_t)zn);
    n += put_chunk(o + n, "IEND", NULL, 0);
    return n;
}

static int paeth_ref(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static void case_png(void) {
    /* gray + alpha, 8 bits, 6 x 5: rows filtered with each filter in turn (0-4) */
    enum { W = 6, H = 5, BPP = 2 };
    uint8_t img[H][W * BPP], raw[H * (1 + W * BPP)], png[16384], *out = NULL;
    uint8_t pal[4 * 3] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 10, 20, 30}, trns[4] = {255, 128, 0, 255};
    uint8_t raw2[4 * (1 + 2)], rgba[7 * 3 * 4], *back = NULL;
    char why[160], path[700];
    int x, y, w, h, i;
    for (y = 0; y < H; y++)
        for (x = 0; x < W * BPP; x++)
            img[y][x] = (uint8_t)(x * 37 + y * 91 + (x * y) * 13);
    for (y = 0; y < H; y++) {
        uint8_t *r = raw + y * (1 + W * BPP);
        r[0] = (uint8_t)y;
        for (x = 0; x < W * BPP; x++) {
            int a = x >= BPP ? img[y][x - BPP] : 0, b = y ? img[y - 1][x] : 0;
            int c = (y && x >= BPP) ? img[y - 1][x - BPP] : 0;
            int pred = y == 0 ? 0 : y == 1 ? a : y == 2 ? b : y == 3 ? (a + b) / 2 : paeth_ref(a, b, c);
            r[1 + x] = (uint8_t)(img[y][x] - pred);
        }
    }
    {
        size_t n = make_png(png, W, H, 8, 4, raw, sizeof(raw), NULL, 0, NULL, 0);
        CHECK(pcgfx_png_decode(png, n, &out, &w, &h, why, sizeof(why)) == 0, "gray+alpha: %s", why);
        if (out != NULL) {
            int bad = 0;
            CHECK(w == W && h == H, "size %dx%d", w, h);
            for (y = 0; y < H; y++)
                for (x = 0; x < W; x++) {
                    const uint8_t *p = out + (y * W + x) * 4;
                    if (p[0] != img[y][x * 2] || p[1] != img[y][x * 2] || p[2] != img[y][x * 2] ||
                        p[3] != img[y][x * 2 + 1])
                        bad++;
                }
            CHECK(bad == 0, "gray+alpha: %d texels differ (filters 0-4)", bad);
        }
        free(out);
        out = NULL;
    }
    /* palette, 2 bits, 7 x 4, with tRNS: indices x + y mod 4 */
    for (y = 0; y < 4; y++) {
        uint8_t *r = raw2 + y * 3;
        r[0] = 0;
        r[1] = r[2] = 0;
        for (x = 0; x < 7; x++) {
            int v = (x + y) & 3;
            r[1 + x / 4] |= (uint8_t)(v << (6 - 2 * (x & 3)));
        }
    }
    {
        size_t n = make_png(png, 7, 4, 2, 3, raw2, sizeof(raw2), pal, 4, trns, 4);
        CHECK(pcgfx_png_decode(png, n, &out, &w, &h, why, sizeof(why)) == 0, "palette: %s", why);
        if (out != NULL) {
            int bad = 0;
            for (y = 0; y < 4; y++)
                for (x = 0; x < 7; x++) {
                    int v = (x + y) & 3;
                    const uint8_t *p = out + (y * 7 + x) * 4;
                    if (p[0] != pal[v * 3] || p[1] != pal[v * 3 + 1] || p[2] != pal[v * 3 + 2] || p[3] != trns[v])
                        bad++;
                }
            CHECK(bad == 0, "palette + tRNS: %d texels differ", bad);
        }
        free(out);
        out = NULL;
        png[20] ^= 0xFF;   /* the width's low byte: a damaged header */
        png[16] = 0xFF;
        CHECK(pcgfx_png_decode(png, n, &out, &w, &h, why, sizeof(why)) != 0, "a 2^31-wide image is refused");
        free(out);
        out = NULL;
    }
    /* the writer's RGBA, read back */
    for (i = 0; i < 7 * 3 * 4; i++)
        rgba[i] = (uint8_t)(i * 29 + 3);
    case_dir(path, sizeof(path), "png");
    strcat(path, "/rt.png");
    CHECK(pcgfx_png_write_rgba(path, rgba, 7, 3) == 0, "write");
    CHECK(pcgfx_png_read(path, &back, &w, &h, why, sizeof(why)) == 0, "read: %s", why);
    CHECK(back != NULL && w == 7 && h == 3 && memcmp(back, rgba, sizeof(rgba)) == 0, "the round trip differs");
    free(back);
}

/* ---------------------------------------------------------------- identity */

static uint16_t s_t16[64 * 64];
static uint8_t s_t8[64 * 64];
static uint8_t s_be16[16 * 8 * 2];
static uint16_t s_pal[256], s_pal2[256];

static void fill(void) {
    int i;
    for (i = 0; i < 64 * 64; i++) {
        s_t16[i] = (uint16_t)((i * 2654435761u) >> 9) | 1;
        s_t8[i] = (uint8_t)((i * 7 + (i >> 6) * 3) & 0x3F);   /* CI8 indices 0-63 */
    }
    for (i = 0; i < 256; i++) {
        s_pal[i] = (uint16_t)(i * 40503u + 7) | 1;
        s_pal2[i] = i < 64 ? s_pal[i] : (uint16_t)(i * 777u);   /* the same where the texels look */
    }
}

static void draw_rect(int x, int y, int w, int h) {
    gsSPTextureRectangle(x << 2, y << 2, (x + w) << 2, (y + h) << 2, 0, 0, 0, 1 << 10, 1 << 10);
}

static void case_identity(void) {
    char dump[700], names[16][128];
    int n, i, x, y;
    fill();
    case_dir(dump, sizeof(dump), "dump");
    textures("original", NULL, dump, NULL, 1, 0);
    /* RGBA16 16 x 8: LOADBLOCK at TMEM 0; LOADTILE of the same texels out of a 64-wide image at TMEM 0 with another
     * line; LOADBLOCK from big-endian memory at TMEM 128 */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 16; x++) {
            uint16_t v = s_t16[y * 16 + x];
            s_t16[1024 + (y + 2) * 64 + x + 5] = v;
            s_be16[(y * 16 + x) * 2] = (uint8_t)(v >> 8);
            s_be16[(y * 16 + x) * 2 + 1] = (uint8_t)v;
        }
    pc_mem_register_be(s_be16, sizeof(s_be16));
    frame_begin(0, 0, 0);
    load_texture_block((uintptr_t)s_t16, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(10, 10, 16, 8);
    load_texture_tile((uintptr_t)&s_t16[1024], G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 5, 2, 20, 9, 0, G_TX_CLAMP, G_TX_CLAMP,
                      0, 0);
    gsSPTextureRectangle(30 << 2, 10 << 2, 46 << 2, 18 << 2, 0, 5 << 5, 2 << 5, 1 << 10, 1 << 10);
    load_texture_block((uintptr_t)s_be16, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 8, 0, 128, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(50, 10, 16, 8);
    /* CI8 16 x 8 through two palettes that differ only past the largest index used */
    gsDPSetTextureLUT(G_TT_RGBA16);
    load_tlut_pal256((uintptr_t)s_pal);
    load_texture_block((uintptr_t)s_t8, G_IM_FMT_CI, G_IM_SIZ_8b, 16, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(10, 30, 16, 8);
    load_tlut_pal256((uintptr_t)s_pal2);
    load_texture_block((uintptr_t)s_t8, G_IM_FMT_CI, G_IM_SIZ_8b, 16, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(30, 30, 16, 8);
    /* CI4 16 x 8: palette bank 2, then bank 5 with the same colours (one name), then bank 5 with others (another
     * palette part, the same texel part) */
    load_tlut_pal16(2, (uintptr_t)&s_pal[32]);
    load_tlut_pal16(5, (uintptr_t)&s_pal[32]);
    load_texture_block((uintptr_t)s_t8, G_IM_FMT_CI, G_IM_SIZ_4b, 16, 8, 2, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(10, 50, 16, 8);
    load_texture_block((uintptr_t)s_t8, G_IM_FMT_CI, G_IM_SIZ_4b, 16, 8, 5, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(30, 50, 16, 8);
    load_tlut_pal16(5, (uintptr_t)&s_pal[100]);
    load_texture_block((uintptr_t)s_t8, G_IM_FMT_CI, G_IM_SIZ_4b, 16, 8, 5, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(50, 50, 16, 8);
    frame_end();
    pc_mem_unregister_be(s_be16);
    pcgfx_tex_wait_idle();
    n = list_dir(dump, names, 16);
    {
        int rgba = 0, ci8 = 0, ci4 = 0;
        char ci4a[128] = "", ci4b[128] = "";
        for (i = 0; i < n; i++) {
            if (!strncmp(names[i], "rgba16_16x8_", 12))
                rgba++;
            if (!strncmp(names[i], "ci8_16x8_", 9))
                ci8++;
            if (!strncmp(names[i], "ci4_16x8_", 9)) {
                if (ci4++ == 0)
                    snprintf(ci4a, sizeof(ci4a), "%s", names[i]);
                else
                    snprintf(ci4b, sizeof(ci4b), "%s", names[i]);
            }
        }
        CHECK(n == 4, "%d textures dumped, want 4 (one RGBA16 three ways, one CI8 two ways, two CI4)", n);
        CHECK(rgba == 1 && ci8 == 1 && ci4 == 2, "rgba16 %d, ci8 %d, ci4 %d", rgba, ci8, ci4);
        CHECK(ci4 == 2 && !strncmp(ci4a, ci4b, 9 + 16) && strcmp(ci4a, ci4b) != 0,
              "CI4: the texel part is the same, the palette part differs: %s %s", ci4a, ci4b);
    }
    /* The names are a property of the texels alone: the same on every host and every run. */
    {
        static const char *const want[] = {"rgba16_16x8_", "ci8_16x8_"};
        char path[900];
        uint8_t *img = NULL;
        int w, h, bad = 0;
        static const char *const golden[] = {
            "rgba16_16x8_81568407ed53e77c.png", "ci8_16x8_17369b244b606ef4_d9d9ac65.png",
            "ci4_16x8_9c536938f44ff2ba_826481f4.png", "ci4_16x8_9c536938f44ff2ba_008664d3.png"};
        int gi, j;
        for (gi = 0; gi < 4; gi++) {
            for (j = 0; j < n && strcmp(names[j], golden[gi]) != 0; j++)
                ;
            CHECK(j < n, "no %s: the names must be the same on every host", golden[gi]);
        }
        for (i = 0; i < n; i++)
            if (!strncmp(names[i], want[0], strlen(want[0]))) {
                snprintf(path, sizeof(path), "%s/%s", dump, names[i]);
                CHECK(pcgfx_png_read(path, &img, &w, &h, NULL, 0) == 0 && w == 16 && h == 8, "the dump reads back");
                for (y = 0; img != NULL && y < 8; y++)
                    for (x = 0; x < 16; x++) {
                        uint16_t v = s_t16[y * 16 + x];
                        const uint8_t *p = img + (y * 16 + x) * 4;
                        int r = (v >> 11) & 31, g = (v >> 6) & 31, b = (v >> 1) & 31;
                        if (p[0] != (r << 3 | r >> 2) || p[1] != (g << 3 | g >> 2) || p[2] != (b << 3 | b >> 2) ||
                            p[3] != 255)
                            bad++;
                    }
                free(img);
                img = NULL;
            }
        CHECK(bad == 0, "the dumped RGBA16 texels: %d differ", bad);
    }
}

/* ---------------------------------------------------------------- roundtrip */

static void case_roundtrip(void) {
    static uint16_t t[8 * 8];
    char dump[700], pack[700], names[8][128], path[900];
    uint8_t *img = NULL, *rep;
    int w, h, x, y, i, n, k = 3, bad = 0;
    for (i = 0; i < 64; i++)
        t[i] = rgba16((i & 7) * 30, (i >> 3) * 30, 100, 1);
    case_dir(dump, sizeof(dump), "dump");
    case_dir(pack, sizeof(pack), "pack");
    textures("original", NULL, dump, NULL, 1, 0);
    frame_begin(0, 0, 0);
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(20, 20, 8, 8);
    frame_end();
    pcgfx_tex_wait_idle();
    n = list_dir(dump, names, 8);
    CHECK(n == 1, "%d dumped", n);
    if (n != 1)
        return;
    snprintf(path, sizeof(path), "%s/%s", dump, names[0]);
    CHECK(pcgfx_png_read(path, &img, &w, &h, NULL, 0) == 0 && w == 8 && h == 8, "dump %s", names[0]);
    if (img == NULL)
        return;
    /* 3x: each texel a 3 x 3 block, its middle white */
    rep = (uint8_t *)malloc((size_t)24 * 24 * 4);
    for (y = 0; y < 24; y++)
        for (x = 0; x < 24; x++) {
            uint8_t *o = rep + (y * 24 + x) * 4;
            memcpy(o, img + ((y / k) * 8 + x / k) * 4, 4);
            if (x % k == 1 && y % k == 1)
                o[0] = o[1] = o[2] = 255;
        }
    snprintf(path, sizeof(path), "%s/%s", pack, names[0]);
    CHECK(pcgfx_png_write_rgba(path, rep, 24, 24) == 0, "write the replacement");
    textures("pack", pack, NULL, NULL, 1, 0);
    pc_gfx_set_scale(3);
    frame_begin(0, 0, 0);
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(20, 20, 8, 8);   /* point sampled, 1:1: each N64 pixel shows its texel's 3 x 3 block */
    frame_end();
    CHECK(scale() == 3, "scale %d", scale());
    for (y = 0; y < 24; y++)
        for (x = 0; x < 24; x++) {
            const uint8_t *p = ipx(20 * 3 + x, 20 * 3 + y), *e = rep + (y * 24 + x) * 4;
            if (abs(p[0] - e[0]) > 2 || abs(p[1] - e[1]) > 2 || abs(p[2] - e[2]) > 2) {
                if (bad++ < 3)
                    printf("  (%d,%d): %d,%d,%d want %d,%d,%d\n", x, y, p[0], p[1], p[2], e[0], e[1], e[2]);
            }
        }
    CHECK(bad == 0, "the replacement drawn: %d of 576 pixels differ", bad);
    {
        unsigned up, disk, replaced, dumped;
        pcgfx_tex_counts(&up, &disk, &replaced, &dumped);
        CHECK(replaced == 1, "%u replaced", replaced);
    }
    /* At scale 1 the replacement is box filtered down to the texture's size: the original's colours, nearly */
    pc_gfx_set_scale(1);
    frame_begin(0, 0, 0);
    load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8, 8, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    draw_rect(20, 20, 8, 8);
    frame_end();
    bad = 0;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            const uint8_t *p = ipx(20 + x, 20 + y), *o = img + (y * 8 + x) * 4;
            int want = (o[0] * 8 + 255) / 9;
            if (abs(p[0] - want) > 3)
                bad++;
        }
    CHECK(bad == 0, "at scale 1, box filtered: %d differ", bad);
    free(rep);
    free(img);
    textures("original", NULL, NULL, NULL, 1, 0);
    pc_gfx_set_scale(2);
}

/* ---------------------------------------------------------------- seams */

static int ch(const uint8_t *img, int w, int x, int y) {
    return img[(y * w + x) * 4];
}

static void case_seams(void) {
    enum { N = 16 };
    uint8_t in[N * N * 4], mir[2 * N * N * 4], *out = NULL;
    int x, y, worst_in, jump;
    for (y = 0; y < N; y++)
        for (x = 0; x < N; x++) {
            uint8_t *p = in + (y * N + x) * 4;
            p[0] = (uint8_t)(128 + 100 * sin(2 * PI * x / N) * cos(2 * PI * y / N));
            p[1] = (uint8_t)(x * 15);   /* a ramp: a jump at the wrap */
            p[2] = 50;
            p[3] = 255;
        }
    /* wrap: the period's last column flows into its first as any column into the next */
    CHECK(pcgfx_tex_upscale_image(in, N, N, 1, 1, 1, 2, &out) == 0, "upscale");
    if (out == NULL)
        return;
    worst_in = 0;
    jump = 0;
    for (y = 0; y < 2 * N; y++) {
        for (x = 0; x + 1 < 2 * N; x++) {
            int d = abs(ch(out, 2 * N, x + 1, y) - ch(out, 2 * N, x, y));
            if (d > worst_in)
                worst_in = d;
        }
        if (abs(ch(out, 2 * N, 0, y) - ch(out, 2 * N, 2 * N - 1, y)) > jump)
            jump = abs(ch(out, 2 * N, 0, y) - ch(out, 2 * N, 2 * N - 1, y));
        if (abs(ch(out, 2 * N, y, 0) - ch(out, 2 * N, y, 2 * N - 1)) > jump)
            jump = abs(ch(out, 2 * N, y, 0) - ch(out, 2 * N, y, 2 * N - 1));
    }
    CHECK(jump <= worst_in + 4, "wrap seam: %d across the edge, at most %d inside", jump, worst_in);
    free(out);
    out = NULL;
    /* mirror: the period is the texture and its mirror image (as the GL texture holds it), wrapping; upscaled, both
     * halves mirror each other and the turn has no seam */
    for (y = 0; y < N; y++)
        for (x = 0; x < 2 * N; x++)
            memcpy(mir + (y * 2 * N + x) * 4, in + (y * N + (x < N ? x : 2 * N - 1 - x)) * 4, 4);
    CHECK(pcgfx_tex_upscale_image(mir, 2 * N, N, 1, 1, 1, 2, &out) == 0, "upscale mirrored");
    if (out != NULL) {
        int asym = 0;
        for (y = 0; y < 2 * N; y++)
            for (x = 0; x < 2 * N; x++) {
                int a = (out + (y * 4 * N + x) * 4)[1], b = (out + (y * 4 * N + 4 * N - 1 - x) * 4)[1];
                if (abs(a - b) > asym)
                    asym = abs(a - b);
            }
        CHECK(asym <= 12, "mirror: the halves differ by up to %d", asym);
        jump = 0;
        for (y = 0; y < 2 * N; y++) {   /* at the turn (x 2N-1 | 2N) and the wrap (4N-1 | 0): twins, no step */
            int t1 = abs((out + (y * 4 * N + 2 * N - 1) * 4)[1] - (out + (y * 4 * N + 2 * N) * 4)[1]);
            int t2 = abs((out + (y * 4 * N + 4 * N - 1) * 4)[1] - (out + (y * 4 * N) * 4)[1]);
            if (t1 > jump) jump = t1;
            if (t2 > jump) jump = t2;
        }
        CHECK(jump <= 8, "mirror: a step of %d where it turns", jump);
    }
    free(out);
    out = NULL;

    /* Drawn: a wrapping 16 x 16 texture over two periods, upscaled (rectangles too), bilinear: both periods alike */
    {
        static uint16_t t[N * N];
        int i, bad = 0, s = 0;
        for (i = 0; i < N * N; i++)
            t[i] = rgba16(in[i * 4], in[i * 4 + 1], in[i * 4 + 2], 1);
        textures("upscaled", NULL, NULL, NULL, 1, 1);
        pc_gfx_set_scale(2);
        frame_begin(0, 0, 0);
        gsDPSetTextureFilter(G_TF_BILERP);
        load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, N, N, 0, 0, G_TX_WRAP, G_TX_WRAP, 4, 4);
        gsSPTextureRectangle(16 << 2, 16 << 2, (16 + 2 * N) << 2, (16 + N) << 2, 0, 0, 0, 1 << 10, 1 << 10);
        frame_end();
        s = scale();
        for (y = 0; y < N * s; y++)
            for (x = 0; x < N * s; x++)
                if (memcmp(ipx(16 * s + x, 16 * s + y), ipx(16 * s + N * s + x, 16 * s + y), 3) != 0)
                    bad++;
        CHECK(bad == 0, "the two periods differ at %d pixels", bad);
        {
            unsigned up, disk, rep, dumped;
            pcgfx_tex_counts(&up, &disk, &rep, &dumped);
            CHECK(up + disk >= 1, "nothing was upscaled");
        }
        textures("original", NULL, NULL, NULL, 1, 0);
    }
}

/* ---------------------------------------------------------------- lod */

static int nearest_level(const uint8_t *p) {
    static const int col[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
    int k, best = 0, bd = 1 << 30;
    for (k = 0; k < 4; k++) {
        int d = (p[0] - col[k][0]) * (p[0] - col[k][0]) + (p[1] - col[k][1]) * (p[1] - col[k][1]) +
                (p[2] - col[k][2]) * (p[2] - col[k][2]);
        if (d < bd)
            bd = d, best = k;
    }
    return best;
}

/* A pixel mostly of one level: its nearest level colour much nearer than the next (not a blend near halfway). */
static int level_clear(const uint8_t *p) {
    static const int col[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
    int k, d, b1 = 1 << 30, b2 = 1 << 30;
    for (k = 0; k < 4; k++) {
        d = (p[0] - col[k][0]) * (p[0] - col[k][0]) + (p[1] - col[k][1]) * (p[1] - col[k][1]) +
            (p[2] - col[k][2]) * (p[2] - col[k][2]);
        if (d < b1)
            b2 = b1, b1 = d;
        else if (d < b2)
            b2 = d;
    }
    return b1 * 4 < b2;
}

static void lod_frame(void) {
    static uint16_t lv[4][32 * 32];
    static Vtx v[6][4];
    static const float r[6] = {0.5f, 1.5f, 2.0f, 3.0f, 5.0f, 12.0f};
    static const int col[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
    int k, i, tmem = 0;
    for (k = 0; k < 4; k++) {   /* each level its colour, a checker of it and a darker shade */
        int w = 32 >> k;
        for (i = 0; i < w * w; i++) {
            int d = ((i % w) ^ (i / w)) & 1 ? 200 : 255;
            lv[k][i] = rgba16(col[k][0] * d / 255, col[k][1] * d / 255, col[k][2] * d / 255, 1);
        }
    }
    frame_begin(0, 0, 0);
    gsDPSetCycleType(G_CYC_2CYCLE);
    gsDPSetRenderMode(RM_OPA_SURF(1), RM_OPA_SURF(2));
    gsDPSetTextureFilter(G_TF_BILERP);
    for (k = 0; k < 4; k++) {
        int w = 32 >> k;
        gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, (uintptr_t)lv[k]);
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, tmem, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gsDPLoadSync();
        gsDPLoadBlock(G_TX_LOADTILE, 0, 0, w * w - 1, calc_dxt(w, 2));
        gsDPPipeSync();
        gsDPSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, (w * 2 + 7) >> 3, tmem, k, 0, G_TX_WRAP, 5 - k, k, G_TX_WRAP,
                    5 - k, k);
        gsDPSetTileSize(k, 0, 0, (w - 1) << 2, (w - 1) << 2);
        tmem += w * w * 2 / 8;
    }
    gsSPTexture(0xFFFF, 0xFFFF, 3, 0, 1);
    gsDPSetCombineLERP(CC_TEXEL1, CC_TEXEL0, CC_LOD_FRACTION, CC_TEXEL0, AC_TEXEL1, AC_TEXEL0, AC_LOD_FRACTION,
                       AC_TEXEL0, CC_0, CC_0, CC_0, CC_COMBINED, AC_0, AC_0, AC_0, AC_COMBINED);
    gsDPSetTextureLOD(G_TL_LOD);
    for (i = 0; i < 6; i++) {   /* 40 x 40 pixels each, steps from magnified to past the last level; a slant */
        int x = 8 + 50 * i, st = (int)lrintf(40 * r[i] * 32);
        vtx(&v[i][0], x, 20, 0, 0);
        vtx(&v[i][1], x, 60, 0, st);
        vtx(&v[i][2], x + 40, 20, st * 2, 0);
        vtx(&v[i][3], x + 40, 60, st * 2, st);
        gsSPVertex((uintptr_t)v[i], 4, 0);
        gsSP2Triangles(0, 1, 2, 2, 1, 3);
    }
    frame_end();
}

static void case_lod(void) {
    uint8_t *orig;
    int x, y, s, differ = 0, total = 0, seen[4] = {0, 0, 0, 0};
    unsigned up0, up1, d, rp, dm;
    pc_gfx_set_scale(2);
    textures("original", NULL, NULL, NULL, 1, 0);
    lod_frame();
    s = scale();
    orig = (uint8_t *)malloc((size_t)s_px_w * s_px_h * 4);
    memcpy(orig, s_px, (size_t)s_px_w * s_px_h * 4);
    pcgfx_tex_counts(&up0, &d, &rp, &dm);
    textures("upscaled", NULL, NULL, NULL, 1, 1);   /* the triangles are orthographic: 2D upscaled too */
    lod_frame();
    pcgfx_tex_counts(&up1, &d, &rp, &dm);
    CHECK(up1 + d > up0, "no level was upscaled");
    for (y = 20 * s; y < 60 * s; y += 3)
        for (x = 8 * s; x < 308 * s; x += 3) {
            const uint8_t *a = orig + ((size_t)y * s_px_w + x) * 4, *b = ipx(x, y);
            if (a[0] + a[1] + a[2] == 0)
                continue;   /* between the quads */
            total++;
            seen[nearest_level(a)]++;
            if (level_clear(a) && nearest_level(a) != nearest_level(b))
                differ++;
        }
    CHECK(seen[0] && seen[1] && seen[2] && seen[3], "the frame shows every level: %d %d %d %d", seen[0], seen[1],
          seen[2], seen[3]);
    CHECK(differ * 1000 <= total, "the level differs at %d of %d samples", differ, total);
    free(orig);
    textures("original", NULL, NULL, NULL, 1, 0);
}

/* ---------------------------------------------------------------- alpha */

static void case_alpha(void) {
    enum { N = 16 };
    uint8_t in[N * N * 4], *out = NULL;
    int x, y, opaque_in = 0, opaque_out = 0, nonbinary = 0, grown = 0, dark = 0, kept = 0;
    for (y = 0; y < N; y++)
        for (x = 0; x < N; x++) {
            uint8_t *p = in + (y * N + x) * 4;
            int inside = (x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 30.0;
            p[0] = inside ? 240 : 0;
            p[1] = inside ? 230 : 0;
            p[2] = inside ? 220 : 0;
            p[3] = inside ? 255 : 0;
            opaque_in += inside;
        }
    CHECK(pcgfx_tex_upscale_image(in, N, N, 0, 0, 1, 2, &out) == 0, "upscale");
    if (out == NULL)
        return;
    for (y = 0; y < 2 * N; y++)
        for (x = 0; x < 2 * N; x++) {
            const uint8_t *p = out + (y * 2 * N + x) * 4, *s = in + ((y / 2) * N + x / 2) * 4;
            int dx, dy, near = 0;
            if (p[3] != 0 && p[3] != 255)
                nonbinary++;
            if (p[3] == 255) {
                opaque_out++;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        int xx = x / 2 + dx, yy = y / 2 + dy;
                        if (xx >= 0 && yy >= 0 && xx < N && yy < N && in[(yy * N + xx) * 4 + 3])
                            near = 1;
                    }
                if (!near)
                    grown++;
                if (p[0] < 200 || p[1] < 190 || p[2] < 180)
                    dark++;
            } else if (s[3] == 0 && (p[0] || p[1] || p[2])) {
                int edge = 0;   /* next to the cut-out a transparent texel keeps the cut-out's colour */
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if (xx >= 0 && yy >= 0 && xx < 2 * N && yy < 2 * N && out[(yy * 2 * N + xx) * 4 + 3])
                            edge = 1;
                    }
                if (!edge)
                    kept++;
            }
        }
    CHECK(nonbinary == 0, "%d texels with alpha between 0 and 255", nonbinary);
    CHECK(opaque_out * 100 >= opaque_in * 4 * 85 && opaque_out * 100 <= opaque_in * 4 * 115,
          "the cut-out's area: %d texels, the original's %d x 4", opaque_out, opaque_in);
    CHECK(grown == 0, "%d opaque texels away from the original's", grown);
    CHECK(dark == 0, "%d opaque texels darkened at the edge (a fringe)", dark);
    CHECK(kept == 0, "%d transparent texels lost their colour", kept);
    free(out);

    /* Drawn with an alpha test over red: nothing dark along the edge */
    {
        static uint16_t t[N * N];
        int i, s, bad = 0, white = 0;
        for (i = 0; i < N * N; i++)
            t[i] = rgba16(in[i * 4], in[i * 4 + 1], in[i * 4 + 2], in[i * 4 + 3]);
        textures("upscaled", NULL, NULL, NULL, 1, 1);
        pc_gfx_set_scale(2);
        frame_begin(255, 0, 0);
        gsDPSetTextureFilter(G_TF_BILERP);
        gsDPSetRenderMode(RM_OPA_SURF(1) | 0, RM_OPA_SURF(2) | 0);
        gsDPSetAlphaCompare(G_AC_THRESHOLD);
        gsDPSetBlendColor(0, 0, 0, 128);
        load_texture_block((uintptr_t)t, G_IM_FMT_RGBA, G_IM_SIZ_16b, N, N, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
        gsSPTextureRectangle(40 << 2, 40 << 2, (40 + 4 * N) << 2, (40 + 4 * N) << 2, 0, 0, 0, 1 << 8, 1 << 8);
        frame_end();
        s = scale();
        for (y = 40 * s; y < (40 + 4 * N) * s; y++)
            for (x = 40 * s; x < (40 + 4 * N) * s; x++) {
                const uint8_t *p = ipx(x, y);
                if (p[0] > 200 && p[1] < 40)
                    continue;   /* the red behind */
                if (p[0] >= 200 && p[1] >= 190)
                    white++;
                else
                    bad++;
            }
        CHECK(white > 0, "the cut-out isn't drawn");
        CHECK(bad == 0, "%d pixels neither the cut-out's colour nor the background", bad);
        textures("original", NULL, NULL, NULL, 1, 0);
    }
}

/* ---------------------------------------------------------------- async and live */

static uint16_t s_tq[32 * 32];

static void quad_frame(void) {
    frame_begin(0, 0, 0);
    gsDPSetTextureFilter(G_TF_BILERP);
    load_texture_block((uintptr_t)s_tq, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0);
    gsSPTextureRectangle(10 << 2, 10 << 2, (10 + 128) << 2, (10 + 128) << 2, 0, 0, 0, 1 << 8, 1 << 8);
    frame_end();
}

static uint8_t *grab(void) {
    uint8_t *p = (uint8_t *)malloc((size_t)s_px_w * s_px_h * 4);
    memcpy(p, s_px, (size_t)s_px_w * s_px_h * 4);
    return p;
}

static void case_async(void) {
    char cache[700];
    uint8_t *orig, *a, *b;
    size_t n;
    int i;
    unsigned up, disk, rp, dm, up2, disk2;
    for (i = 0; i < 32 * 32; i++)
        s_tq[i] = rgba16(((i & 31) * 8) ^ ((i >> 5) & 4 ? 64 : 0), (i >> 5) * 8, (i * 13) & 255, 1);
    case_dir(cache, sizeof(cache), "cache");
    pc_gfx_set_scale(2);
    textures("original", NULL, NULL, cache, 0, 1);
    quad_frame();
    orig = grab();
    n = (size_t)s_px_w * s_px_h * 4;
    textures("upscaled", NULL, NULL, cache, 0, 1);
    pcgfx_tex_counts(&up, &disk, &rp, &dm);
    quad_frame();   /* first sight: noted, the original drawn */
    CHECK(memcmp(s_px, orig, n) == 0, "the first frame isn't the original's");
    quad_frame();   /* second sight: queued, the original drawn */
    CHECK(memcmp(s_px, orig, n) == 0, "the second frame isn't the original's");
    pcgfx_tex_wait_idle();
    quad_frame();   /* done: the upscaled one */
    a = grab();
    CHECK(memcmp(a, orig, n) != 0, "the upscaled texture never came");
    pcgfx_tex_counts(&up2, &disk2, &rp, &dm);
    CHECK(up2 + disk2 == up + disk + 1, "one job: %u upscaled, %u from disk", up2 - up, disk2 - disk);
    /* live: back to the original at the next frame, then upscaled again from memory at once */
    pc_gfx_set_textures(PCGFX_TEX_ORIGINAL);
    quad_frame();
    CHECK(memcmp(s_px, orig, n) == 0, "original again");
    pc_gfx_set_textures(PCGFX_TEX_UPSCALED);
    quad_frame();
    CHECK(memcmp(s_px, a, n) == 0, "upscaled again, from memory, at once");
    /* a new run (the tables forgotten): from the disk cache */
    pcgfx_tex_shutdown();
    textures("upscaled", NULL, NULL, cache, 0, 1);
    quad_frame();
    quad_frame();
    pcgfx_tex_wait_idle();
    quad_frame();
    b = grab();
    pcgfx_tex_counts(&up, &disk, &rp, &dm);
    CHECK(disk == 1 && up == 0, "the second run: %u from the disk cache, %u upscaled", disk, up);
    CHECK(memcmp(a, b, n) == 0, "the disk cache's texture differs");
    free(orig);
    free(a);
    free(b);
    textures("original", NULL, NULL, NULL, 1, 0);
}

typedef struct {
    const char *name;
    void (*fn)(void);
} Case;

static const Case s_cases[] = {
    {"png", case_png},     {"identity", case_identity}, {"roundtrip", case_roundtrip}, {"seams", case_seams},
    {"lod", case_lod},     {"alpha", case_alpha},       {"async", case_async},
};

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all";
    int i, ran = 0, failed = 0;
    s_dir = argc > 2 ? argv[2] : ".";
    s_rdram = (uint8_t *)calloc(1, PC_RDRAM_SIZE);
    mtx_setup();
    setenv("RUSH_GFX_SCALE", "2", 1);
    setenv("RUSH_GFX_TEXTURE_SYNC", "1", 1);
    if (pc_gfx_init(1, 640, 480) != 0) {
        printf("FAIL: pc_gfx_init\n");
        return 2;
    }
    for (i = 0; i < (int)(sizeof(s_cases) / sizeof(s_cases[0])); i++) {
        int before = s_fail;
        if (strcmp(which, "all") != 0 && strcmp(which, s_cases[i].name) != 0)
            continue;
        s_case = s_cases[i].name;
        s_cases[i].fn();
        ran++;
        if (s_fail != before)
            failed++;
        printf("%s %s\n", s_fail != before ? "FAIL" : "ok  ", s_case);
    }
    pc_gfx_shutdown();
    free(s_px);
    free(s_rdram);
    if (ran == 0) {
        printf("no case named %s\n", which);
        return 2;
    }
    printf("%d/%d cases passed\n", ran - failed, ran);
    return failed ? 1 : 0;
}
