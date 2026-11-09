/* StartFrame.c: start a car's frame visual (MAT3 visuals.c, StartFrame()). */
#include "game/game.h"

/* The car record (tCarData, N64 layout: docs/port/CARS.md 2), the fields StartFrame sets. */
typedef struct tCarData
{
  u8 pad000[0x110]; struct pc_visual visuals[21]; u8 pad308[0x374 - 0x308];
  f32 roktScale[3];             /* +0x374 the scale of the rocket car's three flames */
} tCarData;

/* gCarParts[slot]: the car's scene objects (MAT3 CarParts, the N64's 0x40 bytes: CreateCar.c). */
typedef struct CarParts {
  s32 frame;                    /* 0x00 */
  s32 extraparts[3];            /* 0x04 the rocket car's flames */
  s32 hood;                     /* 0x10 */
  s32 sheen;                    /* 0x14 */
  u8 pad18[0x40 - 0x18];
} CarParts;

VIEW(CarParts[], gCarParts);
#define gCarParts gCarParts_view

VIEW(u16, gObjList);
#define gObjList gObjList_view

/* Give car `slot`'s frame its visual (VIS_FRAME, animated by AnimateFrame: the body, its colour and the rocket's
 * flames) and show it. The rocket car (body 2) gets its three flame objects, with their starting scales. In a
 * battle or a stunt race (gRace 6 kBattle, 4 kStunt) with three or four players the sheen is hidden, a cost the
 * N64 saves (MAT3 has no such test). */
void StartFrame(s16 slot)
{
  tCarData *car;
  CarParts *parts;
  u16 *objects = (u16 *) &gObjList;
  parts = &gCarParts[slot];
  car = PC_CAR(slot);
  car->visuals[VIS_FRAME].func = AnimateFrame;
  car->visuals[VIS_FRAME].objnum = (s16) parts->frame;
  car->visuals[VIS_FRAME].slot = slot;
  MBOX_ShowObject(car->visuals[VIS_FRAME].objnum, 0, 15);
  if ((PC_MODEL_BODY(slot)) == 2)
  {
    parts->extraparts[1] = InitCarObj(objects[OBJLIST_ROCKET_FLAME + 1], 15, -1, 0x42000, slot, -1, 1);
    car->roktScale[1] = 0.25f;
    parts->extraparts[2] = InitCarObj(objects[OBJLIST_ROCKET_FLAME + 2], 15, -1, 0x42000, slot, -1, 1);
    car->roktScale[2] = 0.5f;
    parts->extraparts[0] = InitCarObj(objects[OBJLIST_ROCKET_FLAME], 15, -1, 0x42000, slot, -1, 1);
    car->roktScale[0] = 0.25f;
  }
  if (((gRace == kBattle) || (gRace == kStunt)) && (gNumPlayers >= 3))
  {
    MBOX_HideObject(parts->sheen, 1, 15);
  }
}
