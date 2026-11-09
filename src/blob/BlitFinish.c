#include "game/game.h"
#define Gfx GfxView   /* this file's own Gfx: include/game's Gfx is another type */

typedef union 
{
  struct 
  {
    u32 w0;
    uintptr_t w1;
  } words;
  u64 force_structure_alignment;
} Gfx;

VIEW(intptr_t, blit_dl);
#define blit_dl blit_dl_view

/* PC: the same with host Gfx (16-byte commands). blit_dl holds a Gfx pointer; the two buffers are
 * pc_text_dl (groups/blit/init.c). */
extern Gfx pc_text_dl[2][2400];
void *BlitFinish(void)
{
  Gfx **head = (Gfx **) (&blit_dl);
  Gfx *g = *head;
  if (g == ((void *) 0))
  {
    return (void *) 0;
  }
  g->words.w0 = 0xDF000000;
  g->words.w1 = 0;
  *head = (void *) 0;
  return pc_text_dl[blitDLnum];
}

