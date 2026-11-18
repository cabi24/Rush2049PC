/* ZOID_FindMatrix.c: a scene node's matrix (MAT3 zoid.c, ZOID_FindMatrix()). */
#include "game/game.h"

/* The matrix of node `node`: its rotation and position relative to its parent, which the
 * caller may change to move the node. */
tUVMatrix *ZOID_FindMatrix(short node)
{
    return ObjInstanceList[node].mat;
}
