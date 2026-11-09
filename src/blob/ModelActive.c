/* ModelActive (MAT3 waves.c): whether a wave instance holds a model ready to draw (waves/group.c has the waves). */
#include "game/game.h"

/* The model flag of wave instance index when it holds a block that has been set up; else 0. */
s8 ModelActive(s32 index)
{
  anon_loaded_ready_14 *slot = &gWaveInstance[index];
  if (slot->ptr == 0)
  {
    return 0;
  }
  if (slot->ready == 0)
  {
    return 0;
  }
  return slot->model;
}
