/* FindWaveInstanceInternal (MAT3 waves.c _FindWaveInstance): the wave instance that holds a wave (waves/group.c has
 * the waves). */
#include "game/game.h"

/* The first wave instance that holds wave waveNum and, unless handle is negative, has that handle kind; -1 when
 * none does. */
s32 FindWaveInstanceInternal(s32 waveNum, s8 handle)
{
  s32 i;

  for (i = 0; i < 64; i++)
  {
    if ((gWaveInstance[i].ptr != 0) && (waveNum == gWaveInstance[i].wave) && ((handle < 0) || (handle == gWaveInstance[i].handle)))
    {
      return i;
    }
  }
  return -1;
}

/* A stand-in caller of FindWaveInstanceInternal; nothing calls it. */
s32 audio_user(s32 x, s32 y)
{
  return FindWaveInstanceInternal(x, y) + FindWaveInstanceInternal(y, x);
}
