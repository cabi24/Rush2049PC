#include "game/game.h"

s32 DecodeVlist(u8 *arg0, s32 arg1, s16 *arg2)
{
  u32 val;
  s32 cnt;
  s32 t;
  if (arg1 > 0)
  {
    do
    {
      val = (arg0[0] << 8) + arg0[1];
      arg0 += 2;
      if ((arg1 >= 2) && ((t = *arg0), (t >= 0xE0)))
      {
        cnt = t & ~0xE0;
        arg0 += 1;
      }
      else
      {
        cnt = 0;
      }
      arg1 = (arg1 - cnt) - 1;
      do
      {
        *arg2 = val;
        cnt -= 1;
        val += 1;
        arg2 += 1;
      }
      while (cnt >= 0);
    }
    while (arg1 > 0);
  }
  val = (arg0[0] << 8) + arg0[1];
  return val & 0xFFFF;
}
