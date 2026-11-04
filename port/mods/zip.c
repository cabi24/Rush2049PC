/* port/mods/zip.c: a mod as a .zip (mods.h, docs/port/MODS.md 2): the central directory's entries, stored (method 0)
 * or deflated (8), read whole into memory. No zip64, no encryption, no spanned archives: such a zip is refused with
 * the reason. Names are kept as the zip has them, with '/' between folders. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods.h"

#define ZIP_MAX ((size_t)512 << 20)
#define ENTRY_MAX ((size_t)256 << 20)
#define ENTRIES_MAX 65535

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }

void pc_mods_zip_free(PcZipEntry *e, int n)
{
    for (int i = 0; i < n; i++)
        free(e[i].data);
    free(e);
}

int pc_mods_zip_read(const char *path, PcZipEntry **entries, int *count, char *why, size_t why_size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *z = NULL;
    long len = -1;
    PcZipEntry *e = NULL;
    int n = 0;
    *entries = NULL, *count = 0;
    if (!f) {
        snprintf(why, why_size, "%s", strerror(errno));
        return -1;
    }
    if (fseek(f, 0, SEEK_END) == 0)
        len = ftell(f);
    if (len < 22 || (size_t)len > ZIP_MAX || fseek(f, 0, SEEK_SET) != 0 || !(z = malloc((size_t)len)) ||
        fread(z, 1, (size_t)len, f) != (size_t)len) {
        snprintf(why, why_size, len < 22 ? "too short for a zip" : "it can't be read whole");
        fclose(f);
        free(z);
        return -1;
    }
    fclose(f);
    size_t size = (size_t)len, eocd = size - 22;
    for (;;) {   /* the end record, after which only its comment (up to 64 KiB) */
        if (le32(z + eocd) == 0x06054B50u)
            break;
        if (eocd == 0 || size - eocd > 22 + 65535) {
            snprintf(why, why_size, "no zip end record");
            free(z);
            return -1;
        }
        eocd--;
    }
    uint32_t total = le16(z + eocd + 10), cd_size = le32(z + eocd + 12), cd = le32(z + eocd + 16);
    if (le16(z + eocd + 4) || le16(z + eocd + 6) || total == 0xFFFF || cd == 0xFFFFFFFFu) {
        snprintf(why, why_size, "a spanned or zip64 archive");
        free(z);
        return -1;
    }
    if ((uint64_t)cd + cd_size > eocd || total > ENTRIES_MAX || !(e = calloc(total ? total : 1, sizeof *e))) {
        snprintf(why, why_size, "its central directory is outside the file");
        free(z);
        return -1;
    }
    size_t p = cd;
    for (uint32_t i = 0; i < total; i++) {
        if (p + 46 > (size_t)cd + cd_size || le32(z + p) != 0x02014B50u) {
            snprintf(why, why_size, "entry %u of its central directory is damaged", i);
            goto fail;
        }
        uint32_t flags = le16(z + p + 8), method = le16(z + p + 10), csize = le32(z + p + 20), usize = le32(z + p + 24);
        uint32_t nlen = le16(z + p + 28), xlen = le16(z + p + 30), clen = le16(z + p + 32), lho = le32(z + p + 42);
        if (p + 46 + nlen + xlen + clen > (size_t)cd + cd_size) {
            snprintf(why, why_size, "entry %u's name runs past the central directory", i);
            goto fail;
        }
        char name[256];
        snprintf(name, sizeof name, "%.*s", (int)(nlen < 255 ? nlen : 255), (const char *)z + p + 46);
        p += 46 + nlen + xlen + clen;
        for (char *c = name; *c; c++)
            if (*c == '\\')
                *c = '/';
        if (strstr(name, "../") || name[0] == '/') {
            snprintf(why, why_size, "entry '%s' leaves the mod's folder", name);
            goto fail;
        }
        size_t name_len = strlen(name);   /* not nlen: a name with a NUL in it ends there */
        if (!name_len || name[name_len - 1] == '/')
            continue;   /* a folder, or a name with nothing before a NUL */
        if (flags & 1) {
            snprintf(why, why_size, "'%s' is encrypted", name);
            goto fail;
        }
        if ((method != 0 && method != 8) || usize > ENTRY_MAX) {
            snprintf(why, why_size, "'%s' is %s", name, usize > ENTRY_MAX ? "too large" : "packed with a method "
                     "other than store or deflate");
            goto fail;
        }
        if ((uint64_t)lho + 30 > size || le32(z + lho) != 0x04034B50u) {
            snprintf(why, why_size, "'%s''s local header is damaged", name);
            goto fail;
        }
        size_t data = lho + 30 + le16(z + lho + 26) + le16(z + lho + 28);
        if (data > size || csize > size - data) {
            snprintf(why, why_size, "'%s''s data runs past the file", name);
            goto fail;
        }
        PcZipEntry *x = &e[n];
        snprintf(x->name, sizeof x->name, "%s", name);
        if (method == 0) {
            if (csize != usize || !(x->data = malloc(usize ? usize : 1))) {
                snprintf(why, why_size, "'%s' can't be read", name);
                goto fail;
            }
            memcpy(x->data, z + data, usize);
            x->size = usize;
        } else if (!usize) {
            if (!(x->data = malloc(1)))
                goto fail;
        } else {
            size_t got = 0;
            x->data = pc_mods_inflate(z + data, csize, usize, &got);
            if (!x->data || got != usize) {
                free(x->data);
                x->data = NULL;
                snprintf(why, why_size, "'%s' doesn't inflate", name);
                goto fail;
            }
            x->size = got;
        }
        n++;
    }
    free(z);
    *entries = e, *count = n;
    return 0;
fail:
    pc_mods_zip_free(e, n);
    free(z);
    return -1;
}
