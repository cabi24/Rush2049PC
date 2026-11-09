/* CancelPakError: close a pak's error dialog (MAT3 pakerr.c). */
#include "game/game.h"

/* Closes the latest of pak's two error dialogs that is open (err 0: closed). */
void CancelPakError(s32 pak)
{
  s32 i;
  PakErrorState *d = gPakError[pak];
  for (i = 1; i >= 0; i--) {
    if (d[i].err != 0) {
      d[i].err = 0;
      return;
    }
  }
}
