/* RemovePanes: remove the panes (MAT3 pane.c). */
#include "game/game.h"

/* Removes every pane in use and its blits, the permanent ones too when permanent is set. */
void RemovePanes(s32 permanent)
{
  tBlitSet *pane;
  s32 i;

  for (pane = (tBlitSet *) &gPaneBlits; pane < (tBlitSet *) &gPaneBlits + MAX_PANES; pane++)
  {
    if ((pane->used != 0) && ((permanent != 0) || (pane->perm == 0)))
    {
      for (i = 0; i < PANE_BLITS; i++)
      {
        if (pane->blits[i] != 0)
        {
          RemoveBlit(pane->blits[i]);
          pane->blits[i] = 0;
        }
      }
      pane->used = 0;
    }
  }
}
