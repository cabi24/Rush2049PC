/* TargetColl3Piece: collision test 3 of the track objects (gTargetCollFuncPtrs[3], MAT3 targets.c
 * _TargetColl3Piece), for the three-piece targets. It never reports a hit, in MAT3 too. */
#include "game/game.h"

/* The collision test's four words (CheckTargetCollision passes the target, &slot and two zeros): unused. Always 0,
 * no hit. */
int TargetColl3Piece(int target, int slotp, int unused2, int unused3) {
    return 0;
}
