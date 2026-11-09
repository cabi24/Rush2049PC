/* BlitZDist: the depth of what the blitter draws next (MAT3 blit_pc.c). */
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

#define G_SETPRIMDEPTH 0xEE

/* Sets the depth the blitter's rectangles are drawn at: a G_SETPRIMDEPTH command when it differs from the last one
 * (blitZDist), and BLIT_ATTR_ZDIST, which makes the RDP take the depth from it. */
void BlitZDist(unsigned short depth)
{
  Gfx **head = (Gfx **) (&blit_dl);
  Gfx *cmd;
  if ((u16) blitZDist != depth)
  {
    cmd = (*head)++;
    cmd->words.w0 = (u32) G_SETPRIMDEPTH << 24;
    cmd->words.w1 = (((u32) depth) & 0xFFFF) << 0x10;
    blitZDist = depth;
  }
  BlitSetAttribute(BLIT_ATTR_ZDIST);
}
