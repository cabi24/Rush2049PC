/* port/assets/musyx.c: the MusyX sound data in the host's byte order (docs/port/MUSYX_DATA.md).
 *
 * The sound library in the boot segment (src/rom/lib_15a10.c, lib_16320.c, lib_17dc0.c, lib_1a660.c,
 * lib_22300.c) reads these files in place with native loads and keeps pointers into them. So each field is
 * swapped in place at the width the library reads it, and the files keep their N64 size and layout:
 *
 *   wave 6        project: groups, their id lists, sfx tables, song pages and MIDI setups
 *   wave 7        pool: macros, tables (curves and envelopes), keymaps, layers
 *   wave 8        sample directory
 *   waves 10..21  songs: header, track table, track events, patterns, note events, tempo track
 *
 * Bytes stay where they are. The samples stay in the ROM: the library DMAs them into big-endian caches for
 * port/audio. A field is swapped once even when several records lead to it (tracks share patterns), and every
 * offset is checked against the block, so a file that doesn't parse is converted up to the first bad record
 * and logged. RUSH_TRACE_WAVES=1 prints what each file held. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

#define WAVE_PROJECT 6
#define WAVE_POOL 7
#define WAVE_SDIR 8
#define WAVE_SONG_FIRST 10
#define WAVE_SONG_LAST 21

/* What the conversion did at each byte of the file. */
enum { MAP_NONE, MAP_BYTE, MAP_16, MAP_32, MAP_INSIDE };

typedef struct {
    uint8_t *buf;
    size_t size;
    uint8_t *map;       /* one MAP_* per byte */
    uint8_t *adsr;      /* pool: bit per table id that a macro step reads as an envelope */
    int wave;
    unsigned errors;
    unsigned n[8];      /* record counts for the trace line */
} Mx;

static void mx_bad(Mx *m, size_t off, const char *what)
{
    if (!m->errors++)
        fprintf(stderr, "rush2049pc: musyx: wave %d: %s at 0x%zx doesn't parse; that walk stops there\n",
                m->wave, what, off);
}

static int mx_in(const Mx *m, size_t off, size_t len)
{
    return off <= m->size && len <= m->size - off;
}

/* The field of `width` bytes at off: its N64 value, swapped to native in place the first time. Returns 0 when it
 * is outside the block, or overlaps something converted at another width. */
static int mx_field(Mx *m, size_t off, int width, uint32_t *value, const char *what)
{
    uint8_t mark = width == 2 ? MAP_16 : MAP_32;
    uint8_t *p = m->buf + off;
    uint16_t h;
    uint32_t w;

    if (!mx_in(m, off, (size_t)width)) {
        mx_bad(m, off, what);
        return 0;
    }
    if (m->map[off] == mark) {   /* converted already: read it native */
        if (width == 2)
            memcpy(&h, p, 2), w = h;
        else
            memcpy(&w, p, 4);
        *value = w;
        return 1;
    }
    for (int i = 0; i < width; i++)
        if (m->map[off + i] != MAP_NONE) {
            mx_bad(m, off, what);
            return 0;
        }
    if (width == 2) {
        *value = pc_be16(p);
        pc_swap16_at(p);
    } else {
        *value = pc_be32(p);
        pc_swap32_at(p);
    }
    m->map[off] = mark;
    memset(m->map + off + 1, MAP_INSIDE, (size_t)width - 1);
    return 1;
}

static int mx16(Mx *m, size_t off, uint32_t *value, const char *what) { return mx_field(m, off, 2, value, what); }
static int mx32(Mx *m, size_t off, uint32_t *value, const char *what) { return mx_field(m, off, 4, value, what); }

/* A byte the library reads as a byte. It must not sit inside a swapped field. */
static int mx8(Mx *m, size_t off, uint32_t *value, const char *what)
{
    if (!mx_in(m, off, 1) || (m->map[off] != MAP_NONE && m->map[off] != MAP_BYTE)) {
        mx_bad(m, off, what);
        return 0;
    }
    m->map[off] = MAP_BYTE;
    *value = m->buf[off];
    return 1;
}

/* ---------------------------------------------------------------- project (wave 6)
 *
 * Groups follow each other until a size of 0xFFFFFFFF (sndPushGroup, func_8001536C in lib_15a10.c):
 *   +0 u32 size; +4 u16 id; +6 u16 type; +8 u32 offsets[], relative to the group + 8:
 *   [0] macro ids, [1] sample ids, [2] table ids, [3] keymap ids, [4] layer ids (u16 lists ended by 0xFFFF);
 *   type 1 (sfx): [5] the sfx table; type 0 (songs): [5] normal page, [6] drum page, [7] MIDI setups
 *   (func_8001558C). */

static void mx_ids(Mx *m, size_t off)
{
    uint32_t id;
    for (; mx16(m, off, &id, "id list"); off += 2) {
        if (id == 0xFFFF)
            return;
        m->n[1]++;
    }
}

/* u16 count, u16, then count 12-byte entries: u16 id, u16 macro/keymap/layer id, 8 bytes (func_800164D0,
 * func_80017040, func_8001B1D0). */
static void mx_sfx(Mx *m, size_t off)
{
    uint32_t count, v;
    if (!mx16(m, off, &count, "sfx table") || !mx16(m, off + 2, &v, "sfx table"))
        return;
    for (uint32_t i = 0; i < count; i++) {
        size_t e = off + 4 + 12 * (size_t)i;
        if (!mx16(m, e, &v, "sfx entry") || !mx16(m, e + 2, &v, "sfx entry"))
            return;
        m->n[2]++;
    }
}

/* 8-byte entries until one whose byte 5 (the program) is 0xFF: u16 macro, then bytes (func_8001785C,
 * func_800178B0, func_80017720). */
static void mx_page(Mx *m, size_t off)
{
    uint32_t program, v;
    for (;; off += 8) {
        if (!mx8(m, off + 5, &program, "page"))
            return;
        if (program == 0xFF)
            return;
        if (!mx16(m, off, &v, "page"))
            return;
        m->n[3]++;
    }
}

/* 132-byte MIDI setups until id 0xFFFF: u16 song id, u16, 16 channels of 8 bytes (func_8001558C). */
static void mx_setups(Mx *m, size_t off)
{
    uint32_t id, v;
    for (;; off += 132) {
        if (!mx16(m, off, &id, "MIDI setup") || id == 0xFFFF)
            return;
        if (!mx16(m, off + 2, &v, "MIDI setup") || !mx_in(m, off, 132)) {
            mx_bad(m, off, "MIDI setup");
            return;
        }
        m->n[4]++;
    }
}

static void mx_project(Mx *m)
{
    size_t off = 0;
    uint32_t size, id, type, offsets[8];

    while (mx32(m, off, &size, "group") && size != 0xFFFFFFFF) {
        int count;
        if (!mx16(m, off + 4, &id, "group") || !mx16(m, off + 6, &type, "group"))
            return;
        count = type == 0 ? 8 : type == 1 ? 6 : 5;
        if (size < 8 + 4 * (uint32_t)count || !mx_in(m, off, size)) {
            mx_bad(m, off, "group size");
            return;
        }
        for (int k = 0; k < count; k++)
            if (!mx32(m, off + 8 + 4 * (size_t)k, &offsets[k], "group"))
                return;
        for (int k = 0; k < 5; k++)
            mx_ids(m, off + 8 + offsets[k]);
        if (type == 1)
            mx_sfx(m, off + 8 + offsets[5]);
        if (type == 0) {
            mx_page(m, off + 8 + offsets[5]);
            mx_page(m, off + 8 + offsets[6]);
            mx_setups(m, off + 8 + offsets[7]);
        }
        m->n[0]++;
        off += size;
    }
}

/* ---------------------------------------------------------------- pool (wave 7)
 *
 * +0 u32 offsets[4] from the pool start, to four lists of entries {u32 size; u16 id; u16; payload at +8}
 * that end with a size of 0xFFFFFFFF (func_80014E1C):
 *   macros: 8-byte steps, two u32 words each (func_80023E9C copies them word by word);
 *   tables: bytes (volume curves, func_8002321C), except the ones a macro step 0x0C reads as an envelope of
 *           four u16 (func_80022F24);
 *   keymaps: 8-byte entries {u16 id; s8; u8; s16 priority; u8[2]} (func_8001A270);
 *   layers: the entry header has a u16 count at +0xA and the payload at +12, 12-byte entries
 *           {u16 id; u8; u8; s8; u8; s16 priority; u8; u8[3]} (func_80015058, func_80019F48). */

enum { LIST_MACRO, LIST_TABLE, LIST_KEYMAP, LIST_LAYER };

static void mx_pool_list(Mx *m, size_t e, int kind)
{
    static const char *const what[] = {"macro", "table", "keymap", "layer"};
    uint32_t size, id, v, count, w0;

    while (mx32(m, e, &size, what[kind]) && size != 0xFFFFFFFF) {
        size_t header = kind == LIST_LAYER ? 12 : 8;
        if (size < header || !mx_in(m, e, size)) {
            mx_bad(m, e, what[kind]);
            return;
        }
        if (!mx16(m, e + 4, &id, what[kind]) || !mx16(m, e + 6, &v, what[kind]))
            return;
        switch (kind) {
        case LIST_MACRO:
            for (size_t p = e + 8; p + 8 <= e + size; p += 8) {
                if (!mx32(m, p, &w0, "macro step") || !mx32(m, p + 4, &v, "macro step"))
                    return;
                if ((w0 & 0x7F) == 0x0C) {   /* SETADSR: the table id is bits 8..23 */
                    uint32_t table = (w0 >> 8) & 0xFFFF;
                    m->adsr[table >> 3] |= (uint8_t)(1 << (table & 7));
                }
            }
            break;
        case LIST_TABLE:
            if (m->adsr[id >> 3] & (1 << (id & 7))) {
                if (size < 16) {
                    mx_bad(m, e, "envelope");
                    return;
                }
                for (int k = 0; k < 4; k++)
                    if (!mx16(m, e + 8 + 2 * (size_t)k, &v, "envelope"))
                        return;
                m->n[5]++;
            }
            break;
        case LIST_KEYMAP:
            for (size_t p = e + 8; p + 8 <= e + size; p += 8)
                if (!mx16(m, p, &v, "keymap") || !mx16(m, p + 4, &v, "keymap"))
                    return;
            break;
        case LIST_LAYER:
            if (!mx16(m, e + 8, &v, "layer") || !mx16(m, e + 10, &count, "layer"))
                return;
            if (12 + 12 * (size_t)count > size) {
                mx_bad(m, e, "layer count");
                return;
            }
            for (size_t p = e + 12; p < e + 12 + 12 * (size_t)count; p += 12)
                if (!mx16(m, p, &v, "layer") || !mx16(m, p + 6, &v, "layer"))
                    return;
            break;
        }
        m->n[kind]++;
        e += size;
    }
}

static void mx_pool(Mx *m)
{
    uint32_t offsets[4];

    m->adsr = calloc(0x10000 / 8, 1);
    if (!m->adsr)
        return;
    for (int k = 0; k < 4; k++)
        if (!mx32(m, 4 * (size_t)k, &offsets[k], "pool header"))
            return;
    /* Macros first: their steps say which tables are envelopes. */
    for (int k = LIST_MACRO; k <= LIST_LAYER; k++)
        mx_pool_list(m, offsets[k], k);
}

/* ---------------------------------------------------------------- sample directory (wave 8)
 *
 * 0x1C-byte records until id 0xFFFF (func_8001605C, func_800162AC, func_80016CF0): u16 id; u16 reference count
 * and u32 address, both written at run time; u32 offset of the sample from the sample base (ROM 0x39B40);
 * u32 info (key << 24 | rate); u32 length (format << 24 | samples); u32 loop start; u32 loop length. */

static void mx_sdir(Mx *m)
{
    uint32_t id, v;
    for (size_t off = 0; mx16(m, off, &id, "sample") && id != 0xFFFF; off += 0x1C) {
        if (!mx_in(m, off, 0x1C)) {
            mx_bad(m, off, "sample");
            return;
        }
        if (!mx16(m, off + 2, &v, "sample"))
            return;
        for (size_t k = 4; k < 0x1C; k += 4)
            if (!mx32(m, off + k, &v, "sample"))
                return;
        m->n[0]++;
    }
}

/* ---------------------------------------------------------------- songs (waves 10..21)
 *
 * Header (func_800178B0, func_80018634), offsets from the song start: +0 u32 track table; +4 u32 pattern table;
 * +8 u32 channel map (64 bytes); +0xC u32 tempo track (0 = none); +0x10 u32 tempo; +0x14 u32 loop time.
 * Track table: 64 u32 offsets (0 = no track) to 12-byte events {u32 time; u8 program; u8 controller; u16;
 * u16 code; u16 loop index or two bytes}, code 0xFFFF ending the track, 0xFFFE looping it, anything else a
 * pattern index. Pattern table: u32 offsets to patterns {u32; u32 pitch-bend stream; u32 modulation stream;
 * note events}, the streams being bytes (func_8001824C, func_80018448). Note events (func_80017D38):
 * u16 delta time, key and velocity bytes, and for a note a u16 duration; key = velocity = 0xFF ends the
 * pattern. Tempo track (func_80018A30): {u32 time; u32 tempo} until a time of 0xFFFFFFFF. */

/* func_80017D38: which events carry a duration. */
static size_t note_size(uint32_t key, uint32_t velocity)
{
    if ((key & 0x80) && (velocity == 0 || velocity == 1 || (velocity & 0x80)))
        return 4;   /* program change, volume, controller */
    return key || velocity ? 6 : 4;
}

static void mx_pattern(Mx *m, size_t table, uint32_t index)
{
    uint32_t off, v, key, velocity;
    size_t n;

    if (!mx32(m, table + 4 * (size_t)index, &off, "pattern table"))
        return;
    if (mx_in(m, off, 4) && m->map[off] == MAP_32)
        return;   /* converted for an earlier event */
    for (int k = 0; k < 3; k++)
        if (!mx32(m, off + 4 * (size_t)k, &v, "pattern"))
            return;
    for (n = off + 12;; n += note_size(key, velocity)) {
        if (!mx16(m, n, &v, "note") || !mx8(m, n + 2, &key, "note") || !mx8(m, n + 3, &velocity, "note"))
            return;
        if (key == 0xFF && velocity == 0xFF)
            break;
        if (note_size(key, velocity) == 6 && !mx16(m, n + 4, &v, "note"))
            return;
        m->n[4]++;
    }
    m->n[3]++;
}

static void mx_track(Mx *m, size_t off, size_t patterns)
{
    uint32_t v, code;
    for (;; off += 12) {
        if (!mx32(m, off, &v, "track event") || !mx16(m, off + 6, &v, "track event") ||
            !mx16(m, off + 8, &code, "track event"))
            return;
        if (code == 0xFFFF)
            return;
        if (code == 0xFFFE) {   /* loop: the index of the event to go back to */
            mx16(m, off + 10, &v, "track event");
            return;
        }
        m->n[2]++;
        mx_pattern(m, patterns, code);
    }
}

static void mx_song(Mx *m)
{
    uint32_t h[6], track, time, v;

    for (int k = 0; k < 6; k++)
        if (!mx32(m, 4 * (size_t)k, &h[k], "song header"))
            return;
    for (size_t j = 0; j < 64; j++) {
        if (!mx32(m, h[0] + 4 * j, &track, "track table"))
            return;
        if (track && !(mx_in(m, track, 4) && m->map[track] == MAP_32)) {
            m->n[1]++;
            mx_track(m, track, h[1]);
        }
    }
    if (h[3])
        for (size_t off = h[3]; mx32(m, off, &time, "tempo track") && time != 0xFFFFFFFF; off += 8) {
            if (!mx32(m, off + 4, &v, "tempo track"))
                return;
            m->n[5]++;
        }
    m->n[0]++;
}

/* ---------------------------------------------------------------- a blank cartridge's files
 *
 * --no-rom gives every wave a block of zeros (port/main.c). sndPushGroup would walk an all-zero project forever
 * (group size 0), so a block that is entirely zero, which no real MusyX file is, becomes the empty file of its
 * kind, already in host order. A zero song is left as it is: nothing can start it without a group. */

static int mx_blank(Mx *m)
{
    static const uint32_t pool[5] = {16, 16, 16, 16, 0xFFFFFFFF};
    static const uint32_t end32 = 0xFFFFFFFF;
    static const uint16_t end16 = 0xFFFF;

    for (size_t i = 0; i < m->size; i++)
        if (m->buf[i])
            return 0;
    if (m->wave == WAVE_PROJECT)
        memcpy(m->buf, &end32, 4);
    else if (m->wave == WAVE_POOL && m->size >= sizeof pool)
        memcpy(m->buf, pool, sizeof pool);
    else if (m->wave == WAVE_SDIR)
        memcpy(m->buf, &end16, 2);
    return 1;
}

/* ---------------------------------------------------------------- the hook */

static int trace(void)
{
    const char *e = getenv("RUSH_TRACE_WAVES");
    return e && *e && *e != '0';
}

void pc_assets_musyx(int wave, uint8_t *buf, size_t size)
{
    Mx m = {buf, size, NULL, NULL, wave, 0, {0}};
    const char *kind = NULL;

    if (wave != WAVE_PROJECT && wave != WAVE_POOL && wave != WAVE_SDIR &&
        !(wave >= WAVE_SONG_FIRST && wave <= WAVE_SONG_LAST))
        return;
    if (size < 4) {
        fprintf(stderr, "rush2049pc: musyx: wave %d is empty\n", wave);
        return;
    }
    if (mx_blank(&m)) {
        if (trace())
            fprintf(stderr, "rush2049pc: musyx: wave %d is all zero (a blank cartridge): an empty file\n", wave);
        pc_mem_unregister_be(buf);
        return;
    }
    m.map = calloc(size, 1);
    if (!m.map) {
        fprintf(stderr, "rush2049pc: musyx: wave %d: out of memory; left big-endian\n", wave);
        return;
    }
    switch (wave) {
    case WAVE_PROJECT:
        mx_project(&m), kind = "project";
        break;
    case WAVE_POOL:
        mx_pool(&m), kind = "pool";
        break;
    case WAVE_SDIR:
        mx_sdir(&m), kind = "sample directory";
        break;
    default:
        mx_song(&m), kind = "song";
        break;
    }
    if (m.errors > 1)
        fprintf(stderr, "rush2049pc: musyx: wave %d: %u records didn't parse\n", wave, m.errors);
    if (trace()) {
        if (wave == WAVE_PROJECT)
            fprintf(stderr, "rush2049pc: musyx: %s: %u groups, %u ids, %u sfx, %u page entries, %u MIDI setups\n",
                    kind, m.n[0], m.n[1], m.n[2], m.n[3], m.n[4]);
        else if (wave == WAVE_POOL)
            fprintf(stderr, "rush2049pc: musyx: %s: %u macros, %u tables (%u envelopes), %u keymaps, %u layers\n",
                    kind, m.n[0], m.n[1], m.n[5], m.n[2], m.n[3]);
        else if (wave == WAVE_SDIR)
            fprintf(stderr, "rush2049pc: musyx: %s: %u samples\n", kind, m.n[0]);
        else
            fprintf(stderr, "rush2049pc: musyx: %s %d: %u tracks, %u pattern events, %u patterns, %u notes, "
                    "%u tempo changes\n", kind, wave - WAVE_SONG_FIRST, m.n[1], m.n[2], m.n[3], m.n[4], m.n[5]);
    }
    free(m.adsr);
    free(m.map);
    /* The read marked the block as big-endian ROM bytes; it is native now. */
    pc_mem_unregister_be(buf);
}
