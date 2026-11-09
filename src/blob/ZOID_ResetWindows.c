/* ZOID_ResetWindows.c: back to one full-screen window (MAT3 zoidwindow.c, ZOID_ResetWindows()).
 *
 * The scene is drawn in up to four windows (ZoidWinInfo, MAT3 tWindowInfo), one a player in split screen, each with
 * a viewport (vp0 ...) and a scissor rectangle (scissor0 ...). */
#include "game/game.h"

VIEW(u8, gSplitScreen);
#define gSplitScreen gSplitScreen_view
VIEW(s32, vp0);
#define vp0 vp0_view
VIEW(s16[4], scissor0);
#define scissor0 scissor0_view

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes): its
 * viewport and scissor, then the projection's numbers (ZOID_SetWindow), the clip planes and the fog. */
typedef struct tWindowInfo {
    void *vp;
    void *s;
    u8 rest[64];
} tWindowInfo;
VIEW(tWindowInfo[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view

/* One window, window 0, covering the whole screen (screen_width by screen_height) with the default field of view
 * (HFOV), viewport vp0 and scissor rectangle scissor0. (ZOID_SetScissor and ZOID_SetViewport are written out
 * here.) */
void ZOID_ResetWindows(void) {
    tWindowInfo *window;
    u16 height;
    u16 width;

    gNumZoidWindows = 1;
    gSplitScreen = 0;
    window = ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                 (f32)(screen_width / 2), (f32)(screen_height / 2));
    width = screen_width; height = screen_height;
    scissor0[0] = 0;
    scissor0[1] = 0;
    ZoidWinInfo[0].vp = &vp0;
    ZoidWinInfo[0].s = scissor0;
    scissor0[3] = height;
    scissor0[2] = width;
    if (window) {}
}
