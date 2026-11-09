#include "game/game.h"

VIEW(s32, unitmatrix);
#define unitmatrix unitmatrix_view

typedef struct { u8 pad[0x24]; f32 f24; f32 f28; f32 f2C; } E30;
/* PC: the N64's PC_N64_MATRICES at gMatrices, then PC_MATRIX_SPILL more in pc_matrix_spill, a range of the RDRAM
 * window the PC reserves (pc_game_types.h, docs/port/MORECARS.md 8). Past those it says so (pc_pool_overflow: a
 * Debug build stops) and hands out the last one again. */
static E30 *pc_matrix_at(s32 i)
{
  if (i < PC_N64_MATRICES)
    return ((E30 *) &gMatrices) + i;
  if (i >= PC_MATRICES)
  {
    pc_pool_overflow("GetMatrix's matrices", i, PC_MATRICES - 1);
    i = PC_MATRICES - 1;
  }
  return ((E30 *) pc_matrix_spill) + (i - PC_N64_MATRICES);
}
#define PC_MATRIX_AT(i) pc_matrix_at(i)
void *ZOID_NewMatrix(void)
{
  E30 *e;
  gNumMatrices = gNumMatrices + 1;
  if (gMaxMatrices < gNumMatrices)
  {
    gMaxMatrices = gNumMatrices;
  }
  e = PC_MATRIX_AT(gNumMatrices);
  e->f24 = 0.0f;
  e->f28 = 0.0f;
  e->f2C = 0.0f;
  CopyUV(&unitmatrix, e);
  return e;
}
