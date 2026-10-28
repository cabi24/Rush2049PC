/* port/gfx/gfx_sdl.c: SDL2 window and GL context. Built only when SDL2 is found (PC_GFX_HAVE_SDL).
 * want_gles (RUSH_GFX_API, gfx_api.c's pcgfx_want_gles) asks for GLES 3.0 first (then desktop GL), else desktop GL 3.3
 * core (then 3.1, then GLES).
 * port/input handles the window's keys, controllers, fullscreen and focus through the event hook
 * (docs/port/FRONTEND.md); everything here runs on the main thread.
 * The window and the display (GFX.md "Window and display"): sizes the settings give are pixels; on Windows the
 * process is per-monitor DPI aware (no 1440 x 810 virtual window on a 200% 2880 x 1620 desktop, blurred by Windows),
 * and where the window system counts in points (macOS, Wayland) they are turned into its units by the drawable's
 * size against the window's. Fullscreen is borderless over the desktop, or exclusive at video.resolution. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"
#include "pc_port.h"

static SDL_Window *s_win;
static SDL_GLContext s_ctx;

static struct {
    int fs_mode;                  /* PC_GFX_FULLSCREEN_DESKTOP or _EXCLUSIVE: what fullscreen means */
    int res_w, res_h;             /* video.resolution; 0: the desktop's */
    int pos_x, pos_y, pos_set;    /* where the window opens (pc_gfx_set_window_position) */
    int geom[4], geom_set;        /* the last windowed place and size, window units */
} D;

static int fullscreen_flags(void) {
    return s_win != NULL ? (int)(SDL_GetWindowFlags(s_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) : 0;
}

/* The windowed place and size, kept while the window is neither fullscreen nor maximized. */
static void note_geometry(void) {
    Uint32 f;
    if (s_win == NULL)
        return;
    f = SDL_GetWindowFlags(s_win);
    if (f & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))
        return;
    SDL_GetWindowPosition(s_win, &D.geom[0], &D.geom[1]);
    SDL_GetWindowSize(s_win, &D.geom[2], &D.geom[3]);
    D.geom_set = 1;
}

/* Desktop fullscreen at a resolution draws the frame for that size, fitted to the screen (gfx_api.c). */
static void update_render_size(void) {
    if (fullscreen_flags() == SDL_WINDOW_FULLSCREEN_DESKTOP && D.res_w > 0)
        pc_gfx_set_render_size(D.res_w, D.res_h);
    else
        pc_gfx_set_render_size(0, 0);
}

/* Pixels to the window system's units: the drawable against the window (gfx_size.c). */
static void units_of(int px_w, int px_h, int *w, int *h) {
    int ww = 0, wh = 0, dw = 0, dh = 0;
    if (s_win != NULL) {
        SDL_GetWindowSize(s_win, &ww, &wh);
        SDL_GL_GetDrawableSize(s_win, &dw, &dh);
    }
    pcgfx_units_of_pixels(px_w, px_h, ww, wh, dw, dh, w, h);
}

/* A saved place on a display that is still there (at least 64 x 32 of the window on one), else -1. */
static int place_ok(int x, int y, int w, int h) {
    int n = SDL_GetNumVideoDisplays(), d;
    for (d = 0; d < n; d++) {
        SDL_Rect r;
        if (SDL_GetDisplayUsableBounds(d, &r) != 0 && SDL_GetDisplayBounds(d, &r) != 0)
            continue;
        if (x + w - 64 >= r.x && x + 64 <= r.x + r.w && y >= r.y - 8 && y + 32 <= r.y + r.h)
            return 0;
    }
    return -1;
}

static void *sdl_proc(const char *name) {
    return SDL_GL_GetProcAddress(name);
}

static int try_create(int w, int h, int gles, int major, int minor) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, gles ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    if (s_win == NULL) {
        int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED;
        if (D.pos_set && place_ok(D.pos_x, D.pos_y, w, h) == 0)
            x = D.pos_x, y = D.pos_y;
        s_win = SDL_CreateWindow("Rush 2049", x, y, w, h,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    }
    if (s_win == NULL)
        return -1;
    s_ctx = SDL_GL_CreateContext(s_win);
    if (s_ctx == NULL) {
        /* the window's pixel format can depend on the attributes: start over */
        SDL_DestroyWindow(s_win);
        s_win = NULL;
        return -1;
    }
    return 0;
}

/* w x h pixels, or 0 x 0 for two thirds of the desktop at the picture's aspect (gfx_size.c). */
int pcgfx_sdl_init(int w, int h, int want_gles, GfxGetProc *get_proc, int *is_gles) {
    int want_gl = !want_gles, px_w = w, px_h = h;
    /* Before the video subsystem starts: Windows then gives the window real pixels at any display scaling (SDL
     * 2.24+; the name is the hint's, so older headers build too). */
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "gfx: SDL video: %s\n", SDL_GetError());
        return -1;
    }
    if (w <= 0 || h <= 0) {
        SDL_DisplayMode m;
        SDL_Rect u = {0, 0, 0, 0};
        float a = pcgfx_cfg.widescreen > 0.0f ? pcgfx_cfg.widescreen : pcgfx_cfg.widescreen == 0.0f ? 4.0f / 3 : 0.0f;
        if (SDL_GetDesktopDisplayMode(0, &m) != 0)
            m.w = m.h = 0;
        if (SDL_GetDisplayUsableBounds(0, &u) != 0)
            u.w = u.h = 0;
        pcgfx_default_window(m.w, m.h, u.w, u.h, a, &px_w, &px_h);
        w = px_w;
        h = px_h;
    }
    if (!want_gl && try_create(w, h, 1, 3, 0) == 0)
        *is_gles = 1;
    else if (try_create(w, h, 0, 3, 3) == 0 || try_create(w, h, 0, 3, 1) == 0)
        *is_gles = 0;
    else if (want_gl && try_create(w, h, 1, 3, 0) == 0)
        *is_gles = 1;
    else {
        fprintf(stderr, "gfx: SDL GL context: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return -1;
    }
    SDL_GL_SetSwapInterval(1);
    {   /* the size asked for is pixels: where the window system counts points, in its units */
        int uw, uh, dw, dh;
        units_of(px_w, px_h, &uw, &uh);
        if (uw != w || uh != h)
            SDL_SetWindowSize(s_win, uw, uh);
        SDL_GL_GetDrawableSize(s_win, &dw, &dh);
        SDL_GetWindowSize(s_win, &uw, &uh);
        fprintf(stderr, "gfx: window %d x %d pixels (%d x %d in the window system's units)\n", dw, dh, uw, uh);
    }
    note_geometry();
    *get_proc = sdl_proc;
    return 0;
}

void pcgfx_sdl_place(int x, int y) {
    D.pos_x = x;
    D.pos_y = y;
    D.pos_set = 1;
}

void pcgfx_sdl_set_fullscreen_mode(int mode) {
    int was = fullscreen_flags();
    D.fs_mode = mode == PC_GFX_FULLSCREEN_EXCLUSIVE ? mode : PC_GFX_FULLSCREEN_DESKTOP;
    if (was)   /* switch over at once */
        pcgfx_sdl_set_fullscreen(1);
}

void pcgfx_sdl_set_resolution(int w, int h) {
    D.res_w = w > 0 && h > 0 ? w : 0;
    D.res_h = w > 0 && h > 0 ? h : 0;
    if (fullscreen_flags() == SDL_WINDOW_FULLSCREEN && D.fs_mode == PC_GFX_FULLSCREEN_EXCLUSIVE)
        pcgfx_sdl_set_fullscreen(1);   /* the display mode changes */
    update_render_size();
}

/* on: the fullscreen mode in effect (borderless over the desktop, or exclusive at the resolution, the closest mode
 * the display has, at the desktop's refresh when it has that); off: the window, where it was. */
int pcgfx_sdl_set_fullscreen(int on) {
    int rc = 0, d;
    if (s_win == NULL)
        return -1;
    if (!fullscreen_flags())
        note_geometry();
    if (!on)
        rc = SDL_SetWindowFullscreen(s_win, 0);
    else if (D.fs_mode == PC_GFX_FULLSCREEN_EXCLUSIVE) {
        SDL_DisplayMode want, got;
        d = SDL_GetWindowDisplayIndex(s_win);
        if (d < 0 || SDL_GetDesktopDisplayMode(d, &want) != 0)
            return -1;
        if (D.res_w > 0) {
            want.w = D.res_w;
            want.h = D.res_h;
        }
        if (SDL_GetClosestDisplayMode(d, &want, &got) == NULL)
            SDL_GetDesktopDisplayMode(d, &got);
        if (fullscreen_flags())
            SDL_SetWindowFullscreen(s_win, 0);
        rc = SDL_SetWindowDisplayMode(s_win, &got);
        if (rc == 0)
            rc = SDL_SetWindowFullscreen(s_win, SDL_WINDOW_FULLSCREEN);
        if (rc == 0)
            fprintf(stderr, "gfx: exclusive fullscreen %d x %d, %d Hz\n", got.w, got.h, got.refresh_rate);
    } else {
        if (fullscreen_flags() == SDL_WINDOW_FULLSCREEN)
            SDL_SetWindowFullscreen(s_win, 0);
        rc = SDL_SetWindowFullscreen(s_win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    if (rc != 0)
        fprintf(stderr, "gfx: fullscreen: %s\n", SDL_GetError());
    update_render_size();
    return rc;
}

int pcgfx_sdl_is_fullscreen(void) {
    return s_win != NULL ? fullscreen_flags() != 0 : -1;
}

int pcgfx_sdl_set_vsync(int on) {
    if (SDL_GL_SetSwapInterval(on ? 1 : 0) != 0) {
        fprintf(stderr, "gfx: vsync %s: %s\n", on ? "on" : "off", SDL_GetError());
        return -1;
    }
    return 0;
}

/* The window's size in pixels; in fullscreen it applies when the window comes back. */
void pcgfx_sdl_set_window_pixels(int w, int h) {
    int uw, uh;
    if (s_win == NULL)
        return;
    units_of(w, h, &uw, &uh);
    if (fullscreen_flags()) {
        D.geom[2] = uw;
        D.geom[3] = uh;
        SDL_SetWindowSize(s_win, uw, uh);   /* SDL keeps it for the window */
        return;
    }
    if (SDL_GetWindowFlags(s_win) & SDL_WINDOW_MAXIMIZED)
        SDL_RestoreWindow(s_win);
    SDL_SetWindowSize(s_win, uw, uh);
    note_geometry();
}

/* The last windowed place (window units) and size (pixels). */
int pcgfx_sdl_geometry(int *x, int *y, int *w, int *h) {
    int ww = 0, wh = 0, dw = 0, dh = 0;
    if (s_win == NULL || !D.geom_set)
        return -1;
    SDL_GetWindowSize(s_win, &ww, &wh);
    SDL_GL_GetDrawableSize(s_win, &dw, &dh);
    *x = D.geom[0];
    *y = D.geom[1];
    /* pixels from units, the drawable against the window (the other way round from units_of) */
    pcgfx_units_of_pixels(D.geom[2], D.geom[3], dw, dh, ww, wh, w, h);
    return 0;
}

void pcgfx_sdl_swap(void) {
    SDL_GL_SwapWindow(s_win);
}

/* port/input's handler (pc_gfx_set_event_hook): every event goes to it first, so this is the only event loop. */
static int (*s_hook)(const void *sdl_event);

int pc_gfx_set_event_hook(int (*hook)(const void *sdl_event)) {
    s_hook = hook;
    return s_win != NULL ? 0 : -1;
}

int pcgfx_sdl_poll(void) {
    SDL_Event e;
    int quit = 0;
    while (SDL_PollEvent(&e)) {
        if (s_hook != NULL && s_hook(&e) != 0)
            quit = 1;
        if (e.type == SDL_QUIT || (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE))
            quit = 1;
        if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_MOVED ||
                                          e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
            note_geometry();
            update_render_size();
        }
    }
    return quit;
}

void pcgfx_sdl_size(int *w, int *h) {
    SDL_GL_GetDrawableSize(s_win, w, h);
}

int pcgfx_sdl_refresh(void) {
    SDL_DisplayMode mode;
    int d = s_win != NULL ? SDL_GetWindowDisplayIndex(s_win) : -1;
    if (d < 0 || SDL_GetCurrentDisplayMode(d, &mode) != 0)
        return 0;
    return mode.refresh_rate;
}

void pcgfx_sdl_shutdown(void) {
    if (s_ctx != NULL)
        SDL_GL_DeleteContext(s_ctx);
    if (s_win != NULL)
        SDL_DestroyWindow(s_win);
    s_ctx = NULL;
    s_win = NULL;
    s_hook = NULL;
    memset(&D, 0, sizeof(D));
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
