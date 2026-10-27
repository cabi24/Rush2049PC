/* port/assets/font.c: font data, waves 38..53 (docs/port/ASSETS.md §3.7).
 *
 * A 16-byte header of bytes (version, type, ascent, descent, leading, totalHeight, maxWidth, spaceWidth,
 * spacing, nonProp, kern, numTextures, numChars @12), numChars 12-byte letters, and with kern set a
 * numChars x numChars byte table. Every field is a byte, so nothing is swapped. The letter's first 4 bytes are a
 * slot the game fills with a pointer to that letter's row of the kern table (_SetupFontData, sound_update_channel
 * in groups/slot_sound); on PC the letter views keep the slot 4 bytes as a window address (ASSETS.md §4 #16), and
 * the game rewrites it whenever the font block moves (font data is a handle wave, which heap compaction moves).
 *
 * The converter only checks that the letters and kern table fit the block, for files in the RDRAM window (a
 * wave's block is always there; anything else is not a game heap block). */
#include <stdio.h>

#include "assets.h"

void pc_assets_font(int wave, uint8_t *buf, size_t size)
{
    uintptr_t a = (uintptr_t)buf;
    size_t chars, need;

    if (a < PC_RDRAM_BASE || a >= PC_RDRAM_BASE + PC_RDRAM_SIZE || size < 16
        || size > PC_RDRAM_BASE + PC_RDRAM_SIZE - a)
        return;
    chars = buf[12];
    need = 16 + chars * 12 + (buf[10] ? chars * chars : 0);
    if (need > size)
        fprintf(stderr, "rush2049pc: font wave %d: %zu letters need 0x%zx bytes, block is 0x%zx\n", wave, chars,
                need, size);
}
