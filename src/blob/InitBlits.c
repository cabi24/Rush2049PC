#include "game/game.h"

VIEW(intptr_t, inactiveBlit);
#define inactiveBlit inactiveBlit_view

VIEW(intptr_t, BlitPtrs);
#define BlitPtrs BlitPtrs_view

/* PC: a Blit is 0x68 bytes on the host (NewMultiBlit.c, docs/port/POINTERS.md 9), and the 0x3200-byte arena block
 * Blits holds the 200 at the N64's 64. The pool is host storage here; BlitPtrs is pointer-sized. */
static long long pc_blits[200][0x68 / 8];
void InitBlits(void)
{
  s32 i;
  for (i = 0; i < 200; i++) {
    (&BlitPtrs)[i] = (intptr_t) pc_blits[i];
  }
  NumBlits = 0;
  inactiveBlit = 0;
}
