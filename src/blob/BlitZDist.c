/* BlitZDist: the depth of what the blitter draws next (MAT3 blit_pc.c). */
#include "game/game.h"

/* Sets the depth the blitter's rectangles are drawn at: a primitive depth record when it differs from the last one
 * (blitZDist), and BLIT_ATTR_ZDIST, which makes the RDP take the depth from it. */
void BlitZDist(unsigned short depth)
{
  if ((u16) blitZDist != depth)
  {
    gDPSetPrimDepth(blit_dl++, depth, 0);
    blitZDist = depth;
  }
  BlitSetAttribute(BLIT_ATTR_ZDIST);
}
