/* AdjustTextWavePointers: points the text bank's header at its sections once its wave is loaded (MAT3
 * text.c's). Not a sound function: it sits among the sound code in the N64 image. */
#include "game/game.h"

/* The text bank's header (gText, TextTables) has a count and a start for each of the bank's six sections: string
 * tables in sections 0, 2 and 5 (single, taba, tab2b), 16-bit data in 1, 3 and 4 (tab, tab2, tab2a). */

/* PC: the bank is already in host order (port/assets/text.c). The offsets stay in the file, since a host pointer
 * doesn't fit their 4-byte slots; the three pointer sections are native char * tables built from them, and the
 * header points at those. Readers index them as char ** with host strides. */
char **pc_text_strings(s32 section, u32 *offsets, s32 count, void *base);

/* Walks the loaded text wave (gTextWaveInst): each section is a 32-bit count then its entries (32-bit string
 * offsets, or 16-bit values padded to a word), and fills gText's counts and pointers; the string sections become
 * tables of host pointers (pc_text_strings). */
void AdjustTextWavePointers(void) {
    u32 *base;
    u32 *p;

    base = GetWaveInstanceAddr(gTextWaveInst);
    p = base;
    gText.numSingle = *p++;
    gText.single = (u8 **) pc_text_strings(0, p, gText.numSingle, base);
    p += gText.numSingle;
    gText.numTab = *p++;
    gText.tab = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.numTab + 1) & ~1));
    gText.numTabA = *p++;
    gText.taba = (u8 **) pc_text_strings(2, p, gText.numTabA, base);
    p += gText.numTabA;
    gText.numTab2 = *p++;
    gText.tab2 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.numTab2 + 1) & ~1));
    gText.numTab2a = *p++;
    gText.tab2a = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.numTab2a + 1) & ~1));
    gText.numTab2b = *p++;
    gText.tab2b = (u8 **) pc_text_strings(5, p, gText.numTab2b, base);
}
