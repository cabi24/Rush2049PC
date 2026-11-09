#include "game/game.h"

VIEW(s32, carTransGearRatio);
#define carTransGearRatio carTransGearRatio_view

/* PC: the record is the car's model (D_8014A250), which keeps its N64 layout: its car and tune pointers at +0 and +4
 * are u32 window addresses (docs/port/CARS.md 2.2), which m2c's `*(s8 **)` read as 8-byte host pointers. */
#define MODEL_AT(p, o) ((s8 *)PC_N64P(*((u32 *) ((p) + (o)))))
void transmission(s8 *arg0)
{
  /* carTransGearRatio, f32[][6]: a row per transmission (model +9), the gear (+0x3F4) from 0 in column 1 */
  f32 (*gear_ratio)[6] = (f32 (*)[6]) &carTransGearRatio;
  *((f32 *) (arg0 + 0x428)) = (*((f32 *) (MODEL_AT(arg0, 4) + 0xC))) * gear_ratio[*((s8 *) (arg0 + 9))][*((s16 *) (arg0 + 0x3F4)) + 1];
  *((f32 *) (arg0 + 0x42C)) = (*((f32 *) (MODEL_AT(arg0, 0) + 0xA4))) * (*((f32 *) (arg0 + 0x428)));
}

