/* ZOID_GetFog.c: a window's fog (MAT3 zoidwindow.c, ZOID_GetFog()). */
#include "game/game.h"

/* A window (MAT3 tWindowInfo): 0x48 bytes on the N64, 0x50 on the PC, whose viewport and scissor pointers are 8
 * bytes each (docs/port/POINTERS.md 10); the fog at its end. */
typedef struct tWindowInfo { void *vp, *s; u8 pad08[0x40 - 8]; u16 fogmin, fogmax; u8 fogcolor[4]; } tWindowInfo;

/* Window `winidx`'s fog: its range (the fog starts at depth *min and is full at *max, in the RDP's depth units) and
 * its colour. (MAT3 returns the window; the N64's returns nothing.) */
void ZOID_GetFog(u16 *min, u16 *max, u8 *red, u8 *green, u8 *blue, u8 *alpha, s32 winidx)
{
  tWindowInfo *win = &((tWindowInfo *) &ZoidWinInfo)[winidx];
  *min = win->fogmin;
  *max = win->fogmax;
  *red = win->fogcolor[0];
  *green = win->fogcolor[1];
  *blue = win->fogcolor[2];
  *alpha = win->fogcolor[3];
}
