/* ZOID_ObjectFlags.c: a scene node's flags (MAT3 zoid.c, ZOID_ObjectFlags()). */
#include "game/game.h"

/* The flags of node `node` (OBJ_*). */
unsigned int ZOID_ObjectFlags(short node)
{
    return ObjInstanceList[node].flags;
}
