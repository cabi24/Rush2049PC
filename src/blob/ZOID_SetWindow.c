/* ZOID_SetWindow.c: set a window's size, centre and field of view (MAT3 zoidwindow.c, ZOID_SetWindow()). */
#include "game/game.h"

/* A window (MAT3 tWindowInfo; 0x48 bytes on the N64, 0x50 on the PC, whose two pointers are 8 bytes). */
typedef struct tWindowInfo {
    void *vp;                   /* its viewport */
    void *s;                    /* its scissor rectangle */
    s32 flags;                  /* 1: no field of view given, 90 degrees taken */
    f32 whviewang;              /* the horizontal field of view, radians */
    f32 wvviewang;              /* the vertical one */
    f32 whvangtan;              /* the tangent of half the horizontal one */
    f32 wvvangtan;              /* the tangent of half the vertical one */
    f32 wihvangtan;             /* 0.5 / whvangtan: screen x = width * wihvangtan * x / z + hcenter */
    f32 wivvangtan;             /* 0.5 / wvvangtan */
    f32 wwidth;                 /* the size in screen pixels */
    f32 wheight;
    f32 whcenter;               /* the centre on the screen, pixels */
    f32 wvcenter;
    f32 aspect;                 /* width / height */
    s32 pad[4];                 /* the clip planes and the fog */
} tWindowInfo;

/* Set window `winidx`: `width` by `height` pixels centred at (hcenter, vcenter), with a horizontal field of view of
 * `hangle` degrees and a vertical one of `vangle` degrees, or the vertical one that keeps the pixels square when
 * `vangle` is 0. With `hangle` 0 both are 90 degrees (flags 1). In the N64's high-resolution modes (more than 240
 * lines, interlaced) the aspect and the vertical angle take half the height, as the low-resolution screen has it.
 * Then the tangents the projection uses. Returns the window. */
tWindowInfo *ZOID_SetWindow(s32 winidx, f32 hangle, f32 vangle, f32 width, f32 height, f32 hcenter, f32 vcenter) {
    tWindowInfo *win = &ZoidWinInfo[winidx];
    f32 hang;
    f32 vang;
    f32 h;

    win->wwidth = width;
    win->wheight = height;
    win->whcenter = hcenter;
    win->wvcenter = vcenter;
    if (screen_height > 240) {
        win->aspect = width / (height / 2.0f);
    } else {
        win->aspect = width / height;
    }
    if (hangle > 0.0f) {
        hang = hangle * 0.017453292f;           /* degrees to radians */
        if (vangle > 0.0f) {
            vang = vangle * 0.017453292f;
        } else {
            if (screen_height > 240) {
                h = height / 2.0f;
            } else {
                h = height;
            }
            vang = fatan(tan_cody_waite(hang / 2.0f) * h / width) * 2;
        }
        win->flags = 0;
    } else {
        vang = hang = 1.5707964f;               /* 90 degrees */
        win->flags = 1;
    }
    win->whviewang = hang;
    win->wvviewang = vang;
    win->whvangtan = tan_cody_waite(hang / 2.0f);
    win->wvvangtan = tan_cody_waite(win->wvviewang / 2.0f);
    win->wivvangtan = 0.5f / win->wvvangtan;
    win->wihvangtan = .5f / win->whvangtan;
    return win;
}
