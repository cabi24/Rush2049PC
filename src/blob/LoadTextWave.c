/* LoadTextWave: load the strings of the current language (MAT3 text.c). */
#include "game/game.h"

/* Loads the strings of language gLang over the text wave when another language is loaded, and points gText's
 * tables into them again. The language number is its wave's number. */
void LoadTextWave(void)
{
  if (gLang != gTextWave)
  {
    ReplaceWave((s32) gTextWave, (s32) gLang);
    gTextWave = (u8) gLang;
    AdjustTextWavePointers();
  }
}
