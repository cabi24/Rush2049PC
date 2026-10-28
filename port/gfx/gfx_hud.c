/* port/gfx/gfx_hud.c: the race HUD at the screen's edges in widescreen (video.hud = edges; docs/port/GFX.md "The HUD
 * at the screen's edges").
 *
 * In widescreen the 2D stays in the 4:3 frame, in the middle of the wider picture (GFX.md "Widescreen"). During a
 * race with one view (pc_gfx_set_hud_race: main.c reads the game's state), with the setting on edges, the 2D that the
 * game draws at the frame's left or right side moves out by the widening instead, so that it keeps its distance to
 * the screen's edge: the speedometer to the left edge, the position, laps, map and timer to the right one.
 *
 * Pieces move as wholes. While it is on, the task's 2D primitives (texture rectangles, 1/2-cycle fills, orthographic
 * triangles) are queued here instead of drawn, until something else is drawn (3D, a clear, another color image) or
 * the task ends. Then they are grouped, each group gets one move, and they are drawn in their order:
 *   - layers: a panel (a box at least half the frame wide and an eighth of it high: the pause menu's darkening, a
 *     menu's pane) starts a new layer; what is drawn after it belongs to it, and never to what is under it;
 *   - groups: within a layer, pieces whose boxes overlap or nearly touch (8 pixels apart on a line, 3 between lines)
 *     are one group: a speedometer's frame and digits, a text run, the radar and its blips, a pane and its text;
 *   - the move: a group wider than half the frame stays; else by its box's centre against the frame's middle, with
 *     a band of a sixth of the frame on each side that stays (centred banners, menus): left of it the group moves
 *     left by the widening, right of it right. Its scissor moves with it.
 * Fill-mode rectangles (clears) are not queued: they stay as GFX.md describes. The game also places a few 3D models
 * at HUD points (the coin icons beside their counts, ZOID_WindowTo3D): on the PC that function moves its point by
 * pc_gfx_hud_dx, the same rule for a point.
 * Off (centered, 4:3, a menu, split screen) nothing here runs and the drawing is the same as without this file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

#define HUD_MAX_ITEMS 2048
#define HUD_GAP_X 8.0f      /* pieces on one line this far apart are one group (a text run's word spaces) */
#define HUD_GAP_Y 3.0f      /* and lines this far apart */

typedef struct {
    uint32_t state;         /* its draw state, in S.st */
    uint32_t first, count;  /* its vertices, in S.v (as drawn 4:3) and S.x43 */
    float x0, y0, x1, y1;   /* its box in the 4:3 frame, N64 pixels (within its scissor) */
    int32_t sc_x0, sc_x1;   /* its scissor's sides as 2D has them (the frame's, moved with the frame) */
    int tw;                 /* the target's width, N64 pixels */
    int ox;                 /* the widening */
    int fw;                 /* the frame's width */
    int parent;             /* union-find */
    uint16_t layer;
    uint8_t movable, tri;
} HudItem;

static struct {
    int mode;               /* PC_GFX_HUD_* */
    int (*race)(void);
    int on;                 /* this task: edges, in a race with one view */
    HudItem *it;
    int n, cap;
    GfxOutVtx *v;
    float *x43;             /* each vertex's x in target N64 pixels at the 4:3 place (rectangles) */
    uint32_t nv, cv, cx;
    GfxDrawState *st;
    uint32_t nst, cst;
    GfxOutVtx *out;         /* a run of vertices to draw */
    uint32_t cout;
    int dx[HUD_MAX_ITEMS];
    uint16_t layer;         /* panels so far in this run */
} S = {.mode = PC_GFX_HUD_EDGES};

void pc_gfx_set_hud(int mode) {
    S.mode = mode == PC_GFX_HUD_CENTERED ? PC_GFX_HUD_CENTERED : PC_GFX_HUD_EDGES;
}

int pc_gfx_hud(void) {
    return S.mode;
}

int pc_gfx_parse_hud(const char *text, int *ok) {
    *ok = 1;
    if (text != NULL && (!strcmp(text, "edges") || !strcmp(text, "edge")))
        return PC_GFX_HUD_EDGES;
    if (text != NULL && (!strcmp(text, "centered") || !strcmp(text, "centred") || !strcmp(text, "center")))
        return PC_GFX_HUD_CENTERED;
    *ok = 0;
    return PC_GFX_HUD_EDGES;
}

void pc_gfx_set_hud_race(int (*race)(void)) {
    S.race = race;
}

void pcgfx_hud_init(void) {
    const char *e = getenv("RUSH_GFX_HUD");
    if (e != NULL && *e) {
        int ok, m = pc_gfx_parse_hud(e, &ok);
        if (ok)
            S.mode = m;
        else
            fprintf(stderr, "gfx: RUSH_GFX_HUD=%s is not edges or centered; ignored\n", e);
    }
}

void pcgfx_hud_task_begin(void) {
    S.on = S.mode == PC_GFX_HUD_EDGES && S.race != NULL && S.race();
    S.n = 0;
    S.nv = S.nst = 0;
    S.layer = 0;
}

int pc_gfx_hud_dx(int x) {
    int fw = pcgfx_frame_width(), ox = (pcgfx_wide_width(fw) - fw) / 2;
    if (S.mode != PC_GFX_HUD_EDGES || S.race == NULL || ox == 0 || !S.race())
        return 0;
    if (6 * x < 2 * fw)        /* left of the middle band (fw / 2 - fw / 6) */
        return -ox;
    if (6 * x > 4 * fw)        /* right of it */
        return ox;
    return 0;
}

int pcgfx_hud_on(void) {
    return S.on && pcgfx.wide_ox != 0;
}

int pcgfx_hud_pending(void) {
    return S.n > 0;
}

static int grow(void **p, uint32_t *cap, uint32_t need, size_t size) {
    uint32_t c;
    void *q;
    if (need <= *cap)
        return 0;
    c = *cap ? *cap : 256;
    while (c < need)
        c *= 2;
    q = realloc(*p, (size_t)c * size);
    if (q == NULL)
        return -1;
    *p = q;
    *cap = c;
    return 0;
}

/* The state, kept once for a run of items that share it. */
static uint32_t keep_state(const GfxDrawState *st) {
    if (S.nst > 0 && memcmp(&S.st[S.nst - 1], st, sizeof(*st)) == 0)
        return S.nst - 1;
    if (grow((void **)&S.st, &S.cst, S.nst + 1, sizeof(*S.st)))
        return 0xFFFFFFFFu;
    S.st[S.nst] = *st;
    return S.nst++;
}

int pcgfx_hud_add(const GfxDrawState *st, const GfxOutVtx *v, const float *x43, int n, int movable, int tri) {
    HudItem *it;
    uint32_t si;
    int i, tw, th, fw = pcgfx_frame_width(), ox = pcgfx.wide_ox;
    float sx0, sx1, sy0, sy1;
    if (S.n == HUD_MAX_ITEMS)
        pcgfx_hud_resolve();
    if (grow((void **)&S.v, &S.cv, S.nv + (uint32_t)n, sizeof(*S.v)) ||
        grow((void **)&S.x43, &S.cx, S.nv + (uint32_t)n, sizeof(*S.x43)))
        return -1;
    if (S.n >= S.cap) {
        int c = S.cap ? S.cap * 2 : 256;
        HudItem *q = (HudItem *)realloc(S.it, (size_t)c * sizeof(*q));
        if (q == NULL)
            return -1;
        S.it = q;
        S.cap = c;
    }
    si = keep_state(st);
    if (si == 0xFFFFFFFFu)
        return -1;
    pcgfx_target_dims(&tw, &th);
    it = &S.it[S.n];
    memset(it, 0, sizeof(*it));
    it->state = si;
    it->first = S.nv;
    it->count = (uint32_t)n;
    it->tw = tw;
    it->ox = ox;
    it->fw = fw;
    it->tri = (uint8_t)tri;
    it->x0 = it->y0 = 1e30f;
    it->x1 = it->y1 = -1e30f;
    for (i = 0; i < n; i++) {
        float w = v[i].pos[3] != 0.0f ? v[i].pos[3] : 1.0f;
        float x = x43 != NULL ? x43[i] : (v[i].pos[0] / w + 1.0f) * 0.5f * (float)tw;
        float y = (1.0f - v[i].pos[1] / w) * 0.5f * (float)th;
        S.v[S.nv + i] = v[i];
        S.x43[S.nv + i] = x;
        x -= (float)ox;   /* the frame's */
        if (x < it->x0) it->x0 = x;
        if (x > it->x1) it->x1 = x;
        if (y < it->y0) it->y0 = y;
        if (y > it->y1) it->y1 = y;
    }
    S.nv += (uint32_t)n;
    /* the box within its scissor (what the N64 shows of it); an empty one keeps its own */
    sx0 = (float)(pcgfx.sc_ulx >> 2);
    sx1 = (float)(pcgfx.sc_lrx >> 2);
    sy0 = (float)(pcgfx.sc_uly >> 2);
    sy1 = (float)(pcgfx.sc_lry >> 2);
    if (it->x0 < sx1 && it->x1 > sx0 && it->y0 < sy1 && it->y1 > sy0) {
        if (it->x0 < sx0) it->x0 = sx0;
        if (it->x1 > sx1) it->x1 = sx1;
        if (it->y0 < sy0) it->y0 = sy0;
        if (it->y1 > sy1) it->y1 = sy1;
    }
    it->sc_x0 = (int32_t)(pcgfx.sc_ulx >> 2) + ox;
    it->sc_x1 = (int32_t)(pcgfx.sc_lrx >> 2) + ox;
    it->movable = (uint8_t)movable;   /* the HUD's translucent panels are drawn with the primitive depth too */
    /* a panel starts a layer: it and what comes after it never group with what is under it */
    if (it->x1 - it->x0 >= 0.5f * (float)fw && it->y1 - it->y0 >= (float)fw * 0.75f / 8.0f)
        S.layer++;
    it->layer = S.layer;
    it->parent = S.n;
    S.n++;
    return 0;
}

static int root(int i) {
    while (S.it[i].parent != i) {
        S.it[i].parent = S.it[S.it[i].parent].parent;
        i = S.it[i].parent;
    }
    return i;
}

static int near_boxes(const HudItem *a, const HudItem *b) {
    float gx = a->x0 > b->x1 ? a->x0 - b->x1 : b->x0 > a->x1 ? b->x0 - a->x1 : 0.0f;
    float gy = a->y0 > b->y1 ? a->y0 - b->y1 : b->y0 > a->y1 ? b->y0 - a->y1 : 0.0f;
    return gx <= HUD_GAP_X && gy <= HUD_GAP_Y;
}

/* Each item's move, N64 pixels: its group's (GFX.md "The HUD at the screen's edges"). */
static void place(void) {
    int i, j;
    for (i = 0; i < S.n; i++)
        for (j = i + 1; j < S.n && S.it[j].layer == S.it[i].layer; j++)
            if (S.it[i].movable && S.it[j].movable && near_boxes(&S.it[i], &S.it[j])) {
                int a = root(i), b = root(j);
                if (a != b)
                    S.it[a > b ? a : b].parent = a < b ? a : b;
            }
    /* the group's box, kept in its root */
    for (i = 0; i < S.n; i++) {
        HudItem *it = &S.it[i], *r = &S.it[root(i)];
        if (r == it || !it->movable)
            continue;
        if (it->x0 < r->x0) r->x0 = it->x0;
        if (it->x1 > r->x1) r->x1 = it->x1;
    }
    for (i = 0; i < S.n; i++) {
        const HudItem *it = &S.it[i], *r = &S.it[root(i)];
        float half = 0.5f * (float)it->fw, band = (float)it->fw / 6.0f, c = 0.5f * (r->x0 + r->x1);
        S.dx[i] = 0;
        if (!it->movable || r->x1 - r->x0 > half)
            continue;
        if (c < half - band)
            S.dx[i] = -it->ox;
        else if (c > half + band)
            S.dx[i] = it->ox;
    }
}

static void draw_run(const GfxDrawState *st, uint32_t n) {
    if (n == 0)
        return;
    pcgfx_gl_draw(st, S.out, (int)n);
    pcgfx.stat_draws++;
    if (pcgfx_rec != NULL)
        pcgfx_rec_verts(st, S.out, (int)n);
}

void pcgfx_hud_resolve(void) {
    GfxDrawState run_st;
    uint32_t run_n = 0;
    int i, run_state = -1, run_dx = 0;
    if (S.n == 0)
        return;
    pcgfx_flush();   /* what was drawn before the queue */
    place();
    if (grow((void **)&S.out, &S.cout, S.nv, sizeof(*S.out)) == 0) {
        for (i = 0; i < S.n; i++) {
            const HudItem *it = &S.it[i];
            int dx = S.dx[i];
            uint32_t k;
            if ((int)it->state != run_state || dx != run_dx) {
                draw_run(&run_st, run_n);
                run_n = 0;
                run_state = (int)it->state;
                run_dx = dx;
                run_st = S.st[it->state];
                if (dx != 0) {   /* the 2D scissor, moved with it */
                    run_st.scissor[0] = it->sc_x0 + dx;
                    run_st.scissor[2] = it->sc_x1 + dx;
                }
            }
            for (k = 0; k < it->count; k++) {
                GfxOutVtx *o = &S.out[run_n + k];
                *o = S.v[it->first + k];
                if (dx == 0)
                    continue;
                if (it->tri)   /* clip x: w * 2 / tw per N64 pixel */
                    o->pos[0] += (float)dx * 2.0f / (float)it->tw * o->pos[3];
                else           /* a rectangle's corner, as draw_rect places it */
                    o->pos[0] = (S.x43[it->first + k] + (float)dx) * 2.0f / (float)it->tw - 1.0f;
            }
            run_n += it->count;
        }
        draw_run(&run_st, run_n);
    }
    S.n = 0;
    S.nv = S.nst = 0;
    S.layer = 0;
}
