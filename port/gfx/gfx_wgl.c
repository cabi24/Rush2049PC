/* port/gfx/gfx_wgl.c: headless GL context on Windows, in place of gfx_egl.c (it keeps that file's entry points):
 * WGL on a hidden window that is never shown, so rendering goes to framebuffer objects only. Without want_gles
 * (RUSH_GFX_API gl or auto, the default here: pcgfx_want_gles) desktop GL (3.3 core, else 3.1) first, then GLES 3.0
 * when the driver offers ES profiles (WGL_EXT_create_context_es2_profile); with it (gles) the other way round.
 * wglCreateContextAttribsARB comes from a first, legacy
 * context. GL 1.1 entry points are exported by opengl32.dll itself, and wglGetProcAddress doesn't return them.
 * Without a GPU driver (Windows' own GDI renderer is GL 1.1), Mesa's opengl32.dll next to the program gives
 * llvmpipe (docs/port/WINDOWS.md). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>

#include "gfx_internal.h"

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001
#define WGL_CONTEXT_ES2_PROFILE_BIT_EXT 0x0004

typedef HGLRC(WINAPI *CreateContextAttribsProc)(HDC dc, HGLRC share, const int *attribs);
typedef const char *(WINAPI *GetExtensionsStringProc)(HDC dc);

static const wchar_t s_class[] = L"rush2049pc-gl";
static HWND s_wnd;
static HDC s_dc;
static HGLRC s_ctx;
static HMODULE s_gl;

static void *wgl_proc(const char *name) {
    PROC p = wglGetProcAddress(name);
    /* some drivers return 1, 2, 3 or -1 instead of NULL for what they don't have */
    if (p == NULL || (intptr_t)p == 1 || (intptr_t)p == 2 || (intptr_t)p == 3 || (intptr_t)p == -1)
        p = GetProcAddress(s_gl, name);
    return (void *)p;
}

static int has_ext(const char *list, const char *ext) {
    size_t n = strlen(ext);
    const char *p = list;
    while (p != NULL && (p = strstr(p, ext)) != NULL) {
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || p[n] == 0))
            return 1;
        p += n;
    }
    return 0;
}

static HGLRC try_context(CreateContextAttribsProc create, int gles, int major, int minor, int core) {
    int attr[8], n = 0;
    HGLRC c;
    attr[n++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
    attr[n++] = major;
    attr[n++] = WGL_CONTEXT_MINOR_VERSION_ARB;
    attr[n++] = minor;
    if (gles || core) {
        attr[n++] = WGL_CONTEXT_PROFILE_MASK_ARB;
        attr[n++] = gles ? WGL_CONTEXT_ES2_PROFILE_BIT_EXT : WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
    }
    attr[n] = 0;
    c = create(s_dc, NULL, attr);
    if (c != NULL && !wglMakeCurrent(s_dc, c)) {
        wglDeleteContext(c);
        c = NULL;
    }
    return c;
}

static int open_window(void) {
    WNDCLASSW wc;
    PIXELFORMATDESCRIPTOR pfd;
    int format;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = s_class;
    RegisterClassW(&wc);   /* fails harmlessly when a second init registers it again */
    s_wnd = CreateWindowExW(0, s_class, L"Rush 2049", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, NULL, NULL, wc.hInstance,
                            NULL);
    if (s_wnd == NULL)
        return -1;
    s_dc = GetDC(s_wnd);
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    format = s_dc != NULL ? ChoosePixelFormat(s_dc, &pfd) : 0;
    if (format == 0 || !SetPixelFormat(s_dc, format, &pfd))
        return -1;
    return 0;
}

int pcgfx_egl_init(int want_gles, GfxGetProc *get_proc, int *is_gles) {
    HGLRC legacy;
    CreateContextAttribsProc create;
    GetExtensionsStringProc get_ext;
    const char *ext;
    int pass;
    s_gl = LoadLibraryW(L"opengl32.dll");
    if (s_gl == NULL || open_window() != 0) {
        fprintf(stderr, "gfx: no WGL window (error %lu)\n", (unsigned long)GetLastError());
        pcgfx_egl_shutdown();
        return -1;
    }
    legacy = wglCreateContext(s_dc);
    if (legacy == NULL || !wglMakeCurrent(s_dc, legacy)) {
        fprintf(stderr, "gfx: no WGL context (error %lu)\n", (unsigned long)GetLastError());
        if (legacy != NULL)
            wglDeleteContext(legacy);
        pcgfx_egl_shutdown();
        return -1;
    }
    create = (CreateContextAttribsProc)(void *)wglGetProcAddress("wglCreateContextAttribsARB");
    get_ext = (GetExtensionsStringProc)(void *)wglGetProcAddress("wglGetExtensionsStringARB");
    ext = get_ext != NULL ? get_ext(s_dc) : NULL;
    if (create != NULL)
        for (pass = 0; pass < 2 && s_ctx == NULL; pass++) {
            int gles = (pass == 0) == (want_gles != 0);
            if (gles) {
                if (ext != NULL && has_ext(ext, "WGL_EXT_create_context_es2_profile"))
                    s_ctx = try_context(create, 1, 3, 0, 0);
            } else {
                s_ctx = try_context(create, 0, 3, 3, 1);
                if (s_ctx == NULL)
                    s_ctx = try_context(create, 0, 3, 1, 0);
            }
            if (s_ctx != NULL)
                *is_gles = gles;
        }
    if (s_ctx == NULL) {
        wglMakeCurrent(s_dc, legacy);
        fprintf(stderr, "gfx: no OpenGL 3 context: the driver is %s %s (Windows' own renderer is GL 1.1; put "
                "Mesa's opengl32.dll next to the program for a software one)\n",
                (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(legacy);
        pcgfx_egl_shutdown();
        return -1;
    }
    wglDeleteContext(legacy);   /* not current any more: try_context made the new one current */
    *get_proc = wgl_proc;
    return 0;
}

void pcgfx_egl_shutdown(void) {
    if (s_ctx != NULL) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(s_ctx);
    }
    if (s_dc != NULL)
        ReleaseDC(s_wnd, s_dc);
    if (s_wnd != NULL)
        DestroyWindow(s_wnd);
    if (s_gl != NULL)
        FreeLibrary(s_gl);
    s_ctx = NULL;
    s_dc = NULL;
    s_wnd = NULL;
    s_gl = NULL;
}
