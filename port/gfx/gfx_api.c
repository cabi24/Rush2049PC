/* port/gfx/gfx_api.c: the pc_gfx_* interface (port/include/pc_port.h).
 *
 * Environment: RUSH_GFX_API=auto|gl|gles (which API to try first: pcgfx_want_gles),
 * RUSH_GFX_SCALE=<n>|auto (internal resolution multiple; auto: the largest the window shows whole),
 * RUSH_GFX_WIDESCREEN=off|auto|16:9|<w:h>|<ratio> (display aspect; auto: the window's),
 * RUSH_GFX_HEIGHT=<rows> (rows of the frame buffer to show; default: the largest scissor bottom drawn with),
 * RUSH_GFX_FRAME_RATE=original|display|<hz> (frames shown a second, between the game's own: gfx_interp.c),
 * RUSH_GFX_TEXTURES=original|upscaled|pack and the other RUSH_GFX_TEXTURE_* (gfx_tex.c, docs/port/TEXTURES.md),
 * RUSH_GFX_VERBOSE=1|2|3, RUSH_GFX_TIMES=1|2 (the CPU time of tasks and frames at exit; 2: with the GPU's, glFinish). */
#define _POSIX_C_SOURCE 200809L   /* clock_gettime */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "gfx_internal.h"
#include "gfx_tex.h"
#include "pc_port.h"

/* Milliseconds on a monotonic clock: the CPU time the renderer spends (GL calls return before the GPU is done). */
static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
#endif
}

GfxConfig pcgfx_cfg = {1, 1, 0.0f, 0.0f, 0, 0, 0, 0};

void pcgfx_rdp_new_frame(void);
void pcgfx_rdp_forget_targets(void);
void pcgfx_tex_cache_clear(void);

static struct {
    int inited, headless, use_sdl;
    int win_w, win_h;
    uint32_t vi;              /* the last retrace (pc_gfx_retrace) */
    GfxTarget *shown;         /* high frame rate: the target last shown instead of the presented image, or NULL */
    int shown_rows;
    uint32_t present_gen, shown_gen;   /* presented images so far; the one last shown (high frame rate) */
    int shown_w, shown_h;     /* the window size it was shown at */
    int display_hz;           /* the display's refresh rate, for frame_rate = display; 0 not taken yet */
    /* what the renderer's CPU time goes to (the exit's summary with a high frame rate, or RUSH_GFX_VERBOSE) */
    uint32_t tasks, drawn;
    double task_ms, task_ms_max, drawn_ms, drawn_ms_max, tick_ms;
    int times;                /* RUSH_GFX_TIMES: 1 the summary at exit; 2 with glFinish around each timed part, so
                               * its time includes the GPU's */
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

/* Textures at run time (PCGFX_TEX_*: original, upscaled, pack): the texture cache is made again from the next
 * frame on; what was upscaled or loaded before is kept in memory. */
void pc_gfx_set_textures(int mode) {
    pcgfx_tex_set_mode(mode);
}

/* "original" (or 0, "off") 0; "display" -1; else frames a second, 1-1000. *ok = 0 for anything else. */
int pc_gfx_parse_frame_rate(const char *text, int *ok) {
    char *end;
    long v;
    *ok = 1;
    if (text == NULL || !*text || !strcmp(text, "original") || !strcmp(text, "off") || !strcmp(text, "0"))
        return 0;
    if (!strcmp(text, "display"))
        return -1;
    v = strtol(text, &end, 10);
    if (*end == '\0' && v >= 1 && v <= 1000)
        return (int)v;
    *ok = 0;
    return 0;
}

void pc_gfx_set_frame_rate(int rate) {
    int was = pcgfx_cfg.frame_rate != 0;
    pcgfx_cfg.frame_rate = rate < -1 ? 0 : rate;
    if (was != (pcgfx_cfg.frame_rate != 0)) {
        pcgfx_hfr_reset();   /* on: from the next frames drawn; off: the presents show frames again */
        S.shown = NULL;
    }
}

/* The display's refresh: the window's display (taken again at each event pump, so moving the window to another
 * display follows it), or 60 without one. */
static void display_refresh(void) {
    int r = 0;
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        r = pcgfx_sdl_refresh();
#endif
    S.display_hz = r > 0 ? r : 60;
}

int pc_gfx_frame_rate(void) {
    int r = pcgfx_cfg.frame_rate;
    if (r < 0) {
        if (S.display_hz <= 0)
            display_refresh();
        r = S.display_hz;
    }
    return r;
}

void pc_gfx_set_window_size(int w, int h) {
    if (!S.use_sdl && w > 0 && h > 0) {
        S.win_w = w;
        S.win_h = h;
        resolve_auto();
    }
}

/* RUSH_GFX_API (video.api): "gl" tries desktop GL first, "gles" OpenGL ES 3.0 first; each falls back to the other.
 * auto (or anything else): desktop GL on Windows, whose drivers are made and tested for it (their ES profile, through
 * WGL_EXT_create_context_es2_profile, is a side path); ES elsewhere, where Mesa serves both alike. */
int pcgfx_want_gles(const char *api) {
    if (api != NULL && strcmp(api, "gl") == 0)
        return 0;
    if (api != NULL && strcmp(api, "gles") == 0)
        return 1;
#ifdef _WIN32
    return 0;
#else
    return 1;
#endif
}

int pc_gfx_init(int headless, int window_width, int window_height) {
    GfxGetProc get_proc = NULL;
    int want_gles = pcgfx_want_gles(getenv("RUSH_GFX_API")), is_gles = want_gles;
    if (S.inited)
        return 0;
    if (getenv("RUSH_GFX_SCALE") != NULL && !strcmp(getenv("RUSH_GFX_SCALE"), "auto"))
        pcgfx_cfg.internal_scale = 0;
    else
        pcgfx_cfg.internal_scale = env_int("RUSH_GFX_SCALE", pcgfx_cfg.internal_scale);
    if (pcgfx_cfg.internal_scale < 0 || pcgfx_cfg.internal_scale > 8)
        pcgfx_cfg.internal_scale = 1;
    pcgfx_cfg.frame_height = env_int("RUSH_GFX_HEIGHT", pcgfx_cfg.frame_height);
    pcgfx_tex_configure();
    pcgfx_cfg.verbose = env_int("RUSH_GFX_VERBOSE", pcgfx_cfg.verbose);
    pcgfx_cfg.trace_task = env_int("RUSH_GFX_TRACE", 0);
    S.times = env_int("RUSH_GFX_TIMES", 0);
    if (getenv("RUSH_GFX_FRAME_RATE") != NULL) {
        int ok, r = pc_gfx_parse_frame_rate(getenv("RUSH_GFX_FRAME_RATE"), &ok);
        if (ok)
            pcgfx_cfg.frame_rate = r;
        else
            fprintf(stderr, "gfx: RUSH_GFX_FRAME_RATE=%s is not original, display or 1-1000; ignored\n",
                    getenv("RUSH_GFX_FRAME_RATE"));
    }
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
        if (pcgfx_sdl_init(S.win_w, S.win_h, want_gles, &get_proc, &is_gles) == 0)
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
    pcgfx_hfr_reset();
#ifdef PC_GFX_HAVE_SDL
    if (S.use_sdl)
        pcgfx_sdl_size(&S.win_w, &S.win_h);
#endif
    resolve_auto();
    S.inited = 1;
    return 0;
}

/* RUSH_GFX_DUMP=DIR[:FIRST[:COUNT]]: the color image of each graphics task from FIRST (1, the first) on, COUNT of them
 * (100), as DIR/task-NNNNN.png, at the internal scale, as drawn: every frame the game draws, where screenshots take
 * one. tools/port/frame_spikes.py finds frames among them that differ from both neighbours. */
static void dump_task(void) {
    static const char *spec;
    static int checked, first = 1, count = 100;
    const char *colon;
    char path[512];
    uint8_t *px;
    int w, h;
    if (!checked) {
        checked = 1;
        spec = getenv("RUSH_GFX_DUMP");
        if (spec != NULL && !*spec)
            spec = NULL;
        if (spec != NULL && (colon = strchr(spec, ':')) != NULL) {
            first = atoi(colon + 1);
            if ((colon = strchr(colon + 1, ':')) != NULL)
                count = atoi(colon + 1);
        }
    }
    if (spec == NULL || (int)S.tasks < first || (int)S.tasks >= first + count ||
        pcgfx_gl_read_target(pcgfx_gl_last_target(), NULL, &w, &h) != 0)
        return;
    px = (uint8_t *)malloc((size_t)w * h * 4);
    if (px == NULL)
        return;
    colon = strchr(spec, ':');
    snprintf(path, sizeof(path), "%.*s/task-%05u.png", colon != NULL ? (int)(colon - spec) : (int)strlen(spec), spec,
             S.tasks);
    if (pcgfx_gl_read_target(pcgfx_gl_last_target(), px, &w, &h) == 0)
        pcgfx_png_write(path, px, w, h);
    free(px);
}

void pc_gfx_run_dl(const void *dl) {
    if (!S.inited || dl == NULL)
        return;
    double t0;
    if (S.times > 1)
        pcgfx_gl_finish();
    t0 = now_ms();
    pcgfx.stat_cmds = pcgfx.stat_tris = pcgfx.stat_draws = pcgfx.stat_unknown = 0;
    if (pcgfx_cfg.frame_rate != 0)   /* a high frame rate: recorded, to be drawn again between frames */
        pcgfx_rec_task_begin();
    pcgfx_run_list(dl);
    if (pcgfx_rec != NULL) {
        pcgfx_rec_task_end();
        pcgfx_gl_fence_set();   /* frames between are drawn only once the GPU has done this (pcgfx_hfr_frame) */
    }
    if (S.times > 1)
        pcgfx_gl_finish();
    S.tasks++;
    t0 = now_ms() - t0;
    S.task_ms += t0;
    if (t0 > S.task_ms_max)
        S.task_ms_max = t0;
    dump_task();
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

void pc_gfx_retrace(uint32_t vi) {
    S.vi = vi;
}

void pc_gfx_present(const void *framebuffer) {
    GfxTarget *t;
    if (!S.inited)
        return;
    pcgfx_flush();
    t = pcgfx_gl_find_target(pcgfx_resolve((uintptr_t)framebuffer));
    if (t == NULL)
        t = pcgfx_gl_last_target();
    if (pcgfx_cfg.frame_rate != 0) {
        /* a high frame rate: the frame becomes the presented image (screenshots), and the main loop's ticks show
         * it, or the frames drawn between it and the next (pc_gfx_present_tick) */
        if (t != NULL) {
            pcgfx_gl_present_image(t);
            pcgfx_hfr_presented(t, S.vi);
            S.present_gen++;
        }
        resolve_auto();
        pcgfx_rdp_new_frame();
        return;
    }
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

/* A high frame rate's tick (the main loop, video.frame_rate times a second): the frame for time vt, in retraces.
 * The presented frame when it is due, else one drawn between it and the next, or the next one when it is late. */
int pc_gfx_present_tick(double vt, int mode) {
    GfxTarget *t = NULL;
    int rows = 0, kind;
    double t0, t1;
    if (!S.inited || pcgfx_cfg.frame_rate == 0)
        return 0;
    if (S.times > 1)
        pcgfx_gl_finish();
    t0 = now_ms();
    pcgfx_flush();
    window_size();
    kind = pcgfx_hfr_frame(vt, &t, &rows, mode);
    if (kind == 3 || (kind == 0 && S.shown == NULL && S.shown_gen == S.present_gen && S.shown_w == S.win_w &&
                      S.shown_h == S.win_h && !pcgfx_ui_active()))
        return 0;   /* nothing new: the screen keeps what it shows */
    if (S.times > 1)
        pcgfx_gl_finish();
    t1 = now_ms() - t0;
    if (kind == 2) {
        S.drawn++;
        S.drawn_ms += t1;
        if (t1 > S.drawn_ms_max)
            S.drawn_ms_max = t1;
    }
    if (kind != 0 && t != NULL) {
        pcgfx_gl_show_target(t, rows, S.win_w, S.win_h, S.use_sdl);
        S.shown = t;
        S.shown_rows = rows;
    } else {
        pcgfx_gl_show(S.win_w, S.win_h, S.use_sdl);   /* black before the first frame */
        S.shown = NULL;
        S.shown_gen = S.present_gen;
    }
    S.shown_w = S.win_w;
    S.shown_h = S.win_h;
    pcgfx_hfr_dump(S.shown, kind == 2);
    pcgfx_ui_run(S.win_w, S.win_h, S.use_sdl);
    swap();
    S.tick_ms += now_ms() - t0;
    return 1;
}

/* The last frame shown again (the presented image, not a render target, which the game may have drawn into
 * since; with a high frame rate, the frame drawn between two when that was the last shown), with the overlay. */
void pc_gfx_redraw(void) {
    if (!S.inited)
        return;
    pcgfx_flush();
    window_size();
    if (S.shown != NULL && pcgfx_cfg.frame_rate != 0)
        pcgfx_gl_show_target(S.shown, S.shown_rows, S.win_w, S.win_h, S.use_sdl);
    else
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
    if (pcgfx_cfg.frame_rate < 0)
        display_refresh();
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
        PcGfxHfrStats st;
        pcgfx_hfr_stats(&st);
        if (pcgfx_cfg.frame_rate != 0)
            fprintf(stderr, "gfx: frame rate %d: %u frames shown, %u drawn between two, %u next ones early, %u held, "
                    "%u not drawn (the host or its GPU was behind)\n", pc_gfx_frame_rate(), st.ticks, st.drawn, st.early, st.held,
                    st.busy);
        if (pcgfx_cfg.frame_rate != 0 || pcgfx_cfg.verbose || S.times)   /* GL may finish the work later */
            fprintf(stderr, "gfx: %s time: %u graphics tasks, %.3f ms each (at most %.3f); %u frames drawn "
                    "between two, %.3f ms each (at most %.3f); %u frames shown, %.3f ms each\n",
                    S.times > 1 ? "GPU and CPU" : "CPU", S.tasks, S.tasks ? S.task_ms / S.tasks : 0.0, S.task_ms_max,
                    S.drawn, S.drawn ? S.drawn_ms / S.drawn : 0.0, S.drawn_ms_max, st.ticks,
                    st.ticks ? S.tick_ms / st.ticks : 0.0);
        pcgfx_hfr_shutdown();
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
