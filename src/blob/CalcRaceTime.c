#include "game/game.h"

VIEW(f32, ghostRaceTimes);
#define ghostRaceTimes ghostRaceTimes_view

f32 CalcRaceTime(void)
{
  f32 r;
  if ((gRace == 2) && (gTrackNum >= 0) && (gTrackNum < 6))
  {
    r = (&ghostRaceTimes)[gTrackNum];
  }
  else
  {
    r = 600.0f;
  }
  return r;
}

