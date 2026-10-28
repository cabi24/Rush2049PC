/* port/gfx/tests/size_test.c: gfx_size.c with made-up window, drawable and desktop sizes (no GL, no window).
 *
 *   gfx_size_test [scale|edge|fit|units|window|texfactor|all]
 *
 * The internal scale a window gets (native: any fraction; auto: whole), the internal size of a target, where N64
 * pixel edges land at fractional scales (no gap, no overlap, the shader's pixel rule the same), where the frame goes
 * in the window, and window sizes in pixels against the window system's units (Windows with and without DPI
 * awareness, a 2x macOS or Wayland output). Exit status 0 = pass. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "gfx_tex.h"

static int s_fail;

static void expect_i(const char *what, int got, int want) {
    if (got != want) {
        printf("  FAIL %s: %d, not %d\n", what, got, want);
        s_fail++;
    }
}

static void expect_f(const char *what, float got, float want) {
    if (fabsf(got - want) > 1e-4f) {
        printf("  FAIL %s: %.5f, not %.5f\n", what, got, want);
        s_fail++;
    }
}

static void case_scale(void) {
    /* a 3K 16:9 panel: the 16:9 frame (426 x 220 shown) and the 4:3 one (320 x 220) */
    expect_f("native 2880x1620 16:9", pcgfx_window_scale(-1, 2880, 1620, 426, 220), 2880.0f / 426);
    expect_f("native 2880x1620 4:3", pcgfx_window_scale(-1, 2880, 1620, 320, 220), 1620.0f / 220);
    expect_f("auto 2880x1620 16:9", pcgfx_window_scale(0, 2880, 1620, 426, 220), 6.0f);
    expect_i("2880 across 16:9", pcgfx_scaled(426, pcgfx_window_scale(-1, 2880, 1620, 426, 220)), 2880);
    expect_i("1620 rows 4:3", pcgfx_edge(220, 240, pcgfx_scaled(240, pcgfx_window_scale(-1, 2880, 1620, 320, 220))),
             1620);
    /* Windows without DPI awareness at 200%: a 1440 x 810 window, half the pixels */
    expect_f("native 1440x810 virtualized", pcgfx_window_scale(-1, 1440, 810, 426, 220), 1440.0f / 426);
    expect_f("native 1920x1080 16:9", pcgfx_window_scale(-1, 1920, 1080, 426, 220), 1920.0f / 426);
    expect_f("native 3840x2160 16:9", pcgfx_window_scale(-1, 3840, 2160, 426, 220), 3840.0f / 426);
    expect_f("native 640x480 4:3", pcgfx_window_scale(-1, 640, 480, 320, 220), 2.0f);
    expect_f("native: at least 1", pcgfx_window_scale(-1, 200, 150, 320, 220), 1.0f);
    expect_f("native: at most 16", pcgfx_window_scale(-1, 15360, 8640, 320, 220), 16.0f);
    expect_f("auto: at most 8", pcgfx_window_scale(0, 7680, 4320, 320, 220), 8.0f);
    expect_f("auto 640x480", pcgfx_window_scale(0, 640, 480, 320, 220), 2.0f);
    expect_f("no window", pcgfx_window_scale(-1, 0, 0, 320, 220), 1.0f);
    expect_i("scaled whole", pcgfx_scaled(320, 3.0f), 960);
    expect_i("scaled 6.75", pcgfx_scaled(240, 6.75f), 1620);
    expect_i("scaled 6.75 across", pcgfx_scaled(426, 6.75f), 2876);   /* 2875.5 rounds up */
    expect_i("scaled at least 1", pcgfx_scaled(1, 0.1f), 1);
}

/* Every N64 edge of an n-pixel side drawn as px pixels: from 0 to px, each N64 pixel at least one internal pixel
 * wide (no gap at scale >= 1), and each internal pixel's N64 pixel (the shader's rule) the one whose edges hold it. */
static void edges(int n, int px) {
    char what[96];
    int v, i, bad = 0;
    for (v = 0; v < n; v++)
        if (pcgfx_edge(v + 1, n, px) <= pcgfx_edge(v, n, px))
            bad++;
    snprintf(what, sizeof(what), "%d as %d: N64 pixels with no internal pixel", n, px);
    expect_i(what, bad, 0);
    snprintf(what, sizeof(what), "%d as %d: edge 0", n, px);
    expect_i(what, pcgfx_edge(0, n, px), 0);
    snprintf(what, sizeof(what), "%d as %d: edge n", n, px);
    expect_i(what, pcgfx_edge(n, n, px), px);
    for (i = 0, bad = 0; i < px; i++) {
        int k = pcgfx_pixel_of(i, n, px);
        if (k < 0 || k >= n || i < pcgfx_edge(k, n, px) || i >= pcgfx_edge(k + 1, n, px))
            bad++;
    }
    snprintf(what, sizeof(what), "%d as %d: pixels outside their N64 pixel's edges", n, px);
    expect_i(what, bad, 0);
    for (v = 0, bad = 0; v < n; v++)   /* rows: the same from the top, ties the other way */
        if (pcgfx_row_edge(v + 1, n, px) <= pcgfx_row_edge(v, n, px))
            bad++;
    bad += pcgfx_row_edge(0, n, px) != 0 || pcgfx_row_edge(n, n, px) != px;
    for (i = 0; i < px; i++) {
        int k = pcgfx_row_of(i, n, px);
        if (k < 0 || k >= n || i < pcgfx_row_edge(k, n, px) || i >= pcgfx_row_edge(k + 1, n, px))
            bad++;
    }
    snprintf(what, sizeof(what), "%d as %d: rows", n, px);
    expect_i(what, bad, 0);
}

static void case_edge(void) {
    static const float scales[] = {1.0f, 1.5f, 2.0f, 2.25f, 2.5f, 3.0f, 3.375f, 4.5f, 6.75f, 6.7606f, 7.3636f, 9.0f};
    int i, v, s, bad = 0;
    for (s = 1; s <= 8; s++)   /* a whole scale: v * s, as before fractions */
        for (v = 0; v <= 426; v++)
            bad += pcgfx_edge(v, 426, 426 * s) != v * s || pcgfx_row_edge(v, 426, 426 * s) != v * s;
    expect_i("whole scales: edge != v * s", bad, 0);
    for (i = 0; i < (int)(sizeof(scales) / sizeof(scales[0])); i++) {
        edges(240, pcgfx_scaled(240, scales[i]));
        edges(320, pcgfx_scaled(320, scales[i]));
        edges(426, pcgfx_scaled(426, scales[i]));
        edges(560, pcgfx_scaled(560, scales[i]));
    }
    /* 2880 x 1620 at 16:9: 426 x 240 as 2880 x 1623 */
    edges(426, 2880);
    edges(240, 1623);
    /* a tie: at 2.5, N64 edge 1 is at 2.5 internal pixels, and pixel 2's centre on it belongs to the right */
    expect_i("tie at 2.5", pcgfx_edge(1, 2, 5), 2);
    expect_i("tie at 2.5: pixel 2", pcgfx_pixel_of(2, 2, 5), 1);
    /* rows: GL's y-up fill rule gives the tie to the rectangle above: at 1.5, N64 row edge 129 is at 193.5 and
     * internal row 193 is the last of N64 row 128 (as iris and llvmpipe draw it) */
    expect_i("row tie at 1.5", pcgfx_row_edge(129, 240, 360), 194);
    expect_i("row tie at 1.5: row 193", pcgfx_row_of(193, 240, 360), 128);
}

static void case_fit(void) {
    int r[4], exact;
    /* native, drawn for the window: pixel for pixel, centred (2880 x 1488: 220 of 240 rows at 1623) */
    exact = pcgfx_fit(2880, 1488, 426, 220, 2880, 1620, 1, r);
    expect_i("native 16:9 x", r[0], 0);
    expect_i("native 16:9 y", r[1], 66);
    expect_i("native 16:9 w", r[2], 2880);
    expect_i("native 16:9 h", r[3], 1488);
    expect_i("native 16:9 nearest", exact, 1);
    exact = pcgfx_fit(2356, 1620, 320, 220, 2880, 1620, 1, r);   /* 4:3: bars at the sides */
    expect_i("native 4:3 x", r[0], 262);
    expect_i("native 4:3 h", r[3], 1620);
    expect_i("native 4:3 nearest", exact, 1);
    exact = pcgfx_fit(2881, 1488, 426, 220, 2880, 1620, 1, r);   /* a pixel over (rounding): still 1:1 */
    expect_i("native over w", r[2], 2881);
    expect_i("native over nearest", exact, 1);
    /* native drawn for another size (a render size, a resize not yet drawn): fitted, filtered */
    exact = pcgfx_fit(1920, 992, 426, 220, 2880, 1620, 1, r);
    expect_i("native 1080p on 1620p w", r[2], 2880);
    expect_i("native 1080p on 1620p filtered", exact, 0);
    /* whole multiples, as before */
    exact = pcgfx_fit(640, 440, 320, 220, 640, 480, 0, r);
    expect_i("2x in 640x480 y", r[1], 20);
    expect_i("2x nearest", exact, 1);
    exact = pcgfx_fit(320, 220, 320, 220, 2880, 1620, 0, r);   /* N64 pixels at 7x */
    expect_i("1x at 7x w", r[2], 2240);
    expect_i("1x at 7x h", r[3], 1540);
    expect_i("1x at 7x nearest", exact, 1);
    exact = pcgfx_fit(640, 440, 320, 220, 2880, 1620, 0, r);   /* 2x shown at 7x: filtered */
    expect_i("2x at 7x filtered", exact, 0);
    exact = pcgfx_fit(320, 220, 320, 220, 200, 150, 0, r);     /* smaller than the frame: aspect fit */
    expect_i("small w", r[2], 200);
    expect_i("small h", r[3], 137);
}

/* Window sizes in pixels and in the window system's units: SDL_GetWindowSize against SDL_GL_GetDrawableSize. */
static void case_units(void) {
    int w, h;
    /* Windows, per-monitor DPI aware (SDL_HINT_WINDOWS_DPI_AWARENESS permonitorv2): units are pixels */
    pcgfx_units_of_pixels(1920, 1080, 2880, 1620, 2880, 1620, &w, &h);
    expect_i("permonitorv2 w", w, 1920);
    expect_i("permonitorv2 h", h, 1080);
    /* a 2x macOS or Wayland output: the window is in points, the drawable in pixels */
    pcgfx_units_of_pixels(1920, 1080, 1440, 810, 2880, 1620, &w, &h);
    expect_i("2x points w", w, 960);
    expect_i("2x points h", h, 540);
    /* 175%: a 1646 x 926 point window over 2880 x 1620 pixels */
    pcgfx_units_of_pixels(2880, 1620, 1646, 926, 2880, 1620, &w, &h);
    expect_i("175% w", w, 1646);
    expect_i("175% h", h, 926);
    /* no window yet: as given */
    pcgfx_units_of_pixels(1280, 720, 0, 0, 0, 0, &w, &h);
    expect_i("no window w", w, 1280);
}

static void case_window(void) {
    int w, h;
    pcgfx_default_window(2880, 1620, 2880, 1530, 0.0f, &w, &h);   /* the 3K panel, a task bar */
    expect_i("3K w", w, 1920);
    expect_i("3K h", h, 1080);
    pcgfx_default_window(1920, 1080, 1920, 1040, 0.0f, &w, &h);
    expect_i("1080p w", w, 1280);
    expect_i("1080p h", h, 720);
    pcgfx_default_window(3840, 2160, 3840, 2100, 0.0f, &w, &h);
    expect_i("4K w", w, 2560);
    pcgfx_default_window(2880, 1620, 2880, 1530, 4.0f / 3.0f, &w, &h);   /* widescreen off: a 4:3 window */
    expect_i("3K 4:3 w", w, 1440);
    expect_i("3K 4:3 h", h, 1080);
    pcgfx_default_window(1024, 768, 1024, 740, 0.0f, &w, &h);   /* small desktops: at least 480 rows */
    expect_i("1024x768 h", h, 512);
    pcgfx_default_window(800, 600, 800, 560, 0.0f, &w, &h);
    expect_i("800x600 h", h, 480);
    pcgfx_default_window(0, 0, 0, 0, 0.0f, &w, &h);   /* no display */
    expect_i("no display w", w, 640);
    expect_i("no display h", h, 480);
}

/* The upscaled textures' factor and a pack's limit follow the internal scale in effect, fractions too (gfx_tex.c). */
static void case_texfactor(void) {
    static const struct { float s; int factor, limit; } t[] = {
        {1.0f, 1, 1}, {1.4f, 1, 2}, {1.5f, 2, 2}, {2.0f, 2, 2}, {3.0f, 2, 3}, {3.375f, 2, 4}, {3.5f, 4, 4},
        {4.0f, 4, 4}, {6.0f, 4, 6}, {2880.0f / 426, 4, 7}, {8.0f, 4, 8},
    };
    float was = pcgfx_cfg.scale;
    char what[64];
    int i;
    for (i = 0; i < (int)(sizeof(t) / sizeof(t[0])); i++) {
        pcgfx_cfg.scale = t[i].s;
        snprintf(what, sizeof(what), "factor at %.3f", t[i].s);
        expect_i(what, pcgfx_tex_factor(), t[i].factor);
        snprintf(what, sizeof(what), "pack limit at %.3f", t[i].s);
        expect_i(what, pcgfx_tex_pack_limit(), t[i].limit);
    }
    pcgfx_cfg.scale = was;
}

int main(int argc, char **argv) {
    static const struct { const char *name; void (*fn)(void); } cases[] = {
        {"scale", case_scale}, {"edge", case_edge}, {"fit", case_fit}, {"units", case_units}, {"window", case_window},
        {"texfactor", case_texfactor},
    };
    const char *which = argc > 1 ? argv[1] : "all";
    int i, ran = 0, failed = 0;
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
        int before = s_fail;
        if (strcmp(which, "all") && strcmp(which, cases[i].name))
            continue;
        cases[i].fn();
        ran++;
        failed += s_fail != before;
        printf("%s %s\n", s_fail != before ? "FAIL" : "ok  ", cases[i].name);
    }
    if (!ran) {
        printf("no case named %s\n", which);
        return 2;
    }
    printf("%d/%d cases passed\n", ran - failed, ran);
    return failed ? 1 : 0;
}
