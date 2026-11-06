/* port/ultra/replay.c: the input record file (docs/port/LOCKSTEP.md 3): each retrace's final pads, as the game
 * gets them, behind a header that says what a replay must match.
 *
 * Little-endian, fixed layout:
 *
 *   header    0  8  magic "R2PCPADS"
 *             8  4  version (1)
 *            12  4  header bytes H: the first record is at H
 *            16  4  record bytes R (20; a later version may make them longer, and a reader of this one reads the
 *                   first 20 of each)
 *            20  4  the first record's retrace (0)
 *            24  8  xxHash64 of the text (a header's own check)
 *            32  4  the text's bytes T
 *            36  T  the must-match text: "key=value" lines; lines from '#' are said, not compared. Zeros to H.
 *   record    0 16  ports 1-4: buttons (u16), stick x (s8), stick y (s8)
 *            16  1  connected, a bit a port (bit 0: port 1)
 *            17  1  its controller can rumble (PcPad.rumble), a bit a port
 *            18  2  the retrace's low 16 bits: a check that the records are where they should be
 *
 * A file cut short is read up to its last whole record, and a record whose check fails ends it there: the replay
 * plays what it has, then says so. */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_lockstep.h"

#define HEAD_FIXED 36u

struct PcPadWriter {
    FILE *f;
    char *path;
    uint32_t next;
    int failed;
};

static void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

static void put64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t get64(const uint8_t *p)
{
    return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32;
}

PcPadWriter *pc_padfile_create(const char *path, const char *must_match, char *why, size_t why_size)
{
    size_t t = strlen(must_match), h = (HEAD_FIXED + t + 1 + 3) & ~(size_t)3;
    uint8_t *head = calloc(1, h);
    PcPadWriter *w = calloc(1, sizeof *w);
    if (!head || !w) {
        snprintf(why, why_size, "out of memory");
        free(head), free(w);
        return NULL;
    }
    memcpy(head, PC_PADFILE_MAGIC, 8);
    put32(head + 8, PC_PADFILE_VERSION);
    put32(head + 12, (uint32_t)h);
    put32(head + 16, PC_PADFILE_RECORD);
    put32(head + 20, 0);
    put64(head + 24, pc_xxh64(must_match, t, 0));
    put32(head + 32, (uint32_t)t);
    memcpy(head + HEAD_FIXED, must_match, t);
    w->f = fopen(path, "wb");
    if (!w->f || fwrite(head, 1, h, w->f) != h || fflush(w->f) != 0) {
        snprintf(why, why_size, "can't write %s", path);
        if (w->f)
            fclose(w->f);
        free(head), free(w);
        return NULL;
    }
    free(head);
    w->path = strdup(path);
    return w;
}

static void encode(uint8_t r[PC_PADFILE_RECORD], uint32_t n, const PcPad pads[4])
{
    memset(r, 0, PC_PADFILE_RECORD);
    for (int p = 0; p < 4; p++) {
        r[4 * p] = (uint8_t)pads[p].buttons;
        r[4 * p + 1] = (uint8_t)(pads[p].buttons >> 8);
        r[4 * p + 2] = (uint8_t)pads[p].stick_x;
        r[4 * p + 3] = (uint8_t)pads[p].stick_y;
        r[16] |= (uint8_t)((pads[p].connected != 0) << p);
        r[17] |= (uint8_t)((pads[p].rumble != 0) << p);
    }
    r[18] = (uint8_t)n;
    r[19] = (uint8_t)(n >> 8);
}

int pc_padfile_write(PcPadWriter *w, uint32_t n, const PcPad pads[4])
{
    uint8_t r[PC_PADFILE_RECORD];
    if (!w || w->failed)
        return -1;
    if (n != w->next) {   /* records are the retraces in order: a gap would shift the rest */
        fprintf(stderr, "rush2049pc: record: retrace %u after %u; the file ends at %u\n", n, w->next - 1, w->next);
        w->failed = 1;
        return -1;
    }
    encode(r, n, pads);
    if (fwrite(r, 1, sizeof r, w->f) != sizeof r || (++w->next % 60 == 0 && fflush(w->f) != 0)) {
        fprintf(stderr, "rush2049pc: record: can't write %s; it ends at retrace %u\n", w->path, w->next - 1);
        w->failed = 1;
        return -1;
    }
    return 0;
}

void pc_padfile_close(PcPadWriter *w)
{
    if (!w)
        return;
    if (fclose(w->f) != 0 && !w->failed)
        fprintf(stderr, "rush2049pc: record: can't finish %s\n", w->path);
    else
        fprintf(stderr, "rush2049pc: record: %u retraces of pads in %s\n", w->next, w->path);
    free(w->path);
    free(w);
}

int pc_padfile_read(const char *path, PcPadFile *f, char *why, size_t why_size)
{
    FILE *in = fopen(path, "rb");
    uint8_t *b = NULL;
    size_t n = 0, cap = 0, got;
    uint32_t h, rec, t;
    memset(f, 0, sizeof *f);
    if (!in) {
        snprintf(why, why_size, "can't read %s", path);
        return -1;
    }
    do {   /* the whole file: a recording is small (20 bytes a retrace, 4.3 MB an hour) */
        if (n == cap) {
            cap = cap ? cap * 2 : 1 << 16;
            uint8_t *nb = realloc(b, cap);
            if (!nb) {
                free(b), fclose(in);
                snprintf(why, why_size, "out of memory");
                return -1;
            }
            b = nb;
        }
        got = fread(b + n, 1, cap - n, in);
        n += got;
    } while (got);
    fclose(in);
    if (n < HEAD_FIXED || memcmp(b, PC_PADFILE_MAGIC, 8) != 0) {
        snprintf(why, why_size, "%s is not an input recording (no %s header)", path, PC_PADFILE_MAGIC);
        free(b);
        return -1;
    }
    h = get32(b + 12), rec = get32(b + 16), t = get32(b + 32);
    if (get32(b + 8) != PC_PADFILE_VERSION) {
        snprintf(why, why_size, "%s is a version %u recording; this build reads version %u", path, get32(b + 8),
                 PC_PADFILE_VERSION);
        free(b);
        return -1;
    }
    if (rec < PC_PADFILE_RECORD || h < HEAD_FIXED + t || h > n || get32(b + 20) != 0 ||
        pc_xxh64(b + HEAD_FIXED, t, 0) != get64(b + 24)) {
        snprintf(why, why_size, "%s has a damaged header", path);
        free(b);
        return -1;
    }
    f->must_match = malloc(t + 1);
    f->records = malloc((n - h) / rec * PC_PADFILE_RECORD + 1);
    if (!f->must_match || !f->records) {
        pc_padfile_free(f);
        free(b);
        snprintf(why, why_size, "out of memory");
        return -1;
    }
    memcpy(f->must_match, b + HEAD_FIXED, t);
    f->must_match[t] = 0;
    f->count = (uint32_t)((n - h) / rec);
    f->tail = (uint32_t)((n - h) % rec);
    f->bad = f->count;
    for (uint32_t i = 0; i < f->count; i++) {
        const uint8_t *r = b + h + (size_t)i * rec;
        if ((uint16_t)(r[18] | r[19] << 8) != (uint16_t)i) {
            f->bad = i;
            break;
        }
        memcpy(f->records + (size_t)i * PC_PADFILE_RECORD, r, PC_PADFILE_RECORD);
    }
    free(b);
    return 0;
}

void pc_padfile_record(const PcPadFile *f, uint32_t i, PcPad pads[4])
{
    const uint8_t *r = f->records + (size_t)i * PC_PADFILE_RECORD;
    for (int p = 0; p < 4; p++) {
        pads[p].buttons = (uint16_t)(r[4 * p] | r[4 * p + 1] << 8);
        pads[p].stick_x = (int8_t)r[4 * p + 2];
        pads[p].stick_y = (int8_t)r[4 * p + 3];
        pads[p].connected = (r[16] >> p) & 1;
        pads[p].rumble = (r[17] >> p) & 1;
    }
}

void pc_padfile_free(PcPadFile *f)
{
    free(f->must_match);
    free(f->records);
    memset(f, 0, sizeof *f);
}

/* The value of key in text's compared lines (NULL: none), its length in *len. */
static const char *value_of(const char *text, const char *key, size_t klen, size_t *len)
{
    for (const char *l = text; l && *l; l = strchr(l, '\n'), l = l ? l + 1 : NULL) {
        const char *eq = strchr(l, '='), *nl = strchr(l, '\n');
        if (*l == '#' || !eq || (nl && eq > nl))
            continue;
        if ((size_t)(eq - l) == klen && !strncmp(l, key, klen)) {
            *len = nl ? (size_t)(nl - eq - 1) : strlen(eq + 1);
            return eq + 1;
        }
    }
    return NULL;
}

static void say(char *out, size_t size, size_t *at, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (*at >= size)
        return;
    va_start(ap, fmt);
    n = vsnprintf(out + *at, size - *at, fmt, ap);
    va_end(ap);
    *at = n < 0 ? size : *at + (size_t)n < size ? *at + (size_t)n : size;
}

int pc_padfile_mismatch(const char *file, const char *here, char *out, size_t size)
{
    size_t at = 0;
    int differ = 0;
    if (size)
        out[0] = 0;
    for (const char *l = file; l && *l; l = strchr(l, '\n'), l = l ? l + 1 : NULL) {   /* the file's keys */
        const char *eq = strchr(l, '='), *nl = strchr(l, '\n'), *va, *vb;
        size_t la, lb = 0, k;
        if (*l == '#' || !eq || (nl && eq > nl))
            continue;
        k = (size_t)(eq - l);
        va = value_of(file, l, k, &la);
        vb = value_of(here, l, k, &lb);
        if (vb && la == lb && !strncmp(va, vb, la))
            continue;
        differ++;
        say(out, size, &at, "  %.*s: %.*s in the file, here %.*s\n", (int)k, l, (int)la, va, vb ? (int)lb : 4,
            vb ? vb : "none");
    }
    /* keys only this run has */
    for (const char *l = here; l && *l; l = strchr(l, '\n'), l = l ? l + 1 : NULL) {
        const char *eq = strchr(l, '='), *nl = strchr(l, '\n');
        size_t len;
        if (*l == '#' || !eq || (nl && eq > nl) || value_of(file, l, (size_t)(eq - l), &len))
            continue;
        differ++;
        say(out, size, &at, "  %.*s: none in the file, here %.*s\n", (int)(eq - l), l,
            nl ? (int)(nl - eq - 1) : (int)strlen(eq + 1), eq + 1);
    }
    return differ;
}
