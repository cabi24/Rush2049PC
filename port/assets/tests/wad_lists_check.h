/* port/assets/tests/wad_lists_check.h: the WAD converter's native display lists (port/assets/wad.c, docs/port/NATIVE.md
 * step 10) against the N64's, for test_wad.c (a synthetic WAD) and rush2049pc_assetcheck (every WAD of your ROM).
 *
 * The reference is what the N64 did, written here again apart from the converter: SetupWaveModel's relocation of a
 * copy of the file at a made-up RDRAM address (relocate_texture_loads over TXLD by IMAG's address, then
 * prepare_model_dl's walk of each model's levels by the file's, the 24-bit rule, every LOD marker's section kept),
 * and the RCP's reading of a relocated word: the RDRAM at its 24 bits. Then each list is walked from its record (a
 * level of detail's, a listed texture's) in both, command for command, as far as any walk goes (to G_ENDDL, into the
 * lists G_DL, G_RDPHALF_1 before G_BRANCH_Z and a walked LOD marker name):
 *   - the record is the file's command as pc_gfx_from_n64 reads it (port/assets/gfx_n64.c), but for its pointer;
 *   - a command the RCP reads an address from points where the N64's relocated word did: a list there, the same
 *     command the walk of the file comes to (a G_BRANCH_Z's record holds the list its G_RDPHALF_1 named); a VTX's
 *     vertices, the file's in the host's byte order; a MTX's matrix, and a MOVEMEM's matrix or viewport, a copy of the
 *     file's in the host's byte order (16 words, 8 s16); a light or an image, the file's byte there; and NULL where
 *     the N64's word pointed outside the file;
 *   - a command of the file the walks reach twice is one native command.
 * Stdlib only; no N64 address leaves this file. */
#ifndef WAD_LISTS_CHECK_H
#define WAD_LISTS_CHECK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_game_types.h"
#include "pc_gfx_n64.h"
#include "pc_port.h"

#define WLC_PHYS 0x400000u              /* the made-up RDRAM address of the copy: 4 MiB in, so a word relocated twice
                                         * (or not at all) points outside any file the cartridge has */
#define WLC_K0(p) (0x80000000u | (p))
#define WLC_MAX_STEPS (1u << 24)

static uint32_t wlc_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static void wlc_put32(uint8_t *p, uint32_t v) { p[0] = v >> 24, p[1] = v >> 16 & 0xFF, p[2] = v >> 8 & 0xFF, p[3] = v & 0xFF; }

static int wlc_no_address(unsigned op)
{
    return (op & 0xC0) == 0x40 || (op & 0xC0) == 0x80 || (op >= 9 && op < 0x40) || (op >= 0xC0 && op < 0xD6);
}

/* prepare_model_dl's and relocate_texture_loads's rule at game.memory original: w1 + base in 24 bits, w1's segment. */
static uint32_t wlc_reloc(uint32_t w1, uint32_t base)
{
    return ((w1 + base) & 0xFFFFFFu) | (w1 & 0x0F000000u);
}

typedef struct {
    const uint8_t *file;        /* the file in its heap block, which the native lists point into */
    const uint8_t *orig;        /* its bytes as the cartridge has them (before the converter swapped its paths' points) */
    uint8_t *n64;               /* the copy, relocated */
    size_t size;
    uint8_t *marker;            /* a LOD marker prepare_model_dl's walk came to */
    const Gfx **native;  /* the native command each of the file's commands the check came to is */
    struct { uint32_t off; const Gfx *nat; } *todo;
    size_t ntodo, room;
    unsigned bad;
    const char *what;
} WlcCheck;

static void wlc_bad(WlcCheck *c, uint32_t off, const char *fmt, uint32_t a)
{
    if (c->bad++ < 10) {
        printf("FAIL: %s: the native list's command for the file's at 0x%X: ", c->what, off);
        printf(fmt, a);
        putchar('\n');
    }
}

/* The file offset the RCP read for a relocated word, or ~0 outside the file. */
static uint32_t wlc_target(const WlcCheck *c, uint32_t word)
{
    uint32_t phys = word & 0xFFFFFFu;
    return phys >= WLC_PHYS && phys - WLC_PHYS < c->size ? phys - WLC_PHYS : ~0u;
}

static void wlc_push(WlcCheck *c, uint32_t off, const Gfx *nat)
{
    if (c->ntodo == c->room) {
        c->room = c->room ? 2 * c->room : 64;
        c->todo = realloc(c->todo, c->room * sizeof *c->todo);
        if (!c->todo)
            abort();
    }
    c->todo[c->ntodo].off = off;
    c->todo[c->ntodo++].nat = nat;
}

/* The N64's relocation of the copy: SetupWaveModel's. */
static void wlc_relocate(WlcCheck *c, const struct pc_wad_records *wr, int64_t txld, uint32_t txld_bytes, int64_t imag,
                         const uint32_t *levels, int nlevels)
{
    uint8_t *b = c->n64;
    uint32_t steps = 0;

    if (txld >= 0)
        for (uint32_t g = (uint32_t)txld; g < (uint32_t)txld + txld_bytes && g + 8 <= c->size; g += 8) {
            unsigned op = b[g], next = g + 16 <= c->size ? b[g + 8] : 0;
            if (wlc_no_address(op))
                continue;
            if (op == 0xFD || (op == 0xE1 && next == 0x04))
                wlc_put32(b + g + 4, wlc_reloc(wlc_be32(b + g + 4), imag >= 0 ? WLC_K0(WLC_PHYS + (uint32_t)imag) : 0));
        }
    for (int l = 0; l < nlevels; l++) {
        uint32_t off = levels[l], saved = 0;
        while (off + 8 <= c->size && steps++ < WLC_MAX_STEPS) {
            unsigned op = b[off], next = off + 16 <= c->size ? b[off + 8] : 0;
            if (wlc_no_address(op)) {
                off += 8;
                continue;
            }
            if (op == 0x01 || op == 0xDD || op == 0xDE || op == 0xDA || op == 0xDC ||
                (op == 0xE1 && (next == 0x04 || next == 0xDD)) || op >= 0xFD)
                wlc_put32(b + off + 4, wlc_reloc(wlc_be32(b + off + 4), WLC_K0(WLC_PHYS)));
            if (op == 0xE0 && b[off + 1] == 1)
                c->marker[off / 8] = 1;
            if (op == 0xE1)
                saved = wlc_be32(b + off + 4);
            else if (op == 0x04) {
                uint32_t t = wlc_target(c, saved);
                if (t == ~0u || (t & 7))
                    break;   /* on the N64 the walk went on outside the file: the converter says so */
                off = t + 8;
                continue;
            } else if (op == 0xDF)
                break;
            off += 8;
        }
    }
    (void)wr;
}

/* The record's pointer, where its op has one (NULL else), and the record with p in its place. */
static void *wlc_pointer(const Gfx *g)
{
    switch (g->op) {
    case GFX_OP_VERTICES: return g->vertices.vertices;
    case GFX_OP_CALL_LIST:
    case GFX_OP_BRANCH_LIST: return g->list.list;
    case GFX_OP_SECTION: return g->section.end;
    case GFX_OP_BRANCH_Z: return g->branch_z.list;
    case GFX_OP_MATRIX:
    case GFX_OP_FORCE_MATRIX: return g->matrix.matrix;
    case GFX_OP_VIEWPORT: return g->viewport.viewport;
    case GFX_OP_LIGHT: return g->light.light;
    case GFX_OP_TEXTURE_IMAGE:
    case GFX_OP_DEPTH_IMAGE:
    case GFX_OP_COLOR_IMAGE: return g->image.image;
    default: return NULL;
    }
}

static void wlc_set_pointer(Gfx *g, void *p)
{
    switch (g->op) {
    case GFX_OP_VERTICES: g->vertices.vertices = p; break;
    case GFX_OP_CALL_LIST:
    case GFX_OP_BRANCH_LIST: g->list.list = p; break;
    case GFX_OP_SECTION: g->section.end = p; break;
    case GFX_OP_BRANCH_Z: g->branch_z.list = p; break;
    case GFX_OP_MATRIX:
    case GFX_OP_FORCE_MATRIX: g->matrix.matrix = p; break;
    case GFX_OP_VIEWPORT: g->viewport.viewport = p; break;
    case GFX_OP_LIGHT: g->light.light = p; break;
    case GFX_OP_TEXTURE_IMAGE:
    case GFX_OP_DEPTH_IMAGE:
    case GFX_OP_COLOR_IMAGE: g->image.image = p; break;
    default: break;
    }
}

/* The command at off in the cartridge's bytes (w1 the file's word, before any relocation). */
static GfxWords wlc_words(const WlcCheck *c, uint32_t off)
{
    GfxWords w = {wlc_be32(c->orig + off), wlc_be32(c->orig + off + 4)};
    return w;
}

/* A list the record names, which the N64's word `to` (a file offset, ~0u outside it) named: none where that was no
 * command's place in the file (outside it, or not 8-aligned); else the walk goes on there too. */
static void wlc_list(WlcCheck *c, uint32_t off, uint32_t to, uint32_t word, const Gfx *list)
{
    if ((to == ~0u || (to & 7)) && list != NULL)
        wlc_bad(c, off, "the N64's list was no place in the file (0x%X), the native one's is", word);
    else if (to != ~0u && !(to & 7) && list == NULL)
        wlc_bad(c, off, "no list, the N64's at 0x%X", to);
    else if (to != ~0u && !(to & 7))
        wlc_push(c, to, list);
}

/* Walks the lists from the todo list in the copy and in the native lists together. */
static void wlc_walk(WlcCheck *c)
{
    while (c->ntodo) {
        uint32_t off = c->todo[--c->ntodo].off;
        const Gfx *nat = c->todo[c->ntodo].nat;
        if (nat == NULL) {
            wlc_bad(c, off, "a list the file has (at 0x%X) has none", off);
            continue;
        }
        for (;;) {
            if (off + 8 > c->size)
                break;   /* the converter says so */
            if (c->native[off / 8]) {
                if (c->native[off / 8] != nat)
                    wlc_bad(c, off, "the file's command is two native ones (0x%X)", off);
                break;
            }
            c->native[off / 8] = nat;
            uint32_t w0 = wlc_be32(c->n64 + off), w1 = wlc_be32(c->n64 + off + 4), raw = wlc_be32(c->orig + off + 4);
            unsigned op = w0 >> 24, prev = off >= 8 ? c->n64[off - 8] : 0;
            uint32_t t = wlc_target(c, w1);
            void *p = wlc_pointer(nat);
            GfxWords before = {0, 0}, here = wlc_words(c, off), after = {0, 0};
            Gfx want;

            /* The record: the file's command read into one (its neighbours beside it), its pointer checked below. */
            if (off >= 8)
                before = wlc_words(c, off - 8);
            if (off + 16 <= c->size)
                after = wlc_words(c, off + 8);
            pc_gfx_from_n64(&want, off >= 8 ? &before : NULL, &here, off + 16 <= c->size ? &after : NULL);
            wlc_set_pointer(&want, p);
            if (memcmp(&want, nat, sizeof want) != 0)
                wlc_bad(c, off, "the record (op %u) is not the file's command", nat->op);

            if (op == 0xDE || (op == 0xE0 && (w0 >> 16 & 0xFF) == 1 && c->marker[off / 8])) {
                /* a list (a marker's: where its section ends, the file's offset as it is) */
                uint32_t to = op == 0xE0 ? ((raw & 0xFFFFFFu) < c->size ? raw & 0xFFFFFFu : ~0u) : t;
                wlc_list(c, off, to, w1, p);
            } else if (op == 0x04) {
                /* a G_BRANCH_Z: the list its G_RDPHALF_1 named (that half's record is a no-op) */
                uint32_t half = prev == 0xE1 ? wlc_be32(c->n64 + off - 4) : 0;
                if (prev == 0xE1)
                    wlc_list(c, off, wlc_target(c, half), half, p);
                else if (p != NULL)
                    wlc_bad(c, off, "a G_BRANCH_Z without its G_RDPHALF_1 names a list (0x%X)", 0);
            } else if (op == 0x01) {
                uint32_t count = w0 >> 12 & 0xFF;
                const uint8_t *v = p;
                if (t == ~0u || (uint64_t)t + 16ull * count > c->size) {
                    if (t == ~0u && v != NULL)
                        wlc_bad(c, off, "VTX: the N64's vertices were outside the file (0x%X), the native ones aren't", w1);
                } else if (v == NULL)
                    wlc_bad(c, off, "VTX: no vertices, the N64's at 0x%X", t);
                else
                    for (uint32_t k = 0; k < 16 * count; k += 2) {
                        const uint8_t *f = c->orig + t + k;
                        uint16_t want16 = (k & 15) < 12 ? (uint16_t)(f[0] << 8 | f[1]) : 0, have;
                        memcpy(&have, v + k, 2);
                        /* x, y, z, flag, s, t in the host's byte order; the colour's bytes as they are */
                        if ((k & 15) < 12 ? have != want16 : memcmp(v + k, f, 2) != 0) {
                            wlc_bad(c, off, "VTX: its vertex bytes at +0x%X are not the file's", k);
                            break;
                        }
                    }
            } else if (op == 0xDA || (op == 0xDC && ((w0 & 0xFF) == 8 || (w0 & 0xFF) == 14))) {
                /* a matrix (16 words) or a viewport (8 s16), copied in the host's byte order */
                unsigned unit = op == 0xDC && (w0 & 0xFF) == 8 ? 2 : 4, len = unit == 2 ? 16 : 64;
                const uint8_t *m = p;
                if (t == ~0u ? m != NULL : m == NULL)
                    wlc_bad(c, off, "its copy is not where the N64's word pointed (0x%X)", t);
                for (unsigned k = 0; m && t != ~0u && k < len && t + k + unit <= c->size; k += unit) {
                    const uint8_t *f = c->orig + t + k;
                    uint32_t want32 = unit == 2 ? (uint32_t)(f[0] << 8 | f[1]) : wlc_be32(f), have = 0;
                    uint16_t h16;
                    if (unit == 2)
                        memcpy(&h16, m + k, 2), have = h16;
                    else
                        memcpy(&have, m + k, 4);
                    if (have != want32) {
                        wlc_bad(c, off, "its copy's bytes at +0x%X are not the file's", k);
                        break;
                    }
                }
            } else if ((op == 0xDC && (w0 & 0xFF) == 10) || op >= 0xFD) {
                /* a light, an image: the file's place the N64's word pointed at */
                if (t == ~0u ? p != NULL : p != (const void *)(c->file + t))
                    wlc_bad(c, off, "its address is not where the N64's word pointed (0x%X)", t);
            }
            if (op == 0xDF)
                break;
            off += 8;
            nat++;
        }
    }
}
/* The check of the WAD file in its heap block (size bytes; orig: its bytes as the cartridge has them, before the
 * converter, or file itself when the converter changed nothing the check reads: it swaps only the paths' points)
 * whose records are wr. Returns the mismatches (each of the first ten said, what naming the file). */
static unsigned wad_lists_check(const uint8_t *file, const uint8_t *orig, size_t size, const struct pc_wad_records *wr,
                                const char *what)
{
    WlcCheck c;
    uint32_t toc = wlc_be32(orig), n = wlc_be32(orig + 4), *levels;
    int64_t txld = -1, imag = -1, obhd = -1, txhd = -1;
    uint32_t txld_bytes = 0, nobhd = 0, ntxhd = 0;
    int nlevels = 0;

    memset(&c, 0, sizeof c);
    c.file = file, c.orig = orig, c.size = size, c.what = what;
    if (size < 8 || toc < 8 || toc > size || n > (size - toc) / 12)
        return 0;
    for (uint32_t i = n; i-- > 0;) {   /* the first of each tag, as the converter takes it */
        uint32_t tag = wlc_be32(orig + toc + 12 * i), off = wlc_be32(orig + toc + 12 * i + 4);
        uint32_t cnt = wlc_be32(orig + toc + 12 * i + 8);
        if (tag == 0x54584C44)
            txld = off, txld_bytes = cnt;
        else if (tag == 0x494D4147)
            imag = off;
        else if (tag == 0x4F424844)
            obhd = off, nobhd = cnt;
        else if (tag == 0x54584844)
            txhd = off, ntxhd = cnt;
    }
    c.n64 = malloc(size);
    c.marker = calloc(size / 8 + 1, 1);
    c.native = calloc(size / 8 + 1, sizeof *c.native);
    levels = malloc(4 * sizeof *levels * ((size_t)wr->numModels + 1));
    if (!c.n64 || !c.marker || !c.native || !levels)
        abort();
    memcpy(c.n64, orig, size);
    for (int i = 0; obhd >= 0 && i < wr->numModels && (uint32_t)i < nobhd; i++) {
        const uint8_t *r = orig + obhd + 0x58 * (size_t)i;
        for (int k = 0; k < wr->models[i].lodCount; k++) {
            uint32_t dl = wlc_be32(r + 0x18 + 0x10 * k + 8);
            if (dl < size && !(dl & 7)) {
                levels[nlevels++] = dl;
                wlc_push(&c, dl, (const Gfx *)(const void *)wr->models[i].lod[k].displayList);
            }
        }
    }
    wlc_relocate(&c, wr, txld, txld_bytes, imag, levels, nlevels);
    for (int i = 0; txhd >= 0 && imag >= 0 && i < wr->numTextures && (uint32_t)i < ntxhd; i++) {
        const uint8_t *r = orig + txhd + 0x24 * (size_t)i;
        uint32_t list = (uint32_t)imag + wlc_be32(r + 0x18);
        if (!(wlc_be32(r + 0x1C) & 0x08000000u) && list < size && !(list & 7))
            wlc_push(&c, list, (const Gfx *)wr->textures[i].texBase);
    }
    wlc_walk(&c);
    free(levels);
    free(c.n64);
    free(c.marker);
    free(c.native);
    free(c.todo);
    return c.bad;
}

#endif
