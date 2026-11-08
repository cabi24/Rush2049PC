/* port/gfx/gfx_view.c: one view of the game's split screen alone, across the whole picture (net.view = own; docs/port/
 * NETPLAY.md 14, docs/port/GFX.md "One view of the split screen").
 *
 * In an online session both PCs run the same two-player game, split screen and all: the game's logic is per window
 * (cameras, effects, the scene walk, the random numbers they draw), so both PCs must run both windows (NETPLAY.md 1.4).
 * What each PC shows is the renderer's: with own view on, it draws its own player's view alone, filling the frame,
 * and that player's HUD at the frame's edges. The game's display lists are the same; nothing here reaches the game.
 *
 * - When: a race (the countdown and the race, its pause screen too) with several views, as main.c's hook says
 *   (pc_gfx_set_race_views: the views' rectangles, from the game's own window records), and a player to show
 *   (pc_gfx_set_view_player: the session's; RUSH_GFX_VIEW for tests and tools). Anything else is drawn as before.
 * - 3D: a perspective draw is the view whose rectangle holds its viewport's centre. Another view's is not drawn. The
 *   view shown is zoomed (its width to the frame's, keeping its field of view across) and centred in the frame, and
 *   its frustum made taller (or wider) to the frame's shape: the picture shows more above and below than the view did,
 *   as the game's own one-player view of the same width would (gfx_rsp.c's outcodes take ys, as widescreen's take
 *   xs). Widescreen then widens it as any view across the frame. Its scissor goes with it.
 * - Fills (fill and copy mode): one across the frame (the clears) as before; one in the view shown, mapped as its
 *   picture; any other (the dividers between the views, the side panel's background, another view's) not drawn.
 * - 2D: queued with the HUD's (gfx_hud.c) and moved by pcgfx_view_move: a piece of the view shown keeps its distance to
 *   the frame's side it is nearest, as the HUD at the screen's edges does; another view's piece is not drawn; the side
 *   panel's map and race clock go to the picture's right side, its background is not drawn; what is drawn after a
 *   piece across the whole frame (the pause screen's darkening: its title and menu, whose lines each fall in one
 *   view) is the frame's.
 * - The models the game places in 3D at HUD points (the coin icons beside their counts: ZOID_WindowTo3D) go with
 *   their point's 2D rather than with the zoomed picture (pcgfx_view_hud_point).
 * Off (split, the default; no session; one view) nothing here runs and the drawing is the same as without this file. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

#define VIEW_EDGE 4.0f   /* a rectangle's side this near a view's is at the view's side (the 1-2 pixel borders) */

static struct {
    int mode;      /* PC_GFX_VIEW_SPLIT, PC_GFX_VIEW_OWN */
    int player;    /* the view own shows, 1-4: the session's player; 0 none */
    int (*views)(int rect[][4], int max);
    /* this task */
    int on;        /* own view in effect */
    int n, own;    /* the race's views; the one shown, 0-based */
    float pane[PCGFX_VIEWS_MAX][4];   /* each view's rectangle in the frame: x0, y0, x1, y1 (its scissor) */
    float rows;    /* the frame's rows: the views' bottom */
    /* the view shown's last viewport: its zoom and centre (pcgfx_view_place), for rectangles mapped as its picture */
    float k, cx, cy;
    /* RUSH_GFX_VIEW_LOG: the models placed at HUD points in the last task, and the last one's point */
    int log, points;
    float px, py;
} V;

int pc_gfx_parse_view(const char *text, int *ok) {
    *ok = 1;
    if (text != NULL && !strcmp(text, "split"))
        return PC_GFX_VIEW_SPLIT;
    if (text != NULL && !strcmp(text, "own"))
        return PC_GFX_VIEW_OWN;
    *ok = 0;
    return PC_GFX_VIEW_SPLIT;
}

void pc_gfx_set_view(int mode) {
    V.mode = mode == PC_GFX_VIEW_OWN ? PC_GFX_VIEW_OWN : PC_GFX_VIEW_SPLIT;
}

int pc_gfx_view(void) {
    return V.mode;
}

void pc_gfx_set_view_player(int player) {
    V.player = player >= 1 && player <= PCGFX_VIEWS_MAX ? player : 0;
}

void pc_gfx_set_race_views(int (*views)(int rect[][4], int max)) {
    V.views = views;
}

/* RUSH_GFX_VIEW=N (1-4): view N alone in any game with several views, local too (tests and tools). */
void pcgfx_view_init(void) {
    const char *e = getenv("RUSH_GFX_VIEW"), *l = getenv("RUSH_GFX_VIEW_LOG");
    V.log = l != NULL ? atoi(l) : 0;
    if (e != NULL && *e) {
        int n = atoi(e);
        if (n >= 1 && n <= PCGFX_VIEWS_MAX) {
            pc_gfx_set_view(PC_GFX_VIEW_OWN);
            pc_gfx_set_view_player(n);
        } else
            fprintf(stderr, "gfx: RUSH_GFX_VIEW=%s is not a view from 1 to %d; ignored\n", e, PCGFX_VIEWS_MAX);
    }
}

void pcgfx_view_task_begin(void) {
    int r[PCGFX_VIEWS_MAX][4], i, n;
    if (V.on && V.log > 0) {   /* RUSH_GFX_VIEW_LOG (with gfx_hud.c's lines): the last task's models at HUD points */
        V.log--;
        fprintf(stderr, "gfx: own view: %d draws of models at HUD points%s", V.points, V.points ? "" : "\n");
        if (V.points)
            fprintf(stderr, ", the last at (%.0f, %.0f)\n", V.px, V.py);
    }
    V.points = 0;
    V.on = 0;
    if (V.mode != PC_GFX_VIEW_OWN || V.player == 0 || V.views == NULL)
        return;
    n = V.views(r, PCGFX_VIEWS_MAX);
    if (n < 2 || n > PCGFX_VIEWS_MAX || V.player > n)
        return;
    V.rows = 0.0f;
    for (i = 0; i < n; i++) {
        int k;
        if (r[i][2] <= r[i][0] || r[i][3] <= r[i][1])
            return;   /* not set up yet */
        for (k = 0; k < 4; k++)
            V.pane[i][k] = (float)r[i][k];
        if (V.pane[i][3] > V.rows)
            V.rows = V.pane[i][3];
    }
    V.n = n;
    V.own = V.player - 1;
    V.k = 0.0f;
    V.on = 1;
}

int pcgfx_view_on(void) {
    return V.on;
}

int pcgfx_view_shown(void) {
    return V.on ? V.own : -1;
}

static float frame_w(void) {
    return (float)pcgfx_frame_width();
}

/* The view whose rectangle holds (x, y), the frame's N64 pixels; -1 none. */
static int view_at(float x, float y) {
    int i;
    for (i = 0; i < V.n; i++)
        if (x >= V.pane[i][0] - 1.0f && x <= V.pane[i][2] + 1.0f && y >= V.pane[i][1] - 1.0f &&
            y <= V.pane[i][3] + 1.0f)
            return i;
    return -1;
}

int pcgfx_view_of(const float trans[3]) {
    int i;
    if (!V.on)
        return -1;
    i = view_at(trans[0], trans[1]);
    return i < 0 ? -1 : i == V.own;
}

/* The view shown: its viewport (scale, trans: N64 pixels) zoomed by k, the most that keeps it within the frame on both
 * axes (its width to the frame's for the game's two-player views), and centred in the frame. What it shows then
 * reaches the frame's sides: half[] is the frame's half size, which the viewport's zoomed half size is within. */
void pcgfx_view_place(const float scale[3], const float trans[3], float s_out[2], float t_out[2], float half[2]) {
    float hw = 0.5f * frame_w(), hh = 0.5f * V.rows, ax = fabsf(scale[0]), ay = fabsf(scale[1]), k;
    k = ax > 0.0f ? hw / ax : 1.0f;
    if (ay > 0.0f && hh / ay < k)
        k = hh / ay;
    s_out[0] = scale[0] * k;
    s_out[1] = scale[1] * k;
    t_out[0] = hw;
    t_out[1] = hh;
    half[0] = hw;
    half[1] = hh;
    V.k = k;
    V.cx = trans[0];
    V.cy = trans[1];
}

/* An edge v of the view shown's rectangle (lo, hi on that axis) where the view's picture puts it: at the view's sides
 * the frame's (0, size), else zoomed about the viewport's centre c as the picture is. */
static float map_edge(float v, float lo, float hi, float c, float size) {
    if (fabsf(v - lo) <= VIEW_EDGE || v < lo)
        return 0.0f;
    if (fabsf(v - hi) <= VIEW_EDGE || v > hi)
        return size;
    if (V.k <= 0.0f)
        return v;
    v = 0.5f * size + (v - c) * V.k;
    return v < 0.0f ? 0.0f : v > size ? size : v;
}

void pcgfx_view_rect(int32_t r[4]) {
    const float *p = V.pane[V.own];
    float cx = V.k > 0.0f ? V.cx : 0.5f * (p[0] + p[2]), cy = V.k > 0.0f ? V.cy : 0.5f * (p[1] + p[3]);
    r[0] = (int32_t)floorf(map_edge((float)r[0], p[0], p[2], cx, frame_w()) + 0.5f);
    r[2] = (int32_t)floorf(map_edge((float)r[2], p[0], p[2], cx, frame_w()) + 0.5f);
    r[1] = (int32_t)floorf(map_edge((float)r[1], p[1], p[3], cy, V.rows) + 0.5f);
    r[3] = (int32_t)floorf(map_edge((float)r[3], p[1], p[3], cy, V.rows) + 0.5f);
}

int pcgfx_view_fill(int32_t r[4]) {
    const float *p = V.pane[V.own];
    float fw = frame_w();
    if (r[0] <= VIEW_EDGE && r[2] >= fw - VIEW_EDGE && r[1] <= VIEW_EDGE && r[3] >= V.rows - VIEW_EDGE)
        return 1;   /* across the frame: a clear */
    if (r[0] >= p[0] - VIEW_EDGE && r[2] <= p[2] + VIEW_EDGE && r[1] >= p[1] - VIEW_EDGE && r[3] <= p[3] + VIEW_EDGE) {
        pcgfx_view_rect(r);
        return r[2] > r[0] && r[3] > r[1];
    }
    return 0;   /* a divider, the side panel's background, another view's */
}

/* The views' right side: the side panel (the two-player screen's, x 240-320) is right of it. */
static float views_right(void) {
    float r = 0.0f;
    int i;
    for (i = 0; i < V.n; i++)
        if (V.pane[i][2] > r)
            r = V.pane[i][2];
    return r;
}

int pcgfx_view_covers(float x0, float y0, float x1, float y1) {
    return x0 <= VIEW_EDGE && x1 >= frame_w() - VIEW_EDGE && y0 <= VIEW_EDGE && y1 >= V.rows - VIEW_EDGE;
}

int pcgfx_view_box(float x0, float y0, float x1, float y1) {
    int i;
    for (i = 0; i < V.n; i++) {
        const float *p = V.pane[i];
        if (x0 >= p[0] - VIEW_EDGE && x1 <= p[2] + VIEW_EDGE && y0 >= p[1] - VIEW_EDGE && y1 <= p[3] + VIEW_EDGE)
            return i;
    }
    return -1;
}

/* A 2D piece's move (GFX.md "One view of the split screen"). The picture's sides in the target: the target's with
 * video.hud = edges in widescreen, the 4:3 frame's otherwise, as the one-view HUD at the screen's edges has them.
 * - The view shown's pieces (its HUD: the speedometer, the place, the laps, the coin counts, a banner) keep their
 *   distance to the view's side they are nearest: across, by thirds of the view's width (left, middle, right; a group
 *   wider than half the view is in the middle); down, the bottom third goes with the frame's bottom, a piece in the
 *   middle third that is also in the middle across goes to the frame's middle (a centred message), the rest with the
 *   frame's top. So the HUD sits at the frame's edges as the view's HUD sat at the view's, at its own size.
 * - Another view's pieces are not drawn.
 * - The frame's (in no one view): right of the views, the side panel's map and race clock go to the picture's right
 *   side; a piece across the side panel's whole width is its background and is not drawn; anything else (the pause
 *   screen, the results' header across the views) stays where the game puts it in the 4:3 frame. */
int pcgfx_view_move(int v, float x0, float y0, float x1, float y1, int ox, int edges, int *dx, int *dy) {
    float fw = frame_w(), dl = edges ? 0.0f : (float)ox, dr = edges ? fw + 2.0f * ox : fw + (float)ox;
    *dx = *dy = 0;
    if (v >= 0 && v != V.own)
        return -1;
    if (v < 0) {
        float right = views_right();
        if (x0 >= right - VIEW_EDGE) {
            if (x0 <= right + VIEW_EDGE && x1 >= fw - VIEW_EDGE)
                return -1;
            *dx = (int)lrintf(dr - fw - (float)ox);
        }
        return 0;
    } else {
        const float *p = V.pane[v];
        float pw = p[2] - p[0], ph = p[3] - p[1], cx = 0.5f * (x0 + x1), cy = 0.5f * (y0 + y1);
        int across = x1 - x0 > 0.5f * pw ? 0 : cx < p[0] + pw / 3.0f ? -1 : cx > p[2] - pw / 3.0f ? 1 : 0;
        if (across < 0)
            *dx = (int)lrintf(dl - p[0] - (float)ox);
        else if (across > 0)
            *dx = (int)lrintf(dr - p[2] - (float)ox);
        else
            *dx = (int)lrintf(0.5f * (dl + dr) - 0.5f * (p[0] + p[2]) - (float)ox);
        if (cy > p[3] - ph / 3.0f && y1 - y0 <= 0.5f * ph)
            *dy = (int)lrintf(V.rows - p[3]);
        else if (across == 0 && (cy >= p[1] + ph / 3.0f || y1 - y0 > 0.5f * ph))
            *dy = (int)lrintf(0.5f * V.rows - 0.5f * (p[1] + p[3]));
        else
            *dy = (int)lrintf(-p[1]);
        return 0;
    }
}

/* A model the game places in 3D at a HUD point (ZOID_WindowTo3D, ZOID_WindowTo3D.c: the coin icons beside their
 * counts, the battle and stunt status models): its place is the HUD point unprojected 2 ft (5.75 for one battle kind)
 * in front of the eye, so its transform's origin is at that depth, w 32 or 92 in the game's sixteenths of a foot, and
 * on screen in its view. It goes where its point's 2D goes (pcgfx_view_move), at the view's own scale, instead of
 * with the zoomed picture: so it stays beside its count. mp: the transform; scale, trans: the view's viewport. *x, *y:
 * where its point's move puts the viewport's centre, the target's N64 pixels. */
int pcgfx_view_hud_point(const float mp[4][4], const float scale[3], const float trans[3], int ox, int edges,
                         float *x, float *y) {
    float w = mp[3][3], px, py;
    int dx, dy;
    if (!((w > 31.0f && w < 33.0f) || (w > 91.0f && w < 93.0f)))
        return 0;
    px = mp[3][0] / w;
    py = mp[3][1] / w;
    if (px < -1.0f || px > 1.0f || py < -1.0f || py > 1.0f)
        return 0;
    px = trans[0] + px * scale[0];   /* its point in the frame */
    py = trans[1] - py * scale[1];
    if (pcgfx_view_move(V.own, px, py, px, py, ox, edges, &dx, &dy) != 0)
        return 0;
    V.points++;
    V.px = px;
    V.py = py;
    *x = trans[0] + (float)ox + (float)dx;
    *y = trans[1] + (float)dy;
    return 1;
}
