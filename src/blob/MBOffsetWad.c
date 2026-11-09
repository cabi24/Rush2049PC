#include "game/game.h"

/* PC: the same steps on MBSetupWad's handle as the PC build lays it out ({u32 *base; s32 count; void *TOC} with
 * host pointers; the N64 code addresses it at +0/+4/+8). The WAD is in the RDRAM window, so its 32-bit address
 * widens to the host pointer. As on the N64, the base is arg1 + arg2. */
typedef struct { s32 *base; s32 count; s32 *toc; } WadHandle;
void MBOffsetWad(void *arg0, s32 arg1, s32 arg2)
{
  WadHandle *wad = (WadHandle *) arg0;
  s32 *base = (s32 *) PC_N64P((u32) (arg1 + arg2));
  s32 i;
  if (wad != ((void *) 0))
  {
    wad->base = base;
    base[0] += arg2;
    wad->toc = (s32 *) PC_N64P((u32) base[0]);
    wad->count = base[1];
    for (i = 0; i < wad->count; i++)
    {
      wad->toc[(i * 3) + 1] += arg2;
    }
  }
}
