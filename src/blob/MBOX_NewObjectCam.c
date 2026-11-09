/* MBOX_NewObjectCam.c: add a scene node that only some views draw (MAT3 mbox.c, MBOX_NewObjectCam()). */
#include "game/game.h"

/* Add an instance of object `obj` (a handle, MBOX_FindObject_WORK) with matrix `mat` under node `parent` (-1: a
 * root), with `flags`, drawn only in view `camlink`: the node is hidden in the other three (flag bits 8-11, one a
 * view). Returns the new node's index (ZOID_AddObject's, which the MAT3 wrapper returns as a short). */
s32 MBOX_NewObjectCam(s32 obj, void *mat, s16 camlink, s32 parent, s32 flags)
{
  s16 parent_node;
  parent_node = parent;
  return ZOID_AddObject(obj, mat, parent_node, ((camlink << 8) ^ 0xF00) | flags);
}
