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
void BlinkResurrectingCars(void)
{
  GameCar *temp_a0;
  s16 var_v0;
  float new_var;
  unsigned int temp_f8;
  void *temp_v1;
  for (var_v0 = 0; var_v0 < PC_RACE_CARS; var_v0++)
  {
    temp_v1 = PC_MODEL(var_v0);
    if ((*((s16 *) (((s8 *) temp_v1) + 0x7C8))) != 0)
    {
      temp_a0 = PC_CAR(var_v0);
      if (((s8) temp_a0->pad0EC[0]) != 0)
      {
        if (((s8) PAD0EC(temp_a0->pad0EC, 0x21D)) == 0)
        {
          PC_EACH30(PAD0EC(temp_a0->pad0EC, 0x224) = ((s8) PAD0EC(temp_a0->pad0EC, 0x224)) - 1;)
          new_var = (*((f32 *) (((s8 *) temp_v1) + 0x714))) - (*((f32 *) (((s8 *) temp_a0) + CAR_OFS(0x30C))));
          if (((s8) PAD0EC(temp_a0->pad0EC, 0x224)) < 0)
          {
            PAD0EC(temp_a0->pad0EC, 0x225) = ((s8) PAD0EC(temp_a0->pad0EC, 0x225)) + 1;
            if (((s8) PAD0EC(temp_a0->pad0EC, 0x225)) & 1)
            {
              PAD0EC(temp_a0->pad0EC, 0x224) = 0;
              *((s32 *) (((s8 *) temp_v1) + 0x7D4)) = (s32) ((*((s32 *) (((s8 *) temp_v1) + 0x7D4))) & (~8));
            }
            else
            {
              temp_f8 = (s32) new_var;
              switch (temp_f8)
              {
                default:
                  PAD0EC(temp_a0->pad0EC, 0x224) = 0;
                  break;

                case 0:

                case 1:
                  PAD0EC(temp_a0->pad0EC, 0x224) = 2;
                  break;

                case 2:

                case 3:
                  PAD0EC(temp_a0->pad0EC, 0x224) = 1;
                  break;

              }

              if (((s8) PAD0EC(temp_a0->pad0EC, 0x26D)) <= 0)
              {
                *((s32 *) (((s8 *) temp_v1) + 0x7D4)) = (s32) ((*((s32 *) (((s8 *) temp_v1) + 0x7D4))) | 8);
              }
            }
          }
        }
        else
        {
          *((s32 *) (((s8 *) temp_v1) + 0x7D4)) = (s32) ((*((s32 *) (((s8 *) temp_v1) + 0x7D4))) & (~8));
        }
      }
    }
  }
}
