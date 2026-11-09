#include "game/game.h"

VIEW(s32, skyMatrices);
#define skyMatrices skyMatrices_view

VIEW(unsigned char, gCamera);
#define gCamera gCamera_view

/* skyMatrices holds each window's camera matrix (ZOID_NewMatrix's pool in the game arena) as a 32-bit RDRAM-window
 * address: zero-extend it into a pointer (docs/port/CASTS.md). */
#define MTX_WORD u32
/* ... and N64PTR(T, a) is the pointer at one (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64PTR(T, a) ((T)PC_N64P(a))
void SkyUpdate(s32 arg0)
{
  MTX_WORD *temp_v0;
  MTX_WORD temp_v1;
  void *temp_a1;
  temp_v0 = &(&skyMatrices)[arg0];
  temp_v1 = *temp_v0;
  if (temp_v1 != 0)
  {
    temp_a1 = (arg0 * 0x98) + (&gCamera);
    *((f32 *) ((N64PTR(s8 *, temp_v1)) + 0x24)) = (f32) (*((f32 *) (((s8 *) temp_a1) + 0x24)));
    *((f32 *) ((N64PTR(s8 *, (*temp_v0))) + 0x2C)) = (f32) (*((f32 *) (((s8 *) temp_a1) + 0x2C)));
  }
}
