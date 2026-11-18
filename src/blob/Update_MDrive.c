/* Update_MDrive.c: the game thread's per-frame car update (MAT3 mdrive.c, Update_MDrive()).
 *
 * Copies the model thread's results into the car records (update_game_data), places the ghost cars of a ghost race,
 * and picks the camera views (CheckCameraView). */
#include "game/game.h"

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
      ZOID_UpdateObject(pc_cars[i].objnum, pc_cars[i].dr_pos, pc_cars[i].dr_car_uvs);
    }
  }
  CheckCameraView();
}
