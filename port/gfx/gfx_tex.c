/* port/gfx/gfx_tex.c: texture replacement (gfx_tex.h, docs/port/TEXTURES.md).
 *
 * Three tables, each keyed by a 64-bit hash, under one lock: the upscaler's results, the pack's replacements, the
 * textures dumped. The slow work (upscaling, reading and writing PNG and cache files) is a job for one worker
 * thread; the renderer draws the original until the job is done, and uploads the result at a later frame. */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "gfx_internal.h"
#include "gfx_tex.h"
#include "upscale.h"

int pcgfx_png_write_rgba(const char *path, const uint8_t *rgba, int w, int h);
int pcgfx_png_read(const char *path, uint8_t **rgba, int *w, int *h, char *why, size_t why_size);

#define TEX_PATH 1024                     /* a folder from the settings */
#define TEX_FILE (TEX_PATH + 160)         /* a file in one */
#define TBL_BUCKETS 4096
#define UP_MEM_LIMIT ((size_t)384 << 20)   /* upscaled images kept in memory, bytes */
#define SEEN_FRAMES 600                    /* a texture seen once is forgotten after this many frames */
/* The edge handling's version: part of the disk cache's folder name with the upscaler's fingerprint. */
#define PROCESS_VERSION 2

static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
#endif
}

uint64_t pcgfx_tex_mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h *= 0xFF51AFD7ED558CCDull;
    return h ^ (h >> 31);
}

static const char *fmt_name(int fmt) {
    static const char *const n[8] = {"rgba", "yuv", "ci", "ia", "i", "fmt5", "fmt6", "fmt7"};
    return n[fmt & 7];
}

void pcgfx_tex_name(const PcgfxTexId *id, int with_palette, char *out, size_t size) {
    if (id->fmt == 2 && with_palette)
        snprintf(out, size, "%s%d_%ux%u_%016llx_%08x", fmt_name(id->fmt), 4 << id->siz, id->w, id->h,
                 (unsigned long long)id->texels, id->palette);
    else
        snprintf(out, size, "%s%d_%ux%u_%016llx", fmt_name(id->fmt), 4 << id->siz, id->w, id->h,
                 (unsigned long long)id->texels);
}

/* ---------------------------------------------------------------- settings */

static struct {
    int mode, factor_env, up2d, sync, verbose;
    char pack[TEX_PATH], cache[TEX_PATH], dump[TEX_PATH];
    uint32_t gen;
    int last_factor, last_limit;
} C;

static void env_path(const char *name, char *out) {
    const char *v = getenv(name);
    snprintf(out, TEX_PATH, "%s", v != NULL ? v : "");
}

static void tables_forget(void);

int pc_gfx_parse_textures(const char *text, int *ok) {
    *ok = 1;
    if (text == NULL || !*text || !strcmp(text, "original") || !strcmp(text, "off"))
        return PCGFX_TEX_ORIGINAL;
    if (!strcmp(text, "upscaled"))
        return PCGFX_TEX_UPSCALED;
    if (!strcmp(text, "pack"))
        return PCGFX_TEX_PACK;
    *ok = 0;
    return PCGFX_TEX_ORIGINAL;
}

void pcgfx_tex_configure(void) {
    const char *v = getenv("RUSH_GFX_TEXTURES");
    int ok;
    C.mode = pc_gfx_parse_textures(v, &ok);
    if (!ok)
        fprintf(stderr, "gfx: RUSH_GFX_TEXTURES=%s is not original, upscaled or pack; ignored\n", v);
    v = getenv("RUSH_GFX_UPSCALE");
    C.factor_env = v != NULL ? atoi(v) : 0;
    if (C.factor_env != 0 && C.factor_env != 1 && C.factor_env != 2 && C.factor_env != 4)
        C.factor_env = 0;
    v = getenv("RUSH_GFX_UPSCALE_2D");
    C.up2d = v != NULL && atoi(v) != 0;
    v = getenv("RUSH_GFX_TEXTURE_SYNC");
    C.sync = v != NULL && atoi(v) != 0;
    v = getenv("RUSH_GFX_VERBOSE");
    C.verbose = v != NULL ? atoi(v) : 0;
    env_path("RUSH_GFX_TEXTURE_PACK", C.pack);
    env_path("RUSH_GFX_TEXTURE_CACHE", C.cache);
    env_path("RUSH_GFX_TEXTURE_DUMP", C.dump);
    if (!C.pack[0])
        snprintf(C.pack, sizeof(C.pack), "textures");
    C.gen++;
    tables_forget();   /* the pack and the dump may be other folders now */
}

int pcgfx_tex_mode(void) {
    return C.mode;
}

static void pack_forget(void);

void pcgfx_tex_set_mode(int mode) {
    if (mode < PCGFX_TEX_ORIGINAL || mode > PCGFX_TEX_PACK)
        mode = PCGFX_TEX_ORIGINAL;
    if (mode != C.mode) {
        if (mode == PCGFX_TEX_PACK)
            pack_forget();   /* the folder is read again: files added since are found */
        C.mode = mode;
        C.gen++;
    }
}

int pcgfx_tex_variant(int is3d) {
    if (C.mode == PCGFX_TEX_PACK)
        return 1;
    if (C.mode == PCGFX_TEX_UPSCALED)
        return (is3d || C.up2d) && pcgfx_tex_factor() > 1;
    return 0;
}

/* The upscale factor: the largest power of two up to the internal scale in effect, at most 4, fractions too
 * (native at 2880 x 1620, 6.76: 4x; at 1366 x 768, 3.2: 2x). */
int pcgfx_tex_factor(void) {
    float s = pcgfx_cfg.scale;
    if (C.factor_env)
        return C.factor_env;
    return s >= 4.0f ? 4 : s >= 2.0f ? 2 : 1;
}

/* A pack's texture is kept up to this many times the N64's: the internal scale, a fraction rounded up. */
int pcgfx_tex_pack_limit(void) {
    return pcgfx_cfg.scale < 1.0f ? 1 : (int)ceilf(pcgfx_cfg.scale - 1e-4f);
}

int pcgfx_tex_dumping(void) {
    return C.dump[0] != 0;
}

uint32_t pcgfx_tex_generation(void) {
    /* The factor and the pack's limit follow the internal scale, which may change at any frame (auto). */
    if (C.mode == PCGFX_TEX_UPSCALED && pcgfx_tex_factor() != C.last_factor) {
        C.last_factor = pcgfx_tex_factor();
        C.gen++;
    }
    if (C.mode == PCGFX_TEX_PACK && pcgfx_tex_pack_limit() != C.last_limit) {
        C.last_limit = pcgfx_tex_pack_limit();
        C.gen++;
    }
    return C.gen;
}

/* ---------------------------------------------------------------- tables and the worker */

enum { N_SEEN, N_QUEUED, N_DONE, N_FAILED, N_ABSENT };
enum { J_UPSCALE, J_PACK, J_DUMP };

typedef struct Node {
    uint64_t key;
    int state;
    uint32_t frame;        /* N_SEEN: the frame first seen; N_DONE: the frame last used */
    uint8_t *img;          /* N_SEEN: the image to upscale; N_DONE: the result */
    int w, h, rs, rt, indep, factor;
    char path[TEX_FILE];   /* J_PACK: the file */
    struct Node *next;
} Node;

typedef struct Job {
    int type;
    Node *node;            /* J_UPSCALE, J_PACK */
    uint8_t *img;          /* J_DUMP: the texels */
    int w, h;
    char path[TEX_FILE];   /* J_DUMP: where */
    struct Job *next;
} Job;

typedef struct {
    Node *b[TBL_BUCKETS];
    int count;
} Table;

static Table s_up, s_pack, s_dumped;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_cond = PTHREAD_COND_INITIALIZER, s_idle = PTHREAD_COND_INITIALIZER;
static pthread_t s_thread;
static int s_thread_on, s_quit, s_busy;
static Job *s_head, *s_tail;
static uint32_t s_done_gen, s_seen_gen;
static size_t s_up_bytes;
static char s_fingerprint[32];

static struct {
    unsigned up_jobs, up_disk, up_failed, pack_loaded, pack_bad, dumped;
    double up_ms, up_ms_max;
} St;

static Node *tbl_find(Table *t, uint64_t key) {
    Node *n;
    for (n = t->b[key % TBL_BUCKETS]; n != NULL; n = n->next)
        if (n->key == key)
            return n;
    return NULL;
}

static Node *tbl_add(Table *t, uint64_t key) {
    Node *n = (Node *)calloc(1, sizeof(*n));
    if (n == NULL)
        return NULL;
    n->key = key;
    n->next = t->b[key % TBL_BUCKETS];
    t->b[key % TBL_BUCKETS] = n;
    t->count++;
    return n;
}

/* Frees the nodes keep() refuses (under the lock; never a node a queued job holds). */
static void tbl_sweep(Table *t, int (*keep)(const Node *, void *), void *user) {
    int b;
    for (b = 0; b < TBL_BUCKETS; b++) {
        Node **pp = &t->b[b];
        while (*pp) {
            Node *n = *pp;
            if (n->state != N_QUEUED && !keep(n, user)) {
                *pp = n->next;
                if (n->state == N_DONE && t == &s_up)
                    s_up_bytes -= (size_t)n->w * n->h * 4;
                free(n->img);
                free(n);
                t->count--;
            } else {
                pp = &n->next;
            }
        }
    }
}

static int keep_none(const Node *n, void *u) {
    (void)n;
    (void)u;
    return 0;
}

static int mkdirs(const char *dir) {
    char p[TEX_PATH];
    size_t i, n = strlen(dir);
    if (n == 0 || n >= sizeof(p))
        return -1;
    memcpy(p, dir, n + 1);
    for (i = 1; i <= n; i++) {
        if (p[i] == '/' || p[i] == '\\' || p[i] == '\0') {
            char c = p[i];
            p[i] = '\0';
#ifdef _WIN32
            if (!(i == 2 && p[1] == ':'))
                _mkdir(p);
#else
            mkdir(p, 0777);
#endif
            p[i] = c;
        }
    }
    {
        struct stat st;
        return stat(dir, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : -1;
    }
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

/* The disk cache: DIR/<fingerprint>/<key>_<factor>.bin: "R2TX", w, h (little-endian u32), zlib'd RGBA8. */
static void cache_path(char *out, uint64_t key, int factor) {
    snprintf(out, TEX_FILE, "%s/%s/%016llx_%d.bin", C.cache, s_fingerprint, (unsigned long long)key, factor);
}

static uint8_t *cache_read(uint64_t key, int factor, int w, int h) {
    char path[TEX_FILE];
    FILE *f;
    uint8_t hdr[12], *z = NULL, *img = NULL;
    long size;
    uLongf len = (uLongf)w * h * 4;
    if (!C.cache[0])
        return NULL;
    cache_path(path, key, factor);
    f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    if (fread(hdr, 1, 12, f) == 12 && !memcmp(hdr, "R2TX", 4) &&
        (hdr[4] | hdr[5] << 8 | hdr[6] << 16 | (uint32_t)hdr[7] << 24) == (uint32_t)w &&
        (hdr[8] | hdr[9] << 8 | hdr[10] << 16 | (uint32_t)hdr[11] << 24) == (uint32_t)h && fseek(f, 0, SEEK_END) == 0 &&
        (size = ftell(f)) > 12 && fseek(f, 12, SEEK_SET) == 0) {
        z = (uint8_t *)malloc((size_t)size - 12);
        img = (uint8_t *)malloc(len);
        if (z == NULL || img == NULL || fread(z, 1, (size_t)size - 12, f) != (size_t)size - 12 ||
            uncompress(img, &len, z, (uLong)(size - 12)) != Z_OK || len != (uLongf)w * h * 4) {
            free(img);
            img = NULL;
        }
    }
    fclose(f);
    free(z);
    return img;
}

static void cache_write(uint64_t key, int factor, const uint8_t *img, int w, int h) {
    char path[TEX_FILE], tmp[TEX_FILE + 32], dir[TEX_FILE];
    uLongf zlen = compressBound((uLong)w * h * 4);
    uint8_t *z, hdr[12];
    FILE *f;
    int i, ok;
    if (!C.cache[0])
        return;
    snprintf(dir, sizeof(dir), "%s/%s", C.cache, s_fingerprint);
    if (mkdirs(dir) != 0)
        return;
    z = (uint8_t *)malloc(zlen);
    if (z == NULL || compress2(z, &zlen, img, (uLong)w * h * 4, 6) != Z_OK) {
        free(z);
        return;
    }
    cache_path(path, key, factor);
    snprintf(tmp, sizeof(tmp), "%s.%lu.tmp", path, (unsigned long)(now_ms() * 1000.0) & 0xFFFFFFul);
    memcpy(hdr, "R2TX", 4);
    for (i = 0; i < 4; i++) {
        hdr[4 + i] = (uint8_t)((uint32_t)w >> (8 * i));
        hdr[8 + i] = (uint8_t)((uint32_t)h >> (8 * i));
    }
    f = fopen(tmp, "wb");
    ok = f != NULL && fwrite(hdr, 1, 12, f) == 12 && fwrite(z, 1, zlen, f) == zlen;
    if (f != NULL && fclose(f) != 0)
        ok = 0;
    free(z);
#ifdef _WIN32
    if (ok)
        remove(path);   /* rename doesn't replace there */
#endif
    if (!ok || rename(tmp, path) != 0)
        remove(tmp);
}

/* ---------------------------------------------------------------- the edge handling around the upscaler */

static int wrapi(int i, int n, int wrap) {
    if (wrap) {
        i %= n;
        return i < 0 ? i + n : i;
    }
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

/* Transparent texels take the colour of the opaque ones around them (a few texels out, then the average of all),
 * so that the upscaler's colour near a cut-out's edge comes from the opaque side only. */
static void bleed(uint8_t *img, int w, int h, int rs, int rt) {
    uint8_t *filled = (uint8_t *)malloc((size_t)w * h), *next;
    unsigned long sum[3] = {0, 0, 0}, cnt = 0;
    int pass, x, y, i, any;
    if (filled == NULL)
        return;
    next = (uint8_t *)malloc((size_t)w * h);
    if (next == NULL) {
        free(filled);
        return;
    }
    for (i = 0; i < w * h; i++) {
        filled[i] = img[i * 4 + 3] != 0;
        if (filled[i]) {
            sum[0] += img[i * 4];
            sum[1] += img[i * 4 + 1];
            sum[2] += img[i * 4 + 2];
            cnt++;
        }
    }
    if (cnt == 0 || cnt == (unsigned long)w * h) {
        free(filled);
        free(next);
        return;
    }
    for (pass = 0; pass < 4; pass++) {
        any = 0;
        memcpy(next, filled, (size_t)w * h);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                unsigned s[3] = {0, 0, 0}, n = 0;
                int dx, dy;
                if (filled[y * w + x])
                    continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        int xx = wrapi(x + dx, w, rs), yy = wrapi(y + dy, h, rt);
                        if (filled[yy * w + xx]) {
                            const uint8_t *p = img + ((size_t)yy * w + xx) * 4;
                            s[0] += p[0], s[1] += p[1], s[2] += p[2], n++;
                        }
                    }
                if (n) {
                    uint8_t *o = img + ((size_t)y * w + x) * 4;
                    o[0] = (uint8_t)(s[0] / n), o[1] = (uint8_t)(s[1] / n), o[2] = (uint8_t)(s[2] / n);
                    next[y * w + x] = 1;
                    any = 1;
                }
            }
        memcpy(filled, next, (size_t)w * h);
        if (!any)
            break;
    }
    for (i = 0; i < w * h; i++)
        if (!filled[i]) {
            img[i * 4] = (uint8_t)(sum[0] / cnt);
            img[i * 4 + 1] = (uint8_t)(sum[1] / cnt);
            img[i * 4 + 2] = (uint8_t)(sum[2] / cnt);
        }
    free(filled);
    free(next);
}

int pcgfx_tex_upscale_image(const uint8_t *img, int w, int h, int rs, int rt, int alpha_indep, int factor,
                            uint8_t **out) {
    int opaque = 1, binary = 1, i, cw = w, ch = h, f, X, Y, W = w * factor, H = h * factor;
    uint8_t *cur;
    *out = NULL;
    if (factor != 2 && factor != 4)
        return -1;
    for (i = 0; i < w * h; i++) {
        uint8_t a = img[i * 4 + 3];
        if (a != 255)
            opaque = 0;
        if (a != 0 && a != 255)
            binary = 0;
    }
    cur = (uint8_t *)malloc((size_t)w * h * 4);
    if (cur == NULL)
        return -1;
    memcpy(cur, img, (size_t)w * h * 4);
    if (alpha_indep && !opaque)
        bleed(cur, w, h, rs, rt);
    for (f = 1; f < factor; f *= 2) {
        uint8_t *next = (uint8_t *)malloc((size_t)cw * ch * 16);
        if (next == NULL || pc_upscale_rgba2x(cur, cw, ch, rs, rt, next) != 0) {
            free(next);
            free(cur);
            return -1;
        }
        free(cur);
        cur = next;
        cw *= 2;
        ch *= 2;
    }
    /* Alpha: opaque stays opaque; a cut-out (alpha only 0 or 255) stays binary, cut at half along the upscaler's
     * contour, so an alpha test's edge neither grows nor blurs. */
    for (i = 0; i < W * H; i++) {
        uint8_t *o = cur + (size_t)i * 4;
        if (opaque)
            o[3] = 255;
        else if (alpha_indep && binary)
            o[3] = o[3] >= 128 ? 255 : 0;
    }
    /* Where a texel stays transparent and was transparent, its colour is the original's again (a mode that ignores
     * alpha shows what the N64 shows there), except next to an opaque texel: there it keeps the opaque side's
     * colour, so that filtering across the edge brings no dark fringe in. */
    if (!opaque && alpha_indep)
        for (Y = 0; Y < H; Y++)
            for (X = 0; X < W; X++) {
                uint8_t *o = cur + ((size_t)Y * W + X) * 4;
                const uint8_t *s = img + ((size_t)(Y / factor) * w + X / factor) * 4;
                int dx, dy, edge = 0;
                if (s[3] != 0 || o[3] >= 8)
                    continue;
                for (dy = -1; dy <= 1 && !edge; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        if (cur[((size_t)wrapi(Y + dy, H, rt) * W + wrapi(X + dx, W, rs)) * 4 + 3] >= 8) {
                            edge = 1;
                            break;
                        }
                if (!edge)
                    o[0] = s[0], o[1] = s[1], o[2] = s[2];
                if (!binary)
                    o[3] = 0;
            }
    *out = cur;
    return 0;
}

/* The upscaler's fingerprint: a hash of what it makes of a fixed pattern, so the disk cache of another upscaler
 * (or another version of the edge handling) is never read. */
static void fingerprint(void) {
    uint8_t in[8 * 8 * 4], *out = NULL;
    uint64_t h = pcgfx_tex_mix(0x52325458, PROCESS_VERSION);
    int i;
    for (i = 0; i < 64; i++) {
        int x = i & 7, y = i >> 3;
        in[i * 4] = (uint8_t)(x * 36);
        in[i * 4 + 1] = (uint8_t)(y * 36);
        in[i * 4 + 2] = (uint8_t)(((x ^ y) & 1) * 255);
        in[i * 4 + 3] = (uint8_t)(x + y < 9 ? 255 : 0);
    }
    if (pcgfx_tex_upscale_image(in, 8, 8, 1, 0, 1, 2, &out) == 0) {
        for (i = 0; i < 16 * 16 * 4; i++)
            h = pcgfx_tex_mix(h, out[i]);
    }
    free(out);
    snprintf(s_fingerprint, sizeof(s_fingerprint), "%016llx", (unsigned long long)h);
}

/* ---------------------------------------------------------------- jobs */

static void run_upscale(Node *n) {
    int W = n->w * n->factor, H = n->h * n->factor;
    uint8_t *res;
    double t0 = now_ms(), dt;
    if (!s_fingerprint[0])
        fingerprint();
    res = cache_read(n->key, n->factor, W, H);
    if (res != NULL) {
        pthread_mutex_lock(&s_lock);
        St.up_disk++;
    } else {
        if (pcgfx_tex_upscale_image(n->img, n->w, n->h, n->rs, n->rt, n->indep, n->factor, &res) != 0)
            res = NULL;
        dt = now_ms() - t0;
        if (res != NULL)
            cache_write(n->key, n->factor, res, W, H);
        pthread_mutex_lock(&s_lock);
        if (res != NULL) {
            St.up_jobs++;
            St.up_ms += dt;
            if (dt > St.up_ms_max)
                St.up_ms_max = dt;
        } else {
            St.up_failed++;
        }
    }
    free(n->img);
    n->img = res;
    if (res != NULL) {
        n->w = W;
        n->h = H;
        n->state = N_DONE;
        s_up_bytes += (size_t)W * H * 4;
    } else {
        n->state = N_FAILED;
    }
    s_done_gen++;
    pthread_mutex_unlock(&s_lock);
}

static void run_pack(Node *n) {
    uint8_t *img = NULL;
    int w = 0, h = 0, ok;
    char why[160];
    ok = pcgfx_png_read(n->path, &img, &w, &h, why, sizeof(why)) == 0;
    if (ok && (w % n->w != 0 || h % n->h != 0 || w / n->w != h / n->h || w / n->w > 16)) {
        snprintf(why, sizeof(why), "it is %dx%d: it must be the texture's %dx%d times a whole number up to 16", w, h,
                 n->w, n->h);
        ok = 0;
    }
    if (!ok) {
        fprintf(stderr, "gfx: texture pack: %s: %s; the original is drawn\n", n->path, why);
        free(img);
        img = NULL;
    }
    pthread_mutex_lock(&s_lock);
    n->img = img;
    if (ok) {
        n->w = w;
        n->h = h;
        n->state = N_DONE;
        St.pack_loaded++;
    } else {
        n->state = N_FAILED;
        St.pack_bad++;
    }
    s_done_gen++;
    pthread_mutex_unlock(&s_lock);
}

static void run_dump(Job *j) {
    if (!file_exists(j->path) && pcgfx_png_write_rgba(j->path, j->img, j->w, j->h) == 0) {
        pthread_mutex_lock(&s_lock);
        St.dumped++;
        pthread_mutex_unlock(&s_lock);
    }
}

static void run_job(Job *j) {
    if (j->type == J_UPSCALE)
        run_upscale(j->node);
    else if (j->type == J_PACK)
        run_pack(j->node);
    else
        run_dump(j);
    free(j->img);
    free(j);
}

static void *worker(void *arg) {
    (void)arg;
    pthread_mutex_lock(&s_lock);
    for (;;) {
        Job *j;
        while (s_head == NULL && !s_quit) {
            s_busy = 0;
            pthread_cond_broadcast(&s_idle);
            pthread_cond_wait(&s_cond, &s_lock);
        }
        if (s_quit)
            break;
        j = s_head;
        s_head = j->next;
        if (s_head == NULL)
            s_tail = NULL;
        s_busy = 1;
        pthread_mutex_unlock(&s_lock);
        run_job(j);
        pthread_mutex_lock(&s_lock);
    }
    s_busy = 0;
    pthread_cond_broadcast(&s_idle);
    pthread_mutex_unlock(&s_lock);
    return NULL;
}

/* Queue a job (under the lock), or run it at once (sync, or no thread): the lock is dropped meanwhile. */
static void submit(Job *j) {
    if (C.sync || (!s_thread_on && pthread_create(&s_thread, NULL, worker, NULL) != 0)) {
        pthread_mutex_unlock(&s_lock);
        run_job(j);
        pthread_mutex_lock(&s_lock);
        return;
    }
    s_thread_on = 1;
    j->next = NULL;
    if (s_tail != NULL)
        s_tail->next = j;
    else
        s_head = j;
    s_tail = j;
    s_busy = 1;
    pthread_cond_signal(&s_cond);
}

/* ---------------------------------------------------------------- the renderer's calls */

int pcgfx_tex_upscaled(uint64_t ukey, int factor, uint32_t frame, const uint8_t *img, int w, int h, int rs, int rt,
                       int alpha_indep, const uint8_t **out) {
    uint64_t key = pcgfx_tex_mix(ukey, (uint64_t)factor);
    Node *n;
    int r = PCGFX_TEX_WAIT;
    pthread_mutex_lock(&s_lock);
    n = tbl_find(&s_up, key);
    if (n == NULL) {
        if (img == NULL || (n = tbl_add(&s_up, key)) == NULL) {
            pthread_mutex_unlock(&s_lock);
            return PCGFX_TEX_NONE;
        }
        n->img = (uint8_t *)malloc((size_t)w * h * 4);
        if (n->img == NULL) {
            n->state = N_FAILED;
            pthread_mutex_unlock(&s_lock);
            return PCGFX_TEX_NONE;
        }
        memcpy(n->img, img, (size_t)w * h * 4);
        n->state = N_SEEN;
        n->frame = frame;
        n->w = w, n->h = h, n->rs = rs, n->rt = rt, n->indep = alpha_indep, n->factor = factor;
        s_seen_gen++;
    }
    n->key = key;
    if (n->state == N_SEEN && (frame != n->frame || C.sync)) {
        /* seen in a second frame (a texture that is new in every frame is never upscaled), or at once */
        Job *j = (Job *)calloc(1, sizeof(*j));
        if (j != NULL) {
            j->type = J_UPSCALE;
            j->node = n;
            n->state = N_QUEUED;
            submit(j);
        }
    }
    if (n->state == N_DONE) {
        n->frame = frame;
        *out = n->img;
        r = PCGFX_TEX_READY;
    } else if (n->state == N_FAILED) {
        r = PCGFX_TEX_NONE;
    }
    pthread_mutex_unlock(&s_lock);
    return r;
}

/* The pack's files: their names (lower case, without .png) and paths, read once from the folder and the folders
 * in it. */
typedef struct PackFile {
    uint64_t name_hash;
    char *path;
    struct PackFile *next;
} PackFile;
static PackFile *s_files[TBL_BUCKETS];
static int s_files_read, s_files_count;

static uint64_t str_hash(const char *s) {
    uint64_t h = 1469598103934665603ull;
    for (; *s; s++) {
        int c = *s >= 'A' && *s <= 'Z' ? *s + 32 : *s;
        h = (h ^ (uint64_t)(unsigned char)c) * 1099511628211ull;
    }
    return h;
}

static void scan_dir(const char *dir, int depth) {
    DIR *d;
    struct dirent *e;
    if (depth > 4 || (d = opendir(dir)) == NULL)
        return;
    while ((e = readdir(d)) != NULL) {
        char path[TEX_FILE], name[256];
        size_t n = strlen(e->d_name);
        struct stat st;
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            scan_dir(path, depth + 1);
        } else if (n > 4 && n < sizeof(name) && (!strcmp(e->d_name + n - 4, ".png") ||
                                                 !strcmp(e->d_name + n - 4, ".PNG"))) {
            PackFile *f = (PackFile *)calloc(1, sizeof(*f));
            memcpy(name, e->d_name, n - 4);
            name[n - 4] = '\0';
            if (f == NULL || (f->path = strdup(path)) == NULL) {
                free(f);
                continue;
            }
            f->name_hash = str_hash(name);
            f->next = s_files[f->name_hash % TBL_BUCKETS];
            s_files[f->name_hash % TBL_BUCKETS] = f;
            s_files_count++;
        }
    }
    closedir(d);
}

static const char *pack_file(const char *name) {
    uint64_t h = str_hash(name);
    PackFile *f;
    if (!s_files_read) {
        s_files_read = 1;
        scan_dir(C.pack, 0);
        fprintf(stderr, "gfx: texture pack %s: %d PNG files\n", C.pack, s_files_count);
    }
    for (f = s_files[h % TBL_BUCKETS]; f != NULL; f = f->next)
        if (f->name_hash == h)
            return f->path;
    return NULL;
}

static void pack_forget(void) {
    int b;
    pthread_mutex_lock(&s_lock);
    for (b = 0; b < TBL_BUCKETS; b++)
        while (s_files[b]) {
            PackFile *f = s_files[b];
            s_files[b] = f->next;
            free(f->path);
            free(f);
        }
    s_files_read = 0;
    s_files_count = 0;
    tbl_sweep(&s_pack, keep_none, NULL);
    pthread_mutex_unlock(&s_lock);
}

int pcgfx_tex_pack(const PcgfxTexId *id, const uint8_t **out, int *w, int *h) {
    char name[96];
    uint64_t key = pcgfx_tex_mix(pcgfx_tex_mix(id->texels, id->palette), (uint64_t)id->w << 16 | id->h);
    Node *n;
    int r = PCGFX_TEX_WAIT;
    pthread_mutex_lock(&s_lock);
    n = tbl_find(&s_pack, key);
    if (n == NULL && (n = tbl_add(&s_pack, key)) != NULL) {
        const char *path;
        pcgfx_tex_name(id, 1, name, sizeof(name));
        path = pack_file(name);
        if (path == NULL && id->fmt == 2) {   /* CI: a replacement for every palette */
            pcgfx_tex_name(id, 0, name, sizeof(name));
            path = pack_file(name);
        }
        n->w = id->w;
        n->h = id->h;
        if (path == NULL) {
            n->state = N_ABSENT;
        } else {
            Job *j = (Job *)calloc(1, sizeof(*j));
            snprintf(n->path, sizeof(n->path), "%s", path);
            n->state = N_FAILED;
            if (j != NULL) {
                j->type = J_PACK;
                j->node = n;
                n->state = N_QUEUED;
                submit(j);
            }
        }
    }
    if (n == NULL || n->state == N_ABSENT || n->state == N_FAILED)
        r = PCGFX_TEX_NONE;
    else if (n->state == N_DONE) {
        *out = n->img;
        *w = n->w;
        *h = n->h;
        r = PCGFX_TEX_READY;
    }
    pthread_mutex_unlock(&s_lock);
    return r;
}

void pcgfx_tex_dump(const PcgfxTexId *id, const uint8_t *rgba) {
    uint64_t key = pcgfx_tex_mix(pcgfx_tex_mix(id->texels, id->palette), (uint64_t)id->w << 16 | id->h);
    char name[96];
    Job *j;
    static int dir_made;
    pthread_mutex_lock(&s_lock);
    if (tbl_find(&s_dumped, key) != NULL || tbl_add(&s_dumped, key) == NULL) {
        pthread_mutex_unlock(&s_lock);
        return;
    }
    if (!dir_made) {
        dir_made = 1;
        if (mkdirs(C.dump) != 0)
            fprintf(stderr, "gfx: texture dump: cannot make %s\n", C.dump);
        else
            fprintf(stderr, "gfx: texture dump: every texture drawn goes into %s\n", C.dump);
    }
    j = (Job *)calloc(1, sizeof(*j));
    if (j != NULL && (j->img = (uint8_t *)malloc((size_t)id->w * id->h * 4)) != NULL) {
        memcpy(j->img, rgba, (size_t)id->w * id->h * 4);
        j->type = J_DUMP;
        j->w = id->w;
        j->h = id->h;
        pcgfx_tex_name(id, 1, name, sizeof(name));
        snprintf(j->path, sizeof(j->path), "%s/%s.png", C.dump, name);
        submit(j);
    } else {
        free(j);
    }
    pthread_mutex_unlock(&s_lock);
}

static int keep_up(const Node *n, void *user) {
    uint32_t frame = *(const uint32_t *)user;
    if (n->state == N_SEEN)   /* a texture seen once, long ago: its copy goes when there are many */
        return frame - n->frame < SEEN_FRAMES || s_up.count < 2048;
    if (n->state == N_DONE && s_up_bytes > UP_MEM_LIMIT)
        return frame - n->frame < 60;   /* over the limit: only what the last two seconds used */
    return 1;
}

int pcgfx_tex_new_frame(uint32_t frame) {
    static uint32_t seen_done, last_sweep;
    int r;
    pthread_mutex_lock(&s_lock);
    r = s_done_gen != seen_done;
    seen_done = s_done_gen;
    if (frame - last_sweep >= 256 || s_up_bytes > UP_MEM_LIMIT) {
        last_sweep = frame;
        tbl_sweep(&s_up, keep_up, &frame);
    }
    pthread_mutex_unlock(&s_lock);
    return r;
}

static void tables_forget(void) {
    pack_forget();
    pthread_mutex_lock(&s_lock);
    tbl_sweep(&s_dumped, keep_none, NULL);
    pthread_mutex_unlock(&s_lock);
}

void pcgfx_tex_counts(unsigned *upscaled, unsigned *from_disk, unsigned *replaced, unsigned *dumped) {
    pthread_mutex_lock(&s_lock);
    *upscaled = St.up_jobs;
    *from_disk = St.up_disk;
    *replaced = St.pack_loaded;
    *dumped = St.dumped;
    pthread_mutex_unlock(&s_lock);
}

void pcgfx_tex_wait_idle(void) {
    pthread_mutex_lock(&s_lock);
    while (s_thread_on && (s_head != NULL || s_busy))
        pthread_cond_wait(&s_idle, &s_lock);
    pthread_mutex_unlock(&s_lock);
}

void pcgfx_tex_stats_print(void) {
    pthread_mutex_lock(&s_lock);
    if (St.up_jobs || St.up_disk || St.up_failed)
        fprintf(stderr, "gfx: textures: %u upscaled (%.2f ms each on the worker, at most %.2f), %u from the disk "
                "cache, %u not; %.1f MB in memory\n", St.up_jobs, St.up_jobs ? St.up_ms / St.up_jobs : 0.0,
                St.up_ms_max, St.up_disk, St.up_failed, s_up_bytes / 1048576.0);
    if (St.pack_loaded || St.pack_bad)
        fprintf(stderr, "gfx: textures: %u replaced from the pack, %u pack files refused\n", St.pack_loaded,
                St.pack_bad);
    if (St.dumped)
        fprintf(stderr, "gfx: textures: %u dumped into %s\n", St.dumped, C.dump);
    pthread_mutex_unlock(&s_lock);
}

void pcgfx_tex_shutdown(void) {
    Job *j;
    pthread_mutex_lock(&s_lock);
    s_quit = 1;
    pthread_cond_broadcast(&s_cond);
    pthread_mutex_unlock(&s_lock);
    if (s_thread_on)
        pthread_join(s_thread, NULL);
    s_thread_on = 0;
    pcgfx_tex_stats_print();
    pthread_mutex_lock(&s_lock);
    while ((j = s_head) != NULL) {
        s_head = j->next;
        free(j->img);
        free(j);
    }
    s_tail = NULL;
    s_quit = 0;
    /* the jobs' nodes: no job holds one now */
    {
        int b;
        Table *t[3] = {&s_up, &s_pack, &s_dumped};
        for (b = 0; b < 3; b++) {
            int i;
            for (i = 0; i < TBL_BUCKETS; i++)
                while (t[b]->b[i]) {
                    Node *n = t[b]->b[i];
                    t[b]->b[i] = n->next;
                    free(n->img);
                    free(n);
                }
            t[b]->count = 0;
        }
    }
    s_up_bytes = 0;
    memset(&St, 0, sizeof(St));
    pthread_mutex_unlock(&s_lock);
    pack_forget();
}
