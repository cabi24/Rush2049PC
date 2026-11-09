#include "game/game.h"

VIEW(s8[0x15], gOptions);
#define gOptions gOptions_view

s32 SavePlayerOption();

s32 GetPlayerOption();

void SaveSelectPlayer(intptr_t arg0)
{
  s32 i;
  if (gGlobalsSet == 0)
  {
    gGlobalsSet = 1;
    for (i = 0; i < 0x15; i++) {
      gOptions[i] = GetPlayerOption(arg0, i & 0xFF);
    }
    video_set_offset(gOptions[0x13], gOptions[0x14]);
    return;
  }
  for (i = 0; i < 0x15; i++) {
    SavePlayerOption(arg0, i & 0xFF, gOptions[i]);
  }
}
