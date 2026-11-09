/* ZOID_GetWindow.c: a window's record (MAT3 zoidwindow.c, ZOID_GetWindow()). */
#include "game/game.h"

VIEW(s32, ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* A window (MAT3 tWindowInfo): 0x48 bytes on the N64, 0x50 on the PC, whose viewport and scissor pointers are 8
 * bytes each (docs/port/POINTERS.md 10). */
typedef struct tWindowInfo { void *vp, *s; u8 rest[0x48 - 8]; } tWindowInfo;

/* Window `winidx`'s record (ZoidWinInfo). */
void *ZOID_GetWindow(s32 winidx) {
    return &((tWindowInfo *) &ZoidWinInfo)[winidx];
}
