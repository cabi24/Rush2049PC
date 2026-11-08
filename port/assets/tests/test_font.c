/* port/assets/tests/test_font.c: font data (port/assets/font.c needs no swap) read by the game's own
 * _SetupFontData (SetupFontData) and string-width code (HAL_GetTextWidth): the 12-byte letters keep a
 * 4-byte kern slot that holds a window address, the wave is reached through a u32 heap handle slot, and a heap move
 * of the block is picked up on the next call.
 * game: src/blob/groups/slot_sound/group.c src/blob/groups/sound_channel_extra/HAL_GetTextWidth.c
 */
#include <string.h>

#include "assets.h"
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
void *gFontHeader, *gFontLetters;
int16_t gCharMap[256];
intptr_t gFontTextures[16];
struct { int32_t first, second; } gTexDefPtrs[4];
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
    f[12] = CHARS;   /* numChars; numTextures 0 */
    for (i = 0; i < CHARS; i++) {
        uint8_t *l = f + 16 + 12 * i;
        memset(l, 0, 12);
        l[4] = k_codes[i];
        l[6] = k_left[i];
        l[8] = k_right[i];
    }
    for (i = 0; i < CHARS; i++)
        for (j = 0; j < CHARS; j++)
            f[16 + 12 * CHARS + CHARS * i + j] = (uint8_t)(i * 3 + j);   /* kern[letter][previous] */
    return 16 + 12 * CHARS + CHARS * CHARS;
}

static uint32_t slot_of(const uint8_t *font, int i)
{
    uint32_t v;
    memcpy(&v, font + 16 + 12 * i, 4);
    return v;
}

static void check_letters(uint8_t *font)
{
    int i;

    CHECK(gFontHeader == font);
    CHECK(gFontLetters == font + 16);
    for (i = 0; i < CHARS; i++) {
        CHECK(slot_of(font, i) == (uint32_t)(uintptr_t)(font + 16 + 12 * CHARS + CHARS * i));
        CHECK(gCharMap[k_codes[i]] == i);
    }
    CHECK(gCharMap['D'] == -1);
}

int main(void)
{
    uint8_t *w = test_rdram(), *font, copy[64];
    uint32_t *handles;
    size_t size;
    int32_t width;

    if (!w) {
        fprintf(stderr, "test_font: no RDRAM window here, skipped\n");
        return 0;
    }
    handles = (uint32_t *)(w + 0x200000);
    font = w + 0x200100;
    size = make_font(font);
    memcpy(copy, font, size);
    pc_assets_font(38, font, 0x40);
    CHECK(memcmp(copy, font, size) == 0);   /* bytes only: nothing to swap */

    /* A handle wave: the instance holds the handle, a u32 heap slot; the next slot is another handle's. */
    handles[0] = (uint32_t)(uintptr_t)font;
    handles[1] = PC_RDRAM_BASE + 0x123450u;   /* a window address */
    gWaveInstance[2].data = handles;
    gFontDatWaveInst = 2;
    SetupFontData(1);
    check_letters(font);
    CHECK(gSpacing == 1 && gLeading == 2 && font_image_format == 4);

    /* "BC": B, then C kerned against B (kern[2][1] = 7). */
    width = HAL_GetTextWidth((uint8_t *)"BC", -1);
    CHECK(width == (17 - 10 + 1 + 1) + (24 - 20 + 1 + 1) - 7 - 1);
    width = HAL_GetTextWidth((uint8_t *)"A B", -1);
    CHECK(width == (6 - 0 + 1 + 1) + 5 + (17 - 10 + 1 + 1) - 1);

    /* Heap compaction moves the block and rewrites the handle; the next call re-derives the letters. */
    memmove(font + 0x1000, font, size);
    font += 0x1000;
    handles[0] = (uint32_t)(uintptr_t)font;
    SetupFontData(0);
    check_letters(font);
    width = HAL_GetTextWidth((uint8_t *)"BC", -1);
    CHECK(width == (17 - 10 + 1 + 1) + (24 - 20 + 1 + 1) - 7 - 1);
    return TEST_DONE();
}
