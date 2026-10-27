/* port/gfx/gfx_egl.c: headless GL context through EGL without a window system: Mesa's surfaceless platform,
 * then an EGL device, then the default display. Rendering goes to framebuffer objects only.
 * RUSH_GFX_API=gl asks for desktop GL (3.3 core, else 3.1), anything else GLES 3.0 first. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <string.h>

#include "gfx_internal.h"

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif
#ifndef EGL_NO_CONFIG_KHR
#define EGL_NO_CONFIG_KHR ((EGLConfig)0)
#endif

static EGLDisplay s_dpy = EGL_NO_DISPLAY;
static EGLContext s_ctx = EGL_NO_CONTEXT;

static void *egl_proc(const char *name) {
    return (void *)eglGetProcAddress(name);
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

static EGLDisplay open_display(void) {
    const char *cext = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay d;
    EGLint maj, min;
    if (cext != NULL && get_platform != NULL) {
        if (has_ext(cext, "EGL_MESA_platform_surfaceless")) {
            d = get_platform(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
            if (d != EGL_NO_DISPLAY && eglInitialize(d, &maj, &min))
                return d;
        }
        if (has_ext(cext, "EGL_EXT_platform_device")) {
            PFNEGLQUERYDEVICESEXTPROC query = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
            EGLDeviceEXT devs[8];
            EGLint n = 0, i;
            if (query != NULL && query(8, devs, &n))
                for (i = 0; i < n; i++) {
                    d = get_platform(EGL_PLATFORM_DEVICE_EXT, devs[i], NULL);
                    if (d != EGL_NO_DISPLAY && eglInitialize(d, &maj, &min))
                        return d;
                }
        }
    }
    d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (d != EGL_NO_DISPLAY && eglInitialize(d, &maj, &min))
        return d;
    return EGL_NO_DISPLAY;
}

static EGLContext try_context(int gles, int major, int minor, int core) {
    EGLint attr[16], n = 0, count = 0;
    EGLConfig cfg = EGL_NO_CONFIG_KHR;
    EGLint cattr[] = {EGL_RENDERABLE_TYPE, gles ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_BIT, EGL_NONE};
    EGLContext c;
    if (!eglBindAPI(gles ? EGL_OPENGL_ES_API : EGL_OPENGL_API))
        return EGL_NO_CONTEXT;
    if (!eglChooseConfig(s_dpy, cattr, &cfg, 1, &count) || count < 1)
        cfg = EGL_NO_CONFIG_KHR;   /* surfaceless displays may have no configs: EGL_KHR_no_config_context */
    attr[n++] = EGL_CONTEXT_MAJOR_VERSION;
    attr[n++] = major;
    attr[n++] = EGL_CONTEXT_MINOR_VERSION;
    attr[n++] = minor;
    if (!gles && core) {
        attr[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
        attr[n++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    }
    attr[n] = EGL_NONE;
    c = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, attr);
    if (c != EGL_NO_CONTEXT && !eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, c)) {
        eglDestroyContext(s_dpy, c);
        c = EGL_NO_CONTEXT;
    }
    return c;
}

int gfx_egl_init(int want_gles, GfxGetProc *get_proc, int *is_gles) {
    int pass;
    s_dpy = open_display();
    if (s_dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "gfx: no EGL display\n");
        return -1;
    }
    for (pass = 0; pass < 2 && s_ctx == EGL_NO_CONTEXT; pass++) {
        int gles = (pass == 0) == (want_gles != 0);
        if (gles) {
            s_ctx = try_context(1, 3, 0, 0);
        } else {
            s_ctx = try_context(0, 3, 3, 1);
            if (s_ctx == EGL_NO_CONTEXT)
                s_ctx = try_context(0, 3, 1, 0);
        }
        if (s_ctx != EGL_NO_CONTEXT)
            *is_gles = gles;
    }
    if (s_ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "gfx: no EGL context (error 0x%x)\n", eglGetError());
        eglTerminate(s_dpy);
        s_dpy = EGL_NO_DISPLAY;
        return -1;
    }
    *get_proc = egl_proc;
    return 0;
}

void gfx_egl_shutdown(void) {
    if (s_dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_ctx != EGL_NO_CONTEXT)
            eglDestroyContext(s_dpy, s_ctx);
        eglTerminate(s_dpy);
    }
    s_dpy = EGL_NO_DISPLAY;
    s_ctx = EGL_NO_CONTEXT;
}
