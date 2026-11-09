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

VIEW(s32[], gCarParts);
#define gCarParts gCarParts_view

VIEW(u16, gObjList);
#define gObjList gObjList_view

void StartFrame(s16 arg0)
{
  GameCar *temp_s1;
  void *temp_s2;
  u16 *objects = (u16 *) &gObjList;   /* the object definitions by kind (u16[]): the rocket's parts at 213..215 */
  temp_s2 = (s32 *) ((arg0 << 6) + ((u8 *) (&gCarParts)));
  temp_s1 = PC_CAR(arg0);
  *((intptr_t *) (((s8 *) temp_s1) + CAR_OFS(0x274))) = (intptr_t)AnimateFrame;
  *((s16 *) (((s8 *) temp_s1) + CAR_OFS(0x266))) = (s16) (*((s32 *) (((s8 *) temp_s2) + 0)));
  *((s16 *) (((s8 *) temp_s1) + CAR_OFS(0x268))) = arg0;
  MBOX_ShowObject(*((s16 *) (((s8 *) temp_s1) + CAR_OFS(0x266))), 0, 0xF);
  if ((PC_MODEL_BODY(arg0)) == 2)
  {
    *((s32 *) (((s8 *) temp_s2) + 8)) = InitCarObj(objects[214], 0xF, -1, 0x42000, (s32) arg0, -1, 1);
    *((f32 *) (((s8 *) temp_s1) + CAR_OFS(0x378))) = 0.25f;
    *((s32 *) (((s8 *) temp_s2) + 0xC)) = InitCarObj(objects[215], 0xF, -1, 0x42000, (s32) arg0, -1, 1);
    *((f32 *) (((s8 *) temp_s1) + CAR_OFS(0x37C))) = 0.5f;
    *((s32 *) (((s8 *) temp_s2) + 4)) = InitCarObj(objects[213], 0xF, -1, 0x42000, (s32) arg0, -1, 1);
    *((f32 *) (((s8 *) temp_s1) + CAR_OFS(0x374))) = 0.25f;
  }
  if (((gRace == 6) || (gRace == 4)) && (gNumPlayers >= 3))
  {
    MBOX_HideObject(*((s32 *) (((s8 *) temp_s2) + 0x14)), 1, 0xF);
  }
}
