#include "game/game.h"

/* PC: the vertex list is ROM data in N64 form (big-endian Vtx), at the N64 address the relocation computed. */
void mirror_vertex_texcoords(s32 arg0, s32 arg1)
{
  u8 *v = (u8 *) pc_n64_ptr(((u32) arg0) | 0x80000000);
  u16 flags;
  s32 s;
  do
  {
    flags = pc_be16_get(v + 6);
    if (!(flags & 0x4000))
    {
      pc_be16_set(v + 6, (u16) (flags | 0x4000));
      s = flags & 0x1FF;
      if ((arg1 != 0) && (s != 0))
      {
        pc_be16_set(v + 8, (u16) (((s << 5) - ((s16) pc_be16_get(v + 8))) & 0xFFFF));
      }
    }
    v += 0x10;
  }
  while (flags & 0x8000);
}
