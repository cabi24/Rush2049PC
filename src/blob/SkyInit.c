#include "game/game.h"

VIEW(u16, gNumWindows);
#define gNumWindows gNumWindows_view

VIEW(volatile char, gNumModels);
#define gNumModels gNumModels_view

s32 ZOID_AddObject();

/* PC: video.fog off keeps the sky dome with the track select's fog option on (docs/port/DETAIL.md 2.4): the N64 makes
 * none in a fog, whose colour is then the whole sky; without the fog that is a flat colour behind the world. */
#define PC_SKY_FOG(f) (pc_fog() == PC_FOG_OFF ? 0 : (f))
/* A window's camera matrix is kept as its 32-bit N64 address (skyMatrices): N64A(p) is a window pointer's
 * (pc_prelude.h, docs/port/NOFIXED.md 2.3). */
#define N64A(x) PC_N64A(x)
void SkyInit(void)
{
  s32 i; s32 *dst; s32 handle; intptr_t val;
  if (PC_SKY_FOG(gFogIntensity) > 0) {
    SkyClear();
    return;
  }
  if (gTrackNum == 7 || gTrackNum == 8 || gTrackNum == 9 || gTrackNum == 0xA ||
      gTrackNum == 0xB || gTrackNum == 0xC || gTrackNum == 0xD ||
      gTrackNum == 0x11 || gTrackNum == 0x12) {
    SkyClear();
    return;
  }
  dst = skyMatrices;
  for (i = 0; i < (s16) gNumWindows; i++) {
    handle = MBOX_FindObject_WORK(sky_object_name, 0, gNumModels - 1, 1);
    val = ZOID_NewMatrix();
    *dst = N64A(val);
    ZOID_AddObject(handle, val, -1, (((s16) (1 << i)) << 8) ^ 0xF00);
    dst++;
  }
}
