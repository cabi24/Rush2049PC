/* mbox_new_object/group.c: add a scene node (MAT3 mbox.c, MBOX_NewObject()). */
#include "game/game.h"

/* Add an instance of object `obj` (a handle, MBOX_FindObject_WORK) with matrix `mat` under node `parent` (-1: a
 * root), with `flags` (ZOID_AddObject). Returns the new node's index. */
s32 MBOX_NewObject(u32 obj, void *mat, s32 parent, u32 flags)
{
    return ZOID_AddObject(obj, mat, (s16) parent, flags);
}
