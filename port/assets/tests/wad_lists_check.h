/* port/assets/tests/wad_lists_check.h: the WAD converter's native display lists (port/assets/wad.c, docs/port/NATIVE.md
 * step 10) against the N64's, for test_wad.c (a synthetic WAD) and rush2049pc_assetcheck (every WAD of your ROM).
 *
 * The reference is what the N64 did, written here again apart from the converter: SetupWaveModel's relocation of a
 * copy of the file at a made-up RDRAM address (relocate_texture_loads over TXLD by IMAG's address, then
 * relocate_model_dl's walk of each model's levels by the file's, the 24-bit rule, every LOD marker's section kept),
 * and the RCP's reading of a relocated word: the RDRAM at its 24 bits. Then each list is walked from its record (a
 * level of detail's, a listed texture's) in both, command for command, as far as any walk goes (to G_ENDDL, into the
 * lists G_DL, G_RDPHALF_1 before G_BRANCH_Z and a walked LOD marker name):
 *   - w0 is the file's;
 *   - a command the RCP reads an address from points where the N64's relocated word did: a list there, the same
 *     command the walk of the file comes to; a VTX's vertices, the file's in the host's byte order; anything else,
 *     the file's byte there; and NULL where the N64's word pointed outside the file;
 *   - every other w1 is the file's word;
 *   - a command of the file the walks reach twice is one native command.
 * Stdlib only; no N64 address leaves this file. */
#ifndef WAD_LISTS_CHECK_H
#define WAD_LISTS_CHECK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_game_types.h"
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

/* relocate_model_dl's and relocate_texture_loads's rule at game.memory original: w1 + base in 24 bits, w1's segment. */
static uint32_t wlc_reloc(uint32_t w1, uint32_t base)
{
    return ((w1 + base) & 0xFFFFFFu) | (w1 & 0x0F000000u);
}

typedef struct {
    const uint8_t *file;        /* the file in its heap block, which the native lists point into */
    const uint8_t *orig;        /* its bytes as the cartridge has them (before the converter swapped its paths' points) */
    uint8_t *n64;               /* the copy, relocated */
    size_t size;
    uint8_t *marker;            /* a LOD marker relocate_model_dl's walk came to */
    const PcGfxWords **native;  /* the native command each of the file's commands the check came to is */
    struct { uint32_t off; const PcGfxWords *nat; } *todo;
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

static void wlc_push(WlcCheck *c, uint32_t off, const PcGfxWords *nat)
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

/* Walks the lists from the todo list in the copy and in the native lists together. */
static void wlc_walk(WlcCheck *c)
{
    while (c->ntodo) {
        uint32_t off = c->todo[--c->ntodo].off;
        const PcGfxWords *nat = c->todo[c->ntodo].nat;
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
            unsigned op = w0 >> 24, next = off + 16 <= c->size ? c->n64[off + 8] : 0;
            uint32_t t = wlc_target(c, w1);
            if (nat->w0 != w0)
                wlc_bad(c, off, "w0 0x%08X is not the file's", nat->w0);
            if (op == 0xDE || (op == 0xE1 && next == 0x04) || (op == 0xE0 && (w0 >> 16 & 0xFF) == 1 && c->marker[off / 8])) {
                /* a list (a marker's: where its section ends, the file's offset as it is); none where the N64's word
                 * named no command's place in the file (outside it, or not 8-aligned) */
                uint32_t to = op == 0xE0 ? ((raw & 0xFFFFFFu) < c->size ? raw & 0xFFFFFFu : ~0u) : t;
                if ((to == ~0u || (to & 7)) && nat->w1 != 0)
                    wlc_bad(c, off, "the N64's list was no place in the file (0x%X), the native one's is", w1);
                else if (to != ~0u && !(to & 7) && nat->w1 == 0)
                    wlc_bad(c, off, "no list, the N64's at 0x%X", to);
                else if (to != ~0u && !(to & 7))
                    wlc_push(c, to, (const PcGfxWords *)nat->w1);
            } else if (op == 0x01) {
                uint32_t count = w0 >> 12 & 0xFF;
                const uint8_t *v = (const uint8_t *)nat->w1;
                if (t == ~0u || (uint64_t)t + 16ull * count > c->size) {
                    if (t == ~0u && v != NULL)
                        wlc_bad(c, off, "VTX: the N64's vertices were outside the file (0x%X), the native ones aren't", w1);
                } else if (v == NULL)
                    wlc_bad(c, off, "VTX: no vertices, the N64's at 0x%X", t);
                else
                    for (uint32_t k = 0; k < 16 * count; k += 2) {
                        const uint8_t *f = c->orig + t + k;
                        uint16_t want = (k & 15) < 12 ? (uint16_t)(f[0] << 8 | f[1]) : 0, have;
                        memcpy(&have, v + k, 2);
                        /* x, y, z, flag, s, t in the host's byte order; the colour's bytes as they are */
                        if ((k & 15) < 12 ? have != want : memcmp(v + k, f, 2) != 0) {
                            wlc_bad(c, off, "VTX: its vertex bytes at +0x%X are not the file's", k);
                            break;
                        }
                    }
            } else if (op == 0xDA || op == 0xDC || op == 0xDD || (op == 0xE1 && next == 0xDD) || op >= 0xFD) {
                if (t == ~0u ? nat->w1 != 0 : nat->w1 != (uintptr_t)(c->file + t))
                    wlc_bad(c, off, "its address is not where the N64's word pointed (0x%X)", t);
            } else if (nat->w1 != (uintptr_t)raw)
                wlc_bad(c, off, "w1 is not the file's 0x%08X", raw);
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
                wlc_push(&c, dl, (const PcGfxWords *)(const void *)wr->models[i].lod[k].displayList);
            }
        }
    }
    wlc_relocate(&c, wr, txld, txld_bytes, imag, levels, nlevels);
    for (int i = 0; txhd >= 0 && imag >= 0 && i < wr->numTextures && (uint32_t)i < ntxhd; i++) {
        const uint8_t *r = orig + txhd + 0x24 * (size_t)i;
        uint32_t list = (uint32_t)imag + wlc_be32(r + 0x18);
        if (!(wlc_be32(r + 0x1C) & 0x08000000u) && list < size && !(list & 7))
            wlc_push(&c, list, (const PcGfxWords *)wr->textures[i].texBase);
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
