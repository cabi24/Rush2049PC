#include "game/game.h"

void LockAndLoad(s32 id)
{
  if (FindWaveInstanceInternal(id, -1) < 0)
  {
    LockWaveInstance(LoadWave(id, 0, 0, 0, 0));
  }
}

void LoadFrontEndWaves(void)
{
  LockAndLoad(54);
  LockAndLoad(58);
  LockAndLoad(59);
}

void other_user(s32 x)
{
  LockAndLoad(x);
}

