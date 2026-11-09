#include "game/game.h"

VIEW(u8, gSplitScreen);
#define gSplitScreen gSplitScreen_view
VIEW(s32, vp0);
#define vp0 vp0_view
VIEW(s16[4], scissor0);
#define scissor0 scissor0_view

VIEW(ArbEntry[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

ArbEntry *ZOID_InitializeWindow(s32 idx, void *a, void *b, f32 c, f32 d, f32 e, f32 f, f32 g, f32 h);

void ZOID_ResetWindows(void) {
    ArbEntry *e;
    u16 h;
    u16 w;

    gNumZoidWindows = 1;
    gSplitScreen = 0;
    e = ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                 (f32)(screen_width / 2), (f32)(screen_height / 2));
    w = screen_width; h = screen_height;
    scissor0[0] = 0;
    scissor0[1] = 0;
    ZoidWinInfo[0].f0 = &vp0;
    ZoidWinInfo[0].f4 = scissor0;
    scissor0[3] = h;
    scissor0[2] = w;
    if (e) {}
}
