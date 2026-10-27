/* port/gfx/gfx_api.c: the pc_gfx_* interface (port/include/pc_port.h).
 *
 * Environment: RUSH_GFX_API=gl|gles (default gles), RUSH_GFX_SCALE=<n> (internal resolution multiple),
 * RUSH_GFX_HEIGHT=<rows> (rows of the frame buffer to show; default: the largest scissor bottom drawn with),
 * RUSH_GFX_VERBOSE=1|2. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

GfxConfig g_gfx_cfg = {1, 0, 0, 0};

void gfx_rdp_new_frame(void);
void gfx_rdp_forget_targets(void);
void gfx_tex_cache_clear(void);

static struct {
    int inited, headless, use_sdl;
    int win_w, win_h;
} S;

static int env_int(const char *name, int def) {
    const char *v = getenv(name);
    return v != NULL && *v ? atoi(v) : def;
}

int pc_gfx_init(int headless, int window_width, int window_height) {
    GfxGetProc get_proc = NULL;
    const char *api = getenv("RUSH_GFX_API");
    int want_gles = !(api != NULL && strcmp(api, "gl") == 0), is_gles = want_gles;
    if (S.inited)
        return 0;
    g_gfx_cfg.internal_scale = env_int("RUSH_GFX_SCALE", g_gfx_cfg.internal_scale);
    if (g_gfx_cfg.internal_scale < 1 || g_gfx_cfg.internal_scale > 8)
        g_gfx_cfg.internal_scale = 1;
    g_gfx_cfg.frame_height = env_int("RUSH_GFX_HEIGHT", g_gfx_cfg.frame_height);
    g_gfx_cfg.verbose = env_int("RUSH_GFX_VERBOSE", g_gfx_cfg.verbose);
    S.win_w = window_width > 0 ? window_width : 960;
    S.win_h = window_height > 0 ? window_height : 720;
    S.headless = headless;
    S.use_sdl = 0;
    if (!headless) {
#ifdef PC_GFX_HAVE_SDL
        if (gfx_sdl_init(S.win_w, S.win_h, &get_proc, &is_gles) == 0)
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
        if (gfx_egl_init(want_gles, &get_proc, &is_gles) != 0)
            return -1;
    }
    if (gfx_gl_init(get_proc, is_gles) != 0) {
        pc_gfx_shutdown();
        return -1;
    }
    memset(&g_gfx, 0, sizeof(g_gfx));
    gfx_rdp_reset();
    gfx_rsp_reset();
    S.inited = 1;
    return 0;
}

void pc_gfx_run_dl(const void *dl) {
    if (!S.inited || dl == NULL)
        return;
    g_gfx.stat_cmds = g_gfx.stat_tris = g_gfx.stat_draws = g_gfx.stat_unknown = 0;
    gfx_run_list(dl);
    if (g_gfx_cfg.verbose)
        fprintf(stderr, "gfx: task: %u commands, %u triangles, %u draws, %u unknown\n", g_gfx.stat_cmds,
                g_gfx.stat_tris, g_gfx.stat_draws, g_gfx.stat_unknown);
}

void pc_gfx_present(const void *framebuffer) {
    GfxTarget *t;
    if (!S.inited)
        return;
    gfx_flush();
    t = gfx_gl_find_target(gfx_resolve((uintptr_t)framebuffer));
    if (t == NULL)
        t = gfx_gl_last_target();
    if (t != NULL) {
        int w = S.win_w, h = S.win_h;
#ifdef PC_GFX_HAVE_SDL
        if (S.use_sdl)
            gfx_sdl_size(&w, &h);
#endif
        gfx_gl_present(t, w, h, S.use_sdl);
#ifdef PC_GFX_HAVE_SDL
        if (S.use_sdl)
            gfx_sdl_swap();
#endif
    }
    gfx_rdp_new_frame();
}

int pc_gfx_poll_events(void) {
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        return gfx_sdl_poll();
#endif
    return 0;
}

int pc_gfx_save_screenshot(const char *path) {
    uint8_t *px;
    int w, h, rc;
    if (!S.inited || gfx_gl_read_presented(NULL, &w, &h) != 0)
        return -1;
    px = (uint8_t *)malloc((size_t)w * h * 4);
    if (px == NULL)
        return -1;
    rc = gfx_gl_read_presented(px, &w, &h);
    if (rc == 0)
        rc = gfx_png_write(path, px, w, h);
    free(px);
    return rc;
}

int pc_gfx_read_framebuffer(const void *cimg, uint8_t *rgba, int *w, int *h) {
    GfxTarget *t;
    if (!S.inited)
        return -1;
    gfx_flush();
    t = gfx_gl_find_target(gfx_resolve((uintptr_t)cimg));
    if (t == NULL)
        t = gfx_gl_last_target();
    return gfx_gl_read_target(t, rgba, w, h);
}

int pc_gfx_read_window(uint8_t *rgba, int *w, int *h) {
    if (!S.inited || !S.use_sdl)
        return -1;
#ifdef PC_GFX_HAVE_SDL
    gfx_sdl_size(w, h);
#endif
    return rgba != NULL ? gfx_gl_read_window(rgba, *w, *h) : 0;
}

void pc_gfx_shutdown(void) {
    if (S.inited) {
        gfx_tex_cache_clear();
        gfx_rdp_forget_targets();
        gfx_gl_shutdown();
    }
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        gfx_sdl_shutdown();
    else
#endif
        gfx_egl_shutdown();
    memset(&S, 0, sizeof(S));
}
