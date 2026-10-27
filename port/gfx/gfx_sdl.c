/* port/gfx/gfx_sdl.c: SDL2 window and GL context. Built only when SDL2 is found (PC_GFX_HAVE_SDL).
 * RUSH_GFX_API=gl asks for desktop GL 3.3 core (then 3.1), anything else GLES 3.0 (then desktop GL). */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_internal.h"

static SDL_Window *s_win;
static SDL_GLContext s_ctx;

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
    if (s_win == NULL)
        s_win = SDL_CreateWindow("Rush 2049", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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

int pcgfx_sdl_init(int w, int h, GfxGetProc *get_proc, int *is_gles) {
    const char *api = getenv("RUSH_GFX_API");
    int want_gl = api != NULL && strcmp(api, "gl") == 0;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "gfx: SDL video: %s\n", SDL_GetError());
        return -1;
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
    *get_proc = sdl_proc;
    return 0;
}

void pcgfx_sdl_swap(void) {
    SDL_GL_SwapWindow(s_win);
}

int pcgfx_sdl_poll(void) {
    SDL_Event e;
    int quit = 0;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_QUIT || (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE))
            quit = 1;
    return quit;
}

void pcgfx_sdl_size(int *w, int *h) {
    SDL_GL_GetDrawableSize(s_win, w, h);
}

void pcgfx_sdl_shutdown(void) {
    if (s_ctx != NULL)
        SDL_GL_DeleteContext(s_ctx);
    if (s_win != NULL)
        SDL_DestroyWindow(s_win);
    s_ctx = NULL;
    s_win = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
