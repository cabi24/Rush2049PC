#include "game/game.h"

typedef struct PartSlot {
  u8 pad0[0x24]; f32 f24; f32 f28; f32 f2C; u8 pad30[0x60]; f32 f90; s8 f94; u8 pad95[3];
} PartSlot;
VIEW(s32, unitmatrix);
#define unitmatrix unitmatrix_view
VIEW(PartSlot[], gCamera);
#define gCamera gCamera_view

void ZeroCamera(s16 arg0)
{
s32 i = ((arg0 & 1) ? 0 : (arg0 & 2) ? 1 : (arg0 & 4) ? 2 : 3);
 gCamera[i].f94 = i;
 gCamera[i].f24 = 0.0f; gCamera[i].f28 = 0.0f; gCamera[i].f2C = 0.0f;
 CopyUV(&unitmatrix, &gCamera[i]);
 gCamera[i].f90 = 0.0f;
}
