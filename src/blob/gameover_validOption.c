#include "game/game.h"

s32 gameover_validOption(s32 arg0)
{
  s32 result = 1;
  if (gRace == 3) {
    if (arg0 == 1 || arg0 == 2 || arg0 == 3 || arg0 == 4) {
      result = 0;
    }
    if (arg0 == 5 && (PC_CAR_FINISHED(gCircuitPlayer)) == 0) {
      result = 0;
    }
  } else if (gRace == 1) {
    if (arg0 == 0 || arg0 == 1 || arg0 == 2 || arg0 == 5) {
      result = 0;
    }
  } else {
    if (arg0 == 5 || arg0 == 2) {
      result = 0;
    }
  }
  return result;
}
