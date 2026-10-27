/* port/gfx/gfx_ui.c: drawing over the frame in window pixels, for port/input's settings overlay
 * (docs/port/FRONTEND.md, GFX.md "The overlay").
 *
 * The UI hook (pc_gfx_set_ui_hook) runs at each present and pc_gfx_redraw, after the frame is shown in the window
 * and before the swap. It draws rectangles and text, which are batched here as triangles in window pixels and
 * drawn by gfx_gl.c in one call: alpha-blended, through a program of their own and a texture holding the font
 * (gfx_font.c) and a solid cell for the rectangles. Nothing goes through the display-list interpreter or a render
 * target, so the overlay is the same at every internal scale and in widescreen, and the frame under it (and the
 * screenshots, which read the presented frame) is unchanged. Without a hook nothing here runs. */
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

/* The atlas: glyph c - 32 at x = (c - 32) * ATLAS_CELL, then a solid cell for rectangles. */
#define ATLAS_CELL (PC_GFX_FONT_W + 1)
#define ATLAS_W (96 * ATLAS_CELL)
#define ATLAS_H PC_GFX_FONT_H
#define SOLID 95

static void (*s_hook)(int w, int h);
static GfxOutVtx *s_v;
static int s_n, s_cap, s_w, s_h;
static uint8_t s_atlas[ATLAS_W * ATLAS_H * 4];
static int s_atlas_made;

void pc_gfx_set_ui_hook(void (*draw)(int window_w, int window_h)) {
    s_hook = draw;
}

int pcgfx_ui_active(void) {
    return s_hook != NULL;
}

static void make_atlas(void) {
    uint8_t rows[PC_GFX_FONT_H];
    int c, r, x;
    memset(s_atlas, 0, sizeof(s_atlas));
    for (c = 32; c < 127; c++) {
        pc_gfx_font_glyph(c, rows);
        for (r = 0; r < PC_GFX_FONT_H; r++)
            for (x = 0; x < PC_GFX_FONT_W; x++)
                if (rows[r] >> (PC_GFX_FONT_W - 1 - x) & 1)
                    memset(s_atlas + ((size_t)r * ATLAS_W + (c - 32) * ATLAS_CELL + x) * 4, 255, 4);
    }
    for (r = 0; r < PC_GFX_FONT_H; r++)
        memset(s_atlas + ((size_t)r * ATLAS_W + SOLID * ATLAS_CELL) * 4, 255, PC_GFX_FONT_W * 4);
    s_atlas_made = 1;
}

/* A quad from window pixels (x0, y0)-(x1, y1), top left first, sampling atlas texels (u0, v0)-(u1, v1). */
static void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t rgba) {
    float c[4] = {(rgba >> 24) / 255.0f, (rgba >> 16 & 255) / 255.0f, (rgba >> 8 & 255) / 255.0f,
                  (rgba & 255) / 255.0f};
    float px[2] = {x0, x1}, py[2] = {y0, y1}, pu[2] = {u0 / ATLAS_W, u1 / ATLAS_W}, pv[2] = {v0 / ATLAS_H, v1 / ATLAS_H};
    static const int corner[6][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 1}};
    int i;
    if (s_n + 6 > s_cap) {
        GfxOutVtx *v = (GfxOutVtx *)realloc(s_v, sizeof(*v) * (size_t)(s_cap * 2 + 1536));
        if (v == NULL)
            return;
        s_v = v;
        s_cap = s_cap * 2 + 1536;
    }
    for (i = 0; i < 6; i++) {
        GfxOutVtx *o = &s_v[s_n++];
        int a = corner[i][0], b = corner[i][1];
        o->pos[0] = px[a] * 2.0f / s_w - 1.0f;
        o->pos[1] = 1.0f - py[b] * 2.0f / s_h;
        o->pos[2] = 0.0f;
        o->pos[3] = 1.0f;
        memcpy(o->shade, c, sizeof(c));
        o->uv[0] = pu[a];
        o->uv[1] = pv[b];
        o->uv[2] = o->uv[3] = 0.0f;
    }
}

void pc_gfx_ui_rect(int x, int y, int w, int h, uint32_t rgba) {
    float u = SOLID * ATLAS_CELL + 2.5f, v = PC_GFX_FONT_H / 2.0f;   /* the solid cell's middle */
    if (s_w > 0 && w > 0 && h > 0)
        quad((float)x, (float)y, (float)(x + w), (float)(y + h), u, v, u, v, rgba);
}

int pc_gfx_ui_text(int x, int y, int scale, uint32_t rgba, const char *text) {
    int x0 = x, k = scale < 1 ? 1 : scale;
    const unsigned char *p;
    for (p = (const unsigned char *)text; *p; p++) {
        int c = *p >= 32 && *p < 127 ? *p : '?';
        float u = (float)((c - 32) * ATLAS_CELL);
        /* a whole glyph per quad: at a whole scale each window pixel's center lands in its texel */
        if (s_w > 0 && c != ' ')
            quad((float)x, (float)y, (float)(x + PC_GFX_FONT_W * k), (float)(y + PC_GFX_FONT_H * k), u, 0.0f,
                 u + PC_GFX_FONT_W, (float)PC_GFX_FONT_H, rgba);
        x += PC_GFX_FONT_ADVANCE * k;
    }
    return x - x0;
}

void pcgfx_ui_run(int w, int h, int to_window) {
    if (s_hook == NULL || w <= 0 || h <= 0)
        return;
    if (!s_atlas_made)
        make_atlas();
    s_w = w;
    s_h = h;
    s_n = 0;
    s_hook(w, h);
    pcgfx_gl_ui_draw(s_v, s_n, w, h, to_window, s_atlas, ATLAS_W, ATLAS_H);
    s_w = s_h = 0;   /* drawing outside the hook does nothing */
}
