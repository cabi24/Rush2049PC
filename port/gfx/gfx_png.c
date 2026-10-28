/* port/gfx/gfx_png.c: a minimal PNG writer (8-bit RGB or RGBA, no filtering) and reader on zlib. Both written for
 * this port from the PNG specification (ISO/IEC 15948); the reader takes what image editors and upscalers write:
 * every color type and bit depth, palette and tRNS transparency, all five filters. Not interlaced images (Adam7):
 * those are refused with a message (texture packs, docs/port/TEXTURES.md). */
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

static uint32_t rd_be32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
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

static int png_write(const char *path, const uint8_t *rgba, int w, int h, int channels) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    size_t stride = 1 + (size_t)w * channels, raw_len = (size_t)h * stride, x, y;
    uLongf zlen = compressBound((uLong)raw_len);
    uint8_t *raw = (uint8_t *)malloc(raw_len), *z = (uint8_t *)malloc(zlen), ihdr[13];
    FILE *f = NULL;
    int rc = -1;
    if (raw == NULL || z == NULL || w <= 0 || h <= 0)
        goto out;
    for (y = 0; y < (size_t)h; y++) {
        uint8_t *row = raw + y * stride;
        row[0] = 0;
        for (x = 0; x < (size_t)w; x++)
            memcpy(row + 1 + x * channels, rgba + (y * (size_t)w + x) * 4, (size_t)channels);
    }
    if (compress2(z, &zlen, raw, (uLong)raw_len, 6) != Z_OK)
        goto out;
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;                          /* bit depth */
    ihdr[9] = channels == 4 ? 6 : 2;      /* RGBA, RGB */
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

int pcgfx_png_write(const char *path, const uint8_t *rgba, int w, int h) {
    return png_write(path, rgba, w, h, 3);
}

int pcgfx_png_write_rgba(const char *path, const uint8_t *rgba, int w, int h) {
    return png_write(path, rgba, w, h, 4);
}

/* ---------------------------------------------------------------- reading */

#define PNG_MAX_DIM 16384

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* A sample of bits bits (1, 2, 4, 8, 16) at index i of a row, scaled to 0-255 (16 bits: the high byte). */
static unsigned sample(const uint8_t *row, size_t i, int bits) {
    switch (bits) {
    case 16: return row[i * 2];
    case 8: return row[i];
    default: {
        size_t bit = i * (size_t)bits;
        unsigned v = (row[bit >> 3] >> (8 - bits - (bit & 7))) & ((1u << bits) - 1);
        return v;
    }
    }
}

static unsigned sample16(const uint8_t *row, size_t i, int bits) {   /* the raw value, for tRNS keys */
    return bits == 16 ? (unsigned)row[i * 2] << 8 | row[i * 2 + 1] : sample(row, i, bits);
}

int pcgfx_png_decode(const uint8_t *data, size_t size, uint8_t **out, int *out_w, int *out_h, char *why,
                     size_t why_size) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    uint32_t w = 0, h = 0;
    int bits = 0, ctype = -1, interlace = 0, channels, have_ihdr = 0;
    uint8_t pal[256][4];
    unsigned npal = 0, trns_key[3] = {0, 0, 0};
    int have_key = 0;
    uint8_t *idat = NULL, *raw = NULL, *rgba = NULL;
    size_t idat_len = 0, idat_cap = 0, pos = 8, stride, bpp, x, y;
    const char *err = NULL;
    z_stream zs;
    int i;

    *out = NULL;
    for (i = 0; i < 256; i++)
        pal[i][0] = pal[i][1] = pal[i][2] = 0, pal[i][3] = 255;
    if (size < 8 || memcmp(data, sig, 8) != 0) {
        err = "not a PNG file";
        goto fail;
    }
    while (pos + 12 <= size) {
        uint32_t len = rd_be32(data + pos);
        const uint8_t *type = data + pos + 4, *body = data + pos + 8;
        if (len > size - pos - 12) {
            err = "a chunk runs past the end of the file";
            goto fail;
        }
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = rd_be32(body);
            h = rd_be32(body + 4);
            bits = body[8];
            ctype = body[9];
            interlace = body[12];
            have_ihdr = 1;
        } else if (!memcmp(type, "PLTE", 4)) {
            npal = len / 3 > 256 ? 256 : len / 3;
            for (i = 0; i < (int)npal; i++)
                pal[i][0] = body[i * 3], pal[i][1] = body[i * 3 + 1], pal[i][2] = body[i * 3 + 2];
        } else if (!memcmp(type, "tRNS", 4)) {
            if (ctype == 3) {
                for (i = 0; i < (int)len && i < 256; i++)
                    pal[i][3] = body[i];
            } else if (ctype == 0 && len >= 2) {
                trns_key[0] = (unsigned)body[0] << 8 | body[1];
                have_key = 1;
            } else if (ctype == 2 && len >= 6) {
                for (i = 0; i < 3; i++)
                    trns_key[i] = (unsigned)body[i * 2] << 8 | body[i * 2 + 1];
                have_key = 1;
            }
        } else if (!memcmp(type, "IDAT", 4)) {
            if (idat_len + len > idat_cap) {
                size_t cap = idat_cap ? idat_cap * 2 : 65536;
                uint8_t *n;
                while (cap < idat_len + len)
                    cap *= 2;
                n = (uint8_t *)realloc(idat, cap);
                if (n == NULL) {
                    err = "out of memory";
                    goto fail;
                }
                idat = n;
                idat_cap = cap;
            }
            memcpy(idat + idat_len, body, len);
            idat_len += len;
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        pos += 12 + (size_t)len;
    }
    if (!have_ihdr || w == 0 || h == 0 || w > PNG_MAX_DIM || h > PNG_MAX_DIM) {
        err = "no image size, or one past 16384";
        goto fail;
    }
    channels = ctype == 0 ? 1 : ctype == 2 ? 3 : ctype == 3 ? 1 : ctype == 4 ? 2 : ctype == 6 ? 4 : 0;
    if (channels == 0 || !(bits == 1 || bits == 2 || bits == 4 || bits == 8 || bits == 16) ||
        ((ctype == 2 || ctype == 4 || ctype == 6) && bits < 8) || (ctype == 3 && bits == 16)) {
        err = "an unknown color type or bit depth";
        goto fail;
    }
    if (interlace != 0) {
        err = "interlaced (Adam7) PNGs are not read: save it without interlacing";
        goto fail;
    }
    stride = ((size_t)w * channels * bits + 7) / 8;
    bpp = ((size_t)channels * bits + 7) / 8;
    raw = (uint8_t *)malloc((stride + 1) * h);
    rgba = (uint8_t *)malloc((size_t)w * h * 4);
    if (raw == NULL || rgba == NULL) {
        err = "out of memory";
        goto fail;
    }
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) {
        err = "zlib";
        goto fail;
    }
    zs.next_in = idat;
    zs.avail_in = (uInt)idat_len;
    zs.next_out = raw;
    zs.avail_out = (uInt)((stride + 1) * h);
    i = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (zs.avail_out != 0 || (i != Z_STREAM_END && i != Z_BUF_ERROR && i != Z_OK)) {
        err = "the image data is short or damaged";
        goto fail;
    }
    for (y = 0; y < h; y++) {   /* undo the filters in place: each row against the one above (already undone) */
        uint8_t *row = raw + y * (stride + 1), *cur = row + 1;
        const uint8_t *up = y ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        int f = row[0];
        for (x = 0; x < stride; x++) {
            int a = x >= bpp ? cur[x - bpp] : 0, b = up ? up[x] : 0, c = (up && x >= bpp) ? up[x - bpp] : 0;
            switch (f) {
            case 0: break;
            case 1: cur[x] = (uint8_t)(cur[x] + a); break;
            case 2: cur[x] = (uint8_t)(cur[x] + b); break;
            case 3: cur[x] = (uint8_t)(cur[x] + ((a + b) >> 1)); break;
            case 4: cur[x] = (uint8_t)(cur[x] + paeth(a, b, c)); break;
            default:
                err = "an unknown row filter";
                goto fail;
            }
        }
    }
    for (y = 0; y < h; y++) {
        const uint8_t *cur = raw + y * (stride + 1) + 1;
        uint8_t *o = rgba + y * (size_t)w * 4;
        for (x = 0; x < w; x++, o += 4) {
            unsigned v, scale = bits < 8 ? 255u / ((1u << bits) - 1) : 1;
            switch (ctype) {
            case 0:
                v = sample(cur, x, bits) * scale;
                o[0] = o[1] = o[2] = (uint8_t)v;
                o[3] = (have_key && sample16(cur, x, bits) == trns_key[0]) ? 0 : 255;
                break;
            case 2:
                o[0] = (uint8_t)sample(cur, x * 3, bits);
                o[1] = (uint8_t)sample(cur, x * 3 + 1, bits);
                o[2] = (uint8_t)sample(cur, x * 3 + 2, bits);
                o[3] = (have_key && sample16(cur, x * 3, bits) == trns_key[0] &&
                        sample16(cur, x * 3 + 1, bits) == trns_key[1] && sample16(cur, x * 3 + 2, bits) == trns_key[2])
                           ? 0 : 255;
                break;
            case 3:
                v = sample(cur, x, bits);
                memcpy(o, pal[v], 4);
                break;
            case 4:
                o[0] = o[1] = o[2] = (uint8_t)sample(cur, x * 2, bits);
                o[3] = (uint8_t)sample(cur, x * 2 + 1, bits);
                break;
            default:
                for (i = 0; i < 4; i++)
                    o[i] = (uint8_t)sample(cur, x * 4 + (size_t)i, bits);
                break;
            }
        }
    }
    free(idat);
    free(raw);
    *out = rgba;
    *out_w = (int)w;
    *out_h = (int)h;
    return 0;
fail:
    if (why != NULL && why_size)
        snprintf(why, why_size, "%s", err ? err : "unreadable");
    free(idat);
    free(raw);
    free(rgba);
    return -1;
}

int pcgfx_png_read(const char *path, uint8_t **rgba, int *w, int *h, char *why, size_t why_size) {
    FILE *f = fopen(path, "rb");
    uint8_t *data;
    long size;
    int rc;
    *rgba = NULL;
    if (f == NULL) {
        if (why != NULL && why_size)
            snprintf(why, why_size, "cannot open it");
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 0 || size > (1L << 30) || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        if (why != NULL && why_size)
            snprintf(why, why_size, "cannot read it");
        return -1;
    }
    data = (uint8_t *)malloc((size_t)size);
    if (data == NULL || fread(data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(data);
        if (why != NULL && why_size)
            snprintf(why, why_size, "cannot read it");
        return -1;
    }
    fclose(f);
    rc = pcgfx_png_decode(data, (size_t)size, rgba, w, h, why, why_size);
    free(data);
    return rc;
}
