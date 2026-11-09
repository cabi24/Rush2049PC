/* ZOID_SplitMenuScreen.c: lay the screen out in one, two or four windows (MAT3 zoidwindow.c, ZOID_SplitMenuScreen()).
 *
 * The windows' sizes follow the screen's (screen_width, screen_height); each gets its own viewport (vp0 ... vplr)
 * and scissor rectangle (scissor0 ... scissorlr), a pixel or two in from the others so the borders between them
 * show. ZOID_SplitScreen is the race's version. */
#include "game/game.h"

VIEW(PcViewport, vp0);
#define vp0 vp0_view
VIEW(PcViewport, vp1);
#define vp1 vp1_view
VIEW(PcViewport, vp2);
#define vp2 vp2_view
VIEW(PcViewport, vpul);
#define vpul vpul_view
VIEW(PcViewport, vpur);
#define vpur vpur_view
VIEW(PcViewport, vpll);
#define vpll vpll_view
VIEW(PcViewport, vplr);
#define vplr vplr_view
VIEW(s16[4], scissor0);
#define scissor0 scissor0_view
VIEW(s16[4], scissor1);
#define scissor1 scissor1_view
VIEW(s16[4], scissor2);
#define scissor2 scissor2_view
VIEW(s16[4], scissorul);
#define scissorul scissorul_view
VIEW(s16[4], scissorur);
#define scissorur scissorur_view
VIEW(s16[4], scissorll);
#define scissorll scissorll_view
VIEW(s16[4], scissorlr);
#define scissorlr scissorlr_view

/* Lay the screen out for `num` players (nothing for 0 or less) and return the layout, gSplitScreen: 0 one
 * full-screen window; 1 two windows one above the other, each three quarters of the screen's width, from x 2;
 * 2 four quarters (for 3 or 4 players), with a field of view 0.95 times the default (HFOV). (view_bind is
 * ZOID_SetViewport's twin.) */
s32 ZOID_SplitMenuScreen(s32 num) {
    if (num > 0) {
        gNumZoidWindows = num;
        if (num == 1) {
            ZOID_InitializeWindow(0, &vp0, scissor0, HFOV, 0.0f, (f32)screen_width, (f32)screen_height,
                         (f32)(screen_width / 2), (f32)(screen_height / 2));
            {
                u16 right, bottom;
                right = screen_width; bottom = screen_height;
                scissor0[3] = bottom;
                scissor0[2] = right;
                scissor0[0] = 0;
                scissor0[1] = 0;
                view_bind(0, &vp0, scissor0);
            }
            gSplitScreen = 0;
        } else if (num == 2) {
            ZOID_InitializeWindow(0, &vp1, scissor1, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height / 4));
            {
                u16 right, bottom;
                right = screen_width * 3 / 4 + 2; bottom = screen_height / 2 - 1;
                scissor1[3] = bottom;
                scissor1[2] = right;
                scissor1[0] = 2;
                scissor1[1] = 0;
                view_bind(0, &vp1, scissor1);
            }
            ZOID_InitializeWindow(1, &vp2, scissor2, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height * 3 / 4));
            {
                u16 top, right, bottom;
                top = screen_height / 2 + 1; right = screen_width * 3 / 4 + 2; bottom = screen_height;
                scissor2[3] = bottom;
                scissor2[0] = 2;
                scissor2[2] = right;
                scissor2[1] = top;
                view_bind(1, &vp2, scissor2);
            }
            gSplitScreen = 1;
        } else if (num == 3 || num == 4) {
            ZOID_InitializeWindow(0, &vpul, scissorul, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width / 4 + 1), (f32)(screen_height / 4 + 1));
            {
                u16 right, bottom;
                right = screen_width / 2 - 1; bottom = screen_height / 2 - 1;
                scissorul[3] = bottom;
                scissorul[2] = right;
                scissorul[0] = 0;
                scissorul[1] = 0;
                view_bind(0, &vpul, scissorul);
            }
            ZOID_InitializeWindow(1, &vpur, scissorur, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height / 4 + 1));
            {
                u16 left, right, bottom;
                left = screen_width / 2 + 1; right = screen_width; bottom = screen_height / 2 - 1;
                scissorur[3] = bottom;
                scissorur[2] = right;
                scissorur[0] = left;
                scissorur[1] = 0;
                view_bind(1, &vpur, scissorur);
            }
            ZOID_InitializeWindow(2, &vpll, scissorll, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width / 4 + 1), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 top, right, bottom;
                top = screen_height / 2 + 1; right = screen_width / 2 - 1; bottom = screen_height;
                scissorll[3] = bottom;
                scissorll[2] = right;
                scissorll[1] = top;
                scissorll[0] = 0;
                view_bind(2, &vpll, scissorll);
            }
            ZOID_InitializeWindow(3, &vplr, scissorlr, HFOV * 0.95f, 0.0f,
                         (f32)(screen_width / 2), (f32)(screen_height / 2 + 2), (f32)(screen_width * 3 / 4 - 2), (f32)(screen_height * 3 / 4 + 2));
            {
                u16 left, top, right, bottom;
                left = screen_width / 2 + 1; top = screen_height / 2 + 1; right = screen_width; bottom = screen_height;
                scissorlr[3] = bottom;
                scissorlr[2] = right;
                scissorlr[1] = top;
                scissorlr[0] = left;
                view_bind(3, &vplr, scissorlr);
            }
            gSplitScreen = 2;
        }
    }
    return gSplitScreen;
}
