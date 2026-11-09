/* blit_mask_bits: a texture tile's mask size (the N64 side of MAT3 blit_pc.c). */
#include "game/game.h"

/* The number of address bits the RDP needs for a texture dimension of size texels (its sign ignored): the smallest n
 * with 2^n >= size, at most 10 (1024). blit_load_texture passes it as a tile's mask. */
s32 blit_mask_bits(s32 size)
{
  if (size < 0)
  {
    size = -size;
  }
  if (size >= 0x201)
  {
    return 10;
  }
  if (size >= 0x101)
  {
    return 9;
  }
  if (size >= 0x81)
  {
    return 8;
  }
  if (size >= 0x41)
  {
    return 7;
  }
  if (size >= 0x21)
  {
    return 6;
  }
  if (size >= 0x11)
  {
    return 5;
  }
  if (size >= 9)
  {
    return 4;
  }
  if (size >= 5)
  {
    return 3;
  }
  if (size >= 3)
  {
    return 2;
  }
  if (size >= 2)
  {
    return 1;
  }
  return 0;
}

