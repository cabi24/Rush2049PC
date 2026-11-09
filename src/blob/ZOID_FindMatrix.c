/* ZOID_FindMatrix.c: a scene node's matrix (MAT3 zoid.c, ZOID_FindMatrix()). */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* The matrix of node `node`: its rotation and position relative to its parent, which the
 * caller may change to move the node. (Callers declare it with their own matrix type.) */
tUVMatrix *ZOID_FindMatrix(short node)
{
    return ObjInstanceList[node].mat;
}
