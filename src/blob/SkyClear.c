/* SkyClear.c: no sky dome (MAT3 sky.c, SkyClear()).
 *
 * The sky dome is a model each window draws around its camera (SkyInit makes it, SkyUpdate keeps it on the camera);
 * skyMatrices holds each window's dome matrix. */
#include "game/game.h"

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view

VIEW(volatile int, skyMatrices);
#define skyMatrices skyMatrices_view

/* Clear every window's sky matrix, so SkyUpdate moves nothing (a track with no sky, or a fog). */
void SkyClear(void)
{
  s32 i;
  for (i = 0; i < (s16) gNumWindows; i++) {
    (&skyMatrices)[i] = 0;
  }
}
