/* zoid_scale_windows.c: scale every window to another screen size (the N64's; no MAT3 counterpart). */
#include "game/game.h"

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes): its scissor
 * rectangle, size and centre. */
typedef struct tWindowInfo {
    void *vp;
    u16 *s;                     /* the scissor rectangle: left, top, right, bottom */
    u8 pad8[28];
    f32 wwidth, wheight;        /* +0x2C the size, pixels */
    f32 whcenter, wvcenter;     /* the centre, pixels */
    u8 pad52[20];
} tWindowInfo;

VIEW(tWindowInfo[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* Scale each window in use (gNumZoidWindows) by x horizontally and y vertically: its scissor rectangle, its size
 * and its centre (the field of view stays). Used when the screen's resolution changes under the layout. */
void zoid_scale_windows(f32 x, f32 y) {
    s32 i;
    tWindowInfo *win;
    for (i = 0; i < gNumZoidWindows; i++) {
        win = &ZoidWinInfo[i];
        win->s[0] = (u32)((f32)(u32)win->s[0] * x);
        win->s[2] = (u32)((f32)(u32)win->s[2] * x);
        win->wwidth *= x;
        win->whcenter *= x;
        win->s[1] = (u32)((f32)(u32)win->s[1] * y);
        win->s[3] = (u32)((f32)(u32)win->s[3] * y);
        win->wheight *= y;
        win->wvcenter *= y;
    }
}
