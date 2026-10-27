/* port/gfx/gfx_png.c: minimal PNG writer (8-bit RGB, no filtering) on zlib. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "gfx_internal.h"

static void be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t hdr[8];
    uint32_t crc;
    be32(hdr, len);
    memcpy(hdr + 4, type, 4);
    crc = (uint32_t)crc32(0, hdr + 4, 4);
    if (len)
        crc = (uint32_t)crc32(crc, data, len);
    if (fwrite(hdr, 1, 8, f) != 8 || (len && fwrite(data, 1, len, f) != len))
        return -1;
    be32(hdr, crc);
    return fwrite(hdr, 1, 4, f) == 4 ? 0 : -1;
}

int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    size_t raw_len = (size_t)h * (1 + (size_t)w * 3), x, y;
    uLongf zlen = compressBound((uLong)raw_len);
    uint8_t *raw = (uint8_t *)malloc(raw_len), *z = (uint8_t *)malloc(zlen), ihdr[13];
    FILE *f = NULL;
    int rc = -1;
    if (raw == NULL || z == NULL || w <= 0 || h <= 0)
        goto out;
    for (y = 0; y < (size_t)h; y++) {
        uint8_t *row = raw + y * (1 + (size_t)w * 3);
        row[0] = 0;
        for (x = 0; x < (size_t)w; x++)
            memcpy(row + 1 + x * 3, rgba + (y * (size_t)w + x) * 4, 3);
    }
    if (compress2(z, &zlen, raw, (uLong)raw_len, 6) != Z_OK)
        goto out;
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;    /* bit depth */
    ihdr[9] = 2;    /* RGB */
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    f = fopen(path, "wb");
    if (f == NULL)
        goto out;
    if (fwrite(sig, 1, 8, f) != 8 || chunk(f, "IHDR", ihdr, 13) || chunk(f, "IDAT", z, (uint32_t)zlen) ||
        chunk(f, "IEND", NULL, 0))
        goto out;
    rc = 0;
out:
    if (f != NULL && fclose(f) != 0)
        rc = -1;
    free(raw);
    free(z);
    return rc;
}
