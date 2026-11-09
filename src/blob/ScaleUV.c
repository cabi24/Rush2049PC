/* ScaleUV.c: scale a 3x3 matrix (MAT3 fmath.c, ScaleUV()). */
#include "game/game.h"

/* dst = src times `scale`, element by element (row by row; src and dst may be the same matrix). */
void ScaleUV(float src[3][3], float dst[3][3], float scale)
{
 int i, j;
 for (i = 0; i < 3; i++) {
  for (j = 0; j < 3; j++) {
   dst[i][j] = src[i][j] * scale;
  }
 }
}
