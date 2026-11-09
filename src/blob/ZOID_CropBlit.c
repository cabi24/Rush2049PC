/* ZOID_CropBlit: the part of its texture a zoid blit list entry shows (MAT3 zoidblit.c). */
#include "game/game.h"

VIEW(tBlitInstance[4], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

/* Crops entry index to texture rows top..bot and columns left..right; a negative value means the edge of the texture
 * (0 for top and left, the entry's height - 1 and width - 1 for bot and right). */
void ZOID_CropBlit(s32 index, s32 top, s32 bot, s32 left, s32 right)
{
  if (top < 0)
  {
    (&gBlitInstanceList)[index].top = 0;
  }
  else
  {
    (&gBlitInstanceList)[index].top = top;
  }
  if (left < 0)
  {
    (&gBlitInstanceList)[index].left = 0;
  }
  else
  {
    (&gBlitInstanceList)[index].left = left;
  }
  if (bot < 0)
  {
    (&gBlitInstanceList)[index].bot = (&gBlitInstanceList)[index].height - 1;
  }
  else
  {
    (&gBlitInstanceList)[index].bot = bot;
  }
  if (right < 0)
  {
    (&gBlitInstanceList)[index].right = (&gBlitInstanceList)[index].width - 1;
    return;
  }
  (&gBlitInstanceList)[index].right = (s16) right;
}
