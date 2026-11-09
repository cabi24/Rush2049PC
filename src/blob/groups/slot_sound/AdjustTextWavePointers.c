/* AdjustTextWavePointers: points the text bank's header at its sections once its wave is loaded (MAT3
 * text.c's). Not a sound function: it sits among the sound code in the N64 image. */
#include "game/game.h"

/* Points gText (TextTables) at the loaded text wave's strings (gTextWaveInst): each of the bank's six sections, a
 * count and a table, the string tables in sections 0, 2 and 5 (single, taba, tab2b) and 16-bit data in 1, 3 and 4
 * (tab, tab2, tab2a). On the N64 this walked the file and wrote each string's address over its offset there; on the
 * PC the bank's converter has built the tables beside the file when it loaded (port/assets/text.c), the string
 * tables as host pointers, and gText takes them. */
void AdjustTextWavePointers(void) {
    gText = *pc_text_bank(GetWaveInstanceAddr(gTextWaveInst));
}
