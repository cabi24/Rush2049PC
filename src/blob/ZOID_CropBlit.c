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
VIEW(PadConfig[4], gBlitInstanceList);
#define gBlitInstanceList (gBlitInstanceList_view[0])

void ZOID_CropBlit(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
  if (arg1 < 0)
  {
    (&gBlitInstanceList)[arg0].unk18 = 0;
  }
  else
  {
    (&gBlitInstanceList)[arg0].unk18 = arg1;
  }
  if (arg3 < 0)
  {
    (&gBlitInstanceList)[arg0].unk1A = 0;
  }
  else
  {
    (&gBlitInstanceList)[arg0].unk1A = arg3;
  }
  if (arg2 < 0)
  {
    (&gBlitInstanceList)[arg0].unk1C = (&gBlitInstanceList)[arg0].unk12 - 1;
  }
  else
  {
    (&gBlitInstanceList)[arg0].unk1C = arg2;
  }
  if (arg4 < 0)
  {
    (&gBlitInstanceList)[arg0].unk1E = (&gBlitInstanceList)[arg0].unk10 - 1;
    return;
  }
  (&gBlitInstanceList)[arg0].unk1E = (s16) arg4;
}
