/* BlitFinish: end the blitter's display list for this frame (MAT3 blit_pc.c; the N64 side of it). */
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

/* The two display lists the blitter fills alternately (groups/blit/init.c): host Gfx, 16 bytes a command. */
extern Gfx pc_text_dl[2][2400];

/* Ends the blitter's display list with G_ENDDL and returns its start for the frame's main list to call, or NULL when
 * no blitting began this frame (blit_dl NULL: BlitInit wasn't called). blit_dl is NULL again afterwards, so the next
 * BlitInit starts the other list. */
void *BlitFinish(void)
{
  Gfx **head = (Gfx **) (&blit_dl);
  Gfx *cmd = *head;
  if (cmd == NULL)
  {
    return NULL;
  }
  cmd->words.w0 = (u32) G_ENDDL << 24;
  cmd->words.w1 = 0;
  *head = NULL;
  return pc_text_dl[blitDLnum];
}
