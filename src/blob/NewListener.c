#include "game/game.h"

VIEW(s32, gFreeListeners);
#define gFreeListeners gFreeListeners_view

void NewListener(void)
{
  s32 *sp1C;
  s32 **new_var3;
  s8 *new_var2;
  s8 *new_var;
  sp1C = *((s32 **) (((s8 *) (&gFreeListeners)) + 8));
  RemoveLLE(&gFreeListeners, sp1C);
  new_var = (s8 *) (*(new_var3 = &sp1C));
  new_var2 = (s8 *) (new_var + 8);
  *new_var2 = 0;
}

