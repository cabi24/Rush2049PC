#include "game/game.h"

VIEW(u8, gSplitScreen);
#define gSplitScreen gSplitScreen_view
s32 zoid_set_window_count(s32 value) {
 if(value<=0)return gNumZoidWindows;
 gNumZoidWindows=value;gSplitScreen=0;return value;
}
