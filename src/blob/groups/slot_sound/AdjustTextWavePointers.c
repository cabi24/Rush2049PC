/* AdjustTextWavePointers: points the text bank's header at its sections once its wave is loaded (MAT3
 * text.c's). Not a sound function: it sits among the sound code in the N64 image. */
#include "game/game.h"

/* The text bank's header (gText): for each of its six sections a count and where it starts: string tables in
 * sections 0, 2 and 5, 16-bit data in 1, 3 and 4. */
typedef struct {
    /* 0x00 */ u16 count0;
    /* 0x04 */ u32 *ptrs0;
    /* 0x08 */ u16 count1;
    /* 0x0A */ u16 count2;
    /* 0x0C */ u16 *data1;
    /* 0x10 */ u32 *ptrs2;
    /* 0x14 */ u16 count3;
    /* 0x16 */ u16 count4;
    /* 0x18 */ u16 count5;
    /* 0x1C */ u16 *data3;
    /* 0x20 */ u16 *data4;
    /* 0x24 */ u32 *ptrs5;
} ModelTables;

VIEW(ModelTables, gText);
#define gText gText_view

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
    gText.count0 = *p++;
    gText.ptrs0 = (u32 *)pc_text_strings(0, p, gText.count0, base);
    p += gText.count0;
    gText.count1 = *p++;
    gText.data1 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count1 + 1) & ~1));
    gText.count2 = *p++;
    gText.ptrs2 = (u32 *)pc_text_strings(2, p, gText.count2, base);
    p += gText.count2;
    gText.count3 = *p++;
    gText.data3 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count3 + 1) & ~1));
    gText.count4 = *p++;
    gText.data4 = (u16 *) p;
    p = (u32 *) ((u16 *) p + ((gText.count4 + 1) & ~1));
    gText.count5 = *p++;
    gText.ptrs5 = (u32 *)pc_text_strings(5, p, gText.count5, base);
}
