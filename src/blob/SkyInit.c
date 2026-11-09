/* SkyInit.c: make each window's sky dome (MAT3 sky.c, SkyInit()). */
#include "game/game.h"
/* PC: video.fog off keeps the sky dome with the track select's fog option on (docs/port/DETAIL.md 2.4): the N64 makes
 * none in a fog, whose colour is then the whole sky; without the fog that is a flat colour behind the world. */
#define PC_SKY_FOG(f) (pc_fog() == PC_FOG_OFF ? 0 : (f))

/* Give each window a sky dome: the sky model (sky_object_name, MAT3 "SKYSKY") as a scene node of its own matrix
 * (ZOID_NewMatrix), drawn in that window only (its flags hide it in the other three: bits 8-11 hide a node in
 * windows 0-3). None in a fog (the fog's colour is the sky) or on the tracks without one: 7-13 (the battle arenas
 * but the first), 17 (the fourth stunt arena) and 18 (the obstacle course). MAT3 scales the dome's matrix by 2.5;
 * the N64 leaves the new matrix as it is. */
void SkyInit(void)
{
  s32 i; struct tUVMatrix **dst; s32 handle; struct tUVMatrix *matrix;
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
    handle = MBOX_FindObject_WORK(sky_object_name, 0, gNumModels[0] - 1, 1);
    matrix = ZOID_NewMatrix();
    *dst = matrix;   /* a pointer (the N64 kept its 32-bit address) */
    ZOID_AddObject(handle, matrix, -1, (((s16) (1 << i)) << 8) ^ 0xF00);
    dst++;
  }
}
