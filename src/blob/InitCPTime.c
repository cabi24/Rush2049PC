#include "game/game.h"

VIEW(u8, gNumActiveCars);
#define gNumActiveCars gNumActiveCars_view

/* On the PC the car records are pc_cars, with no N64 address (pc_game_types.h, docs/port/DECOUPLE.md). */

void InitCPTime(void)
{
  s32 i;

  gFirstPlaceTime = 0;
  gInFirstPlace = 0;
  for (i = 0; i < (s8) gNumActiveCars; i++) {
    *(f32 *) (PC_CARS + i * CAR_OFS(952) + 264) = 0.0f;
    PC_CP_LAST_POS_X(i) = *(f32 *) (PC_CARS + i * CAR_OFS(952) + 8);
    PC_CP_LAST_POS_Y(i) = *(f32 *) (PC_CARS + i * CAR_OFS(952) + 12);
    PC_CP_LAST_POS_Z(i) = *(f32 *) (PC_CARS + i * CAR_OFS(952) + 16);
  }
}
