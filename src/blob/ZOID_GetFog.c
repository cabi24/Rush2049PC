#include "game/game.h"

VIEW(volatile char, ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* PC: a ZOID window (ZoidWinInfo) is 80 bytes, its first and bounds pointers 8 bytes each (docs/port/POINTERS.md
 * 10); the m2c body uses the N64 stride and offsets. Reads window arg6's fog range and colour. */
typedef struct { void *first, *bounds; u8 pad08[0x40 - 8]; u16 range_start, range_end; u8 red, green, blue, alpha; }
    PcWindow;
void ZOID_GetFog(u16 *arg0, u16 *arg1, u8 *arg2, u8 *arg3, u8 *arg4, u8 *arg5, s32 arg6)
{
  PcWindow *w = &((PcWindow *) &ZoidWinInfo)[arg6];
  *arg0 = w->range_start;
  *arg1 = w->range_end;
  *arg2 = w->red;
  *arg3 = w->green;
  *arg4 = w->blue;
  *arg5 = w->alpha;
}
