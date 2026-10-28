/* port/assets/world.c: the world WADs, waves 120..138 (docs/port/ASSETS.md §3.4).
 *
 * A world WAD is a WAD (pc_assets_wad runs first and handles the container) with four chunks of its own:
 *   WHDR  u32 count; count x {u32 woff; char name[16]} (0x14), sorted by name. LoadTrack adds the file's address
 *         to woff; NewWorld (wheel_torque_apply) bsearches the names.
 *   WOBJ  0x68-byte world nodes: char name[16]; f32 mat[12] @0x10; u32 dlflags @0x40; s16 next, child, sfx1,
 *         sfx2 @0x44; s32 tidx @0x4C; f32 min[3], max[3] @0x50. next and child index from the world's first node.
 *   GDAT  0x1C-byte quadtree nodes: s16 parent; u8, u8 flags; f32 minh, maxh, minv, maxv; u16 child[4].
 *   GTLD  u32 target keys, which race setup (engine_sound_update, groups/level_objects) replaces in place by
 *         object pointers.
 * pc_assets_world swaps all four in place. A host object pointer doesn't fit a GTLD slot, so LoadTrack's PC view
 * (groups/loadtrack/loadtrack.c) gets GTLD as a native array of the keys from pc_world_chunk instead (§4 #14).
 *
 * The chunks are found through the WAD's table of contents (file +0: TOC offset, +4: entry count; entries
 * {u32 tag, u32 offset, u32 count}). pc_assets_wad may already have swapped it to host order, or not: each word
 * is read in whichever order makes it valid (a tag matches in exactly one order). The TOC's third word is a
 * byte length for WHDR (unused: the chunk has its own count) and a record count for the others: LoadTrack hands
 * GTLD's to race setup as the number of keys (D_801497EC), and the retail worlds' WOBJ and GDAT counts are
 * record counts too (ASSETS.md §8). Neither of those two is read by the game, which reaches the nodes from WHDR
 * and the quadtree from its node 0; so when their count doesn't fit the file, both are converted by walking them
 * as the game does instead. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

#define TAG_WHDR 0x57484452u
#define TAG_WOBJ 0x574F424Au
#define TAG_GDAT 0x47444154u
#define TAG_GTLD 0x47544C44u

#define WOBJ_SIZE 0x68
#define GDAT_SIZE 0x1C

static uint32_t rd32(const uint8_t *p, int be)
{
    uint32_t v;
    if (be)
        return pc_be32(p);
    memcpy(&v, p, 4);
    return v;
}

/* The chunk tagged tag: its file offset and the TOC's count word. 0 if the TOC or the entry is not valid. */
static int find_chunk(const uint8_t *buf, size_t size, uint32_t tag, size_t *off, uint32_t *count)
{
    int hdr_be;

    if (size < 8)
        return 0;
    for (hdr_be = 1; hdr_be >= 0; hdr_be--) {
        uint32_t toc = rd32(buf, hdr_be), n = rd32(buf + 4, hdr_be), i;

        if (toc < 8 || toc > size || n > (size - toc) / 12)
            continue;
        for (i = 0; i < n; i++) {
            const uint8_t *e = buf + toc + 12 * i;
            int be;

            for (be = 1; be >= 0; be--) {
                uint32_t o;
                if (rd32(e, be) != tag)
                    continue;
                o = rd32(e + 4, be);
                if (o > size)
                    return 0;
                *off = o;
                *count = rd32(e + 8, be);
                return 1;
            }
        }
    }
    return 0;
}

/* Records of rec bytes in a chunk whose count word is a record count; 0 if they don't fit the block. */
static size_t chunk_records(int wave, const char *what, size_t off, uint32_t count, size_t rec, size_t size)
{
    if (count <= (size - off) / rec)
        return count;
    fprintf(stderr, "rush2049pc: world wave %d: %s's %u records run past the 0x%zx-byte block\n", wave, what, count,
            size);
    return 0;
}

static void swap_node(uint8_t *n)
{
    pc_swap32_n(n + 0x10, 12);   /* mat[12] */
    pc_swap32_at(n + 0x40);      /* dlflags */
    pc_swap16_n(n + 0x44, 4);    /* next, child, sfx1, sfx2 */
    pc_swap32_n(n + 0x4C, 7);    /* tidx, min[3], max[3] */
}

/* Without a WOBJ entry that fits: swap the nodes each world reaches from its first node through next and child,
 * as NewWorld and differential_output do, once each. */
static void swap_reached_nodes(uint8_t *buf, size_t size, const uint8_t *whdr, uint32_t worlds)
{
    size_t cap = size / WOBJ_SIZE + 1;
    uint8_t *done = (uint8_t *)calloc(size / 4 / 8 + 1, 1);
    size_t *stack = (size_t *)malloc(sizeof *stack * cap);
    uint32_t w;

#define NODE_SEEN(at) (done[(at) / 32] & (1 << ((at) / 4 % 8)))
#define NODE_MARK(at) (done[(at) / 32] |= (uint8_t)(1 << ((at) / 4 % 8)))
    if (!done || !stack)
        abort();
    for (w = 0; w < worlds; w++) {
        uint32_t base;
        size_t sp = 0;

        memcpy(&base, whdr + 4 + 0x14 * w, 4);
        if (base > size || (base & 3) || size - base < WOBJ_SIZE || NODE_SEEN(base))
            continue;
        NODE_MARK(base);   /* a node is marked when it is pushed, so each is pushed once: at most cap */
        stack[sp++] = base;
        while (sp) {
            size_t at = stack[--sp];
            int16_t link[2];
            int k;

            swap_node(buf + at);
            memcpy(&link[0], buf + at + 0x44, 2);   /* next */
            memcpy(&link[1], buf + at + 0x46, 2);   /* child */
            for (k = 0; k < 2; k++) {
                size_t to = base + (size_t)WOBJ_SIZE * (uint16_t)link[k];
                if (link[k] >= 0 && to + WOBJ_SIZE <= size && !NODE_SEEN(to)) {
                    NODE_MARK(to);
                    stack[sp++] = to;
                }
            }
        }
    }
#undef NODE_SEEN
#undef NODE_MARK
    free(stack);
    free(done);
}

static void swap_gdat(uint8_t *g)
{
    pc_swap16_at(g);             /* parent; then two bytes */
    pc_swap32_n(g + 4, 4);       /* minh, maxh, minv, maxv */
    pc_swap16_n(g + 0x14, 4);    /* child[4] (leaves: count, first, 0) */
}

/* Without a GDAT count that fits: the quadtree race_setup_2 walks, from node 0; a node with flags bit 0 has four
 * children (GDAT indices), the others are leaves. */
static void swap_gdat_tree(uint8_t *buf, size_t size, size_t off)
{
    size_t n = (size - off) / GDAT_SIZE, sp = 0;
    uint8_t *done = (uint8_t *)calloc(n + 1, 1);
    uint32_t *stack = (uint32_t *)malloc(sizeof *stack * (n + 1));

    if (!done || !stack)
        abort();
    if (n) {
        done[0] = 1;
        stack[sp++] = 0;
    }
    while (sp) {
        uint8_t *g = buf + off + (size_t)GDAT_SIZE * stack[--sp];
        int k;

        swap_gdat(g);
        if (!(g[3] & 1))
            continue;
        for (k = 0; k < 4; k++) {
            uint16_t c;
            memcpy(&c, g + 0x14 + 2 * k, 2);
            if (c < n && !done[c]) {
                done[c] = 1;
                stack[sp++] = c;
            }
        }
    }
    free(stack);
    free(done);
}

void pc_assets_world(int wave, uint8_t *buf, size_t size)
{
    size_t off, i, n;
    uint32_t count, worlds = 0;
    const uint8_t *whdr = NULL;

    if (find_chunk(buf, size, TAG_WHDR, &off, &count) && size - off >= 4
        && pc_be32(buf + off) <= (size - off - 4) / 0x14) {
        worlds = pc_be32(buf + off);
        pc_swap32_at(buf + off);
        for (i = 0; i < worlds; i++)
            pc_swap32_at(buf + off + 4 + 0x14 * i);
        whdr = buf + off;
    } else
        fprintf(stderr, "rush2049pc: world wave %d: no WHDR that fits\n", wave);

    n = 0;
    if (find_chunk(buf, size, TAG_WOBJ, &off, &count) && (n = chunk_records(wave, "WOBJ", off, count, WOBJ_SIZE,
                                                                             size)) != 0)
        for (i = 0; i < n; i++)
            swap_node(buf + off + WOBJ_SIZE * i);
    else if (whdr)
        swap_reached_nodes(buf, size, whdr, worlds);

    if (find_chunk(buf, size, TAG_GDAT, &off, &count)) {
        n = chunk_records(wave, "GDAT", off, count, GDAT_SIZE, size);
        for (i = 0; i < n; i++)
            swap_gdat(buf + off + GDAT_SIZE * i);
        if (!n)
            swap_gdat_tree(buf, size, off);
    }

    if (find_chunk(buf, size, TAG_GTLD, &off, &count))
        pc_swap32_n(buf + off, chunk_records(wave, "GTLD", off, count, 4, size));
}

/* LoadTrack's MBGetFromWad on PC: the chunk's window address for every tag but GTLD, for which it is a native
 * array of the keys, each sign-extended as engine_sound_update compares it ((void *)node->key with an s32 key).
 * One array, reused by the next track. */
uintptr_t pc_world_chunk(uint32_t addr, int32_t tag, int32_t *count)
{
    static void **keys;
    static size_t cap;
    const uint32_t *src = (const uint32_t *)(uintptr_t)addr;
    int32_t n = count ? *count : 0, i;

    if ((uint32_t)tag != TAG_GTLD || !addr)
        return addr;
    if (n < 0)
        n = 0;
    if (cap < (size_t)n || !keys) {
        size_t c = n ? (size_t)n : 1;
        void **v = (void **)realloc(keys, c * sizeof *v);
        if (!v)
            abort();
        keys = v;
        cap = c;
    }
    for (i = 0; i < n; i++)
        keys[i] = (void *)(intptr_t)(int32_t)src[i];
    return (uintptr_t)keys;
}
