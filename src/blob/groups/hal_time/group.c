/* The once-a-frame functions (MAT3 HAL_time.c): up to ten functions UpdateFrame calls every frame
 * (HAL_CallOncePerFrameFuncs.c), such as the flashing text's colour cycle (TB_SetTextColor installs it). */
#include "game/game.h"

/* The functions, NULL for an empty place (MAT3's gOPPFuncs). On the N64 IDO took the loops' end, gOPPFuncs + 10, as
 * the next variable's address (yf; docs/port/PROTOTYPES.md). */
void (*gOPPFuncs[10])(void);

/* Empties the table the first time (MAT3 HAL_InitTime). */
void HAL_InitTime(void)
{
  s32 i;
  if (gInited_HALTime == 0) {
    gInited_HALTime = 1;
    for (i = 0; i < 10; i++)
      gOPPFuncs[i] = NULL;
  }
}

/* Puts func in the first empty place of the table; nothing when the table is full. */
void HAL_InstallOncePerFrameFunc(void (*func)(void))
{
  void (**p)(void);
  if (gInited_HALTime == 0) {
    HAL_InitTime();
  }
  for (p = gOPPFuncs; ; ) {
    if (*p == NULL) { *p = func; break; }
    p++;
    if (p == gOPPFuncs + 10) break;
  }
}
