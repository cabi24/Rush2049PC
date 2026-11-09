/* PickCar.c: which object definition a car's frame shows (MAT3 visuals.c, PickCar()). */
#include "game/game.h"

VIEW(u16, gObjList);
#define gObjList gObjList_view

/* The frame's object definition for car slot `slot` with body `type`: the burnt-out wreck for body 13 (a crashed
 * car, SetupCar), else the slot's own frame. */
u16 PickCar(s16 slot, u8 type)
{
  u16 *objects = (u16 *) &gObjList;
  if (type == 13)
  {
    return objects[OBJLIST_HULK];
  }
  return objects[slot * 3];
}
