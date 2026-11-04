/* port/assets/check.c: whether a file can stand in for a cartridge file (docs/port/MODS.md 4): the rules a mod's
 * replacement must meet before the game reads it in place of the cartridge's.
 *
 * pc_assets_check takes the file as the cartridge holds it once read (plain, big-endian, before any converter) and
 * says whether the game can take it for that wave. It changes nothing and keeps no state, so it can run before the
 * game starts, on files the game will never see. Its rules are of two kinds:
 *
 *   the structure: everything the converters (assets.c's) and the game's set-up read must be inside the file: the
 *     converters convert it whole (none says "left as is"), SetupWaveModel's relocation, its vertex pass and the
 *     mirror pass stay inside it and end, the run-length lists the road's readers expand into fixed stack arrays fit
 *     them, and the names the game bsearches are in order;
 *   the cartridge's file (when cart is given): what the game looks up in it by name or by number is still there: the
 *     models', textures' and palettes' names of a WAD, a world's names, the text bank's string counts, the font's
 *     letters, the paths the cartridge's file has.
 *
 * A file that passes can still look or play wrong (any bytes in its display lists, texels or floats are taken as
 * they come, as on the N64), but the program doesn't read or write outside it while loading and setting it up.
 * tools/mods/rushmods.py `check` applies the same rules. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

#define T4(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define TAG_OBHD T4('O', 'B', 'H', 'D')
#define TAG_TXHD T4('T', 'X', 'H', 'D')
#define TAG_PLHD T4('P', 'L', 'H', 'D')
#define TAG_PTHD T4('P', 'T', 'H', 'D')
#define TAG_TXLD T4('T', 'X', 'L', 'D')
#define TAG_IMAG T4('I', 'M', 'A', 'G')
#define TAG_WHDR T4('W', 'H', 'D', 'R')
#define TAG_WOBJ T4('W', 'O', 'B', 'J')
#define TAG_GDAT T4('G', 'D', 'A', 'T')
#define TAG_GTLD T4('G', 'T', 'L', 'D')

#define MAX_TOC 4096          /* a TOC of more entries is no WAD */
#define MAX_DL_DEPTH 16       /* calls nested in a display list, for the mirror pass's recursion */
#define ROAD_LIST_MAX 35      /* a road leaf's surfaces: list[36] in entity_update and camera_trigger_check, and
                               * camera_victory's polys[142] takes four leaves' */
#define WAVE_SAMPLES 9

typedef struct {
    const uint8_t *b;
    size_t size;
    char *why;
    size_t why_size;
} Ck;

#ifdef __MINGW_PRINTF_FORMAT   /* mingw-w64: the printf vsnprintf is (gnu_printf with its ANSI stdio: %zu) */
#define CK_PRINTF __MINGW_PRINTF_FORMAT
#else
#define CK_PRINTF printf
#endif
static int bad(Ck *c, const char *fmt, ...) __attribute__((format(CK_PRINTF, 2, 3)));
static int bad(Ck *c, const char *fmt, ...)
{
    va_list ap;
    if (c->why && c->why_size) {
        va_start(ap, fmt);
        vsnprintf(c->why, c->why_size, fmt, ap);
        va_end(ap);
    }
    return -1;
}

static int in(const Ck *c, uint64_t off, uint64_t len) { return off <= c->size && len <= c->size - off; }
static uint32_t be32(const Ck *c, size_t off) { return pc_be32(c->b + off); }
static uint16_t be16(const Ck *c, size_t off) { return pc_be16(c->b + off); }

/* ---------------------------------------------------------------- WADs */

typedef struct {
    uint32_t tag, off, count;
} Chunk;

typedef struct {
    Ck *c;
    uint32_t toc, n;
    Chunk first[10];   /* each tag's first entry, as MBFindInWad finds it; tag 0: none */
} Wad;

static const uint32_t k_tags[10] = {TAG_OBHD, TAG_TXHD, TAG_PLHD, TAG_PTHD, TAG_TXLD,
                                    TAG_IMAG, TAG_WHDR, TAG_WOBJ, TAG_GDAT, TAG_GTLD};

static const Chunk *chunk(const Wad *w, uint32_t tag)
{
    for (int i = 0; i < 10; i++)
        if (k_tags[i] == tag)
            return w->first[i].tag ? &w->first[i] : NULL;
    return NULL;
}

/* Bytes from off to the next chunk's start (or the TOC's, or the file's end): what a chunk there may fill. */
static uint64_t room(const Wad *w, uint32_t off)
{
    uint64_t end = w->c->size;
    for (uint32_t i = 0; i < w->n; i++) {
        uint32_t o = be32(w->c, w->toc + 12 * i + 4);
        if (o > off && o < end)
            end = o;
    }
    if (w->toc > off && w->toc < end)
        end = w->toc;
    return end - off;
}

static void tag_text(uint32_t tag, char out[5])
{
    for (int i = 0; i < 4; i++) {
        char ch = (char)(tag >> (24 - 8 * i));
        out[i] = ch >= 0x20 && ch < 0x7F ? ch : '?';
    }
    out[4] = 0;
}

static int wad_open(Ck *c, Wad *w)
{
    memset(w, 0, sizeof *w);
    w->c = c;
    if (c->size < 8)
        return bad(c, "it is %zu bytes: too short for a WAD's header", c->size);
    w->toc = be32(c, 0), w->n = be32(c, 4);
    if (w->toc & 0x80000000u)
        return bad(c, "its table of contents is already relocated (0x%08X): not a file as the cartridge holds it",
                   w->toc);
    if (w->toc < 8 || (w->toc & 3) || w->toc > c->size || w->n > MAX_TOC || w->n > (c->size - w->toc) / 12)
        return bad(c, "no WAD table of contents (offset 0x%X, %u entries, file 0x%zX bytes)", w->toc, w->n, c->size);
    for (uint32_t i = 0; i < w->n; i++) {
        uint32_t tag = be32(c, w->toc + 12 * i), off = be32(c, w->toc + 12 * i + 4);
        char t[5];
        if (off > c->size) {
            tag_text(tag, t);
            return bad(c, "chunk %s starts at 0x%X, past the file's end (0x%zX)", t, off, c->size);
        }
        for (int k = 0; k < 10; k++)
            if (k_tags[k] == tag && !w->first[k].tag)
                w->first[k] = (Chunk){tag, off, be32(c, w->toc + 12 * i + 8)};
    }
    return 0;
}

/* A record chunk: aligned, and its records before the next chunk. */
static int records_fit(const Wad *w, uint32_t tag, uint32_t rec)
{
    const Chunk *k = chunk(w, tag);
    char t[5];
    if (!k || !k->count)
        return 0;
    tag_text(tag, t);
    if (k->off & 3)
        return bad(w->c, "chunk %s at 0x%X is not 4-aligned", t, k->off);
    if ((uint64_t)k->count * rec > room(w, k->off))
        return bad(w->c, "chunk %s's %u records of 0x%X bytes run past it (0x%llX bytes there)", t, k->count, rec,
                   (unsigned long long)room(w, k->off));
    return 0;
}

/* The game bsearches OBHD, TXHD and PLHD by name (entity_name_copy with func_8008AD04, strcmp), and NewWorld WHDR:
 * names end within their 16 bytes and don't go down. */
static int names_sorted(const Wad *w, const char *what, uint32_t first, uint32_t count, uint32_t stride)
{
    const char *prev = NULL;
    for (uint32_t i = 0; i < count; i++) {
        const char *name = (const char *)w->c->b + first + (size_t)i * stride;
        if (!memchr(name, 0, 16))
            return bad(w->c, "%s %u's name doesn't end within its 16 bytes", what, i);
        if (prev && strcmp(prev, name) > 0)   /* the cartridge has a name twice: bsearch finds one of them */
            return bad(w->c, "%s %u, '%.16s', is out of order after '%.16s' (the game looks them up by name, in "
                       "order)", what, i, name, prev);
        prev = name;
    }
    return 0;
}

/* entity_lod_select (SetupWaveModel's relocation, src/blob/entity_lod_select.c): one pass from the list's start,
 * which doesn't enter G_DL, follows E1 + 04 and ends at the first 0xDF; and func_800966D8, the vertex pass, from its
 * first G_VTX: 16-byte vertices while bit 15 of the halfword at +6 says another follows. */
static int dl_relocation(Ck *c, uint32_t start, const char *what)
{
    uint8_t *seen = calloc(c->size / 8 + 1, 1);
    uint32_t pos = start, latched = 0;
    int first = 1, r = 0;
    if (!seen)
        return bad(c, "out of memory");
    for (;;) {
        if (!in(c, pos, 8)) {
            r = bad(c, "%s runs past the file's end (at 0x%X) before its G_ENDDL", what, pos);
            break;
        }
        if (seen[pos / 8]) {
            r = bad(c, "%s comes back to 0x%X: the relocation would never end", what, pos);
            break;
        }
        seen[pos / 8] = 1;
        uint32_t w0 = be32(c, pos), w1 = be32(c, pos + 4);
        uint8_t op = (uint8_t)(w0 >> 24);
        if ((op & 0xC0) == 0x40 || (op & 0xC0) == 0x80 || (op >= 9 && op < 0x40) || (op >= 0xC0 && op < 0xD6)) {
            pos += 8;
            continue;
        }
        if (op == 0x01 && first) {
            first = 0;
            if (w1 & 0xFF000000u) {
                r = bad(c, "%s's first G_VTX (at 0x%X) names 0x%08X, not a place in the file", what, pos, w1);
                break;
            }
            for (uint32_t v = w1;; v += 16) {
                if (!in(c, v, 10)) {
                    r = bad(c, "%s's vertices from 0x%X run past the file's end", what, w1);
                    break;
                }
                if (!(be16(c, v + 6) & 0x8000))
                    break;
            }
            if (r)
                break;
        } else if (op == 0xE1) {
            latched = w1;
        } else if (op == 0x04) {   /* it goes on at the command after the target's first */
            if ((latched & 0xFFFFFF) + 8 >= c->size || (latched & 0xF0000000u)) {
                r = bad(c, "%s branches (E1 04) to 0x%08X, outside the file", what, latched);
                break;
            }
            pos = (latched & 0xFFFFFF) + 8;
            continue;
        } else if (op == 0xDF) {
            break;
        }
        pos += 8;
    }
    free(seen);
    return r;
}

/* display_list_traverse (src/blob/display_list_traverse.c) as the mirror pass runs it (flags 2): to the word
 * 0xDF000000; a G_DL call recurses, a G_DL branch and E1 + 04 jump. Every target in the file, no loop. */
static int dl_traverse(Ck *c, uint32_t start, int depth, uint32_t *stack, const char *what)
{
    uint8_t *seen;
    uint32_t pos = start;
    int r = 0;
    if (depth >= MAX_DL_DEPTH)
        return bad(c, "%s calls display lists more than %d deep", what, MAX_DL_DEPTH);
    for (int i = 0; i < depth; i++)
        if (stack[i] == start)
            return bad(c, "%s calls the list at 0x%X from inside itself", what, start);
    stack[depth] = start;
    if (!(seen = calloc(c->size / 8 + 1, 1)))
        return bad(c, "out of memory");
    for (;;) {
        if (!in(c, pos, 8)) {
            r = bad(c, "%s (a list at 0x%X) runs past the file's end before its G_ENDDL", what, start);
            break;
        }
        if (seen[pos / 8]) {
            r = bad(c, "%s (a list at 0x%X) comes back to 0x%X and never ends", what, start, pos);
            break;
        }
        seen[pos / 8] = 1;
        uint32_t w0 = be32(c, pos), w1 = be32(c, pos + 4);
        if (w0 == 0xDF000000u)
            break;
        if ((w0 >> 24) == 0xDE || (w0 >> 24) == 0xE1) {
            int is_e1 = (w0 >> 24) == 0xE1;
            uint32_t to = w1 & 0xFFFFFF;
            int is_04 = is_e1 && in(c, pos + 8, 8) && c->b[pos + 8] == 0x04;
            if (is_e1 && !is_04) {
                pos += 16;
                continue;
            }
            if (to >= c->size || (to & 7) || (w1 & 0xF0000000u)) {
                r = bad(c, "%s (a list at 0x%X) goes to 0x%08X at 0x%X, outside the file", what, start, w1, pos);
                break;
            }
            if (!is_e1 && !(w0 & 0x00FF0000)) {   /* a call */
                if ((r = dl_traverse(c, to, depth + 1, stack, what)) != 0)
                    break;
                pos += 8;
                continue;
            }
            pos = to;   /* a branch */
            continue;
        }
        pos += 8;
    }
    free(seen);
    return r;
}

static int wad_models(Wad *w)
{
    Ck *c = w->c;
    const Chunk *ob = chunk(w, TAG_OBHD);
    uint32_t stack[MAX_DL_DEPTH];
    char what[48];
    if (!ob || !ob->count)
        return 0;
    if (names_sorted(w, "model", ob->off, ob->count, 0x58))
        return -1;
    for (uint32_t i = 0; i < ob->count; i++) {
        size_t r = ob->off + (size_t)i * 0x58;
        int16_t lods = (int16_t)be16(c, r + 0x16);
        if (lods < 0 || lods > 4)
            return bad(c, "model '%.16s' has %d levels of detail (0-4)", (const char *)c->b + r, lods);
        for (int l = 0; l < lods; l++) {
            uint32_t dl = be32(c, r + 0x18 + 0x10 * l + 8);
            snprintf(what, sizeof what, "model '%.16s' LOD %d", (const char *)c->b + r, l);
            if (dl >= c->size || (dl & 7))
                return bad(c, "%s's display list 0x%X is not an 8-aligned place in the file", what, dl);
            if (dl_relocation(c, dl, what) || dl_traverse(c, dl, 0, stack, what))
                return -1;
        }
    }
    return 0;
}

static int wad_textures(Wad *w)
{
    Ck *c = w->c;
    const Chunk *tx = chunk(w, TAG_TXHD), *pl = chunk(w, TAG_PLHD), *im = chunk(w, TAG_IMAG);
    const Chunk *tl = chunk(w, TAG_TXLD);
    uint32_t imag = im ? im->off : 0;
    if (tl && tl->count) {   /* entity_cull_check walks it from start to start + count, 8 bytes a command */
        if ((tl->off & 7) || !in(c, tl->off, tl->count))
            return bad(c, "the texture-load lists (TXLD, 0x%X bytes at 0x%X) are not inside the file, 8-aligned",
                       tl->count, tl->off);
        for (uint32_t p = tl->off; p + 8 <= (uint64_t)tl->off + tl->count; p += 8)   /* rebased in 24 bits */
            if (c->b[p] == 0xFD && !in(c, (imag + (be32(c, p + 4) & 0xFFFFFF)) & 0xFFFFFF, 1))
                return bad(c, "a texture-load list's G_SETTIMG at 0x%X names 0x%X past IMAG, outside the file", p,
                           be32(c, p + 4));
    }
    if (tx && tx->count) {
        if (names_sorted(w, "texture", tx->off, tx->count, 0x24))
            return -1;
        if (!im)
            return bad(c, "it has textures (TXHD) but no texels (IMAG)");
        for (uint32_t i = 0; i < tx->count; i++) {
            size_t r = tx->off + (size_t)i * 0x24;
            const char *name = (const char *)c->b + r;
            uint16_t wd = be16(c, r + 0x10), ht = be16(c, r + 0x12);
            uint32_t base = be32(c, r + 0x18), flags = be32(c, r + 0x1C);
            if (!wd || !ht || wd > 1024 || ht > 1024 || c->b[r + 0x14] > 3)
                return bad(c, "texture '%.16s' is %u x %u at size %u (1-1024 a side, sizes 0-3)", name, wd, ht,
                           c->b[r + 0x14]);
            uint32_t at = imag + base;   /* the game's 32-bit sum: a list may sit before IMAG */
            if ((flags & 0x08000000u) ? !in(c, at, 1) : !in(c, at, 8))   /* the texels start in the file */
                return bad(c, "texture '%.16s''s %s (0x%X past IMAG) %s outside the file", name,
                           flags & 0x08000000u ? "texels" : "load list", base, flags & 0x08000000u ? "are" : "is");
        }
    }
    if (pl && pl->count) {
        if (names_sorted(w, "palette", pl->off, pl->count, 0x18))
            return -1;
        if (!im)
            return bad(c, "it has palettes (PLHD) but no texels (IMAG)");
        for (uint32_t i = 0; i < pl->count; i++) {
            size_t r = pl->off + (size_t)i * 0x18;
            uint8_t s = c->b[r + 0x10], e = c->b[r + 0x11];
            if (s > e || !in(c, (uint32_t)(imag + be32(c, r + 0x14)), 2u * (e - s + 1)))
                return bad(c, "palette '%.16s' (entries %u-%u at 0x%X past IMAG) is outside the file",
                           (const char *)c->b + r, s, e, be32(c, r + 0x14));
        }
    }
    return 0;
}

/* The paths' points, which wad.c swaps in any WAD; in a track's (pthd), the camera follows trigPtr to another path
 * record (CamScene.link) and tests and clears its flags, so it is 0 or one of them. */
static int wad_paths(Wad *w, int pthd)
{
    Ck *c = w->c;
    const Chunk *pt = chunk(w, TAG_PTHD);
    if (!pt || !pt->count)
        return 0;
    for (uint32_t i = 0; i < pt->count; i++) {
        size_t r = pt->off + (size_t)i * 0x24;
        int16_t n = (int16_t)be16(c, r + 0x14);
        uint32_t pts = be32(c, r + 0x1C), trig = be32(c, r + 0x18);
        if (n > 0 && ((pts & 3) || !in(c, pts, 0x44ull * n)))
            return bad(c, "path '%.16s''s %d points at 0x%X are outside the file", (const char *)c->b + r, n, pts);
        if (pthd && trig && (trig < pt->off || (trig - pt->off) % 0x24 || (trig - pt->off) / 0x24 >= pt->count))
            return bad(c, "path '%.16s' links to 0x%X, which is no path record", (const char *)c->b + r, trig);
    }
    return 0;
}

/* The names of a record chunk of the cartridge's file that this file lacks. */
static int names_kept(Wad *w, const Wad *cw, uint32_t tag, uint32_t rec, const char *what)
{
    const Chunk *k = chunk(w, tag), *ck = chunk(cw, tag);
    if (!ck)
        return 0;
    for (uint32_t i = 0; i < ck->count; i++) {
        const char *name = (const char *)cw->c->b + ck->off + (size_t)i * rec;
        int found = 0;
        for (uint32_t j = 0; k && j < k->count && !found; j++)
            found = !strncmp(name, (const char *)w->c->b + k->off + (size_t)j * rec, 16);
        if (!found)
            return bad(w->c, "the cartridge's file has %s '%.16s', which this one lacks (the game looks it up)", what,
                       name);
    }
    return 0;
}

/* pthd: a track's WAD, whose paths the game reaches by number. wad.c converts the paths of any WAD. */
static int check_wad(Ck *c, Ck *cart, int pthd, Wad *w)
{
    Wad cw;
    if (wad_open(c, w) || records_fit(w, TAG_OBHD, 0x58) || records_fit(w, TAG_TXHD, 0x24) ||
        records_fit(w, TAG_PLHD, 0x18) || records_fit(w, TAG_PTHD, 0x24))
        return -1;
    if (wad_textures(w) || wad_models(w) || wad_paths(w, pthd))
        return -1;
    if (!cart || wad_open(cart, &cw) != 0)
        return 0;
    if (names_kept(w, &cw, TAG_OBHD, 0x58, "model") || names_kept(w, &cw, TAG_TXHD, 0x24, "texture") ||
        names_kept(w, &cw, TAG_PLHD, 0x18, "palette"))
        return -1;
    const Chunk *k = chunk(w, TAG_PTHD), *ck = chunk(&cw, TAG_PTHD);
    if (pthd && ck && ck->count > (k ? k->count : 0))
        return bad(c, "it has %u paths (PTHD), the cartridge's file %u: the game reaches them by number",
                   k ? k->count : 0, ck->count);
    return 0;
}

/* ---------------------------------------------------------------- worlds */

static int check_world(Ck *c, Ck *cart)
{
    Wad w, cw;
    if (check_wad(c, NULL, 0, &w) || records_fit(&w, TAG_WOBJ, 0x68) || records_fit(&w, TAG_GDAT, 0x1C) ||
        records_fit(&w, TAG_GTLD, 4))
        return -1;
    const Chunk *wh = chunk(&w, TAG_WHDR), *wo = chunk(&w, TAG_WOBJ), *gd = chunk(&w, TAG_GDAT);
    const Chunk *gt = chunk(&w, TAG_GTLD);
    if (!wh)
        return bad(c, "it has no world table (WHDR)");
    if ((wh->off & 3) || !in(c, wh->off, 4))
        return bad(c, "its world table (WHDR) at 0x%X is not 4-aligned in the file", wh->off);
    uint32_t worlds = be32(c, wh->off);
    if (4 + 0x14ull * worlds > room(&w, wh->off))
        return bad(c, "its world table's %u entries run past it", worlds);
    if (names_sorted(&w, "world", wh->off + 8, worlds, 0x14))
        return -1;
    uint8_t *seen = calloc(c->size / 4 + 1, 1);
    uint32_t *stack = malloc(sizeof *stack * (c->size / 0x68 + 2));
    int r = seen && stack ? 0 : bad(c, "out of memory");
    for (uint32_t i = 0; i < worlds && !r; i++) {
        uint32_t base = be32(c, wh->off + 4 + 0x14 * i);
        size_t sp = 0;
        if ((base & 3) || !in(c, base, 0x68)) {
            r = bad(c, "world '%.16s' starts at 0x%X, not a node in the file", (const char *)c->b + wh->off + 8 + 0x14 * i,
                    base);
            break;
        }
        if (seen[base / 4])
            continue;
        seen[base / 4] = 1;
        stack[sp++] = base;
        while (sp && !r) {   /* the nodes NewWorld and the placement reach: next and child, from the world's base */
            uint32_t at = stack[--sp];
            if (wo && wo->count && (at < wo->off || (at - wo->off) % 0x68 || (at - wo->off) / 0x68 >= wo->count))
                r = bad(c, "a node of world %u (at 0x%X) is not one of the world nodes (WOBJ)", i, at);
            for (int k = 0; k < 2 && !r; k++) {
                int16_t link = (int16_t)be16(c, at + 0x44 + 2 * k);
                uint64_t to = base + 0x68ull * (uint16_t)link;
                if (link < 0)
                    continue;
                if (!in(c, to, 0x68))
                    r = bad(c, "a node of world %u (at 0x%X) links to node %d, outside the file", i, at, link);
                else if (!seen[to / 4]) {
                    seen[to / 4] = 1;
                    stack[sp++] = (uint32_t)to;
                }
            }
        }
    }
    free(seen);
    free(stack);
    if (r)
        return -1;
    for (uint32_t i = 0; gd && i < gd->count; i++) {   /* the quadtree race_setup_2 walks from node 0 */
        size_t g = gd->off + (size_t)i * 0x1C;
        if (c->b[g + 3] & 1) {
            for (int k = 0; k < 4; k++)
                if (be16(c, g + 0x14 + 2 * k) >= gd->count)
                    return bad(c, "quadtree node %u's child %u is past the %u nodes (GDAT)", i,
                               be16(c, g + 0x14 + 2 * k), gd->count);
        } else if ((uint32_t)be16(c, g + 0x14) + be16(c, g + 0x16) > (gt ? gt->count : 0))
            return bad(c, "quadtree leaf %u's targets run past the target list (GTLD)", i);
    }
    if (!cart || wad_open(cart, &cw) != 0)
        return 0;
    const Chunk *cwh = chunk(&cw, TAG_WHDR);
    if (!cwh || !in(cart, cwh->off, 4))
        return 0;
    for (uint32_t i = 0, n = be32(cart, cwh->off); i < n && in(cart, cwh->off + 8 + 0x14ull * i, 16); i++) {
        const char *name = (const char *)cart->b + cwh->off + 8 + 0x14 * i;
        int found = 0;
        for (uint32_t j = 0; j < worlds && !found; j++)
            found = !strncmp(name, (const char *)c->b + wh->off + 8 + 0x14 * j, 16);
        if (!found)
            return bad(c, "the cartridge's file has world '%.16s', which this one lacks (the game looks it up)", name);
    }
    return 0;
}

/* ---------------------------------------------------------------- text, fonts, roads, paths */

static const unsigned char k_text_offsets[6] = {1, 0, 1, 0, 0, 1};   /* sections of string offsets (text.c) */

static int text_sections(Ck *c, uint32_t n[6], size_t at[6], size_t *end)
{
    size_t pos = 0;
    for (int s = 0; s < 6; s++) {
        if (!in(c, pos, 4))
            return bad(c, "text section %d runs past the file's end", s);
        n[s] = be32(c, pos);
        at[s] = pos + 4;
        uint64_t bytes = k_text_offsets[s] ? 4ull * n[s] : 2ull * ((n[s] + 1ull) & ~1ull);
        if (!in(c, pos + 4, bytes))
            return bad(c, "text section %d's %u items run past the file's end", s, n[s]);
        pos += 4 + (size_t)bytes;
    }
    *end = pos;
    return 0;
}

/* A string's printf conversions in order (%d, %s, ...; %% is none): the game formats some strings with arguments, so a
 * string must keep the cartridge's, or the formatting reads its arguments as what they aren't. */
static void conversions(const char *s, char *out, size_t n)
{
    size_t k = 0;
    for (; *s && k + 1 < n; s++) {
        if (*s != '%')
            continue;
        const char *p = s + 1;
        while (*p && strchr("-+ #0123456789.*hlLqjzt", *p))
            p++;
        if (*p && strchr("diouxXcsfeEgGpn", *p))
            out[k++] = *p;
        s = *p == '%' ? p : s;
    }
    out[k] = 0;
}

static int check_text(Ck *c, Ck *cart)
{
    uint32_t n[6], cn[6];
    size_t at[6], end, cat[6], cend;
    if (text_sections(c, n, at, &end))
        return -1;
    for (int s = 0; s < 6; s++)
        for (uint32_t i = 0; k_text_offsets[s] && i < n[s]; i++) {
            uint32_t o = be32(c, at[s] + 4 * i);
            if (o < end || o >= c->size || !memchr(c->b + o, 0, c->size - o))
                return bad(c, "text section %d's string %u (at 0x%X) is not a string inside the file", s, i, o);
        }
    if (!cart || text_sections(cart, cn, cat, &cend) != 0)
        return 0;
    for (int s = 0; s < 6; s++) {
        if (n[s] < cn[s])
            return bad(c, "text section %d has %u items, the cartridge's %u: the game reads them by number", s, n[s],
                       cn[s]);
        for (uint32_t i = 0; k_text_offsets[s] && i < cn[s]; i++) {
            uint32_t o = be32(c, at[s] + 4 * i), co = be32(cart, cat[s] + 4 * i);
            char a[32], b[32];
            if (co >= cart->size || !memchr(cart->b + co, 0, cart->size - co))
                continue;
            conversions((const char *)c->b + o, a, sizeof a);
            conversions((const char *)cart->b + co, b, sizeof b);
            if (strcmp(a, b))
                return bad(c, "text section %d's string %u has the conversions '%s' where the cartridge's has '%s' "
                           "(the game formats it with those arguments)", s, i, a, b);
        }
    }
    return 0;
}

static int check_font(Ck *c, Ck *cart)
{
    if (c->size < 16)
        return bad(c, "it is %zu bytes: a font's header is 16", c->size);
    size_t chars = c->b[12], need = 16 + chars * 12 + (c->b[10] ? chars * chars : 0);
    if (need > c->size)
        return bad(c, "its %zu letters%s need 0x%zX bytes; the file has 0x%zX", chars, c->b[10] ? " and kerning" : "",
                   need, c->size);
    for (size_t i = 0; i < chars; i++)   /* _DrawText takes the letter's page from the font WAD's textures */
        if (c->b[16 + 12 * i + 5] >= c->b[11])
            return bad(c, "letter %zu is on texture %u; the font has %u", i, c->b[16 + 12 * i + 5], c->b[11]);
    if (cart && cart->size >= 16 && chars < cart->b[12])
        return bad(c, "it has %zu letters, the cartridge's font %u", chars, cart->b[12]);
    if (cart && cart->size >= 16 && c->b[11] > cart->b[11])
        return bad(c, "it has %u textures, the cartridge's font %u (the font WAD's)", c->b[11], cart->b[11]);
    return 0;
}

static int check_road(Ck *c)
{
    static const size_t rec[5] = {0x84, 0x14, 0x18, 8, 0x20};   /* path, tree, surf, svert, save (road.c) */
    size_t pos = 0x10, at[5];
    if (c->size < 0x10)
        return bad(c, "it is %zu bytes: a road's header is 16", c->size);
    for (int s = 0; s < 5; s++) {
        at[s] = pos;
        if (!in(c, pos, (uint64_t)be16(c, 2 * s) * rec[s]))
            return bad(c, "road section %d's %u records run past the file's end", s, be16(c, 2 * s));
        pos += be16(c, 2 * s) * rec[s];
    }
    uint32_t tnum = be16(c, 2), snum = be16(c, 4), vlnum = be16(c, 10), slnum = be32(c, 12);
    size_t vlist = pos, slist = pos + vlnum;
    if (!in(c, pos, (uint64_t)vlnum + slnum))
        return bad(c, "the road's vertex and surface lists (0x%X and 0x%X bytes) run past the file's end", vlnum, slnum);
    if (!tnum)
        return bad(c, "the road has no quadtree");
    for (uint32_t i = 0; i < tnum; i++) {   /* func_800AC9BC, and the readers of a leaf's list */
        size_t t = at[1] + 0x14 * (size_t)i;
        int16_t parent = (int16_t)be16(c, t);
        uint8_t mask = c->b[t + 3];
        if (parent < -1 || parent >= (int32_t)tnum)
            return bad(c, "road node %u's parent %d is not a node", i, parent);
        for (int q = 0; q < 4; q++) {
            uint32_t child = be16(c, t + 0xC + 2 * q);
            if (mask & (1 << q)) {
                if (child >= tnum)
                    return bad(c, "road node %u's quarter %d is node %u, past the %u nodes", i, q, child, tnum);
                continue;
            }
            if (mask & (16 << q))
                child |= 0x10000;
            if (!child)
                continue;
            if (child >= slnum)
                return bad(c, "road node %u's quarter %d lists its surfaces at 0x%X, past the surface list", i, q, child);
            uint32_t left = c->b[slist + child], p = child + 1;   /* func_800ADCE0's runs: exactly the count */
            if (left > ROAD_LIST_MAX)
                return bad(c, "road node %u's quarter %d lists %u surfaces; the game's readers take %d", i, q, left,
                           ROAD_LIST_MAX);
            while (left > 0) {
                if (p + 2 > slnum)
                    return bad(c, "road node %u's quarter %d's surface list runs past its end", i, q);
                uint32_t v = (uint32_t)c->b[slist + p] << 8 | c->b[slist + p + 1], run = (v >> 13) + 1;
                p += 2;
                if (run > left)
                    return bad(c, "road node %u's quarter %d's surface list has more surfaces than its count", i, q);
                if ((v & 0x1FFF) + run > snum)
                    return bad(c, "road node %u's quarter %d names surface %u, past the %u surfaces", i, q,
                               (v & 0x1FFF) + run - 1, snum);
                left -= run;
            }
        }
    }
    for (uint32_t i = 0; i < snum; i++)
        if (be16(c, at[2] + 0x18 * (size_t)i + 0x16) >= vlnum)
            return bad(c, "road surface %u's vertices start past the vertex list (0x%X bytes)", i, vlnum);
    (void)vlist;
    return 0;
}

/* path.c's walk, reading the counts big-endian; with counts set it fills them: the checkpoints, the resurrection
 * path's points, the alternative paths, and each drone path's points. */
typedef struct {
    int checkpoints;
    unsigned primary, alts, drone[4];
} PathCounts;

static int path_walk(Ck *c, PathCounts *pc)
{
    size_t pos = 0x32C, alt_at;
    if (!in(c, 0, 0x32C + 0x10))
        return bad(c, "it is %zu bytes: a path's checkpoints and header take 0x33C", c->size);
    pc->checkpoints = (int16_t)be16(c, 8);
    if (pc->checkpoints < 0 || pc->checkpoints > 10)
        return bad(c, "it has %d checkpoints (0-10)", pc->checkpoints);
    pc->primary = be16(c, pos);
    pc->alts = c->b[pos + 8];
    pos += 0x10;
    alt_at = pos;
    if (!in(c, pos, 0x10ull * pc->alts))
        return bad(c, "its %u alternative paths run past the file's end", pc->alts);
    pos += 0x10 * (size_t)pc->alts;
    if (!in(c, pos, 2 + 6ull * pc->primary))
        return bad(c, "its resurrection path's %u points run past the file's end", pc->primary);
    pos += 2 + 6 * (size_t)pc->primary;
    for (unsigned i = 0; i < pc->alts; i++) {
        unsigned n = be16(c, alt_at + 0x10 * i + 0xA);
        if (!in(c, pos, 6ull * n))
            return bad(c, "alternative path %u's %u points run past the file's end", i, n);
        pos += 6 * (size_t)n;
    }
    for (int p = 0; p < 4; p++) {
        if (!in(c, pos, 8))
            return bad(c, "drone path %d runs past the file's end", p);
        pc->drone[p] = be16(c, pos);
        if (!in(c, pos + 8, 8ull * pc->drone[p]))
            return bad(c, "drone path %d's %u points run past the file's end", p, pc->drone[p]);
        pos += 8 + 8 * (size_t)pc->drone[p];
    }
    return 0;
}

static int check_path(Ck *c, Ck *cart)
{
    PathCounts n, cn;
    if (path_walk(c, &n))
        return -1;
    if (!cart)
        return 0;
    char scratch[8];
    Ck q = *cart;
    q.why = scratch, q.why_size = sizeof scratch;
    if (path_walk(&q, &cn))
        return 0;
    if (cn.primary && !n.primary)
        return bad(c, "its resurrection path is empty; the cartridge's has %u points", cn.primary);
    for (int p = 0; p < 4; p++)
        if (cn.drone[p] && !n.drone[p])
            return bad(c, "drone path %d is empty; the cartridge's has %u points", p, cn.drone[p]);
    if (cn.checkpoints && !n.checkpoints)
        return bad(c, "it has no checkpoints; the cartridge's has %d", cn.checkpoints);
    return 0;
}

/* ---------------------------------------------------------------- the entry */

int pc_assets_check(int wave, const uint8_t *file, size_t size, const uint8_t *cart, size_t cart_size, char *why,
                    size_t why_size)
{
    Ck c = {file, size, why, why_size}, k = {cart, cart_size, NULL, 0}, *kp = cart ? &k : NULL;
    Wad w;
    if (why && why_size)
        why[0] = 0;
    if (wave < 0 || wave > PC_WAVE_PATH_LAST)
        return bad(&c, "there is no wave %d (0-%d)", wave, PC_WAVE_PATH_LAST);
    if (!size)
        return bad(&c, "it is empty");
    if (wave == WAVE_SAMPLES)   /* sample data: ADPCM and PCM bytes, read through the sample directory */
        return 0;
    if (wave >= PC_WAVE_MUSYX_FIRST && wave <= PC_WAVE_MUSYX_LAST) {
        int errors = pc_assets_musyx_check(wave, file, size);
        if (errors < 0)
            return bad(&c, "it is all zero, or too short for a sound file");
        return errors ? bad(&c, "%d of its sound records don't parse (port/assets/musyx.c's walk)", errors) : 0;
    }
    if (wave <= PC_WAVE_TEXT_LAST)
        return check_text(&c, kp);
    if (wave <= PC_WAVE_FONTWAD_LAST)
        return check_wad(&c, kp, 0, &w);
    if (wave <= PC_WAVE_FONT_LAST)
        return check_font(&c, kp);
    if (wave <= PC_WAVE_WAD_LAST)
        return check_wad(&c, kp, wave >= 101, &w);
    if (wave <= PC_WAVE_WORLD_LAST)
        return check_world(&c, kp);
    if (wave <= PC_WAVE_ROAD_LAST)
        return check_road(&c);
    return check_path(&c, kp);
}
