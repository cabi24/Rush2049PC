/* The front end's waves kept loaded (MAT3 state.c's LockAndLoad and LoadFrontEndWaves). */
#include "game/game.h"

/* The front end's waves (asset files) kept in memory while its screens run (their sounds and art). */
#define WAVE_FRONT_END_A 54
#define WAVE_FRONT_END_B 58
#define WAVE_FRONT_END_C 59

/* Loads wave id and locks it in memory, unless an instance of it is already loaded. */
void LockAndLoad(s32 id)
{
  if (FindWaveInstanceInternal(id, -1) < 0)
  {
    LockWaveInstance(LoadWave(id, 0, 0, 0, 0));
  }
}

/* Loads the front end's three waves (the game state's change to the front end). */
void LoadFrontEndWaves(void)
{
  LockAndLoad(WAVE_FRONT_END_A);
  LockAndLoad(WAVE_FRONT_END_B);
  LockAndLoad(WAVE_FRONT_END_C);
}

/* LockAndLoad under another name. Nothing calls it: a stand-in caller left from the N64 build (it kept LockAndLoad
 * out of line there). */
void other_user(s32 x)
{
  LockAndLoad(x);
}
