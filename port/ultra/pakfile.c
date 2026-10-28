/* port/ultra/pakfile.c: the controller pak's file (docs/port/ULTRA.md "Controller pak", RELEASE.md "Saves"): a
 * blank formatted image, what an image is (usable, in N64 byte order, or damaged), and writing a file so that a crash
 * or a power cut leaves the old one or the new one, never half of one. Pure: no messages and no game code, so si.c,
 * port/app's saves.c and the tests share it.
 *
 * The image is the native Pfs C's (si.c): the .mpk page layout, its u16 fields in host order. An image is damaged
 * when the game's own libultra would write over it at its first osPfsInitPak: no ID block copy (of four) has a
 * right checksum, and __osRepairPackId then writes a new ID; or neither inode table (page 1, its backup page 2) has
 * a right checksum, and the game is told the pak is inconsistent. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#endif

#include "ultra.h"

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }

void pc_pak_id_sums(const uint8_t *id, int be, uint16_t *sum, uint16_t *isum)
{
    *sum = *isum = 0;
    for (int j = 0; j < 28; j += 2) {
        uint16_t w;
        if (be)
            w = be16(id + j);
        else
            memcpy(&w, id + j, 2);
        *sum = (uint16_t)(*sum + w);
        *isum = (uint16_t)(*isum + (uint16_t)~w);
    }
}

int pc_pak_id_valid(const uint8_t *id, int be)
{
    uint16_t sum, isum, s2, i2;
    pc_pak_id_sums(id, be, &sum, &isum);
    if (be)
        s2 = be16(id + 0x1C), i2 = be16(id + 0x1E);
    else
        memcpy(&s2, id + 0x1C, 2), memcpy(&i2, id + 0x1E, 2);
    return sum == s2 && isum == i2;
}

/* __osSumcalc over an inode page from the first data page's entry (bank 0: page 5, byte 10), against the byte
 * __osPfsRWInode compares it with, inode_page[0].inode_t.page. */
static int inode_valid(const uint8_t *page)
{
    unsigned bytes = 0;
    for (int i = 10; i < 256; i++)
        bytes += page[i];
    return (uint8_t)bytes == page[1];
}

void pc_pak_format(uint8_t *img)
{
    memset(img, 0, PC_PAK_SIZE);
    img[0] = 0x81;
    for (int i = 1; i < 32; i++)
        img[i] = (uint8_t)i;
    uint8_t id[32] = {0};
    uint32_t repaired = 0xFFFFFFFFu, random = 0x135F1A05u;
    uint16_t device = 0x0001, sum, isum;   /* device id bit 0: the ID is valid */
    memcpy(id, &repaired, 4);
    memcpy(id + 4, &random, 4);
    memcpy(id + 0x18, &device, 2);
    id[0x1A] = 1;   /* banks */
    id[0x1B] = 0;   /* version */
    pc_pak_id_sums(id, 0, &sum, &isum);
    memcpy(id + 0x1C, &sum, 2);
    memcpy(id + 0x1E, &isum, 2);
    static const int copies[] = {0x20, 0x60, 0x80, 0xC0};   /* PFS_ID_0AREA..3AREA */
    for (int i = 0; i < 4; i++)
        memcpy(img + copies[i], id, 32);
    uint8_t *inode = img + 0x100;
    for (int page = 5; page < 128; page++) {
        uint16_t unused = 0x0003;   /* PFS_PAGE_NOT_USED */
        memcpy(inode + 2 * page, &unused, 2);
    }
    int bytes = 0;
    for (int i = 10; i < 256; i++)   /* __osSumcalc from the first data page's entry */
        bytes += inode[i];
    inode[1] = (uint8_t)bytes;       /* inode_page[0].inode_t.page */
    memcpy(img + 0x200, inode, 256);
}

int pc_pak_image_check(const uint8_t *img, size_t size, char *why, size_t why_size)
{
    static const int copies[] = {0x20, 0x60, 0x80, 0xC0};
    char scratch[8];
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    why[0] = 0;
    if (size != PC_PAK_SIZE) {
        if (size == 0)
            snprintf(why, why_size, "it is empty");
        else
            snprintf(why, why_size, "it is %zu bytes, not 32 KiB (%u bytes) as a controller pak is", size,
                     PC_PAK_SIZE);
        return PC_PAK_IMAGE_DAMAGED;
    }
    int host = 0, n64 = 0;
    for (int i = 0; i < 4; i++) {
        host |= pc_pak_id_valid(img + copies[i], 0);
        n64 |= pc_pak_id_valid(img + copies[i], 1);
    }
    if (!host && n64) {
        snprintf(why, why_size, "it is in N64 byte order (v5's or an emulator's)");
        return PC_PAK_IMAGE_N64;
    }
    if (!host) {
        snprintf(why, why_size, "its ID block and the three copies of it are damaged");
        return PC_PAK_IMAGE_DAMAGED;
    }
    if (!inode_valid(img + 0x100) && !inode_valid(img + 0x200)) {
        snprintf(why, why_size, "its file table and the backup of it are damaged");
        return PC_PAK_IMAGE_DAMAGED;
    }
    return PC_PAK_IMAGE_OK;
}

int pc_pak_file_load(const char *path, uint8_t *image, char *why, size_t why_size)
{
    static uint8_t img[PC_PAK_SIZE + 1];   /* one byte more: a bigger file is told by its size */
    char scratch[8];
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    why[0] = 0;
    FILE *f = fopen(path, "rb");
    if (!f && errno == ENOENT) {
        pc_pak_format(image);
        return PC_PAK_LOAD_NEW;
    }
    if (!f) {
        snprintf(why, why_size, "it cannot be read: %s", strerror(errno));
        return PC_PAK_LOAD_NONE;
    }
    size_t n = fread(img, 1, sizeof img, f);
    if (n == sizeof img && fseek(f, 0, SEEK_END) == 0 && ftell(f) > 0)
        n = (size_t)ftell(f);   /* too big: its size, for the message */
    fclose(f);
    switch (pc_pak_image_check(img, n, why, why_size)) {
    case PC_PAK_IMAGE_OK:
        memcpy(image, img, PC_PAK_SIZE);
        return PC_PAK_LOAD_FILE;
    case PC_PAK_IMAGE_N64:
        pc_pak_format(image);
        return PC_PAK_LOAD_N64;
    default:
        return PC_PAK_LOAD_NONE;
    }
}

int pc_file_write_atomic(const char *path, const void *data, size_t size)
{
    size_t len = strlen(path) + 5;
    char *tmp = malloc(len);
    if (!tmp)
        return -1;
    snprintf(tmp, len, "%s.tmp", path);
    int ok = 0, err = 0;
    FILE *f = fopen(tmp, "wb");
    if (f) {
        ok = fwrite(data, 1, size, f) == size && fflush(f) == 0;
        /* on the disk before the rename, so that the name never points at a file the system hasn't written */
#ifdef _WIN32
        ok = ok && _commit(_fileno(f)) == 0;
#else
        ok = ok && fsync(fileno(f)) == 0;
#endif
        err = ok ? 0 : errno;
        ok = fclose(f) == 0 && ok;
        if (!err && !ok)
            err = errno;
#ifdef _WIN32
        ok = ok && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!ok && !err)
            err = EACCES;
#else
        ok = ok && rename(tmp, path) == 0;
        if (!ok && !err)
            err = errno;
#endif
        if (!ok)
            remove(tmp);
    } else {
        err = errno;
    }
#ifndef _WIN32
    if (ok) {   /* the rename itself on the disk: the folder's entry */
        char *dir = strdup(path), *slash = dir ? strrchr(dir, '/') : NULL;
        if (dir) {
            if (slash == dir)
                slash[1] = 0;
            else if (slash)
                *slash = 0;
            int fd = open(slash ? dir : ".", O_RDONLY | O_DIRECTORY);
            if (fd >= 0) {
                fsync(fd);
                close(fd);
            }
            free(dir);
        }
    }
#endif
    free(tmp);
    errno = err;
    return ok ? 0 : -1;
}
