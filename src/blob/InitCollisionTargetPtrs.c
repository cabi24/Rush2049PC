#include "game/game.h"

intptr_t CollisionTargetPtrs[30];   /* Camera *[30]: pointer-sized slots on the PC */

void InitCollisionTargetPtrs(void)
{
  s32 i;
  for (i = 0; i < 30; i++) {
    CollisionTargetPtrs[i] = 0;
  }
  numCollisionTargetPtrs = 0;
}
