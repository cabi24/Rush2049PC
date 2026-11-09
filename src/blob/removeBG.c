/* removeBG: the front end's scrolling background taken down (MAT3 front.c removeBG); createBG puts it up and updateBG
 * scrolls it. */
#include "game/game.h"


/* Remove the background's blit and its four polygons, if they are up. */
void removeBG(void)
{
  s32 i;
  if (backBlit != 0)
  {
    RemoveBlit((tBlit *) backBlit);
    backBlit = 0;
  }
  for (i = 0; i < 4; i++)
  {
    if (backPolys[i] != 0)
    {
      ZOID_RemovePoly(backPolys[i]);
      backPolys[i] = 0;
    }
  }
}
