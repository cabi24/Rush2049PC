/* MBFindInWad (MAT3 mb_wad.c): a WAD's table entry by tag (MBSetupWad.c has the WADs). */
#include "game/game.h"

/* The first entry of wad's table whose tag is tag, or 0 (also for a null wad). */
void *MBFindInWad(WadHandle *wad, s32 tag)
{
  s32 i;
  if (wad == 0) {
    return 0;
  }
  for (i = 0; i < wad->TOCentries; i++) {
    if (wad->TOC[i].tag == tag) {
      return &wad->TOC[i];
    }
  }
  return 0;
}
