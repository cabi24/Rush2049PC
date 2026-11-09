#include "game/game.h"

void LoadTextWave(void)
{
  if (gLang != gTextWave)
  {
    ReplaceWave((s32) gTextWave, (s32) gLang);
    gTextWave = (u8) gLang;
    AdjustTextWavePointers();
  }
}
