/* ZOID_RemoveObject.c: take a node out of the scene (MAT3 zoid.c, ZOID_RemoveObject()). */
#include "game/game.h"

/* Free node `node` alone (RemoveObjectTree): its children are not freed. */
void ZOID_RemoveObject(s16 node) {
    RemoveObjectTree(node, 0, 0);
}
