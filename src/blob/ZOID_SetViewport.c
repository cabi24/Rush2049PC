/* ZOID_SetViewport.c: set a window's viewport and scissor rectangle (MAT3 zoidwindow.c, ZOID_SetViewport()). */
#include "game/game.h"

VIEW(s32[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* A window (MAT3 tWindowInfo): 0x48 bytes on the N64, 0x50 on the PC, whose viewport and scissor pointers are 8
 * bytes each (docs/port/POINTERS.md 10). */
typedef struct tWindowInfo { void *vp, *s; u8 rest[0x48 - 8]; } tWindowInfo;

/* Give window `winidx` the viewport `vp` (vp0 ...) and the scissor rectangle `s` (scissor0 ...). */
void ZOID_SetViewport(s32 winidx, void *vp, void *s) {
    tWindowInfo *win = &((tWindowInfo *) &ZoidWinInfo)[winidx];

    win->vp = vp;
    win->s = s;
}
