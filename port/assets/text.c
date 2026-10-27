/* port/assets/text.c: the text bank, waves 0..5, one per language (docs/port/ASSETS.md §3.3).
 *
 * The file is six sections, each a u32 count followed by its items, then the strings:
 *   0 single  u32[n] string offsets        3 tab2   u16[n rounded up to even]
 *   1 tab     u16[n rounded up to even]    4 tab2a  u16[n rounded up to even]
 *   2 taba    u32[n] string offsets        5 tab2b  u32[n] string offsets
 * pc_assets_text swaps the counts and tables in place; the strings are bytes.
 *
 * The game's _AdjustTextWavePointers (func_800A4E58, groups/slot_sound) then fills the text header gText
 * (D_8017A4E0). On the N64 it writes base + offset into each 4-byte offset slot and points the header at the
 * slots, which a host pointer doesn't fit. Its PC version asks pc_text_strings for a native char * table per
 * offset section instead, and the file keeps its offsets. The game's readers index the header's tables as
 * char ** (their PC views use host strides, ASSETS.md §4 #15).
 *
 * The language switch (func_800A510C) reads the new bank with ReplaceWave, through _ReadWave into the same
 * block, so it is converted here again; then it runs func_800A4E58 again, which rebuilds the tables. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"

/* Which sections hold string offsets (u32 items); the others hold u16 items padded to an even count. */
static const unsigned char k_offset_section[6] = {1, 0, 1, 0, 0, 1};

/* Item count and byte size of section s whose count word is at buf + pos (big-endian); 0 if it doesn't fit. */
static int section_at(const uint8_t *buf, size_t size, size_t pos, int s, uint32_t *n, size_t *bytes)
{
    if (size - pos < 4)
        return 0;
    *n = pc_be32(buf + pos);
    *bytes = k_offset_section[s] ? (size_t)*n * 4 : (size_t)((*n + 1) & ~1u) * 2;
    return *bytes <= size - pos - 4;
}

void pc_assets_text(int wave, uint8_t *buf, size_t size)
{
    size_t pos = 0, bytes;
    uint32_t n;
    int s;

    /* All six sections must fit before anything is swapped: a file that doesn't is left as it is. */
    for (s = 0; s < 6; s++) {
        if (!section_at(buf, size, pos, s, &n, &bytes)) {
            fprintf(stderr, "rush2049pc: text wave %d: section %d runs past its 0x%zx-byte block, left as is\n",
                    wave, s, size);
            return;
        }
        pos += 4 + bytes;
    }
    for (s = 0, pos = 0; s < 6; s++) {
        section_at(buf, size, pos, s, &n, &bytes);
        pc_swap32_at(buf + pos);
        if (k_offset_section[s])
            pc_swap32_n(buf + pos + 4, n);
        else
            pc_swap16_n(buf + pos + 4, (n + 1) & ~1u);
        pos += 4 + bytes;
    }
}

/* The three offset sections (0, 2 and 5) as native char * tables: base + offset per item. One table per
 * section, reused by the next bank (the language switch); it only moves when a bank has more items. */
static struct {
    char **v;
    size_t cap;
} g_tables[3];

char **pc_text_strings(int section, const uint32_t *offsets, int count, void *base)
{
    int t = section == 0 ? 0 : section == 2 ? 1 : section == 5 ? 2 : -1;
    int i;

    if (t < 0 || count < 0)
        return NULL;
    if (g_tables[t].cap < (size_t)count || !g_tables[t].v) {
        size_t cap = count ? (size_t)count : 1;
        char **v = (char **)realloc(g_tables[t].v, cap * sizeof *v);
        if (!v)
            abort();
        g_tables[t].v = v;
        g_tables[t].cap = cap;
    }
    for (i = 0; i < count; i++)
        g_tables[t].v[i] = (char *)base + offsets[i];
    return g_tables[t].v;
}
