#include "game/game.h"

VIEW(s32, gPakError);
#define gPakError gPakError_view

typedef struct { u8 pad[0x14]; s32 f14; u8 pad2[0x14]; } S2C;
void CancelPakError(s32 arg0)
{
  s32 i;
  S2C *d = (S2C *) (((u8 *) &gPakError) + arg0 * 0x58);
  for (i = 1; i >= 0; i--) {
    if (d[i].f14 != 0) {
      d[i].f14 = 0;
      return;
    }
  }
}
