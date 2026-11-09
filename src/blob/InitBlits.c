/* InitBlits: empty the blit pool (MAT3 blit.c). */
#include "game/game.h"

VIEW(intptr_t, BlitPtrs);
#define BlitPtrs BlitPtrs_view

/* The pool of blits: 200 records in the PC's layout (tBlit, 0x68 bytes; the cartridge image's block Blits holds the
 * N64's 0x40-byte ones and is unused). */
static long long pc_blits[MAX_BLITS][sizeof(tBlit) / 8];

/* Frees every blit: BlitPtrs[i] points to the pool's record i again and none is in use. Called when a screen or a
 * race starts, before it makes its blits. (MAT3 also hides the controller warning here.) */
void InitBlits(void)
{
  s32 i;
  for (i = 0; i < MAX_BLITS; i++) {
    (&BlitPtrs)[i] = (intptr_t) pc_blits[i];
  }
  NumBlits = 0;
  inactiveBlit = 0;
}
