/* port/assets/text.c: the text bank, waves 0..5, one per language (docs/port/ASSETS.md §3.3).
 *
 * The file is six sections, each a u32 count followed by its items, then the strings:
 *   0 single  u32[n] string offsets        3 tab2   u16[n rounded up to even]
 *   1 tab     u16[n rounded up to even]    4 tab2a  u16[n rounded up to even]
 *   2 taba    u32[n] string offsets        5 tab2b  u32[n] string offsets
 * pc_assets_text swaps the counts and tables in place (the strings are bytes), then builds the bank's records beside
 * the file (records.c, docs/port/NATIVE.md 4.4): a struct TextTables (pc_game_types.h) with each section's count and
 * table, the three string sections as native tables of host pointers to the strings. On the N64 the game itself
 * (AdjustTextWavePointers, groups/slot_sound) wrote each string's address over its offset in the file and pointed
 * the header gText at the file's sections; a host pointer doesn't fit a 4-byte slot, so the file keeps its offsets
 * and AdjustTextWavePointers copies these tables into gText (pc_text_bank).
 *
 * The language switch (LoadTextWave) reads the new bank with ReplaceWave, through _ReadWave into the same block, so
 * it is converted here again and its string tables rebuilt where the last language's were (they move only for a
 * bank with more strings; the first bank loaded is the largest, InitTextWave), then AdjustTextWavePointers runs
 * again. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"
#include "pc_game_types.h"

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

/* A bank's records: its tables, and the room of its three string tables (0 single, 1 taba, 2 tab2b). */
typedef struct {
    struct TextTables tables;
    size_t room[3];
} Bank;

static void release_bank(void *records)
{
    Bank *b = (Bank *)records;

    free(b->tables.single);
    free(b->tables.taba);
    free(b->tables.tab2b);
    free(b);
}

/* String table t of bank b (0 single, 1 taba, 2 tab2b), `table` until now, for the count offsets at `offsets`
 * (native u32s): the file's start plus each offset. The table stays where it is when it has the room. */
static unsigned char **string_table(Bank *b, int t, unsigned char **table, const uint8_t *offsets, uint32_t count,
                                    uint8_t *file)
{
    uint32_t i, off;

    /* A table with no strings (a bank of zeros: no ROM) is NULL, so that the game's read of a string in it faults
     * at once, as its read through the bank's zero offsets did, instead of reading past an empty allocation. Every
     * table of the retail cartridge's banks has strings. */
    if (count == 0) {
        free(table);
        b->room[t] = 0;
        return NULL;
    }
    if (!table || b->room[t] < count) {
        size_t room = count ? count : 1;
        table = (unsigned char **)realloc(table, room * sizeof *table);
        if (!table)
            abort();
        b->room[t] = room;
    }
    for (i = 0; i < count; i++) {
        memcpy(&off, offsets + 4 * i, 4);
        table[i] = file + off;
    }
    return table;
}

void pc_assets_text(int wave, uint8_t *buf, size_t size)
{
    struct TextTables *t;
    size_t pos = 0, bytes;
    uint32_t n[6];
    uint8_t *at[6];
    Bank *bank;
    int s;

    /* All six sections must fit before anything is swapped: a file that doesn't is left as it is, with no tables. */
    for (s = 0; s < 6; s++) {
        if (!section_at(buf, size, pos, s, &n[s], &bytes)) {
            fprintf(stderr, "rush2049pc: text wave %d: section %d runs past its 0x%zx-byte block, left as is\n",
                    wave, s, size);
            pc_records_drop(buf, size);
            return;
        }
        pos += 4 + bytes;
    }
    for (s = 0, pos = 0; s < 6; s++) {
        section_at(buf, size, pos, s, &n[s], &bytes);
        pc_swap32_at(buf + pos);
        at[s] = buf + pos + 4;
        if (k_offset_section[s])
            pc_swap32_n(at[s], n[s]);
        else
            pc_swap16_n(at[s], (n[s] + 1) & ~1u);
        pos += 4 + bytes;
    }

    /* The records: the last language's when this one is read over it (ReplaceWave), so its tables stay put. The
     * counts are 16-bit in the game's header, as on the N64. */
    bank = (Bank *)pc_records_detach(buf, PC_RECORDS_TEXT);
    if (!bank && !(bank = (Bank *)calloc(1, sizeof *bank)))
        abort();
    t = &bank->tables;
    t->numSingle = (unsigned short)n[0];
    t->single = string_table(bank, 0, t->single, at[0], n[0], buf);
    t->numTab = (unsigned short)n[1];
    t->tab = (unsigned short *)at[1];
    t->numTabA = (unsigned short)n[2];
    t->taba = string_table(bank, 1, t->taba, at[2], n[2], buf);
    t->numTab2 = (unsigned short)n[3];
    t->tab2 = (unsigned short *)at[3];
    t->numTab2a = (unsigned short)n[4];
    t->tab2a = (unsigned short *)at[4];
    t->numTab2b = (unsigned short)n[5];
    t->tab2b = string_table(bank, 2, t->tab2b, at[5], n[5], buf);
    pc_records_keep(buf, size, PC_RECORDS_TEXT, bank, release_bank);
}

const struct TextTables *pc_text_bank(const void *file)
{
    static const struct TextTables none;
    const Bank *bank = (const Bank *)pc_records_find(file, PC_RECORDS_TEXT);

    if (bank)
        return &bank->tables;
    fprintf(stderr, "rush2049pc: no text bank was converted at %p: no strings\n", file);
    return &none;
}
