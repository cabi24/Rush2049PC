#include "game/game.h"

typedef struct Voice { u8 pad0[5]; s8 sub; u8 id; u8 pad7[5]; void *ptr; u8 pad10[4]; } Voice;
VIEW(Voice[64], gWaveInstance);
#define gWaveInstance gWaveInstance_view

s32 FindWaveInstanceInternal(s32 arg0, s8 arg1)
{
  s32 i;

  for (i = 0; i < 64; i++)
  {
    if ((gWaveInstance[i].ptr != 0) && (arg0 == gWaveInstance[i].id) && ((arg1 < 0) || (arg1 == gWaveInstance[i].sub)))
    {
      return i;
    }
  }
  return -1;
}


s32 audio_user(s32 x, s32 y)
{
  return FindWaveInstanceInternal(x, y) + FindWaveInstanceInternal(y, x);
}
