#include "game/game.h"

s8 *game_strncpy(s8 *arg0, u8 *arg1, u32 arg2)
{
  s8 *d;
  u8 *s;
  u8 c;
  u32 n;

  d = arg0;
  s = arg1;
  n = arg2;
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
  return arg0;
}

