/* port/gfx/gfx_api.c: the pc_gfx_* interface (port/include/pc_port.h).
 *
 * Environment: RUSH_GFX_API=gl|gles (default gles), RUSH_GFX_SCALE=<n>|auto (internal resolution multiple; auto:
 * the largest the window shows whole),
 * RUSH_GFX_WIDESCREEN=off|auto|16:9|<w:h>|<ratio> (display aspect; auto: the window's),
 * RUSH_GFX_HEIGHT=<rows> (rows of the frame buffer to show; default: the largest scissor bottom drawn with),
 * RUSH_GFX_VERBOSE=1|2. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

GfxConfig pcgfx_cfg = {1, 1, 0.0f, 0.0f, 0, 0};

void pcgfx_rdp_new_frame(void);
void pcgfx_rdp_forget_targets(void);
void pcgfx_tex_cache_clear(void);

static struct {
    int inited, headless, use_sdl;
    int win_w, win_h;
} S;

static int env_int(const char *name, int def) {
    const char *v = getenv(name);
    return v != NULL && *v ? atoi(v) : def;
}

/* "off" (or 4:3) 0, "auto" -1, "W:H" or a ratio from 4:3 to 4. *ok = 0 for anything else (and 0 is returned). */
float pc_gfx_parse_aspect(const char *text, int *ok) {
    float a = 0.0f, b = 0.0f;
    char tail;
    *ok = 1;
    if (text == NULL || !*text || !strcmp(text, "off") || !strcmp(text, "0"))
        return 0.0f;
    if (!strcmp(text, "auto"))
        return -1.0f;
    if (sscanf(text, "%f:%f%c", &a, &b, &tail) == 2 && b > 0.0f)
        a /= b;
    else if (sscanf(text, "%f%c", &a, &tail) != 1)
        a = 0.0f;
    if (a >= 4.0f / 3.0f - 1e-4f && a <= 4.0f + 1e-4f)
        return a <= 4.0f / 3.0f + 1e-4f ? 0.0f : a;
    *ok = 0;
    return 0.0f;
}

/* The values in effect, taken at init and at each present, so a resize applies from the next frame's color image:
 * - the aspect: the setting, or the window's for auto;
 * - the scale: the setting, or for auto the largest whole multiple of the next frame (the last one's 4:3 frame,
 *   widened to that aspect, and its rows shown) that the window holds, so the present shows it pixel for pixel. */
static void resolve_auto(void) {
    GfxTarget *t = S.inited ? pcgfx_gl_last_target() : NULL;
    float a = pcgfx_cfg.widescreen;
    int s = pcgfx_cfg.internal_scale;
    if (a < 0.0f)
        a = S.win_h > 0 ? (float)S.win_w / (float)S.win_h : 0.0f;
    pcgfx_cfg.aspect = a > 4.0f / 3.0f + 1e-4f ? a : 0.0f;
    if (s <= 0) {
        int fw = 320, rows = pcgfx_cfg.frame_height > 0 ? pcgfx_cfg.frame_height : 240, w;
        if (t != NULL) {
            int tw, th;
            pcgfx_gl_target_shown(t, &tw, &rows);
            pcgfx_gl_target_dims(t, &tw, &th);
            fw = th * 4 / 3;
        }
        w = pcgfx_wide_width(fw);
        s = S.win_w / w < S.win_h / rows ? S.win_w / w : S.win_h / rows;
    }
    pcgfx_cfg.scale = s < 1 ? 1 : s > 8 ? 8 : s;
}

void pc_gfx_set_widescreen(float aspect) {
    pcgfx_cfg.widescreen = aspect;
    resolve_auto();
}

void pc_gfx_set_scale(int scale) {
    pcgfx_cfg.internal_scale = scale >= 0 && scale <= 8 ? scale : 1;
    resolve_auto();
}

void pc_gfx_set_window_size(int w, int h) {
    if (!S.use_sdl && w > 0 && h > 0) {
        S.win_w = w;
        S.win_h = h;
        resolve_auto();
    }
}

int pc_gfx_init(int headless, int window_width, int window_height) {
    GfxGetProc get_proc = NULL;
    const char *api = getenv("RUSH_GFX_API");
    int want_gles = !(api != NULL && strcmp(api, "gl") == 0), is_gles = want_gles;
    if (S.inited)
        return 0;
    if (getenv("RUSH_GFX_SCALE") != NULL && !strcmp(getenv("RUSH_GFX_SCALE"), "auto"))
        pcgfx_cfg.internal_scale = 0;
    else
        pcgfx_cfg.internal_scale = env_int("RUSH_GFX_SCALE", pcgfx_cfg.internal_scale);
    if (pcgfx_cfg.internal_scale < 0 || pcgfx_cfg.internal_scale > 8)
        pcgfx_cfg.internal_scale = 1;
    pcgfx_cfg.frame_height = env_int("RUSH_GFX_HEIGHT", pcgfx_cfg.frame_height);
    pcgfx_cfg.verbose = env_int("RUSH_GFX_VERBOSE", pcgfx_cfg.verbose);
    if (getenv("RUSH_GFX_WIDESCREEN") != NULL) {
        int ok;
        float a = pc_gfx_parse_aspect(getenv("RUSH_GFX_WIDESCREEN"), &ok);
        if (ok)
            pcgfx_cfg.widescreen = a;
        else
            fprintf(stderr, "gfx: RUSH_GFX_WIDESCREEN=%s is not off, auto or an aspect from 4:3 to 4; ignored\n",
                    getenv("RUSH_GFX_WIDESCREEN"));
    }
    S.win_w = window_width > 0 ? window_width : 960;
    S.win_h = window_height > 0 ? window_height : 720;
    S.headless = headless;
    S.use_sdl = 0;
    if (!headless) {
#ifdef PC_GFX_HAVE_SDL
        if (pcgfx_sdl_init(S.win_w, S.win_h, &get_proc, &is_gles) == 0)
            S.use_sdl = 1;
        else
            fprintf(stderr, "gfx: no window; rendering headless\n");
#else
        fprintf(stderr, "gfx: built without SDL2; rendering headless\n");
#endif
    }
    if (!S.use_sdl) {
        S.headless = 1;
        is_gles = want_gles;
        if (pcgfx_egl_init(want_gles, &get_proc, &is_gles) != 0)
            return -1;
    }
    if (pcgfx_gl_init(get_proc, is_gles) != 0) {
        pc_gfx_shutdown();
        return -1;
    }
    memset(&pcgfx, 0, sizeof(pcgfx));
    pcgfx_rdp_reset();
    pcgfx_rsp_reset();
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        pcgfx_sdl_size(&S.win_w, &S.win_h);
#endif
    resolve_auto();
    S.inited = 1;
    return 0;
}

void pc_gfx_run_dl(const void *dl) {
    if (!S.inited || dl == NULL)
        return;
    pcgfx.stat_cmds = pcgfx.stat_tris = pcgfx.stat_draws = pcgfx.stat_unknown = 0;
    pcgfx_run_list(dl);
    if (pcgfx_cfg.verbose)
        fprintf(stderr, "gfx: task: %u commands, %u triangles, %u draws, %u unknown\n", pcgfx.stat_cmds,
                pcgfx.stat_tris, pcgfx.stat_draws, pcgfx.stat_unknown);
}

static void window_size(void) {
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        pcgfx_sdl_size(&S.win_w, &S.win_h);
#endif
}

static void swap(void) {
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        pcgfx_sdl_swap();
#endif
}

void pc_gfx_present(const void *framebuffer) {
    GfxTarget *t;
    if (!S.inited)
        return;
    pcgfx_flush();
    t = pcgfx_gl_find_target(pcgfx_resolve((uintptr_t)framebuffer));
    if (t == NULL)
        t = pcgfx_gl_last_target();
    if (t != NULL) {
        window_size();
        pcgfx_gl_present(t, S.win_w, S.win_h, S.use_sdl);
        pcgfx_ui_run(S.win_w, S.win_h, S.use_sdl);
        swap();
    } else if (pcgfx_ui_active()) {   /* nothing drawn yet: the overlay over black */
        window_size();
        pcgfx_gl_show(S.win_w, S.win_h, S.use_sdl);
        pcgfx_ui_run(S.win_w, S.win_h, S.use_sdl);
        swap();
    }
    resolve_auto();
    pcgfx_rdp_new_frame();
}

/* The last presented frame again, from the presented image (not a render target, which the game may have drawn
 * into since), with the overlay over it. */
void pc_gfx_redraw(void) {
    if (!S.inited)
        return;
    pcgfx_flush();
    window_size();
    pcgfx_gl_show(S.win_w, S.win_h, S.use_sdl);
    pcgfx_ui_run(S.win_w, S.win_h, S.use_sdl);
    swap();
    resolve_auto();
}

static int (*s_tick)(void);

void pc_gfx_set_tick_hook(int (*tick)(void)) {
    s_tick = tick;
}

int pc_gfx_poll_events(void) {
    int quit = 0;
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        quit = pcgfx_sdl_poll();
#endif
    if (s_tick != NULL && s_tick() != 0)
        quit = 1;
    return quit;
}

int pc_gfx_save_screenshot(const char *path) {
    uint8_t *px;
    int w, h, rc;
    if (!S.inited || pcgfx_gl_read_presented(NULL, &w, &h) != 0)
        return -1;
    px = (uint8_t *)malloc((size_t)w * h * 4);
    if (px == NULL)
        return -1;
    rc = pcgfx_gl_read_presented(px, &w, &h);
    if (rc == 0)
        rc = pcgfx_png_write(path, px, w, h);
    free(px);
    return rc;
}

int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h) {
    GfxTarget *t;
    if (!S.inited)
        return -1;
    pcgfx_flush();
    t = pcgfx_gl_find_target(pcgfx_resolve((uintptr_t)cimg));
    if (t == NULL)
        t = pcgfx_gl_last_target();
    return pcgfx_gl_read_target(t, rgba, w, h);
}

int pc_gfx_read_window(uint8_t *rgba, int *w, int *h) {
    if (!S.inited)
        return -1;
    if (!S.use_sdl)   /* headless: the window's offscreen image */
        return pcgfx_gl_read_headless_window(rgba, w, h);
#ifdef PC_GFX_HAVE_SDL
    pcgfx_sdl_size(w, h);
#endif
    return rgba != NULL ? pcgfx_gl_read_window(rgba, *w, *h) : 0;
}

void pc_gfx_shutdown(void) {
    if (S.inited) {
        pcgfx_tex_cache_clear();
        pcgfx_rdp_forget_targets();
        pcgfx_gl_shutdown();
    }
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        pcgfx_sdl_shutdown();
    else
#endif
        pcgfx_egl_shutdown();
    memset(&S, 0, sizeof(S));
}
