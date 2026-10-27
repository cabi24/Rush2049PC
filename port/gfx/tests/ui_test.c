/* port/gfx/tests/ui_test.c: drawing over the frame (gfx_ui.c, gfx_font.c), headless.
 *
 *   gfx_ui_test font|panel|closed|scaled|all
 *
 * Headless, the window is an offscreen image of the window's size that the present fills (pc_gfx_read_window
 * reads it). Each case sets a UI hook, shows a frame (a fill through the display-list interpreter) or none, and
 * checks the window's pixels:
 *   font    the glyph table (every printable glyph, distinct, descenders only where they belong) and four glyphs
 *           against bitmaps written out here; a string drawn at scales 1, 2 and 3 matches them pixel for pixel
 *   panel   a semi-transparent rectangle blends over the frame by its alpha; an opaque one covers it
 *   closed  without a hook the window, the render target and a screenshot are byte for byte what they were
 *           before the overlay was ever shown; with one, a screenshot still has the frame without it
 *   scaled  the overlay's pixels are the same at internal scale 1 and at 3 in widescreen; the hook gets the
 *           window's size, which follows pc_gfx_set_window_size */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_port.h"
#include "test_gbi.h"

int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h);
int pc_gfx_read_window(uint8_t *rgba, int *w, int *h);
void pc_gfx_set_widescreen(float aspect);
void pc_gfx_set_scale(int scale);
void pc_gfx_set_window_size(int w, int h);

/* ---------------------------------------------------------------- what port/ultra provides */

void *pc_n64_ptr(uint32_t a) { (void)a; return NULL; }
uint32_t pc_n64_addr(const void *h) { (void)h; return 0; }
void pc_mem_register_be(const void *start, size_t size) { (void)start, (void)size; }
void pc_mem_unregister_be(const void *start) { (void)start; }
int pc_mem_is_be(const void *p) { (void)p; return 0; }

/* ---------------------------------------------------------------- harness */

static int s_fail, s_checks;

#define CHECK(cond, ...)                                                                                      \
    do {                                                                                                      \
        s_checks++;                                                                                           \
        if (!(cond)) {                                                                                        \
            s_fail++;                                                                                         \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                                                     \
            printf(__VA_ARGS__);                                                                              \
            printf("\n");                                                                                     \
        }                                                                                                     \
    } while (0)

static uint16_t s_fb[320 * 240];
static PcGfxWords s_list[64];

/* A 320 x 240 frame of one 5551 color, presented. */
static void show_frame(uint16_t color) {
    gp = s_list;
    g_be_mode = 0;
    gsDPPipeSync();
    gsDPSetScissor(0, 0, 320, 240);
    gsDPSetCycleType(G_CYC_FILL);
    gsDPSetRenderMode(0, 0);
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, (uintptr_t)s_fb);
    gsDPSetFillColor((uint32_t)color << 16 | color);
    gsDPFillRectangle(0, 0, 319, 239);
    gsDPFullSync();
    gsSPEndDisplayList();
    pc_gfx_run_dl(s_list);
    pc_gfx_present(s_fb);
}

static uint8_t *read_window(int *w, int *h) {
    uint8_t *px;
    if (pc_gfx_read_window(NULL, w, h) != 0)
        return NULL;
    px = (uint8_t *)malloc((size_t)*w * *h * 4);
    if (pc_gfx_read_window(px, w, h) != 0) {
        free(px);
        return NULL;
    }
    return px;
}

static uint8_t *read_target(int *w, int *h) {
    uint8_t *px;
    pc_gfx_read_framebuffer(s_fb, NULL, w, h);
    px = (uint8_t *)malloc((size_t)*w * *h * 4);
    pc_gfx_read_framebuffer(s_fb, px, w, h);
    return px;
}

static int near(const uint8_t *p, int r, int g, int b, int tol) {
    return abs(p[0] - r) <= tol && abs(p[1] - g) <= tol && abs(p[2] - b) <= tol;
}

/* ---------------------------------------------------------------- font */

/* What four glyphs must look like, written out again here: 5 columns, 9 rows (7 above the baseline). */
static const struct {
    char c;
    const char *rows[PC_GFX_FONT_H];
} s_expect[] = {
    {'H', {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#", ".....", "....."}},
    {'i', {"..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###.", ".....", "....."}},
    {'!', {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#..", ".....", "....."}},
    {'g', {".....", ".....", ".####", "#...#", "#...#", "#...#", ".####", "....#", ".###."}},
};

static const char *s_text;
static int s_text_x, s_text_y, s_text_k, s_text_drawn;
static uint32_t s_text_rgba;

static void draw_text(int w, int h) {
    pc_gfx_ui_rect(0, 0, w, h, 0x000000FFu);
    s_text_drawn = pc_gfx_ui_text(s_text_x, s_text_y, s_text_k, s_text_rgba, s_text);
}

/* The string drawn at scale k matches the expected bitmaps pixel for pixel, the gaps between glyphs included. */
static void check_text(const char *text, int x0, int y0, int k, uint32_t rgba) {
    int w, h, i, x, y, bad = 0;
    uint8_t r = rgba >> 24, g = rgba >> 16 & 255, b = rgba >> 8 & 255, *px;
    s_text = text;
    s_text_x = x0;
    s_text_y = y0;
    s_text_k = k;
    s_text_rgba = rgba;
    pc_gfx_set_ui_hook(draw_text);
    pc_gfx_redraw();
    pc_gfx_set_ui_hook(NULL);
    px = read_window(&w, &h);
    CHECK(px != NULL, "window readback");
    if (px == NULL)
        return;
    CHECK(s_text_drawn == (int)strlen(text) * PC_GFX_FONT_ADVANCE * k, "'%s' at %d: width %d", text, k,
          s_text_drawn);
    for (i = 0; text[i]; i++) {
        const char *const *rows = NULL;
        size_t e;
        for (e = 0; e < sizeof(s_expect) / sizeof(s_expect[0]); e++)
            if (s_expect[e].c == text[i])
                rows = s_expect[e].rows;
        for (y = 0; y < PC_GFX_FONT_H * k + 3; y++)     /* the cell and a little below */
            for (x = 0; x < PC_GFX_FONT_ADVANCE * k; x++) {   /* the glyph and the gap after it */
                int col = x / k, row = y / k;
                int lit = rows && col < PC_GFX_FONT_W && row < PC_GFX_FONT_H && rows[row][col] == '#';
                const uint8_t *p = px + ((size_t)(y0 + y) * w + x0 + i * PC_GFX_FONT_ADVANCE * k + x) * 4;
                if (lit ? !near(p, r, g, b, 0) : !near(p, 0, 0, 0, 0)) {
                    if (bad++ < 4)
                        printf("  '%c' scale %d (%d, %d): %d,%d,%d, %s\n", text[i], k, x, y, p[0], p[1], p[2],
                               lit ? "lit" : "dark");
                }
            }
    }
    CHECK(bad == 0, "'%s' at scale %d: %d pixels differ from the expected glyphs", text, k, bad);
    free(px);
}

static void case_font(void) {
    uint8_t rows[PC_GFX_FONT_H], seen[95][PC_GFX_FONT_H];
    int c, r, dup = 0, empty = 0, wide = 0, desc_wrong = 0;
    size_t e;
    CHECK(pc_gfx_font_glyph(31, rows) == 0 && pc_gfx_font_glyph(127, rows) == 0, "no glyph outside 32-126");
    for (c = 32; c < 127; c++) {
        int any = 0, below = 0, d;
        CHECK(pc_gfx_font_glyph(c, rows) == 1, "glyph %d", c);
        for (r = 0; r < PC_GFX_FONT_H; r++) {
            any |= rows[r];
            wide |= rows[r] >> PC_GFX_FONT_W;
            if (r >= 7)
                below |= rows[r];
        }
        empty += c != ' ' && !any;
        empty += c == ' ' && any;
        desc_wrong += (below != 0) != (strchr(",;_gjpqy", c) != NULL);
        for (d = 32; d < c; d++)
            dup += !memcmp(seen[d - 32], rows, sizeof(rows));
        memcpy(seen[c - 32], rows, sizeof(rows));
    }
    CHECK(!dup, "%d glyphs alike", dup);
    CHECK(!empty, "%d glyphs empty (or a lit space)", empty);
    CHECK(!wide, "a glyph wider than %d", PC_GFX_FONT_W);
    CHECK(!desc_wrong, "%d glyphs with descenders where they don't belong, or without", desc_wrong);
    for (e = 0; e < sizeof(s_expect) / sizeof(s_expect[0]); e++) {
        pc_gfx_font_glyph(s_expect[e].c, rows);
        for (r = 0; r < PC_GFX_FONT_H; r++) {
            int x, want = 0;
            for (x = 0; x < PC_GFX_FONT_W; x++)
                want = want << 1 | (s_expect[e].rows[r][x] == '#');
            CHECK(rows[r] == want, "'%c' row %d: %02x, expected %02x", s_expect[e].c, r, rows[r], want);
        }
    }
    check_text("Hi!g", 13, 17, 1, 0xFFFFFFFFu);
    check_text("gH!i", 41, 70, 2, 0x40FF80FFu);
    check_text("Hi!g", 100, 200, 3, 0xFFD24AFFu);
}

/* ---------------------------------------------------------------- panel */

static void draw_panel(int w, int h) {
    pc_gfx_ui_rect(100, 50, 200, 100, 0x0000FF80u);   /* blue, alpha 128 */
    pc_gfx_ui_rect(150, 70, 20, 20, 0x10203040u);
    pc_gfx_ui_rect(150, 70, 20, 20, 0x10C030FFu);     /* opaque over the rest */
}

static void case_panel(void) {
    int w, h, tw, th;
    uint8_t *px, *t, fr, fg, fb;
    show_frame(0xF801);
    t = read_target(&tw, &th);
    fr = t[0], fg = t[1], fb = t[2];
    pc_gfx_set_ui_hook(draw_panel);
    pc_gfx_present(s_fb);
    pc_gfx_set_ui_hook(NULL);
    px = read_window(&w, &h);
    CHECK(px != NULL && w == 640 && h == 480, "headless window %dx%d", px ? w : 0, px ? h : 0);
    if (px != NULL) {
        const uint8_t *in = px + ((size_t)60 * w + 110) * 4, *out = px + ((size_t)40 * w + 110) * 4;
        const uint8_t *solid = px + ((size_t)80 * w + 160) * 4, *edge = px + ((size_t)149 * w + 299) * 4;
        const uint8_t *past = px + ((size_t)150 * w + 300) * 4;
        int a = 128;
        CHECK(near(out, fr, fg, fb, 0), "outside: %d,%d,%d, the frame %d,%d,%d", out[0], out[1], out[2], fr, fg, fb);
        CHECK(near(in, fr * (255 - a) / 255, fg * (255 - a) / 255, (fb * (255 - a) + 255 * a) / 255, 2),
              "blended: %d,%d,%d", in[0], in[1], in[2]);
        CHECK(near(edge, in[0], in[1], in[2], 0), "the rectangle's last pixel (299, 149)");
        CHECK(near(past, fr, fg, fb, 0), "past its edge (300, 150): %d,%d,%d", past[0], past[1], past[2]);
        CHECK(near(solid, 0x10, 0xC0, 0x30, 0), "opaque: %d,%d,%d", solid[0], solid[1], solid[2]);
    }
    free(px);
    free(t);
}

/* ---------------------------------------------------------------- closed */

static void draw_much(int w, int h) {
    pc_gfx_ui_rect(0, 0, w, h, 0x00000060u);
    pc_gfx_ui_rect(16, 16, w - 32, h - 32, 0x0C1018D8u);
    pc_gfx_ui_text(24, 24, 2, 0xFFFFFFFFu, "RUSH 2049 SETTINGS");
}

static long file_bytes(const char *path, uint8_t **out) {
    FILE *f = fopen(path, "rb");
    long n;
    if (f == NULL)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *out = (uint8_t *)malloc((size_t)n);
    if (fread(*out, 1, (size_t)n, f) != (size_t)n)
        n = -1;
    fclose(f);
    return n;
}

static void case_closed(const char *dir) {
    int w, h, tw, th, w2, h2;
    uint8_t *w0, *t0, *w1, *w2p, *t1, *png0 = NULL, *png1 = NULL;
    char p0[512], p1[512];
    long n0, n1;
    snprintf(p0, sizeof(p0), "%s/ui_without.png", dir);
    snprintf(p1, sizeof(p1), "%s/ui_with.png", dir);
    show_frame(0x07C1);   /* green */
    w0 = read_window(&w, &h);
    t0 = read_target(&tw, &th);
    pc_gfx_save_screenshot(p0);
    pc_gfx_set_ui_hook(draw_much);
    pc_gfx_present(s_fb);
    w1 = read_window(&w2, &h2);
    CHECK(w0 && w1 && memcmp(w0, w1, (size_t)w * h * 4) != 0, "the overlay shows");
    pc_gfx_save_screenshot(p1);
    n0 = file_bytes(p0, &png0);
    n1 = file_bytes(p1, &png1);
    CHECK(n0 > 0 && n0 == n1 && !memcmp(png0, png1, (size_t)n0), "a screenshot doesn't have the overlay");
    pc_gfx_redraw();
    free(w1);
    w1 = read_window(&w2, &h2);
    CHECK(w1 && memcmp(w0, w1, (size_t)w * h * 4) != 0, "pc_gfx_redraw draws it too");
    pc_gfx_set_ui_hook(NULL);
    pc_gfx_redraw();
    w2p = read_window(&w2, &h2);
    CHECK(w2p && w2 == w && h2 == h && !memcmp(w0, w2p, (size_t)w * h * 4), "closed, redrawn: the same window");
    free(w2p);
    pc_gfx_present(s_fb);
    show_frame(0x07C1);
    w2p = read_window(&w2, &h2);
    t1 = read_target(&tw, &th);
    CHECK(w2p && !memcmp(w0, w2p, (size_t)w * h * 4), "closed, presented: the same window");
    CHECK(!memcmp(t0, t1, (size_t)tw * th * 4), "the render target never changed");
    free(w0);
    free(w1);
    free(w2p);
    free(t0);
    free(t1);
    free(png0);
    free(png1);
}

/* ---------------------------------------------------------------- scaled */

static int s_hook_w, s_hook_h;

static void draw_box(int w, int h) {
    s_hook_w = w;
    s_hook_h = h;
    pc_gfx_ui_rect(20, 20, 200, 40, 0x202020FFu);
    pc_gfx_ui_text(24, 26, 2, 0xFFFFFFFFu, "Scale");
}

static void region(const uint8_t *px, int w, uint8_t *out) {
    int y;
    for (y = 0; y < 40; y++)
        memcpy(out + (size_t)y * 200 * 4, px + ((size_t)(20 + y) * w + 20) * 4, 200 * 4);
}

static void case_scaled(void) {
    static uint8_t a[200 * 40 * 4], b[200 * 40 * 4];
    int w, h, tw, th;
    uint8_t *px;
    pc_gfx_set_ui_hook(draw_box);
    show_frame(0xF801);
    px = read_window(&w, &h);
    if (px != NULL)
        region(px, w, a);
    free(px);
    pc_gfx_read_framebuffer(s_fb, NULL, &tw, &th);
    CHECK(tw == 320 && th == 240 && s_hook_w == 640 && s_hook_h == 480, "1x: target %dx%d, hook %dx%d", tw, th,
          s_hook_w, s_hook_h);
    pc_gfx_set_scale(3);
    pc_gfx_set_widescreen(16.0f / 9.0f);
    show_frame(0xF801);
    show_frame(0xF801);
    px = read_window(&w, &h);
    if (px != NULL)
        region(px, w, b);
    free(px);
    pc_gfx_read_framebuffer(s_fb, NULL, &tw, &th);
    CHECK(tw == 426 * 3 && th == 240 * 3, "3x at 16:9: target %dx%d", tw, th);
    CHECK(!memcmp(a, b, sizeof(a)), "the overlay's pixels are the same at 1x and at 3x in widescreen");
    pc_gfx_set_window_size(1280, 960);
    show_frame(0xF801);
    pc_gfx_read_window(NULL, &w, &h);
    CHECK(s_hook_w == 1280 && s_hook_h == 960 && w == 1280 && h == 960, "window 1280x960: hook %dx%d, window %dx%d",
          s_hook_w, s_hook_h, w, h);
    pc_gfx_set_ui_hook(NULL);
    pc_gfx_set_scale(1);
    pc_gfx_set_widescreen(0);
    pc_gfx_set_window_size(640, 480);
}

/* ---------------------------------------------------------------- main */

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "all", *dir = argc > 2 ? argv[2] : ".";
    int ran = 0;
    if (pc_gfx_init(1, 640, 480) != 0) {
        printf("FAIL: pc_gfx_init\n");
        return 2;
    }
    if (!strcmp(which, "font") || !strcmp(which, "all"))
        case_font(), ran++;
    if (!strcmp(which, "panel") || !strcmp(which, "all"))
        case_panel(), ran++;
    if (!strcmp(which, "closed") || !strcmp(which, "all"))
        case_closed(dir), ran++;
    if (!strcmp(which, "scaled") || !strcmp(which, "all"))
        case_scaled(), ran++;
    pc_gfx_shutdown();
    if (!ran) {
        printf("no case named %s\n", which);
        return 2;
    }
    printf("%s: %d checks, %d failed\n", which, s_checks, s_fail);
    return s_fail ? 1 : 0;
}
