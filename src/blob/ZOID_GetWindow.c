/* ZOID_GetWindow.c: a window's record (MAT3 zoidwindow.c, ZOID_GetWindow()). */
#include "game/game.h"

/* Window `winidx`'s record (ZoidWinInfo). */
tWindowInfo *ZOID_GetWindow(s32 winidx) {
    return ZoidWinInfo + winidx;
}
