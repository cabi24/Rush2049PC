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

/* PC: the same G_SETPRIMDEPTH with host Gfx (16-byte commands); blit_dl holds a Gfx pointer. */
void BlitZDist(unsigned short arg0)
{
  Gfx **head = (Gfx **) (&blit_dl);
  Gfx *g;
  if ((u16) blitZDist != arg0)
  {
    g = (*head)++;
    g->words.w0 = 0xEE000000;
    g->words.w1 = (((u32) arg0) & 0xFFFF) << 0x10;
    blitZDist = arg0;
  }
  BlitSetAttribute(0x10);
}
