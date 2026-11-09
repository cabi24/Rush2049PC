/* SkyUpdate.c: keep a window's sky dome on its camera (MAT3 sky.c, SkyUpdate()). */
#include "game/game.h"

/* A matrix of the persistent pool (MAT3 tUVMatrix, as its mat3 view): the rotation's three rows, then the position. */
typedef struct tUVMatrix { f32 uvs[3][3]; f32 pos[3]; } tUVMatrix;

/* Move window winidx's sky dome, if it has one, to its camera's x and z (the dome keeps its height), so the sky is
 * always around the camera. skyMatrices holds the dome's matrix as a pointer; the N64 kept its 32-bit address, which
 * the PC read through the RDRAM window until 2026-11-09 (docs/port/NATIVE.md step 4). */
void SkyUpdate(s32 winidx)
{
  tUVMatrix *sky;
  tCamera *camera;
  sky = skyMatrices[winidx];
  if (sky != NULL)
  {
    camera = &gCamera[winidx];
    sky->pos[0] = camera->mat.pos[0];
    sky->pos[2] = camera->mat.pos[2];
  }
}
