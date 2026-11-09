#include "game/game.h"

typedef struct { s32 key; s32 a; s32 b; } E12;
/* PC: MBSetupWad's handle {Header *, s32, Entry *} with host pointers, as waves.c and loadtrack.c declare it. */
#define H12_BASE void *
typedef struct H12 { H12_BASE pad0; s32 count; E12 *tbl; } H12;
void *MBFindInWad(H12 *h, s32 arg1)
{
  s32 i;
  if (h == 0) {
    return 0;
  }
  for (i = 0; i < h->count; i++) {
    if (h->tbl[i].key == arg1) {
      return &h->tbl[i];
    }
  }
  return 0;
}
