/* port/gfx/gfx_detail.c: the depth range and the fog for video.draw_distance and video.fog (docs/port/DETAIL.md 3).
 *
 * The game's projection (zoid_draw_window: guPerspective with its near and far planes, the lookat multiplied in) makes
 * z/w a function of the eye's depth w alone: z/w = a + b / w, -1 at the near plane and 1 at the far one. The renderer
 * keeps that z/w for everything the game sees of it (the fog, G_BRANCH_Z, G_CULLDL) and changes only:
 *
 * - The depth GL stores, with draw_distance far or max: the game's far plane is moved out (PC_DRAW_FAR_FACTOR times,
 *   PC_DRAW_MAX_DEPTH times), with the near plane where it was. That is a linear map of the N64's depth for a given
 *   projection and viewport, d' = d * mul + add (d its screen z over 1023, or the primitive depth over 32767), exact
 *   per vertex since GL interpolates z/w linearly across the screen. Geometry past the game's far plane, which the
 *   N64 and the renderer at original draw at the far plane's depth (clamped), is then in order. The near plane is
 *   what sets the precision, and it doesn't move: with a 24-bit depth buffer, a depth step is d^2 / (n 2^24) at
 *   distance d, 0.1 ft at 2,000 ft, 0.6 ft at 5,000 and 2.4 ft at 10,000 (n = 2.5 ft).
 * - The fog, with fog far or off: far, the fog at eye depth w is the game's at w / PC_DRAW_FAR_FACTOR (its start and
 *   end that much further: z/w' = a + k (z/w - a)); off, none. With anything but original, the fog's z/w is held at
 *   the game's far plane (1): geometry past it, which the settings bring in, gets the far plane's fog, not the
 *   N64's s16 wrap of an unfogged view's fog factor beyond it (gSPFogPosition(1000, 999) is fm 3072, fo -3072).
 *
 * At original (all three) nothing here runs: the depth and the fog are the N64's, bit for bit. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

int pc_gfx_parse_draw_distance(const char *text, int *ok) {
    *ok = 1;
    if (text != NULL && !strcmp(text, "original"))
        return PC_DRAW_ORIGINAL;
    if (text != NULL && !strcmp(text, "far"))
        return PC_DRAW_FAR;
    if (text != NULL && !strcmp(text, "max"))
        return PC_DRAW_MAX;
    *ok = 0;
    return PC_DRAW_ORIGINAL;
}

int pc_gfx_parse_fog(const char *text, int *ok) {
    *ok = 1;
    if (text != NULL && !strcmp(text, "original"))
        return PC_FOG_ORIGINAL;
    if (text != NULL && !strcmp(text, "far"))
        return PC_FOG_FAR;
    if (text != NULL && !strcmp(text, "off"))
        return PC_FOG_OFF;
    *ok = 0;
    return PC_FOG_ORIGINAL;
}

int pc_gfx_parse_texture_lod(const char *text, int *ok) {
    *ok = 1;
    if (text != NULL && !strcmp(text, "original"))
        return PC_TEXLOD_ORIGINAL;
    if (text != NULL && !strcmp(text, "sharp"))
        return PC_TEXLOD_SHARP;
    if (text != NULL && !strcmp(text, "anisotropic"))
        return PC_TEXLOD_ANISO;
    *ok = 0;
    return PC_TEXLOD_ORIGINAL;
}

/* video.texture_lod: the shader's uLodScale and uLodTaps follow it from the next draw (gfx_gl.c) */
void pc_gfx_set_texture_lod(int mode) {
    pcgfx_cfg.texture_lod = mode == PC_TEXLOD_SHARP || mode == PC_TEXLOD_ANISO ? mode : PC_TEXLOD_ORIGINAL;
}

int pc_gfx_texture_lod(void) {
    return pcgfx_cfg.texture_lod;
}

void pc_gfx_set_draw_distance(int level) {
    pcgfx_cfg.draw_distance = level == PC_DRAW_FAR || level == PC_DRAW_MAX ? level : PC_DRAW_ORIGINAL;
    pcgfx.dz_dirty = 1;
}

void pc_gfx_set_fog(int mode) {
    pcgfx_cfg.fog = mode == PC_FOG_FAR || mode == PC_FOG_OFF ? mode : PC_FOG_ORIGINAL;
}

int pc_gfx_draw_distance(void) {
    return pcgfx_cfg.draw_distance;
}

int pc_gfx_fog(void) {
    return pcgfx_cfg.fog;
}

/* RUSH_GFX_DRAW_DISTANCE, RUSH_GFX_FOG, RUSH_GFX_TEXTURE_LOD (main sets them from video.draw_distance, fog and
 * texture_lod) */
void pcgfx_detail_init(void) {
    const char *e = getenv("RUSH_GFX_DRAW_DISTANCE");
    int ok, v;
    if (e != NULL && *e) {
        v = pc_gfx_parse_draw_distance(e, &ok);
        if (ok)
            pc_gfx_set_draw_distance(v);
        else
            fprintf(stderr, "gfx: RUSH_GFX_DRAW_DISTANCE=%s is not original, far or max; ignored\n", e);
    }
    e = getenv("RUSH_GFX_FOG");
    if (e != NULL && *e) {
        v = pc_gfx_parse_fog(e, &ok);
        if (ok)
            pc_gfx_set_fog(v);
        else
            fprintf(stderr, "gfx: RUSH_GFX_FOG=%s is not original, far or off; ignored\n", e);
    }
    e = getenv("RUSH_GFX_TEXTURE_LOD");
    if (e != NULL && *e) {
        v = pc_gfx_parse_texture_lod(e, &ok);
        if (ok)
            pc_gfx_set_texture_lod(v);
        else
            fprintf(stderr, "gfx: RUSH_GFX_TEXTURE_LOD=%s is not original, sharp or anisotropic; ignored\n", e);
    }
}

/* The projection's depth terms, when it changes: with row vectors, z = a w + b for every point, where w's column is
 * -(the lookat's third column) and z's is a times it plus b in the fourth row (guPerspective's (n + f) / (n - f) and
 * 2 n f / (n - f), the lookat rotation multiplied in). a and b from those columns; ok when they are a perspective
 * view's, near and far in front of the eye with far beyond near. */
void pcgfx_detail_projection(const float p[4][4]) {
    float ww = p[0][3] * p[0][3] + p[1][3] * p[1][3] + p[2][3] * p[2][3];
    float a, b;
    pcgfx.proj_depth = 0;
    pcgfx.dz_dirty = 1;
    if (!(ww > 1e-12f) || p[3][3] != 0.0f)
        return;
    a = (p[0][2] * p[0][3] + p[1][2] * p[1][3] + p[2][2] * p[2][3]) / ww;
    b = p[3][2];
    /* near = b / (-1 - a) > 0 and far = b / (1 - a) > near: b < 0, a > 1 */
    if (!(b < 0.0f) || !(a > 1.0f))
        return;
    pcgfx.proj_za = a;
    pcgfx.proj_zb = b;
    pcgfx.proj_depth = 1;
}

/* The depth map for the projection and viewport in effect (draw_distance far or max): d' = d * mul + add. */
static void dz_update(void) {
    double a = pcgfx.proj_za, b = pcgfx.proj_zb, n, f, f2, a2, b2, al, be, t, s;
    pcgfx.dz_dirty = 0;
    pcgfx.dz_on = 0;
    if (pcgfx_cfg.draw_distance == PC_DRAW_ORIGINAL || !pcgfx.proj_depth || pcgfx.mp_forced)
        return;
    n = b / (-1.0 - a);
    f = b / (1.0 - a);
    if (!(n > 0.0) || !(f > n))
        return;
    f2 = f * (pcgfx_cfg.draw_distance == PC_DRAW_MAX ? PC_DRAW_MAX_DEPTH : PC_DRAW_FAR_FACTOR);
    a2 = (f2 + n) / (f2 - n);
    b2 = -2.0 * f2 * n / (f2 - n);
    al = b2 / b;          /* z/w' = al z/w + be: the same w, the far plane at f2 */
    be = a2 - al * a;
    t = pcgfx.vp_trans[2];
    s = pcgfx.vp_scale[2];
    pcgfx.dz_mul = (float)al;
    pcgfx.dz_add = (float)((t * (1.0 - al) + s * be) / 1023.0);
    pcgfx.dz_on = 1;
}

/* After view_x (gfx_rsp.c's tri_ctx). */
void pcgfx_detail_ctx(GfxTriCtx *c) {
    if (pcgfx_cfg.draw_distance == PC_DRAW_ORIGINAL) {
        c->dz_on = 0;
        c->dz_mul = 1.0f;
        c->dz_add = 0.0f;
        return;
    }
    if (pcgfx.dz_dirty)
        dz_update();
    c->dz_on = pcgfx.dz_on;
    c->dz_mul = pcgfx.dz_mul;
    c->dz_add = pcgfx.dz_add;
}

/* A rectangle's primitive depth (gfx_rdp.c), in the 3D view it is drawn in: mapped as that view's triangles are. */
float pcgfx_detail_rect_depth(float d) {
    if (pcgfx_cfg.draw_distance == PC_DRAW_ORIGINAL)
        return d;
    if (pcgfx.dz_dirty)
        dz_update();
    return pcgfx.dz_on ? d * pcgfx.dz_mul + pcgfx.dz_add : d;
}

/* G_FOG's shade alpha: clamp(z/w fm + fo, 0, 255) / 255 (GFX.md "Fog"), za the projection's z/w at infinity (0 when it
 * has none: no fog far then). */
float pcgfx_fog_alpha(float z, float w, int16_t fog_mul, int16_t fog_off, float za) {
    float f, zw;
    if (w < 1e-4f)   /* GFX_W_EPS, the eye plane (gfx_rsp.c) */
        w = 1e-4f;
    if (pcgfx_cfg.fog == PC_FOG_ORIGINAL && pcgfx_cfg.draw_distance == PC_DRAW_ORIGINAL) {
        f = z / w * (float)fog_mul + (float)fog_off;
    } else {
        if (pcgfx_cfg.fog == PC_FOG_OFF)
            return 0.0f;
        zw = z / w;
        if (pcgfx_cfg.fog == PC_FOG_FAR && za > 1.0f)
            zw = za + (zw - za) * PC_DRAW_FAR_FACTOR;
        if (zw > 1.0f)
            zw = 1.0f;
        f = zw * (float)fog_mul + (float)fog_off;
    }
    if (f < 0.0f) f = 0.0f;
    if (f > 255.0f) f = 255.0f;
    return f / 255.0f;
}

/* The graphics tasks' triangles (pc_gfx_run_dl: pcgfx.stat_tris after each), for the exit's scene line. */
static uint32_t s_tasks, s_most;
static uint64_t s_tris;

void pcgfx_detail_task(uint32_t triangles) {
    s_tasks++;
    s_tris += triangles;
    if (triangles > s_most)
        s_most = triangles;
}

void pc_gfx_scene_stats(uint32_t *tasks, uint64_t *triangles, uint32_t *most) {
    *tasks = s_tasks;
    *triangles = s_tris;
    *most = s_most;
}
