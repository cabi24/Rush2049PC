/* SaveSelectPlayer: the options when a profile is chosen (MAT3 save.c). */
#include "game/game.h"


VIEW(s8[0x15], gOptions);
#define gOptions gOptions_view

/* The first profile chosen gives the game its options (and the screen offset, options 19 and 20); every later one
 * takes the game's options into its file. */
void SaveSelectPlayer(tProfile **profile)
{
  s32 i;
  if (gGlobalsSet == 0)
  {
    gGlobalsSet = 1;
    for (i = 0; i < kNumGameOptions; i++) {
      gOptions[i] = GetPlayerOption(profile, i & 0xFF);
    }
    video_set_offset(gOptions[kOptScreenX], gOptions[kOptScreenY]);
    return;
  }
  for (i = 0; i < kNumGameOptions; i++) {
    SavePlayerOption(profile, i & 0xFF, gOptions[i]);
  }
}
