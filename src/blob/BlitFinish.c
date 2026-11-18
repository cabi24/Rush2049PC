/* BlitFinish: end the blitter's display list for this frame (MAT3 blit_pc.c; the N64 side of it). */
#include "game/game.h"

/* Ends the blitter's display list and returns its start for the frame's main list to call, or NULL when
 * no blitting began this frame (blit_dl NULL: BlitInit wasn't called). blit_dl is NULL again afterwards, so the next
 * BlitInit starts the other list. */
void *BlitFinish(void)
{
  if (blit_dl == NULL)
  {
    return NULL;
  }
  gSPEndDisplayList(blit_dl);
  blit_dl = NULL;
  return pc_text_dl[blitDLnum];
}
