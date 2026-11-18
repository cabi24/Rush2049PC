/* zoid_scale_windows.c: scale every window to another screen size (the N64's; no MAT3 counterpart). */
#include "game/game.h"

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
