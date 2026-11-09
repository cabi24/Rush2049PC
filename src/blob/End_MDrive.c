#include "game/game.h"

void End_MDrive(s16 arg0)
{
  void *sp1C;
  void *temp_a0;
  temp_a0 = PC_MODEL(arg0);
  if (1)
  {
    *((s16 *) (((s8 *) temp_a0) + 0x71C)) = 0;
    sp1C = temp_a0;
    StopCarSounds(temp_a0);
    *((f32 *) (((s8 *) temp_a0) + 0x7FC)) = 0.0f;
    *((f32 *) (((s8 *) temp_a0) + 0x800)) = 0.0f;
  }
  *((f32 *) (((s8 *) temp_a0) + 0x804)) = 0.0f;
  *((f32 *) (((s8 *) temp_a0) + 0x7F8)) = 0.0f;
}
