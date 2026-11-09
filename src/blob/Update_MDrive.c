/* Update_MDrive.c: the game thread's per-frame car update (MAT3 mdrive.c, Update_MDrive()).
 *
 * Copies the model thread's results into the car records (update_game_data), places the ghost cars of a ghost race,
 * and picks the camera views (CheckCameraView). */
#include "game/game.h"

/* The car record (tCarData): its scene node handle. objnum is an s32 the code reads the low half of as an s16:
 * the second half on the N64, the first on a little-endian host, so the PC view puts it first (docs/port/ENDIAN.md
 * 3.1). */
typedef struct tCarData {
    u8 pad0[0xF4];
    s16 objnum_low;             /* +0x0F4 the low half of objnum, the scene node */
} tCarData;

/* After update_game_data has copied the cars' states over, move the ghost cars' scene nodes (cars 1 on, the
 * recorded runs a ghost race replays) to their positions (dr_pos) and orientations (dr_car_uvs); then choose what
 * each view's camera shows. */
void Update_MDrive(void)
{
  s32 i;
  s32 j;   /* counts along with i; nothing reads it */

  update_game_data();
  if (gRace == kGhost)
  {
    for (i = 1, j = 0; i < gNumPlayers; i++, j++)
    {
      ZOID_UpdateObject(((tCarData *) PC_CAR(i))->objnum_low, (s8 *) PC_CAR_POS(i), (s8 *) PC_CAR_UVS50(i));
    }
  }
  CheckCameraView();
}
