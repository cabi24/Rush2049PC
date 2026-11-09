/* game_strstr: the game's strstr (the N64's own C library routines). */
#include "game/game.h"

/* The first place needle occurs in haystack, or NULL; an empty needle is found at the start (an empty haystack holds
 * only the empty needle). */
u8 *game_strstr(u8 *haystack, u8 *needle)
{
  s32 i;
  u8 *p;
  if (*haystack == 0)
  {
    if (*needle != 0)
    {
      return (u8 *) 0;
    }
    return haystack;
  }
  if (*haystack != 0)
  {
    do
    {
      i = 0;
      p = needle;
      while (1)
      {
        if (*p == 0)
        {
          return haystack;
        }
        if (*p != haystack[i]) break;
        i++;
        p++;
      }
    } while (*++haystack != 0);
  }
  return (u8 *) 0;
}
