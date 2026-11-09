/* game_strncpy: the C library's strncpy, as the game links its own copy (the build renames the C library's names the
 * game defines: docs/code/README.md "The headers"). */
#include "game/game.h"

/* Copies at most count bytes of the string src to dest, and pads dest with zero bytes up to count when src is
 * shorter. As strncpy, dest is not terminated when src has count bytes or more. Returns dest. */
s8 *game_strncpy(s8 *dest, u8 *src, u32 count)
{
  s8 *d;
  u8 *s;
  u8 c;
  u32 n;

  d = dest;
  s = src;
  n = count;
  if (n != 0)
  {
    do
    {
      c = *s;
      n--;
      d++;
      s++;
      d[-1] = c;
      if (c == 0)
      {
        break;
      }
    } while (n != 0);
  }
  while (n-- > 0)
  {
    *d++ = 0;
  }
  return dest;
}
