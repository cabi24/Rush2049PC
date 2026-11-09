/* ZOID_SplitScreen (MAT3 zoidwindow.c): the scene's windows for one to four players, and their viewports and
 * scissor boxes. */
#include "game/game.h"

/* Gives window k the viewport vp and the scissor box r (left, top, right, bottom); returns the window. */
ArbEntry *view_bind(s32 k, void *vp, s16 *r) {
    ZoidWinInfo[k].vp = vp;
    ZoidWinInfo[k].s = r;
    return &ZoidWinInfo[k];
}

/* Lays out n windows (nothing for n <= 0): one full screen (90-degree field of view); two, one above the other,
 * each half the height with a wider field of view (127/90) and a line between them; three or four in quarters
 * (0.95 the field of view), a line between them. Returns the split (gSplitScreen: 0 one window, 1 two, 2 four). */
s32 ZOID_SplitScreen(s32 n) {
    if (n > 0) {
        gNumZoidWindows = n;
        if (n == 1) {
            ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                         (f32)(screen_width / 2), (f32)(screen_height / 2));
            {
                u16 w, h;
                w = screen_width; h = screen_height;
                scissor0[3] = h;
                scissor0[2] = w;
                scissor0[0] = 0;
                scissor0[1] = 0;
                view_bind(0, &vp0, scissor0);
            }
            gSplitScreen = 0;
        } else if (n == 2) {
            ZOID_InitializeWindow(0, &vp1, scissor1, HFOV * 127.0f / 90.0f, 0.0f,
                         (f32)screen_width, (f32)(screen_height / 2), (f32)(screen_width / 2), (f32)(screen_height / 4));
            {
                u16 w, h;
                w = screen_width; h = screen_height / 2 - 1;
                scissor1[3] = h;
                scissor1[2] = w;
                scissor1[0] = 0;
                scissor1[1] = 0;
                view_bind(0, &vp1, scissor1);
            }
            ZOID_InitializeWindow(1, &vp2, scissor2, HFOV * 127.0f / 90.0f, 0.0f,
                         (f32)screen_width, (f32)(screen_height / 2), (f32)(screen_width / 2), (f32)(screen_height * 3 / 4));
            {
                u16 y, w, h;
                y = screen_height / 2 + 1; w = screen_width; h = screen_height;
                scissor2[3] = h;
                scissor2[2] = w;
                scissor2[1] = y;
                scissor2[0] = 0;
                view_bind(1, &vp2, scissor2);
            }
            gSplitScreen = 1;
        } else if (n == 3 || n == 4) {
            ZOID_InitializeWindow(0, &vpul, scissorul, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width / 4 + 1), (f32)(screen_height / 4 + 1));
            {
                u16 w, h;
                w = screen_width / 2 - 1; h = screen_height / 2 - 1;
                scissorul[3] = h;
                scissorul[2] = w;
                scissorul[0] = 0;
                scissorul[1] = 0;
                view_bind(0, &vpul, scissorul);
            }
            ZOID_InitializeWindow(1, &vpur, scissorur, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height / 4 + 1));
            {
                u16 x, w, h;
                x = screen_width / 2 + 1; w = screen_width; h = screen_height / 2 - 1;
                scissorur[3] = h;
                scissorur[2] = w;
                scissorur[0] = x;
                scissorur[1] = 0;
                view_bind(1, &vpur, scissorur);
            }
            ZOID_InitializeWindow(2, &vpll, scissorll, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width / 4 + 1), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 y, w, h;
                y = screen_height / 2 + 1; w = screen_width / 2 - 1; h = screen_height;
                scissorll[3] = h;
                scissorll[2] = w;
                scissorll[1] = y;
                scissorll[0] = 0;
                view_bind(2, &vpll, scissorll);
            }
            ZOID_InitializeWindow(3, &vplr, scissorlr, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 x, y, w, h;
                x = screen_width / 2 + 1; y = screen_height / 2 + 1; w = screen_width; h = screen_height;
                scissorlr[3] = h;
                scissorlr[2] = w;
                scissorlr[1] = y;
                scissorlr[0] = x;
                view_bind(3, &vplr, scissorlr);
            }
            gSplitScreen = 2;
        }
    }
    return gSplitScreen;
}
