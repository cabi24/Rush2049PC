/* ZOID_InitializeWindow/candidate.c: set a window up from scratch (MAT3 zoidwindow.c, ZOID_InitializeWindow()). */
#include "game/game.h"

/* A viewport (libultra Vp): half the size and the centre, in quarter pixels. */
typedef struct Viewport16 { s16 vscale[4], vtrans[4]; } Viewport16;
/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes). */
typedef struct tWindowInfo {
    void *vp, *s;
    int flags;
    f32 whviewang, wvviewang, whvangtan, wvvangtan, wihvangtan, wivvangtan;
    f32 wwidth, wheight, whcenter, wvcenter, aspect;
    f32 frontclip, backclip;    /* the near and far clip planes, feet */
    s16 fogmin, fogmax;
    u8 fogcolor[4];
} tWindowInfo;
VIEW(tWindowInfo[], ZoidWinInfo);
#define ZoidWinInfo ZoidWinInfo_view
VIEW(Viewport16, vp0);
#define vp0 vp0_view

/* Set window `idx` up: viewport `vp` and scissor `s`, clip planes at 2.5 and 2,000 feet (MAT3: 0.01 and 4,000; the
 * world is drawn out to gViewableDistance, set here too), `width` by `height` pixels centred at (hcenter, vcenter)
 * with fields of view `hang` and `vang` degrees (ZOID_SetWindow), the fog range 1000 to 999 (MAT3: 0 to 0) in
 * opaque black, and a black screen behind. Returns the window. */
tWindowInfo *ZOID_InitializeWindow(int idx, void *vp, void *s, f32 hang, f32 vang, f32 width, f32 height, f32 hcenter,
                                   f32 vcenter)
{
    ZoidWinInfo[idx].vp = vp;
    ZoidWinInfo[idx].s = s;
    ZoidWinInfo[idx].frontclip = 2.5f;
    ZoidWinInfo[idx].backclip = 2000.0f;
    gViewableDistance = 2000.0f;
    /* Original game: viewport vp0 gets this window's size and centre whichever viewport `vp` is (MAT3 too). In the
     * game's layouts only the full-screen window has vp0, so it looks harmless. */
    vp0.vscale[0] = (s16)(width * 2);
    vp0.vtrans[0] = (s16)(hcenter * 2);
    vp0.vscale[1] = (s16)(height * 2);
    vp0.vtrans[1] = (s16)(vcenter * 2);
    ZOID_SetWindow(idx, hang, vang, width, height, hcenter, vcenter);
    ZoidWinInfo[idx].fogmin = 1000;
    ZoidWinInfo[idx].fogmax = 999;
    ZoidWinInfo[idx].fogcolor[0] = 0;
    ZoidWinInfo[idx].fogcolor[1] = 0;
    ZoidWinInfo[idx].fogcolor[2] = 0;
    ZoidWinInfo[idx].fogcolor[3] = 255;
    ZOID_SetBGColor((u16)(((ZoidWinInfo[idx].fogcolor[0]<<8)&0xf800)|((ZoidWinInfo[idx].fogcolor[1]<<3)&0x7c0)|((ZoidWinInfo[idx].fogcolor[2]>>2)&0x3e))|1);
    return &ZoidWinInfo[idx];
}
