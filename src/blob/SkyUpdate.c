/* SkyUpdate.c: keep a window's sky dome on its camera (MAT3 sky.c, SkyUpdate()). */
#include "game/game.h"

VIEW(s32, skyMatrices);
#define skyMatrices skyMatrices_view

VIEW(unsigned char, gCamera);
#define gCamera gCamera_view

/* skyMatrices holds each window's camera matrix (ZOID_NewMatrix's pool in the game arena) as a 32-bit RDRAM-window
 * address: zero-extend it into a pointer (docs/port/CASTS.md). */
#define MTX_WORD u32
/* The x and z of a place's position (MAT3 tUVMatrix mat3.pos, after the 3x3 orientation), by byte offset as the
 * decompiled code reaches them, in the sky's matrix and in the camera (gCamera[].position, 0x98 bytes a camera). */
#define POS_X(p) (*((f32 *) (((s8 *) (p)) + 0x24)))
#define POS_Z(p) (*((f32 *) (((s8 *) (p)) + 0x2C)))

/* Move window winidx's sky dome, if it has one, to its camera's x and z (the dome keeps its height), so the sky is
 * always around the camera. */
void SkyUpdate(s32 winidx)
{
  MTX_WORD *slot;
  MTX_WORD sky;
  void *camera;
  slot = &(&skyMatrices)[winidx];
  sky = *slot;
  if (sky != 0)
  {
    camera = (winidx * 0x98) + (&gCamera);
    POS_X(((s8 *)PC_N64P(sky))) = (f32) POS_X(camera);
    POS_Z(((s8 *)PC_N64P((*slot)))) = (f32) POS_Z(camera);
  }
}
