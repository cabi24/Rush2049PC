#include "game/game.h"

typedef struct 
{
  u8 pad000[0xE8];
  s32 unkE8;
  u8 pad0EC[0x110 - 0xEC]; struct pc_visual visuals[21]; u8 pad0EC_308[0x35B - 0x308];
  u8 unk35B;
  u8 pad35C[0x380 - 0x35C];
  void *unk380;
  u8 pad384[0x3A3 - 0x384];
  u8 unk3A3;
  u8 pad3A4[0x3B8 - 0x3A4];
} GameCar;

/* GameCar's pad0EC[x] is the car's byte 0xEC + x; on the PC it is at CAR_OFS(0xEC + x) (docs/port/CARS.md). */
#define PAD0EC(a, x) (*((u8 *)(a) - 0xEC + CAR_OFS(0xEC + (x))))
void CheckCrash(s16 arg0)
{
  GameCar *temp_v0;
  s32 *temp_a1;
  temp_v0 = PC_CAR(arg0);
  switch ((s8) PAD0EC(temp_v0->pad0EC, 0x26C))
  {
    case 0:
      if (((s8) PAD0EC(temp_v0->pad0EC, 0x26D)) <= 0)
    {

      temp_a1 = PC_MODEL(arg0);
      if ((*((s8 *) (((s8 *) temp_a1) + 0x640))) != 0)
      {
        ((s8 *) temp_v0->pad0EC)[1] = -1;
        SetResurrectionData(2, temp_a1, 0);
        PAD0EC(temp_v0->pad0EC, 0x26C) = 1;
        *((f32 *) (((s8 *) temp_v0) + 0x10C)) = (f32) (*((f32 *) (((s8 *) temp_a1) + 0x714)));
      }
    }

    return;
    case 2:

    case 3:
      return;

    case 1:
      if (gRace == 6)
    {
      if (((PC_MODEL_MODEL_TIME(arg0)) - (*((f32 *) (((s8 *) temp_v0) + 0x10C)))) > 2.25f)
      {
        PAD0EC(temp_v0->pad0EC, 0x26C) = 2;
        return;
      }
    }
    else
      if (((PC_MODEL_MODEL_TIME(arg0)) - (*((f32 *) (((s8 *) temp_v0) + 0x10C)))) > 3.5f)
    {
      PAD0EC(temp_v0->pad0EC, 0x26C) = 2;
    }
      break;

  }

}
