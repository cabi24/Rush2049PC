/* zoid_set_window_count.c: say how many windows the scene has (the N64's; no MAT3 counterpart). */
#include "game/game.h"

VIEW(u8, gSplitScreen);
#define gSplitScreen gSplitScreen_view

/* Use `count` windows from now on, set up elsewhere, with no split-screen layout (gSplitScreen 0), and return it;
 * a count of 0 or less changes nothing and returns the current count. */
s32 zoid_set_window_count(s32 count) {
    if (count <= 0) return gNumZoidWindows;
    gNumZoidWindows = count;
    gSplitScreen = 0;
    return count;
}
