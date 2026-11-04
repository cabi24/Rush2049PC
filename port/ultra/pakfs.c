/* port/ultra/pakfs.c: a controller pak's notes as files (docs/port/SAVES.md): the notes of an image and an image of
 * notes, the save files that hold one note each, and the game's own checks of a profile and a ghost. Pure: no
 * messages and no game code, so si.c's saves folder (pakstore.c), tools and tests share it.
 *
 * The image is the native Pfs C's (si.c, docs/port/ULTRA.md "Controller pak"): the .mpk layout of a one-bank pak
 * (page 0 the label and the ID block with its copies, page 1 the inode table and page 2 its backup, pages 3-4 the
 * directory's 16 entries of 32 bytes, pages 5-127 the notes' pages), its u16 and u32 fields in host order. An inode
 * entry is {bank, page} bytes for a link, which the Pfs C reads as a native u16 ipage: below the first data page (5)
 * it ends the chain, and 1 (PFS_EOF) ends it rightly; 3 (PFS_PAGE_NOT_USED) is a free page. */
#define _GNU_SOURCE
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>

#include "pakfs.h"
#include "ultra.h"

#define INODE 0x100
#define INODE_BACKUP 0x200
#define DIR0 0x300
#define FIRST_PAGE 5
#define PAGES 128
#define PFS_EOF 1
#define PFS_PAGE_NOT_USED 3

static uint16_t host16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

static uint32_t host32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8); }

static int host_little(void)
{
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 1;
}

/* ---------------------------------------------------------------- the game's checks */

uint32_t pc_save_hash(const uint8_t *data, uint32_t size)
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < size; i++) {
        uint32_t value = data[i];
        switch (value & 7) {
        case 0: result -= value; break;
        case 1: result |= value; break;
        case 2: result &= value; break;
        case 3: result ^= value; break;
        case 4: result *= value; break;
        case 5: result /= value; break;   /* value & 7 == 5: never 0 */
        default: result += value; break;
        }
    }
    return result;
}

uint32_t pc_note_game_code(const PcNote *note) { return host32(note->dir); }
uint16_t pc_note_company(const PcNote *note) { return host16(note->dir + 4); }

static int rush_note(const PcNote *note)
{
    return pc_note_game_code(note) == PC_RUSH_GAME_CODE && pc_note_company(note) == PC_RUSH_COMPANY;
}

/* group.c's draw_speedometer: the revision byte, and the first block's checksum (the id words and the name) */
static int is_profile(const uint8_t *d, size_t size)
{
    return size >= PC_PROFILE_SIZE_N64 && d[12] == 16 && pc_save_hash(d + 4, 24) == host32(d);
}

/* group.c's draw_ui_element without the data: the revision and the header's checksum over +4..+0x44 */
static int is_ghost(const uint8_t *d, size_t size)
{
    return size >= PC_GHOST_HEADER && d[4] == PC_GHOST_REVISION && pc_save_hash(d + 4, 0x40) == host32(d);
}

static void ascii_name(char *out, const uint8_t *in, size_t n)
{
    size_t i;
    for (i = 0; i < n && in[i]; i++)
        out[i] = (char)in[i];
    out[i] = 0;
}

void pc_note_info(const PcNote *note, PcNoteInfo *info)
{
    size_t size = (size_t)note->pages * PC_PAK_PAGE;
    const uint8_t *d = note->data;
    memset(info, 0, sizeof *info);
    info->track = info->backward = info->car = -1;
    if (!rush_note(note) || !d)
        return;
    if (is_ghost(d, size)) {
        info->kind = PC_NOTE_GHOST;
        info->track = (int8_t)d[6];
        info->backward = d[7];
        info->car = d[8];
        ascii_name(info->name, d + 0x16, 13);
        memcpy(&info->finish, d + 0x38, 4);
        info->id[0] = host32(d + 0x28), info->id[1] = host32(d + 0x2C);
    } else if (is_profile(d, size)) {
        info->kind = PC_NOTE_PROFILE;
        ascii_name(info->name, d + 13, 13);
        info->id[0] = host32(d + 4), info->id[1] = host32(d + 8);
    }
}

/* %g as glibc prints it, on every C library: NaN as "nan" or "-nan" by its sign, infinities as "inf" and "-inf"
 * (Windows' C library prints NaN without its sign), so a refusal reads the same as tools/port/ghost.py's. */
static const char *fmt_g(char out[32], float x)
{
    if (isnan(x))
        snprintf(out, 32, "%s", signbit(x) ? "-nan" : "nan");
    else if (isinf(x))
        snprintf(out, 32, "%s", x < 0 ? "-inf" : "inf");
    else
        snprintf(out, 32, "%g", (double)x);
    return out;
}

int pc_ghost_check(const uint8_t *d, size_t size, int inflate_it, char *why, size_t why_size)
{
    char scratch[8];
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    why[0] = 0;
    if (size < PC_GHOST_HEADER) {
        snprintf(why, why_size, "%zu bytes: shorter than a ghost's %d-byte header", size, PC_GHOST_HEADER);
        return -1;
    }
    if (d[4] != PC_GHOST_REVISION) {
        snprintf(why, why_size, "its ghost header's revision is %u, not %d", d[4], PC_GHOST_REVISION);
        return -1;
    }
    if (pc_save_hash(d + 4, 0x40) != host32(d)) {
        snprintf(why, why_size, "its ghost header's checksum is wrong");
        return -1;
    }
    int track = (int8_t)d[6];
    if (track < 0 || track >= PC_GHOST_TRACKS) {
        snprintf(why, why_size, "its track is %d, not a race track (0-%d)", track, PC_GHOST_TRACKS - 1);
        return -1;
    }
    if (d[7] > 1) {
        snprintf(why, why_size, "its direction is %u, not 0 (forward) or 1 (backward)", d[7]);
        return -1;
    }
    if (d[8] >= PC_GHOST_CARS) {
        snprintf(why, why_size, "its car is %u, not one of the %d cars", d[8], PC_GHOST_CARS);
        return -1;
    }
    float frame, finish;
    char num[32];
    memcpy(&frame, d + 0x34, 4);
    memcpy(&finish, d + 0x38, 4);
    if (!(frame > 0.0f && frame <= 0.1f)) {   /* the game's retrace time: 1/60 (1/50 on PAL) */
        snprintf(why, why_size, "its frame time is %s s, not a retrace's", fmt_g(num, frame));
        return -1;
    }
    if (!(finish >= 0.0f && finish < 36000.0f)) {
        snprintf(why, why_size, "its race time is %s s", fmt_g(num, finish));
        return -1;
    }
    uint32_t zsize = host32(d + 0x3C), usize = host32(d + 0x40);
    if (zsize == 0 || zsize > size - PC_GHOST_HEADER) {
        snprintf(why, why_size, "its zipped samples are %u bytes, and the note holds %zu after the header", zsize,
                 size - PC_GHOST_HEADER);
        return -1;
    }
    if (pc_save_hash(d + PC_GHOST_HEADER, zsize) != host32(d + 0x44)) {
        snprintf(why, why_size, "its samples' checksum is wrong");
        return -1;
    }
    if (usize == 0 || usize % 3 || usize / 3 > PC_GHOST_MAX_FRAMES) {
        snprintf(why, why_size, "its samples unzip to %u bytes, not 1 to %d frames of 3 bytes", usize,
                 PC_GHOST_MAX_FRAMES);
        return -1;
    }
    if (!inflate_it)
        return 0;
    uint8_t *out = malloc((size_t)usize + 1);   /* one byte more: a stream that goes on past the size is told */
    if (!out) {
        snprintf(why, why_size, "no memory to check its samples");
        return -1;
    }
    z_stream z;
    memset(&z, 0, sizeof z);
    int ok = inflateInit2(&z, -15) == Z_OK;
    if (ok) {
        z.next_in = (Bytef *)(uintptr_t)(d + PC_GHOST_HEADER);
        z.avail_in = zsize;
        z.next_out = out;
        z.avail_out = usize + 1;
        int r = inflate(&z, Z_FINISH);
        ok = r == Z_STREAM_END && z.total_out == usize;
        if (!ok)
            snprintf(why, why_size, "its samples don't unzip to the %u bytes its header says (zlib %d, %lu bytes)",
                     usize, r, (unsigned long)z.total_out);
        inflateEnd(&z);
    } else {
        snprintf(why, why_size, "zlib can't start");
    }
    if (ok && pc_save_hash(out, usize) != host32(d + 0x48)) {
        snprintf(why, why_size, "its unzipped samples' checksum is wrong");
        ok = 0;
    }
    free(out);
    return ok ? 0 : -1;
}

/* ---------------------------------------------------------------- notes */

int pc_note_same_key(const uint8_t *a, const uint8_t *b)
{
    return !memcmp(a, b, 6) && !memcmp(a + 12, b + 12, 20);   /* game, company; ext and name */
}

int pc_note_equal(const PcNote *a, const PcNote *b)
{
    uint8_t da[32], db[32];
    memcpy(da, a->dir, 32), memcpy(db, b->dir, 32);
    da[6] = da[7] = db[6] = db[7] = 0;
    return !memcmp(da, db, 32) && a->pages == b->pages &&
           !memcmp(a->data, b->data, (size_t)a->pages * PC_PAK_PAGE);
}

int pc_note_copy(PcNote *to, const PcNote *from)
{
    size_t n = (size_t)from->pages * PC_PAK_PAGE;
    uint8_t *d = malloc(n ? n : 1);
    if (!d)
        return -1;
    memcpy(d, from->data, n);
    *to = *from;
    to->data = d;
    return 0;
}

void pc_note_free(PcNote *note)
{
    free(note->data);
    note->data = NULL;
    note->pages = 0;
}

/* inode_valid in pakfile.c: __osSumcalc from the first data page's entry against inode_page[0].inode_t.page */
static int inode_ok(const uint8_t *page)
{
    unsigned bytes = 0;
    for (int i = 2 * FIRST_PAGE; i < PC_PAK_PAGE; i++)
        bytes += page[i];
    return (uint8_t)bytes == page[1];
}

int pc_pak_notes(const uint8_t *img, PcNote out[PC_PAK_NOTES], int *damaged, char *why, size_t why_size)
{
    char scratch[8];
    int bad = 0, n = 0;
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    why[0] = 0;
    const uint8_t *inode = inode_ok(img + INODE) ? img + INODE : inode_ok(img + INODE_BACKUP) ? img + INODE_BACKUP : NULL;
    if (damaged)
        *damaged = 0;
    if (!inode) {
        snprintf(why, why_size, "its file table and the backup of it are damaged");
        return -1;
    }
    for (int slot = 0; slot < PC_PAK_NOTES; slot++) {
        const uint8_t *e = img + DIR0 + 32 * slot;
        if (host32(e) == 0 || host16(e + 4) == 0)   /* osPfsFileState: no game or company code, a free entry */
            continue;
        int chain[PC_PAK_DATA_PAGES], np = 0, ok = 1;
        uint8_t seen[PAGES] = {0};
        const uint8_t *link = e + 6;
        for (;;) {
            uint16_t ipage = host16(link);
            if (ipage < FIRST_PAGE) {
                ok = ipage == PFS_EOF;
                break;
            }
            int bank = link[0], page = link[1];
            if (bank != 0 || page < FIRST_PAGE || page >= PAGES || seen[page] || np == PC_PAK_DATA_PAGES) {
                ok = 0;
                break;
            }
            seen[page] = 1;
            chain[np++] = page;
            link = inode + 2 * page;
        }
        if (!ok || !np) {
            bad++;
            continue;
        }
        PcNote *note = &out[n];
        note->data = malloc((size_t)np * PC_PAK_PAGE);
        if (!note->data) {
            for (int i = 0; i < n; i++)
                pc_note_free(&out[i]);
            snprintf(why, why_size, "no memory");
            return -1;
        }
        memcpy(note->dir, e, 32);
        note->dir[6] = note->dir[7] = 0;
        note->slot = slot;
        note->pages = np;
        for (int i = 0; i < np; i++)
            memcpy(note->data + (size_t)i * PC_PAK_PAGE, img + (size_t)chain[i] * PC_PAK_PAGE, PC_PAK_PAGE);
        n++;
    }
    if (damaged)
        *damaged = bad;
    return n;
}

int pc_pak_build(uint8_t *img, const uint8_t *page0, const PcNote *const *notes, int n, int *slots_out, char *why,
                 size_t why_size)
{
    char scratch[8];
    int at[PC_PAK_NOTES], total = 0;
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    why[0] = 0;
    for (int s = 0; s < PC_PAK_NOTES; s++)
        at[s] = -1;
    if (n > PC_PAK_NOTES) {
        snprintf(why, why_size, "%d notes: a pak holds %d", n, PC_PAK_NOTES);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (notes[i]->pages < 1 || notes[i]->pages > PC_PAK_DATA_PAGES) {
            snprintf(why, why_size, "a note of %d pages", notes[i]->pages);
            return -1;
        }
        total += notes[i]->pages;
    }
    if (total > PC_PAK_DATA_PAGES) {
        snprintf(why, why_size, "%d pages: a pak holds %d", total, PC_PAK_DATA_PAGES);
        return -1;
    }
    for (int i = 0; i < n; i++)   /* each at its own slot, the first to ask for one keeping it */
        if (notes[i]->slot >= 0 && notes[i]->slot < PC_PAK_NOTES && at[notes[i]->slot] < 0)
            at[notes[i]->slot] = i;
    for (int i = 0; i < n; i++) {
        int placed = 0;
        for (int s = 0; s < PC_PAK_NOTES; s++)
            placed |= at[s] == i;
        for (int s = 0; !placed && s < PC_PAK_NOTES; s++)
            if (at[s] < 0)
                at[s] = i, placed = 1;
    }
    pc_pak_format(img);
    if (page0)
        memcpy(img, page0, PC_PAK_PAGE);
    uint8_t *inode = img + INODE;
    int page = FIRST_PAGE;
    for (int s = 0; s < PC_PAK_NOTES; s++) {
        if (slots_out)
            slots_out[s] = at[s];
        if (at[s] < 0)
            continue;
        const PcNote *note = notes[at[s]];
        uint8_t *e = img + DIR0 + 32 * s;
        memcpy(e, note->dir, 32);
        e[6] = 0, e[7] = (uint8_t)page;   /* start_page {bank 0, page} */
        for (int k = 0; k < note->pages; k++, page++) {
            memcpy(img + (size_t)page * PC_PAK_PAGE, note->data + (size_t)k * PC_PAK_PAGE, PC_PAK_PAGE);
            if (k == note->pages - 1) {
                uint16_t eof = PFS_EOF;
                memcpy(inode + 2 * page, &eof, 2);
            } else {
                inode[2 * page] = 0, inode[2 * page + 1] = (uint8_t)(page + 1);
            }
        }
    }
    unsigned bytes = 0;
    for (int i = 2 * FIRST_PAGE; i < PC_PAK_PAGE; i++)
        bytes += inode[i];
    inode[1] = (uint8_t)bytes;
    memcpy(img + INODE_BACKUP, inode, PC_PAK_PAGE);
    return 0;
}

/* ---------------------------------------------------------------- save files */

static uint32_t file_crc(const uint8_t *buf, size_t size)
{
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, buf, 0x10);
    static const uint8_t zero[4];
    crc = crc32(crc, zero, 4);
    size_t at = 0x14;
    while (at < size) {   /* zlib takes a uInt at a time */
        size_t k = size - at > 0x40000000u ? 0x40000000u : size - at;
        crc = crc32(crc, buf + at, (uInt)k);
        at += k;
    }
    return (uint32_t)crc;
}

int pc_save_file_make(const PcNote *note, int origin, const char *writer, uint8_t **out, size_t *size)
{
    PcNoteInfo info;
    size_t data = (size_t)note->pages * PC_PAK_PAGE, n = PC_NOTE_HEADER + data;
    uint8_t *b = calloc(1, n);
    if (!b)
        return -1;
    pc_note_info(note, &info);
    memcpy(b, "R49S", 4);
    put16(b + 4, 1);
    put16(b + 6, PC_NOTE_HEADER);
    b[8] = (uint8_t)info.kind;
    b[9] = (uint8_t)origin;
    b[10] = note->slot >= 0 && note->slot < PC_PAK_NOTES ? (uint8_t)note->slot : 0xFF;
    b[11] = host_little() ? 'L' : 'B';
    put32(b + 12, (uint32_t)data);
    memcpy(b + 0x14, note->dir, 32);
    b[0x14 + 6] = b[0x14 + 7] = 0;
    b[0x34] = info.track >= 0 ? (uint8_t)info.track : 0xFF;
    b[0x35] = info.backward >= 0 ? (uint8_t)info.backward : 0xFF;
    b[0x36] = info.car >= 0 ? (uint8_t)info.car : 0xFF;
    uint32_t ms = 0xFFFFFFFFu;
    if (info.kind == PC_NOTE_GHOST && info.finish >= 0 && info.finish < 36000.0f)
        ms = (uint32_t)lrintf(info.finish * 1000.0f);
    put32(b + 0x38, ms);
    memcpy(b + 0x3C, info.name, strlen(info.name) < 16 ? strlen(info.name) : 15);
    if (writer)
        memcpy(b + 0x4C, writer, strlen(writer) < 32 ? strlen(writer) : 31);
    memcpy(b + PC_NOTE_HEADER, note->data, data);
    put32(b + 0x10, file_crc(b, n));
    *out = b;
    *size = n;
    return 0;
}

int pc_save_file_read(const uint8_t *b, size_t size, PcNote *note, PcSaveFileInfo *fi, char *why, size_t why_size)
{
    char scratch[8];
    PcSaveFileInfo local;
    if (!why || !why_size)
        why = scratch, why_size = sizeof scratch;
    if (!fi)
        fi = &local;
    why[0] = 0;
    memset(note, 0, sizeof *note);
    memset(fi, 0, sizeof *fi);
    if (size < 4 || memcmp(b, "R49S", 4)) {
        snprintf(why, why_size, "it is not a Rush 2049 PC save file");
        return -1;
    }
    if (size < PC_NOTE_HEADER) {
        snprintf(why, why_size, "it is %zu bytes, shorter than the %d-byte header", size, PC_NOTE_HEADER);
        return -1;
    }
    fi->version = le16(b + 4);
    if (fi->version != 1) {
        snprintf(why, why_size, "it is version %d of the format (this program reads version 1)", fi->version);
        return -1;
    }
    if (le16(b + 6) != PC_NOTE_HEADER) {
        snprintf(why, why_size, "its header says it is %u bytes, not %d", le16(b + 6), PC_NOTE_HEADER);
        return -1;
    }
    uint32_t data = le32(b + 12);
    if (data == 0 || data % PC_PAK_PAGE || data / PC_PAK_PAGE > PC_PAK_DATA_PAGES) {
        snprintf(why, why_size, "it says it holds %u bytes, not 1 to %d pages of 256", data, PC_PAK_DATA_PAGES);
        return -1;
    }
    if (size != PC_NOTE_HEADER + (size_t)data) {
        snprintf(why, why_size, "it is %zu bytes, and its header says %zu", size, PC_NOTE_HEADER + (size_t)data);
        return -1;
    }
    if (file_crc(b, size) != le32(b + 0x10)) {
        snprintf(why, why_size, "its CRC is wrong (the file is damaged)");
        return -1;
    }
    if (b[11] != (host_little() ? 'L' : 'B')) {
        snprintf(why, why_size, "its note is in the other byte order ('%c')", b[11] >= 32 && b[11] < 127 ? b[11] : '?');
        return -1;
    }
    fi->kind = b[8], fi->origin = b[9], fi->slot = b[10] < PC_PAK_NOTES ? b[10] : -1;
    memcpy(fi->writer, b + 0x4C, 32);
    fi->writer[32] = 0;
    if (fi->kind > PC_NOTE_GHOST || fi->origin > PC_ORIGIN_EXPORT || (b[10] >= PC_PAK_NOTES && b[10] != 0xFF)) {
        snprintf(why, why_size, "its header's kind, origin or slot is out of range");
        return -1;
    }
    memcpy(note->dir, b + 0x14, 32);
    note->dir[6] = note->dir[7] = 0;
    if (host32(note->dir) == 0 || host16(note->dir + 4) == 0) {
        snprintf(why, why_size, "its directory entry has no game code");
        return -1;
    }
    note->slot = fi->slot;
    note->pages = (int)(data / PC_PAK_PAGE);
    note->data = malloc(data);
    if (!note->data) {
        snprintf(why, why_size, "no memory");
        return -1;
    }
    memcpy(note->data, b + PC_NOTE_HEADER, data);
    PcNoteInfo info;
    pc_note_info(note, &info);
    if (info.kind != fi->kind) {
        static const char *const kinds[] = {"another game's note", "a profile", "a ghost"};
        char ghost_why[160] = "";
        if (fi->kind == PC_NOTE_GHOST)   /* why the note isn't a ghost: the game's own check says it */
            pc_ghost_check(note->data, data, 0, ghost_why, sizeof ghost_why);
        snprintf(why, why_size, "its header says %s and the note is %s%s%s%s", kinds[fi->kind],
                 info.kind == PC_NOTE_OTHER && rush_note(note) ? "not one of the game's" : kinds[info.kind],
                 ghost_why[0] ? " (" : "", ghost_why, ghost_why[0] ? ")" : "");
        pc_note_free(note);
        return -1;
    }
    return 0;
}

void pc_save_safe_name(char *s)
{
    static const char *const reserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
                                           "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
                                           "LPT6", "LPT7", "LPT8", "LPT9"};
    for (char *p = s; *p; p++)
        if ((unsigned char)*p < 32 || (unsigned char)*p > 126 || strchr("\\/:*?\"<>|", *p))
            *p = '_';
    char *start = s;
    while (*start == ' ')
        start++;
    memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '.'))
        s[--n] = 0;
    if (!n)
        snprintf(s, 8, "unnamed");
    size_t base = strcspn(s, ".");
    for (size_t i = 0; i < sizeof reserved / sizeof *reserved; i++)
        if (strlen(reserved[i]) == base && !strncasecmp(s, reserved[i], base)) {
            memmove(s + base + 1, s + base, strlen(s + base) + 1);
            s[base] = '_';
            break;
        }
}

void pc_save_file_name(const PcNote *note, char *out, size_t size)
{
    PcNoteInfo info;
    char name[32];
    pc_note_info(note, &info);
    snprintf(name, sizeof name, "%s", info.name);
    pc_save_safe_name(name);
    if (info.kind == PC_NOTE_PROFILE) {
        snprintf(out, size, "%s.r49save", name);
    } else if (info.kind == PC_NOTE_GHOST) {
        char when[32];
        if (info.finish > 0 && info.finish < 36000.0f) {
            long cs = lrintf(info.finish * 100.0f);
            snprintf(when, sizeof when, "%ldm%02ld.%02lds", cs / 6000, cs / 100 % 60, cs % 100);
        } else {
            snprintf(when, sizeof when, "no time");
        }
        snprintf(out, size, "track %d %s - %s - %s.r49ghost", info.track + 1, info.backward ? "backward" : "forward",
                 name, when);
    } else {
        snprintf(out, size, "note %08X %d.r49note", (unsigned)pc_note_game_code(note), note->slot >= 0 ? note->slot : 0);
    }
}
