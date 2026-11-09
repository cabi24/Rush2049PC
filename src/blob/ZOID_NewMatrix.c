/* ZOID_NewMatrix.c: hand out a persistent matrix for a scene node (MAT3 zoid.c, ZOID_NewMatrix()).
 *
 * Matrices come from a pool that ZOID_Reset empties: index 0 is never used. */
#include "game/game.h"

/* A matrix (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* The pool's matrices past the N64's PC_N64_MATRICES (pc_game_types.h, docs/port/MORECARS.md 8): PC storage. Until
 * 2026-11-09 the PC reserved them a range of the RDRAM window, since each window's sky matrix was kept as a 32-bit
 * address (skyMatrices, pointers since: MORECARS.md 2.7). */
float pc_matrix_spill[PC_MATRIX_SPILL][12] __attribute__((aligned(16)));

/* Matrix i of the pool. PC: the N64's PC_N64_MATRICES at gMatrices, then PC_MATRIX_SPILL more in pc_matrix_spill.
 * Past those it says so (pc_pool_overflow: a Debug build stops) and hands out the last one again. */
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
