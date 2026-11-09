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

VIEW(u8, gDeathMode);
#define gDeathMode gDeathMode_view

typedef struct ModelRecord 
{
  u8 pad000[0x7C6];
  u16 unk7C6;
  u8 pad7C8[0x7E8 - 0x7C8];
  u8 unk7E8;
} ModelRecord;

/* GameCar's pad0EC[x] is the car's byte 0xEC + x; on the PC it is at CAR_OFS(0xEC + x) (docs/port/CARS.md). */
#define PAD0EC(a, x) (*((u8 *)(a) - 0xEC + CAR_OFS(0xEC + (x))))
void AbortCar(s16 arg0, s32 arg1)
{
  GameCar *temp_v0_2;
  ModelRecord *new_var;
  int new_var2;
  s32 temp_v0;
  if (!(gModelTime < 5.0f))
  {
    ;
    new_var2 = -1U;
    new_var = PC_MODELS;
    if (gRace == 4)
    {
      CrashedStunt((s32) arg0);
    }
    if (gRace == 6)
    {
      InitBattlePlayer(arg0);
    }
    SetResurrectionData(2, (ModelRecord *) ((arg0 * 0x808) + ((u8 *) new_var)), temp_v0 = 1);
    temp_v0_2 = PC_CAR(arg0);
    PAD0EC(temp_v0_2->pad0EC, 0x26C) = 2;
    if ((((s8) gDeathMode) != 0) && (arg1 != 0))
    {
      PAD0EC(temp_v0_2->pad0EC, 0x26D) = 0;
      return;
    }
    PAD0EC(temp_v0_2->pad0EC, 0x26D) = new_var2;
  }
}
