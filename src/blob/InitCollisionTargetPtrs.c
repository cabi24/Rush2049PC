/* InitCollisionTargetPtrs: forgets the path targets that move collision objects (MAT3 targetpaths.c). */
#include "game/game.h"

/* The path targets that move a collision object (StartPathTarget registers them; GetTargetVelocity reads them):
 * tTarget * on the PC, pointer-sized. */
intptr_t CollisionTargetPtrs[30];

/* Clears CollisionTargetPtrs, at a track's set-up (initTargetStructs). */
void InitCollisionTargetPtrs(void)
{
    s32 i;
    for (i = 0; i < 30; i++) {
        CollisionTargetPtrs[i] = 0;
    }
    numCollisionTargetPtrs = 0;
}
