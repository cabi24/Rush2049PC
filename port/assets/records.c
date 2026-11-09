/* port/assets/records.c: the native records the converters build beside a file (docs/port/NATIVE.md 4.4).
 *
 * The game reads some of its files as records that hold pointers: the text bank's string tables, a world's name table
 * and target groups, a path file's headers. On the N64 it wrote those pointers into the file itself, as 32-bit
 * addresses. On the PC the file stays in its heap block as the game allocated it (converted to the host's byte order,
 * its size unchanged, so the game's heap holds what it held), and the converter builds the records in PC memory when
 * the file loads, with host pointers into the block. The game asks for them by the block (pc_text_bank,
 * pc_world_records, pc_path_records).
 *
 * This file keeps them, by block: a record goes when its block is freed (pc_heap_block_freed, which the game's heap
 * reports), released (pc_wave_released), or loaded over by another file, and follows its block when the heap moves it
 * (pc_heap_block_moved: heap compaction moves handle blocks, and the only waves loaded as handles are the font data,
 * whose letters hold no pointer into the block). Only one N64 thread runs at a time (docs/port/ULTRA.md), the
 * background loader's included, so the list needs no lock. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

typedef struct {
    const uint8_t *block;           /* the file's heap block */
    size_t size;                    /* its size */
    int kind;                       /* PC_RECORDS_* */
    void *records;
    void (*release)(void *records);
} Kept;

static Kept *g_kept;
static size_t g_count, g_room;

static void release_at(size_t i)
{
    Kept k = g_kept[i];

    g_kept[i] = g_kept[--g_count];
    if (k.release)
        k.release(k.records);
}

void pc_records_keep(void *block, size_t size, int kind, void *records, void (*release)(void *records))
{
    const uint8_t *b = (const uint8_t *)block;

    if (!size)
        size = 1;
    /* What an earlier file in this memory left goes: it isn't there any more. This file's records of another kind
     * stay (a world WAD has its WAD's and its world's). */
    for (size_t i = 0; i < g_count;)
        if (g_kept[i].block < b + size && b < g_kept[i].block + g_kept[i].size &&
            !(g_kept[i].block == b && g_kept[i].kind != kind))
            release_at(i);
        else
            i++;
    if (g_count == g_room) {
        size_t room = g_room ? 2 * g_room : 16;
        Kept *k = (Kept *)realloc(g_kept, room * sizeof *k);
        if (!k)
            abort();
        g_kept = k;
        g_room = room;
    }
    g_kept[g_count++] = (Kept){b, size, kind, records, release};
}

void *pc_records_find(const void *block, int kind)
{
    for (size_t i = 0; i < g_count; i++)
        if (g_kept[i].block == block && g_kept[i].kind == kind)
            return g_kept[i].records;
    return NULL;
}

void *pc_records_detach(const void *block, int kind)
{
    for (size_t i = 0; i < g_count; i++)
        if (g_kept[i].block == block && g_kept[i].kind == kind) {
            void *r = g_kept[i].records;
            g_kept[i] = g_kept[--g_count];
            return r;
        }
    return NULL;
}

void pc_records_drop(const void *p, size_t size)
{
    const uint8_t *b = (const uint8_t *)p;

    for (size_t i = 0; i < g_count;)
        if (g_kept[i].block < b + size && b < g_kept[i].block + g_kept[i].size)
            release_at(i);
        else
            i++;
}

void pc_records_move(const void *from, void *to, size_t size)
{
    const uint8_t *f = (const uint8_t *)from, *t = (const uint8_t *)to;

    if (!size)
        size = 1;
    for (size_t i = 0; i < g_count;)   /* stale records in the free memory the block moves to */
        if (g_kept[i].block != f && g_kept[i].block < t + size && t < g_kept[i].block + g_kept[i].size)
            release_at(i);
        else
            i++;
    for (size_t i = 0; i < g_count; i++)
        if (g_kept[i].block == f) {
            g_kept[i].block = t;
            g_kept[i].size = size;
        }
}
