/* TB_CenterText: where to start a string to centre it (MAT3 TB_text.c). */
#include "game/game.h"

/* The x at which text, drawn in the current font, is centred on centerX. */
s16 TB_CenterText(void *text, s16 centerX)
{
  unsigned int width;
  width = HAL_GetTextWidth(text, -1);
  return (s16) (centerX - (width >> 1));
}
