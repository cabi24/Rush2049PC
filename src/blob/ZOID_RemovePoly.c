#include "game/game.h"

VIEW(s32, PolyInstanceList);
#define PolyInstanceList PolyInstanceList_view

typedef struct { s16 f0; u8 pad[0x56]; } E58;
void ZOID_RemovePoly(s16 *arg0) {
  *arg0 = 0;
  if (arg0 + 0x2C == (s16 *) (((E58 *)&PolyInstanceList) + gNumPolyInsts)) {
    do {
      --gNumPolyInsts;
      arg0 -= 0x2C;
      if (*arg0) break;
    } while (gNumPolyInsts > 0);
  }
}
