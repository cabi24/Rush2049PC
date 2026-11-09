#include "game/game.h"

/* On the PC maxpath's controls per car slot are pc_mp_steer, pc_mp_brake and pc_mp_gas, PC_MAX_LINKS long, with no
 * N64 address (pc_game_types.h, docs/port/DECOUPLE.md 8): the reset clears them all. */
void InitPathData(void)
{
  s16 i;
  ReadPathData();
  for (i = 0; i < PC_MAX_LINKS; i++) {
    PC_MP_STEER(i) = 0.0f;
    PC_MP_BRAKE(i) = 0.0f;
    PC_MP_GAS(i) = 0.0f;
  }
  gLastPathSwitchTime = gModelTime;
  current_drone = -1;
}
