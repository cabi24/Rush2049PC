/* game_strcmp.c: compare two strings (the game's copy of the C library's strcmp). */
#include "game/game.h"

/* strcmp: less than, equal to or greater than zero as the string s1 sorts before, equal to or after s2, comparing
 * unsigned bytes. */
s32 game_strcmp(u8 *s1, u8 *s2)
{
  int c;
  c = *s1;
  if (c != 0 && c == *s2)
  {
    do
    {
      c = s1[1];
      s1++;
      s2++;
    } while (c != 0 && c == *s2);
  }
  return c - *s2;
}
