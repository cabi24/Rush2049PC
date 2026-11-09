#include "game/game.h"

u8 *game_strstr(u8 *arg0, u8 *arg1)
{
  s32 i;
  u8 *p;
  if (*arg0 == 0)
  {
    if (*arg1 != 0)
    {
      return (u8 *) 0;
    }
    return arg0;
  }
  if (*arg0 != 0)
  {
    do
    {
      i = 0;
      p = arg1;
      while (1)
      {
        if (*p == 0)
        {
          return arg0;
        }
        if (*p != arg0[i]) break;
        i++;
        p++;
      }
    } while (*++arg0 != 0);
  }
  return (u8 *) 0;
}
