#include "game/game.h"

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view

VIEW(volatile int, skyMatrices);
#define skyMatrices skyMatrices_view

void SkyClear(void)
{
  s32 i;
  for (i = 0; i < (s16) gNumWindows; i++) {
    (&skyMatrices)[i] = 0;
  }
}
