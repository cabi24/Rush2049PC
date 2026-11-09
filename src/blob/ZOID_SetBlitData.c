#include "game/game.h"

typedef struct PadConfig_s
{
  struct PadConfig_s *unk0;
  struct PadConfig_s *unk4;
  s16 unk8;
  s16 unkA;
  s16 unkC;
  s16 unkE;
  s16 unk10;
  s16 unk12;
  u8 unk14;
  u8 unk15;
  u8 unk16;
  u8 pad17;
  s16 unk18;
  s16 unk1A;
  s16 unk1C;
  s16 unk1E;
} PadConfig;
VIEW(PadConfig[], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

void ZOID_SetBlitData(s32 arg0, s32 arg1, s32 arg2, unsigned short arg3)
{
  PadConfig *temp_v0;
  s16 temp_t6;
  temp_v0 = &(&gBlitInstanceList)[arg0];
  temp_t6 = arg3 & 0xFFFF;
  if (((s8) temp_v0->unk16) != 2)
  {
    temp_v0->unk16 = arg1 != 0;
    if (arg2 >= 0)
    {
      temp_v0->unk14 = (u8) arg2;
    }
    if (arg3 != 0xFFFF)
    {
      temp_v0->unkE = temp_t6;
    }
  }
}

