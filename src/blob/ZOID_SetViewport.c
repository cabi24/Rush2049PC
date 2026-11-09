#include "game/game.h"

VIEW(s32[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* PC: a ZOID window (ZoidWinInfo) is 80 bytes, its first and bounds pointers 8 bytes each (docs/port/POINTERS.md
 * 10); the m2c body uses the N64 stride. Stores a window's first and bounds. */
typedef struct { void *first, *bounds; u8 rest[0x48 - 8]; } PcWindow;
void ZOID_SetViewport(s32 arg0, void *arg1, void *arg2) {
    PcWindow *w = &((PcWindow *) &ZoidWinInfo)[arg0];

    w->first = arg1;
    w->bounds = arg2;
}
