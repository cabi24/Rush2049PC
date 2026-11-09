/* mirror_vertex_texcoords: prepares a model's vertices for a mirrored track, once (relocate_model_dl calls it for a
 * display list's first vertex load). N64 only. */
#include "game/game.h"

/* The game's use of a vertex's flag word (Vtx_t.flag, which the RSP ignores). */
#define VTX_FLAG_MORE 0x8000        /* another vertex of the run follows */
#define VTX_FLAG_DONE 0x4000        /* already prepared */
#define VTX_FLAG_S_SPAN 0x1FF       /* the texture's width the S coordinate mirrors over, in texels */

/* Walks the run of vertices at window address vertices (to the first without VTX_FLAG_MORE) and marks each one done;
 * when mirrored is set, a vertex not yet done gets its S texture coordinate mirrored over its span (span * 32 - s,
 * in the 10.5 fixed point of Vtx_t.tc).
 * PC: the vertex list is ROM data in N64 form (big-endian Vtx), at the N64 address the relocation computed. */
void mirror_vertex_texcoords(s32 vertices, s32 mirrored)
{
  u8 *v = (u8 *) pc_n64_ptr(((u32) vertices) | 0x80000000);
  u16 flags;
  s32 span;
  do
  {
    flags = pc_be16_get(v + 6);
    if (!(flags & VTX_FLAG_DONE))
    {
      pc_be16_set(v + 6, (u16) (flags | VTX_FLAG_DONE));
      span = flags & VTX_FLAG_S_SPAN;
      if ((mirrored != 0) && (span != 0))
      {
        pc_be16_set(v + 8, (u16) (((span << 5) - ((s16) pc_be16_get(v + 8))) & 0xFFFF));
      }
    }
    v += 0x10;
  }
  while (flags & VTX_FLAG_MORE);
}
