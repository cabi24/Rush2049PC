/* ZOID_NewMatrix.c: hand out a persistent matrix for a scene node (MAT3 zoid.c, ZOID_NewMatrix()).
 *
 * Matrices come from a pool that ZOID_Reset empties: index 0 is never used. */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* Matrix i of the pool. PC: the N64's PC_N64_MATRICES at gMatrices, then PC_MATRIX_SPILL more in pc_matrix_spill,
 * a range of the RDRAM window the PC reserves (pc_game_types.h, docs/port/MORECARS.md 8). Past those it says so
 * (pc_pool_overflow: a Debug build stops) and hands out the last one again. */
static tUVMatrix *pc_matrix_at(s32 i)
{
  if (i < PC_N64_MATRICES)
    return ((tUVMatrix *) &gMatrices) + i;
  if (i >= PC_MATRICES)
  {
    pc_pool_overflow("GetMatrix's matrices", i, PC_MATRICES - 1);
    i = PC_MATRICES - 1;
  }
  return ((tUVMatrix *) pc_matrix_spill) + (i - PC_N64_MATRICES);
}

/* The next matrix of the pool, set to the identity at the origin (gMaxMatrices keeps the high-water mark). */
void *ZOID_NewMatrix(void)
{
  tUVMatrix *mat;
  gNumMatrices = gNumMatrices + 1;
  if (gMaxMatrices < gNumMatrices)
  {
    gMaxMatrices = gNumMatrices;
  }
  mat = pc_matrix_at(gNumMatrices);
  mat->pos[0] = 0.0f;
  mat->pos[1] = 0.0f;
  mat->pos[2] = 0.0f;
  CopyUV(&unitmatrix, mat);
  return mat;
}
