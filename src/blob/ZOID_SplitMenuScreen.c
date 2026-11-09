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

ArbEntry *ZOID_InitializeWindow(s32 idx, void *a, void *b, f32 c, f32 d, f32 e, f32 f, f32 g, f32 h);

s32 ZOID_SplitMenuScreen(s32 n) {
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
            ZOID_InitializeWindow(0, &vp1, scissor1, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height / 4));
            {
                u16 w, h;
                w = screen_width * 3 / 4 + 2; h = screen_height / 2 - 1;
                scissor1[3] = h;
                scissor1[2] = w;
                scissor1[0] = 2;
                scissor1[1] = 0;
                view_bind(0, &vp1, scissor1);
            }
            ZOID_InitializeWindow(1, &vp2, scissor2, HFOV, 0.0f,
                         (f32)(screen_width * 3 / 4), (f32)(screen_height / 2), (f32)(screen_width * 3 / 8 + 2), (f32)(screen_height * 3 / 4));
            {
                u16 y, w, h;
                y = screen_height / 2 + 1; w = screen_width * 3 / 4 + 2; h = screen_height;
                scissor2[3] = h;
                scissor2[0] = 2;
                scissor2[2] = w;
                scissor2[1] = y;
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
