/* port/gfx/gfx_size.c: sizes without GL (docs/port/GFX.md "Resolution and scale"): the internal scale for a window,
 * the internal size of a target, where N64 pixel edges fall in it, where the frame goes in the window, and window
 * sizes in pixels against the window system's units. Pure functions: gfx_size_test checks them with made-up sizes. */
#include <math.h>

#include "gfx_internal.h"

/* n N64 pixels at scale s: round(n * s), at least 1. A whole scale gives n * s exactly. */
int pcgfx_scaled(int n, float s) {
    int v = (int)floor((double)n * (double)s + 0.5);
    return v < 1 ? 1 : v;
}

/* N64 pixel edge v of a side n N64 pixels long, drawn as px internal pixels: the first internal pixel whose centre
 * is at or past the edge, ceil(v * px / n - 1/2), in whole numbers. It is how GL covers a rectangle with that edge
 * (a pixel is in when its centre is), so scissors, clears and rectangles agree: adjacent ones share the edge, no
 * gap, no overlap. At a whole scale it is v * scale. */
int pcgfx_edge(int v, int n, int px) {
    long long num = 2LL * v * px - n, den = 2LL * n;
    if (n <= 0)
        return 0;
    if (num <= 0)
        return 0;
    return (int)((num + den - 1) / den);
}

/* The internal pixel's N64 pixel: the one its centre is in (the shader's rule for point-sampled rectangles). A centre
 * on an edge goes to the pixel after it, as GL's fill rule gives it to the rectangle whose left side it is on. */
int pcgfx_pixel_of(int i, int n, int px) {
    if (px <= 0)
        return 0;
    return (int)((2LL * i + 1) * n / (2LL * px));
}

/* Rows, from the top: GL's fill rule works in its window coordinates, y up, where a centre on an edge goes to the
 * rectangle above it (the one whose bottom side it is on; iris, llvmpipe), so rows are counted from the bottom
 * there. pcgfx_row_edge is the first internal row (from the top) of N64 row v; pcgfx_row_of an internal row's N64 row.
 * At a whole scale the same as pcgfx_edge and pcgfx_pixel_of. */
int pcgfx_row_edge(int v, int n, int px) {
    return px - pcgfx_edge(n - v, n, px);
}

int pcgfx_row_of(int i, int n, int px) {
    return n - 1 - pcgfx_pixel_of(px - 1 - i, n, px);
}

/* The scale for a window of win_w x win_h pixels showing a frame fw x rows N64 pixels:
 * mode 0 (auto) the largest whole multiple it holds (1-8); mode < 0 (native) the frame's fit, any fraction (1-16):
 * the frame drawn at the window's own pixels. */
float pcgfx_window_scale(int mode, int win_w, int win_h, int fw, int rows) {
    if (fw <= 0 || rows <= 0 || win_w <= 0 || win_h <= 0)
        return 1.0f;
    if (mode < 0) {
        double a = (double)win_w / fw, b = (double)win_h / rows, s = a < b ? a : b;
        return s < 1.0 ? 1.0f : s > PCGFX_SCALE_MAX ? (float)PCGFX_SCALE_MAX : (float)s;
    } else {
        int a = win_w / fw, b = win_h / rows, s = a < b ? a : b;
        return (float)(s < 1 ? 1 : s > 8 ? 8 : s);
    }
}

/* Where the shown frame (src_w x src_h internal pixels, fw x rows N64 pixels) goes in a win_w x win_h window, with
 * square N64 pixels. native: aspect fit, and when the frame was drawn for this window (within 2 pixels of the fit
 * on both sides) pixel for pixel, centred. Otherwise the largest whole multiple of the N64 frame that fits, else
 * aspect fit (the frame smaller than the window's N64 multiple, or larger). Returns 1 when that is a whole multiple
 * of the source (nearest filtering), 0 when it needs filtering. */
int pcgfx_fit(int src_w, int src_h, int fw, int rows, int win_w, int win_h, int native, int r[4]) {
    int k = fw > 0 && rows > 0 ? (win_w / fw < win_h / rows ? win_w / fw : win_h / rows) : 0, dw, dh;
    if (fw <= 0 || rows <= 0 || win_w <= 0 || win_h <= 0 || src_w <= 0 || src_h <= 0) {
        r[0] = r[1] = r[2] = r[3] = 0;
        return 1;
    }
    if (k >= 1 && !native) {
        dw = fw * k;
        dh = rows * k;
    } else if ((long long)win_w * rows > (long long)win_h * fw) {
        dh = win_h;
        dw = (int)((long long)win_h * fw / rows);
    } else {
        dw = win_w;
        dh = (int)((long long)win_w * rows / fw);
    }
    if (native && src_w - dw <= 2 && dw - src_w <= 2 && src_h - dh <= 2 && dh - src_h <= 2) {
        dw = src_w;
        dh = src_h;
    }
    r[0] = (win_w - dw) / 2;
    r[1] = (win_h - dh) / 2;
    r[2] = dw;
    r[3] = dh;
    return dw % src_w == 0 && dh % src_h == 0;
}

/* A size in pixels as the window system's units, given the window's size in those units and its drawable's in
 * pixels (SDL_GetWindowSize, SDL_GL_GetDrawableSize: the same on Windows with per-monitor DPI awareness and X11; half
 * on a 2x macOS or Wayland output). */
void pcgfx_units_of_pixels(int px_w, int px_h, int win_w, int win_h, int draw_w, int draw_h, int *w, int *h) {
    if (win_w <= 0 || win_h <= 0 || draw_w <= 0 || draw_h <= 0) {
        *w = px_w;
        *h = px_h;
        return;
    }
    *w = (int)((long long)px_w * win_w / draw_w);
    *h = (int)((long long)px_h * win_h / draw_h);
}

/* The first window's size when video.width/height are auto: two thirds of the desktop (desk_w x desk_h pixels) at
 * the picture's aspect (0: the desktop's), inside the usable area (the desktop without its task bar), and at least
 * 640 x 480. On a 2880 x 1620 desktop with the aspect the desktop's: 1920 x 1080. */
void pcgfx_default_window(int desk_w, int desk_h, int usable_w, int usable_h, float aspect, int *w, int *h) {
    double a;
    int ww, hh;
    if (desk_w <= 0 || desk_h <= 0) {
        *w = 640;
        *h = 480;
        return;
    }
    a = aspect > 0.0f ? aspect : (double)desk_w / desk_h;
    hh = desk_h * 2 / 3;
    if (usable_h > 0 && hh > usable_h * 9 / 10)
        hh = usable_h * 9 / 10;
    ww = (int)(hh * a + 0.5);
    if (usable_w > 0 && ww > usable_w * 9 / 10) {
        ww = usable_w * 9 / 10;
        hh = (int)(ww / a + 0.5);
    }
    if (hh < 480) {
        hh = 480;
        ww = (int)(hh * a + 0.5);
    }
    *w = ww & ~1;
    *h = hh & ~1;
}
