#include "game/game.h"

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view

VIEW(s32[], gCamera);
#define gCamera gCamera_view

typedef struct { u8 pad0[0x24]; f32 f24, f28, f2C; u8 pad30[4]; f32 f34, f38, f3C; u8 pad40[8]; f32 f48; u8 pad4C[8]; f32 f54, f58, f5C; u8 pad60[0x38]; } EmitA;
typedef struct { u8 pad0[0x98]; } EmitB;

void UpdateCameras(void)
{
  s32 i;
  EmitA *e;
  EmitB *b;

  for (i = 0; i < (s16) gNumWindows; i++) {
    e = (EmitA *) &gCamera + i;
    b = (EmitB *) ((u8 *) e + 0x30);   /* the camera's handedness-flipped copy of its matrix (gCamera[i].n64, +0x30) */
    e->f54 = e->f24;
    e->f58 = e->f28;
    e->f5C = e->f2C;
    CopyUV(e, b);
    e->f34 = -e->f34;
    e->f38 = -e->f38;
    e->f3C = -e->f3C;
    e->f48 = -e->f48;
    if (gPauseState == 0) {
      SkyUpdate(i);
    }
  }
  UpdateHUDPostCamera();
  ShowCountdown(gCountDown > 0);
}
