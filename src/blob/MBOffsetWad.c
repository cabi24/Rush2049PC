/* MBOffsetWad (MAT3 mb_wad.c): rebases a WAD the heap has moved (MBSetupWad.c has the WADs). */
#include "game/game.h"

/* Fills wad for the WAD now at window address oldBase + offset, whose table and entries still hold addresses for
 * oldBase: adds offset to the table's address and to every entry's. */
void MBOffsetWad(WadHandle *wad, s32 oldBase, s32 offset)
{
  WadFileHeader *file = (WadFileHeader *) PC_N64P((u32) (oldBase + offset));
  s32 i;
  if (wad != ((void *) 0))
  {
    wad->base = file;
    file->toc += offset;
    wad->TOC = (WadEntry *) PC_N64P((u32) file->toc);
    wad->TOCentries = file->TOCentries;
    for (i = 0; i < wad->TOCentries; i++)
    {
      wad->TOC[i].address += offset;
    }
  }
}
