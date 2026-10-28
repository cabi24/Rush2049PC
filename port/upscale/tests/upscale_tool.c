/* port/upscale/tests/upscale_tool.c: the upscaler from the command line (docs/port/UPSCALE.md).
 *
 *   upscale_tool run net|cubic|bilinear IN OUT   a batch file through the upscaler (tools/upscale/evaluate.py)
 *   upscale_tool bench [N [T]]                   time N synthetic textures of the N64's usual sizes (default 300) on T
 *                                                threads per texture (default: one per processor, up to 8)
 *   upscale_tool info                            the network's name and size
 *
 * Batch files: "UPS1", a little-endian u32 count, then per texture four little-endian i32 (w, h, wrap_s, wrap_t)
 * and w * h RGBA8 texels. The output has the same form, with 2w x 2h texels.
 */
#define _POSIX_C_SOURCE 200809L   /* clock_gettime */
#include "upscale.h"
#include "upscale_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static double now(void) {
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / (double)f.QuadPart;
}
#else
#include <time.h>
static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
#endif

static int rd32(FILE *f, int *v) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4)
        return -1;
    *v = (int)((unsigned)b[0] | (unsigned)b[1] << 8 | (unsigned)b[2] << 16 | (unsigned)b[3] << 24);
    return 0;
}

static void wr32(FILE *f, int v) {
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                           (unsigned char)(v >> 24) };
    fwrite(b, 1, 4, f);
}

static int run(const char *m, const char *inp, const char *outp) {
    int method = !strcmp(m, "net") ? PC_UPSCALE_NET : !strcmp(m, "cubic") ? PC_UPSCALE_CUBIC
               : !strcmp(m, "bilinear") ? PC_UPSCALE_BILINEAR : -1;
    FILE *f = fopen(inp, "rb"), *o = fopen(outp, "wb");
    char magic[4];
    int count;
    if (method < 0 || !f || !o || fread(magic, 1, 4, f) != 4 || memcmp(magic, "UPS1", 4) || rd32(f, &count)) {
        fprintf(stderr, "upscale_tool: bad arguments or input\n");
        return 2;
    }
    fwrite("UPS1", 1, 4, o);
    wr32(o, count);
    double t = 0;
    for (int i = 0; i < count; i++) {
        int w, h, ws, wt;
        if (rd32(f, &w) || rd32(f, &h) || rd32(f, &ws) || rd32(f, &wt) || w < 1 || h < 1) {
            fprintf(stderr, "upscale_tool: texture %d: bad header\n", i);
            return 2;
        }
        uint8_t *in = malloc((size_t)w * h * 4), *out = malloc((size_t)w * h * 16);
        if (!in || !out || fread(in, 4, (size_t)w * h, f) != (size_t)w * h) {
            fprintf(stderr, "upscale_tool: texture %d: short input\n", i);
            return 2;
        }
        double t0 = now();
        if (pc_upscale_rgba2x_method(in, w, h, ws, wt, out, method)) {
            fprintf(stderr, "upscale_tool: texture %d (%dx%d) refused\n", i, w, h);
            return 1;
        }
        t += now() - t0;
        wr32(o, 2 * w);
        wr32(o, 2 * h);
        wr32(o, ws);
        wr32(o, wt);
        fwrite(out, 4, (size_t)w * h * 4, o);
        free(in);
        free(out);
    }
    fclose(f);
    fclose(o);
    fprintf(stderr, "%d textures, %.3f s (%s)\n", count, t, m);
    return 0;
}

/* Sizes as N64 textures come (4 KB of TMEM): mostly 32x32 and 64x32, some 16x16, 64x64, 32x16, 128x32. */
static int bench(int n) {
    static const int sizes[][2] = { { 32, 32 }, { 32, 32 }, { 32, 32 }, { 64, 32 }, { 64, 32 }, { 16, 16 },
                                    { 16, 16 }, { 64, 64 }, { 32, 16 }, { 128, 32 }, { 32, 64 }, { 8, 8 } };
    const int ns = (int)(sizeof sizes / sizeof sizes[0]);
    uint8_t *in = malloc(128 * 128 * 4), *out = malloc(256 * 256 * 4);
    unsigned s = 12345;
    double t[PC_UPSCALE_METHODS] = { 0 };
    long texels = 0;
    for (int i = 0; i < n; i++) {
        const int w = sizes[i % ns][0], h = sizes[i % ns][1];
        for (int k = 0; k < w * h * 4; k++) {
            s = s * 1103515245u + 12345u;
            in[k] = (uint8_t)((k & 3) == 3 ? ((i % 4 == 0) ? (s >> 16 & 1) * 255 : 255) : (s >> 16) & 255);
        }
        texels += w * h;
        for (int m = 0; m < PC_UPSCALE_METHODS; m++) {
            double t0 = now();
            if (pc_upscale_rgba2x_method(in, w, h, i & 1, i & 1, out, m))
                return 1;
            t[m] += now() - t0;
        }
    }
    printf("%d textures, %ld texels: network %.3f s, Catmull-Rom %.3f s, bilinear %.3f s\n", n, texels, t[0], t[1],
           t[2]);
    free(in);
    free(out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "run"))
        return run(argv[2], argv[3], argv[4]);
    if (argc >= 2 && !strcmp(argv[1], "bench"))
    {
        if (argc >= 4)
            pc_upscale_set_threads(atoi(argv[3]));
        return bench(argc >= 3 ? atoi(argv[2]) : 300);
    }
    if (argc >= 2 && !strcmp(argv[1], "info")) {
        printf("%s, %d parameters\n", pc_upscale_model_name, pc_upscale_model_params);
        return 0;
    }
    fprintf(stderr, "usage: upscale_tool run net|cubic|bilinear IN OUT | bench [N] | info\n");
    return 2;
}
