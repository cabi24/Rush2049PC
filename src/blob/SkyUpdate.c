/* SkyUpdate.c: keep a window's sky dome on its camera (MAT3 sky.c, SkyUpdate()). */
#include "game/game.h"



/* skyMatrices holds each window's camera matrix (ZOID_NewMatrix's pool in the game arena) as a 32-bit RDRAM-window
 * address: zero-extend it into a pointer (docs/port/CASTS.md). */
#define MTX_WORD u32
/* The x and z of the sky's matrix's position (MAT3 tUVMatrix mat3.pos, after the 3x3 orientation), by byte
 * offset. */
#define POS_X(p) (*((f32 *) (((s8 *) (p)) + 0x24)))
#define POS_Z(p) (*((f32 *) (((s8 *) (p)) + 0x2C)))

/* Move window winidx's sky dome, if it has one, to its camera's x and z (the dome keeps its height), so the sky is
 * always around the camera. */
void SkyUpdate(s32 winidx)
{
  MTX_WORD *slot;
  MTX_WORD sky;
  tCamera *camera;
  slot = (MTX_WORD *)&skyMatrices[winidx];
  sky = *slot;
  if (sky != 0)
  {
    camera = &gCamera[winidx];
    POS_X(((s8 *)PC_N64P(sky))) = camera->mat.pos[0];
    POS_Z(((s8 *)PC_N64P((*slot)))) = camera->mat.pos[2];
  }
}
