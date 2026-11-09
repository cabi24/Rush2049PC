/* GetPasswordElement: take bits from the password being read (MAT3 password.c). */
#include "game/game.h"

/* Reads the next nbits bits of the password's data (password_gData, from bit gNumBits), lowest first, into *value.
 * Returns 0 without reading when nbits is over 32 or past gDataBits, else 1. */
s32 GetPasswordElement(s32 *value, u32 nbits)
{
  u16 i;
  if (nbits > 0x20)
  {
    return 0;
  }
  if (gDataBits < gNumBits + nbits)
  {
    return 0;
  }
  *value = 0;
  for (i = 0; i < nbits; i++, gNumBits++)
  {
    *value |= ((password_gData[gNumBits >> 3] >> (gNumBits & 7)) & 1) << i;
  }
  return 1;
}
