/* ZOID_SetViewport.c: set a window's viewport and scissor rectangle (MAT3 zoidwindow.c, ZOID_SetViewport()). */
#include "game/game.h"

/* Give window `winidx` the viewport `vp` (vp0 ...) and the scissor rectangle `s` (scissor0 ...). */
void ZOID_SetViewport(s32 winidx, void *vp, void *s) {
    tWindowInfo *win = ZoidWinInfo + winidx;

    win->vp = vp;
    win->s = s;
}
