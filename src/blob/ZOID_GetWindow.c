#include "game/game.h"

VIEW(s32, ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* PC: a ZOID window (ZoidWinInfo) is 80 bytes, its first and bounds pointers 8 bytes each (docs/port/POINTERS.md
 * 10); the m2c body uses the N64 stride. Returns window arg0. */
typedef struct { void *first, *bounds; u8 rest[0x48 - 8]; } PcWindow;
void *ZOID_GetWindow(s32 arg0) {
    return &((PcWindow *) &ZoidWinInfo)[arg0];
}
