/* RemoveBlit: give a blit and its children back to the pool (MAT3 blit.c). */
#include "game/game.h"

/* Removes blit and the rest of its multiblit (its children): frees each one's zoid blit list entry (ZOID_RemoveBlit,
 * inline) and swaps it with the last blit in use in BlitPtrs, so the first NumBlits entries stay the ones in use.
 * Not in include/game: its callers pass a blit as a pointer or as an integer (PCONLY.md 4, "Wave 2: headers"). */
void RemoveBlit(tBlit *blit) {
    s32 i;
    s32 entry;

    while (blit != 0) {
        for (i = 0; i < NumBlits; i++) {
            if (blit == BlitPtrs[i]) break;
        }
        if (i) {}
        entry = blit->blidx;
        gBlitInstanceList[entry].hide = BLITINST_FREE;
        /* Original game: nothing checks that the blit was found. Removing one that is not in BlitPtrs (a double
         * remove) leaves i == NumBlits and still takes one off NumBlits and swaps BlitPtrs[NumBlits] with the
         * entry past the end, which drops a live blit from the table (MAT3 blit.c alike). Kept as retail. */
        NumBlits--;
        BlitPtrs[i] = BlitPtrs[NumBlits];
        BlitPtrs[NumBlits] = blit;
        blit = blit->child;
    }
}
