/* port/assets/font.c: font data, waves 38..53 (docs/port/ASSETS.md 3.7, docs/port/NATIVE.md step 7).
 *
 * A 16-byte header of bytes (FontHeader: version, type, ascent, descent, leading, totalHeight, maxWidth, spaceWidth,
 * spacing, nonProp, kern @10, numTextures, numChars @12), numChars 12-byte letters, and with kern set a numChars x
 * numChars byte table, one row per letter. Every field is a byte, so nothing is swapped. A letter's first 4 bytes are
 * a slot where the game kept a pointer to its row of the kerning table: on the N64 SetupFontData (groups/slot_sound)
 * wrote the row's 32-bit address there, and again whenever the heap moved the file (the font data is loaded as a
 * handle, which heap compaction moves).
 *
 * Here the converter builds the letters beside the file when it loads (struct FontLetter, pc_game_types.h), each
 * with a host pointer to its row of a copy of the kerning table, and keeps them with the file's block (records.c),
 * which follows the block when the heap moves it (pc_heap_block_moved): nothing points into the block, so nothing is
 * relocated. SetupFontData takes them (pc_font_letters); the header stays the file's, which the
 * game reads and writes (HAL_SetFontFixedWidth: nonProp) where it is. */
#include <stdio.h>
#include <stdlib.h>

#include "assets.h"
#include "pc_game_types.h"

#define FONT_HEADER_SIZE 16
#define FONT_LETTER_SIZE 12

static void release_font(void *letters)
{
    free(letters);
}

void pc_assets_font(int wave, uint8_t *buf, size_t size)
{
    size_t chars, kern, need, i, have;
    struct FontLetter *letters;
    unsigned char *table;

    if (size < FONT_HEADER_SIZE)
        return;
    chars = buf[12];
    kern = buf[10] != 0;
    need = FONT_HEADER_SIZE + chars * FONT_LETTER_SIZE + (kern ? chars * chars : 0);
    if (need > size)
        fprintf(stderr, "rush2049pc: font wave %d: %zu letters need 0x%zx bytes, block is 0x%zx\n", wave, chars,
                need, size);
    /* The letters, then the copy of the kerning table, in one allocation; what the block doesn't hold reads 0. */
    letters = calloc(1, (chars ? chars : 1) * sizeof *letters + (kern ? chars * chars : 0));
    if (!letters)
        abort();
    table = (unsigned char *)(letters + (chars ? chars : 1));
    if (kern) {
        have = need <= size ? chars * chars : size > FONT_HEADER_SIZE + chars * FONT_LETTER_SIZE
                                                  ? size - FONT_HEADER_SIZE - chars * FONT_LETTER_SIZE : 0;
        memcpy(table, buf + FONT_HEADER_SIZE + chars * FONT_LETTER_SIZE, have);
    }
    for (i = 0; i < chars; i++) {
        size_t at = FONT_HEADER_SIZE + i * FONT_LETTER_SIZE;
        uint8_t l[FONT_LETTER_SIZE] = {0};

        if (at < size)
            memcpy(l, buf + at, size - at < FONT_LETTER_SIZE ? size - at : FONT_LETTER_SIZE);
        letters[i].kerns = kern ? table + i * chars : NULL;
        letters[i].value = l[4];
        letters[i].texIdx = l[5];
        letters[i].uls = l[6];
        letters[i].ult = l[7];
        letters[i].lrs = l[8];
        letters[i].lrt = l[9];
        memcpy(&letters[i].pad12, l + 10, 2);
    }
    pc_records_keep(buf, size, PC_RECORDS_FONT, letters, release_font);
}

struct FontLetter *pc_font_letters(const void *file)
{
    /* A font no converter took (it said why): 256 letters of nothing, none kerned. */
    static struct FontLetter none[256];
    struct FontLetter *letters = pc_records_find(file, PC_RECORDS_FONT);

    if (letters)
        return letters;
    fprintf(stderr, "rush2049pc: no font data was converted at %p: no letters\n", file);
    memset(none, 0, sizeof none);
    return none;
}
