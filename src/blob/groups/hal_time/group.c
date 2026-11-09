#include "game/game.h"

intptr_t gOPPFuncs[10];
/* The table holds function pointers, 10 of them: the loops end at gOPPFuncs + 10 (on the N64 the address of the next
 * variable, yf, which IDO took for it; docs/port/PROTOTYPES.md). */
void HAL_InitTime(void)
{
  s32 i;
  if (gInited_HALTime == 0) {
    gInited_HALTime = 1;
    for (i = 0; i < 10; i++)
      gOPPFuncs[i] = 0;
  }
}
void HAL_InstallOncePerFrameFunc(intptr_t arg0)
{
  intptr_t *p;
  if (gInited_HALTime == 0) {
    HAL_InitTime();
  }
  for (p = gOPPFuncs; ; ) {
    if (*p == 0) { *p = arg0; break; }
    p++;
    if (p == gOPPFuncs + 10) break;
  }
}
