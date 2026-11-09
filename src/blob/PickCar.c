#include "game/game.h"

VIEW(u16, gObjList);
#define gObjList gObjList_view

u16 PickCar(s16 arg0, u8 arg1)
{
  u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind (u16[]) */
  if (arg1 == 0xD)
  {
    return objects[157];   /* the burnt-out car (hulk) */
  }
  return objects[arg0 * 3];
}

