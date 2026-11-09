/* ZOID_SetObjectDef.c: change the object a scene node draws (MAT3 zoid.c, ZOID_SetObjectDef()). */
#include "game/game.h"

/* Make node `node` draw object `def` (a handle, (model << 10) | definition). */
void ZOID_SetObjectDef(s16 node, u16 def) {
    ObjInstanceList[node].index = def;
}
