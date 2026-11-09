/* port/assets/tests/test_font.c: font data (port/assets/font.c) read by the game's own _SetupFontData (SetupFontData)
 * and string-width code (HAL_GetTextWidth): the converter builds the letters beside the file with each letter's row
 * of a copy of the kerning table (the file's bytes untouched), the wave is reached through a heap handle (a
 * pointer-sized master slot, include/game/heap.h), the font's textures are its texture WAD's records, and a heap move
 * of the block (pc_heap_block_moved) is picked up on the next call with the same letters. No window
 * (docs/port/NATIVE.md step 7).
 * game: src/blob/groups/slot_sound/group.c src/blob/groups/sound_channel_extra/HAL_GetTextWidth.c
 */
#include <string.h>

#include "assets.h"
#include "pc_game_types.h"
#include "support.h"

/* Stand-ins for what the two game files reference, with their PC views' layouts. */
struct ResSlot {
    uint8_t unk0;
    int8_t state;
    uint8_t active;
    uint8_t pad3[9];
    void *data;
    int32_t unk10;
} gWaveInstance[4];
void PurgeableMemory(void *ptr, int32_t dec) {}
int32_t font_image_format, gFontDatWaveInst, gFontTexWaveInst, font_texel_size;
void *gFontHeader;
struct FontLetter *gFontLetters;
int16_t gCharMap[256];
struct tTexture *gFontTextures[16];
struct { struct tTexture *texDefs; int32_t count; } gTexDefPtrs[4];
uint8_t gLeading, gSpacing;

void SetupFontData(int32_t force);
int32_t HAL_GetTextWidth(uint8_t *str, int16_t maxlen);

enum { CHARS = 3 };
static const uint8_t k_codes[CHARS] = {'A', 'B', 'C'};
static const uint8_t k_left[CHARS] = {0, 10, 20}, k_right[CHARS] = {6, 17, 24};

static size_t make_font(uint8_t *f)
{
    int i, j;

    memset(f, 0, 16);
    f[1] = 1;        /* mode */
    f[4] = 2;        /* leading */
    f[6] = 9;        /* maxWidth */
    f[7] = 5;        /* spaceWidth */
    f[8] = 1;        /* spacing */
    f[10] = 1;       /* kern */
    f[11] = 2;       /* numTextures */
    f[12] = CHARS;   /* numChars */
    for (i = 0; i < CHARS; i++) {
        uint8_t *l = f + 16 + 12 * i;
        memset(l, 0, 12);
        l[4] = k_codes[i];
        l[5] = (uint8_t)(i & 1);
        l[6] = k_left[i];
        l[8] = k_right[i];
    }
    for (i = 0; i < CHARS; i++)
        for (j = 0; j < CHARS; j++)
            f[16 + 12 * CHARS + CHARS * i + j] = (uint8_t)(i * 3 + j);   /* kern[letter][previous] */
    return 16 + 12 * CHARS + CHARS * CHARS;
}

static void check_letters(uint8_t *font, struct FontLetter *letters)
{
    int i, j;

    CHECK(gFontHeader == font);
    CHECK(gFontLetters == letters && letters == pc_font_letters(font));
    for (i = 0; i < CHARS; i++) {
        CHECK(letters[i].value == k_codes[i] && letters[i].texIdx == (i & 1));
        CHECK(letters[i].uls == k_left[i] && letters[i].lrs == k_right[i]);
        for (j = 0; j < CHARS; j++)
            CHECK(letters[i].kerns[j] == i * 3 + j);
        CHECK(letters[i].kerns < font || letters[i].kerns >= font + 0x40);   /* a copy, not the file's row */
        CHECK(gCharMap[k_codes[i]] == i);
    }
    CHECK(gCharMap['D'] == -1);
}

int main(void)
{
    static uint8_t heap[0x3000];
    static struct tTexture textures[2];
    uint8_t *font = heap + 0x100, copy[64];
    void *handles[2];
    struct FontLetter *letters;
    size_t size;
    int32_t width;

    size = make_font(font);
    memcpy(copy, font, size);
    pc_wave_loaded(38, font, 0x40);
    CHECK(memcmp(copy, font, size) == 0);   /* the file's bytes: nothing written into it */
    letters = pc_font_letters(font);

    /* A handle wave: the instance holds the handle, a heap master slot; the next slot is another handle's. */
    handles[0] = font;
    handles[1] = heap + 0x2000;
    gWaveInstance[2].data = handles;
    gFontDatWaveInst = 2;
    gTexDefPtrs[1].texDefs = textures, gTexDefPtrs[1].count = 2;
    gFontTexWaveInst = 1;
    SetupFontData(1);
    check_letters(font, letters);
    CHECK(gSpacing == 1 && gLeading == 2 && font_image_format == 4);
    CHECK(gFontTextures[0] == &textures[0] && gFontTextures[1] == &textures[1]);

    /* "BC": B, then C kerned against B (kern[2][1] = 7). */
    width = HAL_GetTextWidth((uint8_t *)"BC", -1);
    CHECK(width == (17 - 10 + 1 + 1) + (24 - 20 + 1 + 1) - 7 - 1);
    width = HAL_GetTextWidth((uint8_t *)"A B", -1);
    CHECK(width == (6 - 0 + 1 + 1) + 5 + (17 - 10 + 1 + 1) - 1);

    /* Heap compaction moves the block, tells the PC layer and rewrites the handle; the next call takes the header at
     * its new place and the same letters, which followed the block. */
    memmove(font + 0x1000, font, 0x40);
    pc_heap_block_moved(font, font + 0x1000, 0x40);
    pc_heap_block_freed(font, 0x40);   /* the old chunk is freed after the move */
    font += 0x1000;
    handles[0] = font;
    SetupFontData(0);
    check_letters(font, letters);
    width = HAL_GetTextWidth((uint8_t *)"BC", -1);
    CHECK(width == (17 - 10 + 1 + 1) + (24 - 20 + 1 + 1) - 7 - 1);

    /* The block freed: the letters go; a font no converter took gives none. */
    pc_heap_block_freed(font, 0x40);
    CHECK(pc_records_find(font, PC_RECORDS_FONT) == NULL && pc_font_letters(font)[0].kerns == NULL);
    return TEST_DONE();
}
