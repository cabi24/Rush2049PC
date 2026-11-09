#include "game/game.h"

/* PC: +0xF4 is a car's scene-node handle, an s32, and the s16 read is its low half: the second s16 on the N64, the
 * first on a little-endian host (docs/port/ENDIAN.md 3.1). */
#define NODE_LOW 0xF4
void Update_MDrive(void)
{
  s32 i;
  s32 j;

  update_game_data();
  if (gRace == 2)
  {
    for (i = 1, j = 0; i < gNumPlayers; i++, j++)
    {
      /* car i = j + 1: its scene node, position (MAT3 dr_pos) and orientation (dr_car_uvs) */
      ZOID_UpdateObject(*((s16 *) ((s8 *) PC_CAR(i) + NODE_LOW)), (s8 *) PC_CAR_POS(i), (s8 *) PC_CAR_UVS50(i));
    }
  }
  CheckCameraView();
}

