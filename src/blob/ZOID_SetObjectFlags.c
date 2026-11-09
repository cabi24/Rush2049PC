/* ZOID_SetObjectFlags.c: set a scene node's flags (MAT3 zoid.c, ZOID_SetObjectFlags()). */
#include "game/game.h"

/* Set the flags of node `node` (ZOID_ObjectFlags). */
void ZOID_SetObjectFlags(int node, unsigned int flags)
{
    ObjInstanceList[node].flags = flags;
}
