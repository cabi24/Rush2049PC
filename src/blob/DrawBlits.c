/* DrawBlits: run every blit's animation function, once a frame (MAT3 blit.c). */
#include "game/game.h"

extern void RemoveBlit(intptr_t blit);

/* Calls the animation function of each blit in use that has one and a texture (a text blit's function is the
 * renderer's to call), in BlitPtrs order. A function that returns 0 asks for its blit to go: RemoveBlit moves the last
 * blit in use into its slot, so the same slot is looked at again. */
void DrawBlits(void)
{
  tBlit **slot;
  tBlit *blit;

  if (NumBlits > 0)
  {
    slot = (tBlit **) &BlitPtrs;
    do
    {
      blit = *slot;
      if ((blit->animFunc != 0) && (blit->name != BLIT_NO_TEXTURE) && (blit->animFunc(blit) == 0))
      {
        RemoveBlit((intptr_t) blit);
        slot -= 1;
      }
      slot += 1;
    } while (slot < &((tBlit **) &BlitPtrs)[NumBlits]);
  }
}
