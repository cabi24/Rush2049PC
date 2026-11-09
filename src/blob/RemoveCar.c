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

VIEW(s32, gCarParts);
#define gCarParts gCarParts_view

void RemoveCar(s16 arg0)
{
  GameCar *temp_s2;
  M2C_UNK (*temp_v0)(u8 *, M2C_UNK);
  s16 temp_a0;
  s16 var_s1;
  u8 *var_s0;
  int new_var;
 do { temp_s2 = PC_CAR(arg0); if ((*((s32 *) (((s8 *) temp_s2) + 0xF4))) >= 0) { MBOX_HideObject((*((s32 *) (((u8 *) (&gCarParts)) + (arg0 << 6)))), 2, 0xF); var_s0 = &temp_s2->pad0EC[0x24]; var_s1 = 0; do { temp_v0 = *((M2C_UNK (**)(u8 *, M2C_UNK)) (((s8 *) var_s0) + VIS_OFS(0x14))); if (temp_v0 != ((void *) 0)) { temp_v0(var_s0, 0); } else { temp_a0 = *((s16 *) (((s8 *) var_s0) + VIS_OFS(6))); if (temp_a0 >= 0) { MBOX_HideObject(temp_a0, 1, 0xF); *((s16 *) (((s8 *) var_s0) + VIS_OFS(6))) = -(new_var = 1); } } var_s1 += 1; var_s0 += VIS_OFS(0x18); } while (var_s1 < 0x15); } } while (0);
}
